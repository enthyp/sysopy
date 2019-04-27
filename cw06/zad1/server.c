#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <signal.h>
#include "commons.h"
#include <unistd.h> // sleep
#include <errno.h>

int g_server_queue_id;
int g_client_queue_ids[MAX_CLIENTS + 1];

msgbuf * g_msg; 
size_t g_msgsz = sizeof(msgbuf) - sizeof(long) + MAX_MSG_LEN;


/***/

// Defined below.
int
set_signal_handling(void);

void
remove_server_queue(void);

int
get_server_queue(void);

void
setup(void) {
	// Set signal mask and handler.
	if (set_signal_handling() == -1) {
		exit(EXIT_FAILURE);
	}

	// Set queue removal at exit.
	if (atexit(remove_server_queue) != 0) {
		fprintf(stderr, "Failed to set atexit function.\n");
		exit(EXIT_FAILURE);
	}

	// Get server queue.
	g_server_queue_id = get_server_queue();
	if (g_server_queue_id == -1) {
		exit(EXIT_FAILURE);
	} else {
		printf("Server queue ID: %d\n", g_server_queue_id);
	}

	// Get incoming message memory.
	g_msg = malloc(sizeof(msgbuf) + MAX_MSG_LEN);
	if (g_msg == NULL) {
		perror("Allocate message memory: ");
		exit(EXIT_FAILURE);
	}
}

/*** ***/

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
	
	if (sigprocmask(SIG_SETMASK, &mask_set, NULL) == -1) {
		perror("Set process signal mask: ");
		return -1;
	}

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

/*** *** ***/

int
send_msg(int, long, char *);

int
recv_msg(int);

void 
sigint_handler(int sig) {
	printf("Received SIGINT.\n");

	int i, count = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (g_client_queue_ids[i] != 0 && send_msg(i, SERVER_STOP, "") == 0) {
			count++;
		}	
	}
	
	// TODO: timeout!
	while (count > 0) {
		if (recv_msg(STOP) == 0) {
			count--;
		}
	}

	free(g_msg);

	if (count == 0)
		exit(EXIT_SUCCESS);
	else 
		exit(EXIT_FAILURE);
}

/*** *** *** ***/

int
send_msg(int client_id, long mtype, char * content) {
	size_t contentsz = strlen(content) + 1; 
	size_t msgsz = contentsz + sizeof(int);

	msgbuf * msg = malloc(sizeof(msgbuf) + contentsz);
	if (msg == NULL) {
		int err = errno;
		perror("Allocate outgoing message memory: ");
		return err;
	}

	msg -> mtype = mtype;
	msg -> uid = 0;
	memcpy(msg -> mtext, content, contentsz);
	
	// TODO: maybe should handle resends on timeout somehow...	
	if (msgsnd(g_client_queue_ids[client_id], msg, msgsz, 0) == -1) {
		int err = errno;
		perror("Send message: ");
		free(msg);
		return err;
	}

	free(msg);
	return 0;
}

int
recv_msg(int type) {
	// TODO: must be non-blocking!
	if (msgrcv(g_server_queue_id, g_msg, g_msgsz, type, MSG_NOERROR) == -1) {
		int err = errno;
		perror("Receive message: ");
		return err;
	}
	
	return 0;
}

/*** *** *** ***/
/*** *** ***/

void
remove_server_queue(void) {
	if (msgctl(g_server_queue_id, IPC_RMID, NULL) == -1) {
		if (errno == EINVAL) {
			printf("No server queue to be removed.\n");
		} else {
			perror("Remove server queue: ");
		}
	} else {
		printf("Succesfully removed server queue.\n");
	}
}

int
get_server_queue(void) {
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

	int server_queue_id = msgget(key, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	if (server_queue_id == -1) {
		perror("Create queue: ");
		return -1;
	}

	return server_queue_id;
}

/*** ***/

void
dispatch_msg(msgbuf *);

void
listen(void) {		
	while (1) {
		if (recv_msg(0) == -1) {
			free(g_msg);
			exit(EXIT_FAILURE);
		}
		
		dispatch_msg(g_msg);	
	}
}

/*** ***/

// Defined below.
void
dispatch_init(msgbuf * msg);

void
dispatch_stop(msgbuf * msg);

void 
dispatch_msg(msgbuf * msg) {
	switch (msg -> mtype) {
		case INIT: dispatch_init(msg); break;
		case STOP: dispatch_stop(msg); break;
		default: fprintf(stderr, "Unknown message type: %ld\n", msg -> mtype);
	}
}

/*** *** ***/

int 
find_slot(int);

void
dispatch_init(msgbuf * msg) {
	errno = 0;
	int client_queue_id = (int) strtol(msg -> mtext, NULL, 10);
	if (errno != 0) {
		fprintf(stderr, "Incorrect user queue ID: %s\n", msg -> mtext);
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
	int res;
	if ((res = send_msg(client_id, client_id, msg_content)) != 0) {
		// TODO: timeout handling? Some resend queue??
		g_client_queue_ids[client_id] = 0;
		return;
	}
}

/*** *** *** ***/

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
		fprintf(stderr, "Repeated INIT for client queue %d.\n", client_queue_id);
		return -1;
	}

	if (slot == -1) {
		fprintf(stderr, "Limit of connections reached.\n");
		return -1;
	}

	return slot;
}

/*** *** *** ***/

void
dispatch_stop(msgbuf * msg) {
	int client_id = msg -> uid;
	printf(">>> STOP from ID: %d\n", client_id);

	if (!(client_id >= 0 && client_id < MAX_CLIENTS)) {
		fprintf(stderr, "Client ID incorrect: %d\n", client_id);
	} else if (g_client_queue_ids[client_id] != 0) {
		g_client_queue_ids[client_id] = 0;
	} else {
		fprintf(stderr, "No such client registered: %d\n", client_id);
	}
}

/*** *** ***/
/*** ***/
/***/

int 
main(void) {
	// Set signal handling and server message queue.
	setup();

	// Start listening on the queue.
	listen();
	
	return 0;
}

