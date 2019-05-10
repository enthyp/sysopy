#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "protocol.h"
#include "queue.h"

mqd_t
get_named_queue(char * queue_name, int flags) {
    mqd_t queue_des = mq_open(queue_name, flags, S_IRUSR | S_IWUSR, NULL);
    if (queue_des == (mqd_t) -1) {
        perror("Get named queue");
    }
    return queue_des;
}

void
remove_queue(char * queue_name, mqd_t queue_des) {
    mq_close(queue_des);

    if (mq_unlink(queue_name) == -1) {
        if (errno == ENOENT) {
            printf("No queue to be removed.\n");
        } else {
            perror("Remove queue");
        }
    } else {
        printf("Successfully removed queue.\n");
    }
}

long
get_max_msgsz(mqd_t queue_des) {
    struct mq_attr attr;
    if (mq_getattr(queue_des, &attr) == -1) {
        perror("Get message queue attributes");
        return -1;
    }

    return attr.mq_msgsize;
}

int
send_msg(mqd_t queue_des, char * msg_buf, char * content, int mtype, int uid, size_t max_msg_size) {
    msg_buf[0] = (char) mtype;
    msg_buf[1] = (char) uid;

    strncpy(msg_buf + 2, content, max_msg_size - 2);
    msg_buf[max_msg_size - 1] = '\0';

    if (mq_send(queue_des, msg_buf, strlen(msg_buf), (mtype == STOP) ? 1 : 0) == -1) {
        perror("Send message");
        return -1;
    }

    return 0;
}

int
recv_msg(mqd_t queue_des, char * msg_buf, char * content, int * mtype, int * uid, size_t max_msg_size) {
    ssize_t nr;
    if ((nr = mq_receive(queue_des, msg_buf, max_msg_size, NULL)) == -1) {
        return errno;
    }

    *mtype = (int) msg_buf[0];
    *uid = (int) msg_buf[1];
    strncpy(content, msg_buf + 2, nr - 2);
    if (nr == max_msg_size) {
        content[max_msg_size -3] = '\0';
    } else {
        content[nr - 2] = '\0';
    }

    return 0;
}
