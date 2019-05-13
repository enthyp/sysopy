#include <unistd.h>

#ifndef CONVEYOR_BELT_H
#define CONVEYOR_BELT_H

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                           (Linux-specific) */
};

typedef struct {
    pid_t loader_pid;
    long load_time;
    int weight;
} cargo_unit;

typedef struct {
    int head, tail;
    int current_units, current_weight;
    int max_units, max_weight;
} conveyor_belt;

int create(int max_units, int max_weight);

int open_belt();

int put(int weight);

cargo_unit take();

int lock(void);

int close_belt(void);

int delete();

#endif // CONVEYOR_BELT_H
