#ifndef UTIL_H
#define UTIL_H

typedef struct {
    int dim[2];     // width, height
    double ** content;
} file_content;

int alloc_file_content(file_content * content, int width, int height);

int read_file_content(char * name, file_content ** content);

int save_file_content(char * name, file_content * f_content);

void free_file_content(file_content * content);

int make_random_filter(char * name, int size);

int read_filter(char * name, file_content ** content);

long get_time(void);

#endif // UTIL_H
