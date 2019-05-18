#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <sys/wait.h>
#include "util.h"

int
main(int argc, char * argv[]) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Pass number of loaders, max cargo weight and (optionally) number of cargo units.\n");
        exit(EXIT_FAILURE);
    }

    int num_loaders;
    if ((num_loaders = read_natural(argv[1])) == -1) {
        fprintf(stderr, "First argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    int max_weight;
    if ((max_weight = read_natural(argv[2])) == -1) {
        fprintf(stderr, "Second argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    int no_units = -1;
    if (argc == 4 && (no_units = read_natural(argv[3])) == -1) {
        fprintf(stderr, "Third argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    char arg1[8];
    char arg2[8];

    int i;
    for (i = 0; i < num_loaders; i++) {
        int weight = (int) ((rand() / (RAND_MAX + 1.0)) * max_weight) + 1;
        pid_t pid;
        if ((pid = fork()) == -1) {
            fprintf(stderr, "Fork failed.\n");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            if (argc == 3) {
                sprintf(arg1, "%d", weight);
                execl("loader", "loader", arg1, NULL);
                perror("Launch");
                exit(EXIT_FAILURE);
            } else {
                sprintf(arg1, "%d", weight);
                sprintf(arg2, "%d", no_units);
                execl("loader", "loader", arg1, arg2, NULL);
                perror("Launch");
                exit(EXIT_FAILURE);
            }
        } else {
            printf("Launched.\n");
        }
    }

    for (i = 0; i < num_loaders; i++) {
        wait(NULL);
    }

    return 0;
}