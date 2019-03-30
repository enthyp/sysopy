#pragma once 

typedef struct file_list {
	int size;
	char ** name;
	char ** path;
	double * period;
} flist;

long read_natural(char * string);

void free_flist(flist * fl);

void print_flist(flist *fl);

flist get_flist(char * list_path);
