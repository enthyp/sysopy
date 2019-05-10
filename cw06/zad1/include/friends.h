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

void add_friend(friends_collection * fc, int id, int friend_id);

void remove_friend(friends_collection * fc, int id, int friend_id);

void remove_all_friends(friends_collection * fc, int id);

int get_friend(friends_collection * fc, int id);

void display_friends(friends_collection * fc, int id);

#endif // FRIENDS_H
