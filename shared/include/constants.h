#ifndef CONSTANTS_H
#define CONSTANTS_H

// Configurazioni di rete
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 8080
#define BACKLOG_SIZE 10

// Buffer sizes
#define BUFFER_SIZE 1024
#define MAX_FILENAME_LENGTH 256

// Timeout
#define CONNECTION_TIMEOUT 30
#define READ_TIMEOUT 10

// Return codes
#define SUCCESS 0
#define ERROR_SOCKET -1
#define ERROR_BIND -2
#define ERROR_LISTEN -3
#define ERROR_ACCEPT -4
#define ERROR_CONNECT -5

#endif
