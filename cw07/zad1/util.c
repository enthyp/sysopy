#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include "util.h"

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


int
set_signal_handling(signal_handler sigint_handler) {
    sigset_t mask_set;
    if (sigfillset(&mask_set) == -1) {
        perror("Fill mask set");
        return -1;
    }

    if (sigdelset(&mask_set, SIGINT) == -1) {
        perror("Unmask SIGINT");
        return -1;
    }

    if (sigprocmask(SIG_SETMASK, &mask_set, NULL) == -1) {
        perror("Set process signal mask");
        return -1;
    }

    struct sigaction act;
    act.sa_handler = sigint_handler;
    act.sa_flags = 0;

    if (sigemptyset(&(act.sa_mask)) == -1) {
        perror("Clean signal mask");
        return -1;
    }

    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("Set SIGINT handler");
        return -1;
    }

    return 0;
}

long
get_time(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        perror("Get current time");
        return -1;
    }

    return (long)(ts.tv_sec * 1.0e6 + ts.tv_nsec / 1.0e3);
}