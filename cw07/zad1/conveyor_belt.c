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

int g_semaphores = -1; // WRITE (count, weight) READ: (count) ACCESS (count)
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
        if (errno == EINVAL) {
            return 1;
        }
        perror("Lock write semaphore");
        return -1;
    }

    return 0;
}

int
read_lock(int flag) {
    struct sembuf sb = { .sem_num = 2, .sem_op = -1, .sem_flg = flag };

    if (semop(g_semaphores, &sb, 1) == -1) {
        if (errno == EAGAIN) {
            return 1;
        } else if (errno == EINTR) {
            return 2;
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
        if (errno == EINVAL) {
            return 1;
        } else if (errno == EINTR) {
            return 2;
        }
        perror("Lock access semaphore");
        return -1;
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
        perror("Release write semaphore");
        return -1;
    }

    return 0;
}

int
read_release(void) {
    struct sembuf sb = { .sem_num = 2, .sem_op = 1, .sem_flg = 0 };

    if (semop(g_semaphores, &sb, 1) == -1) {
        perror("Release read semaphore");
        return -1;
    }

    return 0;
}

int
access_release(void) {
    struct sembuf sb = { .sem_num = 3, .sem_op = 1, .sem_flg = 0 };

    if (semop(g_semaphores, &sb, 1) == -1) {
        perror("Release access semaphore");
        return -1;
    }

    return 0;
}

int
lock(void) {
    union semun sem;
    sem.val = 0;

    if (semctl(g_semaphores, 0, SETVAL, sem) == -1) {
        perror("Lock for writing");
        return -1;
    }

    return 0;
}

int
release(void) {
    union semun sem;
    sem.val = g_queue -> size - g_queue -> current_units;

    if (semctl(g_semaphores, 0, SETVAL, sem) == -1) {
        perror("Release for writing");
        return -1;
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
        write_release(weight);
        return 1;
    }

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
    printf("Belt state after enqueue:\n\tWeight: %d\n\tCount: %d\n", g_queue -> current_weight, g_queue -> current_units);

    if ((res = access_release()) == -1) {
        read_release();
        return -1;
    } else if (res == 1) {
        return 1;
    }

    return read_release();
}

int
dequeue(cargo_unit * cargo, int immediate) {
    int res;
    int flag = immediate ? IPC_NOWAIT : 0;

    if ((res = read_lock(flag)) == -1) {
        return -1;
    } else if (res == 1) {
        return 1;
    } else if (res == 2) {
        return 2;
    }

    if ((res = access_lock()) == -1) {
        read_release();
        return -1;
    } else if (res == 1) {
        read_release();
        return 1;
    } else if (res == 2) {
        read_release();
        return 2;
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
delete(void) {
    if (semctl(g_semaphores, 0, IPC_RMID) == -1) {
        perror("Delete semaphores");
        return -1;
    }

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
