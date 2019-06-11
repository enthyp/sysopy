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
#include "protocol.h"

int g_socket = -1;

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

    connect(g_socket, address, sizeof(*address));

    // Open communication.
    strncpy(name, argv[1], CLIENT_NAME_MAX);
    char mtype = 0;
    write(g_socket, &mtype, 1);
    write(g_socket, name, CLIENT_NAME_MAX);

    read(g_socket, &mtype, 1);
    if (mtype == OK_REGISTER) {
        printf("REGISTERED!\n");
    } else if (mtype == NAME_EXISTS){
        printf("NAME EXISTS!\n");
    } else {
        printf("PROTOCOL BREACH\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
