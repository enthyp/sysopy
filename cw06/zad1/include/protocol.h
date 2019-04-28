#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROJ_ID 1

#define MAX_MSG_LEN 1024
#define MAX_CLIENTS 64

typedef enum {
    INIT = 1,
    ECHO,
    LIST,
    FRIENDS,
    TO_ALL,
    TO_FRIENDS,
    TO_ONE,
    STOP,
    SERVER_STOP = 65
} msg_type;

int send_cmd(int queue_id, int mflags, int uid, char * cmd);

typedef struct {
    int uid;
    char mtext[MAX_MSG_LEN];
} msgcontent;

typedef struct {
	long mtype;
	msgcontent mcontent;
} msgbuf;

#endif // PROTOCOL_H
