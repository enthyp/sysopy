#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/prctl.h>
#include "queue.h"
#include "util.h"

int g_sigint = 0;   // shared with queue.c, ONLY needed by trucker process

void
sigint_handler(int sig) {
    (close_queue() == -1) ? exit(EXIT_FAILURE) : exit(EXIT_SUCCESS);
}

int 
main(int argc, char * argv[]) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Pass cargo weight, max number of units on belt and (optionally) number of cargo units.\n");
        exit(EXIT_FAILURE);
    }

    int weight;
    if ((weight = read_natural(argv[1])) == -1) {
        fprintf(stderr, "First argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    int max_units;
    if ((max_units = read_natural(argv[2])) == -1) {
        fprintf(stderr, "Second argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    int no_units = -1;
    if (argc == 4 && (no_units = read_natural(argv[3])) == -1) {
        fprintf(stderr, "Third argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    if (set_signal_handling(sigint_handler) == -1) {
        exit(EXIT_FAILURE);
    }

    if (open_queue(max_units) == -1) {
        exit(EXIT_FAILURE);
    }

    prctl(PR_SET_PDEATHSIG, SIGINT);

    while (no_units != 0) {
        long cur_time = get_time();
        printf(">>> Awaiting belt access...\nPID: %d\nTime: %ld microsec\n", getpid(), cur_time);
        int res = enqueue(weight);

        if (res == -1) {
            fprintf(stderr, "Error occurred.\n");
            close_queue();
            exit(EXIT_FAILURE);
        } else if (res == 1) {
            fprintf(stderr, "Cargo unit would block the belt.\n");
            close_queue();
            exit(EXIT_FAILURE);
        }

        no_units--;
    }

    if (close_queue() == -1) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
