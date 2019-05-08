#ifndef FRIENDS_H
#define FRIENDS_H

typedef struct friend {
    int friend_id;
    struct friend * next;
} friend;

typedef struct {
    int size;
    friend ** friends_list;
} friends_collection;

int setup_friends(friends_collection * fc, int size);

void teardown_friends(friends_collection * fc);

int init_position(friends_collection * fc, int id);

void remove_position(friends_collection * fc, int id);

int add_friends(friends_collection * fc, int id, int * friend_ids);

void remove_friends(friends_collection * fc, int id, int * friend_ids);

void remove_all_friends(friends_collection * fc, int id);

#endif // FRIENDS_H
