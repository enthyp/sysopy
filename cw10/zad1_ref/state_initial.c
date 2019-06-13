#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

#include "state_initial.h"
#include "state_free.h"
#include "server.h"
#include "protocol.h"


// Prototypes defined in this file.
int check_name(server_state * state, int client_id, char * name);
void copy_str(unsigned char * from, char * to);
int initial_receive(void * self, server_state * state, int client_id);
int initial_send(void * self, server_state * state, int client_id);
int initial_close(void * self, server_state * state, int client_id);
int initial_to_free(handler_initial * self, server_state * state, int client_id);
int initial_cleanup(handler_initial * self, server_state * state, int client_id);


int
initialize_handler(handler_initial * handler) {
    handler -> state = handler -> return_code = 0;
    handler -> tb_received = CLIENT_NAME_MAX;

    handler -> handle_receive = initial_receive;
    handler -> handle_send = initial_send;
    handler -> handle_closed = initial_close;

    return 0;
}

int
initial_receive(void * p_self, server_state * state, int client_id) {
    handler_initial * self = (handler_initial *) p_self;
    printf("LOG: INITIAL/RECEIVE FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);
    // Here we are not at risk.
    pthread_mutex_unlock(&(conn -> mutex));

    if (self -> state == 0) {
        // First entry - check if message type OK.
        char mtype;
        read(conn -> socket_fd, &mtype, 1);

        if (mtype != REGISTER) {
            // Close connection and clean up - without responding.
            return initial_cleanup(self, state, client_id);
        } else {
            self -> state += 1;
        }
    }

    if (self -> state > 0) {
        // Subsequent entries - receive.
        int nr = recv(conn -> socket_fd, self -> receiver_buffer, self -> tb_received, MSG_DONTWAIT);
        if (nr == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0;
        }

        self -> tb_received -= nr;

        if (self -> tb_received == 0) {
            // ...and we're yet to check it - check it.
            client * client = &(state -> clients[client_id]);
            copy_str(self -> receiver_buffer, client -> name);

            if (check_name(state, client_id, client -> name) == 0) {
                // Name ok!
                self -> return_code = OK_REGISTER;
            } else {
                self -> return_code = NAME_EXISTS;
            }

            // So we stop listening and start trying to self -> handle_send the return code.
            del_event(state, client_id);
            add_event(state, client_id, EPOLLOUT);
        }
    }

    return 0;
}

int
check_name(server_state * state, int client_id, char * name) {
    int i;
    for (i = 0; i < CLIENTS_MAX; i++) {
        if (i != client_id) {
            client * client = &(state -> clients[i]);
            if (client -> connection.socket_fd != -1 && strcmp(name, client -> name) == 0) {
                printf("FOUND %d\n", i);
                return -1;
            }
        }
    }

    return 0;
}

// No-warning version of strcpy from unsigned char to char.
void
copy_str(unsigned char * from, char * to) {
    int i;
    for (i = 0; from[i] != '\0'; i++) {
        to[i] = from[i];
    }
    to[i] = '\0';
}

int
initial_send(void * p_self, server_state * state, int client_id) {
    handler_initial * self = (handler_initial *) p_self;
    printf("LOG: INITIAL/SEND FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);
    // Here we are not at risk.
    pthread_mutex_unlock(&(conn -> mutex));

    if (self -> return_code == OK_REGISTER) {
        send(conn -> socket_fd, &(self -> return_code), 1, MSG_DONTWAIT);
        return initial_to_free(self, state, client_id);
    }

    if (self -> return_code == NAME_EXISTS) {
        // ...and it was taken but we didn't manage to respond - then respond.
        send(conn -> socket_fd, &(self -> return_code), 1, MSG_DONTWAIT);
        return initial_cleanup(self, state, client_id);
    }

    fprintf(stderr, "UNEXPECTED: INITIAL/SEND OF %d!!!\n", self -> return_code);
    exit(EXIT_FAILURE);
}

int
initial_cleanup(handler_initial * self, server_state * state, int client_id) {
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
    self -> state = self -> return_code = 0;
    self -> tb_received = CLIENT_NAME_MAX;
    return 0;
}

int
initial_close(void * p_self, server_state * state, int client_id) {
    handler_initial * self = (handler_initial *) p_self;
    printf("LOG: INITIAL/CLOSE FOR ID: %d\n", client_id);
    return initial_cleanup(self, state, client_id);
}

int
initial_to_free(handler_initial * self, server_state * state, int client_id) {
    printf("LOG: INITIAL -> FREE FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    conn -> handler = (handler_free *) malloc(sizeof(handler_free));
    if (conn -> handler == NULL) {
        perror("swap connection handler");
        exit(EXIT_FAILURE);
    }
    if (initialize_handler_free((handler_free *) conn -> handler) == -1) {
        perror("initialize connection handler");
        exit(EXIT_FAILURE);
    }

    free(self);

    pthread_mutex_lock(&(conn -> mutex));
    conn -> state = FREE;
    pthread_mutex_unlock(&(conn -> mutex));

    del_event(state, client_id);
    add_event(state, client_id, EPOLLIN);

    return 0;
}