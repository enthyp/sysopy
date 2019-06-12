#include "server.h"

#ifndef STATE_PROC_H
#define STATE_PROC_H

struct handler_proc;
typedef struct handler_proc handler_proc;

struct handler_proc {
    unsigned char pong;

    int (*handle_receive)(void * self, server_state * state, int client_id);
    int (*handle_closed)(void * self, server_state * state, int client_id);
    int (*handle_evict)(void * self, server_state * state, int client_id);
};

int initialize_handler_proc(handler_proc * handler);

#endif // STATE_PROC_H
