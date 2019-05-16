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

int dequeue(cargo_unit * cargo, int immediate);

int lock(void);

int release(void);

int close_belt(void);

int delete(void);

#endif // CONVEYOR_BELT_H
