#include <stdio.h>
#include <stdlib.h>
#include "util.h"


int
main(int argc, char * argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Pass filter size (3 to 65) and name only.\n");
        exit(EXIT_FAILURE);
    }

    int size;
    if (sscanf(argv[1], "%d", &size) == 0) {
        fprintf(stderr, "Argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    if (make_random_filter(argv[2], size) == -1) {
        fprintf(stderr, "Failed to get random filter!\n");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}