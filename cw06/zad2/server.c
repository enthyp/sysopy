#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <signal.h>
#include "protocol.h"
#include "util.h"
#include "queue.h"
#include <unistd.h>
#include <errno.h>
#include "friends.h"

// globals

friends_collection g_client_friends;
mqd_t g_client_queue_des[MAX_CLIENTS + 1];
char g_client_queue_names[MAX_CLIENTS + 1][NAME_LEN + 1];
char * g_msg = NULL;
size_t g_msgsz = 0;
mqd_t g_server_queue_des = (mqd_t) -1;
char * g_server_queue_name = NULL;


// prototypes defined in this file

void dispatch_add(char *, int);
void dispatch_del(char *, int);
void dispatch_echo(char *, int);
void dispatch_friends(char *, int);
void dispatch_init(char *, int);
void dispatch_list(char *, int);
void dispatch_msg(char *, int, int);
void dispatch_stop(char *, int);
void dispatch_to_all(char *, int);
void dispatch_to_friends(char *, int);
void dispatch_to_one(char *, int);
void e_handler(void);
int find_slot(char *);
void get_client_list(char *);
int set_sigusr1_handling(void);
void sigint_handler(int);
void sigusr1_handler(int);

// definitions

void
setup(void) {
    if (base_setup(e_handler, sigint_handler) == -1) {
        exit(EXIT_FAILURE);
    }

    if (set_sigusr1_handling() == -1) {
        exit(EXIT_FAILURE);
    }

    // Clear global client registers.
    int i;
    for (i = 0; i <= MAX_CLIENTS; i++) {
        g_client_queue_des[i] = (mqd_t) -1;
        g_client_queue_names[i][0] = '\0';
    }

	// Get server queue.
	g_server_queue_name = get_server_queue_name();
    if (g_server_queue_name == NULL) {
        exit(EXIT_FAILURE);
    }

    // BTW: server queue could easily be opened in blocking mode, no need for notifications then.
	g_server_queue_des = get_named_queue(g_server_queue_name, O_CREAT | O_EXCL | O_NONBLOCK | O_RDONLY);
	if (g_server_queue_des == -1) {
		exit(EXIT_FAILURE);
	} else {
		printf("Server queue name: %s\n Server queue descriptor: %d\n", g_server_queue_name, g_server_queue_des);
	}

	// Get max message size of server queue.
    g_msgsz = (size_t) get_max_msgsz(g_server_queue_des);
	if (g_msgsz == -1) {
        exit(EXIT_FAILURE);
	}

	// Get incoming message memory.
	g_msg = (char *) malloc(g_msgsz);
	if (g_msg == NULL) {
		perror("Allocate message memory");
		exit(EXIT_FAILURE);
	}

	// Allocate friends memory.
	if (setup_friends(&g_client_friends, MAX_CLIENTS + 1) == -1) {
	    fprintf(stderr, "Failed to setup friends collection.\n");
	    exit(EXIT_FAILURE);
	}

	// Set notifications on incoming messages.
    if (set_notification(g_server_queue_des, SIGUSR1) != 0) {
        exit(EXIT_FAILURE);
    }
}

void
e_handler(void) {
    remove_queue(g_server_queue_name, g_server_queue_des);
    teardown_friends(&g_client_friends);
    if (g_msg != NULL) {
        free(g_msg);
    }
    if (g_server_queue_name != NULL) {
        free(g_server_queue_name);
    }
}

void 
sigint_handler(int sig) {
	printf("Received SIGINT.\n");

	int i, count = 0;
	for (i = 1; i <= MAX_CLIENTS; i++) {
		if (g_client_queue_des[i] != (mqd_t) -1 &&
		    send_msg(g_client_queue_des[i], "", SERVER_STOP, i, g_msgsz) == 0) {
			count++;
		}
	}

    while (count > 0) {
	    int res;
	    int mtype, uid;
		if ((res = recv_msg(g_server_queue_des, &g_msg, &mtype, &uid, g_msgsz)) == 0 &&
                (mtype == STOP)) {
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


int
set_sigusr1_handling(void) {
    sigset_t mask_set;
    if (sigemptyset(&mask_set) == -1) {
        perror("Empty mask set");
        return -1;
    }

    if (sigaddset(&mask_set, SIGUSR1) == -1) {
        perror("Add SIGUSR1 to mask");
        return -1;
    }

    if (sigprocmask(SIG_UNBLOCK, &mask_set, NULL) == -1) {
        perror("Modify process signal mask");
        return -1;
    }

    struct sigaction act;
    act.sa_handler = sigusr1_handler;
    act.sa_flags = 0;

    if (sigemptyset(&(act.sa_mask)) == -1) {
        perror("Clean signal mask");
        return -1;
    }

    if (sigaction(SIGUSR1, &act, NULL) == -1) {
        perror("Set SIGUSR1 handler");
        return -1;
    }

    return 0;
}

void
sigusr1_handler(int sig) {
    // Set notification again.
    if (set_notification(g_server_queue_des, SIGUSR1) != 0) {
        exit(EXIT_FAILURE);
    }

    int mtype = 0;
    int uid = 0;
    int res;
    while ((res = recv_msg(g_server_queue_des, &g_msg, &mtype, &uid, g_msgsz)) == 0) {
        dispatch_msg(g_msg, mtype, uid);
    }
    if (res != EAGAIN) {
        exit(EXIT_FAILURE);
    }
}

void
listen(void) {
    while (1) {
        // Assumption: no client sends message to queue before notification is set for the first time.
        pause();
    }
}

void 
dispatch_msg(char * msg, int mtype, int uid) {
	switch (mtype) {
		case INIT: dispatch_init(msg, uid); break;
		case ECHO: dispatch_echo(msg, uid); break;
		case LIST: dispatch_list(msg, uid); break;
		case FRIENDS: dispatch_friends(msg, uid); break;
		case ADD: dispatch_add(msg, uid); break;
		case DEL: dispatch_del(msg, uid); break;
		case TO_ALL: dispatch_to_all(msg, uid); break;
		case TO_FRIENDS: dispatch_to_friends(msg, uid); break;
		case TO_ONE: dispatch_to_one(msg, uid); break;
		case STOP: dispatch_stop(msg, uid); break;
		default: fprintf(stderr, ">>> ERR: unknown message type: %d\n", mtype);
	}
}

void
dispatch_init(char * msg, int uid) {
	printf(">>> INIT from queue name: %s\n", msg);

	int client_id = find_slot(msg);
	if (client_id == -1) {
		return;
	}

	mqd_t client_queue_des;
	if ((client_queue_des = get_named_queue(msg, O_NONBLOCK | O_WRONLY)) == (mqd_t) -1) {
	    fprintf(stderr, ">>> ERR: failed to open client queue under name: %s\n", msg);
	    return;
	}
	g_client_queue_des[client_id] = client_queue_des;
    strcpy(g_client_queue_names[client_id], msg);

	if (send_msg(client_queue_des, "", 0, client_id, g_msgsz) != 0) {
		g_client_queue_des[client_id] = (mqd_t) -1;
        g_client_queue_names[client_id][0] = '\0';
		return;
	}
}

int
find_slot(char * name) {
	int i, slot = -1;
	int exists = 0;
	for (i = 1; i <= MAX_CLIENTS; i++) {
		if (slot == -1 && g_client_queue_des[i] == (mqd_t) -1) {
			slot = i;
		}
		if (strcmp(g_client_queue_names[i], name) == 0) {
			exists = 1;
		}
	}
	
	if (exists) {
		fprintf(stderr, ">>> ERR: repeated INIT for client queue %s.\n", name);
		return -1;
	}

	if (slot == -1) {
		fprintf(stderr, ">>> ERR: limit of connections reached.\n");
		return -1;
	}

	return slot;
}

void
dispatch_echo(char * msg, int uid) {
    printf(">>> ECHO from ID: %d\n", uid);

    char * response = NULL;
    if (prefix_date(msg, &response) == -1) {
        return;
    }
    send_msg(g_client_queue_des[uid], response, 0, uid, g_msgsz);
    free(response);
}

void
dispatch_list(char * msg, int uid) {
    printf(">>> LIST from ID: %d\n", uid);

    char response[256] = {'\0'};
    get_client_list(response);
    send_msg(g_client_queue_des[uid], response, 0, 0, g_msgsz);
}

void
get_client_list(char * output) {
    int i;
    for (i = 1; i <= MAX_CLIENTS; i++) {
        if (g_client_queue_des[i] != (mqd_t) -1) {
            char number[4], * num = number;
            sprintf(num, "%d ", i);
            strcat(output, number);
        }
    }
}

void
dispatch_friends(char * msg, int uid) {
    printf(">>> FRIENDS from ID: %d\n", uid);

    if (is_empty(msg)) {
        remove_all_friends(&g_client_friends, uid);
    } else {
        int * ids = NULL;
        int friend_count = read_numbers_list(msg, &ids);
        if (friend_count > 0) {
            remove_all_friends(&g_client_friends, uid);

            int i;
            for (i = 0; i < friend_count; i++) {
                int id = ids[i];
                if (g_client_queue_des[id] != (mqd_t) -1) {
                    add_friend(&g_client_friends, uid, id);
                }
            }

            free(ids);
        }
    }
}

void
dispatch_add(char * msg, int uid) {
    printf(">>> ADD from ID: %d\n", uid);

    int * ids = NULL;
    int friend_count = read_numbers_list(msg, &ids);
    if (friend_count > 0) {
        int i;
        for (i = 0; i < friend_count; i++) {
            int id = ids[i];
            if (g_client_queue_des[id] != (mqd_t) -1) {
                add_friend(&g_client_friends, uid, id);
            }
        }

        free(ids);
    }
}

void
dispatch_del(char * msg, int uid) {
    printf(">>> DEL from ID: %d\n", uid);

    int * ids = NULL;
    int friend_count = read_numbers_list(msg, &ids);
    if (friend_count > 0) {
        int i;
        for (i = 0; i < friend_count; i++) {
            int id = ids[i];
            remove_friend(&g_client_friends, uid, id);
        }

        free(ids);
    }
}

void
dispatch_to_all(char * msg, int uid) {
    printf(">>> 2ALL from ID: %d\n", uid);

    char * mid_response = NULL;
    if (prefix_id(msg, &mid_response, uid) == -1) {
        return;
    }

    char * response = NULL;
    if (prefix_date(mid_response, &response) == -1) {
        free(mid_response);
        return;
    }

    int i;
    for (i = 1; i <= MAX_CLIENTS; i++) {
        if (uid != i && g_client_queue_des[i] != (mqd_t) -1) {
            send_msg(g_client_queue_des[i], response, 0, i, g_msgsz);
        }
    }

    free(mid_response);
    free(response);
}

void
dispatch_to_friends(char * msg, int uid) {
    printf(">>> 2FRIENDS from ID: %d\n", uid);

    char * mid_response = NULL;
    if (prefix_id(msg, &mid_response, uid) == -1) {
        return;
    }

    char * response = NULL;
    if (prefix_date(mid_response, &response) == -1) {
        free(mid_response);
        return;
    }

    int friend_id = get_friend(&g_client_friends, uid);
    while (friend_id != -1) {
        if (g_client_queue_des[friend_id] != (mqd_t) -1) {
            send_msg(g_client_queue_des[friend_id], response, 0, friend_id, g_msgsz);
        }

        friend_id = get_friend(&g_client_friends, uid);
    }

    free(mid_response);
    free(response);
}

void
dispatch_to_one(char * msg, int uid) {
    printf(">>> 2ONE from ID: %d\n", uid);

    int id = strip_id(&msg);

    if (id == -1) {
        return;
    }

    if (msg == NULL) {
        msg = "";
    }

    char * mid_response = NULL;
    if (prefix_id(msg, &mid_response, uid) == -1) {
        return;
    }

    char * response = NULL;
    if (prefix_date(mid_response, &response) == -1) {
        free(mid_response);
        return;
    }

    if (g_client_queue_des[id] != (mqd_t) -1) {
        send_msg(g_client_queue_des[id], response, 0, id, g_msgsz);
    }

    free(mid_response);
    free(response);
}

void
dispatch_stop(char * msg, int uid) {
	printf(">>> STOP from ID: %d\n", uid);

	if (!(uid >= 0 && uid < MAX_CLIENTS)) {
		fprintf(stderr, ">>> ERR: client ID incorrect: %d\n", uid);
	} else if (g_client_queue_des[uid] != (mqd_t) -1) {
	    mq_close(g_client_queue_des[uid]);
		g_client_queue_des[uid] = (mqd_t) -1;
		g_client_queue_names[uid][0] = '\0';
	} else {
		fprintf(stderr, ">>> ERR: no such client registered: %d\n", uid);
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
