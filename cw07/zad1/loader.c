#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "conveyor_belt.h"
#include "util.h"

int g_mode = 0; // loader mode
int g_running = 1;
int g_locked = 0; // used by conveyor_belt.c in trucker process only!
int g_after = 0; // used by conveyor_belt.c in trucker process only!

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
        long cur_time = get_time();
        if (cur_time == -1) {
            exit(EXIT_FAILURE);
        }
        printf(">>> Awaiting belt access...\nPID: %d\nTime: %ld microsec\n", getpid(), cur_time);

        int res = enqueue(weight);
        if (res == -1) {
            fprintf(stderr, "Error occurred.\n");
            break;
        } else if (res == 1) {
            printf("Belt closed.\n");
            break;
        } else if (res == 2) {
            // Roll dice -> enqueue failed
            continue;
        }

        no_units--;
    }

    if (close_belt() == -1) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
