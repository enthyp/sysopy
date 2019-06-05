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
#include "common.h"


#define MIN(a,b) ((a) < (b)) ? (a) : (b)

int g_port;
char g_local_socket_path[UNIX_PATH_MAX];
char buffer[BUFFER_SIZE];

int g_net_socket;
int g_local_socket;
int g_active_sockets;

int g_listening = 1;

void
sigint_handler(int sig) {
    g_listening = 0;
}

int
main(int argc, char * argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Pass port number and UNIX socket path.\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(argv[1], "%d", &g_port) == 0) {
        fprintf(stderr, "First argument incorrect!\n");
        exit(EXIT_FAILURE);
    }

    strncpy(g_local_socket_path, argv[2], UNIX_PATH_MAX);
    signal(SIGINT, sigint_handler);

    // Local.
    g_local_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(g_local_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_local_socket_path, UNIX_PATH_MAX);

    if (bind(g_local_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind local");
        exit(EXIT_FAILURE);
    }

    if (listen(g_local_socket, 1) == -1) {
        perror("listen local");
        exit(EXIT_FAILURE);
    }

    // Network.
    g_net_socket = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(g_net_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_in net_addr;
    net_addr.sin_family = AF_INET;
    net_addr.sin_addr.s_addr = INADDR_ANY;
    net_addr.sin_port = htons(g_port);

    if (bind(g_net_socket, (struct sockaddr *)&net_addr, sizeof(addr)) == -1) {
        perror("bind net");
        exit(EXIT_FAILURE);
    }

    if (listen(g_net_socket, 1) == -1) {
        perror("listen net");
        exit(EXIT_FAILURE);
    }

    // Register listener sockets with epoll.
    g_active_sockets = epoll_create1(0);

    struct epoll_event local_event, net_event;
    local_event.events = net_event.events = EPOLLIN;
    local_event.data = (epoll_data_t) g_local_socket;
    net_event.data = (epoll_data_t) g_net_socket;

    if (epoll_ctl(g_active_sockets, EPOLL_CTL_ADD, g_local_socket, &local_event) == -1) {
        perror("register local socket descriptor with epoll");
        exit(EXIT_FAILURE);
    }

    if (epoll_ctl(g_active_sockets, EPOLL_CTL_ADD, g_net_socket, &net_event) == -1) {
        perror("register net socket descriptor with epoll");
        exit(EXIT_FAILURE);
    }

    struct epoll_event socket_events[MAX_CLIENTS + 2];
    int client_count = 0;

    // Accept incoming connections.
    while (g_listening) {
        int i, epoll_count = epoll_wait(g_active_sockets, socket_events, MAX_CLIENTS + 2, -1);
        for (i = 0; i < epoll_count; i++) {
            int fd = socket_events[i].data.fd;

            if (fd == g_net_socket || fd == g_local_socket) {
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

                // Client connection received - register with epoll.
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLOUT;
                event.data = (epoll_data_t) client_socket;
                if (epoll_ctl(g_active_sockets, EPOLL_CTL_ADD, client_socket, &event) == -1) {
                    perror("register client socket descriptor with epoll");
                    exit(EXIT_FAILURE);
                }

                int val_read;
                if ((val_read = read(client_socket, buffer, 1024)) == 0) {
                    // Connection closed - unregister socket.
                    if (epoll_ctl(g_active_sockets, EPOLL_CTL_DEL, client_socket, &event) == -1) {
                        perror("unregister client socket descriptor from epoll");
                        exit(EXIT_FAILURE);
                    }
                } else if (val_read != -1) {
                    buffer[MIN(val_read, BUFFER_SIZE - 1)] = '\0';
                    printf("OPENED: %s\n", buffer);
                } else {
                    perror("read from client socket");
                    exit(EXIT_FAILURE);
                }
            } else {
                if (socket_events[i].events & EPOLLIN == EPOLLIN) {
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
        }
    }

    if (shutdown(g_local_socket, SHUT_RDWR) == -1 || shutdown(g_net_socket, SHUT_RDWR) == -1) {
        perror("shutdown");
        exit(EXIT_FAILURE);
    }

    close(g_local_socket);
    unlink(g_local_socket_path);
    close(g_net_socket);
    close(g_active_sockets);

    return 0;
    //exit(EXIT_SUCCESS);
}