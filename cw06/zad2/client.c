#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <mqueue.h>
#include "protocol.h"
#include "util.h"
#include "queue.h"
#include <errno.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <signal.h>

// globals

int g_client_id;
mqd_t g_client_queue_des = (mqd_t) -1;
char * g_client_queue_name = NULL;
char * g_file_input = NULL;
FILE * g_fp = NULL;
char * g_input = NULL;
char * g_msg = NULL;
char * g_msg_content = NULL;
size_t g_msgsz = 0;
int g_pid = 0;
int g_running = 1;
mqd_t g_server_queue_des = (mqd_t) -1;

// prototypes defined in this file

void dispatch_incoming_msg(char *, int, int);
void dispatch_outgoing_msg(char *);
void e_handler(void);
void from_file(char * file_name);
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
    g_server_queue_des = get_named_queue(SERVER_QUEUE_NAME, O_NONBLOCK | O_WRONLY);
    if (g_server_queue_des == -1) {
        exit(EXIT_FAILURE);
    } else {
        printf("Server queue name: %s\n Server queue descriptor: %d\n", SERVER_QUEUE_NAME, g_server_queue_des);
    }

    g_client_queue_name = rand_name();
	g_client_queue_des = get_named_queue(g_client_queue_name, O_CREAT | O_EXCL | O_RDONLY);
	if (g_server_queue_des == (mqd_t) -1 || g_client_queue_des == (mqd_t) -1) {
		exit(EXIT_FAILURE);
	} else {
		printf("Client queue name: %s\nClient queue descriptor: %d\n", g_client_queue_name, g_client_queue_des);
	}

    // Get max message size for both queues.
    size_t server_msgsz = (size_t) get_max_msgsz(g_server_queue_des);
    if (server_msgsz == -1) {
        exit(EXIT_FAILURE);
    }

    size_t client_msgsz = (size_t) get_max_msgsz(g_client_queue_des);
    if (client_msgsz == -1) {
        exit(EXIT_FAILURE);
    }

    g_msgsz = (server_msgsz > client_msgsz) ? client_msgsz : server_msgsz;

	// Get incoming message memory.
	g_msg = (char *) malloc(g_msgsz);
	if (g_msg == NULL) {
		perror("Allocate incoming message memory");
		exit(EXIT_FAILURE);
	}

    g_msg_content = (char *) malloc(g_msgsz - 2);
    if (g_msg_content == NULL) {
        perror("Allocate message memory");
        exit(EXIT_FAILURE);
    }
}

void
e_handler(void) {
    if (g_pid != 0) {
        mq_close(g_server_queue_des);
        remove_queue(g_client_queue_name, g_client_queue_des);
        if (g_fp != NULL) {
            fclose(g_fp);
        }
        if (g_file_input != NULL) {
            free(g_file_input);
        }
    }
    if (g_client_queue_name != NULL) {
        free(g_client_queue_name);
    }
    if (g_msg != NULL) {
        free(g_msg);
    }
    if (g_msg_content != NULL) {
        free(g_msg_content);
    }
    if (g_input != NULL) {
        free(g_input);
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
    if (g_pid != 0) {
        stop();
    }
    exit(EXIT_SUCCESS);
}

void
stop(void) {
    send_msg(g_server_queue_des, g_msg, "", STOP, g_client_id, g_msgsz);
}

void
init(void) {
	int res;
	if ((res = send_msg(g_server_queue_des, g_msg, g_client_queue_name, INIT, INIT, g_msgsz)) != 0) {
		switch (res) {
			case EAGAIN: { 
				fprintf(stderr, ">>> ERR: failed to register with server: too many connections.\n");
				break;
			}	
			case EBADF: {
				fprintf(stderr, ">>> ERR: failed to register with server: server queue is down.\n");
				break;
			}
			default: {
				perror("Initialize server connection");
				break; 
			}
		}

		exit(EXIT_FAILURE);
	}

	int mtype, uid;
	if ((res = recv_msg(g_client_queue_des, g_msg, g_msg_content, &mtype, &uid, g_msgsz)) != 0) {
		fprintf(stderr, ">>> ERR: initialize server connection: %s\n", strerror(res));
		exit(EXIT_FAILURE);
	}

	g_client_id = uid;
}

void
run(void) {
    if ((g_pid = fork()) == -1) {
        fprintf(stderr, "Failed to fork sending process.\n");
        return;
    } else if (g_pid == 0) {
        // Process all incoming messages.
        while (g_running) {
            // Receive commands from the server.
            int mtype, uid;
            if (recv_msg(g_client_queue_des, g_msg, g_msg_content, &mtype, &uid, g_msgsz) != 0) {
                exit(EXIT_FAILURE);
            }

            dispatch_incoming_msg(g_msg_content, mtype, uid);
        }
    } else {
        g_input = NULL;
        size_t size = 0;

        // Process all outgoing messages.
        while (g_running) {
            // Send command to server.
            if (getline(&g_input, &size, stdin) == -1) {
                perror("Get input line");
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

    if (cmd.mtype != READ) {
        // Send message.
        if (send_cmd(g_server_queue_des, g_client_id, g_msg, &cmd, g_msgsz) == -1) {
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
    } else {
        from_file(cmd.mtext);
    }
}

void
from_file(char * file_name) {
    g_fp = fopen(file_name, "r");
    if (g_fp == NULL) {
        perror("Opening input file: ");
        return;
    }

    size_t size = 0;
    while (getline(&g_file_input, &size, g_fp) != -1) {
        dispatch_outgoing_msg(g_file_input);
    }

    fclose(g_fp);
    g_fp = NULL;

    free(g_file_input);
    g_file_input = NULL;
}

void
dispatch_incoming_msg(char * msg, int mtype, int uid) {
	if (mtype == SERVER_STOP) {
		printf(">>> SERVER STOPPED.\n");
        kill(getppid(), SIGUSR1);
        g_running = 0;
	} else if (mtype == uid) {
		printf(">>> MSG: %s\n", msg);
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
