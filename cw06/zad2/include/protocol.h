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
    int uid;
    char mtext[MAX_MSG_LEN];
} msgcontent;

typedef struct {
	long mtype;
	msgcontent mcontent;
} msgbuf;

typedef struct {
    msg_type mtype;
    char * mtext;
} msg;

msg process_cmd(char * command_text);

int send_cmd(int queue_id, int mflags, int uid, msg * command);

#endif // PROTOCOL_H
