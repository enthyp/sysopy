#include <stdio.h>
#include "server.h"
#include "state_initial.h"
#include "state_free.h"
#include "state_busy.h"
#include "state_trans.h"
#include "state_proc.h"
#include "state_recv.h"

#ifndef STATES_H
#define STATES_H

// TODO: delete dead routes
int
handle_closed(void * handler, server_state * s_state, int client_id, conn_state c_state) {
    switch (c_state) {
        case INITIAL: {
            return ((handler_initial * )handler)->handle_closed(handler, s_state, client_id);
        }
        case BUSY: {
            return ((handler_busy * )handler)->handle_closed(handler, s_state, client_id);
        }
        case FREE: {
            return ((handler_free * )handler)->handle_closed(handler, s_state, client_id);
        }
        case TRANSMITTING: {
            return ((handler_trans * )handler)->handle_closed(handler, s_state, client_id);
        }
        case PROCESSING: {
            return ((handler_free * )handler)->handle_closed(handler, s_state, client_id);
        }
        case RECEIVING: {
            return ((handler_free * )handler)->handle_closed(handler, s_state, client_id);
        }
        default: fprintf(stderr, "ERR ROUTING!\n"); exit(EXIT_FAILURE);
    }
}

int
handle_receive(void * handler, server_state * s_state, int client_id, conn_state c_state) {
    switch (c_state) {
        case INITIAL: {
            return ((handler_initial * )handler)->handle_receive(handler, s_state, client_id);
        }
        case FREE: {
            return ((handler_free * )handler)->handle_receive(handler, s_state, client_id);
        }
        case BUSY: {
            return ((handler_busy * )handler)->handle_receive(handler, s_state, client_id);
        }
        case PROCESSING: {
            return ((handler_free * )handler)->handle_receive(handler, s_state, client_id);
        }
        case RECEIVING: {
            return ((handler_free * )handler)->handle_receive(handler, s_state, client_id);
        }
        default: fprintf(stderr, "ERR ROUTING!\n"); exit(EXIT_FAILURE);
    }
}

int
handle_send(void * handler, server_state * s_state, int client_id, conn_state c_state) {
    switch (c_state) {
        case INITIAL: {
            return ((handler_initial * )handler)->handle_send(handler, s_state, client_id);
        }
        case BUSY: {
            return ((handler_busy * )handler)->handle_send(handler, s_state, client_id);
        }
        case TRANSMITTING: {
            return ((handler_trans * )handler)->handle_send(handler, s_state, client_id);
        }
        case RECEIVING: {
            return ((handler_initial * )handler)->handle_send(handler, s_state, client_id);
        }
        default: fprintf(stderr, "ERR ROUTING!\n"); exit(EXIT_FAILURE);
    }
}

#endif //STATES_H
