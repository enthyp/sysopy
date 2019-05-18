#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <signal.h>
#include "queue.h"
#include "util.h"

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};

typedef struct {
    pid_t producer_supervisor;
    int head, tail;
    int current_units, current_weight;
    int max_units, max_weight;
    int consumer_sleeping, producer_sleeping;
    int producer_weight;
} queue_state;

typedef struct {
    queue_state * state;
    cargo_unit * content;
    char * mutex_name[4];
    sem_t * mutex[4];
    int state_mem_seg, content_mem_seg;
} queue;

queue * g_queue = NULL;
extern int g_sigint;    // trucker.c
int g_locked = 0;
int g_empty = 1;
int g_sleep_empty = 0;

int delete_sem(queue *);

int
get_semaphores(queue * queue, int create) {
    int i, j;
    for (i = 0; i < 4; i++) {
        if (create) {
            queue -> mutex[i] = sem_open(queue -> mutex_name[i], O_CREAT | O_EXCL | O_RDWR, 0666, i < 2);
        } else {
            queue -> mutex[i] = sem_open(queue -> mutex_name[i], O_RDWR);
        }

        if (queue -> mutex[i] == SEM_FAILED) {
            int err = errno;
            fprintf(stderr, "Get mutex %d: %s\n", i, strerror(err));
            for (j = 0; j < i; j++) {
                sem_close(queue -> mutex[i]);
                sem_unlink(queue -> mutex_name[i]);
            }

            return -1;
        }
    }

    return 0;
}

int
get_shared_memory(queue * queue, int max_units, int create) {
    int flags = O_RDWR;
    if (create) {
        flags |= O_CREAT | O_EXCL;
    }

    if ((queue -> state_mem_seg = shm_open("/state", flags, 0666)) == -1) {
        perror("Get queue state shared memory");
        delete_sem(g_queue);
        return -1;
    } else {
        printf("Queue state shared memory ID: %d\n", queue -> state_mem_seg);
    }

    if ((queue -> content_mem_seg = shm_open("/content", flags, 0666)) == -1) {
        perror("Get queue content shared memory");
        shm_unlink("/state");
        delete_sem(g_queue);
        return -1;
    } else {
        printf("Queue content shared memory ID: %d\n", queue -> content_mem_seg);
    }

    if (create) {
        if (ftruncate(queue -> state_mem_seg, sizeof(queue_state)) == -1) {
            perror("Set queue state size");
            shm_unlink("/state");
            shm_unlink("/content");
            delete_sem(g_queue);
            return -1;
        }

        if (ftruncate(queue -> content_mem_seg, max_units * sizeof(cargo_unit)) == -1) {
            perror("Set queue content size");
            shm_unlink("/state");
            shm_unlink("/content");
            delete_sem(g_queue);
            return -1;
        }
    }

    if((queue -> state = (queue_state *)
            mmap(NULL, sizeof(queue_state), PROT_READ | PROT_WRITE, MAP_SHARED,
                    queue -> state_mem_seg, 0)) == MAP_FAILED) {
        perror("Map queue state shared memory");
        shm_unlink("/state");
        shm_unlink("/content");
        delete_sem(g_queue);
        return -1;
    }

    if((queue -> content = (cargo_unit *)
            mmap(NULL, max_units * sizeof(cargo_unit), PROT_READ | PROT_WRITE, MAP_SHARED,
                    queue -> content_mem_seg, 0)) == MAP_FAILED) {
        perror("Map queue content shared memory");
        shm_unlink("/state");
        shm_unlink("/content");
        delete_sem(g_queue);
        return -1;
    }

    close(queue -> state_mem_seg);
    close(queue -> content_mem_seg);

    return 0;
}

int
init_queue_state(queue * queue, int max_units, int max_weight) {
    queue_state state = {
            .producer_supervisor = (pid_t) 0,
            .head = 0,
            .tail = 0,
            .current_units = 0,
            .current_weight = 0,
            .max_units = max_units,
            .max_weight = max_weight,
            .consumer_sleeping = 0,
            .producer_sleeping = 0,
            .producer_weight = 0
    };
    memcpy(queue -> state, &state, sizeof(queue_state));
    return 0;

}

int
init_queue(queue ** queue_p) {
    *queue_p = (queue *) malloc(sizeof(queue));
    if (*queue_p == NULL) {
        fprintf(stderr, "Failed to allocate queue memory.\n");
        return -1;
    }

    queue q;
    q.mutex_name[0] = "/enq_mutex";
    q.mutex_name[1] = "/state_mutex";
    q.mutex_name[2] = "/cons_alarm";
    q.mutex_name[3] = "/prod_alarm";
    memcpy(*queue_p, &q, sizeof(queue));

    return 0;
}

int
create_queue(int max_units, int max_weight) {
    if (init_queue(&g_queue) == -1) {
        return -1;
    }

    if (get_semaphores(g_queue, 1) == -1) {
        return -1;
    }

    if (get_shared_memory(g_queue, max_units, 1) == -1) {
        delete_sem(g_queue);
        return -1;
    }

    if (init_queue_state(g_queue, max_units, max_weight) == -1) {
        delete_queue();
        return -1;
    }

    return 0;
}

int
open_queue(int max_units) {
    if (init_queue(&g_queue) == -1) {
        return -1;
    }

    if (get_semaphores(g_queue, 0) == -1) {
        return -1;
    }

    if (get_shared_memory(g_queue, max_units, 0) == -1) {
        return -1;
    }

    g_queue -> state -> producer_supervisor = getppid();

    return 0;
}

int
close_sem(queue * queue) {
    int i;
    for (i = 0; i < 4; i++) {
        if (sem_close(queue -> mutex[i]) == -1) {
            perror("Close sem");
            return -1;
        }
    }

    return 0;
}

int
close_queue(void) {
    if (close_sem(g_queue) == -1) {
        return -1;
    }

    if (munmap(g_queue -> content, g_queue -> state -> max_units * sizeof(cargo_unit)) == -1) {
        perror("Unmap queue content shared memory");
        return -1;
    }

    if (munmap(g_queue -> state, sizeof(queue_state)) == -1) {
        perror("Unmap queue state shared memory");
        return -1;
    }

    free(g_queue);
    return 0;
}

int
delete_sem(queue * queue) {
    int i;
    for (i = 0; i < 4; i++) {
        if (sem_unlink(queue -> mutex_name[i]) == -1) {
            perror("Unlink sem");
            return -1;
        }
    }

    return 0;
}

int
delete_queue(void) {
    pid_t supervisor = g_queue -> state -> producer_supervisor;

    if (kill(supervisor, SIGINT) == -1) {
        perror("Kill producer supervisor");
        if (!g_sleep_empty) {
            return -1;
        }
    }

    if (close_queue() == -1) {
        return -1;
    }

    if (delete_sem(g_queue) == -1) {
        return -1;
    }

    if (shm_unlink("/state") == -1) {
        perror("Unlink queue state shared memory");
        return -1;
    }

    if (shm_unlink("/content") == -1) {
        perror("Unlink queue content shared memory");
        return -1;
    }

    return 0;
}

int
lock(queue * queue, int mutex_id) {
    if (g_locked) {
        return 0;
    }

    if (sem_wait(queue -> mutex[mutex_id]) == -1) {
        int err = errno;
        if (err == EINTR) {
            return lock(queue, mutex_id);   // just don't SIGINT repeatedly, will you?
        }
        if (err == EINVAL || err == EIDRM) {
            close_queue();
            exit(EXIT_SUCCESS);
        }
        fprintf(stderr, "Lock semaphore %d: %s", mutex_id, strerror(err));
        return -1;
    }

    return 0;
}

int
release(queue * queue, int mutex_id) {
    if (g_locked) {
        return 0;
    }

    if (sem_post(queue -> mutex[mutex_id]) == -1) {
        int err = errno;
        if (err == EINTR) {
            return release(queue, mutex_id);
        }
        if (err == EINVAL || err == EIDRM) {
            close_queue();
            exit(EXIT_SUCCESS);
        }
        fprintf(stderr, "Release semaphore %d: %s", mutex_id, strerror(err));
        return -1;
    }

    return 0;
}

void
really_enqueue(queue * queue, int weight) {
    long load_time = get_time();
    pid_t loader_pid = getpid();
    cargo_unit cargo = {
            .loader_pid = loader_pid,
            .weight = weight,
            .load_time = load_time
    };

    queue_state state = *(queue -> state);
    memcpy(queue -> content + state.tail, &cargo, sizeof(cargo_unit));

    queue -> state -> tail = (state.tail + 1) % state.max_units;
    queue -> state -> current_weight += weight;
    queue -> state -> current_units += 1;

    printf(">>> ENQUEUED!\n\tPID: %d\nTime: %ld microsec\n\tWeight: %d\n\tCount: %d\n\n",
           loader_pid, load_time, queue -> state -> current_weight, queue -> state -> current_units);

    if (state.consumer_sleeping) {
        release(queue, 3);    // notify sleeping consumer
        queue -> state-> consumer_sleeping = 0;
    }
}

int
enqueue(int weight) {
    lock(g_queue, 0);
    lock(g_queue, 1);

    queue_state state = *(g_queue -> state);
    if (weight > state.max_weight) {
        release(g_queue, 1);
        release(g_queue, 0);
        return 1;   // Blocking cargo - loader must quit.
    } else if (state.current_units == state.max_units || weight > state.max_weight - state.current_weight) {
        g_queue -> state -> producer_sleeping = 1;
        g_queue -> state -> producer_weight = weight;
        release(g_queue, 1);

        lock(g_queue, 2);   // wait for consumer to up that semaphore

        lock(g_queue, 1);
        really_enqueue(g_queue, weight);
        release(g_queue, 1);
        release(g_queue, 0);
    } else {
        really_enqueue(g_queue, weight);
        release(g_queue, 1);
        release(g_queue, 0);
    }

    return 0;
}

int
peek(queue * queue) {
    return (queue -> content + queue -> state -> head) -> weight;
}

void
really_dequeue(queue * queue, cargo_unit * cargo) {
    long unload_time = get_time();
    queue_state state = *(queue -> state);
    memcpy(cargo, queue -> content + state.head, sizeof(cargo_unit));

    queue -> state -> head = (state.head + 1) % state.max_units;
    queue -> state -> current_weight -= cargo -> weight;
    queue -> state -> current_units -= 1;

    printf(">>> DEQUEUED!\n\tPID: %d\nTimediff: %ld microsec\n\tWeight: %d\n\tCount: %d\n\n",
           cargo -> loader_pid, unload_time - cargo -> load_time, queue -> state -> current_weight,
           queue -> state -> current_units);

    if (state.producer_sleeping) {
        state = *(queue -> state);
        if (state.max_weight - state.current_weight >= state.producer_weight) {
            release(queue, 2);    // notify sleeping producer
            queue -> state -> producer_sleeping = 0;
        }
    }
}

int
dequeue(cargo_unit * cargo, int available_weight, int max_weight) {
    lock(g_queue, 1);

    if (g_sigint) {
        g_locked = 1;
    }

    queue_state state = *(g_queue -> state);
    if (state.current_units > 0) {
        int head_weight = peek(g_queue);
        if (head_weight > max_weight) {
            release(g_queue, 1);
            return 1;
        } else if (head_weight > available_weight) {
            release(g_queue, 1);
            return 2;
        }

        really_dequeue(g_queue, cargo);
        release(g_queue, 1);
    } else {
        if (g_locked) {
            return 3;
        }

        g_queue -> state -> consumer_sleeping = 1;
        release(g_queue, 1);

        g_sleep_empty = 1;  // this is a hackaround for SIGINT to work after loaders finish
        lock(g_queue, 3);   // wait for producer to up that semaphore
        g_sleep_empty = 0;  // it's a race condition but won't occur unless SIGINT is received
        // in great sync with loaders finishing all operation (but SIGINT need
        // not be used in that case at all!)

        g_empty = 0;        // for SIGINT to work even before loaders start

        lock(g_queue, 1);
        really_dequeue(g_queue, cargo);
        release(g_queue, 1);
    }

    return 0;
}
