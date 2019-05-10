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
    int queue_des = mq_open(queue_name, flags, S_IRUSR | S_IWUSR, NULL);
    return queue_des;
}

char *
get_server_queue_name(void) {
    char * home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "No HOME variable was found.\n");
        return NULL;
    }

    int home_length = strlen(home);
    char * queue_name = (char *) malloc((home_length + 1 + 1) * sizeof(char));
    if (queue_name == NULL) {
        fprintf(stderr, "Failed to allocate memory for server queue name.\n");
        return NULL;
    }

    strcpy(queue_name, "/");
    strcat(queue_name, home);

    return queue_name;
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
send_msg(mqd_t queue_des,  char * content, int mtype, int uid, size_t max_msg_size) {
    char * msg = (char *) malloc(max_msg_size);
    if (msg == NULL) {
        fprintf(stderr, "Failed to allocate memory for outgoing message.\n");
        return -1;
    }

    msg[0] = (char) mtype;
    msg[1] = (char) uid;
    strncpy(msg + 2, content, max_msg_size - 2);
    msg[max_msg_size - 1] = '\0';

    if (mq_send(queue_des, msg, strlen(msg), (mtype == STOP) ? 1 : 0) == -1) {
        perror("Send message");
        free(msg);
        return -1;
    }

    free(msg);
    return 0;
}

int
recv_msg(mqd_t queue_des, char ** content, int * mtype, int * uid, size_t max_msg_size) {
    char * msg = (char *) malloc(max_msg_size);
    if (msg == NULL) {
        fprintf(stderr, "Failed to allocate memory for incoming message.\n");
        return -1;
    }

    if (mq_receive(queue_des, msg, max_msg_size, NULL) == -1) {
        int err = errno;
        free(msg);
        return err;
    }

    *mtype = (int) msg[0];
    *uid = (int) msg[1];

    *content = (char *) malloc(max_msg_size - 2);
    if (*content == NULL) {
        fprintf(stderr, "Failed to allocate memory for incoming message content.\n");
        return -1;
    }

    strcpy(*content, msg + 2);
    return 0;
}

int
set_notification(mqd_t queue_des, int sig) {
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = sig;

    if (mq_notify(queue_des, &sev) == -1) {
        perror("Set notification");
        return -1;
    }

    return 0;
}
