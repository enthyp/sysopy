#include "find.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Struct describing what will be looked for
 * and where to save the results. 
 */
typedef struct {
	char * dir_name;
	char * file_name;
	char * tmp_file_name;
} search_context;

static search_context context;

/* Struct describing memory blocks allocated 
 * by module's functions.
 */
typedef struct {
	int count;
	bool * taken;
	char ** blocks;
} search_results;

static search_results results;

/* 
 * Function sets target value to source string and returns 0; 
 *
 * If passed value is NULL or memory allocation fails,
 * previous value is retained and 1 is returned. 
 */
int 
_set_str(char ** target,
         char * source) {
    if (source != NULL) {
        char * tmp;
        if ((tmp = strdup(source)) != NULL) {
            free(*target);
            *target = tmp;
            return 0;
        } else {
            fprintf(stderr, "Memory allocation failure");
        }
    }
    
    return 1;
}

int 
set_search_context(search_context_el context_element, 
                   char * context_element_value) {
    char ** target;
    switch(context_element) {
        case SEARCH_DIR: 
            target = &context.dir_name;
            break;
        case SEARCH_FILE: 
            target = &context.file_name;
            break;
        case TMP_FILE: 
            target = &context.tmp_file_name;
            break;
        default:
            return 1;
    }

    return _set_str(target, context_element_value);
}

/*
 * Function frees search context memory.
 */
void 
_free_context_mem() {
    free(context.dir_name);
    free(context.file_name);
    free(context.tmp_file_name);
}

/*
 * Function frees search results memory.
 */
void 
_free_results_mem() {
    free(results.taken);
    results.taken = NULL;

    int i;
    char ** iter = results.blocks;
    for (i = 0; i < results.count; i++) {
        free(*(iter + i));
    }

    free(results.blocks);
    results.blocks = NULL;
}

int
create_table(int size) {
    if (size > 0) {
        char ** tmp_blocks = (char **) calloc(size, sizeof(char *));
        bool * tmp_taken = (bool *) calloc(size, sizeof(bool));

        if (tmp_blocks != NULL && tmp_taken != NULL) {
            _free_results_mem();
            results.taken = tmp_taken;
            results.blocks = tmp_blocks; 
            return 0;
        } 
    }

    return 1;
}

int 
remove_tmp_file(char * tmp_file) {
    return 0;
}

void
free_all() {
    _free_context_mem();
    _free_results_mem();
}

