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
#include "util.h"

#define DATE_LEN 20

typedef enum mode {
	MEM,
	CP
} mode;

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
		fprintf(stderr, "Failed to alocate memory for file %s\n", path);
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

// TODO: make struct of first 3
int 
monitor_mem(char * name, char * path, long period, long monitime, int has_dupl) {
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

	struct timespec mod_time = sb.st_mtim;
	long elapsed = 0;
	int count = 0;
	while (elapsed <= monitime) {
		sleep(period);
		if (lstat(path, &sb) == -1) {
			fprintf(stderr, "Failed to get stat for: %s.\n", name);
			free(cache);
			return -1;
		}

		if (difftime(sb.st_mtim.tv_sec, mod_time.tv_sec) != 0) {
			mod_time = sb.st_mtim;
			int name_len = strlen(name) + DATE_LEN + 1; 
			char * arch_name = (char *) malloc(name_len * sizeof(char));
			char mod_date[DATE_LEN];
			if (timetos(&mod_time, &mod_date) == -1) {
     	  		fprintf(stderr, "Failed to convert date for %s.\n", name);
				free(arch_name);
				break;
			}
	
			strcpy(arch_name, name);
			strcat(arch_name, "_");
			strcat(arch_name, mod_date);
			if (has_dupl == 1) {
				arch_name = (char *) realloc(arch_name, name_len + 6);
				if (arch_name == NULL) {
					fprintf(stderr, "Failed to allocate file name memory.\n");
					break;
				}
				char pid[5];
				sprintf(pid, "%d", getpid());
				strcat(arch_name, "_");
				strcat(arch_name, pid);
			}

			if (arch_name == NULL || to_archive(arch_name, cache, fsize) == -1) {
				fprintf(stderr, "Failed to archive file.\n");
				if (arch_name != NULL) {
					free(arch_name);
				}
				break;
			}
			free(arch_name);
			count++;
			fsize = from_file(&cache, path);
			
			if (fsize < 0) {
				fprintf(stderr, "Failed to store new version of %s.\n", name);
				free(cache);
				return -1;
			}
		}

		elapsed += period;
	}
	
	free(cache);
	return -count;
}

int 
monitor_cp(char * name, char * path, long period, long monitime, int has_dupl) {
	return 0;
}

int
monitor_inner(flist * list, long monitime, mode mode) {
	// To name files properly (with PID).
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

	if (mkdir("./archive", S_IRWXU) == -1 && errno != EEXIST) {
		fprintf(stderr, "Failed to create archive directory.\n");
		free(has_dupl);
		free(children);
		free_flist(list);
		return -1;
	}

	int proc_count = list -> size;
	for (i = 0; i < list -> size; i++) {
		pid_t pid;
        if ((pid = fork()) < 0) {
			proc_count--;
			fprintf(stderr, "Failed to fork child process for %s.\n ", list -> name[i]);
		} else if (pid == 0) {
			int result;
			switch (mode) {
				case MEM: 
					result = -monitor_mem(list -> name[i], list -> path[i], list -> period[i], monitime, has_dupl[i]); break;
				case CP: 
					result = -monitor_cp(list -> name[i], list -> path[i], list -> period[i], monitime, has_dupl[i]); break;
				default: break;
			}
			free_flist(list);
			free(children);
			free(has_dupl);
			return result;
		} else {
			children[i] = pid;
		}
	}
	
	for (i = 0; i < proc_count; i++) {
		int statloc;
		if (waitpid(children[i], &statloc, 0) == -1) {
			fprintf(stderr, "Error waiting for child process.\n");
		} else if (WIFEXITED(statloc)) {
			printf("Proces %d utworzył %d kopii pliku %s.\n", children[i], WEXITSTATUS(statloc), list -> name[i]);
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
monitor(flist * list, long monitime, char * mode) {
	if (strcmp(mode, "mem") == 0) { 
		return monitor_inner(list, monitime, MEM);
	} else if (strcmp(mode, "cp") == 0) {
		return monitor_inner(list, monitime, CP);
	} else {
		fprintf(stderr, "Pass mem or cp as mode.\n");
		return -1;
	}
}

int 
main(int argc, char * argv[]) {
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

	//print_flist(&list);	
	
	return monitor(&list, monitime, argv[3]);
}

