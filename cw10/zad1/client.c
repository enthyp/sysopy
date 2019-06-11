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
#include <sys/wait.h>

char * TMP_FILE = "tmp";
char g_filepath[CLIENT_NAME_MAX + 3 + 2];
FILE * g_file;

char * g_line = NULL;

int g_socket = -1;
char g_buffer[BUFFER_SIZE];
pthread_mutex_t g_transmitter_mutex = PTHREAD_MUTEX_INITIALIZER;


void
sigint_handler(int sig) {
    pthread_mutex_lock(&g_transmitter_mutex);
    char mtype = UNREGISTER;
    send(g_socket, &mtype, 1, MSG_DONTWAIT);
    free(g_line);
    exit(EXIT_SUCCESS);
}

void *
processing(void * args) {
    sprintf(g_buffer, "./cnt_occ.sh %s", g_filepath);
    FILE * script_input = popen(g_buffer, "r");
    if (script_input == NULL) {
        perror("FORK FAILED");
    }
    pclose(script_input);

    size_t len = 0;
    int total = 0, count;
    g_file = fopen(g_filepath, "r");
    fseek(g_file, 0, SEEK_SET);
    while (getline(&g_line, &len, g_file) > 0) {
        sscanf(g_line, "%d %s", &count, g_line);  // implicit assumption about max word length...
        total += count;
    }

    fclose(g_file);
    printf("RESULT: %d\n", total);
    // TODO: send back
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
    while ((bytes_read += read(g_socket, g_buffer, 2 - bytes_read)) < 2) ;

    int task_id = (int) ((g_buffer[0] << 8) | g_buffer[1]);
    printf("TASK ID: %d\n", task_id);

    while ((bytes_read += read(g_socket, g_buffer, 4 - bytes_read)) < 4) ;
    int task_size = (int) ((g_buffer[0] << 8) | g_buffer[1]);
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
        read(g_socket, &mtype, 1);

        switch (mtype) {
            case TASK: handle_task(); break;
            case PING: handle_ping(); break;
            default: fprintf(stderr, "PROTOCOL BREACH!\n"); close(g_socket); exit(EXIT_FAILURE);
        }
    }
}

int
main(int argc, char * argv[]) {
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Pass client name, local/net and UNIX socket path or IP with port number, respectively.\n");
        exit(EXIT_FAILURE);
    }

    char name[CLIENT_NAME_MAX];
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
    } else if (strcmp(argv[2], "net")) {
        if (argc != 5) {
            fprintf(stderr, "local requires 4 arguments!\n");
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
