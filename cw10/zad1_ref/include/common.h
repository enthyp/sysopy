#ifndef COMMON_H
#define COMMON_H

// Miscellaneous.

#define MIN(a,b) ((a) < (b)) ? (a) : (b)
#define MAX(a,b) ((a) < (b)) ? (b) : (a)

#define UNIX_PATH_MAX 108                   // Max length of Unix socket name.
#define CLIENT_NAME_MAX 10                  // Max client name length

#endif // COMMON_H
