#define _XOPEN_SOURCE 500
#define __USE_XOPEN
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h> // realpath
#include <ftw.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define MAX_DIRS 20
#define DATE_LEN 17

typedef enum ord_qualifier {
	LE, 
	EQ,
	GR
} ord_qualifier;

ord_qualifier ord;
time_t ref_date;

int
timetos(const time_t * time, char * str_buffer) {
	struct tm * tm;
	tm = localtime(time);
	if (tm == NULL) {
		return -1;
	}
	
	size_t res = strftime(str_buffer, DATE_LEN, "%d-%m-%Y_%H:%M", tm);
	return (res < DATE_LEN-1) ? -1 : 0;
}

int 
is_cur_par(const char * path) {
	int len = strlen(path);
	if (len == 1 && strcmp(path, ".") == 0) {
		return 1;
	}

	if (len == 2 && strcmp(path, "..") == 0) {
		return 1;
	}

	if (len > 2 && (strcmp(path + len - 1, ".") == 0 || strcmp(path + len - 2, "..") == 0)) {
		return 1;
	}
	
	return 0;
}

int
print_info(const char *fpath, const struct stat *sb) {
	double d = difftime(ref_date, sb -> st_mtime);
	if (!(ord == LE && d > 60) && !(ord == EQ && d <= 60 && d >= -60) && !(ord == GR && d < -60)) {
		return 0;
	}

	char abs_path[PATH_MAX];
	if(realpath(fpath, abs_path) != NULL) {	
		printf("Path: %s\nType: ", abs_path);
	} else {
		printf("Path: %s (could not resolve absolute)\nType: ", fpath);
	}

	switch (sb -> st_mode & S_IFMT) {
		case S_IFBLK:  printf("block dev\n");  break;
		case S_IFCHR:  printf("char dev\n");   break;
		case S_IFDIR:  printf("dir\n");        break;
		case S_IFIFO:  printf("fifo\n");       break;
		case S_IFLNK:  printf("slink\n");      break;
		case S_IFREG:  printf("file\n");       break;
		case S_IFSOCK: printf("sock\n");       break;
		default:       printf("unknown\n");    break;
	}
	
	printf("Size: %ld bytes\n", sb -> st_size);

	char date_buffer[17];
	if (timetos(&(sb -> st_atime), date_buffer) == -1) {
		printf("Last access: failed to convert.\n");
	} else {
		printf("Last access: %s\n", date_buffer);
	}
	
	if (timetos(&(sb -> st_mtime), date_buffer) == -1) {
		printf("Last modification: failed to convert.\n\n");
	} else {
		printf("Last modification: %s\n\n", date_buffer);
	}

	return 0;
}

int print_info_wrapper(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
	if (is_cur_par(fpath) == 1) {
		return 0;
	}

	return print_info(fpath, sb);
}

int 
walk_manual(char * path) {
	struct stat sb;
	if (lstat(path, &sb) == -1) {
		return -1;
	}
	
	if ((sb.st_mode & S_IFMT) == S_IFDIR) {	
		DIR * dir = opendir(path);
		if (dir == NULL) {
			return -1;
		}
	
		struct dirent * entry;
		char abs_path[PATH_MAX];
		while ((entry = readdir(dir)) != NULL) {		
			if (strcmp(entry -> d_name, ".") != 0 
				&& strcmp(entry -> d_name, "..") != 0) {	
				strcpy(abs_path, path);
				strcat(abs_path, "/");
				strcat(abs_path, entry -> d_name);
				walk_manual(abs_path);
			}	
		}

		closedir(dir);
	}

	return print_info(path, &sb);
}


int walk(char * path, ord_qualifier o, time_t time, char * type) {
	ord = o;
	ref_date = time;

	if (strcmp(type, "nftw") == 0) {
		return nftw(path, print_info_wrapper, MAX_DIRS, FTW_PHYS);
	}
 
	if (strcmp(type, "dir") == 0) {
		return walk_manual(path);
	}
	
	fprintf(stderr, "Choose nftw or dir.\n");
	return -1;
}

int main(int argc, char * argv[]) {
	if (argc != 5) {
		fprintf(stderr, "Pass 4 arguments please.\n");
		return -1;
	}

	struct tm dt;
	char * res = strptime(argv[3], "%d-%m-%Y_%H:%M", &dt);

	if (res == NULL || *res != '\0') {
		fprintf(stderr, "Incorrect datetime string provided! Use dd-mm-yyyy_hh:mm format.\n");
		return -1;
	}

	time_t time;
	if ((time = mktime(&dt)) == (time_t) -1) {
		fprintf(stderr, "Could not convert struct tm to time_t.\n");
		return -1;
	}

	int outcome;
	if (strcmp(argv[2], "<") == 0) {
		outcome = walk(argv[1], LE, time, argv[4]);
	} else if (strcmp(argv[2], "=") == 0) {
		outcome = walk(argv[1], EQ, time, argv[4]);
	} else if (strcmp(argv[2], ">") == 0) {
		outcome = walk(argv[1], GR, time, argv[4]);
	} else {
		fprintf(stderr, "Choose \">\", \"=\" or \"<\".\n");
		outcome = -1;
	}

	return outcome;
}
