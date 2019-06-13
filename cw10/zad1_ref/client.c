#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "protocol.h"
#include "common.h"
#include <sys/wait.h>

#define BUFFER_SIZE 1024

char * TMP_FILE = "tmp";
char g_filepath[CLIENT_NAME_MAX + 3 + 2];
FILE * g_file;
int g_task_id;

char * g_line = NULL;

int g_socket = -1;
unsigned char g_buffer[BUFFER_SIZE];
pthread_mutex_t g_transmitter_mutex = PTHREAD_MUTEX_INITIALIZER;


void
sigint_handler(int sig) {
    pthread_mutex_lock(&g_transmitter_mutex);
    char mtype = UNREGISTER;
    send(g_socket, &mtype, 1, MSG_DONTWAIT);
    free(g_line);
    exit(EXIT_SUCCESS);
}

void
send_results() {
    g_file = fopen(g_filepath, "r");
    if (g_file == NULL) {
        perror("OPEN FILE");
        exit(EXIT_FAILURE);
    }

    // Prepare message header: task ID, length of the output and then output
    int header_size = ID_BYTES + LEN_BYTES;
    int tb_transmitted;
    fseek(g_file, 0, SEEK_END);
    tb_transmitted = ftell(g_file) + header_size + 1; // + header + terminator
    fseek(g_file, 0, SEEK_SET);

    unsigned char * transmitter_buffer = (unsigned char *) malloc(tb_transmitted * sizeof(unsigned char));

    transmitter_buffer[0] = TASK_RESULT;
    serialize(transmitter_buffer + 1, g_task_id, ID_BYTES);
    serialize(transmitter_buffer + 1 + ID_BYTES, tb_transmitted - header_size - 1, LEN_BYTES);
    int a;
    deserialize(transmitter_buffer + 1, &a, ID_BYTES);
    printf("ID: %d\n", a);
    deserialize(transmitter_buffer + 1 + ID_BYTES, &a, LEN_BYTES);
    printf("SIZE: %d\n", a);
    fread(transmitter_buffer + header_size + 1, sizeof(unsigned char), tb_transmitted - header_size - 1, g_file);
    transmitter_buffer[tb_transmitted - 1] = '\0';
    pthread_mutex_lock(&g_transmitter_mutex);

    // Send it (can block).
    write(g_socket, transmitter_buffer, tb_transmitted);

    printf("DONE!\n");
    pthread_mutex_unlock(&g_transmitter_mutex);
    free(transmitter_buffer);
    fclose(g_file);
}

void *
processing(void * args) {
    char buffer[100];
    sprintf(buffer, "./script/cnt_occ.sh %s", g_filepath);
    FILE * script_input = popen(buffer, "r");
    if (script_input == NULL) {
        perror("FORK FAILED");
    }
    pclose(script_input);

    send_results();
    return NULL;
}

void
flush_buffer(int size) {
    int nw = 0;
    while ((nw += fwrite(g_buffer, sizeof(char), size - nw, g_file)) < size) ;
}

void
handle_task(void) {
    printf("LOG: HANDLING TASK\n");
    g_file = fopen(g_filepath, "w+");
    if (g_file == NULL) {
        perror("OPEN FILE");
        exit(EXIT_FAILURE);
    }

    // Read task ID, length of incoming input and then input.
    int bytes_read = 0;
    while ((bytes_read += read(g_socket, g_buffer, ID_BYTES - bytes_read)) < ID_BYTES);

    deserialize(g_buffer, &g_task_id, ID_BYTES);
    printf("TASK ID: %d\n", g_task_id);

    bytes_read = 0;
    while ((bytes_read += read(g_socket, g_buffer, LEN_BYTES - bytes_read)) < LEN_BYTES);
    int task_size;
    deserialize(g_buffer, &task_size, LEN_BYTES);
    printf("TASK SIZE: %d\n", task_size);

    int in_buffer = bytes_read = 0;

    while (bytes_read < task_size) {
        int b_read = read(g_socket, g_buffer, MIN(task_size - bytes_read, BUFFER_SIZE - in_buffer));
        bytes_read += b_read;
        in_buffer += b_read;

        if (in_buffer == BUFFER_SIZE) {
            flush_buffer(BUFFER_SIZE);
            in_buffer = 0;
        }
    }

    if (in_buffer > 0) {
        flush_buffer(in_buffer);
    }

    fflush(g_file);
    fsync(fileno(g_file));
    fclose(g_file);

    // So now task content is in the file - let another thread process it as we go back
    // to respond to some pings.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t processing_thread;
    pthread_create(&processing_thread, &attr, processing, NULL);
}

void
handle_ping(void) {
    printf("LOG: HANDLING PING\n");
    pthread_mutex_lock(&g_transmitter_mutex);

    char mtype = PONG;
    write(g_socket, &mtype, 1);

    pthread_mutex_unlock(&g_transmitter_mutex);
}

void
work() {
    while(1) {
        char mtype;
        if (read(g_socket, &mtype, 1) == 0) {
            fprintf(stderr, "Connection lost!\n");
            exit(EXIT_FAILURE);
        }

        switch (mtype) {
            case TASK: handle_task(); break;
            case PING: handle_ping(); break;
            default: fprintf(stderr, "PROTOCOL BREACH! %d received\n", mtype); close(g_socket); exit(EXIT_FAILURE);
        }
    }
}

int
main(int argc, char * argv[]) {
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Pass client name, local/net and UNIX socket path or IP with port number, respectively.\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr * address;

    if (strcmp(argv[2], "local") == 0) {
        if (argc != 4) {
            fprintf(stderr, "local requires 3 arguments!\n");
            exit(EXIT_FAILURE);
        }

        g_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(g_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, argv[3], UNIX_PATH_MAX);
        address = (struct sockaddr *) &addr;
    } else if (strcmp(argv[2], "net") == 0) {
        if (argc != 5) {
            fprintf(stderr, "net requires 4 arguments!\n");
            exit(EXIT_FAILURE);
        }

        int port;
        struct in_addr addr;
        if (inet_aton(argv[3], &addr) == 0) {
            fprintf(stderr, "Incorrect IP!\n");
            exit(EXIT_FAILURE);
        }
        if (sscanf(argv[4], "%d", &port) == 0) {
            fprintf(stderr, "Incorrect port!\n");
            exit(EXIT_FAILURE);
        }

        g_socket = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(g_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
        struct sockaddr_in net_addr;
        net_addr.sin_family = AF_INET;
        net_addr.sin_addr = addr;
        net_addr.sin_port = htons(port);
        address = (struct sockaddr *) &net_addr;
    } else {
        fprintf(stderr, "Choose local/net as the second argument!\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);
    connect(g_socket, address, sizeof(*address));

    // Open communication.
    char name[CLIENT_NAME_MAX];
    strcpy(name, argv[1]);
    strcpy(g_filepath, TMP_FILE);
    strcat(g_filepath, name);

    char mtype = 0;
    write(g_socket, &mtype, 1);
    write(g_socket, name, CLIENT_NAME_MAX);

    read(g_socket, &mtype, 1);
    if (mtype == OK_REGISTER) {
        printf("REGISTERED!\n");
    } else if (mtype == NAME_EXISTS){
        printf("NAME EXISTS!\n");
        exit(EXIT_FAILURE);
    } else {
        printf("PROTOCOL BREACH!\n");
        exit(EXIT_FAILURE);
    }

    work();

    return 0;
}
