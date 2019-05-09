#include <mqueue.h>

#ifndef QUEUE_H
#define QUEUE_H

msq_d get_named_queue(char * queue_name, int flags);

char * get_server_queue_name(void);

void remove_queue(char * queue_name, msq_d queue_des);

int recv_msg(msq_d queue_des, char ** content, int * mtype, int * uid, size_t max_msg_size);

int send_msg(msq_d queue_des, char * content, int mtype, int uid, size_t max_msg_size);

#endif // QUEUE_H
