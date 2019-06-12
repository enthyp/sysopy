#ifndef PROTOCOL_H
#define PROTOCOL_H

// Message types - sent by clients.
#define REGISTER 00
#define TASK_RESULT 01
#define PONG 02
#define UNREGISTER 03

// Message types - sent by server.
#define OK_REGISTER 010
#define NAME_EXISTS 011
#define TASK 012
#define PING 013

#endif // PROTOCOL_H
