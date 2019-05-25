#ifndef UTIL_H
#define UTIL_H

typedef struct {
    int dim[2]; // width, height
    double ** content;
} file_content;

int read_file_content(char * name, file_content ** content);

void free_file_content(file_content * content);

int get_random_filter(char * name, int size);

#endif // UTIL_H
