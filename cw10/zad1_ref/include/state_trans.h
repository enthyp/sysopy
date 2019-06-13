#include "server.h"

#ifndef STATE_TRANS_H
#define STATE_TRANS_H

#define TRANS_BUFFER_SIZE 1024


struct handler_trans;
typedef struct handler_trans handler_trans;

struct handler_trans {
    int state;

    // TODO: could add a flag so that pinger can check if transmission still going
    int fd, task_id;
    unsigned char transmitter_buffer[TRANS_BUFFER_SIZE];
    int tb_transmitted, in_buffer, head;

    int (*handle_send)(void * self, server_state * state, int client_id);
    int (*handle_closed)(void * self, server_state * state, int client_id);
};

int initialize_handler_trans(handler_trans * handler);

#endif // STATE_TRANS_H
