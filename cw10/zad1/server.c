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
#include "protocol.h"


char * g_line;  // input line in the main thread

typedef enum {
    INITIAL,        // Name tb checked
    FREE,           // Empty task queue
    WORKING,        // Enqueued tasks tb processed
    TRANSMITTING,   // Sending task data
    PROCESSING,     // Waiting for results
    RECEIVING,      // Receiving results data
} connection_state;

typedef struct {
    int id;
    int fd;
    int size;
} task;

typedef struct {
    int socket_fd;
    char name[CLIENT_NAME_MAX];

    unsigned char * recv_buffer;
    int rec_buf_head, tb_received;

    unsigned char trans_buffer[BUFFER_SIZE];
    int buf_head, inbuf_pending, tb_transmitted;

    connection_state state;

    int q_head, q_tail, q_full;
    task task_queue[CLIENT_TASK_MAX];

    pthread_mutex_t conn_mutex;
} client_connection;

typedef struct {
    int port;
    int net_socket;

    char local_socket_path[UNIX_PATH_MAX];
    int local_socket;

    int active_sockets;     // epoll file descriptor

    int clients_count;
    pthread_mutex_t count_mutex;

    client_connection clients[MAX_CLIENTS];
    unsigned int task_id;   // next server task ID
} server_state;

server_state g_server_state;

void
init_server(server_state * state, int port, char * socket_path) {
    state -> task_id = 0;

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

    // Clients count initialization.
    state -> clients_count = 0;
    pthread_mutex_init(&(state -> count_mutex), NULL);

    // Initialize client connections.
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        state -> clients[i].socket_fd = -1;
        state -> clients[i].q_head = state -> clients[i].q_tail = state -> clients[i].q_full = 0;
        state -> clients[i].recv_buffer = NULL;
        state -> clients[i].buf_head = state -> clients[i].rec_buf_head = 0;
        state -> clients[i].inbuf_pending = state -> clients[i].tb_transmitted = state -> clients[i].tb_received = 0;
        state -> clients[i].state = INITIAL;
        pthread_mutex_init(&(state -> clients[i].conn_mutex), NULL);
    }
}

int
enqueue_task(server_state * state, task * new_task) {
    int i, found = 0;
    for (i = 0; i < MAX_CLIENTS && !found; i++) {
        pthread_mutex_lock(&(state->clients[i].conn_mutex));

        if (state->clients[i].state == FREE) {
            int *tail = &(state->clients[i].q_tail);
            memcpy(state->clients[i].task_queue + *tail, new_task, sizeof(*new_task));
            *tail = (*tail + 1) % CLIENT_TASK_MAX;
            if (*tail == state->clients[i].q_head) {
                state->clients[i].q_full = 1;
            }
            state->clients[i].state = WORKING;
            state -> clients[i].tb_transmitted = new_task -> size + 5;

            client_connection * conn = &(state -> clients[i]);
            if (epoll_ctl(state -> active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL) == -1) {
                perror("register socket descriptor with epoll");
                exit(EXIT_FAILURE);
            }

            struct epoll_event event;
            event.events = EPOLLOUT | EPOLLIN;
            event.data = (epoll_data_t) -i;

            if (epoll_ctl(state -> active_sockets, EPOLL_CTL_ADD, conn -> socket_fd, &event) == -1) {
                perror("register socket descriptor with epoll");
                exit(EXIT_FAILURE);
            }

            found = 1;
        }

        pthread_mutex_unlock(&(state->clients[i].conn_mutex));
    }

    if (!found) {
        for (i = 0; i < MAX_CLIENTS && !found; i++) {
            pthread_mutex_lock(&(state->clients[i].conn_mutex));

            if (state->clients[i].state != INITIAL && !state->clients[i].q_full) {
                int *tail = &(state->clients[i].q_tail);
                memcpy(state->clients[i].task_queue + *tail, new_task, sizeof(*new_task));
                *tail = (*tail + 1) % CLIENT_TASK_MAX;
                if (*tail == state->clients[i].q_head) {
                    state->clients[i].q_full = 1;
                }

                state -> clients[i].tb_transmitted = new_task -> size + 5;
                if (state->clients[i].state == FREE) {
                    state -> clients[i].state = WORKING;

                    client_connection * conn = &(state -> clients[i]);
                    if (epoll_ctl(state -> active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL) == -1) {
                        perror("register socket descriptor with epoll");
                        exit(EXIT_FAILURE);
                    }

                    struct epoll_event event;
                    event.events = EPOLLOUT | EPOLLIN;
                    event.data = (epoll_data_t) -i;

                    if (epoll_ctl(state -> active_sockets, EPOLL_CTL_ADD, conn -> socket_fd, &event) == -1) {
                        perror("register socket descriptor with epoll");
                        exit(EXIT_FAILURE);
                    }
                }
                found = 1;
            }

            pthread_mutex_unlock(&(state->clients[i].conn_mutex));
        }
    }

    return found ? i - 1 : -1;
}

void
post_task(char * line, server_state * state) {
    int fd = open(line, O_RDONLY);
    if (fd == -1) {
        perror("OPEN FILE");
        return;
    }

    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    int id = state -> task_id;
    state -> task_id += 1;

    task new_task = { .id = id, .fd = fd, .size = size };

    int client_id;
    if ((client_id = enqueue_task(state, &new_task)) == -1) {
        close(fd);
        printf("No empty client queue was found! Try again later.\n");
    } else {
        printf("Task posted for client %s.\n", g_server_state.clients[client_id].name);
    }
}

void
cleanup(void) {
    free(g_line);

    epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, g_server_state.local_socket, NULL);
    epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, g_server_state.net_socket, NULL);
    if (shutdown(g_server_state.local_socket, SHUT_RDWR) == -1 || shutdown(g_server_state.net_socket, SHUT_RDWR) == -1) {
        perror("shutdown");
        exit(EXIT_FAILURE);
    }

    close(g_server_state.local_socket);
    unlink(g_server_state.local_socket_path);
    close(g_server_state.net_socket);
    close(g_server_state.active_sockets);

    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        int client_socket = g_server_state.clients[i].socket_fd;
        if (client_socket == -1) {
            continue;
        }
        epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, client_socket, NULL);
        shutdown(client_socket, SHUT_RDWR);
        close(client_socket);
    }
}

void
sigint_handler(int sig) {
    cleanup();
    exit(EXIT_SUCCESS);
}

int
check_name(char * name, int id) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (i != id) {
            pthread_mutex_lock(&(g_server_state.clients[i].conn_mutex));
            if (g_server_state.clients[i].socket_fd != -1 && strcmp(name, g_server_state.clients[i].name) == 0) {
                pthread_mutex_unlock(&(g_server_state.clients[i].conn_mutex));
                return -1;
            }
            pthread_mutex_unlock(&(g_server_state.clients[i].conn_mutex));
        }
    }

    return 0;
}

void
handle_initial(int client_id) {
    printf("LOG: HANDLING INITIAL FOR ID: %d\n", client_id);

    // Receive full name, check it and drop connection or add client.
    client_connection * conn = &(g_server_state.clients[client_id]);
    char mtype;
    if (conn -> recv_buffer == NULL) {
        recv(conn -> socket_fd, &mtype, 1, MSG_PEEK);
        if (mtype != REGISTER) {
            // Incorrect opening - drop it.
            close(conn -> socket_fd);
            epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL);
            conn -> socket_fd = -1;
            pthread_mutex_lock(&(g_server_state.count_mutex));
            g_server_state.clients_count -= 1;
            pthread_mutex_unlock(&(g_server_state.count_mutex));
            pthread_mutex_unlock(&(g_server_state.clients[client_id].conn_mutex));
            return;
        }

        conn -> recv_buffer = (unsigned char *) malloc((CLIENT_NAME_MAX + 1) * sizeof(unsigned char));
        conn -> tb_received = CLIENT_NAME_MAX + 1;
    }

    int received = recv(conn -> socket_fd, conn -> recv_buffer, conn -> tb_received, MSG_DONTWAIT);
    conn -> tb_received -= received;

    int i = 0;
    while (i <= CLIENT_NAME_MAX && conn -> recv_buffer[i + 1] != '\0') i++;
    memcpy(conn -> name, conn -> recv_buffer + 1, i);

    if (conn -> tb_received == 0) {
        // Whole name received - check.
        if (check_name(conn -> name, client_id) != 0) {
            // Name repetition - drop it.
            printf("NAME EXISTS: %s\n", conn -> recv_buffer + 1);
            mtype = NAME_EXISTS;
            write(conn -> socket_fd, &mtype, 1);
            close(conn -> socket_fd);
            epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL);
            conn -> socket_fd = -1;
            free(conn -> recv_buffer);
            conn -> recv_buffer = NULL;
            pthread_mutex_lock(&(g_server_state.count_mutex));
            g_server_state.clients_count -= 1;
            pthread_mutex_unlock(&(g_server_state.count_mutex));
            pthread_mutex_unlock(&(g_server_state.clients[client_id].conn_mutex));
        } else {
            // Name ok - add it.
            free(conn -> recv_buffer);
            conn -> recv_buffer = NULL;
            conn -> state = FREE;
            printf("NEW CLIENT: %s\n", conn -> name);
            mtype = OK_REGISTER;
            write(conn -> socket_fd, &mtype, 1);
            epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL);

            struct epoll_event event;
            event.events = EPOLLIN;
            event.data = (epoll_data_t) -client_id;

            if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_ADD, conn -> socket_fd, &event) == -1) {
                perror("register socket descriptor with epoll");
                exit(EXIT_FAILURE);
            }

            pthread_mutex_unlock(&(g_server_state.clients[client_id].conn_mutex));
        }
    } else {
        pthread_mutex_unlock(&(g_server_state.clients[client_id].conn_mutex));
    }
}

void
handle_free(int client_id, uint32_t events) {
    client_connection * conn = &(g_server_state.clients[client_id]);
    printf("LOG: HANDLING FREE FOR: %s\n", conn -> name);
    char mtype;
    read(conn -> socket_fd, &mtype, 1);
    // TODO: ping handling only!

    pthread_mutex_unlock(&(conn -> conn_mutex));
}

void
handle_working(int client_id, uint32_t events) {
    client_connection * conn = &(g_server_state.clients[client_id]);
    printf("LOG: HANDLING WORKING FOR: %s\n", conn -> name);

    if ((events & EPOLLOUT) == EPOLLOUT) {
        conn -> state = TRANSMITTING;

        if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL) == -1) {
            perror("register socket descriptor with epoll");
            exit(EXIT_FAILURE);
        }

        struct epoll_event event;
        event.events = EPOLLOUT;
        event.data = (epoll_data_t) -client_id;

        if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_ADD, conn -> socket_fd, &event) == -1) {
            perror("register socket descriptor with epoll");
            exit(EXIT_FAILURE);
        }

    } else if ((events & EPOLLIN) == EPOLLIN) {
        // TODO: pong maybe?
    }

    pthread_mutex_unlock(&(conn -> conn_mutex));
}

void
handle_trans(int client_id, uint32_t events) {
    client_connection * conn = &(g_server_state.clients[client_id]);
    printf("LOG: HANDLING TRANSMITTING FOR: %s\n", conn -> name);

    if ((events & EPOLLOUT) == EPOLLOUT) {
        // We can write to the socket - start transmitting the task.
        task * cur_task = &(conn -> task_queue[conn -> q_head]);

        // Fill the buffer if starting.
        if (conn -> tb_transmitted == cur_task -> size + 5) {
            conn -> trans_buffer[0] = TASK;
            conn -> trans_buffer[1] = (unsigned char) (cur_task->id >> 8);
            conn -> trans_buffer[2] = (unsigned char) cur_task->id;
            conn -> trans_buffer[3] = (unsigned char) (cur_task->size >> 8);
            conn -> trans_buffer[4] = (unsigned char) cur_task->size;

            int nr = read(cur_task -> fd, conn -> trans_buffer + 5, BUFFER_SIZE - 5);
            conn -> inbuf_pending += nr + 5;
            conn -> buf_head = 0;
        }

        if (conn -> inbuf_pending == 0) {
            // Fill the buffer if necessary.
            int nr = read(cur_task -> fd, conn -> trans_buffer, BUFFER_SIZE);
            conn -> inbuf_pending += nr;
            conn -> buf_head = 0;
        }

        int nw = send(conn -> socket_fd, conn -> trans_buffer + conn -> buf_head, conn -> inbuf_pending, MSG_DONTWAIT);
        if (nw == -1) {
            perror ("ERR IN TRANSMITTING STATE: WRITE");
            exit(EXIT_FAILURE);
        }
        conn -> inbuf_pending -= nw;
        conn -> tb_transmitted -= nw;
        conn -> buf_head += nw;

        if (conn -> tb_transmitted == 0) {
            conn -> state = PROCESSING;
            close(cur_task -> fd);
            conn -> q_head = (conn -> q_head + 1) % CLIENT_TASK_MAX;
            conn -> q_full = 0;

            if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL) == -1) {
                perror("register socket descriptor with epoll");
                exit(EXIT_FAILURE);
            }

            struct epoll_event event;
            event.events = EPOLLIN;
            event.data = (epoll_data_t) -client_id;

            if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_ADD, conn -> socket_fd, &event) == -1) {
                perror("register socket descriptor with epoll");
                exit(EXIT_FAILURE);
            }
        }

        pthread_mutex_unlock(&(conn -> conn_mutex));
    } else {
        fprintf(stderr, "ERR IN STATE TRANSMITTING: UNEXPECTED EVENT!\n");
        exit(EXIT_FAILURE);
    }

}

void
handle_proc(int client_id, uint32_t events) {
    client_connection * conn = &(g_server_state.clients[client_id]);
    printf("LOG: HANDLING PROCESSING FOR: %s\n", conn -> name);

    if ((events & EPOLLIN) == EPOLLIN) {
        // We can read from the socket - this could be pong or results.
        // TODO: handle pong
        char mtype;
        if (conn -> recv_buffer == NULL) {
            read(conn -> socket_fd, &mtype, 1);

            if (mtype != TASK_RESULT && mtype != PONG) {
                // Drop it.
                close(conn -> socket_fd);
                epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL);
                conn -> socket_fd = -1;
                pthread_mutex_lock(&(g_server_state.count_mutex));
                g_server_state.clients_count -= 1;
                pthread_mutex_unlock(&(g_server_state.count_mutex));
                pthread_mutex_unlock(&(g_server_state.clients[client_id].conn_mutex));
                return;
            } else if (mtype == TASK_RESULT) {
                conn -> state = RECEIVING;

                if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL) == -1) {
                    perror("register socket descriptor with epoll");
                    exit(EXIT_FAILURE);
                }

                struct epoll_event event;
                event.events = EPOLLIN;
                event.data = (epoll_data_t) -client_id;

                if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_ADD, conn -> socket_fd, &event) == -1) {
                    perror("register socket descriptor with epoll");
                    exit(EXIT_FAILURE);
                }
            } else {
                // TODO: handle pong and quit.
            }
        } else {
            printf("ERR IN STATE PROCESSING: buffer already allocated\n");
        }
    }

    pthread_mutex_unlock(&(conn -> conn_mutex));
}

void
handle_recv(int client_id, uint32_t events) {
    client_connection * conn = &(g_server_state.clients[client_id]);
    printf("LOG: HANDLING RECEIVING FOR: %s\n", conn -> name);

    if (conn -> recv_buffer == NULL) {
        // Allocate memory for header.
        conn -> recv_buffer = (unsigned char *) malloc(4 * sizeof(unsigned char));
        conn -> tb_received = 4;
        conn -> rec_buf_head = -1;  // another hack - need a state-specific struct in client_connection!
        pthread_mutex_unlock(&(conn -> conn_mutex));
        return;
    }

    if (conn -> rec_buf_head < 0) {
        // Still receiving header.
        printf("ON HEADER\n");
        if (conn -> rec_buf_head != -5) {
            int nr = recv(conn -> socket_fd,
                    conn -> recv_buffer - conn -> rec_buf_head - 1,
                    conn -> tb_received, MSG_DONTWAIT);
            conn -> tb_received -= nr;
            conn -> rec_buf_head -= nr;
        } else {
            // Received the whole header.
            printf("HEADER DONE\n");
            int tb_received = (int) (conn -> recv_buffer[2] << 8 | conn -> recv_buffer[3]) + 2;  // + 2 for task ID
            conn -> recv_buffer = realloc(conn -> recv_buffer, tb_received);
            conn -> tb_received = tb_received - 2;
            printf("TB: %d\n", conn -> tb_received);
            conn -> rec_buf_head = 0;
        }
    } else {
        // Header received, buffer allocated - go!
        int nr = recv(conn -> socket_fd,
                conn -> recv_buffer + 2 + conn -> rec_buf_head,
                conn -> tb_received, MSG_DONTWAIT);
        conn -> rec_buf_head += nr;
        conn -> tb_received -= nr;
        if (conn -> tb_received == 0) {
            // Got the whole result - print it out and free the memory.
            int task_id = (int) (conn -> recv_buffer[0] << 8 | conn -> recv_buffer[1]);
            printf("RECEIVED TASK %d\n%s", task_id, conn -> recv_buffer + 2);
            free(conn -> recv_buffer);
            conn -> recv_buffer = NULL;

            // Change state appropriately.
            if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_DEL, conn -> socket_fd, NULL) == -1) {
                perror("register socket descriptor with epoll");
                exit(EXIT_FAILURE);
            }

            if (conn -> q_head != conn -> q_tail || conn -> q_full) {
                conn -> state = WORKING;

                struct epoll_event event;
                event.events = EPOLLIN | EPOLLOUT;
                event.data = (epoll_data_t) -client_id;

                if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_ADD, conn -> socket_fd, &event) == -1) {
                    perror("register socket descriptor with epoll");
                    exit(EXIT_FAILURE);
                }
            } else {
                conn -> state = FREE;

                struct epoll_event event;
                event.events = EPOLLIN;
                event.data = (epoll_data_t) -client_id;

                if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_ADD, conn -> socket_fd, &event) == -1) {
                    perror("register socket descriptor with epoll");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    pthread_mutex_unlock(&(conn -> conn_mutex));
}

void
evict(int client_id) {
    client_connection * conn = &(g_server_state.clients[client_id]);
    printf("LOG: HANDLING EVICTION FOR: %s\n", conn -> name);

    // Drop fully initialized connection.
    pthread_mutex_lock(&(conn -> conn_mutex));

    epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_ADD, conn -> socket_fd, NULL);
    conn -> socket_fd = -1;

    free(conn -> recv_buffer);
    conn -> tb_received = conn -> inbuf_pending = 0;
    conn -> state = INITIAL;

    // TODO: assign tasks to other clients!
    for (; conn -> q_full || conn -> q_head != conn -> q_tail;
         conn -> q_head = (conn -> q_head + 1) % CLIENT_TASK_MAX) {
        if (conn -> q_full) {
            conn -> q_full = 0;
        }

        // FUCK. THAT. SHIT.
        free(&(conn -> task_queue[conn -> q_head]));
    }

    conn -> q_head = conn -> q_tail = 0;
    pthread_mutex_unlock(&(conn -> conn_mutex));
}

void *
event_loop(void * arg) {
    struct epoll_event socket_events[MAX_CLIENTS + 2];

    // Accept incoming connections.
    while (1) {
        printf("IN!\n");
        int i, epoll_count = epoll_wait(g_server_state.active_sockets, socket_events, MAX_CLIENTS + 2, -1);
        printf("OUT!\n");

        for (i = 0; i < epoll_count; i++) {
            int fd = socket_events[i].data.fd;

            if (fd == g_server_state.net_socket || fd == g_server_state.local_socket) {
                int client_socket = accept(fd, NULL, NULL);
                if (client_socket == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                pthread_mutex_lock(&(g_server_state.count_mutex));
                if (g_server_state.clients_count == MAX_CLIENTS) {
                    printf("Connection rejected!\n");
                    close(client_socket);
                    pthread_mutex_unlock(&(g_server_state.count_mutex));
                    continue;
                } else {
                    g_server_state.clients_count += 1;
                    pthread_mutex_unlock(&(g_server_state.count_mutex));
                }

                int j;
                for (j = 0; j < MAX_CLIENTS; j++) {
                    if (g_server_state.clients[j].socket_fd == -1) {
                        g_server_state.clients[j].socket_fd = client_socket;
                        break;
                    }
                }

                struct epoll_event event;
                event.events = EPOLLIN | EPOLLOUT;
                event.data = (epoll_data_t) -j;  // a hack
                if (epoll_ctl(g_server_state.active_sockets, EPOLL_CTL_ADD, client_socket, &event) == -1) {
                    perror("register client socket descriptor with epoll");
                    exit(EXIT_FAILURE);
                }

                client_connection * conn = &(g_server_state.clients[j]);
                pthread_mutex_lock(&(conn -> conn_mutex));
                handle_initial(j);
            } else {
                client_connection * conn = &(g_server_state.clients[-fd]);

                char mtype;
                if ((socket_events[i].events | EPOLLIN) == EPOLLIN) {
                    if (recv(conn -> socket_fd, &mtype, 1, MSG_PEEK) == 0) {
                        fprintf(stderr, "CONNECTION LOST FOR %s!\n", conn -> name);
                        pthread_mutex_lock(&(conn -> conn_mutex));
                        close(conn -> socket_fd);
                        conn -> socket_fd = -1;
                        // TODO: depends on state - free memory etc.
                        pthread_mutex_unlock(&(conn -> conn_mutex));
                        continue;
                    }
                }

                // Handle client socket events.
                pthread_mutex_lock(&(conn -> conn_mutex));

                switch (conn -> state) {
                    case INITIAL: handle_initial(-fd); break;
                    case FREE: handle_free(-fd, socket_events[i].events); break;
                    case WORKING: handle_working(-fd, socket_events[i].events); break;
                    case TRANSMITTING: handle_trans(-fd, socket_events[i].events); break;
                    case PROCESSING: handle_proc(-fd, socket_events[i].events); break;
                    case RECEIVING: handle_recv(-fd, socket_events[i].events); break;
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
            if (g_line[nr - 1] == '\n') {
                g_line[nr - 1] = '\0';
            }
            post_task(g_line, &g_server_state);
        }
    }

    cleanup();
    exit(EXIT_SUCCESS);
}