#ifndef UTIL_H
#define UTIL_H

typedef void(*signal_handler)(int);

long read_natural(char * string);

int set_signal_handling(signal_handler sigint_handler);

long get_time(void);

#endif // UTIL_H
