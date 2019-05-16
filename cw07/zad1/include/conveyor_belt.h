#ifndef CONVEYOR_BELT_H
#define CONVEYOR_BELT_H

typedef struct {
    pid_t loader_pid;
    long load_time;
    int weight;
} cargo_unit;

int create(int max_units, int max_weight);

int open_belt(void);

int enqueue(int weight);

int dequeue(cargo_unit * cargo);

int access_lock(void);

int access_release(void);

int close_belt(void);

int delete_sem(void);

int delete_mem(void);

#endif // CONVEYOR_BELT_H
