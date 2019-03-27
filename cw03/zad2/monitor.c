#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

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

typedef struct file_list {
	int size;
	char ** name;
	char ** path;
	long * period;
} flist;

void free_flist(flist * fl) {
	int i;
	for (i = 0; i < fl -> size; i++) {
		free(fl -> name[i]);
		free(fl -> path[i]);
	}
	free(fl -> name);
	free(fl -> path);
	free(fl -> period);
}

void print_flist(flist * fl) {
	printf("Size: %d\n", fl -> size);
	int i;
	for (i = 0; i < fl -> size; i++) {
		printf("Name: %s\n", fl -> name[i]);
		printf("Path: %s\n", fl -> path[i]);
		printf("Period: %ld\n", fl -> period[i]);
	}
}

flist get_flist(char * list_path) {
	flist result; 
	result.size = -1;

	FILE * fp;
	if ((fp = fopen(list_path, "r")) == NULL) {
		fprintf(stderr, "Failed to open list file.\n");
		return result;
	}

	// Get number of lines and allocate memory.
	int no_lines = 0;
	while (!feof(fp)) { if(fgetc(fp) == '\n') no_lines++; }
	int rewind = fseek(fp, 0, SEEK_SET);

	result.name = (char **) malloc(no_lines * sizeof(char *)); 
	result.path = (char **) malloc(no_lines * sizeof(char *)); 
	result.period = (long *) malloc(no_lines * sizeof(long));
	if (rewind == -1 || result.name == NULL || result.path == NULL || result.period == NULL) {
		if (result.name != NULL) free(result.name);
		if (result.path != NULL) free(result.path);
		if (result.period != NULL) free(result.period);
		result.size = -1;
		fprintf(stderr, "Failed to allocate memory for files list.\n");
		return result;
	}

	// Process the file.
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	int i = 0;
	while ((read = getline(&line, &len, fp)) != -1) {
		char * str, * token;
		char * saveptr;
		char * tokens[3];
		int j, k;
		for (j = 0, str = line; j < 3; j++, str = NULL) {
			token = strtok_r(str, " \n", &saveptr);
			if (token == NULL) {
				for (k = 0; k < j; k++) { free(tokens[k]); }
				fprintf(stderr, "Too few tokens in list file line.\n");
				i--;
				break;
			}
			tokens[j] = strdup(token);
			if (tokens[j] == NULL) {
				for (k = 0; k < j; k++) { free(tokens[k]); }
				fprintf(stderr, "Failed to allocate memory for list file token.\n");
				i--;
				token = NULL;
				break;
			}
		}

		if (token != NULL && strtok_r(str, " \n", &saveptr) != NULL) {
			for (k = 0; k < 3; k++) { free(tokens[k]); }
			fprintf(stderr, "Too many tokens in list file line.\n");
			i--;
		} else if (token != NULL) {
			result.name[i] = tokens[0];
			result.path[i] = tokens[1];
			result.period[i] = read_natural(tokens[2]);

			if (result.period[i] < 0) { 
				fprintf(stderr, "Incorrect period format.\n");
				for (k = 0; k < j; k++) { 
					free(tokens[k]); 
				}
				i--;
			}	 
		}

		i++;
	}
	result.size = i;
	
	if (i == 0) {
		result.size = -1;
		fprintf(stderr, "Not even one correct line...\n");
		return result;
	}

	if (i < no_lines) {
		result.name = (char **) realloc(result.name, i * sizeof(char *));
		result.path = (char **) realloc(result.path, i * sizeof(char *));
		result.period = (long *) realloc(result.period, i * sizeof(long));
	}

	free(line);
	fclose(fp);
	return result;
}


int main(int argc, char * argv[]) {
	if (argc != 4) {
		fprintf(stderr, "Pass 4 arguments exactly.\n");
		return -1;
	}

	long monitime;
	if ((monitime = read_natural(argv[2])) < 0) {
		fprintf(stderr, "Pass correct monitoring time.\n");
		return -1;
	}

	flist list = get_flist(argv[1]);
	if (list.size < 0) {
		return -1;
	}

	print_flist(&list);	
	
	return 0;
}
