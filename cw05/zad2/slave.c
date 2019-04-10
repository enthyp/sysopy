#include <stdio.h> 
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

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


void
send(char * path, int N) {
	FILE * fp;
	if ((fp = fopen(path, "w")) == NULL) {
		fprintf(stderr, "Failed to open FIFO: %s.\n", strerror(errno));
		exit(-1);
	}

	pid_t self = getpid();
	printf("PID: %d\n", self);

	srand(time(NULL));   
	int i;
	for (i = 0; i < N; i++) {
		char date[50];
		FILE * date_output = popen("date", "r");
		fgets(date, 49, date_output);
		pclose(date_output);
		printf("Written...\n");
		fprintf(fp, "PID: %d, Date: %s", self, date);
		fflush(fp);
		sleep(rand() % 4 + 2);
	}

	fclose(fp);
}

int main(int argc, char * argv[]) {
	if (argc != 3) {
		fprintf(stderr, "Pass path to FIFO and a whole number.\n");
		return -1;
	}

	int N;
	if ((N = read_natural(argv[2])) == -1) {
		fprintf(stderr, "Pass a correct number.\n");
		return -1;
	}

	send(argv[1], N);
	
	return 0;
}
