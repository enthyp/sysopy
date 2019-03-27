#include <stdio.h>
#include "reader/reader.h"

int main(int argc, char * argv[]) {
	if (argc != 4) {
		fprintf(stderr, "Pass 4 arguments exactly.\n");
		return -1;
	}

	long monitime;
	if ((monitime = read_natural(argv[2])) < 0) {
		fprintf(stderr, "Pass correct monitoring time.\n");
		return -1;
	}

	flist list = get_flist(argv[1]);
	if (list.size < 0) {
		return -1;
	}

	print_flist(&list);	
	
	return 0;
}
