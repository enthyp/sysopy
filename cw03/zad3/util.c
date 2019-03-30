#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "util.h"

double 
read_double(char * string) {
	char * rem = NULL;
	double outcome = strtod(string, &rem);
	if ((outcome == 0.0) 
		|| errno == ERANGE) {	
		return -1;
	}

	return outcome;	
}

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
		printf("Period: %e\n", fl -> period[i]);
	}
}

flist 
get_flist(char * list_path) {
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
	result.period = (double *) malloc(no_lines * sizeof(double));
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
			result.period[i] = read_double(tokens[2]);

			if (result.period[i] < 0) { 
				fprintf(stderr, "Incorrect period.\n");
				for (k = 0; k < 3; k++) { free(tokens[k]); }
				i--;
			} else {
				free(tokens[2]);
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
		result.period = (double *) realloc(result.period, i * sizeof(double));
	}

	free(line);
	fclose(fp);
	return result;
}

