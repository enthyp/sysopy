#include "server.h"


#ifndef HANDLERS_H
#define HANDLERS_H

typedef struct {
    unsigned char * receiver_buffer;
    int rec_buf_head, tb_received;

    unsigned char transmitter_buffer[10?];
    int buf_head, inbuf_pending, tb_transmitted;

    int (*handle_receive)(server_state *, int client_id);
    int (*handle_send)(server_state *, int client_id);
} handler_initial;

typedef struct {
    unsigned char * receiver_buffer;
    int rec_buf_head, tb_received;

    unsigned char transmitter_buffer[10?];
    int buf_head, inbuf_pending, tb_transmitted;

    int (*handle_receive)(server_state *);
    int (*handle_send)(server_state *);
} handler_free;

int initialize_handler(handler_initial * handler);


#endif // HANDLERS_H
