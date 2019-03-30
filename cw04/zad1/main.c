#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

char * wait_msg = "Oczekuję na CTRL+Z - kontynuacja albo CTR+C - zakończenie programu.";
char * sigint_msg = "Odebrano sygnał SIGINT.";
int show = 1;


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
	show = 1 - show;
	if (!show) {
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
	signal(SIGTSTP, handle_SIGTSTP);

	struct sigaction act;
	act.sa_handler = handle_SIGINT;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	while (1) {
		if (show) {
			print_time();
			sleep(1);
		}
	}
}

void
run_subprocess() {
	exit(0);
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
