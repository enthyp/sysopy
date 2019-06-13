#include "server.h"


#ifndef HANDLERS_H
#define HANDLERS_H

typedef struct {
    int state;

    unsigned char receiver_buffer[CLIENT_NAME_MAX];
    int tb_received;

    unsigned char return_code;

    int (*handle_receive)(handler_initial * self, server_state * state, int client_id);
    int (*handle_send)(handler_initial * self, server_state * state, int client_id);
    int (*handle_closed)(handler_initial * self, server_state * state, int client_id);
} handler_initial;

typedef struct {
    unsigned char * receiver_buffer;
    int rec_buf_head, tb_received;

    unsigned char transmitter_buffer[10?];
    int buf_head, inbuf_pending, tb_transmitted;

    int (*handle_receive)(handler_initial * self, server_state * state, int client_id);
    int (*handle_send)(handler_initial * self, server_state * state, int client_id);
    int (*handle_closed)(handler_initial * self, server_state * state, int client_id);
    int (*handle_evict)(handler_initial * self, server_state * state, int client_id);
} handler_free;

int initialize_handler(void * handler);

#endif // HANDLERS_H
