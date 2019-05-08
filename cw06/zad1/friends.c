#include <stdlib.h>
#include "friends.h"


int
setup_friends(friends_collection * fc, int size) {
    fc -> size = size;
    fc -> friends_list = (friend **) malloc(size * sizeof(friend *));

    if (fc -> friends_list == NULL) {
        return -1;
    }

    return 0;
}

void teardown_friends(friends_collection * fc) {
    int i;
    for (i = 0; i < fc -> size; i++) {
        remove_all_friends(fc, i);
        remove_position(fc, i);
    }

    free(fc -> friends_list);
}

int
init_position(friends_collection * fc, int id) {
    friend * guard = (friend *) malloc(sizeof(friend));

    if (guard == NULL) {
        return -1;
    }

    guard -> friend_id = -1;
    guard -> next = NULL;

    fc -> friends_list[id] = guard;

    return 0;
}

void
remove_position(friends_collection * fc, int id) {
    friend * guard = fc -> friends_list[id];
    if (guard != NULL) {
        free(guard);
        fc -> friends_list[id] = NULL;
    }
}

int add_friends(friends_collection * fc, int id, int * friend_ids) {
    return 0;
}

void remove_friends(friends_collection * fc, int id, int * friend_ids) {

}

void remove_all_friends(friends_collection * fc, int id) {

}