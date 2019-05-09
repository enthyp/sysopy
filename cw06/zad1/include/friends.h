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

void add_friends(friends_collection * fc, int id, int * friend_ids, int friend_count);

void remove_friends(friends_collection * fc, int id, int * friend_ids, int friend_count);

void remove_all_friends(friends_collection * fc, int id);

int get_friend(friends_collection * fc, int id);

void display_friends(friends_collection * fc, int id);

#endif // FRIENDS_H