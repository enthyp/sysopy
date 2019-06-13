#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "state_initial.h"
#include "state_free.h"
#include "state_recv.h"
#include "state_proc.h"
#include "server.h"
#include "protocol.h"

int proc_receive(void * p_self, server_state * state, int client_id);
int proc_evict(void * p_self, server_state * state, int client_id);
int proc_close(void * p_self, server_state * state, int client_id);
int proc_to_recv(handler_proc * self, server_state * state, int client_id);

int
initialize_handler_proc(handler_proc * handler) {
    handler -> pong = 0;

    handler -> handle_receive = proc_receive;
    handler -> handle_closed = proc_close;
    handler -> handle_evict = proc_evict;

    return 0;
}

int
proc_receive(void * p_self, server_state * state, int client_id) {
    handler_proc * self = (handler_proc *) p_self;
    printf("LOG: PROC/RECEIVE FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    read(conn -> socket_fd, &(self -> pong), 1);
    if (self -> pong != PONG && self -> pong != TASK_RESULT) {
        return proc_evict(self, state, client_id);
    }

    if (self -> pong == PONG) {
        // TODO: handle pong!
    }

    if (self -> pong == TASK_RESULT) {
        return proc_to_recv(self, state, client_id);
    }

    return 0;
}

int
proc_cleanup(handler_proc * self, server_state * state, int client_id) {
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
proc_close(void * p_self, server_state * state, int client_id) {
    handler_proc * self = (handler_proc *) p_self;
    printf("LOG: PROC/CLOSE FOR ID: %d\n", client_id);
    return proc_cleanup(self, state, client_id);
}

int proc_evict(void * p_self, server_state * state, int client_id) {
    handler_proc * self = (handler_proc *) p_self;
    printf("LOG: PROC/EVICT FOR ID: %d\n", client_id);
    return proc_cleanup(self, state, client_id);
}

int
proc_to_recv(handler_proc * self, server_state * state, int client_id) {
    printf("LOG: PROC -> RECV FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    conn -> handler = (handler_recv *) malloc(sizeof(handler_recv));
    if (conn -> handler == NULL) {
        perror("swap connection handler");
        exit(EXIT_FAILURE);
    }
    if (initialize_handler_recv((handler_recv *) conn -> handler) == -1) {
        perror("initialize connection handler");
        exit(EXIT_FAILURE);
    }

    free(self);

    pthread_mutex_lock(&(conn -> mutex));
    conn -> state = RECEIVING;
    pthread_mutex_unlock(&(conn -> mutex));

    del_event(state, client_id);
    add_event(state, client_id, EPOLLIN);

    return 0;
}