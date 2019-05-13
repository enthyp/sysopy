#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "conveyor_belt.h"

int
main(void) {
    if (create(10, 20) == -1) {
        exit(EXIT_FAILURE);
    }

    sleep(5);

    if (delete() == -1) {
        exit(EXIT_FAILURE);
    }

	exit(EXIT_SUCCESS);
}
