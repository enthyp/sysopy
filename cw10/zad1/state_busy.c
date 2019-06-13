#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "server.h"
#include "protocol.h"
#include "state_initial.h"
#include "state_trans.h"
#include "state_busy.h"

// Prototypes defined in this file.
int busy_send(void * p_self, server_state * state, int client_id);
int busy_receive(void * p_self, server_state * state, int client_id);
int busy_close(void * p_self, server_state * state, int client_id);
int busy_to_trans(handler_busy * self, server_state * state, int client_id);
int busy_ping(void * self, server_state * state, int client_id);

int
initialize_handler_busy(handler_busy * handler) {
    handler -> pong = handler -> pinged = 0;

    handler -> handle_send = busy_send;
    handler -> handle_receive = busy_receive;
    handler -> handle_closed = busy_close;
    handler -> ping = busy_ping;

    return 0;
}

int
busy_receive(void * p_self, server_state * state, int client_id) {
    handler_busy * self = (handler_busy *) p_self;
    printf("LOG: PROC/RECEIVE FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    read(conn -> socket_fd, &(self -> pong), 1);
    if (self -> pong != PONG) {
        return busy_close(self, state, client_id);
    } else {
        self -> pinged = 0;
    }

    return 0;
}

int
busy_send(void * p_self, server_state * state, int client_id) {
    handler_busy *self = (handler_busy *) p_self;
    printf("LOG: BUSY/SEND FOR ID: %d\n", client_id);

    self -> pinged = 0;

    return busy_to_trans(self, state, client_id);
}

int
busy_cleanup(handler_busy * self, server_state * state, int client_id) {
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

    free(self);

    pthread_mutex_lock(&(conn -> mutex));
    conn -> state = INITIAL;
    pthread_mutex_unlock(&(conn -> mutex));

    del_event(state, client_id);
    add_event(state, client_id, EPOLLIN);

    return 0;
}

int
busy_close(void * p_self, server_state * state, int client_id) {
    handler_busy * self = (handler_busy *) p_self;
    printf("LOG: BUSY/CLOSE FOR ID: %d\n", client_id);
    return busy_cleanup(self, state, client_id);
}


int
busy_to_trans(handler_busy * self, server_state * state, int client_id) {
    printf("LOG: BUSY -> TRANS FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    conn -> handler = (handler_trans *) malloc(sizeof(handler_trans));
    if (conn -> handler == NULL) {
        perror("swap connection handler");
        exit(EXIT_FAILURE);
    }
    if (initialize_handler_trans((handler_trans *) conn -> handler) == -1) {
        perror("initialize connection handler");
        exit(EXIT_FAILURE);
    }

    free(self);
    conn -> state = TRANSMITTING;

    del_event(state, client_id);
    add_event(state, client_id, EPOLLOUT);

    return 0;
}

int
busy_ping(void * p_self, server_state * state, int client_id) {
    handler_busy * self = (handler_busy *) p_self;
    printf("LOG: BUSY/CLOSE FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    self -> pong = PING;
    if (send(conn -> socket_fd, &(self -> pong), 1, MSG_DONTWAIT) == -1) {
        fprintf(stderr, "ERR: FAILED TO BUSY/PING FOR ID: %d\n", client_id);
    }

    self -> pinged = 1;
    return 0;
}
