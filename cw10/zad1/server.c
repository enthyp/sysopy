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
#include "protocol.h"

#define MIN(a,b) ((a) < (b)) ? (a) : (b)

char * g_line;  // input line in the main thread

typedef enum {
    FREE,
    TRANSMITTING,
    PROCESSING,
    RECEIVING
} connection_state;

typedef struct {
    int socket_fd;
    char * trans_buffer, * recv_buffer;
    int tb_transmitted, tb_received;

    connection_state state;
    pthread_mutex_t state_mutex;

    int q_head, q_tail, q_full;
    char task_queue[CLIENT_TASK_MAX][UNIX_PATH_MAX];
    pthread_mutex_t queue_mutex;
} client_connection;

typedef struct {
    int port;
    int net_socket;

    char local_socket_path[UNIX_PATH_MAX];
    int local_socket;

    int active_sockets;  // epoll file descriptor

    client_connection clients[MAX_CLIENTS];
} server_state;

server_state g_server_state;

void
init_server(server_state * state, int port, char * socket_path) {
    state -> port = port;
    strncpy(state -> local_socket_path, socket_path, UNIX_PATH_MAX);

    // Local socket.
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

    // Network socket.
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

    // Register listener sockets with epoll.
    state -> active_sockets = epoll_create1(0);

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data = (epoll_data_t) state -> local_socket;

    if (epoll_ctl(state -> active_sockets, EPOLL_CTL_ADD, state -> local_socket, &event) == -1) {
        perror("register local socket descriptor with epoll");
        exit(EXIT_FAILURE);
    }

    event.data = (epoll_data_t) state -> net_socket;

    if (epoll_ctl(state -> active_sockets, EPOLL_CTL_ADD, state -> net_socket, &event) == -1) {
        perror("register net socket descriptor with epoll");
        exit(EXIT_FAILURE);
    }

    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        state -> clients[i].socket_fd = -1;
        state -> clients[i].trans_buffer = state -> clients[i].recv_buffer = NULL;
        state -> clients[i].tb_transmitted = state -> clients[i].tb_received = 0;
        state -> clients[i].state = FREE;
        state -> clients[i].q_head = state -> clients[i].q_tail = state -> clients[i].q_full = 0;
        pthread_mutex_init(&(state -> clients[i].state_mutex), NULL);
        pthread_mutex_init(&(state -> clients[i].queue_mutex), NULL);
    }
}

void
post_task(char * line, server_state * state) {
    int i, found = 0;
    for (i = 0; i < MAX_CLIENTS && !found; i++) {
        pthread_mutex_lock(&(state->clients[i].state_mutex));

        if (state->clients[i].state == FREE) {
            pthread_mutex_unlock(&(state->clients[i].state_mutex));

            pthread_mutex_lock(&(state->clients[i].queue_mutex));
            if (!state->clients[i].q_full) {
                int *tail = &(state->clients[i].q_tail);
                strncpy(state->clients[i].task_queue[*tail], line, UNIX_PATH_MAX);
                *tail = (*tail + 1) % CLIENT_TASK_MAX;
                if (*tail == state->clients[i].q_head) {
                    state->clients[i].q_full = 1;
                }
                found = 1;
            }

            pthread_mutex_unlock(&(state->clients[i].queue_mutex));
        } else {
            pthread_mutex_unlock(&(state->clients[i].state_mutex));
        }
    }

    if (!found) {
        for (i = 0; i < MAX_CLIENTS && !found; i++) {
            pthread_mutex_lock(&(state->clients[i].queue_mutex));
            if (!state->clients[i].q_full) {
                int *tail = &(state->clients[i].q_tail);
                strncpy(state->clients[i].task_queue[*tail], line, UNIX_PATH_MAX);
                *tail = (*tail + 1) % CLIENT_TASK_MAX;
                if (*tail == state->clients[i].q_head) {
                    state->clients[i].q_full = 1;
                }
                found = 1;
            }

            pthread_mutex_unlock(&(state->clients[i].queue_mutex));
        }
    }

    if (!found) {
        printf("Client task queues are full, try again later!\n");
    } else {
        printf("Task posted.\n");
    }
}


void
cleanup(void) {
    free(g_line);

    if (shutdown(g_server_state.local_socket, SHUT_RDWR) == -1 || shutdown(g_server_state.net_socket, SHUT_RDWR) == -1) {
        perror("shutdown");
        exit(EXIT_FAILURE);
    }

    close(g_server_state.local_socket);
    unlink(g_server_state.local_socket_path);
    close(g_server_state.net_socket);
    close(g_server_state.active_sockets);
}

void
sigint_handler(int sig) {
    cleanup();
    exit(EXIT_SUCCESS);
}

void
register_client(int client_socket) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (g_server_state.clients[i].socket_fd != -1) {
            g_server_state.clients[i].socket_fd = client_socket;
            break;
        }
    }

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT;
    event.data = (epoll_data_t) -i;  // a hack
    if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_ADD, client_socket, &event) == -1) {
        perror("register client socket descriptor with epoll");
        exit(EXIT_FAILURE);
    }


//    int val_read;
//    if ((val_read = read(client_socket, buffer, 1024)) == 0) {
//        // Connection closed - unregister socket.
//        if (epoll_ctl(g_active_sockets, EPOLL_CTL_DEL, client_socket, &event) == -1) {
//            perror("unregister client socket descriptor from epoll");
//            exit(EXIT_FAILURE);
//        }
//    } else if (val_read != -1) {
//        buffer[MIN(val_read, BUFFER_SIZE - 1)] = '\0';
//        printf("OPENED: %s\n", buffer);
//    } else {
//        perror("read from client socket");
//        exit(EXIT_FAILURE);
//    }
}

void
handle_free(int client_socket, uint32_t events) {
    if ((socket_events[i].events & EPOLLIN) == EPOLLIN) {
        fprintf(stderr, "ERR: UNEXPECTED TRANSMISSION IN STATE: FREE\n");
        return;  // TODO: remove maybe?
    } else {
        //
        // TODO: move reads to global buffer to a separate function!
        int val_read;
        if ((val_read = read(fd, buffer, 1024)) == 0) {
            // Connection closed - unregister socket.
            if (epoll_ctl(g_active_sockets, EPOLL_CTL_DEL, fd, NULL) == -1) {
                perror("unregister client socket descriptor from epoll");
                exit(EXIT_FAILURE);
            }
        } else if (val_read != -1) {
            buffer[MIN(val_read, BUFFER_SIZE - 1)] = '\0';
            send(fd, buffer, strlen(buffer), 0);
        } else {
            perror("read from client socket");
            exit(EXIT_FAILURE);
        }
    }
}

void
handle_trans(int client_socket, uint32_t events) {

}

void
handle_proc(int client_socket, uint32_t events) {

}

void
handle_recv(int client_socket, uint32_t events) {

}

void *
event_loop(void * arg) {
    struct epoll_event socket_events[MAX_CLIENTS + 2];
    int client_count = 0;

    // Accept incoming connections.
    while (1) {
        int i, epoll_count = epoll_wait(g_server_state.active_sockets, socket_events, MAX_CLIENTS + 2, -1);
        for (i = 0; i < epoll_count; i++) {
            int fd = socket_events[i].data.fd;

            if (fd == g_server_state.net_socket || fd == g_server_state.local_socket) {
                int client_socket = accept(fd, NULL, NULL);
                if (client_socket == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                if (client_count == MAX_CLIENTS) {
                    printf("Connection rejected!\n");
                    shutdown(client_socket, SHUT_RDWR);
                    continue;
                }

                // Client connection received - register the socket.
                register_client(client_socket);
            } else {
                // Handle client socket events!
                client_connection * conn = &(g_server_state.clients[-fd]);
                int client_socket = conn -> socket_fd;

                pthread_mutex_lock(&(conn -> state_mutex));
                switch (conn -> state) {
                    case FREE: handle_free(client_socket, socket_events[i].events); break;
                    case TRANSMITTING: handle_trans(client_socket, socket_events[i].events); break;
                    case PROCESSING: handle_proc(client_socket, socket_events[i].events); break;
                    case RECEIVING: handle_recv(client_socket, socket_events[i].events); break;
                }
                pthread_mutex_unlock(&(conn -> state_mutex));  // TODO: maybe release it in handler?
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
    init_server(&g_server_state, port, socket_path);

    // SIGINT.
    signal(SIGINT, sigint_handler);

    // Run event loop and ping threads.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t event_thread, ping_thread;
    pthread_create(&event_thread, &attr, event_loop, NULL);
    pthread_create(&ping_thread, &attr, ping, NULL);

    // Read user input lines.
    size_t len = 0;
    ssize_t nr;

    while ((nr = getline(&g_line, &len, stdin)) > 0) {
        if (nr > 1) {
            post_task(g_line, &g_server_state);
        }
    }

    cleanup();
    exit(EXIT_SUCCESS);
}