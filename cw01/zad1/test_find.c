#include "find.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

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
	if (endp != "\0") {
		return -1L;
	}

	return result;	
}

typedef enum operation {
	CREATE_TABLE,
	SEARCH_DIRECTORY,
	REMOVE_BLOCK,
	UNKNOWN
} op;


/*
 * Function returns operation code for given command.
 */
op get_op(char * string) {
	int cmd_count = 3;
	char * commands[] = {"create_table", 
						 "search_directory",
						 "remove_block"};
	
	int i;
	for (i = 0; i < cmd_count; i++)	
		if (strcmp(string, commands[i]) == 0)
			break;
	return (op) i;
}

int 
main(int argc, 
	 char * argv[]) {
	if (argc == 1) {
		return 0;
	}

	int i;
	
	for (i = 1; i < argc; i++) {
		int result;
		switch (get_op(argv[i])) {
			case CREATE_TABLE: {
				int size = (int) read_natural(argv[i + 1]);
				result = create_table(size);
				i += 2;	
				break;
			}
			case SEARCH_DIRECTORY: {
				set_search_context(SEARCH_DIR, argv[i + 1]);
				set_search_context(SEARCH_FILE, argv[i + 2]);
				set_search_context(TMP_FILE, argv[i + 3]);
				result = run_search();
				i += 4;
				break;
			}
			case REMOVE_BLOCK: {
				int index = (int) read_natural(argv[i + 1]);
				result = free_block(index);
				i += 2;
				break;
			}
			default:
				fprintf(stderr, "Unknown argument: %s\n", argv[i]);
		}
	}
	// TODO: add time measurement.
	free_all();

	return 0;
}

