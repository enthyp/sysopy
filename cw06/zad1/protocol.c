#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "queue.h"

int
send_cmd(int queue_id, int mflags, int uid, char * cmd) {
    char * prefix = strtok(cmd, " ");

    if (prefix != NULL) {
        long mtype;
        if (strcmp(prefix, "ECHO") == 0) {
            mtype = ECHO;
        } else if (strcmp(prefix, "STOP") == 0) {
            mtype = STOP;
        } else {
            fprintf(stderr, "Unknown command type.\n");
            return -1;
        }

        char * content = strtok(NULL, "");
        if (content == NULL) {
            return send_msg(queue_id, mflags, mtype, uid, "");
        }

        return send_msg(queue_id, mflags, mtype, uid, content);
    }

    fprintf(stderr, "No command type given.\n");
    return -1;
}