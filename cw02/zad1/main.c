#include <stdio.h>
#include <fcntl.h> // open etc.
#include <unistd.h> // close
#include <string.h> // strcmp
#include <stdlib.h> // strtol
#include <errno.h>
#include <limits.h>

// TODO: better error messages!

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

int 
sort_sys(char * fpath, long blocknum, long blocklen) {
	int fd;
	int flags = O_RDWR;
	if ((fd = open(fpath, flags)) == -1) {
		perror(NULL);
		return -1;
	}

	char * cur_buffer = (char *) malloc(blocklen);
	char * min_buffer = (char *) malloc(blocklen);
	if (cur_buffer == NULL || min_buffer == NULL) {
		perror(NULL);	
		close(fd);	
		return -1;
	}

	int i, outcome = 0;
	for (i = 0; outcome == 0 && i < blocknum - 1; i++) {
		if (lseek(fd, i * blocklen, SEEK_SET) == (off_t) -1 
			|| read(fd, cur_buffer, blocklen) < blocklen) {
			outcome = -1;
			perror(NULL);
			break;	
		}
		
		int min_ind = i;
		unsigned char min_el = (unsigned char) cur_buffer[0];
		int j;
		for (j = i + 1; j < blocknum; j++) {
			if (lseek(fd, j * blocklen, SEEK_SET) == (off_t) -1 
				|| read(fd, min_buffer, 1) < 1)	 {
				outcome = -1;
				perror(NULL);
				break;
			} 
	
			if ((unsigned char) min_buffer[0] < min_el) {
				min_ind = j * blocklen;
				min_el = (unsigned char) min_buffer[0];
			}	
		}

		if (outcome == 0 && i != min_ind)  {
			if (lseek(fd, min_ind, SEEK_SET) == (off_t) -1 
				|| read(fd, min_buffer, blocklen) < blocklen)	 {
				outcome = -1;
				perror(NULL);
				break;
			}	

			if (lseek(fd, i * blocklen, SEEK_SET) == -1
				|| write(fd, min_buffer, blocklen) < blocklen
				|| lseek(fd, min_ind, SEEK_SET) == -1
				|| write(fd, cur_buffer, blocklen) < blocklen) {
				outcome = -1;
				perror(NULL);
				break;
			}
		}
	}

	close(fd);
	free(cur_buffer);
	free(min_buffer);

	return outcome;
}

int
copy_sys(char * from, char * to, long blocknum, long blocklen) {
	int from_fd;
	int flags = O_RDONLY;
	if ((from_fd = open(from, flags)) == -1) {
		perror(NULL);
		return -1;
	}

	int to_fd;
	int mode = S_IRWXU;
	flags = O_WRONLY | O_CREAT; 
	if ((to_fd = open(to, flags, mode)) == -1) {
		perror(NULL);
		close(from_fd);		
		return -1;
	}

	char * buffer = (char *) malloc(blocklen);
	if (buffer == NULL) {
		perror(NULL);	
		close(from_fd);
		close(to_fd);	
		return -1;
	}

	int i, outcome = 0;
	for (i = 0; i < blocknum; i++) {
		if (read(from_fd, buffer, blocklen) < blocklen) {
			perror(NULL);		
			outcome = -1;
			break;
		} else if (write(to_fd, buffer, blocklen) < blocklen) {
			perror(NULL);
			outcome = -1;
			break;
		}
	}
	
	close(from_fd);
	close(to_fd);
	free(buffer);

	return outcome;
}

int
generate(char * fpath, long blocknum, long blocklen) {	
	return copy_sys("/dev/urandom", fpath, blocknum, blocklen);
}

int 
sort_lib(char * fpath, long blocknum, long blocklen) {
	FILE * f;
	if ((f = fopen(fpath, "r+")) == NULL) {
		perror(NULL);
		return -1;
	}

	char * cur_buffer = (char *) malloc(blocklen);
	char * min_buffer = (char *) malloc(blocklen);
	if (cur_buffer == NULL || min_buffer == NULL) {
		perror(NULL);	
		fclose(f);	
		return -1;
	}

	int i, outcome = 0;
	for (i = 0; outcome == 0 && i < blocknum - 1; i++) {
		if (fseek(f, i * blocklen, SEEK_SET) == -1 
			|| fread(cur_buffer, 1, blocklen, f) < blocklen) {
			outcome = -1;
			perror(NULL);
			break;	
		}
		
		int min_ind = i;
		unsigned char min_el = (unsigned char) cur_buffer[0];
		int j;
		for (j = i + 1; j < blocknum; j++) {
			if (fseek(f, j * blocklen, SEEK_SET) == -1 
				|| fread(min_buffer, 1, 1, f) < 1)	 {
				outcome = -1;
				perror(NULL);
				break;
			} 
	
			if ((unsigned char) min_buffer[0] < min_el) {
				min_ind = j * blocklen;
				min_el = (unsigned char) min_buffer[0];
			}	
		}

		if (outcome == 0 && i != min_ind)  {
			if (fseek(f, min_ind, SEEK_SET) == -1 
				|| fread(min_buffer, 1, blocklen, f) < blocklen)	 {
				outcome = -1;
				perror(NULL);
				break;
			}	

			if (fseek(f, i * blocklen, SEEK_SET) == -1
				|| fwrite(min_buffer, 1, blocklen, f) < blocklen
				|| fseek(f, min_ind, SEEK_SET) == -1
				|| fwrite(cur_buffer, 1, blocklen, f) < blocklen) {
				outcome = -1;
				perror(NULL);
				break;
			}
		}
	}

	fclose(f);
	free(cur_buffer);
	free(min_buffer);

	return outcome;
}

int 
copy_lib(char * from , char * to, long blocknum, long blocklen) {
	FILE * from_f;
	if ((from_f = fopen(from, "r")) == NULL) {
		perror(NULL);
		return -1;
	}

	FILE * to_f;
	if ((to_f = fopen(to, "w")) == NULL) {
		perror(NULL);
		fclose(from_f);		
		return -1;
	}

	char * buffer = (char *) malloc(blocklen);
	if (buffer == NULL) {
		perror(NULL);	
		fclose(from_f);
		fclose(to_f);	
		return -1;
	}

	int i, outcome = 0;
	for (i = 0; i < blocknum; i++) {
		if (fread(buffer, 1, blocklen, from_f) < blocklen) {
			perror(NULL);		
			outcome = -1;
			break;
		} else if (fwrite(buffer, 1, blocklen, to_f) < blocklen) {
			perror(NULL);
			outcome = -1;
			break;
		}
	}
	
	fclose(from_f);
	fclose(to_f);
	free(buffer);

	return outcome;
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
	} else if (strcmp(argv[1], "sort") == 0) {
		if (argc > 5) {
			long blocknum = read_natural(argv[3]);
			long blocklen = read_natural(argv[4]);
			if (blocknum <= 0 && blocklen <= 0) {
				fprintf(stderr, "Numeric arguments incorrect!\n");
				return -1;
			}

			if (strcmp(argv[5], "lib") == 0) {
				return sort_lib(argv[2], blocknum, blocklen);
			} else if (strcmp(argv[5], "sys") == 0) {
				return sort_sys(argv[2], blocknum, blocklen);
			} else {
				fprintf(stderr,"Unknown argument: choose sys or lib.\n");
				return -1;
			}
		} else {
			fprintf(stderr, "Not enough arguments!\n");
			return -1;
		}
	} else if (strcmp(argv[1], "copy") == 0) {
		if (argc > 6) {
			long blocknum = read_natural(argv[4]);
			long blocklen = read_natural(argv[5]);
			if (blocknum <= 0 && blocklen <= 0) {
				fprintf(stderr, "Numeric arguments incorrect!\n");
				return -1;
			}

			if (strcmp(argv[6], "lib") == 0) {
				return copy_lib(argv[2], argv[3], blocknum, blocklen);
			} else if (strcmp(argv[6], "sys") == 0) {
				return copy_sys(argv[2], argv[3], blocknum, blocklen);
			} else {
				fprintf(stderr,"Unknown argument: choose sys or lib.\n");
				return -1;
			}
		} else {
			fprintf(stderr, "Not enough arguments!\n");
			return -1;
		}
	
	} else {
		fprintf(stderr, "Unknown argument: %s\n", argv[1]);
		return -1;
	}

	return 0;
}

