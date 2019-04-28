#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include "protocol.h"
#include "queue.h"


int
get_queue(int msgflag) {
    char * home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "No HOME variable was found.\n");
        return -1;
    }

    key_t key = ftok(home, PROJ_ID);
    if (key == -1) {
        perror("Generate key: ");
        return -1;
    }

    int queue_id = msgget(key, msgflag);
    if (queue_id == -1) {
        perror("Get queue: ");
        return -1;
    }

    return queue_id;
}

int
get_private_queue(void) {
    int queue_id = (int) msgget(IPC_PRIVATE, S_IRUSR | S_IWUSR);
    if (queue_id == -1) {
        perror("Create private queue: ");
        return -1;
    }

    return queue_id;
}

void
remove_queue(int queue_id) {
    if (msgctl(queue_id, IPC_RMID, NULL) == -1) {
        if (errno == EINVAL) {
            printf("No queue to be removed.\n");
        } else {
            perror("Remove queue: ");
        }
    } else {
        printf("Successfully removed queue.\n");
    }
}

int
send_msg(int queue_id, int mflags, long mtype, int uid, char * content) {
    size_t msgsz = sizeof(msgcontent);
    msgbuf * msg = malloc(sizeof(msgbuf));
    if (msg == NULL) {
        int err = errno;
        perror("Allocate outgoing message memory: ");
        return err;
    }

    msg -> mtype = mtype;
    (msg -> mcontent).uid = uid;
    strncpy((msg -> mcontent).mtext, content, MAX_MSG_LEN);
    (msg -> mcontent).mtext[MAX_MSG_LEN - 1] = '\0';

    if (msgsnd(queue_id, msg, msgsz, mflags) == -1) {
        int err = errno;
        perror("Send message: ");
        free(msg);
        return err;
    }

    free(msg);
    return 0;
}


int
recv_msg(int queue_id, msgbuf * msg, size_t msgsz, long mtype, int mflags) {
    if (msgrcv(queue_id, msg, msgsz, mtype, mflags) == -1) {
        int err = errno;
        perror("Receive message: ");
        return err;
    }

    return 0;
}

int
get_msg_cnt(int queue_id, struct msqid_ds * buf) {
    if (msgctl(queue_id, IPC_STAT, buf) == -1) {
        perror("Get message queue stat: ");
        return -1;
    }

    return buf -> msg_qnum;
}
