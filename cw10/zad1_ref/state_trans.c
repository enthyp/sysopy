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
#include "state_proc.h"
#include "state_trans.h"


// Prototypes defined in this file.
int trans_send(void * p_self, server_state * state, int client_id);
int trans_close(void * p_self, server_state * state, int client_id);
int trans_to_proc(handler_trans * self, server_state * state, int client_id);

int
initialize_handler_trans(handler_trans * handler) {
    handler -> state = handler -> in_buffer = handler -> head = 0;

    handler -> handle_send = trans_send;
    handler -> handle_closed = trans_close;

    return 0;
}

int
trans_send(void * p_self, server_state * state, int client_id) {
    handler_trans * self = (handler_trans *) p_self;
    printf("LOG: TRANS/SEND FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    // Here we are not at risk - active states (trans/rec) are not being pinged!
    pthread_mutex_unlock(&(conn -> mutex));

    if (self -> state == 0) {
        // First entry - load task, prepare the buffer and initialize communication.
        client_queue * q = &(state -> clients[client_id].task_queue);
        pthread_mutex_lock(&(q -> mutex));

        task new_task = q -> q_table[q -> q_head];
        self -> task_id = new_task.id;
        self -> fd = new_task.fd;
        self -> tb_transmitted = new_task.size;

        // Dequeue for real.
        q -> q_head = (q -> q_head + 1) % CLIENT_TASK_MAX;
        q -> q_full = 0;

        pthread_mutex_unlock(&(q -> mutex));

        // Prepare the buffer now.
        self -> transmitter_buffer[0] = TASK;
        serialize(self -> transmitter_buffer + 1, self -> task_id, ID_BYTES);
        serialize(self -> transmitter_buffer + 1 + ID_BYTES, self -> tb_transmitted, LEN_BYTES);

        self -> in_buffer = 1 + ID_BYTES + LEN_BYTES;
        self -> tb_transmitted += self -> in_buffer;

        self -> state += 1;
    }

    if (self -> state > 0) {
        if (self -> in_buffer == 0) {
            // Refill the buffer.
            self -> head = 0;
            int nr = read(self -> fd, self -> transmitter_buffer, MIN(TRANS_BUFFER_SIZE, self -> tb_transmitted));
            self -> in_buffer += nr;
        }

        int nw = send(conn -> socket_fd,
                self -> transmitter_buffer + self -> head,
                self -> in_buffer,
                MSG_DONTWAIT);
        self -> in_buffer -= nw;
        self -> tb_transmitted -= nw;
        self -> head += nw;

        if (self -> tb_transmitted == 0) {
            // Whole file was transmitted! Time to change the state...
            return trans_to_proc(self, state, client_id);
        }
    }

    return 0;
}

int
trans_cleanup(handler_trans * self, server_state * state, int client_id) {
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

    close(self -> fd);
    free(self);

    pthread_mutex_lock(&(conn -> mutex));
    conn -> state = INITIAL;
    pthread_mutex_unlock(&(conn -> mutex));

    del_event(state, client_id);
    add_event(state, client_id, EPOLLIN);

    // TODO: what about the task?
    return 0;
}

int
trans_close(void * p_self, server_state * state, int client_id) {
    handler_trans * self = (handler_trans *) p_self;
    printf("LOG: TRANS/CLOSE FOR ID: %d\n", client_id);
    return trans_cleanup(self, state, client_id);
}

int
trans_to_proc(handler_trans * self, server_state * state, int client_id) {
    printf("LOG: TRANS -> PROC FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    conn -> handler = (handler_proc *) malloc(sizeof(handler_proc));
    if (conn -> handler == NULL) {
        perror("swap connection handler");
        exit(EXIT_FAILURE);
    }
    if (initialize_handler_proc((handler_proc *) conn -> handler) == -1) {
        perror("initialize connection handler");
        exit(EXIT_FAILURE);
    }

    close(self -> fd);
    free(self);

    pthread_mutex_lock(&(conn -> mutex));
    conn -> state = PROCESSING;
    pthread_mutex_unlock(&(conn -> mutex));

    del_event(state, client_id);
    add_event(state, client_id, EPOLLIN);

    return 0;
}