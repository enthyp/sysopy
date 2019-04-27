#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <signal.h>
#include "protocol.h"
#include "commons.h"
#include "queue.h"
#include <unistd.h> // sleep
#include <errno.h>

// globals

int g_client_queue_ids[MAX_CLIENTS + 1];
int g_server_queue_id = -1;
msgbuf * g_msg; 
size_t g_msgsz = sizeof(msgbuf) - sizeof(long) + MAX_MSG_LEN;

// prototypes defined in this file

void dispatch_init(msgbuf * msg);
void dispatch_msg(msgbuf *);
void dispatch_stop(msgbuf * msg);
void e_handler(void);
int find_slot(int);
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
		perror("Allocate message memory: ");
		exit(EXIT_FAILURE);
	}
}

void
e_handler(void) {
    remove_queue(g_server_queue_id);
    free(g_msg);
}

void 
sigint_handler(int sig) {
	printf("Received SIGINT.\n");

	int i, count = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (g_client_queue_ids[i] != 0 && send_msg(g_client_queue_ids[i], IPC_NOWAIT, SERVER_STOP, 0, "") == 0) {
			count++;
		}	
	}
	
	// TODO: timeout!
	while (count > 0) {
		if (recv_msg(g_server_queue_id, g_msg, g_msgsz, STOP, IPC_NOWAIT | MSG_NOERROR) == 0) {
			count--;
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
		if (recv_msg(g_server_queue_id, g_msg, g_msgsz, 0, MSG_NOERROR) == -1) {
			exit(EXIT_FAILURE);
		}
		
		dispatch_msg(g_msg);	
	}
}

void 
dispatch_msg(msgbuf * msg) {
	switch (msg -> mtype) {
		case INIT: dispatch_init(msg); break;
		case STOP: dispatch_stop(msg); break;
		default: fprintf(stderr, "Unknown message type: %ld\n", msg -> mtype);
	}
}

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
	if ((res = send_msg(client_queue_id, IPC_NOWAIT, client_id, 0, msg_content)) != 0) {
		// TODO: timeout handling? Some resend queue??
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
		fprintf(stderr, "Repeated INIT for client queue %d.\n", client_queue_id);
		return -1;
	}

	if (slot == -1) {
		fprintf(stderr, "Limit of connections reached.\n");
		return -1;
	}

	return slot;
}


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

int 
main(void) {
	// Set signal handling and server message queue.
	setup();

	// Start listening on the queue.
	listen();
	
	return 0;
}

