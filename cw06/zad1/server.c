#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <signal.h>
#include "protocol.h"
#include "util.h"
#include "queue.h"
#include <unistd.h>
#include <errno.h>
#include "friends.h"

// globals

int g_client_queue_ids[MAX_CLIENTS + 1];
friends_collection g_client_friends;
int g_server_queue_id = -1;
msgbuf * g_msg = NULL;
size_t g_msgsz = sizeof(msgbuf) - sizeof(long) + MAX_MSG_LEN;

// prototypes defined in this file

void dispatch_add(msgbuf *);
void dispatch_del(msgbuf *);
void dispatch_echo(msgbuf *);
void dispatch_friends(msgbuf *);
void dispatch_init(msgbuf *);
void dispatch_list(msgbuf *);
void dispatch_msg(msgbuf *);
void dispatch_stop(msgbuf *);
void dispatch_to_all(msgbuf *);
void dispatch_to_friends(msgbuf *);
void dispatch_to_one(msgbuf *);
void e_handler(void);
int find_slot(int);
void get_client_list(char *);
void sigint_handler(int);

// definitions

void
setup(void) {
    if (base_setup(e_handler, sigint_handler) == -1) {
        exit(EXIT_FAILURE);
    }

	// Get server queue.
	g_server_queue_id = get_queue(IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	if (g_server_queue_id == -1) {
		exit(EXIT_FAILURE);
	} else {
		printf("Server queue ID: %d\n", g_server_queue_id);
	}

	// Get incoming message memory.
	g_msg = malloc(sizeof(msgbuf) + MAX_MSG_LEN);
	if (g_msg == NULL) {
		perror(">>> ERR: allocate message memory: ");
		exit(EXIT_FAILURE);
	}

	// Allocate friends memory.
	if (setup_friends(&g_client_friends, MAX_CLIENTS + 1) == -1) {
	    fprintf(stderr, "Failed to setup friends collection.\n");
	    exit(EXIT_FAILURE);
	}
}

void
e_handler(void) {
    remove_queue(g_server_queue_id);
    teardown_friends(&g_client_friends);
    if (g_msg != NULL) {
        free(g_msg);
    }
}

void 
sigint_handler(int sig) {
	printf("Received SIGINT.\n");

	int i, count = 0;
	for (i = 1; i <= MAX_CLIENTS; i++) {
		if (g_client_queue_ids[i] != 0 && send_msg(g_client_queue_ids[i], IPC_NOWAIT, SERVER_STOP, 0, "") == 0) {
			count++;
		}
	}

    while (count > 0) {
	    int res;
		if ((res = recv_msg(g_server_queue_id, g_msg, g_msgsz, STOP, MSG_NOERROR)) == 0) {
			count--;
		} else {
		    fprintf(stderr, ">>> ERR: error waiting for clients to stop: %s\n", strerror(res));
            break;
		}
	}

    if (count == 0)
		exit(EXIT_SUCCESS);
	else 
		exit(EXIT_FAILURE);
}

void
listen(void) {		
	while (1) {
        if (recv_msg(g_server_queue_id, g_msg, g_msgsz, STOP, IPC_NOWAIT) != 0) {
            if (errno != ENOMSG) {
                exit(EXIT_FAILURE);
            }
        } else {
            dispatch_msg(g_msg);
        }

		if (recv_msg(g_server_queue_id, g_msg, g_msgsz, 0, MSG_NOERROR) != 0) {
			exit(EXIT_FAILURE);
		}
		
		dispatch_msg(g_msg);
		sleep(3);
	}
}

void 
dispatch_msg(msgbuf * msg) {
	switch (msg -> mtype) {
		case INIT: dispatch_init(msg); break;
		case ECHO: dispatch_echo(msg); break;
		case LIST: dispatch_list(msg); break;
		case FRIENDS: dispatch_friends(msg); break;
		case ADD: dispatch_add(msg); break;
		case DEL: dispatch_del(msg); break;
		case TO_ALL: dispatch_to_all(msg); break;
		case TO_FRIENDS: dispatch_to_friends(msg); break;
		case TO_ONE: dispatch_to_one(msg); break;
		case STOP: dispatch_stop(msg); break;
		default: fprintf(stderr, ">>> ERR: unknown message type: %ld\n", msg -> mtype);
	}
}

void
dispatch_init(msgbuf * msg) {
	errno = 0;
	int client_queue_id = (int) strtol((msg -> mcontent).mtext, NULL, 10);
	if (errno != 0) {
		fprintf(stderr, ">>> ERR: incorrect user queue ID: %s\n", (msg -> mcontent).mtext);
		return;
	}
	printf(">>> INIT from queue ID: %d\n", client_queue_id);	

	int client_id = find_slot(client_queue_id);
	if (client_id == -1) {
		return;
	}
	
	g_client_queue_ids[client_id] = client_queue_id;

	char msg_content[3] = {'\0'};
	sprintf(msg_content, "%d", client_id);

	if (send_msg(client_queue_id, IPC_NOWAIT, client_id, 0, msg_content) != 0) {
		g_client_queue_ids[client_id] = 0;
		return;
	}
}

int
find_slot(int client_queue_id) {
	// Could be more efficient with a hash set of free IDs + hash map of queue IDs but no one cares.
	int i, slot = -1;
	int exists = 0;
	for (i = 1; i <= MAX_CLIENTS; i++) {
		if (slot == -1 && g_client_queue_ids[i] == 0) {
			slot = i;
		}
		if (g_client_queue_ids[i] == client_queue_id) {
			exists = 1;
		}
	}
	
	if (exists) {
		fprintf(stderr, ">>> ERR: repeated INIT for client queue %d.\n", client_queue_id);
		return -1;
	}

	if (slot == -1) {
		fprintf(stderr, ">>> ERR: limit of connections reached.\n");
		return -1;
	}

	return slot;
}

void
dispatch_echo(msgbuf * msg) {
    int client_id = (msg -> mcontent).uid;
    printf(">>> ECHO from ID: %d\n", client_id);

    char * response = NULL;
    if (prefix_date((msg -> mcontent).mtext, &response) == -1) {
        return;
    }
    send_msg(g_client_queue_ids[client_id], IPC_NOWAIT, client_id, 0, response);
    free(response);
}

void
dispatch_list(msgbuf * msg) {
    int client_id = (msg -> mcontent).uid;
    printf(">>> LIST from ID: %d\n", client_id);

    char response[256] = {'\0'};
    get_client_list(response);
    send_msg(g_client_queue_ids[client_id], IPC_NOWAIT, client_id, 0, response);
}

void
get_client_list(char * output) {
    int i;
    for (i = 1; i <= MAX_CLIENTS; i++) {
        if (g_client_queue_ids[i] != 0) {
            char number[4], * num = number;
            sprintf(num, "%d ", i);
            strcat(output, number);
        }
    }
}

void
dispatch_friends(msgbuf * msg) {
    int client_id = (msg -> mcontent).uid;
    printf(">>> FRIENDS from ID: %d\n", client_id);

    if (is_empty((msg -> mcontent).mtext)) {
        remove_all_friends(&g_client_friends, client_id);
    } else {
        int * ids = NULL;
        int friend_count = read_numbers_list((msg->mcontent).mtext, &ids);
        if (friend_count > 0) {
            remove_all_friends(&g_client_friends, client_id);

            int i;
            for (i = 0; i < friend_count; i++) {
                int id = ids[i];
                if (g_client_queue_ids[id] != 0) {
                    add_friend(&g_client_friends, client_id, id);
                }
            }

            free(ids);
        }
    }
}

void
dispatch_add(msgbuf * msg) {
    int client_id = (msg -> mcontent).uid;
    printf(">>> ADD from ID: %d\n", client_id);

    int * ids = NULL;
    int friend_count = read_numbers_list((msg->mcontent).mtext, &ids);
    if (friend_count > 0) {
        int i;
        for (i = 0; i < friend_count; i++) {
            int id = ids[i];
            if (g_client_queue_ids[id] != 0) {
                add_friend(&g_client_friends, client_id, id);
            }
        }

        free(ids);
    }
}

void
dispatch_del(msgbuf * msg) {
    int client_id = (msg -> mcontent).uid;
    printf(">>> DEL from ID: %d\n", client_id);

    int * ids = NULL;
    int friend_count = read_numbers_list((msg->mcontent).mtext, &ids);
    if (friend_count > 0) {
        int i;
        for (i = 0; i < friend_count; i++) {
            int id = ids[i];
            remove_friend(&g_client_friends, client_id, id);
        }

        free(ids);
    }
}

void
dispatch_to_all(msgbuf * msg) {
    int client_id = (msg -> mcontent).uid;
    printf(">>> 2ALL from ID: %d\n", client_id);

    char * mid_response = NULL;
    if (prefix_id((msg -> mcontent).mtext, &mid_response, client_id) == -1) {
        return;
    }

    char * response = NULL;
    if (prefix_date(mid_response, &response) == -1) {
        free(mid_response);
        return;
    }

    int i;
    for (i = 1; i <= MAX_CLIENTS; i++) {
        if (client_id != i && g_client_queue_ids[i] != 0) {
            send_msg(g_client_queue_ids[i], IPC_NOWAIT, i, 0, response);
        }
    }

    free(mid_response);
    free(response);
}

void
dispatch_to_friends(msgbuf * msg) {
    int client_id = (msg -> mcontent).uid;
    printf(">>> 2FRIENDS from ID: %d\n", client_id);

    char * mid_response = NULL;
    if (prefix_id((msg -> mcontent).mtext, &mid_response, client_id) == -1) {
        return;
    }

    char * response = NULL;
    if (prefix_date(mid_response, &response) == -1) {
        free(mid_response);
        return;
    }

    int friend_id = get_friend(&g_client_friends, client_id);
    while (friend_id != -1) {
        if (g_client_queue_ids[friend_id] != 0) {
            send_msg(g_client_queue_ids[friend_id], IPC_NOWAIT, friend_id, 0, response);
        }

        friend_id = get_friend(&g_client_friends, client_id);
    }

    free(mid_response);
    free(response);
}

void
dispatch_to_one(msgbuf * msg) {
    int client_id = (msg -> mcontent).uid;
    printf(">>> 2ONE from ID: %d\n", client_id);

    char * mtext = (msg -> mcontent).mtext;
    int id = strip_id(&mtext);

    if (id == -1) {
        return;
    }

    if (mtext == NULL) {
        mtext = "";
    }

    char * mid_response = NULL;
    if (prefix_id(mtext, &mid_response, client_id) == -1) {
        return;
    }

    char * response = NULL;
    if (prefix_date(mid_response, &response) == -1) {
        free(mid_response);
        return;
    }

    if (g_client_queue_ids[id] != 0) {
        send_msg(g_client_queue_ids[id], IPC_NOWAIT, id, 0, response);
    }

    free(mid_response);
    free(response);
}

void
dispatch_stop(msgbuf * msg) {
	int client_id = (msg -> mcontent).uid;
	printf(">>> STOP from ID: %d\n", client_id);

	if (!(client_id >= 0 && client_id < MAX_CLIENTS)) {
		fprintf(stderr, ">>> ERR: client ID incorrect: %d\n", client_id);
	} else if (g_client_queue_ids[client_id] != 0) {
		g_client_queue_ids[client_id] = 0;
	} else {
		fprintf(stderr, ">>> ERR: no such client registered: %d\n", client_id);
	}
}

int 
main(void) {
	// Set signal handling and server message queue.
	setup();

	// Start listening on the queue.
	listen();
	
	return 0;
}
