#ifndef PROTOCOL_H
#define PROTOCOL_H

/***/

#define PROJ_ID 1

#define MAX_MSG_LEN 1024
#define MAX_CLIENTS 64

/***/

// Message types.
#define INIT 1
#define ECHO 2
#define LIST 3
#define FRIENDS 4
#define TO_ALL 5
#define TO_FRIENDS 6
#define TO_ONE 7
#define STOP 8

#define SERVER_STOP 65

/***/

typedef struct {
	long mtype;
	int uid;
	char mtext[0];
} msgbuf;

#endif /* protocol */
