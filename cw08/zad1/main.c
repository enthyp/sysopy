#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include "util.h"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

typedef struct {
    int num;    // thread number
    long ms;
} data;

int g_num_thr;
int g_block;
file_content * g_input = NULL, * g_filter = NULL, * g_output = NULL;

int
single_conv(int i, int j) {
    double val = 0;
    int k, l;
    int f_size = g_filter -> dim[0];
    for (k = 0; k < f_size; k++) {
        for (l = 0; l < f_size; l++) {
            int x = MIN(MAX(0, j - ceil(f_size / 2.0) + k), g_input -> dim[0] - 1);
            int y = MIN(MAX(0, i - ceil(f_size / 2.0) + l), g_input -> dim[1] - 1);
            double f_val = g_filter -> content[k][l] * g_input -> content[y][x];
            val += f_val;
        }
    }

    return round(val);
}

void *
convolution(void * input) {
    long start = get_time();
    data * args = (data *) input;
    int num = args -> num;
    int width = g_input -> dim[0];
    int height = g_input -> dim[1];

    if (g_block) {
        int i, j, m = ceil(width / g_num_thr);
        for (i = 0; i < height; i++) {
            for (j = num * m; j < (num + 1) * m && j < width; j++) {
                g_output -> content[i][j] = single_conv(i, j);
            }
        }
    } else {
        int i, j;
        for (i = 0; i < height; i++) {
            for (j = num; j < width; j += g_num_thr) {
                g_output -> content[i][j] = single_conv(i, j);
            }
        }
    }

    args -> ms = get_time() - start;

    return args;
}

int
main(int argc, char * argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Pass num of threads, block/interleaved, input image file name, "
                        "filter file name and output file name.\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(argv[1], "%d", &g_num_thr) == 0) {
        fprintf(stderr, "First argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[2], "block") == 0) {
        g_block = 1;
    } else if (strcmp(argv[2], "interleaved") == 0) {
        g_block = 0;
    } else {
        fprintf(stderr, "Second argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    char * input_file = argv[3];
    char * filter_file = argv[4];
    char * output_file = argv[5];

    if (read_file_content(input_file, &g_input) == -1) {
        exit(EXIT_FAILURE);
    }
    if (read_filter(filter_file, &g_filter) == -1) {
        free_file_content(g_filter);
        exit(EXIT_FAILURE);
    }

    g_output = (file_content *) malloc(sizeof(file_content));
    if (g_output == NULL) {
        fprintf(stderr, "Failed to allocate output image memory.\n");
        free_file_content(g_input);
        free_file_content(g_filter);
        exit(EXIT_FAILURE);
    }
    if (alloc_file_content(g_output, g_input -> dim[0], g_input -> dim[1]) == -1) {
        fprintf(stderr, "Failed to allocate output image memory.\n");
        free_file_content(g_input);
        free_file_content(g_filter);
        exit(EXIT_FAILURE);
    }

    // Do actual work...
    pthread_t * thread_ids = (pthread_t *) malloc(g_num_thr * sizeof(pthread_t));
    if (thread_ids == NULL) {
        free_file_content(g_input);
        free_file_content(g_filter);
        free_file_content(g_output);
        exit(EXIT_FAILURE);
    }
    data * args_table = (data *) malloc(g_num_thr * sizeof(data));
    if (args_table == NULL) {
        free_file_content(g_input);
        free_file_content(g_filter);
        free_file_content(g_output);
        free(thread_ids);
        exit(EXIT_FAILURE);
    }

    long start = get_time();
    int i;
    for (i = 0; i < g_num_thr; i++) {
        args_table[i].num = i;
        pthread_create(thread_ids + i, NULL, convolution, args_table + i);
    }

    for (i = 0; i < g_num_thr; i++) {
        void * ret;
        pthread_join(thread_ids[i], &ret);
        long ms = ((data *)ret) -> ms;
        printf("Thread %ld: %.6lfs\n", thread_ids[i], ms / 1e6);
    }

    printf("Total time: %.6lfs\n", (get_time() - start) / 1e6);

    if (save_file_content(output_file, g_output) == -1) {
        free_file_content(g_input);
        free_file_content(g_filter);
        free_file_content(g_output);
        free(thread_ids);
        free(args_table);
        exit(EXIT_FAILURE);
    }

    free_file_content(g_input);
    free_file_content(g_filter);
    free_file_content(g_output);
    free(thread_ids);
    free(args_table);

    exit(EXIT_SUCCESS);
}
