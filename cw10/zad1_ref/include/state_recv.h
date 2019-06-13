#include "server.h"

#ifndef STATE_RECV_H
#define STATE_RECV_H


struct handler_recv;
typedef struct handler_recv handler_recv;

struct handler_recv {
    int state;

    int task_id;
    unsigned char * receiver_buffer;
    int tb_received, head;

    int (*handle_receive)(void * self, server_state * state, int client_id);
    int (*handle_closed)(void * self, server_state * state, int client_id);
    // TODO: add evict - based on pinger flag!
};

int initialize_handler_recv(handler_recv * handler);

#endif // STATE_RECV_H
