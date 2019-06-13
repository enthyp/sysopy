#include "server.h"

#ifndef STATE_INITIAL_H
#define STATE_INITIAL_H

struct handler_initial;
typedef struct handler_initial handler_initial;

struct handler_initial {
    int state;
    int pinged;

    unsigned char receiver_buffer[CLIENT_NAME_MAX];
    int tb_received;

    unsigned char return_code;

    int (*handle_receive)(void * self, server_state * state, int client_id);
    int (*handle_send)(void * self, server_state * state, int client_id);
    int (*handle_closed)(void * self, server_state * state, int client_id);
    int (*ping)(void * self, server_state * state, int client_id);
};

int initialize_handler(handler_initial * handler);

#endif // STATE_INITIAL_H
