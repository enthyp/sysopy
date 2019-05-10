#include <mqueue.h>

#ifndef PROTOCOL_H
#define PROTOCOL_H

#define NAME_LEN 10

#define MAX_MSG_LEN 1024
#define MAX_CLIENTS 64

typedef enum {
    INIT,
    ECHO,
    LIST,
    FRIENDS,
    TO_ALL,
    TO_FRIENDS,
    TO_ONE,
    ADD,
    DEL,
    READ,
    STOP,
    SERVER_STOP
} msg_type;

typedef struct {
    msg_type mtype;
    char * mtext;
} msg;

msg process_cmd(char * command_text);

int send_cmd(mqd_t queue_des, int uid, msg * command, size_t max_msg_size);

#endif // PROTOCOL_H
