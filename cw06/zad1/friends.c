#include <stdlib.h>
#include <stdio.h>
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

void
add_friend(friends_collection * fc, int id, int friend_id) {
    friend * iter = fc -> friends_list[id];
    while (iter -> next != NULL) {
        if (iter -> next -> friend_id == friend_id) {
            return;
        }

        iter = iter -> next;
    }

    friend * new_friend = (friend *) malloc(sizeof(friend));
    if (new_friend == NULL) {
        return;
    }

    new_friend -> friend_id = friend_id;
    new_friend -> next = fc -> friends_list[id] -> next;
    fc -> friends_list[id] -> next = new_friend;
}

void
add_friends(friends_collection * fc, int id, int * friend_ids, int friend_count) {
    // Not optimal at all, but well... no one cares.
    int i;
    for (i = 0; i < friend_count; i++) {
        add_friend(fc, id, friend_ids[i]);
    }
}

void
remove_friend(friends_collection * fc, int id, int friend_id) {
    friend * iter = fc -> friends_list[id];
    while (iter -> next != NULL) {
        if (iter -> next -> friend_id == friend_id) {
            friend * tmp = iter -> next;
            iter -> next = tmp -> next;
            free(tmp);

            break;
        }

        iter = iter -> next;
    }
}

void
remove_friends(friends_collection * fc, int id, int * friend_ids, int friend_count) {
    // Again...
    int i;
    for (i = 0; i < friend_count; i++) {
        remove_friend(fc, id, friend_ids[i]);
    }
}

void
remove_all_friends(friends_collection * fc, int id) {
    friend * guard = fc -> friends_list[id];
    if (guard == NULL) {
        return;
    }

    while (guard -> next != NULL) {
        friend * tmp = guard -> next;
        guard -> next = tmp -> next;
        free(tmp);
    }
}

friend * g_iter = NULL;

int
get_friend(friends_collection * fc, int id) {
    if (g_iter == NULL) {
        g_iter = fc -> friends_list[id];
    }

    int friend_id = -1;
    if (g_iter -> next != NULL) {
        friend_id = g_iter -> next -> friend_id;
    } else {
        g_iter = NULL;
    }

    return friend_id;
}

void
display_friends(friends_collection * fc, int id) {
    friend * iter = fc -> friends_list[id];
    while (iter -> next != NULL) {
        printf("Friend: %d\n", iter -> next -> friend_id);
        iter = iter -> next;
    }
}