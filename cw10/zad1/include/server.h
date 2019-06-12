#ifndef SERVER_H
#define SERVER_H

#define CLIENT_NAME_MAX 10          // Max client name length
#define CLIENTS_MAX 15              // Max clients for server
#define CLIENT_TASK_MAX 10          // Size of (server-side) client task queue

#define LOCAL_SOCK CLIENTS_MAX      // Local socket ID
#define NET_SOCK CLIENTS_MAX + 1    // Net socket ID

typedef enum {
    INITIAL,        // Before name registration
    FREE,           // Idle, with empty task queue
    TRANSMITTING,   // Sending task data
    PROCESSING,     // Waiting for results
    RECEIVING,      // Receiving results data
    BUSY,           // Enqueued tasks tb processed but none of the above
} conn_state;

typedef struct {
    int id;
    int fd;
    int size;
} task;

typedef struct {
    int socket_fd;

    void * handler;     // buffer manager for given connection state

    conn_state state;
    pthread_mutex_t mutex;
} client_conn;

typedef struct {
    int q_head, q_tail, q_full;
    task task_queue[CLIENT_TASK_MAX];
    pthread_mutex_t mutex;
} client_queue;

typedef struct {
    char name[CLIENT_NAME_MAX];
    client_conn connection;
    client_queue task_queue;
} client;

typedef struct {
    int port;
    int net_socket;

    char local_socket_path[UNIX_PATH_MAX];
    int local_socket;

    unsigned int next_task_id;

    int clients_count;
    pthread_mutex_t count_mutex;

    client clients[CLIENTS];

    int events;     // epoll file descriptor
} server_state;

#endif // SERVER_H
