#include <mqueue.h>

#ifndef QUEUE_H
#define QUEUE_H

mqd_t get_named_queue(char * queue_name, int flags);

char * get_server_queue_name(void);

void remove_queue(char * queue_name, mqd_t queue_des);

long get_max_msgsz(mqd_t queue_des);

int set_notification(mqd_t queue_des, int sig);

int recv_msg(mqd_t queue_des, char ** content, int * mtype, int * uid, size_t max_msg_size);

int send_msg(mqd_t queue_des, char * content, int mtype, int uid, size_t max_msg_size);

#endif // QUEUE_H
