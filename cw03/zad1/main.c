#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // fork
#include <stdlib.h> // system
#include <string.h>
#include <dirent.h>
#include <libgen.h> // basename
#include <sys/wait.h>

int 
walk_inner(char * path, char * rel_path) {
	DIR * dir = opendir(path);
	if (dir == NULL) {
		fprintf(stderr, "Failed to open directory: %s\n", path);	
		return -1;
	}

	struct stat sb;
	struct dirent * entry;
	while ((entry = readdir(dir)) != NULL) {
		int path_len = strlen(path);
		int name_len = strlen(entry -> d_name);

		char * new_path = (char *) malloc((path_len + name_len + 2) * sizeof(char));
		strcpy(new_path, path);
		strcat(new_path, "/");
		strcat(new_path, entry -> d_name);

		if (lstat(new_path, &sb) == -1) {
			fprintf(stderr, "Failed to get stat for path: %s\n", new_path);
			free(new_path);
			continue;
		}
		if ((sb.st_mode & S_IFMT) != S_IFDIR) {	
			free(new_path);
			continue;
		}

		if (strcmp(entry -> d_name, ".") != 0 
			&& strcmp(entry -> d_name, "..") != 0) {
			int rel_path_len = strlen(rel_path);
						
			char * new_rel_path = (char *) malloc((rel_path_len + name_len + 2) * sizeof(char));
			strcpy(new_rel_path, rel_path);
			strcat(new_rel_path, "/");
			strcat(new_rel_path, entry -> d_name);
			
			pid_t pid;
			if ((pid = fork()) < 0) {
				fprintf(stderr, "Failed to fork child process for: %s\n", new_path);
				free(new_path);
				free(new_rel_path);
			} else if (pid == 0) {
				char * cmd = (char *) malloc((path_len + name_len + 6 + 2) * sizeof(char)); 
				if (cmd == NULL) {
					fprintf(stderr, "Failed to compose 'ls' command for %s\n", new_path);
					free(new_path);
					free(rel_path);
					return -1;
				}
				
				strcpy(cmd, "ls -l ");
				strcat(cmd, new_path);
				printf("Path: %s, PID: %d\n", new_rel_path, getppid());
				if (system(cmd) != 0) {
					fprintf(stderr, "Failed to call 'ls' for: %s\n", new_path);
					free(cmd);
					free(new_path);
					free(rel_path);
					return -1;
				}	
				free(cmd);

				int result = walk_inner(new_path, new_rel_path);
				free(path);
				free(rel_path);
				return result;
			} else {
				wait(NULL);
				free(new_path);
				free(new_rel_path);
			}
		} else {
			free(new_path);
		}
	}

	closedir(dir);
	free(path);
	free(rel_path);
	return 0;
}

int 
walk(char * path) {
	struct stat sb;
	if (lstat(path, &sb) == -1) {
		fprintf(stderr, "Could not get stat for given path.\n");
		return -1;
	}	
	if ((sb.st_mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, "Pass path to a directory please.\n");
		return -1;
	}

	char * path_c = strdup(path); 
	if (path_c == NULL) {
		fprintf(stderr, "Failed to allocate memory for: %s\n", path);
		return -1;
	}
	char * rel_path = (char *) malloc(2 * sizeof(char));
	if (rel_path == NULL) {
		fprintf(stderr, "Failed to allocate memory for: %s\n", rel_path);
		return -1;
	}
	strcpy(rel_path, "#");

	return walk_inner(path_c, rel_path);
}

int 
main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Pass root directory path only.\n");
		return -1;
	}

	return walk(argv[1]);
}

