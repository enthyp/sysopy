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

int
handle_close(void * handler, server_state * s_state, int client_id, conn_state c_state) {
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
            return ((handler_proc * )handler)->handle_closed(handler, s_state, client_id);
        }
        case RECEIVING: {
            return ((handler_recv * )handler)->handle_closed(handler, s_state, client_id);
        }
        default: fprintf(stderr, "ERR ROUTING CLOSE FOR state %d!\n", c_state); exit(EXIT_FAILURE);
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
            return ((handler_proc * )handler)->handle_receive(handler, s_state, client_id);
        }
        case RECEIVING: {
            return ((handler_recv * )handler)->handle_receive(handler, s_state, client_id);
        }
        default: fprintf(stderr, "ERR ROUTING RECEIVE FOR state %d!\n", c_state); exit(EXIT_FAILURE);
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
        default: fprintf(stderr, "ERR ROUTING SEND FOR state %d!\n", c_state); exit(EXIT_FAILURE);
    }
}

int
pinged(void * handler, server_state * s_state, int client_id, conn_state c_state) {
    switch (c_state) {
        case INITIAL: {
            return ((handler_initial * )handler)->pinged;
        }
        case FREE: {
            return ((handler_free * )handler)->pinged;
        }
        case BUSY: {
            return ((handler_busy * )handler)->pinged;
        }
        case TRANSMITTING: {
            return ((handler_trans * )handler)->pinged;
        }
        case PROCESSING: {
            return ((handler_proc * )handler)->pinged;
        }
        case RECEIVING: {
            return ((handler_recv * )handler)->pinged;
        }
        default: fprintf(stderr, "ERR ROUTING PINGED FOR state %d!\n", c_state); exit(EXIT_FAILURE);
    }
}

int
ping_client(void * handler, server_state * s_state, int client_id, conn_state c_state) {
    switch (c_state) {
        case INITIAL: {
            return ((handler_initial * )handler)->ping(handler, s_state, client_id);
        }
        case FREE: {
            return ((handler_free * )handler)->ping(handler, s_state, client_id);
        }
        case BUSY: {
            return ((handler_busy * )handler)->ping(handler, s_state, client_id);
        }
        case TRANSMITTING: {
            return ((handler_trans * )handler)->ping(handler, s_state, client_id);
        }
        case PROCESSING: {
            return ((handler_proc * )handler)->ping(handler, s_state, client_id);
        }
        case RECEIVING: {
            return ((handler_recv * )handler)->ping(handler, s_state, client_id);
        }
        default: fprintf(stderr, "ERR ROUTING PING FOR state %d!\n", c_state); exit(EXIT_FAILURE);
    }
}

#endif // STATES_H
