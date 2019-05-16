#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "conveyor_belt.h"
#include "util.h"

int g_running = 1;

void
sigint_handler(int sig) {
    g_running = 0;
}

int 
main(int argc, char * argv[]) {
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Pass cargo weight and (optionally) number of cargo units.\n");
        exit(EXIT_FAILURE);
    }

    int weight;
    if ((weight = read_natural(argv[1])) == -1) {
        fprintf(stderr, "First argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    int no_units = -1;
    if (argc == 3 && (no_units = read_natural(argv[2])) == -1) {
        fprintf(stderr, "Second argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    if (set_signal_handling(sigint_handler) == -1) {
        exit(EXIT_FAILURE);
    }

    if (open_belt() == -1) {
        exit(EXIT_FAILURE);
    }

    while (g_running && no_units != 0) {
        int res = enqueue(weight);
        if (res == -1) {
            continue;
        } else if (res == 1) {
            printf("Belt closed.\n");
            break;
        }

        no_units--;
    }

    if (close_belt() == -1) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
