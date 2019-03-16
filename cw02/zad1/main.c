#include <stdio.h>
#include <fcntl.h> // open etc.
#include <unistd.h> // close
#include <string.h> // strcmp
#include <stdlib.h> // strtol
#include <errno.h>
#include <limits.h>

#define MAX_RETRY 5 // max failures to read given number of bytes

/*
 * Convert string representation of a natural number (>= 0) 
 * to long and return it. 
 *
 * Otherwise return -1L.
 */
long 
read_natural(char * string) {
	char *endp;
	long result = strtol(string, &endp, 10);

	if (endp == string) {
		return -1L;
	}
	if ((result == LONG_MAX || result == LONG_MIN) && errno == ERANGE) {
		return -1L;
	}
	if (*endp != '\0') {
		return -1L;
	}

	return result;	
}

/*
 * Create new file and fill with random data blocks, return 0.
 * 
 * Otherwise return -1.
 */
int
generate(char * fpath, long blocknum, long blocklen) {	
	// Create output file.
	int fd;
	int flags = O_WRONLY | O_CREAT | O_EXCL;
	int mode = S_IRWXU;
	if ((fd = open(fpath,  flags, mode)) == -1 && errno == EEXIST) {
		fprintf(stderr, "generate: file already exists!\n");	
		return -1;
	}
	
	// Get buffer and open /dev/urandom.
	char * buffer = (char *) malloc(blocklen);
	if (buffer == NULL) {
		fprintf(stderr, "generate: memory allocation failure!\n");
		return -1;
	}

	int frand = open("/dev/urandom", O_RDONLY);
	if (frand == -1) {
		fprintf(stderr, "generate: could not open random device!\n");
		free(buffer);
		return -1;
	}

	// Fill with random.
	int i;
	int retry = MAX_RETRY;
	int num_written = 0;
	int pos_correct = 1;
	for (i = 0; i < blocknum && retry >= 0; i++) {
		if (read(frand, buffer, blocklen) < blocklen) {
			fprintf(stderr, "generate: failed to read required number of bytes!\n");
			retry--;
			i--;	
		} else {
			if (pos_correct == 1 && (num_written = write(fd, buffer, blocklen)) < blocklen) {
				fprintf(stderr, "generate: failed to write to file!\n");
				retry--;
				i--;
			
				if (lseek(fd, -num_written, SEEK_CUR) == -1) {
					pos_correct = 0;
				}
			} else if (pos_correct == 0) {
				if (lseek(fd, -num_written, SEEK_CUR) == -1) {
					retry--;
					i--;
				} else {
					pos_correct = 1;
				}
			}
		}
	}

	if (retry < 0) {
		fprintf(stderr, "generate: failed to generate random data!\n");
		close(fd);
		close(frand);
		free(buffer);
		return -1;
	}
	
	// Free resources.
	if (close(fd) == -1) {
		fprintf(stderr, "generate: could not close file!\n");
		close(frand);
		free(buffer);
		return -1;
	} else if (close(frand) == -1) {
		fprintf(stderr, "generate: could not close random device!\n");
		free(buffer);
		return -1;	
	}
	
	free(buffer);
	return 0;
}

int 
main(int argc, char * argv[]) {
	if (argc < 5) {
		fprintf(stderr, "Not enough arguments!\n");
		return -1;
	}

	if (strcmp(argv[1], "generate") == 0) {
		long blocknum = read_natural(argv[3]);
		long blocklen = read_natural(argv[4]);

		if (blocknum <= 0 && blocklen <= 0) {
			fprintf(stderr, "Numeric arguments incorrect!\n");
			return -1;
		}

		generate(argv[2], blocknum, blocklen);
	}
	
	return 0;
}

