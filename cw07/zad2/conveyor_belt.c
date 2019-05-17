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
#include <time.h> // for srand
#include <math.h> // for enqueue probability
#include "conveyor_belt.h"
#include "util.h"

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};

typedef struct {
    int head, tail;
    int current_units;
    int remaining_weight;
    int size;
} queue;

queue * g_queue = NULL;
cargo_unit * g_belt = NULL;

extern int g_mode;          // 0/1 - loader/trucker
extern int g_locked;        // whether trucker process holds the lock or not (loader ignores this one)
extern int g_after;         // whether trucker process received SIGINT or not
int g_sigint = 0;
sem_t * g_write_count = NULL;
sem_t * g_read_count = NULL;
sem_t * g_acc_mutex = NULL;
int g_queue_mem_seg = -1;
int g_belt_mem_seg = -1;

void
handle_sigint(void) {
    if (g_sigint == 0) {
        g_sigint = 1;
        close_sem();
        delete_sem();
    }
}

int
create_semaphores(int max_units, int max_weight) {
    g_write_count = sem_open("/write_count", O_CREAT | O_EXCL | O_RDWR, 0666, max_units);
    if (g_write_count == SEM_FAILED) {
        perror("Create write counter");
        return -1;
    }

    g_read_count = sem_open("/read_count", O_CREAT | O_EXCL | O_RDWR, 0666, 0);
    if (g_read_count == SEM_FAILED) {
        perror("Create read counter");
        sem_close(g_write_count);
        sem_unlink("/write_count");

        return -1;
    }

    g_acc_mutex = sem_open("/acc_mutex", O_CREAT | O_EXCL | O_RDWR, 0666, 1);
    if (g_acc_mutex == SEM_FAILED) {
        perror("Create mutex");
        sem_close(g_write_count);
        sem_unlink("/write_count");
        sem_close(g_read_count);
        sem_unlink("/read_count");
        return -1;
    }

    return 0;
}

int
get_semaphores(void) {
    g_write_count = sem_open("/write_count", O_RDWR);
    if (g_write_count == SEM_FAILED) {
        perror("Get write counter");
        return -1;
    }

    g_read_count = sem_open("/read_count", O_RDWR);
    if (g_read_count == SEM_FAILED) {
        perror("Get read counter");
        sem_close(g_write_count);
        return -1;
    }
    g_acc_mutex = sem_open("/acc_mutex", O_RDWR);
    if (g_acc_mutex == SEM_FAILED) {
        perror("Get mutex");
        sem_close(g_write_count);
        sem_close(g_read_count);
        return -1;
    }

    return 0;
}

int
create_shared_memory(int max_units, int flags) {
    if ((g_queue_mem_seg = shm_open("/int_queue", O_CREAT | O_RDWR, 0666)) == -1) {
        perror("Create conveyor belt queue shared memory");
        delete_sem();
        return -1;
    } else {
        printf("Conveyor belt queue shared memory ID: %d\n", g_queue_mem_seg);
    }

    if ((g_belt_mem_seg = shm_open("/int_belt", O_CREAT | O_RDWR, 0666)) == -1) {
        perror("Create conveyor belt shared memory");
        shm_unlink("/int_queue");
        delete_sem();
        return -1;
    } else {
        printf("Conveyor belt shared memory ID: %d\n", g_belt_mem_seg);
    }

    if (ftruncate(g_queue_mem_seg, sizeof(queue)) == -1) {
        perror("Set conveyor belt queue size");
        // TODO: rest?
        return -1;
    }

    if (ftruncate(g_belt_mem_seg, max_units * sizeof(cargo_unit)) == -1) {
        perror("Set conveyor belt size");
        // TODO: rest?
        return -1;
    }

    if((g_queue =
            (queue *) mmap(NULL, sizeof(queue), PROT_READ | PROT_WRITE, MAP_SHARED, g_queue_mem_seg, 0)) == MAP_FAILED) {
        perror("Map conveyor belt queue shared memory");
        // TODO: rest?
        return -1;
    }

    if((g_belt = (cargo_unit *) mmap(NULL, max_units * sizeof(cargo_unit), PROT_READ | PROT_WRITE, MAP_SHARED,
            g_belt_mem_seg, 0)) == MAP_FAILED) {
        perror("Map conveyor belt shared memory");
        // TODO: rest?
        return -1;
    }

    close(g_belt_mem_seg);
    close(g_queue_mem_seg);

    return 0;
}

int
get_shared_memory(int max_units) {
    if ((g_queue_mem_seg = shm_open("/int_queue", O_RDWR, 0666)) == -1) {
        perror("Get conveyor belt queue shared memory");
        delete_sem();
        return -1;
    } else {
        printf("Conveyor belt queue shared memory ID: %d\n", g_queue_mem_seg);
    }

    if ((g_belt_mem_seg = shm_open("/int_belt", O_RDWR, 0666)) == -1) {
        perror("Get conveyor belt shared memory");
        shm_unlink("/int_queue");
        delete_sem();
        return -1;
    } else {
        printf("Conveyor belt shared memory ID: %d\n", g_belt_mem_seg);
    }

    if((g_queue =
                (queue *) mmap(NULL, sizeof(queue), PROT_READ | PROT_WRITE, MAP_SHARED, g_queue_mem_seg, 0)) == MAP_FAILED) {
        perror("Map conveyor belt queue shared memory");
        // TODO: rest?
        return -1;
    }

    if((g_belt = (cargo_unit *) mmap(NULL, max_units * sizeof(cargo_unit), PROT_READ | PROT_WRITE, MAP_SHARED,
                                     g_belt_mem_seg, 0)) == MAP_FAILED) {
        perror("Map conveyor belt shared memory");
        // TODO: rest?
        return -1;
    }

    close(g_belt_mem_seg);
    close(g_queue_mem_seg);

    return 0;
}

int
create(int max_units, int max_weight) {
    // Create conveyor belt access semaphores.
    if (create_semaphores(max_units, max_weight) == -1) {
        return -1;
    }

    // Initialize conveyor belt in shared memory.
    if (create_shared_memory(max_units, max_weight) == -1) {
        return -1;
    }

    // Initialize queue.
    queue q = { .head = 0, .tail = 0, .current_units = 0, .remaining_weight = max_weight, .size = max_units };
    memcpy(g_queue, &q, sizeof(queue));

    return 0;
}

int
open_belt(int max_units) {
    // Get conveyor belt access semaphores.
    if (get_semaphores() == -1) {
        return -1;
    }

    // Attach to conveyor belt shared memory.
    if (get_shared_memory(max_units) == -1) {
        return -1;
    }

    return 0;
}

int
write_lock(void) {
    if (!g_mode && g_sigint) {
        return 1;
    }
    if (sem_wait(g_write_count) == -1) {
        int err = errno;
        if (g_mode == 0) {
            if (err == EINVAL || errno == EIDRM) {
                return 1;
            }
        }
        perror("Lock write semaphore");
        return -1;
    }

    return 0;
}

int
read_lock(void) {
    if (g_mode && g_after) {
        return 0;
    }

    if (sem_wait(g_read_count) == -1) {
        int err = errno;
        if (g_mode == 1) {
            if (g_after && (err == EINVAL || err == EIDRM)) {
                return 0;
            } else if (err == EINTR) {
                handle_sigint();
                return 1;
            }
        }

        perror("Lock read semaphore");
        return -1;
    }

    return 0;
}

int
access_lock(void) {
    if (!g_mode && g_sigint) {
        return 1;
    }
    if (g_after && g_mode) {
        return 0;
    }
    if (sem_wait(g_acc_mutex) == -1) {
        int err = errno;
        if (g_mode == 0) {
            if (err == EINVAL || err == EIDRM) {
                return 1;
            }
        } else if (g_mode == 1) {
            if (g_after && (err == EINVAL || err == EIDRM)) {
                return 0;
            } else if (err == EINTR) {
                handle_sigint();
                return 1;
            }
        }

        perror("Lock access semaphore");
        return -1;
    } else if (g_mode == 1) {
        g_locked = 1;
    }

    return 0;
}

int
write_release(void) {
    if (sem_post(g_write_count) == -1) {
        if (g_mode == 1) {
            if (g_after == 1) {
                if (errno == EINVAL || errno == EIDRM || errno == EINTR) {
                    return 0;
                }
            }
        }
        perror("Release write semaphore");
        return -1;
    }

    return 0;
}

int
read_release(void) {
    if (sem_post(g_read_count) == -1) {
        if (g_mode == 0) {
            if (errno == EINVAL || errno == EIDRM) {
                return 1;
            }
        } else if (g_mode == 1) {
            if (errno == EINVAL || errno == EIDRM) {
                return 0;
            }
        }
        perror("Release read semaphore");
        return -1;
    }

    return 0;
}

int
access_release(void) {
    if (sem_post(g_acc_mutex) == -1) {
        if (g_mode == 1) {
            if (g_after == 1) {
                if (errno == EINVAL || errno == EIDRM || errno == EINTR) {
                    return 0;
                }
            }
        } else if (g_mode == 0) {
            if (errno == EINVAL || errno == EIDRM) {
                return 1;
            }
        }

        perror("Release access semaphore");
        return -1;
    } else if (g_mode == 1) {
        g_locked = 0;
    }

    return 0;
}

int
enqueue(int weight) {
    int res;
    if ((res = write_lock()) == -1) {
        return -1;
    } else if (res == 1) {
        return 1;
    }

    if ((res = access_lock()) == -1) {
        write_release();
        return -1;
    } else if (res == 1) {
        return 1;
    }

    // RANDOMIZATION!!!
    srand(time(NULL));
    double prob = 0.5 * pow(0.8, weight - 1); // 0.5 for weight 1, exponentially decreasing with weight increase
    if (rand() / (RAND_MAX + 1.0) < prob) {
        if (write_release() == -1 || access_release() == -1) {
            return -1;
        }

        return 2;
    }
    // !!!

    if (g_queue -> remaining_weight < weight) {
        if (access_release() == -1 || write_release() == -1) {
            return -1;
        }

        return 2;
    }

    long load_time = get_time();
    if (load_time == -1) {
        access_release();
        write_release();
        return -1;
    }

    cargo_unit cargo = { .loader_pid = getpid(),
                         .weight = weight,
                         .load_time = load_time };
    memcpy(g_belt + g_queue -> tail, &cargo, sizeof(cargo_unit));
    g_queue -> tail = (g_queue -> tail + 1) % g_queue -> size;

    g_queue -> remaining_weight -= weight;
    g_queue -> current_units += 1;

    printf(">>> Belt state after enqueue:\n\tWeight remaining: %d\n\tCount: %d\n\n", g_queue -> remaining_weight,
            g_queue -> current_units);
    printf("PID: %d\nTime: %ld microsec\n", getpid(), load_time);

    if ((res = access_release()) == -1) {
        read_release();
        return -1;
    } else if (res == 1) {
        return 1;
    }

    return read_release();
}

int
dequeue(cargo_unit * cargo, int max_weight, int blocking_weight) {
    int res;

    if ((res = read_lock()) == -1) {
        return -1;
    } else if (res == 1) {
        return 1;
    }

    if ((res = access_lock()) == -1) {
        read_release();
        return -1;
    } else if (res == 1) {
        read_release();
        return 1;
    }

    if (g_after && g_queue -> current_units == 0) {
        return 2;
    }

    if ((g_belt + g_queue -> head) -> weight > blocking_weight) {
        if (access_release() == -1 || read_release() == -1) {
            return -1;
        }

        return 3;
    } else if ((g_belt + g_queue -> head) -> weight > max_weight) {
        if (access_release() == -1 || read_release() == -1) {
            return -1;
        }

        return 4;
    }

    memcpy(cargo, g_belt + g_queue -> head, sizeof(cargo_unit));
    g_queue -> head = (g_queue -> head + 1) % g_queue -> size;

    g_queue -> remaining_weight += cargo -> weight;
    g_queue -> current_units -= 1;

    printf("Belt state after dequeue:\n\tWeight remaining: %d\n\tCount: %d\n", g_queue -> remaining_weight,
            g_queue -> current_units);

    if (access_release() == -1) {
        write_release();
        return -1;
    }

    return write_release();
}

int
close_belt(void) {
    if (close_sem() == -1) {
        return -1;
    }

    if (munmap(g_belt, g_queue -> size * sizeof(cargo_unit)) == -1) {
        perror("Unmap conveyor belt shared memory");
        return -1;
    }

    if (munmap(g_queue, sizeof(queue)) == -1) {
        perror("Unmap conveyor belt queue shared memory");
        return -1;
    }

    return 0;
}

int
close_sem(void) {
    if (sem_close(g_write_count) == -1) {
        perror("Close sem");
        return -1;
    }

    if (sem_close(g_read_count) == -1) {
        perror("Close sem");
        return -1;
    }

    if (sem_close(g_acc_mutex) == -1) {
        perror("Close sem");
        return -1;
    }

    return 0;
}

int
delete_sem(void) {
    if (sem_unlink("/write_count") == -1) {
        perror("Unlink sem");
        return -1;
    }

    if (sem_unlink("/read_count") == -1) {
        perror("Unlink sem");
        return -1;
    }

    if (sem_unlink("/acc_mutex") == -1) {
        perror("Unlink sem");
        return -1;
    }

    return 0;
}

int
delete_mem(void) {
    if (munmap(g_belt, g_queue -> size * sizeof(cargo_unit)) == -1) {
        perror("Unmap conveyor belt shared memory");
        return -1;
    }

    if (munmap(g_queue, sizeof(queue)) == -1) {
        perror("Unmap conveyor belt queue shared memory");
        return -1;
    }

    if (shm_unlink("/int_queue") == -1) {
        perror("Unlink conveyor belt queue shared memory");
        return -1;
    }

    if (shm_unlink("/int_belt") == -1) {
        perror("Unlink conveyor belt shared memory");
        return -1;
    }

    return 0;
}
