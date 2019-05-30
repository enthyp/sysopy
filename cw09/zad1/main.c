#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>


typedef struct {
    int id;
} thread_id;

typedef struct {
    sem_t exec_sem;
    sem_t get_off_sem;

    int start;
    pthread_mutex_t start_mutex;
    pthread_cond_t start_cond;

    int passenger_count;
    pthread_mutex_t pc_mutex;
    pthread_cond_t pc_cond;
} train_state;

int g_current_train_id = 0;
train_state * g_trains = NULL;
sem_t get_on_sem;

int g_num_trains, g_train_capacity, g_train_rounds;


long
get_time(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        perror("Get current time");
        return -1;
    }

    return (long)(ts.tv_sec * 1.0e3 + ts.tv_nsec * 1.0e-6);
}


void *
train(void * arg) {

    return arg;
}

void *
passenger(void * arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); // TODO: maybe change



    return arg;
}

int
main(int argc, char * argv[]) {
    // Read arguments.

    if (argc != 5) {
        fprintf(stderr, "Pass number of passenger threads, number of train threads, "
                        "number of passengers per train and number of train rounds.\n");
        exit(EXIT_FAILURE);
    }
    int num_passengers;
    if (sscanf(argv[1], "%d", &num_passengers) < 1 || num_passengers <= 0) {
        fprintf(stderr, "First argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(argv[2], "%d", &g_num_trains) < 1 || g_num_trains <= 0) {
        fprintf(stderr, "Second argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(argv[3], "%d", &g_train_capacity) < 1 || g_train_capacity <= 0) {
        fprintf(stderr, "Third argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(argv[4], "%d", &g_train_rounds) < 1 || g_train_rounds <= 0) {
        fprintf(stderr, "Fourth argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    // Setup track state.

    if (sem_init(&get_on_sem, 0, 0) == -1) {
        exit(EXIT_FAILURE);
    }

    g_trains = (train_state *) malloc(g_num_trains * sizeof(train_state));
    if (g_trains == NULL) {
        exit(EXIT_FAILURE);
    }

    int i;
    for (i = 0; i < g_num_trains; i++) {
        if (sem_init(&(g_trains[i].exec_sem), 0, i == 0) == -1) {
            exit(EXIT_FAILURE);
        }
        if (sem_init(&(g_trains[i].get_off_sem), 0, 1) == -1) {
            exit(EXIT_FAILURE);
        }
        g_trains[i].start = 0;
        pthread_cond_init(&(g_trains[i].start_cond), NULL);
        pthread_mutex_init(&(g_trains[i].start_mutex), NULL);

        g_trains[i].passenger_count = 0;
        pthread_cond_init(&(g_trains[i].pc_cond), NULL);
        pthread_mutex_init(&(g_trains[i].pc_mutex), NULL);
    }

    pthread_t * thread_ids = (pthread_t *) malloc((g_num_trains + num_passengers) * sizeof(pthread_t));
    if (thread_ids == NULL) {
        exit(EXIT_FAILURE);
    }

    thread_id * args = (thread_id *) malloc((g_num_trains + num_passengers) * sizeof(thread_id));
    if (args == NULL) {
        exit(EXIT_FAILURE);
    }

    // Run and wait for threads to finish.

    for (i = 0; i < g_num_trains + num_passengers; i++) {
        args[i].id = i % g_num_trains;
        pthread_create(thread_ids + i, NULL, i < g_num_trains ? train : passenger, args + i);
    }

    for (i = 0; i < g_num_trains + num_passengers; i++) {
        void * ret;
        pthread_join(thread_ids[i], &ret);
    }

    // TODO: Clean up resources.

    sem_destroy(&get_on_sem);
    for (i = 0; i < g_num_trains; i++) {
        sem_destroy(&(g_trains[i].exec_sem));
        sem_destroy(&(g_trains[i].get_off_sem));
    }

    free(thread_ids);
    free(args);
    free(g_trains);

    printf("DONE!\n");
    return 0;
}