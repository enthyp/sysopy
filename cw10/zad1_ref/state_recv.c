#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "server.h"
#include "protocol.h"
#include "common.h"
#include "state_initial.h"
#include "state_free.h"
#include "state_busy.h"
#include "state_recv.h"

// Prototypes defined in this file.
int recv_receive(void * p_self, server_state * state, int client_id);
int recv_close(void * p_self, server_state * state, int client_id);
int received(handler_recv * self, server_state * state, int client_id);

int
initialize_handler_recv(handler_recv * handler) {
    handler -> receiver_buffer = NULL;
    handler -> state = handler -> head = 0;

    handler -> handle_receive = recv_receive;
    handler -> handle_closed = recv_close;

    return 0;
}

// Turns a sequence of num_bytes bytes into an integer.
int
deserialize(unsigned char * head, int * result, int num_bytes) {
    int outcome = (int) head[0];
    int i;
    for (i = 1; i < num_bytes; i++) {
        outcome <<= 8;
        outcome |= *(head + i);
    }
    *result = outcome;
    return 0;
}

int
recv_receive(void * p_self, server_state * state, int client_id) {
    handler_recv * self = (handler_recv *) p_self;
    printf("LOG: RECV/RECEIVE FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    if (self -> state == 0) {
        // First entry - prepare the buffer to take header and start receiving.
        self -> receiver_buffer = (unsigned char *) malloc((ID_BYTES + LEN_BYTES) * sizeof(unsigned char));
        if (self -> receiver_buffer == NULL) {
            perror("allocate receiver memory");
            exit(EXIT_FAILURE);
        }
        self -> tb_received = ID_BYTES + LEN_BYTES;
        self -> state += 1;
    }

    if (self -> state == 1) {
        // Receiving header.
        int nr = recv(conn -> socket_fd,
                self -> receiver_buffer + self -> head,
                self -> tb_received,
                MSG_DONTWAIT);
        self -> tb_received -= nr;
        self -> head += nr;

        if (self -> tb_received == 0) {
            // We got message header - parse it and prepare.
            deserialize(self -> receiver_buffer, &(self -> task_id), ID_BYTES);
            deserialize(self -> receiver_buffer + ID_BYTES, &(self -> tb_received), LEN_BYTES);
            self -> receiver_buffer = realloc(self -> receiver_buffer, self -> tb_received);
            if (self -> receiver_buffer == NULL) {
                perror("reallocate receiver memory");
                exit(EXIT_FAILURE);
            }
            self -> head = 0;
            self -> state += 1;
        }
    }

    if (self -> state == 2) {
        // Receive the content.
        int nr = recv(conn -> socket_fd,
                      self -> receiver_buffer + self -> head,
                      self -> tb_received,
                      MSG_DONTWAIT);
        self -> tb_received -= nr;
        self -> head += nr;

        if (self -> tb_received == 0) {
            // We got the message - print it!
            printf("RECEIVED TASK %d from %s!\n%s\n",
                    self -> task_id,
                    state -> clients[client_id].name,
                    self -> receiver_buffer);
            return received(self, state, client_id);
        }
    }

    pthread_mutex_unlock(&(conn -> mutex));
    return 0;
}

int
recv_cleanup(handler_recv * self, server_state * state, int client_id) {
    client_conn * conn = &(state -> clients[client_id].connection);

    // Erase socket and its events.
    del_event(state, client_id);
    close(conn -> socket_fd);
    conn -> socket_fd = -1;

    // Decrement client count.
    pthread_mutex_lock(&(state -> count_mutex));
    state -> client_count -= 1;
    pthread_mutex_unlock(&(state -> count_mutex));

    // Reinitialize handler.
    conn -> handler = (handler_initial *) malloc(sizeof(handler_initial));
    if (conn -> handler == NULL) {
        perror("swap connection handler");
        exit(EXIT_FAILURE);
    }
    if (initialize_handler((handler_initial *) conn -> handler) == -1) {
        perror("initialize connection handler");
        exit(EXIT_FAILURE);
    }

    free(self -> receiver_buffer);
    free(self);

    pthread_mutex_lock(&(conn -> mutex));
    conn -> state = INITIAL;
    pthread_mutex_unlock(&(conn -> mutex));

    del_event(state, client_id);
    add_event(state, client_id, EPOLLIN);

    return 0;
}

int
recv_close(void * p_self, server_state * state, int client_id) {
    handler_recv * self = (handler_recv *) p_self;
    printf("LOG: RECV/CLOSE FOR ID: %d\n", client_id);
    return recv_cleanup(self, state, client_id);
}

int
received(handler_recv * self, server_state * state, int client_id) {
    client_conn * conn = &(state -> clients[client_id].connection);
    client_queue * q = &(state -> clients[client_id].task_queue);
    pthread_mutex_lock(&(q -> mutex));

    if (!(q -> q_full) && q -> q_head == q -> q_tail) {
        // Go to state FREE.
        printf("LOG: RECV -> FREE FOR ID: %d\n", client_id);

        conn -> handler = (handler_free *) malloc(sizeof(handler_free));
        if (conn -> handler == NULL) {
            perror("swap connection handler");
            exit(EXIT_FAILURE);
        }
        if (initialize_handler_free((handler_free *) conn -> handler) == -1) {
            perror("initialize connection handler");
            exit(EXIT_FAILURE);
        }

        free(self -> receiver_buffer);
        free(self);

        // We have the lock from recv_receive.
        conn -> state = FREE;

        del_event(state, client_id);
        add_event(state, client_id, EPOLLIN);
        pthread_mutex_unlock(&(q -> mutex));
        return 0;
    }

    printf("LOG: RECV -> BUSY FOR ID: %d\n", client_id);

    conn -> handler = (handler_busy *) malloc(sizeof(handler_busy));
    if (conn -> handler == NULL) {
        perror("swap connection handler");
        exit(EXIT_FAILURE);
    }
    if (initialize_handler_busy((handler_busy *) conn -> handler) == -1) {
        perror("initialize connection handler");
        exit(EXIT_FAILURE);
    }

    free(self -> receiver_buffer);
    free(self);

    // We have the lock from recv_receive.
    conn -> state = BUSY;

    del_event(state, client_id);
    add_event(state, client_id, EPOLLIN | EPOLLOUT);

    pthread_mutex_unlock(&(q -> mutex));
    pthread_mutex_unlock(&(conn -> mutex));

    return 0;
}