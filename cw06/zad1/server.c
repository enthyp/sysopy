#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <signal.h>
#include "protocol.h"
#include <unistd.h> // sleep

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

	int server_queue_id = msgget(key, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	if (server_queue_id == -1) {
		perror("Create queue: ");
		return -1;
	}

	return server_queue_id;
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

void 
sigint_handler(int sig) {
	printf("Received SIGINT.\n");
	// TODO: finish your job.
	exit(EXIT_SUCCESS);
}

int debug = 0;

int 
main(int argc, char * argv[]) {
	// Set debugging mode.
	if (argc == 2 && strcmp(argv[1], "debug") == 0) {
		debug = 1;
	}

	// Set signal mask and handler.
	if (set_signal_handling() == -1) {
		exit(EXIT_FAILURE);
	}

	// Get server queue.
	int server_queue_id = get_server_queue();
	if (server_queue_id == -1) {
		exit(EXIT_FAILURE);
	} else {
		printf("Server queue ID: %d\n", server_queue_id);
	}

	// Start listening on queue.
	sleep(3000);
	
	return 0;
}
