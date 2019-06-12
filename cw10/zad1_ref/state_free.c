#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "server.h"
#include "state_initial.h"
#include "state_free.h"

// Prototypes defined in this file.
int free_receive(void * self, server_state * state, int client_id);
int free_send(void * self, server_state * state, int client_id);
int free_close(void * self, server_state * state, int client_id);
int free_evict(void * self, server_state * state, int client_id);


int
initialize_handler_free(handler_free * handler) {
    handler -> pong = 0;

    handler -> handle_receive = free_receive;
    handler -> handle_closed = free_close;
    handler -> handle_evict = free_evict;

    return 0;
}

int
free_receive(void * self, server_state * state, int client_id) {
    // TODO: handle ping! - server_state needs ping array with semaphores or sth
    fprintf(stderr, "UNEXPECTED! FREE/RECEIVE!!!");
    exit(EXIT_FAILURE);
}

int
free_cleanup(handler_free * self, server_state * state, int client_id) {
    client_conn * conn = &(state -> clients[client_id].connection);

    pthread_mutex_lock(&(conn -> mutex));

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

    pthread_mutex_unlock(&(conn -> mutex));

    return 0;
}

int
free_close(void * p_self, server_state * state, int client_id) {
    handler_free * self = (handler_free *) p_self;
    printf("LOG: FREE/CLOSE FOR ID: %d\n", client_id);
    return free_cleanup(self, state, client_id);
}

int
free_evict(void * p_self, server_state * state, int client_id) {
    handler_free * self = (handler_free *) p_self;
    printf("LOG: FREE/EVICT FOR ID: %d\n", client_id);
    return free_cleanup(self, state, client_id);
}

int
free_to_trans(handler_free * self, server_state * state, int client_id) {
    printf("LOG: FREE -> TRANS FOR ID: %d\n", client_id);
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

    // Mutex is locked outside, when enqueueing.
    conn -> state = TRANSMITTING;

    del_event(state, client_id);
    add_event(state, client_id, EPOLLOUT);

    return 0;
}