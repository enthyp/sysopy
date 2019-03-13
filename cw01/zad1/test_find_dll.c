#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <sys/times.h>
#include <dlfcn.h>


/*
 * Function converts string representation of 
 * a natural number (>= 0) to long and returns it. 
 *
 * If it could not be successfully converted, 
 * -1L is returned.
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

typedef enum operation {
	CREATE_TABLE,
	SEARCH_DIRECTORY,
	STORE_RESULT,
	REMOVE_BLOCK,
	STORE_REMOVE_REP,
	UNKNOWN
} op;


/*
 * Function returns operation code for given command.
 */
op get_op(char * string) {
	int cmd_count = 5;
	char * commands[] = {"create_table", 
						 "search_directory",
						 "store_result",
						 "remove_block",
						 "store_remove_rep"};
	
	int i;
	for (i = 0; i < cmd_count; i++)	
		if (strcmp(string, commands[i]) == 0)
			break;
	return (op) i;
}

typedef struct time_structs {
	struct timespec real_time;
	struct tms cpu_time;	
} time_structs;

typedef struct meas {
	double real_time;
	double ucpu_time;
	double scpu_time;
} meas;

time_structs set_time() {
	time_structs result;
	clock_gettime(CLOCK_MONOTONIC, &(result.real_time));
	times(&(result.cpu_time));

	return result;
}

meas get_runtime(time_structs start) {
	struct timespec start_ts, end_ts;
	struct tms start_tms, end_tms;
	time_structs end = set_time();

	start_ts = start.real_time;
	end_ts = end.real_time;
	start_tms = start.cpu_time;
	end_tms = end.cpu_time;
	
	meas result;
	result.real_time = end_ts.tv_sec - start_ts.tv_sec + 
		(end_ts.tv_nsec - start_ts.tv_nsec) / 100000000.;
	result.ucpu_time = (double)(end_tms.tms_cutime + end_tms.tms_utime - 
		start_tms.tms_cutime - start_tms.tms_utime) / CLOCKS_PER_SEC;
	result.scpu_time = (double)(end_tms.tms_cstime + end_tms.tms_stime - 
		start_tms.tms_cstime - start_tms.tms_stime) / CLOCKS_PER_SEC;
		
	return result;
}

int (*create_table)(int);
int (*set_search_context)(int, char *);
int (*run_search)();
int (*store_result)(char *);
int (*free_block)(int);
void (*free_all)();

/*
 * Function sets library function pointers and returns handle.
 * If functions could not be loaded NULL is returned.
 */
void * 
load_lib() {
	void *handle = dlopen("lib-shared/libfind.so", RTLD_LAZY);
	if (!handle) {
		return NULL;	
	}   
	
	create_table = dlsym(handle, "create_table");
	if (dlerror() != NULL) {
		dlclose(handle);
		return NULL;
	}
	set_search_context = dlsym(handle, "set_search_context");
	if (dlerror() != NULL) {
		dlclose(handle);
		return NULL;
	}
	run_search = dlsym(handle, "run_search");
	if (dlerror() != NULL) {
		dlclose(handle);
		return NULL;
	}
	store_result = dlsym(handle, "store_result");
	if (dlerror() != NULL) {
		dlclose(handle);
		return NULL;
	}
	free_block = dlsym(handle, "free_block");
	if (dlerror() != NULL) {
		dlclose(handle);
		return NULL;
	}
	free_all = dlsym(handle, "free_all");
	if (dlerror() != NULL) {
		dlclose(handle);
		return NULL;
	}
	
	return handle;
}

int 
main(int argc, 
	 char * argv[]) {
	if (argc == 1) {
		return 0;
	}

	void * handle = load_lib();
	if (handle == NULL) {
		fprintf(stderr, "Could not load dynamic library.\n");
		return 1;
	}

	printf("\n%s\n", argv[0]);

	int i;
	
	for (i = 1; i < argc; i++) {
		int result = 1;
		time_structs start;
		meas run_times;

		switch (get_op(argv[i])) {
			case CREATE_TABLE: {
				printf("%-20s", "CREATE_TABLE: ");
				int size = (int) read_natural(argv[i + 1]);

				start = set_time();
				result = create_table(size);
				run_times = get_runtime(start);

				i += 1;	
				break;
			}
			case SEARCH_DIRECTORY: {
				printf("%-20s", "SEARCH_DIRECTORY: ");
				set_search_context(0, argv[i + 1]);
				set_search_context(1, argv[i + 2]);
				set_search_context(2, argv[i + 3]);
				
				start = set_time();				
				result = run_search();
				run_times = get_runtime(start);

				i += 3;
				break;
			}
			case STORE_RESULT: {
				printf("%-20s", "STORE_RESULT");
				start = set_time();
				result = store_result(argv[i + 1]);
				run_times = get_runtime(start);

				i += 1;
				break;
			}
			case REMOVE_BLOCK: {
				printf("%-20s", "REMOVE_BLOCK");
				int index = (int) read_natural(argv[i + 1]);
				
				start = set_time();
				result = free_block(index);
				run_times = get_runtime(start);
				
				i += 1;
				break;
			}
			case STORE_REMOVE_REP: {
				printf("%-20s", "STORE_REMOVE_REP: ");
				int reps = (int) read_natural(argv[i + 2]);				
	
				start = set_time();
				while (reps > 0) {
					int ind = store_result(argv[i + 1]);
					free_block(ind);	
					reps--;
				}
				run_times = get_runtime(start);
				
				result = 0;	
				i += 2;
				break;
			}
			default:
				fprintf(stderr, "Unknown argument: %s\n", argv[i]);
		}

		if (result == 0) {
			printf("%f %f %f\n", run_times.real_time, 
				run_times.ucpu_time, run_times.scpu_time);
		} else {
			fprintf(stderr, "Error occurred: code %d\n", result);
		}	
	}

	free_all();
	dlclose(handle);
	return 0;
}

