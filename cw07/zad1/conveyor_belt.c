#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
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
    int current_units, current_weight;
    int size;
} queue;

queue * g_queue = NULL;
cargo_unit * g_belt = NULL;

extern int g_mode;          // 0/1 - loader/trucker
extern int g_locked;        // whether trucker process holds the lock or not (loader ignores this one)
extern int g_after;         // whether trucker process received SIGINT or not (loader ignores this one)
int g_semaphores = -1;      // write_count, write_weight, read_count, access_count (effectively binary)
int g_queue_mem_seg = -1;
int g_belt_mem_seg = -1;

key_t
get_key(int id) {
    char * home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "No HOME variable to build key.\n");
        return (key_t) -1;
    }

    key_t result = ftok(home, id);
    if (result == (key_t) -1) {
        perror("Generate key");
        return (key_t) -1;
    }

    return result;
}

int
get_semaphores(int flags) {
    key_t sem_key = get_key(1);
    if (sem_key == (key_t) -1) {
        return -1;
    }

    if ((g_semaphores = semget(sem_key, 4, flags)) == -1) {
        perror("Get semaphore set ID");
        return -1;
    } else {
        printf("Semaphore set ID: %d\n", g_semaphores);
    }

    return 0;
}

int
create_shared_memory(int max_units, int flags) {
    key_t key = get_key(2);
    if (key == (key_t) -1) {
        return -1;
    }

    if ((g_queue_mem_seg = shmget(key, sizeof(queue), flags)) == -1) {
        perror("Create conveyor belt queue shared memory");
        semctl(g_semaphores, 0, IPC_RMID);
        return -1;
    } else {
        printf("Conveyor belt queue shared memory ID: %d\n", g_queue_mem_seg);
    }

    key = get_key(3);
    if (key == (key_t) -1) {
        return -1;
    }

    if ((g_belt_mem_seg = shmget(key, max_units * sizeof(cargo_unit), flags)) == -1) {
        perror("Create conveyor belt shared memory");
        shmdt(g_queue);
        shmctl(g_queue_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphores, 0, IPC_RMID);
        return -1;
    } else {
        printf("Conveyor belt shared memory ID: %d\n", g_belt_mem_seg);
    }

    if((g_queue = (queue *) shmat(g_queue_mem_seg, NULL, 0)) == (void *) -1) {
        perror("Attach to conveyor belt queue shared memory");
        shmctl(g_queue_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphores, 0, IPC_RMID);
        return -1;
    }

    if((g_belt = (cargo_unit *) shmat(g_belt_mem_seg, NULL, 0)) == (void *) -1) {
        perror("Attach to conveyor belt shared memory");
        shmdt(g_queue);
        shmctl(g_queue_mem_seg, IPC_RMID, NULL);
        shmctl(g_belt_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphores, 0, IPC_RMID);
        return -1;
    }

    return 0;
}

int
get_shared_memory(int flags) {
    key_t key = get_key(2);
    if (key == (key_t) -1) {
        return -1;
    }

    if ((g_queue_mem_seg = shmget(key, 0, flags)) == -1) {
        perror("Create conveyor belt queue shared memory");
        semctl(g_semaphores, 0, IPC_RMID);
        return -1;
    } else {
        printf("Conveyor belt queue shared memory ID: %d\n", g_queue_mem_seg);
    }

    key = get_key(3);
    if (key == (key_t) -1) {
        return -1;
    }

    if ((g_belt_mem_seg = shmget(key, 0, flags)) == -1) {
        perror("Create conveyor belt shared memory");
        shmdt(g_queue);
        shmctl(g_queue_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphores, 0, IPC_RMID);
        return -1;
    } else {
        printf("Conveyor belt shared memory ID: %d\n", g_belt_mem_seg);
    }

    if((g_queue = (queue *) shmat(g_queue_mem_seg, NULL, 0)) == (void *) -1) {
        perror("Attach to conveyor belt queue shared memory");
        shmctl(g_queue_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphores, 0, IPC_RMID);
        return -1;
    }

    if((g_belt = (cargo_unit *) shmat(g_belt_mem_seg, NULL, 0)) == (void *) -1) {
        perror("Attach to conveyor belt shared memory");
        shmdt(g_queue);
        shmctl(g_queue_mem_seg, IPC_RMID, NULL);
        shmctl(g_belt_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphores, 0, IPC_RMID);
        return -1;
    }

    return 0;
}

int
create(int max_units, int max_weight) {
    // Create conveyor belt access semaphores.
    if (get_semaphores(IPC_CREAT | IPC_EXCL | 0666) == -1) {
        return -1;
    }

    // Initialize conveyor belt in shared memory.
    if (create_shared_memory(max_units, IPC_CREAT | 0666) == -1) {
        return -1;
    }

    // Initialize queue.
    queue q = { .head = 0, .tail = 0, .current_units = 0, .current_weight = 0, .size = max_units };
    memcpy(g_queue, &q, sizeof(queue));

    // Initialize semaphores.
    union semun sem;
    unsigned short sem_arr[] = {max_units, max_weight, 0, 1};
    sem.array = sem_arr;

    if (semctl(g_semaphores, 0, SETALL, sem) == -1) {
        fprintf(stderr, "Failed to initialize semaphores.\n");
        shmdt(g_queue);
        shmdt(g_belt);
        shmctl(g_queue_mem_seg, IPC_RMID, NULL);
        shmctl(g_belt_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphores, 0, IPC_RMID);
        return -1;
    }

    return 0;
}

int
open_belt(void) {
    // Get conveyor belt access semaphores.
    if (get_semaphores(0) == -1) {
        return -1;
    }

    // Attach to conveyor belt shared memory.
    if (get_shared_memory(0) == -1) {
        return -1;
    }

    return 0;
}

int
write_lock(int weight) {
    struct sembuf sbs[2] = {
            { .sem_num = 0, .sem_op = -1, .sem_flg = 0 },
            { .sem_num = 1, .sem_op = -weight, .sem_flg = 0 }
    };

    if (semop(g_semaphores, sbs, 2) == -1) {
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
    struct sembuf sb = { .sem_num = 2, .sem_op = -1, .sem_flg = 0 };

    if (semop(g_semaphores, &sb, 1) == -1) {
        int err = errno;
        if (g_mode == 1) {
            if (g_after && (err == EINVAL || err == EIDRM)) {
                return 0;
            } else if (err == EINTR) {
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
    struct sembuf sb = { .sem_num = 3, .sem_op = -1, .sem_flg = 0 };

    if (semop(g_semaphores, &sb, 1) == -1) {
        int err = errno;
        if (g_mode == 0) {
            if (err == EINVAL || err == EIDRM) {
                return 1;
            }
        } else if (g_mode == 1) {
            if (g_after && (err == EINVAL || err == EIDRM)) {
                return 0;
            } else if (err == EINTR) {
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
write_release(int weight) {
    struct sembuf sbs[2] = {
            { .sem_num = 0, .sem_op = 1, .sem_flg = 0 },
            { .sem_num = 1, .sem_op = weight, .sem_flg = 0 }
    };

    if (semop(g_semaphores, sbs, 2) == -1) {
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
    struct sembuf sb = { .sem_num = 2, .sem_op = 1, .sem_flg = 0 };

    if (semop(g_semaphores, &sb, 1) == -1) {
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
    struct sembuf sb = { .sem_num = 3, .sem_op = 1, .sem_flg = 0 };

    if (semop(g_semaphores, &sb, 1) == -1) {
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
    if ((res = write_lock(weight)) == -1) {
        return -1;
    } else if (res == 1) {
        return 1;
    }

    if ((res = access_lock()) == -1) {
        write_release(weight);
        return -1;
    } else if (res == 1) {
        return 1;
    }


    // RANDOMIZATION!!!
    srand(time(NULL));
    double prob = 0.5 * pow(0.8, weight - 1); // 0.5 for weight 1, exponentially decreasing with weight increase
    if (rand() / (RAND_MAX + 1.0) < prob) {
        if (write_release(weight) == -1 || access_release() == -1) {
            return -1;
        }

        return 2;
    }
    // !!!

    long load_time = get_time();
    if (load_time == -1) {
        access_release();
        write_release(weight);
        return -1;
    }

    cargo_unit cargo = { .loader_pid = getpid(),
                         .weight = weight,
                         .load_time = load_time };
    memcpy(g_belt + g_queue -> tail, &cargo, sizeof(cargo_unit));
    g_queue -> tail = (g_queue -> tail + 1) % g_queue -> size;

    g_queue -> current_weight += weight;
    g_queue -> current_units += 1;

    printf(">>> Belt state after enqueue:\n\tWeight: %d\n\tCount: %d\n\n", g_queue -> current_weight, g_queue -> current_units);
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

    g_queue -> current_weight -= cargo -> weight;
    g_queue -> current_units -= 1;

    printf("Belt state after dequeue:\n\tWeight: %d\n\tCount: %d\n", g_queue -> current_weight, g_queue -> current_units);

    if (access_release() == -1) {
        write_release(cargo -> weight);
        return -1;
    }

    return write_release(cargo -> weight);
}

int
close_belt(void) {
    if (shmdt(g_queue) == -1) {
        perror("Detach conveyor belt queue shared memory");
        return -1;
    }

    if (shmdt(g_belt) == -1) {
        perror("Detach conveyor belt shared memory");
        return -1;
    }

    return 0;
}

int
delete_sem(void) {
    if (semctl(g_semaphores, 0, IPC_RMID) == -1) {
        perror("Delete semaphores");
        return -1;
    }

    return 0;
}

int
delete_mem(void) {
    if (shmdt(g_queue) == -1) {
        perror("Detach conveyor belt queue shared memory");
        return -1;
    }
    if (shmctl(g_queue_mem_seg, IPC_RMID, NULL) == -1) {
        perror("Delete conveyor belt queue shared memory");
        return -1;
    }

    if (shmdt(g_belt) == -1) {
        perror("Detach conveyor belt shared memory");
        return -1;
    }
    if (shmctl(g_belt_mem_seg, IPC_RMID, NULL) == -1) {
        perror("Delete conveyor belt shared memory");
        return -1;
    }

    return 0;
}
