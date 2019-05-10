#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "queue.h"
#include "util.h"

msg
process_cmd(char * command_text) {
    // Strip newline character.
    int cmd_text_length = strlen(command_text);
    if (command_text[cmd_text_length - 1] == '\n') {
        command_text[cmd_text_length - 1] = '\0';
    }

    msg command = { .mtype = -1, .mtext = NULL };
    char * prefix = strtok(command_text, " ");

    if (prefix != NULL) {
        if (strcmp(prefix, "ECHO") == 0) {
            command.mtype = ECHO;
        } else if (strcmp(prefix, "STOP") == 0) {
            command.mtype = STOP;
        } else if (strcmp(prefix, "LIST") == 0) {
            command.mtype = LIST;
        } else if (strcmp(prefix, "FRIENDS") == 0) {
            command.mtype = FRIENDS;
        } else if (strcmp(prefix, "2ALL") == 0) {
            command.mtype = TO_ALL;
        } else if (strcmp(prefix, "2FRIENDS") == 0) {
            command.mtype = TO_FRIENDS;
        } else if (strcmp(prefix, "2ONE") == 0) {
            command.mtype = TO_ONE;
        } else if (strcmp(prefix, "ADD") == 0) {
            command.mtype = ADD;
        } else if (strcmp(prefix, "DEL") == 0) {
            command.mtype = DEL;
        } else if (strcmp(prefix, "READ") == 0) {
            command.mtype = READ;
        } else if (is_empty(prefix)) {
            return command;
        } else {
            fprintf(stderr, ">>> ERR: unknown command type.\n");
            return command;
        }

        char * content = strtok(NULL, "");
        if (content == NULL) {
            if (command.mtype == ADD) {
                fprintf(stderr, ">>> ERR: ADD must have parameters.\n");
                command.mtype = -1;
            } else if (command.mtype == DEL) {
                fprintf(stderr, ">>> ERR: DEL must have parameters.\n");
                command.mtype = -1;
            } else {
                command.mtext = "";
            }
        } else {
            command.mtext = content;
        }
    }

    return command;
}

int
send_cmd(mqd_t queue_des, int uid, msg * command, size_t max_msg_size) {
    if (command -> mtype == -1) {
        return -1;
    }

    return send_msg(queue_des, command -> mtext, command -> mtype, uid, max_msg_size);
}
