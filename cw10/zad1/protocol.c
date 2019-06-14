#include "protocol.h"
#include <stdio.h>

int
serialize(unsigned char * head, int to_serialize, int num_bytes) {
    *(head + num_bytes - 1) = (unsigned char) to_serialize;
    int i;
    for (i = num_bytes - 2; i >= 0; i--) {
        to_serialize >>= 8;
        *(head + i) = (unsigned char) to_serialize;
    }
    return 0;
}

int
deserialize(unsigned char * head, int * result, int num_bytes) {
    int outcome = (int) head[0];
    int i;
    for (i = 1; i < num_bytes; i++) {
        outcome <<= 8;
        outcome |= *(head + i);
    }
    *result = outcome;
    return 0;
}
