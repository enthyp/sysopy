#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "queue.h"
#include "util.h"

int g_max_truck_load = 0;
int g_current_truck_load = 0;
int g_current_truck_count = 0;
cargo_unit * g_off_cargo = NULL;

int load_truck(void);
void handle_cargo(void);

int g_sigint = 0;   // shared with queue.c
extern int g_empty; // shared with queue.c
extern int g_sleep_empty;   // shared with queue.c

void
sigint_handler(int sig) {
    g_sigint = 1;
    if (g_empty || g_sleep_empty) {
        delete_queue();
        exit(EXIT_SUCCESS);
    }
}

int
load_truck(void) {
    int done = 0;
    g_current_truck_count = 0;
    g_current_truck_load = 0;

    long cur_time = get_time();
    printf(">>> Empty truck arrives.\nTime: %ld microsec\n\n", cur_time);

    while(1) {
        cur_time = get_time();
        printf(">>> Awaiting load...\nTime: %ld microsec\n\n", cur_time);

        int take_return = dequeue(g_off_cargo, g_max_truck_load - g_current_truck_load, g_max_truck_load);
        if (take_return == -1) {
            fprintf(stderr, "ERROR\n");
            return -1;
        } else if (take_return == 1) {
            fprintf(stderr, "Cargo unit too heavy for the truck.\n");
            return -1;
        } else if (take_return == 2) {
            // Next cargo unit is too heavy to load - truck must depart.
            break;
        } else if (take_return == 3) {
            // No more cargo after SIGINT.
            done = 1;
            break;
        }

        handle_cargo();
    }

    cur_time = get_time();
    printf(">>> Truck departs.\nTime: %ld microsec\n\n", cur_time);

    if (done) {
        return 1;
    }

    return 0;
}

void
handle_cargo(void) {
    g_current_truck_load += g_off_cargo -> weight;
    g_current_truck_count++;

    long load_time = get_time();
    printf("Loading:\n\tPID: %d\n\tTime difference: %ld microseconds\n\tCargo count on truck: %d\n\t"
           "Loaded weight: %d\n", g_off_cargo -> loader_pid, load_time - g_off_cargo -> load_time,
           g_current_truck_count, g_current_truck_load);
}

int
main(int argc, char * argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Pass max truck load, max number of units on belt and max belt weight.\n");
        return -1;
    }

    if ((g_max_truck_load = read_natural(argv[1])) == -1) {
        fprintf(stderr, "First argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    int max_units = -1;
    if ((max_units = read_natural(argv[2])) == -1) {
        fprintf(stderr, "Second argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    int max_weight = -1;
    if ((max_weight = read_natural(argv[3])) == -1) {
        fprintf(stderr, "Third argument incorrect.\n");
        exit(EXIT_FAILURE);
    }

    if (set_signal_handling(sigint_handler) == -1) {
        exit(EXIT_FAILURE);
    }

    if (create_queue(max_units, max_weight) == -1) {
        exit(EXIT_FAILURE);
    }

    g_off_cargo = malloc(sizeof(cargo_unit));
    if (g_off_cargo == NULL) {
        fprintf(stderr, "Failed to allocate cargo memory.\n");
        delete_queue();
        exit(EXIT_FAILURE);
    }

    int res;
    while (1) {
        if ((res = load_truck()) != 0) {
            delete_queue();
            free(g_off_cargo);
            break;
        }
    }

    (res == 1) ? exit(EXIT_SUCCESS) : exit(EXIT_FAILURE);
}
