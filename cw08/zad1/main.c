#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"


int
main(int argc, char * argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Pass num of threads, block/interleaved, input image file name, "
                        "filter file name and output file name.\n");
        exit(EXIT_FAILURE);
    }

    int num_thr;
    if (sscanf(argv[1], "%d", &num_thr) == 0) {
        fprintf(stderr, "First argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    int block;
    if (strcmp(argv[2], "block") == 0) {
        block = 1;
    } else if (strcmp(argv[2], "interleaved") == 0) {
        block = 0;
    } else {
        fprintf(stderr, "Second argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    char * input_file = argv[3];
    char * filter_file = argv[4];
    char * output_file = argv[5];

    file_content * input = NULL, * filter = NULL, * output = NULL;
    if (read_file_content(input_file, &input) == -1) {
        exit(EXIT_FAILURE);
    }
    if (read_file_content(filter_file, &filter) == -1) {
        free_file_content(filter);
        exit(EXIT_FAILURE);
    }

    // Do your thing.

    free_file_content(input);
    free_file_content(filter);
    exit(EXIT_SUCCESS);
}
