#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>

char * wait_msg = "Oczekuję na CTRL+Z - kontynuacja albo CTR+C - zakończenie programu.";
char * sigint_msg = "Odebrano sygnał SIGINT.";
int glob_show = 1;


int
print_time(void) {
	time_t t;
	if((t = time(NULL)) == -1) {
		fprintf(stderr, "Current datetime unavailable!\n");
		return -1;
	}

	struct tm tm = *localtime(&t);
	printf("%d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	return 0;
}

void
handle_SIGTSTP(int signum){
	glob_show = 1 - glob_show;
	if (!glob_show) {
		printf("%s\n", wait_msg);
	}
};

void
handle_SIGINT(int signum) {
	printf("%s\n", sigint_msg);
	exit(0);
}

void
run() {
	if (signal(SIGTSTP, handle_SIGTSTP) == SIG_ERR) {
		fprintf(stderr, "Failed to set SIGTSTP handler.\n");
		exit(-1);
	}

	struct sigaction act;
	act.sa_handler = handle_SIGINT;
	if (sigemptyset(&act.sa_mask) != 0) {
		fprintf(stderr, "Failed to create sigset.\n");
		exit(-1);
	}
	act.sa_flags = 0;
	if (sigaction(SIGINT, &act, NULL) != 0) {
		fprintf(stderr, "Failed to set SIGINT handler.\n");
		exit(-1);
	}

	while (1) {
		if (glob_show) {
			print_time();
			sleep(1);
		}
	}
}

int child_pid = -1;

void
fork_sub() {
	if ((child_pid = fork()) < 0) {
		fprintf(stderr, "Failed to fork subprocess.\n");
		exit(-1);
	} else if (child_pid == 0) {
		prctl(PR_SET_PDEATHSIG, SIGKILL);
		if (execlp("./date_loop.sh", "./date_loop.sh", NULL) == -1) {
			fprintf(stderr, "Failed to run the loop in subprocess.\n");
			exit(-1);
		}
	}
}

void
handle_SIGTSTP_sub(int signum){
	glob_show = 1 - glob_show;
	if (!glob_show) {
		printf("%s\n", wait_msg);
		if (waitpid(child_pid, NULL, WNOHANG) == 0 && kill(child_pid, SIGKILL) != 0) {
			fprintf(stderr, "Failed to kill the child process.\n");
			exit(-1);
		}
	} else {
		fork_sub();
	}	
};

void
handle_SIGINT_sub(int signum) {
	printf("%s\n", sigint_msg);
	if (waitpid(child_pid, NULL, WNOHANG) == 0 && kill(child_pid, SIGKILL) != 0) {
		fprintf(stderr, "Failed to stop child process.\n");
		exit(-1);
	}
	exit(0);
}

void
run_subprocess() {
	fork_sub();

	if (signal(SIGTSTP, handle_SIGTSTP_sub) == SIG_ERR) {
		fprintf(stderr, "Failed to set SIGTSTP handler.\n");
		exit(-1);
	}

	struct sigaction act;
	act.sa_handler = handle_SIGINT_sub;
	if (sigemptyset(&act.sa_mask) != 0) {
		fprintf(stderr, "Failed to create sigset.\n");
		exit(-1);
	}
	act.sa_flags = 0;
	if (sigaction(SIGINT, &act, NULL) != 0) {
		fprintf(stderr, "Failed to set SIGINT handler.\n");
		exit(-1);
	}

	while (1) {
		sleep(1);
	}
}

int
main(int argc, char * argv[]) {
	if (argc == 1) {
		run();
	} else if (argc == 2) {
		run_subprocess();
	} else {
		fprintf(stderr, "Pass 1 argument or none.\n");
		return -1;
	}

	return 0;
}
