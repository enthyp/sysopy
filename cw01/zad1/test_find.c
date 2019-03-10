#include "find.h"
#include <stdio.h>

int 
main(int argc, 
	 char * argv[]) {
	printf("%d\n", create_table(10000));
	printf("%d\n", create_table(1000000));
	free_all();

	printf("%d\n", set_search_context(SEARCH_FILE, "bob"));
	printf("%d\n", set_search_context(SEARCH_FILE, "dog"));
	printf("%d\n", set_search_context(4, "whatever"));
	free_all();	
	return 0;
}
