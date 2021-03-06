#include <mqueue.h>

#ifndef QUEUE_H
#define QUEUE_H

mqd_t get_named_queue(char * queue_name, int flags);

void remove_queue(char * queue_name, mqd_t queue_des);

long get_max_msgsz(mqd_t queue_des);

int recv_msg(mqd_t queue_des, char * msg_buf, char * content, int * mtype, int * uid, size_t max_msg_size);

int send_msg(mqd_t queue_des, char * msg_buf, char * content, int mtype, int uid, size_t max_msg_size);

#endif // QUEUE_H
