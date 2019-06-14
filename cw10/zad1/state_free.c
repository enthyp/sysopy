#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>

#include "server.h"
#include "state_initial.h"
#include "state_free.h"
#include "state_trans.h"
#include "state_busy.h"
#include "protocol.h"


// Prototypes defined in this file.
int free_receive(void * self, server_state * state, int client_id);
int free_send(void * self, server_state * state, int client_id);
int free_close(void * self, server_state * state, int client_id);
int free_ping(void * self, server_state * state, int client_id);


int
initialize_handler_free(handler_free * handler) {
    handler -> pong = 0;
    handler -> pinged = 0;

    handler -> handle_receive = free_receive;
    handler -> handle_closed = free_close;
    handler -> ping = free_ping;

    return 0;
}

int
free_receive(void * p_self, server_state * state, int client_id) {
    handler_free * self = (handler_free *) p_self;
    client_conn * conn = &(state -> clients[client_id].connection);
    printf("LOG: FREE/RECEIVE FOR ID: %d\n", client_id);

    read(conn -> socket_fd, &(self -> pong), 1);
    if (self -> pong == PONG) {
        self -> pinged = 0;
    } else {
        fprintf(stderr, "ERR: UNEXPECTED MSG IN FREE/RECEIVE FOR ID: %d\n", client_id);
    }

    return 0;
}

int
free_cleanup(handler_free * self, server_state * state, int client_id) {
    client_conn * conn = &(state -> clients[client_id].connection);

    // Erase socket and its events.
    del_event(state, client_id);
    close(conn -> socket_fd);
    conn -> socket_fd = -1;

    // State back to INITIAL.
    conn -> state = INITIAL;

    // Decrement client count.
    pthread_mutex_lock(&(state -> count_mutex));
    state -> client_count -= 1;
    pthread_mutex_unlock(&(state -> count_mutex));

    // Reinitialize handler.
    conn -> handler = (handler_initial *) malloc(sizeof(handler_initial));
    if (conn -> handler == NULL) {
        perror("add connection handler");
        exit(EXIT_FAILURE);
    }
    if (initialize_handler((handler_initial *) conn -> handler) == -1) {
        perror("initialize connection handler");
        exit(EXIT_FAILURE);
    }

    free(self);

    return 0;
}

int
free_close(void * p_self, server_state * state, int client_id) {
    handler_free * self = (handler_free *) p_self;
    printf("LOG: FREE/CLOSE FOR ID: %d\n", client_id);
    return free_cleanup(self, state, client_id);
}

int
free_to_busy(handler_free * self, server_state * state, int client_id) {
    printf("LOG: FREE -> BUSY FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    self -> pinged = 0;

    conn -> handler = (handler_busy *) malloc(sizeof(handler_busy));
    if (conn -> handler == NULL) {
        perror("swap connection handler");
        exit(EXIT_FAILURE);
    }
    if (initialize_handler_busy((handler_busy *) conn -> handler) == -1) {
        perror("initialize connection handler");
        exit(EXIT_FAILURE);
    }

    free(self);
    conn -> state = BUSY;

    del_event(state, client_id);
    add_event(state, client_id, EPOLLOUT);

    return 0;
}

int
free_ping(void * p_self, server_state * state, int client_id) {
    handler_free * self = (handler_free *) p_self;
    printf("LOG: FREE/PING FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    self -> pong = PING;
    if (send(conn -> socket_fd, &(self -> pong), 1, MSG_DONTWAIT) == -1) {
        fprintf(stderr, "ERR: FAILED TO FREE/PING FOR ID: %d\n", client_id);
    }

    self -> pinged = 1;
    return 0;
}
