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

// Defined below.
void 
_free_results_mem();

int
create_table(int size) {
    if (size > 0) {
        char ** tmp_blocks = (char **) calloc(size, sizeof(char *));
        bool * tmp_taken = (bool *) calloc(size, sizeof(bool));

        if (tmp_blocks != NULL && tmp_taken != NULL) {
            _free_results_mem();
            results.count = size;
			results.taken = tmp_taken;
            results.blocks = tmp_blocks; 
            return 0;
        } 
    }

    return 1;
}

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

int
run_search() {
    // Check if context defined.
    if (context.dir_name != NULL 
        && context.file_name != NULL 
        && context.tmp_file_name != NULL) {
		// Build command.
		char * cmd = (char *) calloc(16 + strlen(context.dir_name) + 
			strlen(context.file_name) + 
			strlen(context.tmp_file_name), 
			sizeof(char));

		if (cmd != NULL) {
        	if (sprintf(cmd, "find %s -name %s > %s", context.dir_name,
				context.file_name, context.tmp_file_name) > 0) {
				return system(cmd);
			}            
		}
    }

    return 1;
}

/* Function returns first index available in results structure
 * or -1 if none are available.
 *
 * This is suboptimal obviously - linear insert, constant delete.
 * Both could be logarithmic with a balanced tree structure... 
 */
int 
_first_free() {
	int i;
	for (i = 0; i < results.count; i++)
		if (!results.taken[i])
			return i;

	return -1;
}

/*
 * Function returns file length in bytes given a non-NULL file 
 * pointer or -1 in case of a failure to do so.
 */
long
_file_size(FILE * fp) {
	long fsize;
	if (fseek(fp, 0, SEEK_END) == 0 
		&& (fsize = ftell(fp)) >= 0
		&& fseek(fp, 0, SEEK_SET) == 0) {
		return fsize;
	}

	return -1L;
}

/* 
 * Function allocates memory for file content and returns 0.
 *
 * In case of any failure 1 is returned.
 */
int 
_allocate_file(FILE * fp, char ** mem_block) {
	long fsize = _file_size(fp);
	char * tmp_mem_block;	

	if (fsize >= 0) {
		tmp_mem_block = (char *) calloc(fsize, sizeof(char));
		if (tmp_mem_block != NULL) {
			*mem_block = tmp_mem_block;
			return 0;
		}
	}

	return 1;
}

int 
store_result(char * tmp_file) {
	int ind = _first_free();
	FILE * fp = fopen(tmp_file, "r");

	if (fp != NULL && ind >= 0) {
		long fsize = 0L;
		char * mem_block = NULL;		

		if (_allocate_file(fp, &mem_block) == 0) {
			if (fread(mem_block, 1, fsize, fp) == fsize 
					&& fclose(fp) == 0) {
				results.blocks[ind] = mem_block;
				results.taken[ind] = true;
				return ind;
			} else {
				free(mem_block);
				return -1;
			}
		}
	}

	if (fp != NULL) {
		fclose(fp);
	}

	return -1;	
}

int free_block(int index) {
    if (index >= 0 && index < results.count) {
        free(*(results.blocks + index));
        *(results.taken + index) = false;
		return 0;
    }

    return 1;
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
    int i;
    char ** iter = results.blocks;
    for (i = 0; i < results.count; i++) {
		if (results.taken[i])
        	free(*(iter + i));
    }
	
	free(results.taken);
    results.taken = NULL;

    free(results.blocks);
    results.blocks = NULL;

    results.count = 0;
}

void
free_all() {
    _free_context_mem();
    _free_results_mem();
}

