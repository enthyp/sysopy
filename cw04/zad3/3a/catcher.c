#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

typedef enum {
	KILL,
	SIGQUEUE,
	SIGRT
} mode;

int glob_receiving = 1;
int glob_count = 0;
pid_t glob_sender_pid = 0;

void 
set_receive(int sig_out, int sig_fin);

void
send_signal(pid_t sender_pid, int sig, mode mode);

void
catch(mode mode) {
	printf("Catcher PID: %d.\n", getpid());

	// Choose signals depending on mode.
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

	set_receive(sig1, sig2);

	// Receive.
	while (glob_receiving) {
		sleep(1);
	}

	// Send signals.
	int i;
	for (i = 0; i < glob_count; i++) {
		send_signal(sig1, i, mode);
	}	
	send_signal(sig2, glob_count, mode);

	printf("Received: %d.\n", glob_count);
}

void
handle_SIG1(int sig, siginfo_t * info, void * ucontext) {
	glob_sender_pid = info -> si_pid;
	glob_count++;
}

void
handle_SIG2(int sig) {
	glob_receiving = 0;
}

void
set_receive(int sig_out, int sig_fin) {
	sigset_t blocked_signals;
	sigfillset(&blocked_signals);	
	sigdelset(&blocked_signals, sig_out);
	sigdelset(&blocked_signals, sig_fin);
	if (sigprocmask(SIG_BLOCK, &blocked_signals, NULL) != 0) {
		fprintf(stderr, "Failed to set blocked signals.\n");
		exit(-1);
	}

	struct sigaction act;
	if (sigemptyset(&act.sa_mask) != 0) {
		fprintf(stderr, "Failed to set signal handler mask.\n");
	}
	act.sa_flags = SA_SIGINFO;

	act.sa_sigaction = handle_SIG1;
	sigaction(sig_out, &act, NULL);

	act.sa_flags = 0;
	act.sa_handler = handle_SIG2;
	sigaction(sig_fin, &act, NULL);
}

void
send_signal(int sig, int count, mode mode) {
	switch (mode) {
		case KILL: case SIGRT: {
			if (kill(glob_sender_pid, sig) != 0) {
				fprintf(stderr, "Sending signal %d failed.\n", sig);
				return;
			}
			break;
		}
		case SIGQUEUE: {
			union sigval sv;
			sv.sival_int = count;
			if (sigqueue(glob_sender_pid, sig, sv) != 0)	{
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

int main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Pass 1 argument exactly.\n");
		return -1;
	}
	if (strcmp(argv[1], "KILL") == 0) {
		catch(KILL);
	} else if (strcmp(argv[1], "SIGQUEUE") == 0) {
		catch(SIGQUEUE);
	} else if (strcmp(argv[1], "SIGRT") == 0) {
		catch(SIGRT);
	} else {
		fprintf(stderr, "Mode incorrect.\n");
		return -1;
	}
	return 0;
}

