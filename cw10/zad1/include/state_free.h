#include "server.h"

#ifndef STATE_FREE_H
#define STATE_FREE_H

struct handler_free;
typedef struct handler_free handler_free;

struct handler_free {
    unsigned char pong;
    int pinged;

    int (*handle_receive)(void * self, server_state * state, int client_id);
    int (*handle_closed)(void * self, server_state * state, int client_id);
    int (*ping)(void * self, server_state * state, int client_id);
};

int initialize_handler_free(handler_free * handler);

int free_to_busy(handler_free * self, server_state * state, int client_id);

#endif //STATE_FREE_H
