#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include "conveyor_belt.h"
#include "util.h"

typedef enum {
    PENDING,
    LOADED
} cargo_state;

int g_running = 1;
int g_mode = 0;
int g_max_truck_load = -1;
int g_current_truck_load = 0;
int g_current_truck_count = 0;
cargo_unit * g_off_cargo = NULL;
cargo_state g_cargo_state = LOADED;

void sigint_handler(int);
int load_truck(void);
void handle_cargo(void);

void
sigint_handler(int sig) {
    printf(">>> SIGINT received!\n");
    if (lock() == -1) {
        exit(EXIT_FAILURE);
    }
    printf(">>> Belt locked.\n");

    g_running = 0;
    g_mode = 1;
}

int
load_truck(void) {
    g_current_truck_count = 0;
    g_current_truck_load = 0;
    if (release() == -1) {
        exit(EXIT_FAILURE);
    }
    printf(">>> Empty truck arrives.\n");

    do {
        if (g_cargo_state == LOADED) {
            if (g_mode == 0) {
                printf(">>> Awaiting load...\n");
            }
            int take_return = dequeue(g_off_cargo, g_mode);

            if (take_return == -1) {
                // Failure receiving the message - belt state may be corrupt.
                return -1;
            } else if (take_return == 1) {
                // No more messages after SIGINT.
                break;
            } else if (take_return == 2) {
                // Interrupted while waiting to acquire read lock.
                continue;
            }
        }

        if (g_off_cargo -> weight > g_max_truck_load) {
            // Belt was blocked by this cargo unit.
            fprintf(stderr, "Cargo unit blocked the belt.\n");
            return 0;
        }

        if (g_current_truck_load + g_off_cargo -> weight <= g_max_truck_load) {
            handle_cargo();
            if (g_current_truck_load == g_max_truck_load) {
                break;
            }
        } else {
            g_cargo_state = PENDING;
            break;
        }

        sleep(1);
    } while (1);

    if (lock() == -1) {
        exit(EXIT_FAILURE);
    }

    printf(">>> Truck departs.\n");
    return 0;
}

void
handle_cargo(void) {
    g_current_truck_load += g_off_cargo -> weight;
    g_current_truck_count++;
    g_cargo_state = LOADED;

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
    
    if (create(max_units, max_weight) == -1) {
        exit(EXIT_FAILURE);
    }

    g_off_cargo = malloc(sizeof(cargo_unit));
    if (g_off_cargo == NULL) {
        fprintf(stderr, "Failed to allocate cargo memory.\n");
        delete();
        exit(EXIT_FAILURE);
    }

    while (g_running) {
        if (load_truck() == -1) {
            delete();
            free(g_off_cargo);
            exit(EXIT_FAILURE);
        }
    }

    if (delete() == -1) {
        free(g_off_cargo);
        exit(EXIT_FAILURE);
    }

    free(g_off_cargo);
    exit(EXIT_SUCCESS);
}
