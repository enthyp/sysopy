#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "protocol.h"
#include "util.h"
#include "queue.h"
#include <errno.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <signal.h>

// globals

int g_client_id;
int g_client_queue_id = -1;
char * g_input = NULL;
msgbuf * g_msg = NULL;
struct msqid_ds * g_msqid_ds = NULL;
size_t g_msgsz = sizeof(msgcontent);
int g_running = 1;
int g_server_queue_id;
int g_pid = 0;

// prototypes defined in this file

void dispatch_incoming_msg(msgbuf *);
void dispatch_outgoing_msg(char *);
void e_handler(void);
int set_sigusr1_handling(void);
void sigint_handler(int);
void sigusr1_handler(int);
void stop(void);

// definitions

void
setup(void) {
	if (base_setup(e_handler, sigint_handler) == -1) {
	    exit(EXIT_FAILURE);
	}

    if (set_sigusr1_handling() == -1) {
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
		perror(">>> ERR: allocate incoming message memory: ");
		exit(EXIT_FAILURE);
	}

	// Get message queue stat memory.
    g_msqid_ds = malloc(sizeof(struct msqid_ds));
    if (g_msqid_ds == NULL) {
        perror(">>> ERR: allocate message queue stat memory: ");
        exit(EXIT_FAILURE);
    }
}

void
e_handler(void) {
    if (g_pid != 0) {
        remove_queue(g_client_queue_id);
    }
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
    if (g_pid != 0) {
        printf("Received SIGINT.\n");
        stop();
        kill(g_pid, SIGUSR1);
    }
	exit(EXIT_SUCCESS);
}

int
set_sigusr1_handling(void) {
    sigset_t mask_set;
    if (sigemptyset(&mask_set) == -1) {
        perror("Empty mask set: ");
        return -1;
    }

    if (sigaddset(&mask_set, SIGUSR1) == -1) {
        perror("Add SIGUSR1 to mask: ");
        return -1;
    }

    if (sigprocmask(SIG_UNBLOCK, &mask_set, NULL) == -1) {
        perror("Modify process signal mask: ");
        return -1;
    }

    struct sigaction act;
    act.sa_handler = sigusr1_handler;
    act.sa_flags = 0;

    if (sigemptyset(&(act.sa_mask)) == -1) {
        perror("Clean signal mask: ");
        return -1;
    }

    if (sigaction(SIGUSR1, &act, NULL) == -1) {
        perror("Set SIGUSR1 handler: ");
        return -1;
    }

    return 0;
}

void
sigusr1_handler(int sig) {
    if (g_pid != 0) {
        stop();
    }
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

	int res;
	if ((res = send_msg(g_server_queue_id, IPC_NOWAIT, INIT, g_client_queue_id, client_queue_id_str)) != 0) {
		switch (res) {
			case EAGAIN: { 
				fprintf(stderr, ">>> ERR: failed to register with server: too many connections.\n");
				break;
			}	
			case EIDRM: {
				fprintf(stderr, ">>> ERR: failed to register with server: server queue is down.\n");
				break;
			}
			default: {
				perror(">>> ERR: initialize server connection: ");
				break; 
			}
		}

		exit(EXIT_FAILURE);
	}

	if ((res = recv_msg(g_client_queue_id, g_msg, g_msgsz, 0, 0)) != 0) {
		fprintf(stderr, ">>> ERR: initialize server connection: %s\n", strerror(res));
		exit(EXIT_FAILURE);
	}

	errno = 0;
	g_client_id = (int) strtol((g_msg -> mcontent).mtext, NULL, 10);
	if (errno != 0) {
		perror(">>> ERR: convert server response to client ID: ");
		exit(EXIT_FAILURE);
	}
}

void
run(void) {
    if ((g_pid = fork()) == -1) {
        fprintf(stderr, "ERR: failed to fork sending process.\n");
        return;
    } else if (g_pid == 0) {
        // Process all incoming messages.
        while (g_running) {
            // Receive commands from the server.
            if (recv_msg(g_client_queue_id, g_msg, g_msgsz, 0, 0) != 0) {
                exit(EXIT_FAILURE);
            }

            dispatch_incoming_msg(g_msg);
        }
    } else {
        g_input = NULL;
        size_t size = 0;

        // Process all outgoing messages.
        while (g_running) {
            // Send command to server.
            if (getline(&g_input, &size, stdin) == -1) {
                perror(">>> ERR: get input line: ");
            } else {
                dispatch_outgoing_msg(g_input);
            }
        }
    }
}

void
dispatch_outgoing_msg(char * command_text) {
    msg cmd = process_cmd(command_text);

    // Handle incorrect message type.
    if (cmd.mtype == -1) {
        return;
    }

    // Send message.
    if (send_cmd(g_server_queue_id, IPC_NOWAIT, g_client_id, &cmd) == -1) {
        fprintf(stderr, ">>> ERR: failed to send message.\n");
        return;
    }

    // Stop if requested.
    if (cmd.mtype == STOP) {
        if (g_pid != 0) {
            kill(g_pid, SIGUSR1);
        }
        exit(EXIT_SUCCESS);
    }
}

void
dispatch_incoming_msg(msgbuf * msg) {
	if (msg -> mtype == SERVER_STOP) {
		printf(">>> SERVER STOPPED.\n");
        kill(getppid(), SIGUSR1);
        g_running = 0;
	} else if (msg -> mtype == g_client_id) {
		printf(">>> MSG: %s\n", (msg -> mcontent).mtext);
	} else {
		fprintf(stderr, ">>> ERR: unknown message type.\n");
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
