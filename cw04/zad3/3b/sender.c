#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

long 
read_natural(char * string) {
	char *endp;
	long outcome = strtol(string, &endp, 10);

	if (endp == string) {
		return -1L;
	}
	if ((outcome == LONG_MAX || outcome == LONG_MIN) && errno == ERANGE) {
		return -1L;
	}
	if (*endp != '\0') {
		return -1L;
	}

	return outcome;	
}

typedef enum {
	KILL,
	SIGQUEUE,
	SIGRT
} mode;

int glob_receiving = 1;
int glob_waiting = 0;
int glob_sending = 1;
int glob_count_back = 0;
int glob_count_caught = 0;
mode glob_mode;


void 
set_receive(int sig_out, int sig_fin);

void
send_signal(pid_t catcher_pid, int sig);

void
send(pid_t catcher_pid, int no_signals, mode mode) {
	// Choose signals depending on mode;
	int sig1, sig2;
	switch (mode) {
		case KILL: case SIGQUEUE: {
			sig1 = SIGUSR1; sig2 = SIGUSR2; break; 
		}
		case SIGRT: {
			sig1 = SIGRTMIN; sig2 = SIGRTMIN + 1; break;
		}
		default: {
			fprintf(stderr, "Incorrect mode.\n");
			return;
		}
	}

	glob_mode = mode;
	set_receive(sig1, sig2);
	
	// Send signals.	
	int i = 0;
	while (i < no_signals) {
		if (!glob_waiting) {
			glob_waiting = 1;
			send_signal(catcher_pid, sig1);
			i++;
		}
	}
	glob_sending = 0;
	send_signal(catcher_pid, sig2);

	// Receive.	
	while (glob_receiving) {
		sleep(1);
	}

	printf("Expected: %d, received: %d. ", no_signals, glob_count_back);
	if (glob_mode == SIGQUEUE) {
		printf("Caught by catcher: %d.", glob_count_caught);
	}
	printf("\n");

	return;
}

void
handle_SIG1_R(int signum) {
	glob_count_back++;
}

void
handle_SIG1_K(int signum) {
	if (glob_sending) {
		glob_waiting = 0;
	} else {
		glob_count_back++;
	}
}

void 
handle_SIG2_K(int signum) {
	glob_receiving = 0;
}

void
handle_SIG1_Q(int signum, siginfo_t * info, void * context) {
	if (glob_sending) {
		glob_waiting = 0;
	} else {
		glob_count_back++;
		int value = info -> si_value.sival_int;
		if (value > glob_count_caught) {
			glob_count_caught = value;
		}
	}
}

void
handle_SIG2_Q(int signum, siginfo_t * info, void * context) {
	int value = info -> si_value.sival_int;
	if (value > glob_count_caught) {
		glob_count_caught = value;
	}
	glob_receiving = 0;
}

void 
handle_SIGUSR1(int signum) {
	glob_waiting = 0;
}

void 
set_receive(int sig_out, int sig_fin) {
	sigset_t blocked_signals;
	sigfillset(&blocked_signals);	
	sigdelset(&blocked_signals, sig_out);
	sigdelset(&blocked_signals, sig_fin);
	sigdelset(&blocked_signals, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &blocked_signals, NULL) != 0) {
		fprintf(stderr, "Failed to set blocked signals.\n");
		exit(-1);
	}

	struct sigaction act;
	if (sigemptyset(&act.sa_mask) != 0) {
		fprintf(stderr, "Failed to set signal handler mask.\n");
	}		

	if (glob_mode == SIGQUEUE) {
		act.sa_flags = SA_SIGINFO;
		
		act.sa_sigaction = handle_SIG1_Q;
		sigaction(sig_out, &act, NULL);

		act.sa_sigaction = handle_SIG2_Q;
		sigaction(sig_fin, &act, NULL);
	} else if (glob_mode == KILL) {
		act.sa_flags = 0; 
		
		act.sa_handler = handle_SIG1_K;
		sigaction(sig_out, &act, NULL);

		act.sa_handler = handle_SIG2_K;
		sigaction(sig_fin, &act, NULL);
	} else {
		act.sa_flags = 0; 

		act.sa_handler = handle_SIGUSR1;
		sigaction(SIGUSR1, &act, NULL);			

		act.sa_handler = handle_SIG1_R;
		sigaction(sig_out, &act, NULL);

		act.sa_handler = handle_SIG2_K;
		sigaction(sig_fin, &act, NULL);
	}
}

void
send_signal(pid_t catcher_pid, int sig) {
	switch (glob_mode) {
		case KILL: case SIGRT: {
			if (kill(catcher_pid, sig) != 0) {
				fprintf(stderr, "Sending signal %d failed.\n", sig);
				return;
			}
			break;
		}
		case SIGQUEUE: {
			if (sigqueue(catcher_pid, sig, (union sigval) 0) != 0)	{
				fprintf(stderr, "Sending signal %d failed.\n", sig);
				return;
			}
			break;
		}
		default: 
			fprintf(stderr, "Incorrect mode.\n");
	}

	return;
}


int
main(int argc, char * argv[]) {
	if (argc != 4) {
		fprintf(stderr, "Pass 3 parameters exactly.\n");
		return -1;
	}
	
	int catcher_pid;
	int no_signals;
	
	if ((catcher_pid = (int) read_natural(argv[1])) == -1L) {
		fprintf(stderr, "Catcher PID incorrect.\n");
		return -1;
	}
	if ((no_signals = read_natural(argv[2])) == -1L) {
		fprintf(stderr, "Number of signals incorrect.\n");
		return -1;
	}

	if (strcmp(argv[3], "KILL") == 0) {
		send((pid_t) catcher_pid, no_signals, KILL);
	} else if (strcmp(argv[3], "SIGQUEUE") == 0) {
		send((pid_t) catcher_pid, no_signals, SIGQUEUE);
	} else if (strcmp(argv[3], "SIGRT") == 0) {
		send((pid_t) catcher_pid, no_signals, SIGRT);
	} else {
		fprintf(stderr, "Mode incorrect.\n");
		return -1;
	}

	return 0;
}
