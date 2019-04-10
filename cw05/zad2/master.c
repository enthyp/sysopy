#include <stdio.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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
listen(char * path) {
	FILE * fp;
	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Failed to open FIFO: %s.\n", strerror(errno));
		exit(-1);
	}

	char * line = NULL; 
	size_t len = 0;

	while (getline(&line, &len, fp) > 0) {
		printf("%s", line);
	}
	
	free(line);
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
