#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "commons.h"
#include <errno.h>
#include <unistd.h>

int g_server_queue_id;
int g_client_queue_id;
int g_client_id;

msgbuf * g_msg;
size_t g_msgsz = sizeof(msgbuf) - sizeof(long) + MAX_MSG_LEN;

/***/

// Defined below.
int
set_signal_handling(void);

void 
remove_client_queue(void);

int
get_server_queue(void);

int 
get_client_queue(void);

void
setup(void) {
	// Set signal mask and handler.
	if (set_signal_handling() == -1) {
		exit(EXIT_FAILURE);
	}

	// Set queue removal at exit.
	if (atexit(remove_client_queue) != 0) {
		fprintf(stderr, "Failed to set atexit function.\n");
		exit(EXIT_FAILURE);
	}

	// Get server queue.
	g_server_queue_id = get_server_queue();
	g_client_queue_id = get_client_queue();
	if (g_server_queue_id == -1 || g_client_queue_id == -1) {
		exit(EXIT_FAILURE);
	} else {
		printf("Server queue ID: %d\nClient queue ID: %d\n", g_server_queue_id, g_client_queue_id);
	}

	// Get incoming message memory.
	g_msg = malloc(sizeof(msgbuf) + MAX_MSG_LEN);
	if (g_msg == NULL) {
		perror("Allocate incoming message memory: ");
		exit(EXIT_FAILURE);
	}
}

// Defined below.
void 
sigint_handler(int);

int 
set_signal_handling() {
	sigset_t mask_set;
	if (sigfillset(&mask_set) == -1) {
		perror("Fill mask set: ");
		return -1;
	}

	if (sigdelset(&mask_set, SIGINT) == -1) {
		perror("Unmask SIGINT: ");
		return -1;
	}
	if (sigdelset(&mask_set, SIGALRM) == -1) {
		perror("Unmask SIGALRM: ");
		return -1;
	}

	if (sigprocmask(SIG_SETMASK, &mask_set, NULL) == -1) {
		perror("Set process signal mask: ");
		return -1;
	}

	signal(SIGALRM, SIG_IGN);	

	struct sigaction act;
	act.sa_handler = sigint_handler;
	act.sa_flags = 0;

	if (sigemptyset(&(act.sa_mask)) == -1) {
		perror("Clean signal mask: ");
		return -1;
	}

	if (sigaddset(&(act.sa_mask), SIGINT) == -1) {
		perror("Add SIGINT to mask: ");
		return -1;
	}

	if (sigaction(SIGINT, &act, NULL) == -1) {
		perror("Set SIGINT handler: ");
		return -1;
	}	

	return 0;
}

// Defined below.
void
stop(void);

void 
sigint_handler(int sig) {
	printf("Received SIGINT.\n");
	
	stop();
	free(g_msg);
	exit(EXIT_SUCCESS);
}

void
remove_client_queue(void) {
	if (msgctl(g_client_queue_id, IPC_RMID, NULL) == -1) {
		if (errno == EINVAL) {
			printf("No client queue to be removed.\n");
		} else {
			perror("Remove client queue: ");
		}
	} else {
		printf("Succesfully removed client queue.\n");
	}
}

int
get_server_queue() {
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

	int server_queue_id = msgget(key, 0);
	if (server_queue_id == -1) {
		perror("Get  server queue: ");
		return -1;
	}

	return server_queue_id;
}

int
get_client_queue() {
	int client_queue_id = (int) msgget(IPC_PRIVATE, S_IRUSR | S_IWUSR);
	if (client_queue_id == -1) {
		perror("Create client queue: ");
		return -1;
	}

	return client_queue_id;
}

/***/

int
send_msg(long mtype, char * content) {
	size_t contentsz = strlen(content) + 1; 
	size_t msgsz = contentsz + sizeof(int);
	
	msgbuf * msg = malloc(sizeof(msgbuf) + contentsz);
	if (msg == NULL) {
		int err = errno;
		perror("Allocate outgoing message memory: ");
		return err;
	}

	msg -> mtype = mtype;
	msg -> uid = g_client_id;
	memcpy(msg -> mtext, content, contentsz);
	
	if (msgsnd(g_server_queue_id, msg, msgsz, IPC_NOWAIT) == -1) {	
		int err = errno;
		perror("Send message: ");
		return err;
	}

	free(msg);
	return 0;
}

int 
recv_msg(void) {
	// TODO: message order!
	if (msgrcv(g_client_queue_id, g_msg, g_msgsz, 0, MSG_NOERROR) == -1) {
		int err = errno;
		perror("Receive message: ");
		return err;
	}

	return 0;
}

/***/

void
init(void) {
	char client_queue_id[10];
	sprintf(client_queue_id, "%d", g_client_queue_id);
	
	int res;
	if ((res = send_msg(INIT, client_queue_id)) != 0) {
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
	if ((res = recv_msg()) != 0) {
		if (res == EIDRM) {
			fprintf(stderr, "Initialize server connection: connection timeout.\n");
		} else {
			fprintf(stderr, "Initialize server connection: %s\n", strerror(res));
		}

		exit(EXIT_FAILURE);
	}

	errno = 0;
	g_client_id = (int) strtol(g_msg -> mtext, NULL, 10);
	if (errno != 0) {
		perror("Convert server response to client ID: ");
		free(g_msg);
		exit(EXIT_FAILURE);
	}
}

void
stop(void) {
	send_msg(STOP, "");
}

/***/

void
dispatch_msg(msgbuf *);

int g_running = 1;

void
run(void) {
	while (g_running) {
		int res;
		if ((res = recv_msg()) != 0) {
			free(g_msg);
			exit(EXIT_FAILURE);
		}

		dispatch_msg(g_msg);
	}
}

/*** ***/

void
dispatch_msg(msgbuf * msg) {
	if (msg -> mtype == SERVER_STOP) {
		printf(">>> SERVER STOPPED.\n");
		stop();
		g_running = 0;
	} else if (msg -> mtype == g_client_id) {
		printf(">>> MSG: %s\n", msg -> mtext); // TODO: handle ID, date and so on...
	} else {
		fprintf(stderr, "Unknown message type.\n");
	}
}

int main(void) {
	// Setup signal handling and queues.
	setup();
	
	// Register with server.
	init();
		
	// Run communication loop.
	run();

	free(g_msg);
	
	return 0;
}

