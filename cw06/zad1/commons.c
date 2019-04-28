#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "include/commons.h"

// prototypes defined in this file

int set_signal_handling(signal_handler);

// definitions

void
sigalrm_handler(int sig) {}

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
    if (sigdelset(&mask_set, SIGALRM) == -1) {
        perror("Unmask SIGALRM: ");
        return -1;
    }

    if (sigprocmask(SIG_SETMASK, &mask_set, NULL) == -1) {
        perror("Set process signal mask: ");
        return -1;
    }

    //signal(SIGALRM, SIG_IGN);

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

    act.sa_handler = sigalrm_handler;

    if (sigaction(SIGALRM, &act, NULL) == -1) {
        perror("Set SIGALRM handler: ");
        return -1;
    }

    return 0;
}
