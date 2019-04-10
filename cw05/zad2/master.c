#include <stdio.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int g_listening = 1;

int 
prep_fifo(char * path) {
	struct stat stats;
    if (stat(path, &stats) != 0) {
        if (errno != ENOENT) { 
            perror("stat failed");   
            return -1;
        }
    } else {
        if (unlink(path) != 0) {
            perror("unlink failed");
            return -1;
        }
    }

	if (mkfifo(path, S_IRUSR | S_IWUSR) == -1) {
		fprintf(stderr, "Failed to create a FIFO: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

void 
handle_SIGINT(int signum) {
	printf("Received SIGINT.\n");
	g_listening = 0;
}

int 
set_handler() {
	struct sigaction act;
	if (sigemptyset(&act.sa_mask) != 0) {
		fprintf(stderr, "Failed to set signal handler mask.\n");
	}

	act.sa_flags = 0;	
	act.sa_handler = handle_SIGINT;
	return sigaction(SIGINT, &act, NULL);
}

void 
listen(char * path) {
	if (set_handler() != 0) {
		fprintf(stderr, "Failed to set SIGINT handler.\n");
		exit(-1);
	}
	
	FILE * fp;
	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Failed to open FIFO: %s.\n", strerror(errno));
		exit(-1);
	}

	while (g_listening) {
		char * line = NULL; 
		size_t len = 0;
		if (getline(&line, &len, fp) > 0) {
			printf("%s", line);
		}
		free(line);
	}
	
	fclose(fp);
}

int
main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Pass only path to FIFO.\n");
		return -1;
	}

	if (prep_fifo(argv[1]) != 0) {
		return -1;
	}

	listen(argv[1]);

	return 0;
}
