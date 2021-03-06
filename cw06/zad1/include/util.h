#ifndef UTIL_H
#define UTIL_H

typedef void (*exit_handler)(void);
typedef void(*signal_handler)(int);

int base_setup(exit_handler e_handler, signal_handler sigint_handler);

int is_empty(char * string);

int read_numbers_list(char * string, int ** result_list);

int prefix_date(char * input, char ** output);

int prefix_id(char * input, char ** output, int id);

int strip_id(char ** string);

#endif // UTIL_H
