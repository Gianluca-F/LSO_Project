#include "protocol.h"
#include <string.h>
#include <arpa/inet.h>

int serialize_message(const message_t *msg, char *buffer, size_t buffer_size) {
    if (!msg || !buffer || buffer_size < sizeof(message_t)) {
        return -1;
    }
    
    // Converti in network byte order
    uint32_t type = htonl((uint32_t)msg->type);
    uint32_t length = htonl(msg->length);
    
    size_t offset = 0;
    
    // Copia type
    memcpy(buffer + offset, &type, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    // Copia length
    memcpy(buffer + offset, &length, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    // Copia data
    memcpy(buffer + offset, msg->data, msg->length);
    
    return offset + msg->length;
}

int deserialize_message(const char *buffer, message_t *msg) {
    if (!buffer || !msg) {
        return -1;
    }
    
    size_t offset = 0;
    
    // Leggi type
    uint32_t type;
    memcpy(&type, buffer + offset, sizeof(uint32_t));
    msg->type = (message_type_t)ntohl(type);
    offset += sizeof(uint32_t);
    
    // Leggi length
    uint32_t length;
    memcpy(&length, buffer + offset, sizeof(uint32_t));
    msg->length = ntohl(length);
    offset += sizeof(uint32_t);
    
    // Verifica che la lunghezza sia valida
    if (msg->length > MAX_MESSAGE_SIZE) {
        return -1;
    }
    
    // Leggi data
    memcpy(msg->data, buffer + offset, msg->length);
    msg->data[msg->length] = '\0'; // Null terminate per sicurezza
    
    return offset + msg->length;
}
