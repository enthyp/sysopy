#include <stdio.h>
#include <unistd.h>
#include "initial.h"
#include "server.h"
#include "protocol.h"


// Prototypes defined in this file.
int check_name(server_state * state, int client_id, char * name);
void copy_str(unsigned char * from, char * to);
int initial_receive(handler_initial * self, server_state * state, int client_id);
int initial_send(handler_initial * self, server_state * state, int client_id);
int initial_close(handler_initial * self, server_state * state, int client_id);
int initial_to_free(handler_initial * self, server_state * state, int client_id);


int
initialize_handler(void * handler) {
    handler_initial * p_handler = (handler_initial *) handler;
    p_handler -> state = p_handler -> return_code = 0;
    p_handler -> tb_received = CLIENT_NAME_MAX;

    p_handler -> handle_receive = initial_receive;
    p_handler -> handle_send = initial_send;
    p_handler -> handle_closed = initial_close;

    return 0;
}

int
initial_receive(handler_initial * self, server_state * state, int client_id) {
    printf("LOG: INITIAL/RECEIVE FOR ID: %d\n", client_id);
    client_conn * conn = &(state -> clients[client_id].connection);

    if (self -> state == 0) {
        // First entry - check if message type OK.
        char mtype;
        read(conn -> socket_fd, &mtype, 1);
        if (mtype != REGISTER) {
            // Close connection and clean up - without responding.
            return initial_cleanup(self, state, client_id);
        } else {
            self -> state += 1;
        }
    }

    if (self -> state > 0) {
        // Subsequent entries - receive.
        int nr = recv(conn -> socket_fd, self -> receiver_buffer, self -> tb_received, MSG_DONTWAIT);
        self -> tb_received -= nr;

        if (self -> tb_received == 0) {
            // ...and we're yet to check it - check it.
            if (check_name(state, client_id, self -> receiver_buffer) == 0) {
                // Name ok!
                client * client = &(state -> clients[client_id]);
                copy_str(self -> receiver_buffer, client -> name);
                self -> return_code = OK_REGISTER;
            } else {
                self -> return_code = NAME_EXISTS;
            }

            // So we stop listening and start trying to self -> handle_send the return code.
            del_event(state, client_id);
            add_event(state, client_id, EPOLLOUT);
        }
    }
}

int
check_name(server_state * state, int client_id, char * name) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (i != client_id) {
            client * client = &(state -> clients[client_id]);

            if (client -> connection.socket_fd != -1 && strcmp(name, client -> name) == 0) {
                return -1;
            }
        }
    }

    return 0;
}

// No-warning version of strcpy from unsigned char to char.
void
copy_str(unsigned char * from, char * to) {
    int i;
    for (i = 0; from[i] != '\0'; i++) {
        to[i] = from[i];
    }
    to[i] = '\0';
}

int
initial_send(handler_initial * self, server_state * state, int client_id) {
    printf("LOG: INITIAL/SEND FOR ID: %d\n", client_id);

    if (self -> return_code == OK_REGISTER) {
        send(conn -> socket_fd, &(self -> return_code), 1, MSG_DONTWAIT);
        return initial_to_free(self, state, client_id);
    }

    if (self -> return_code == NAME_EXISTS) {
        // ...and it was taken but we didn't manage to respond - then respond.
        send(conn -> socket_fd, &(self -> return_code), 1, MSG_DONTWAIT);
        return initial_cleanup(self, state, client_id);
    }

    fprintf(stderr, "UNEXPECTED! INITIAL/SEND!!!");
    exit(EXIT_FAILURE);
}

int
initial_cleanup(handler_initial * self, server_state * state, int client_id) {
    client_conn * conn = &(state -> clients[client_id].connection);

    // Erase socket and its events.
    del_event(state, client_id);
    close(conn -> socket_fd);
    conn -> socket_fd = -1;

    // Decrement client count.
    pthread_mutex_lock(&(state -> count_mutex));
    state -> client_count -= 1;
    pthread_mutex_unlock(&(state -> count_mutex));

    // Reinitialize handler.
    self -> state = self -> return_code = 0;
    self -> tb_received = CLIENT_NAME_MAX;
    return 0;
}

int
initial_close(handler_initial * self, server_state * state, int client_id) {
    printf("LOG: INITIAL/CLOSE FOR ID: %d\n", client_id);
    return initial_cleanup(self, state, client_id);
}

int
initial_to_free(handler_initial * self, server_state * state, int client_id) {
    printf("LOG: INITIAL -> FREE FOR ID: %d\n", client_id);

}