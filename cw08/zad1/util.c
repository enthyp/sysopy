#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "util.h"


int
alloc_file_content(file_content * content, int width, int height) {
    content -> content = (double **) malloc(height * sizeof(double *));
    if (content -> content == NULL) {
        free(content);
        return -1;
    }

    int i;
    for (i = 0; i < height; i++) {
        content -> content[i] = (double *) malloc(width * sizeof(double));
        if (content -> content[i] == NULL) {
            int j;
            for (j = 0; j < i; j++) {
                free(content -> content[j]);
            }
            free(content -> content);
            return -1;
        }
    }

    return 0;
}

void
free_file_content(file_content * content) {
    int i;
    for (i = 0; i < content -> dim[1]; i++) {
        free(content -> content[i]);
    }
    free(content -> content);
    free(content);
}

int
read_file_content(char * name, file_content ** f_content) {
    char * line = NULL;
    size_t len = 0;

    FILE * fp = fopen(name, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open %s file: %s\n", name, strerror(errno));
        return -1;
    }

    file_content * content = (file_content *) malloc(sizeof(file_content));
    if (content == NULL) {
        fprintf(stderr, "Failed to allocate memory for file: %s\n", name);
        return -1;
    }

    if (getline(&line, &len, fp) == -1) {
        fprintf(stderr, "Error at the header of the file: %s\n", name);
        free(content);
        free(line);
        return -1;
    } else {
        if (fscanf(fp, "%d %d", &(content -> dim[0]), &(content -> dim[1])) < 2) {
            free(content);
            free(line);
            return -1;
        }
    }

    // Ignore max pixel value.
    int i;
    fscanf(fp, "%d", &i);
    free(line);

    if (alloc_file_content(content, content -> dim[0], content -> dim[1]) == -1) {
        fprintf(stderr, "Failed to allocate memory for content of file: %s\n", name);
        free(content);
        return -1;
    }

    int j;
    for (i = 0; i < content -> dim[1]; i++) {
        for (j = 0; j < content -> dim[0]; j++) {
            if (fscanf(fp, "%lf", &(content -> content[i][j])) == 0) {
                fprintf(stderr, "Failed to read token no %d!\n", i * content -> dim[1] + j + 1);
                free_file_content(content);
                return -1;
            }
        }
    }

    fclose(fp);
    *f_content = content;

    return 0;
}

int
get_random_filter(char * name, int size) {
    FILE * fp = fopen(name, "w");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open %s file: %s\n", name, strerror(errno));
        return -1;
    }

    double ** filter = (double **) malloc(size * sizeof(double *));
    if (filter == NULL) {
        fclose(fp);
        return -1;
    }

    int i;
    for (i = 0; i < size; i++) {
        filter[i] = (double *) malloc(size * sizeof(double));
        if (filter[i] == NULL) {
            int j;
            for (j = 0; j < i; j++) {
                free(filter[j]);
            }
            free(filter);
            fclose(fp);
            return -1;
        }
    }

    int j;
    double sum = 0;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            filter[i][j] = (int) ((rand() / (RAND_MAX + 1.0)) * 1000);
            sum += filter[i][j];
        }
    }

    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            filter[i][j] /= sum;
        }
    }

    fprintf(fp, "FILTER\n%d %d\n", size, size);

    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            fprintf(fp, "%.4lf ", filter[i][j]);
        }
        fprintf(fp, "\n");
    }

    for (i = 0; i < size; i++) {
        free(filter[i]);
    }
    free(filter);
    fclose(fp);

    return 0;
}