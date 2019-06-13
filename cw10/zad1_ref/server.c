#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "server.h"
#include "states.h"
#include "protocol.h"
#include "common.h"


char * g_line;  // input line in the main thread

// Globals defined in this file.
void * event_loop(void * arg);
void init_server(server_state * state, int port, char * socket_path);
int register_conn(server_state * state, int sock_fd);

server_state g_state;


int
enqueue_task(server_state * state, task * new_task) {
    int i, found;
    for (i = 0, found = 0; i < CLIENTS_MAX && !found; i++) {
        client_conn * conn = &(state -> clients[i].connection);
        client_queue * q = &(state -> clients[i].task_queue);

        pthread_mutex_lock(&(conn -> mutex));

        if (conn -> state == FREE) {
            pthread_mutex_lock(&(q -> mutex));

            int *tail = &(q -> q_tail);
            memcpy(q -> q_table + *tail, new_task, sizeof(*new_task));
            *tail = (*tail + 1) % CLIENT_TASK_MAX;
            if (*tail == q -> q_head) {
                q -> q_full = 1;
            }

            pthread_mutex_unlock(&(q -> mutex));
            free_to_busy(conn -> handler, &g_state, i);

            found = 1;
        }

        pthread_mutex_unlock(&(conn -> mutex));
    }

    if (!found) {
        for (i = 0; i < CLIENTS_MAX && !found; i++) {
            client_conn * conn = &(state -> clients[i].connection);
            client_queue * q = &(state -> clients[i].task_queue);

            pthread_mutex_lock(&(conn -> mutex));

            if (conn -> state != INITIAL && !(q -> q_full)) {
                pthread_mutex_lock(&(q -> mutex));

                int *tail = &(q -> q_tail);
                memcpy(q -> q_table + *tail, new_task, sizeof(*new_task));
                *tail = (*tail + 1) % CLIENT_TASK_MAX;
                if (*tail == q -> q_head) {
                    q -> q_full = 1;
                }

                pthread_mutex_unlock(&(q -> mutex));

                if (conn -> state == FREE) {
                    free_to_busy(conn -> handler, &g_state, i);
                }

                found = 1;
            }

            pthread_mutex_unlock(&(conn -> mutex));
        }
    }

    return found ? i - 1 : -1;
}

void
enqueue(server_state * state, char * line) {
    int fd = open(line, O_RDONLY);
    if (fd == -1) {
        perror("OPEN FILE");
        return;
    }

    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    int id = state -> next_task_id;
    state -> next_task_id += 1;

    task new_task = { .id = id, .fd = fd, .size = size };

    int client_id;
    if ((client_id = enqueue_task(state, &new_task)) == -1) {
        close(fd);
        printf("No empty client queue was found! Try again later.\n");
    } else {
        printf("Task posted for client %s.\n", state -> clients[client_id].name);
    }
}

void
sigint_handler(int sig) {
    exit(EXIT_SUCCESS);
}

void
init_server(server_state * state, int port, char * socket_path) {
    state -> next_task_id = 0;

    state -> port = port;
    strncpy(state -> local_socket_path, socket_path, UNIX_PATH_MAX);

    // Initialize and listen on local socket.
    state -> local_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(state -> local_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, state -> local_socket_path, UNIX_PATH_MAX);

    if (bind(state -> local_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind local");
        exit(EXIT_FAILURE);
    }

    if (listen(state -> local_socket, 1) == -1) {
        perror("listen local");
        exit(EXIT_FAILURE);
    }

    // Initialize and listen on network socket.
    state -> net_socket = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(state -> net_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_in net_addr;
    net_addr.sin_family = AF_INET;
    net_addr.sin_addr.s_addr = INADDR_ANY;
    net_addr.sin_port = htons(port);

    if (bind(state -> net_socket, (struct sockaddr *)&net_addr, sizeof(addr)) == -1) {
        perror("bind net");
        exit(EXIT_FAILURE);
    }

    if (listen(state -> net_socket, 1) == -1) {
        perror("listen net");
        exit(EXIT_FAILURE);
    }

    // Register both sockets with epoll.
    state -> events = epoll_create1(0);

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data = (epoll_data_t) 0;
    event.data.fd = LOCAL_SOCK;

    if (epoll_ctl(state -> events, EPOLL_CTL_ADD, state -> local_socket, &event) == -1) {
        perror("register local socket descriptor with epoll");
        exit(EXIT_FAILURE);
    }

    event.data = (epoll_data_t) 0;
    event.data.fd = NET_SOCK;

    if (epoll_ctl(state -> events, EPOLL_CTL_ADD, state -> net_socket, &event) == -1) {
        perror("register net socket descriptor with epoll");
        exit(EXIT_FAILURE);
    }

    // Clients count initialization.
    state -> client_count = 0;
    pthread_mutex_init(&(state -> count_mutex), NULL);

    // Initialize clients.
    int i;
    for (i = 0; i < CLIENTS_MAX; i++) {
        client * cl = &(state -> clients[i]);
        client_conn * conn = &(cl -> connection);
        client_queue * q = &(cl -> task_queue);

        conn -> state = INITIAL;
        conn -> socket_fd = -1;
        conn -> handler = (handler_initial *) malloc(sizeof(handler_initial));
        if (conn -> handler == NULL) {
            perror("add connection handler");
            exit(EXIT_FAILURE);
        }
        if (initialize_handler((handler_initial *) conn -> handler) == -1) {
            perror("initialize connection handler");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_init(&(conn -> mutex), NULL);

        q -> q_head = q -> q_tail = q -> q_full = 0;
        pthread_mutex_init(&(q -> mutex), NULL);
    }
}

int
add_event(server_state * state, int client_id, uint32_t events) {
    struct epoll_event event;
    event.events = events;
    event.data = (epoll_data_t) client_id;

    int sock_fd = state -> clients[client_id].connection.socket_fd;
    if (epoll_ctl(state -> events, EPOLL_CTL_ADD, sock_fd, &event) == -1) {
        perror("register client socket descriptor with epoll");
        return -1;
    }

    return 0;
}

int
del_event(server_state * state, int client_id) {
    int sock_fd = state -> clients[client_id].connection.socket_fd;
    if (epoll_ctl(state -> events, EPOLL_CTL_DEL, sock_fd, NULL) == -1) {
        perror("unregister client socket descriptor from epoll");
        return -1;
    }

    return 0;
}

int
register_conn(server_state * state, int sock_fd) {
    // Bump client count if possible.
    pthread_mutex_lock(&(state -> count_mutex));

    if (state -> client_count == CLIENTS_MAX) {
        // Too many connections.
        close(sock_fd);
        pthread_mutex_unlock(&(state -> count_mutex));
        return -1;
    } else {
        state -> client_count += 1;
        pthread_mutex_unlock(&(state -> count_mutex));
    }

    int j;
    for (j = 0; j < CLIENTS_MAX; j++) {
        if (state -> clients[j].connection.socket_fd == -1) {
            state -> clients[j].connection.socket_fd = sock_fd;
            break;
        }
    }

    // Add two-way event on this socket.
    if (add_event(state, j, EPOLLIN | EPOLLOUT) == -1) {
        exit(EXIT_FAILURE);
    }

    return j;
}

void *
event_loop(void * arg) {
    struct epoll_event socket_events[CLIENTS_MAX + 2];

    // Accept incoming connections.
    while (1) {
        int i, epoll_count = epoll_wait(g_state.events, socket_events, CLIENTS_MAX + 2, -1);

        for (i = 0; i < epoll_count; i++) {
            int id = socket_events[i].data.fd;

            if (id == LOCAL_SOCK || id == NET_SOCK) {
                // Accept connection.
                int sock_fd = id == NET_SOCK ? g_state.net_socket : g_state.local_socket;
                int client_socket = accept(sock_fd, NULL, NULL);
                if (client_socket == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                // Register the connection with server so that it can be handled properly.
                int client_id;
                if ((client_id = register_conn(&g_state, client_socket)) == -1) {
                    // Too many connections - it was dropped.
                    continue;
                }

                // Handle initialization.
                client_conn * conn = &(g_state.clients[client_id].connection);
                pthread_mutex_lock(&(conn -> mutex));
                handle_receive(conn -> handler, &g_state, client_id, conn -> state);
            } else {
                client_conn * conn = &(g_state.clients[id].connection);
                pthread_mutex_lock(&(conn -> mutex));

                if ((socket_events[i].events & EPOLLIN) == EPOLLIN) {
                    // Check if connection still up.
                    char mtype;
                    if (recv(conn -> socket_fd, &mtype, 1, MSG_PEEK) == 0) {
                        handle_closed(conn -> handler, &g_state, id, conn -> state);
                    } else {
                        // Input data ready.
                        handle_receive(conn -> handler, &g_state, id, conn -> state);
                    }
                } else {
                    // Ready for output.
                    handle_send(conn -> handler, &g_state, id, conn -> state);
                }
            }
        }
    }

    return NULL;
}

void *
ping(void * arg) {

    return NULL;
}

int
main(int argc, char * argv[]) {
    // Read args.
    if (argc != 3) {
        fprintf(stderr, "Pass port number and UNIX socket path.\n");
        exit(EXIT_FAILURE);
    }

    int port;
    char socket_path[UNIX_PATH_MAX];

    if (sscanf(argv[1], "%d", &port) == 0) {
        fprintf(stderr, "First argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    strncpy(socket_path, argv[2], UNIX_PATH_MAX);

    // Initialize server state.
    init_server(&g_state, port, socket_path);

    // SIGINT.
    signal(SIGINT, sigint_handler);

    // Run event loop and pinging thread.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t event_thread, ping_thread;
    pthread_create(&event_thread, &attr, event_loop, NULL);
    pthread_create(&ping_thread, &attr, ping, NULL);

    // Read user input lines.
    size_t len = 0;
    ssize_t nr;

    // Read user input and assign tasks.
    while ((nr = getline(&g_line, &len, stdin)) > 0) {
        if (nr > 1) {
            if (g_line[nr - 1] == '\n') {
                g_line[nr - 1] = '\0';
            }
            enqueue(&g_state, g_line);
        }
    }

    exit(EXIT_SUCCESS);
}