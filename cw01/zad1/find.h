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
 * Function sets chosen search context element value and returns 0.
 *
 * If passed value is NULL or memory allocation fails,
 * previous value is retained and 1 is returned. 
 */
int 
set_search_context(search_context_el context_element, 
                   char * context_element_value);

/*
 * Function searches according to predefined context (searches given
 * directory for given filename and saves results to file of given 
 * name) and returns 0.
 *
 * If any problem occurs during execution 1 is returned.
 */
int 
run_search();

/*
 * Function stores contents of given temporary file in allocated
 * table and returns table index. 
 *
 * If memory allocation fails, no table indices are available 
 * or given file could not be opened for whatever reason, 
 * -1 is returned.
 */
int 
store_result(char * tmp_file);

/*
 * Function frees block of memory under given table index and returns 0.
 *
 * If index is out of table bounds, 1 is returned.
 */
int 
free_block(int index);

/*
 * Function frees all memory allocated by library functions.
 * Temporary files are not deleted.
 */
void 
free_all();

#endif /* find */
