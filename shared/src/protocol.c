#include "protocol.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// FUNZIONI DI UTILITÀ - HEADER
// ============================================================================

inline void protocol_init_header(protocol_header_t *header, uint8_t msg_type, 
                                uint16_t length, uint32_t seq_id) {
    if (!header) return;
    
    header->msg_type = msg_type;
    header->length = htons(length);
    header->seq_id = htonl(seq_id);
}

inline void protocol_header_to_network(protocol_header_t *header) {
    if (!header) return;
    
    header->length = htons(header->length);
    header->seq_id = htonl(header->seq_id);
}

inline void protocol_header_to_host(protocol_header_t *header) {
    if (!header) return;
    
    header->length = ntohs(header->length);
    header->seq_id = ntohl(header->seq_id);
}

// ============================================================================
// FUNZIONI DI INVIO/RICEZIONE
// ============================================================================

ssize_t protocol_send(int sockfd, uint8_t msg_type, const void *payload, 
                     size_t payload_size, uint32_t seq_id) {
    protocol_header_t header;
    ssize_t total_sent = 0;
    ssize_t bytes_sent;
    
    // Initialize header
    protocol_init_header(&header, msg_type, (uint16_t)payload_size, seq_id);
    
    // Send header
    bytes_sent = send(sockfd, &header, sizeof(protocol_header_t), 0);
    if (bytes_sent <= 0) {
        return -1;
    }
    total_sent += bytes_sent;
    
    // Send payload if present
    if (payload && payload_size > 0) {
        bytes_sent = send(sockfd, payload, payload_size, 0);
        if (bytes_sent <= 0) {
            return -1;
        }
        total_sent += bytes_sent;
    }
    
    return total_sent;
}

ssize_t protocol_recv_header(int sockfd, protocol_header_t *header) {
    if (!header) return -1;
    
    ssize_t bytes_received = 0;
    size_t total_received = 0;
    char *buffer = (char*)header;
    
    // Receive header completely
    while (total_received < sizeof(protocol_header_t)) {
        bytes_received = recv(sockfd, buffer + total_received, 
                             sizeof(protocol_header_t) - total_received, 0);
        
        if (bytes_received <= 0) {
            return bytes_received; // Error or connection closed
        }
        
        total_received += bytes_received;
    }
    
    // Convert from network to host byte order
    protocol_header_to_host(header);
    
    return total_received;
}

ssize_t protocol_recv_payload(int sockfd, void *buffer, size_t length) {
    if (!buffer || length == 0) return 0;
    
    ssize_t bytes_received = 0;
    size_t total_received = 0;
    char *buf = (char*)buffer;
    
    // Receive payload completely
    while (total_received < length) {
        bytes_received = recv(sockfd, buf + total_received, 
                             length - total_received, 0);
        
        if (bytes_received <= 0) {
            return bytes_received; // Error or connection closed
        }
        
        total_received += bytes_received;
    }
    
    return total_received;
}

// ============================================================================
// FUNZIONI DI VALIDAZIONE
// ============================================================================

int protocol_validate_name(const char *name) {
    if (!name) return 0;
    
    size_t len = strlen(name);
    
    // Check length
    if (len == 0 || len >= MAX_PLAYER_NAME) {
        return 0;
    }
    
    // Check for valid characters (alphanumeric + underscore)
    for (size_t i = 0; i < len; i++) {
        if (!isalnum(name[i]) && name[i] != '_') {
            return 0;
        }
    }
    
    return 1;
}

inline int protocol_validate_move(uint8_t pos) {
    // For tictactoe, valid positions are 1-9
    return (pos >= 1 && pos <= 9);
}
