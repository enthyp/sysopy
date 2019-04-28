#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "protocol.h"
#include "commons.h"
#include "queue.h"
#include <errno.h>
#include <unistd.h>

// globals

int g_client_id;
int g_client_queue_id = -1;
char * g_input = NULL;
msgbuf * g_msg = NULL;
struct msqid_ds * g_msqid_ds = NULL;
size_t g_msgsz = sizeof(msgcontent);
int g_running = 1;
int g_server_queue_id;

// prototypes defined in this file

void dispatch_incoming_msg(msgbuf *);
void dispatch_outgoing_msg(char *);
void e_handler(void);
void sigint_handler(int);
void stop(void);

// definitions

void
setup(void) {
	if (base_setup(e_handler, sigint_handler) == -1) {
	    exit(EXIT_FAILURE);
	}

	// Get queues.
	g_server_queue_id = get_queue(0);
	g_client_queue_id = get_private_queue();
	if (g_server_queue_id == -1 || g_client_queue_id == -1) {
		exit(EXIT_FAILURE);
	} else {
		printf("Server queue ID: %d\nClient queue ID: %d\n", g_server_queue_id, g_client_queue_id);
	}

	// Get incoming message memory.
	g_msg = malloc(sizeof(msgbuf));
	if (g_msg == NULL) {
		perror("Allocate incoming message memory: ");
		exit(EXIT_FAILURE);
	}

	// Get message queue stat memory.
    g_msqid_ds = malloc(sizeof(struct msqid_ds));
    if (g_msqid_ds == NULL) {
        perror("Allocate message queue stat memory: ");
        exit(EXIT_FAILURE);
    }
}

void
e_handler(void) {
    remove_queue(g_client_queue_id);
    if (g_msg != NULL) {
        free(g_msg);
    }
    if (g_input != NULL) {
        free(g_input);
    }
    if (g_msqid_ds != NULL) {
        free(g_msqid_ds);
    }
}

void 
sigint_handler(int sig) {
	printf("Received SIGINT.\n");
	
	stop();
	exit(EXIT_SUCCESS);
}

void
stop(void) {
    send_msg(g_server_queue_id, IPC_NOWAIT, STOP, g_client_id, "");
}

void
init(void) {
	char client_queue_id_str[10];
	sprintf(client_queue_id_str, "%d", g_client_queue_id);

	// TODO: add timeout?
	int res;
	if ((res = send_msg(g_server_queue_id, IPC_NOWAIT, INIT, g_client_queue_id, client_queue_id_str)) != 0) {
		switch (res) {
			case EAGAIN: { 
				fprintf(stderr, "Failed to register with server: too many connections.\n"); 
				break;
			}	
			case EIDRM: {
				fprintf(stderr, "Failed to register with server: server queue is down.\n"); 
				break;
			}
			default: {
				perror("Initialize server connection: "); 
				break; 
			}
		}

		exit(EXIT_FAILURE);
	}

	// Set timeout.
	alarm(2);
	if ((res = recv_msg(g_client_queue_id, g_msg, g_msgsz, 0, 0)) != 0) {
		if (res == EINTR) {
			fprintf(stderr, "Initialize server connection: connection timeout.\n");
		} else {
			fprintf(stderr, "Initialize server connection: %s\n", strerror(res));
		}

		exit(EXIT_FAILURE);
	}
	alarm(0);

	errno = 0;
	g_client_id = (int) strtol((g_msg -> mcontent).mtext, NULL, 10);
	if (errno != 0) {
		perror("Convert server response to client ID: ");
		exit(EXIT_FAILURE);
	}
}

void
run(void) {
	while (g_running) {
	    // Send command to server.
        size_t size = 0;
	    if (getline(&g_input, &size, stdin) == -1) {
	        perror("Get input line: ");
	    } else {
	        dispatch_outgoing_msg(g_input);
	    }

	    // Process all messages currently in the queue.
	    int msg_cnt;
	    if ((msg_cnt = get_msg_cnt(g_client_queue_id, g_msqid_ds)) > 0) {
	        while (msg_cnt > 0 && g_running) {
                // TODO: message order!
                if (recv_msg(g_client_queue_id, g_msg, g_msgsz, 0, 0) != 0) {
                    exit(EXIT_FAILURE);
                }

                dispatch_incoming_msg(g_msg);
	            msg_cnt--;
	        }
	    }
	}
}

void
dispatch_outgoing_msg(char * command) {
    send_cmd(g_server_queue_id, IPC_NOWAIT, g_client_id, command);
}

void
dispatch_incoming_msg(msgbuf * msg) {
	if (msg -> mtype == SERVER_STOP) {
		printf(">>> SERVER STOPPED.\n");
		stop();
		g_running = 0;
	} else if (msg -> mtype == g_client_id) {
		printf(">>> MSG: %s\n", (msg -> mcontent).mtext);
	} else {
		fprintf(stderr, ">>> ERR: Unknown message type.\n");
	}
}

int main(void) {
	// Setup signal handling and queues.
	setup();
	
	// Register with server.
	init();
		
	// Run communication loop.
	run();
	
	return 0;
}

