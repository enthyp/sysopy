#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "conveyor_belt.h"

int 
main(void) {
    if (open_belt() == -1) {
        exit(EXIT_FAILURE);
    }

    sleep(1);

    if (close_belt() == -1) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
