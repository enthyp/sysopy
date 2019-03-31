#pragma once 

typedef struct file_list {
	int size;
	char ** name;
	char ** path;
	double * period;
} flist;

long read_natural(char * string);

int split_str(char * input, char ** tokens, int count);

void free_flist(flist * fl);

void print_flist(flist *fl);

flist get_flist(char * list_path);
