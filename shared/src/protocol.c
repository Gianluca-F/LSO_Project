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

void protocol_init_header(protocol_header_t *header, uint8_t msg_type, 
                         uint16_t length, uint32_t seq_id) {
    if (!header) return;
    
    header->msg_type = msg_type;
    header->reserved = 0;
    header->length = htons(length);      // Convert to network byte order
    header->seq_id = htonl(seq_id);      // Convert to network byte order
}

void protocol_header_to_network(protocol_header_t *header) {
    if (!header) return;
    
    header->length = htons(header->length);
    header->seq_id = htonl(header->seq_id);
}

void protocol_header_to_host(protocol_header_t *header) {
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

int protocol_validate_move(uint8_t row, uint8_t col) {
    // For tictactoe, valid positions are 1-9
    // This function validates if we're using row/col system (0-2 each)
    return (row < BOARD_SIZE && col < BOARD_SIZE);
}

int protocol_validate_game_id(const char *game_id) {
    if (!game_id) return 0;
    
    size_t len = strlen(game_id);
    
    // Check length
    if (len == 0 || len >= MAX_GAME_ID_LEN) {
        return 0;
    }
    
    // Check for valid characters (alphanumeric + underscore + hyphen)
    for (size_t i = 0; i < len; i++) {
        if (!isalnum(game_id[i]) && game_id[i] != '_' && game_id[i] != '-') {
            return 0;
        }
    }
    
    return 1;
}

// ============================================================================
// FUNZIONI DI DEBUG
// ============================================================================

const char* protocol_msg_type_str(uint8_t msg_type) {
    switch (msg_type) {
        // Client -> Server messages
        case MSG_REGISTER:      return "MSG_REGISTER";
        case MSG_CREATE_GAME:   return "MSG_CREATE_GAME";
        case MSG_LIST_GAMES:    return "MSG_LIST_GAMES";
        case MSG_JOIN_GAME:     return "MSG_JOIN_GAME";
        case MSG_ACCEPT_JOIN:   return "MSG_ACCEPT_JOIN";
        case MSG_MAKE_MOVE:     return "MSG_MAKE_MOVE";
        case MSG_LEAVE_GAME:    return "MSG_LEAVE_GAME";
        case MSG_NEW_GAME:      return "MSG_NEW_GAME";
        case MSG_QUIT:          return "MSG_QUIT";
        
        // Server -> Client messages
        case MSG_RESPONSE:      return "MSG_RESPONSE";
        case MSG_NOTIFY:        return "MSG_NOTIFY";
        
        default:                return "UNKNOWN_MSG_TYPE";
    }
}

const char* protocol_notify_type_str(uint8_t notify_type) {
    switch (notify_type) {
        case NOTIFY_GAME_CREATED:   return "NOTIFY_GAME_CREATED";
        case NOTIFY_PLAYER_JOINED:  return "NOTIFY_PLAYER_JOINED";
        case NOTIFY_GAME_START:     return "NOTIFY_GAME_START";
        case NOTIFY_YOUR_TURN:      return "NOTIFY_YOUR_TURN";
        case NOTIFY_MOVE_MADE:      return "NOTIFY_MOVE_MADE";
        case NOTIFY_GAME_END:       return "NOTIFY_GAME_END";
        case NOTIFY_OPPONENT_LEFT:  return "NOTIFY_OPPONENT_LEFT";
        
        default:                    return "UNKNOWN_NOTIFY_TYPE";
    }
}

const char* protocol_error_str(error_code_t error) {
    switch (error) {
        case ERR_NONE:              return "No error";
        case ERR_GAME_NOT_FOUND:    return "Game not found";
        case ERR_GAME_FULL:         return "Game is full";
        case ERR_ALREADY_IN_GAME:   return "Already in a game";
        case ERR_NOT_IN_GAME:       return "Not in a game";
        case ERR_NOT_YOUR_TURN:     return "Not your turn";
        case ERR_INVALID_MOVE:      return "Invalid move";
        case ERR_CELL_OCCUPIED:     return "Cell already occupied";
        case ERR_SERVER_FULL:       return "Server is full";
        case ERR_INTERNAL:          return "Internal server error";
        
        default:                    return "Unknown error";
    }
}

void protocol_print_header(const protocol_header_t *header) {
    if (!header) {
        printf("Header: NULL\n");
        return;
    }
    
    printf("Header: type=%s(%d), reserved=%d, length=%d, seq_id=%u\n",
           protocol_msg_type_str(header->msg_type),
           header->msg_type,
           header->reserved,
           header->length,
           header->seq_id);
}