#include <stdio.h>
#include <stdlib.h>
#include "common.h"

int
main(int argc, char * argv[]) {
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Pass client name, local/net and UNIX socket path or IP with port number, respectively.\n");
        exit(EXIT_FAILURE);
    }

    if (argc == 4) {

    } else {

    }

    return 0;
}