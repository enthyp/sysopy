#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include "util.h"

// prototypes defined in this file

int set_signal_handling(signal_handler);

// definitions

int
base_setup(exit_handler e_handler, signal_handler sigint_handler) {
    // Set signal mask and signal handler.
    if (set_signal_handling(sigint_handler) == -1) {
        return -1;
    }

    // Set at exit handler.
    if (atexit(e_handler) != 0) {
        fprintf(stderr, "Failed to set atexit handler.\n");
        return -1;
    }

    return 0;
}

int
set_signal_handling(signal_handler sigint_handler) {
    sigset_t mask_set;
    if (sigfillset(&mask_set) == -1) {
        perror("Fill mask set: ");
        return -1;
    }

    if (sigdelset(&mask_set, SIGINT) == -1) {
        perror("Unmask SIGINT: ");
        return -1;
    }

    if (sigprocmask(SIG_SETMASK, &mask_set, NULL) == -1) {
        perror("Set process signal mask: ");
        return -1;
    }

    struct sigaction act;
    act.sa_handler = sigint_handler;
    act.sa_flags = 0;

    if (sigemptyset(&(act.sa_mask)) == -1) {
        perror("Clean signal mask: ");
        return -1;
    }

    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("Set SIGINT handler: ");
        return -1;
    }

    return 0;
}

int
is_empty(char * s) {
    while (*s != '\0') {
        if (!isspace((unsigned char) *s)) {
            return 0;
        }
        s++;
    }

    return 1;
}

long
read_natural(char * string) {
    char *endp;
    long outcome = strtol(string, &endp, 10);

    if (endp == string) {
        return -1L;
    }
    if ((outcome == LONG_MAX || outcome == LONG_MIN) && errno == ERANGE) {
        return -1L;
    }
    if (*endp != '\0') {
        return -1L;
    }

    return outcome;
}

int *
read_numbers_list(char * string) {
    int i, token_count;
    for (i = 0, token_count = 0; string[i] != '\0'; i++) {
        int j;
        for (j = i; string[j] != ' ' && string[j] != '\t' && string[j] != '\0'; j++);

        if (j != i) {
            token_count++;
            i = j - 1;
        }
    }

    if (token_count == 0) {
        return NULL;
    }

    int * result = (int *) malloc(token_count);
    if (result == NULL) {
        fprintf(stderr, "Failed to allocate memory for numbers list.\n");
        return NULL;
    }

    char * tok, * str = string;
    for (i = 0, tok = strtok(str, " \t"), str = NULL; tok != NULL; i++, tok = strtok(str, " ")) {
        int number = read_natural(tok);
        if (number == -1) {
            fprintf(stderr, "Incorrect token encountered.\n");
            free(result);
            return NULL;
        } else {
            result[i] = number;
        }
    }

    return result;
}

int
prefix_date(char * input, char ** output) {
    time_t now = time(NULL);
    if (now == (time_t) -1) {
        fprintf(stderr, "Failed to get current time.\n");
        return -1;
    }

    struct tm * t = localtime(&now);
    if (t == NULL) {
        fprintf(stderr, "Failed to get time structure.\n");
        return -1;
    }

    int output_length = strlen(input) + 19 + 1;
    *output = (char *) malloc(output_length);
    if (*output == NULL) {
        perror("Memory allocation for date: ");
        return -1;
    }

    if (strftime(*output, output_length, "<%d-%m-%Y %H:%M> ", t) < 19) {
        fprintf(stderr, "Failed to convert date to string.\n");
        free(*output);
        return -1;
    }

    strcat(*output, input);
    return 0;
}