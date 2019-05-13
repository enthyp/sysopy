#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <string.h>
#include "include/conveyor_belt.h"

conveyor_belt * g_belt = NULL;
cargo_unit * g_array = NULL;
int g_semaphore = -1;
int g_interface_mem_seg = -1;
int g_content_mem_seg = -1;

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
create(int max_units, int max_weight) {
    // Initialize conveyor belt access semaphore.
    key_t sem_key = get_key(1);
    if (sem_key == (key_t) -1) {
        return -1;
    }

    if ((g_semaphore = semget(sem_key, 1, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        perror("Get semaphore ID");
        return -1;
    } else {
        printf("Semaphore ID: %d\n", g_semaphore);
    }

    union semun sem;
    sem.val = 0;

    if (semctl(g_semaphore, 0, SETVAL, sem) == -1) {
        fprintf(stderr, "Failed to initialize semaphore.\n");
        return -1;
    }

    // Initialize conveyor belt in shared memory.
    key_t interface_key = get_key(2);
    if (interface_key == (key_t) -1) {
        return -1;
    }

    if ((g_interface_mem_seg = shmget(interface_key, sizeof(conveyor_belt), IPC_CREAT | 0666)) == -1) {
        perror("Create conveyor belt interface shared memory");
        semctl(g_semaphore, 0, IPC_RMID);
        return -1;
    } else {
        printf("Interface shared memory ID: %d\n", g_interface_mem_seg);
    }

    key_t content_key = get_key(3);
    if (content_key == (key_t) -1) {
        return -1;
    }

    if ((g_content_mem_seg = shmget(content_key, max_units * sizeof(cargo_unit), IPC_CREAT | 0666)) == -1) {
        perror("Create conveyor belt contents shared memory");
        shmctl(g_interface_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphore, 0, IPC_RMID);
        return -1;
    } else {
        printf("Contents shared memory ID: %d\n", g_content_mem_seg);
    }

    if((g_belt = (conveyor_belt *) shmat(g_interface_mem_seg, NULL, 0)) == (void *) -1) {
        perror("Attach to interface shared memory");
        shmctl(g_content_mem_seg, IPC_RMID, NULL);
        shmctl(g_interface_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphore, 0, IPC_RMID);
        return -1;
    }

    conveyor_belt belt = { .head = 0,
                           .tail = 0,
                           .current_units = 0,
                           .current_weight = 0,
                           .max_units = max_units,
                           .max_weight = max_weight };

    if((g_array = (cargo_unit *) shmat(g_content_mem_seg, NULL, 0)) == (void *) -1) {
        perror("Attach to content shared memory");
        shmdt(g_belt);
        shmctl(g_content_mem_seg, IPC_RMID, NULL);
        shmctl(g_interface_mem_seg, IPC_RMID, NULL);
        semctl(g_semaphore, 0, IPC_RMID);
        return -1;
    }

    memcpy(g_belt, &belt, sizeof(conveyor_belt));

    sem.val = 1;
    if (semctl(g_semaphore, 0, SETVAL, sem) == -1) {
        fprintf(stderr, "Failed to release semaphore.\n");
        return -1;
    }

    return 0;
}

int
open_belt(void) {
    key_t sem_key = get_key(1);
    if (sem_key == (key_t) -1) {
        return -1;
    }

    if ((g_semaphore = semget(sem_key, 1, 0)) == -1) {
        perror("Get semaphore ID");
        return -1;
    } else {
        printf("Semaphore ID: %d\n", g_semaphore);
    }

    key_t interface_key = get_key(2);
    if (interface_key == (key_t) -1) {
        return -1;
    }

    if ((g_interface_mem_seg = shmget(interface_key, 0, 0)) == -1) {
        perror("Get conveyor belt interface shared memory");
        return -1;
    } else {
        printf("Interface shared memory ID: %d\n", g_interface_mem_seg);
    }

    key_t content_key = get_key(3);
    if (content_key == (key_t) -1) {
        return -1;
    }

    if ((g_content_mem_seg = shmget(content_key, 0, 0)) == -1) {
        perror("Get conveyor belt contents shared memory");
        shmctl(g_interface_mem_seg, IPC_RMID, NULL);
        return -1;
    } else {
        printf("Contents shared memory ID: %d\n", g_content_mem_seg);
    }

    if((g_belt = (conveyor_belt *) shmat(g_interface_mem_seg, NULL, 0)) == (void *) -1) {
        perror("Attach to interface shared memory");
        shmctl(g_content_mem_seg, IPC_RMID, NULL);
        shmctl(g_interface_mem_seg, IPC_RMID, NULL);
        return -1;
    }

    if((g_array = shmat(g_content_mem_seg, NULL, 0)) == (void *) -1) {
        perror("Attach to content shared memory");
        shmdt(g_belt);
        shmctl(g_content_mem_seg, IPC_RMID, NULL);
        shmctl(g_interface_mem_seg, IPC_RMID, NULL);
        return -1;
    }

    return 0;
}

int
put(int weight);

cargo_unit
take(void);

int
release(void) {
    struct sembuf sb = {
            .sem_num = 0,
            .sem_op = 1,
            .sem_flg = 0 };

    if (semop(g_semaphore, &sb, 1) == -1) {
        perror("Increment semaphore");
        return -1;
    }

    return 0;
}

int
lock(void) {
    struct sembuf sb = {
            .sem_num = 0,
            .sem_op = -1,
            .sem_flg = SEM_UNDO };

    if (semop(g_semaphore, &sb, 1) == -1) {
        perror("Decrement semaphore");
        return -1;
    }

    return 0;
}

int
close_belt(void) {
    if (shmdt(g_array) == -1) {
        perror("Detach conveyor belt content shared memory");
        return -1;
    }
    if (shmdt(g_belt) == -1) {
        perror("Detach conveyor belt interface shared memory");
        return -1;
    }

    return 0;
}

int
delete(void) {
    if (semctl(g_semaphore, 0, IPC_RMID) == -1) {
        perror("Delete semaphore");
        return -1;
    }
    if (shmdt(g_array) == -1) {
        perror("Detach conveyor belt content shared memory");
        return -1;
    }
    if (shmdt(g_belt) == -1) {
        perror("Detach conveyor belt interface shared memory");
        return -1;
    }
    if (shmctl(g_content_mem_seg, IPC_RMID, NULL) == -1) {
        perror("Delete conveyor belt content shared memory");
        return -1;
    }
    if (shmctl(g_interface_mem_seg, IPC_RMID, NULL) == -1) {
        perror("Delete conveyor belt interface shared memory");
        return -1;
    }

    return 0;
}
