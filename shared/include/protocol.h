#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// Costanti del protocollo
#define MAX_MESSAGE_SIZE 1024
#define DEFAULT_PORT 8080
#define MAX_CLIENTS 100

// Tipi di messaggio
typedef enum {
    MSG_CONNECT = 1,
    MSG_DISCONNECT,
    MSG_REQUEST,
    MSG_RESPONSE,
    MSG_ERROR
} message_type_t;

// Struttura messaggio base
typedef struct {
    message_type_t type;
    uint32_t length;
    char data[MAX_MESSAGE_SIZE];
} message_t;

// Funzioni del protocollo
int serialize_message(const message_t *msg, char *buffer, size_t buffer_size);
int deserialize_message(const char *buffer, message_t *msg);

#endif
