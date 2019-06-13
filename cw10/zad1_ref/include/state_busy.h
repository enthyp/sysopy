#include "server.h"

#ifndef STATE_BUSY_H
#define STATE_BUSY_H

struct handler_busy;
typedef struct handler_busy handler_busy;

struct handler_busy {
    unsigned char pong;

    int (*handle_receive)(void * self, server_state * state, int client_id);
    int (*handle_send)(void * self, server_state * state, int client_id);
    int (*handle_closed)(void * self, server_state * state, int client_id);
    int (*handle_evict)(void * self, server_state * state, int client_id);
};

int initialize_handler_busy(handler_busy * handler);

#endif // STATE_BUSY_H
