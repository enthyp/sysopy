#include "find.h"
#include <stdio.h>

int 
main(int argc, 
	 char * argv[]) {
	printf("%d\n", set_search_context(SEARCH_FILE, "dude.txt"));
	printf("%d\n", set_search_context(SEARCH_DIR, "~/AGH/sysopy/"));
	printf("%d\n", set_search_context(TMP_FILE, "~/AGH/sysopy/tempempe"));

	run_search();
	create_table(1);	
	int i = store_result("~/AGH/sysopy/tempempe");
	printf("%d %d\n", i, free_block(i));	
	free_all();	
	
	return 0;
}
