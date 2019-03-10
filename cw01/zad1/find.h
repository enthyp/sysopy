/* 
 * Library providing basic management of memory blocks, allocated 
 * for `find` Unix command results.
 *
 * */

#ifndef FIND_H
#define FIND_H

typedef enum search_context_el {
    SEARCH_DIR,        /* search directory */
    SEARCH_FILE,       /* file to search for */
    TMP_FILE    /* file for search results */
} search_context_el;

/* 
 * Function sets chosen search context element value and returns 0.
 *
 * If passed value is NULL or memory allocation fails,
 * previous value is retained and 1 is returned. 
 */
int 
set_search_context(search_context_el context_element, 
                   char * context_element_value);

/*
 * Function creates table of pointers to memory blocks containing
 * search results and returns 0;
 * 
 * If a table has been allocated already, it is freed beforehand.
 * If memory allocation fails, however, previous values are 
 * retained and 1 is returned.
 */
int 
create_table(int size);

/*
 * Function removes temporary file under given path and returns 0.
 *
 * If file is not found or permissions are insufficient 1 is returned.
 */
int 
remove_tmp_file(char * tmp_file);

/*
 * Function frees all memory allocated by library functions.
 * Temporary files are not deleted.
 */
void 
free_all();

#endif /* find */
