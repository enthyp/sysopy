#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <signal.h>

int
split_commands(char *** commands, char * line) {
	int len, no_cmd, is_white;
	for (len = 0, no_cmd = 1, is_white = 1; line[len] != '\0'; len++) {
		if (line[len] == '|') {
			no_cmd++;
		}
		if (line[len] != ' ' && line[len] != '\t' && line[len] != '\n') {	
			is_white = 0;
		}
	}
	
	if (no_cmd == 1 && is_white == 1) {
		return 0;
	}

	*commands = (char **) malloc(no_cmd * sizeof(char *));
	if (*commands == NULL) {
		fprintf(stderr, "Failed to allocate memory for commands.\n");
		return -1;
	}

	int i, ind;
	for (i = 0, ind = 0; i < len && line[i] != '\0'; i++, ind++) {
		int j = i;
		for (; line[j] != '|' && line[j] != '\n' && line[j] != '\0'; j++);

		line[j] = '\0';
		(*commands)[ind] = &(line[i]);
		i = j;
	}

	return no_cmd;
}

int
word_count(char * string) {
	int i, count;
	
	for (i = count = 0; string[i] != '\0'; i++) {
		int j, k;
		for (j = i; string[j] == ' ' || string[j] == '\t'; j++);
		for (k = j; string[k] != '\0' && string[k] != ' ' && string[k] != '\t'; k++);
		if (k != j) {
			count++;
		}
		i = k;
	}

	return count;
}

int
preprocess_commands(char ** (*commands)[], char ** command_strings, int no_cmd) {
	int i;
	for (i = 0; i < no_cmd; i++) {
		int no_words = word_count(command_strings[i]);
		if (no_words == 0) {
			fprintf(stderr, "Empty command encountered in block %d.\n", i + 1);
			int j;
			for (j = 0; j < i; j++) {
				free((*commands)[j]);
			}
			return -1;
		}
		
		(*commands)[i] = (char **) malloc((no_words + 1) * sizeof(char *));
		if ((*commands)[i] == NULL) {
			fprintf(stderr, "Failed to allocate memory for command words in block %d.\n", i + 1);
			int j;
			for (j = 0; j < i; j++) {
				free((*commands)[j]);
			}
			return -1;
		}

		char * str, * token;
		char * saveptr;

		int j, k;
		for (j = 0, str = command_strings[i]; j < no_words; j++, str = NULL) {
			token = strtok_r(str, " \t", &saveptr);
			if (token == NULL) {
				fprintf(stderr, "strtok_r failed for block %d.\n", i + 1);
				for (k = 0; k <= i; k++) {
					free((*commands)[k]);
				}
				return -1;
			}
			(*commands)[i][j] = token;
		}
		(*commands)[i][j] = NULL;

		if (strtok_r(str, " \n", &saveptr) != NULL) {
			for (j = 0; j <= i; j++) { 
				free((*commands)[j]); 
			}
			fprintf(stderr, "Unknown processing error for block %d.\n", i + 1);
			return -1;
		}
	}

	return 0;
} 

int 
run_pipeline(char ** commands[], char * line, int no_cmd) {
	if (no_cmd == 1) {
		pid_t pid;
		if ((pid = fork()) == -1) {
			fprintf(stderr, "Failed to fork child process.\n");
			return -1;
		} else if (pid == 0) {
			prctl(PR_SET_PDEATHSIG, SIGKILL);
			if (execvp(commands[0][0], commands[0]) == -1) {
				fprintf(stderr, "Failed to execute command in child process.\n");
				free(line);
				int i;
				for (i = 0; i < no_cmd; i++) { free(commands[i]);  }
				return -1;
			}
		} else {
			if (waitpid(pid, NULL, 0) == -1) {
				fprintf(stderr, "Failed waiting for child process.\n");
				return -1;
			}
			return 0;
		}	 
	} 

	

	return 0;
}

int
execute_line(char * line) {
	char ** command_strings;
	int no_cmd;
	// Split on pipe characters into separate commands.
	if ((no_cmd = split_commands(&command_strings, line)) == -1) {
		fprintf(stderr, "Failed to split commands on pipe characters: ");
		return -1;
	}

	if (no_cmd == 0) {
		return 0;
	}

	char ** commands[no_cmd];
	// Split each command into separate words on whitespace characters (' ' and '\t' for now).
	if (preprocess_commands(&commands, command_strings, no_cmd) == -1) {
		fprintf(stderr, "Failed to split command on whitespace characters: ");
		free(command_strings);
		return -1;
	}
	free(command_strings);

	// Stick pipes together and run the chain.
	int i;
	if (run_pipeline(commands, line, no_cmd) == -1) {
		fprintf(stderr, "Failed to run pipeline for given commands: ");
		for (i = 0; i < no_cmd; i++) {
			free(commands[i]);
		}	
		return -1;
	}

	//for (i = 0; i < no_cmd; i++) {
	//	int j;
	//	printf("Cmd %d:\n", i + 1);
	//	for (j = 0; commands[i][j] != NULL; j++) {
	//		printf("\tword: %s\n", commands[i][j]);
	//	}
	//}
	
	/* Clean up. */
	for (i = 0; i < no_cmd; i++) {
		free(commands[i]);
	}	

	return 0;
}

void
execute(FILE * fp) {
	int i = 1;
	char * line = NULL;
	size_t len = 0;
	while (getline(&line, &len, fp) != -1) {
		if (execute_line(line) == -1) {
			fprintf(stderr, "line %d.\n", i);
		}
		i++;
	}
	free(line);
}

int main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Pass only path to command file.\n");
		return -1;
	}

	FILE * input;
	if ((input = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "Failed to open input file.\n");
		return -1;
	}

	execute(input);

	fclose(input);
	return 0;
}
