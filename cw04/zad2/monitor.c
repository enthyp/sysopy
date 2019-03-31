#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <sys/prctl.h>
#include "util.h"

#define DATE_LEN 20
#define MAX_CMD_LEN 13

int
timetos(const struct timespec * time, char (* str_buffer)[DATE_LEN]) {
	time_t secs = time -> tv_sec;
	struct tm * tm;
	tm = localtime(&secs);
	if (tm == NULL) {
		return -1;
	}
	
	size_t res = strftime(*str_buffer, DATE_LEN, "%F_%H-%M-%S", tm);
	return (res < DATE_LEN-1) ? -1 : 0;
}

long
from_file(char ** cache, char * path) {
	FILE * fp;
	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Failed to open file %s.\n", path);
		return -1;
	}

	if (fseek(fp, 0, SEEK_END) == -1) {
		fprintf(stderr, "Failed to seek in file %s.\n", path);
		fclose(fp);
		return -1;
	}
	long fsize;
	if ((fsize = ftell(fp)) == -1) {
		fprintf(stderr, "Failed to get offset in %s.\n", path);
		fclose(fp);
		return -1;
	}

	if (fseek(fp, 0, SEEK_SET) == -1) {
		fprintf(stderr, "Failed to seek in file %s.\n", path);
		fclose(fp);
		return -1;
	}
	char * tmp_cache = (char *) malloc(fsize + 1);
	if (tmp_cache == NULL) {
		fprintf(stderr, "Failed to allocate memory for file %s\n", path);
		fclose(fp);
		return -1;
	}
	if (fread(tmp_cache, 1, fsize, fp) < fsize) {
		fprintf(stderr, "Failed to read file %s to memory.\n", path);
		free(tmp_cache);
		fclose(fp);
		return -1;
	}
	free(*cache);
	*cache = tmp_cache;
	fclose(fp);
	return fsize;
}

int 
to_archive(char * name, char * content, long fsize) {
	char arch_path[PATH_MAX];
	strcpy(arch_path, "./archive/");
	strcat(arch_path, name);
	FILE * arch_fp;
	if ((arch_fp = fopen(arch_path, "w")) == NULL) {
		fprintf(stderr, "Failed to open copy of file %s.\n", arch_path);
		return -1;
	}
	if (fwrite(content, 1, fsize, arch_fp) < fsize) {
		fprintf(stderr, "Failed to save copy of file %s.\n", arch_path);
		fclose(arch_fp);
		return -1;
	}
	
	fclose(arch_fp);
	return 0;
}

int running = 1, working = 1;

void
handle_SIGINT_sub(int signum) {
	running = 0;
}

void
handle_SIGUSR1_sub(int signum) {
	if (!working) {
		printf("Process %d starting...\n", getpid());
	}
	working = 1;
}

void
handle_SIGUSR2_sub(int signum) {
	if (working) {
		printf("Process %d stopping...\n", getpid());
	}
	working = 0;
}

int
get_file_name(char ** buffer, char * name, struct timespec mod_time, int has_dupl) {
	int name_len = strlen(name) + strlen("./archive/") + DATE_LEN + 2; 
	char * tmp_buffer = (char *) malloc(name_len * sizeof(char));
	if (tmp_buffer == NULL) {
		fprintf(stderr, "Failed to allocate memory for file name: %s.\n", name);
		return -1;
	}

	char mod_date[DATE_LEN];
	if (timetos(&mod_time, &mod_date) == -1) {
		fprintf(stderr, "Failed to convert date for %s.\n", name);
		free(tmp_buffer);
		return -1;
	}

	strcpy(tmp_buffer, "./archive/");
	strcat(tmp_buffer, name);
	strcat(tmp_buffer, "_");
	strcat(tmp_buffer, mod_date);
	if (has_dupl == 1) {
		tmp_buffer = (char *) realloc(tmp_buffer, name_len + 6);
		if (tmp_buffer == NULL) {
			fprintf(stderr, "Failed to allocate file name memory.\n");
			return -1;
		}
		char pid[5];
		sprintf(pid, "%d", getpid());
		strcat(tmp_buffer, "_");
		strcat(tmp_buffer, pid);
	}

	if (*buffer != NULL) { 
		free(*buffer);
	}
	*buffer = tmp_buffer;

	return 0;
}

int 
monitor_inner(char * name, char * path, double period, int has_dupl) {
	char * cache = NULL;
	long fsize = from_file(&cache, path);
	if (fsize < 0) {
		return -1;
	}

	struct stat sb;
	if (lstat(path, &sb) == -1) {
		fprintf(stderr, "Failed to get stat for: %s.\n", name);
		free(cache);
		return -1;
	}

	struct sigaction act;
	act.sa_handler = handle_SIGINT_sub;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	act.sa_handler = handle_SIGUSR1_sub;
	sigaction(SIGUSR1, &act, NULL);

	act.sa_handler = handle_SIGUSR2_sub;
	sigaction(SIGUSR2, &act, NULL);

	struct timespec mod_time = sb.st_mtim;
	struct timespec sleep_dur; 
	sleep_dur.tv_sec = (long)period;
	sleep_dur.tv_nsec = (time_t)((period - (long)period) * 1.0e9);
	int count = 0;
	while (running) {
		nanosleep(&sleep_dur, NULL);
		if (!working) {
			continue;
		}

		if (lstat(path, &sb) == -1) {
			fprintf(stderr, "Failed to get stat for: %s.\n", name);
			free(cache);
			return -1;
		}

		if (difftime(sb.st_mtim.tv_sec, mod_time.tv_sec) != 0) {
			char * arch_name;
			if (get_file_name(&arch_name, name, mod_time, has_dupl) == -1) {
				fprintf(stderr, "Failed to create archive file name.\n");
				free(cache);
				return -1;
			}

			if (to_archive(arch_name, cache, fsize) == -1) {
				fprintf(stderr, "Failed to archive file.\n");
				if (arch_name != NULL) {
					free(arch_name);
				}
				free(cache);
				return -1;
			}
			free(arch_name);
			count++;
			fsize = from_file(&cache, path);
			
			if (fsize < 0) {
				fprintf(stderr, "Failed to store new version of %s.\n", name);
				free(cache);
				return -1;
			}
			mod_time = sb.st_mtim;
		}
	}

	free(cache);
	return count;
}

void 
list(pid_t * children, int no_children) {
	printf("Monitoring processes:\n");
	int i;
	for (i = 0; i < no_children; i++) {
		printf("\t%d\n", children[i]);
	}
}

void
start(pid_t pid, pid_t * children, int no_children) {
	int i;
	for (i = 0; i < no_children; i++) {
		if (pid == children[i]) {
			if (kill(pid, SIGUSR1) != 0) {
				fprintf(stderr, "Failed to send starting signal to child process %d.\n", pid);
			} 
			return;
		}
	}
	fprintf(stderr, "No monitoring process with such PID.\n");
}

void 
stop(pid_t pid, pid_t * children, int no_children) {
	int i;
	for (i = 0; i < no_children; i++) {
		if (pid == children[i]) {
			if (kill(pid, SIGUSR2) != 0) {
				fprintf(stderr, "Failed to send stopping signal to child process %d.\n", pid);
			} 
			return;
		}
	}
	fprintf(stderr, "No monitoring process with such PID.\n");
}

void
start_all(pid_t * children, int no_children) {
	int i;
	for (i = 0; i < no_children; i++) {
		if (kill(children[i], SIGUSR1) != 0) {
			fprintf(stderr, "Failed to send starting signal to child process %d.\n", children[i]);
		}	
	}
}

void 
stop_all(pid_t * children, int no_children) {
	int i;
	for (i = 0; i < no_children; i++) {
		if (kill(children[i], SIGUSR2) != 0) {
			fprintf(stderr, "Failed to send starting signal to child process %d.\n", children[i]);
		}	
	}
}

void 
end(int signum) {
	running = 0;
}

void
handle_cmd(char * cmd, pid_t * children, int no_children) {
	if (strcmp(cmd, "LIST") == 0) {
		list(children, no_children);
	} else if (strcmp(cmd, "STOP ALL") == 0) {
		stop_all(children, no_children);
	} else if (strcmp(cmd, "START ALL") == 0) {
		start_all(children, no_children);
	} else if (strcmp(cmd, "END") == 0) {
		end(0);
	} else {
		char * tokens[2];
		if (split_str(cmd, tokens, 2) == -1) {
			fprintf(stderr, "Failed to process command.\n");
			return;
		}
		
		long pid;
		if ((pid = read_natural(tokens[1])) == -1) {
			fprintf(stderr, "Failed to process command.\n");
			free(tokens[0]); free(tokens[1]);
			return;
		}

		if (strcmp(tokens[0], "START") == 0) {
			start((pid_t) pid, children, no_children);
		} else if (strcmp(tokens[0], "STOP") == 0) {
			stop((pid_t) pid, children, no_children);
		} else {
			fprintf(stderr, "Unknown command.\n");	
		}

		free(tokens[0]); free(tokens[1]);
		return;
	}
}

int
monitor(flist * list) {
	// Check for duplicate file names.
	int * has_dupl = (int *) calloc(list -> size, sizeof(int));	
	pid_t * children = (pid_t *) malloc(list -> size * sizeof(pid_t));
	if (has_dupl == NULL || children == NULL) {
		if (has_dupl != NULL) free(has_dupl);
		if (children != NULL) free(children);
		free_flist(list);
		fprintf(stderr, "Failed to allocate memory.\n");
		return -1;
	}
	int i, j;
	for (i = 0; i < list -> size - 1; i++) {
		if (has_dupl[i] == 0) {
			for (j = i + 1; j < list -> size; j++) {
				if (has_dupl[j] == 0 && 
					strcmp(list -> name[i], list -> name[j]) == 0) {
					has_dupl[i] = has_dupl[j] = 1;
				}
			}
		}
	}

	// Create archive directory.
	if (mkdir("./archive", S_IRWXU) == -1 && errno != EEXIST) {
		fprintf(stderr, "Failed to create archive directory.\n");
		free(has_dupl);
		free(children);
		free_flist(list);
		return -1;
	}

	// Start off monitoring subprocesses.
	int proc_count = list -> size;
	for (i = 0; i < list -> size; i++) {
		pid_t pid;
        if ((pid = fork()) < 0) {
			proc_count--;
			fprintf(stderr, "Failed to fork child process for %s.\n ", list -> name[i]);
		} else if (pid == 0) {
			prctl(PR_SET_PDEATHSIG, SIGKILL);
			int result = monitor_inner(list -> name[i], list -> path[i], list -> period[i], has_dupl[i]);	
			free_flist(list);
			free(children);
			free(has_dupl);
			printf("Process %d exits...\n", getpid());
			return -result;
		} else {
			children[i] = pid;
			printf("Monitoring %s...\n", list -> path[i]);
		}
	}
	
	struct sigaction act = {0};
	act.sa_handler = end;
	if (sigaddset(&act.sa_mask, SIGTSTP) != 0) {
		fprintf(stderr, "Failed to add SIGTSTP to blocked signals.\n");
	}
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	// Command loop.
	while (running) {
		char cmd[MAX_CMD_LEN] = "";
		if (fgets(cmd, MAX_CMD_LEN, stdin) == NULL && running) {
			fprintf(stderr, "Failed to read command.\n");
			continue;
		}
	
		if ((strlen(cmd) > 0) && (cmd[strlen(cmd) - 1] == '\n')) {
			cmd[strlen(cmd) - 1] = '\0';
		}

		if (running) {
			handle_cmd(cmd, children, proc_count);
		}
	}

	// Upon receiving SIGINT or END command.	
	for (i = 0; i < proc_count; i++) {
		if (kill(children[i], SIGINT) != 0) {
			fprintf(stderr, "Failed to send ending signal to child process %d.\n", children[i]);
		}
	}

	for (i = 0; i < proc_count; i++) {
		int statloc;
		if (waitpid(children[i], &statloc, 0) == -1) {
			fprintf(stderr, "Error waiting for child process.\n");
		} else if (WIFEXITED(statloc) && WEXITSTATUS(statloc) != 1) {
			printf("Proces %d utworzył %d kopii pliku %s.\n", children[i], (256 - WEXITSTATUS(statloc)) % 256, list -> name[i]);
		} else {
			printf("Proces %d: wystąpił błąd.\n", children[i]);
		}
	}

	free(has_dupl);
	free(children);
	free_flist(list);
	return 0;
}

int 
main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Pass list file path only.\n");
		return -1;
	}

	flist list = get_flist(argv[1]);
	if (list.size < 0) {
		return -1;
	}
	
	return monitor(&list);
}

