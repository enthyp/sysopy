#ifndef SERVER_H
#define SERVER_H

#define CLIENT_NAME_MAX 10          // Max client name length
#define CLIENTS_MAX 15              // Max clients for server
#define CLIENT_TASK_MAX 10          // Size of (server-side) client task queue

#define LOCAL_SOCK CLIENTS_MAX      // Local socket ID
#define NET_SOCK CLIENTS_MAX + 1    // Net socket ID

#endif // SERVER_H
