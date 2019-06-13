#ifndef PROTOCOL_H
#define PROTOCOL_H

// Task ID encoded on 2 bytes.
#define ID_BYTES 2

// Task length encoded on 4 bytes.
#define LEN_BYTES 4

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

// Turns an integer into sequence of num_bytes bytes (disregards its actual size).
int serialize(unsigned char * head, int to_serialize, int num_bytes);

// Turns a sequence of num_bytes bytes into an integer.
int deserialize(unsigned char * head, int * result, int num_bytes);

#endif // PROTOCOL_H
