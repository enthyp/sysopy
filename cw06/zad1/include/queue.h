#include <sys/msg.h>

#ifndef QUEUE_H
#define QUEUE_H

int get_queue(int msgflag);

int get_private_queue(void);

void remove_queue(int queue_id);

int recv_msg(int queue_id, msgbuf * msg, size_t msgsz, long mtype, int mflags);

int send_msg(int queue_id, int mflags, long mtype, int uid, char * content);

#endif // QUEUE_H
