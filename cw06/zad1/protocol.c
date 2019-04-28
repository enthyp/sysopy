#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "protocol.h"
#include "queue.h"

int is_empty(char * s) {
    while (*s != '\0') {
        if (!isspace((unsigned char) *s)) {
            return 0;
        }
        s++;
    }

    return 1;
}

msg
process_cmd(char * command_text) {
    msg command = { .mtype = -1, .mtext = NULL };
    char * prefix = strtok(command_text, " ");

    if (prefix != NULL) {
        if (strcmp(prefix, "ECHO") == 0) {
            command.mtype = ECHO;
        } else if (strcmp(prefix, "STOP") == 0) {
            command.mtype = STOP;
        } else if (is_empty(prefix)) {
            return command;
        } else {
            fprintf(stderr, "Unknown command type.\n");
            return command;
        }

        // TODO: handle ADD and DEL without args!
        char * content = strtok(NULL, "");
        command.mtext = (content == NULL) ? "" : content;
    }

    return command;
}

int
send_cmd(int queue_id, int mflags, int uid, msg * command) {
    if (command -> mtype == -1) {
        return -1;
    }

    return send_msg(queue_id, mflags, command -> mtype, uid, command -> mtext);
}

