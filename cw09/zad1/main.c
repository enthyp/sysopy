#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>


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

int g_num_passengers, g_num_trains, g_train_capacity, g_train_rounds;
pthread_t * g_thread_ids = NULL;

int g_stop = 0;

char * g_timestamp = NULL;


void
set_timestamp(char * timestamp) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("Get current time");
        return;
    }

    struct tm * t = localtime(&(ts.tv_sec));
    strftime(timestamp, 9, "%H:%M:%S", t);

    int millis = (int) ts.tv_nsec * 1.0e-6;
    char ms[4];
    snprintf(ms, 5, ".%.3d", millis);
    strcat(timestamp, ms);
}


void *
train(void * arg) {
    char timestamp[13];
    thread_id * inp = (thread_id *) arg;
    int id = inp -> id;

    train_state * state = g_trains + id;

    int ride_count = 0;
    while (1) {
        sem_wait(&(state->exec_sem));
        g_current_train_id = id;

        // Let all passengers depart.
        pthread_mutex_lock(&(state->pc_mutex));

        set_timestamp(timestamp);
        printf("TRAIN %d <%s>: doors opening!\n", id, timestamp);

        if (state->passenger_count > 0) {
            sem_post(&(state->get_off_sem));
        }

        while (state->passenger_count > 0) {
            pthread_cond_wait(&(state->pc_cond), &(state->pc_mutex));
        }

        pthread_mutex_unlock(&(state->pc_mutex));

        // Check if it's all over.
        if (ride_count == g_train_rounds) {
            if (id != g_num_trains - 1) {
                // Just let the next one proceed.
                int next_id = (id + 1) % g_num_trains;
                sem_post(&(g_trains[next_id].exec_sem));
            }

            break;
        }

        // Reset the start button.
        pthread_mutex_lock(&(state -> start_mutex));
        state -> start = 0;

        // Let passengers on board and wait till train fills up.
        sem_wait(&(state -> get_off_sem));
        sem_post(&get_on_sem);

        pthread_mutex_lock(&(state -> pc_mutex));
        if (state -> passenger_count < g_train_capacity) {
            pthread_cond_wait(&(state->pc_cond), &(state->pc_mutex));
        }
        pthread_mutex_unlock(&(state -> pc_mutex));

        set_timestamp(timestamp);
        printf("TRAIN %d <%s>: doors closing!\n", id, timestamp);

        // Let passengers start it.

        pthread_mutex_unlock(&(state -> start_mutex));
        if (state -> start == 0) {
            pthread_cond_wait(&(state->start_cond), &(state->start_mutex));
        }
        pthread_mutex_unlock(&(state -> start_mutex));

        // Once it has started (passengers wait for get_off_sem) -
        // let the next train proceed.

        set_timestamp(timestamp);
        printf("TRAIN %d <%s>: starting the ride!!!\n\n\n", id, timestamp);

        int next_id = (id + 1) % g_num_trains;
        sem_post(&(g_trains[next_id].exec_sem));
        ride_count++;

        // And wait for the next turn (from the beginning).

        int dur = 1000 + (rand() / (RAND_MAX + 1.0)) * 9000;
        usleep(dur);
    }

    set_timestamp(timestamp);
    printf("TRAIN %d <%s>: shutdown.\n", id, timestamp);

    if (id == g_num_trains -1) {
        g_stop = 1;
        sem_post(&get_on_sem);
    }

    return arg;
}


void *
passenger(void * arg) {
    char timestamp[13];

    //signal(SIGINT, sigint_handler);
    thread_id * inp = (thread_id *) arg;
    int id = inp -> id;

    while (1) {
        // Wait till train opens for newcomers.
        sem_wait(&get_on_sem);
        if (g_stop) {
            sem_post(&get_on_sem);
            break;
        }
        train_state * state = g_trains + g_current_train_id;
        pthread_mutex_lock(&(state->pc_mutex));
        state -> passenger_count += 1;

        set_timestamp(timestamp);
        printf("PASSENGER %d <%s>: gets on the train. Passengers now aboard: %d\n",
                id, timestamp, state -> passenger_count);

        if (state -> passenger_count < g_train_capacity) {
            sem_post(&get_on_sem);
        } else {
            pthread_cond_broadcast(&(state -> pc_cond));
        }
        pthread_mutex_unlock(&(state->pc_mutex));

        // Wait till trains fills up to press start button.

        pthread_mutex_lock(&(state->start_mutex));

        if (state->start == 0) {
            set_timestamp(timestamp);
            printf("PASSENGER %d <%s>: pressed the START button!\n", id, timestamp);
            state -> start += 1;
            pthread_cond_broadcast(&(state -> start_cond));
        }
        pthread_mutex_unlock(&(state->start_mutex));

        // The button was pressed - wait for doors opening back at the station.

        sem_wait(&(state -> get_off_sem));

        pthread_mutex_lock(&(state->pc_mutex));
        state -> passenger_count -= 1;
        set_timestamp(timestamp);
        printf("PASSENGER %d <%s>: gets off the train. Passengers now aboard: %d\n",
                id, timestamp, state -> passenger_count);

        if (state -> passenger_count == 0) {
            pthread_cond_broadcast(&(state -> pc_cond));
        }

        sem_post(&(state -> get_off_sem));
        pthread_mutex_unlock(&(state->pc_mutex));

        // We got off - try to aboard again (from the beginning).
    }

    set_timestamp(timestamp);
    printf("PASSENGER %d <%s>: shutdown.\n", id, timestamp);
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

    if (sscanf(argv[1], "%d", &g_num_passengers) < 1 || g_num_passengers <= 0) {
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

    g_thread_ids = (pthread_t *) malloc((g_num_trains + g_num_passengers) * sizeof(pthread_t));
    if (g_thread_ids == NULL) {
        exit(EXIT_FAILURE);
    }

    thread_id * args = (thread_id *) malloc((g_num_trains + g_num_passengers) * sizeof(thread_id));
    if (args == NULL) {
        exit(EXIT_FAILURE);
    }

    // Run and wait for threads to finish.

    for (i = 0; i < g_num_trains + g_num_passengers; i++) {
        args[i].id = i < g_num_trains ? i : i - g_num_trains;
        pthread_create(g_thread_ids + i, NULL, i < g_num_trains ? train : passenger, args + i);
    }

    for (i = 0; i < g_num_trains + g_num_passengers; i++) {
        void * ret;
        pthread_join(g_thread_ids[i], &ret);
    }

    sem_destroy(&get_on_sem);

    for (i = 0; i < g_num_trains; i++) {
        sem_destroy(&(g_trains[i].exec_sem));
        sem_destroy(&(g_trains[i].get_off_sem));
    }

    free(g_thread_ids);
    free(args);
    free(g_trains);

    printf("DONE!\n");
    return 0;
}