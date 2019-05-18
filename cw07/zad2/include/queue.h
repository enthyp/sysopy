#ifndef QUEUE_H
#define QUEUE_H

typedef struct {
    pid_t loader_pid;
    long load_time;
    int weight;
} cargo_unit;

int create_queue(int max_units, int max_weight);

int open_queue(void);

int enqueue(int weight);

int dequeue(cargo_unit * cargo, int available_weight, int max_weight);

int close_queue(void);

int delete_queue(void);

#endif // QUEUE_H
