#include "protocol.h"

int
serialize(unsigned char * head, int to_serialize, int num_bytes) {
    int i;
    for (i = 0; i < num_bytes; i++) {
        to_serialize >>= (i * 8);
        *(head + num_bytes - i - 1) = (unsigned char) to_serialize;
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
