#ifndef COMMONS_H
#define COMMONS_H

typedef void (*exit_handler)(void);
typedef void(*signal_handler)(int);

int base_setup(exit_handler e_handler, signal_handler sigint_handler);

#endif // COMMONS_H
