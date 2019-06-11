#ifndef PROTOCOL_H
#define PROTOCOL_H

#define UNIX_PATH_MAX 108   // Max length of Unix socket name

#define BUFFER_SIZE 1024    // Transmitter buffer size (both sides)

#define CLIENT_NAME_MAX 10  // Max client name length
#define MAX_CLIENTS 15  // Max clients for server
#define CLIENT_TASK_MAX 10  // Size of (server-side) client task queue

// Messages sent by clients
#define REGISTER 00
#define TASK_RESULT 01
#define WORD_RESULT 02
#define PONG 03
#define UNREGISTER 04

// Messages sent by server
#define OK_REGISTER 00
#define NAME_EXISTS 01
#define TASK 02
#define PING 03

#endif // PROTOCOL_H
