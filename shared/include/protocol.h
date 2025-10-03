#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "constants.h"
#include <stdint.h>
#include <sys/types.h>

// ============================================================================
// HEADER DEL PROTOCOLLO (8 bytes fissi)
// ============================================================================

typedef struct __attribute__((packed)) {
    uint8_t msg_type;       // Tipo di messaggio (vedi sotto)
    uint8_t reserved;       // Riservato per uso futuro
    uint16_t length;        // Lunghezza del payload (network byte order)
    uint32_t seq_id;        // ID sequenza messaggio (network byte order)
} protocol_header_t;

// ============================================================================
// STRUTTURE DATI AUSILIARIE
// ============================================================================

typedef struct __attribute__((packed)) {
    char game_id[MAX_GAME_ID_LEN];          // ID univoco della partita
    char creator[MAX_PLAYER_NAME];          // Nome del creatore
    uint8_t status;                         // Stato: GAME_WAITING, GAME_IN_PROGRESS, etc.
    uint8_t players_count;                  // Numero giocatori attuali (0-2)
} game_info_t;

// ============================================================================
// TIPI DI MESSAGGIO
// ============================================================================

// Messaggi Client -> Server
#define MSG_REGISTER        1   // Registra giocatore
#define MSG_CREATE_GAME     2   // Crea partita
#define MSG_LIST_GAMES      3   // Lista partite disponibili
#define MSG_JOIN_GAME       4   // Unisciti a partita
#define MSG_ACCEPT_JOIN     5   // Accetta richiesta di join
#define MSG_MAKE_MOVE       6   // Fai mossa
#define MSG_LEAVE_GAME      7   // Abbandona partita
#define MSG_NEW_GAME        8   // Richiesta nuova partita
#define MSG_QUIT            9   // Disconnessione client

// Messaggi Server -> Client
#define MSG_RESPONSE        50  // Risposta generica
#define MSG_NOTIFY          51  // Notifica asincrona

// ============================================================================
// PAYLOADS: CLIENT -> SERVER
// ============================================================================

// MSG_REGISTER: Registrazione giocatore
typedef struct __attribute__((packed)) {
    char player_name[MAX_PLAYER_NAME];
} payload_register_t;

// MSG_CREATE_GAME: Creazione partita (no payload, solo header)

// MSG_LIST_GAMES: Richiesta lista partite (no payload, solo header)

// MSG_JOIN_GAME: Unisciti a partita
typedef struct __attribute__((packed)) {
    char game_id[MAX_GAME_ID_LEN];
} payload_join_game_t;

// MSG_ACCEPT_JOIN: Accetta richiesta di join
typedef struct __attribute__((packed)) {
    uint8_t accept;     // 1 = accetta, 0 = rifiuta
} payload_accept_join_t;

// MSG_MAKE_MOVE: Esegui mossa
typedef struct __attribute__((packed)) {
    uint8_t pos;    // 1-9
} payload_make_move_t;

// MSG_LEAVE_GAME: Abbandona partita (no payload, solo header)

// MSG_NEW_GAME: Richiesta nuova partita (no payload, solo header)

// MSG_QUIT: Disconnessione client (no payload, solo header)

// ============================================================================
// PAYLOADS: SERVER -> CLIENT - RISPOSTE
// ============================================================================

// Codici di stato
typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR = 1
} response_status_t;

// Codici di errore
typedef enum {
    ERR_NONE = 0,
    ERR_GAME_NOT_FOUND = 1,
    ERR_GAME_FULL = 2,
    ERR_ALREADY_IN_GAME = 3,
    ERR_NOT_IN_GAME = 4,
    ERR_NOT_YOUR_TURN = 5,
    ERR_INVALID_MOVE = 6,
    ERR_CELL_OCCUPIED = 7,
    ERR_SERVER_FULL = 8,
    ERR_INTERNAL = 99,
} error_code_t;

// Risposta a MSG_REGISTER
typedef struct __attribute__((packed)) {
    uint8_t status;         // STATUS_OK o STATUS_ERROR
    uint8_t error_code;     // error_code_t (se status == STATUS_ERROR)
} response_register_t;

// Risposta a MSG_CREATE_GAME
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
    char game_id[MAX_GAME_ID_LEN];  // ID della partita creata (se OK)
} response_create_game_t;

// Risposta a MSG_LIST_GAMES
// La dimensione totale del payload è: sizeof(response_list_games_t) + (game_count * sizeof(game_info_t))
typedef struct __attribute__((packed)) {
    uint8_t status;                         // STATUS_OK o STATUS_ERROR
    uint8_t error_code;                     // error_code_t (se status == STATUS_ERROR)
    uint8_t game_count;                     // Numero di partite nella lista
    uint8_t reserved;                       // Padding per allineamento
    // Seguito da: game_info_t games[game_count] (array dinamico)
} response_list_games_t;

// Risposta a MSG_JOIN_GAME
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
    uint8_t your_symbol;    // 'X' o 'O' (se OK)
    char opponent[MAX_PLAYER_NAME];
} response_join_game_t;

// Risposta a MSG_MAKE_MOVE
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
} response_make_move_t;

// Risposta a MSG_LEAVE_GAME
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
} response_leave_game_t;

// Risposta a MSG_NEW_GAME
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
    char game_id[MAX_GAME_ID_LEN];  // ID della nuova partita (se OK)
} response_new_game_t;

// Risposta a MSG_QUIT
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
} response_quit_t;

// ============================================================================
// PAYLOADS: SERVER -> CLIENT - NOTIFICHE
// ============================================================================

// Tipi di notifica
typedef enum {
    NOTIFY_GAME_CREATED = 100,   // Partita creata
    NOTIFY_JOIN_RESPONSE = 101,  // Risposta a richiesta di join (accettata/rifiutata)
    NOTIFY_PLAYER_JOINED = 102,  // Secondo giocatore entrato
    NOTIFY_GAME_START = 103,     // Partita iniziata
    NOTIFY_YOUR_TURN = 104,      // È il tuo turno
    NOTIFY_MOVE_MADE = 105,      // L'avversario ha fatto una mossa
    NOTIFY_GAME_END = 106,       // Partita terminata
    NOTIFY_OPPONENT_LEFT = 107,  // Avversario ha abbandonato
} notify_type_t;

// NOTIFY_GAME_CREATED: Partita creata (messaggio broadcast a tutti i client registrati)
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_GAME_CREATED
    char game_id[MAX_GAME_ID_LEN];
    char creator[MAX_PLAYER_NAME];
} notify_game_created_t;

// NOTIFY_JOIN_RESPONSE: Risposta a richiesta di join
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_JOIN_RESPONSE
    uint8_t accepted;       // 1 = accettato, 0 = rifiutato
    char game_id[MAX_GAME_ID_LEN];
} notify_join_response_t;

// NOTIFY_PLAYER_JOINED: Secondo giocatore si è unito
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_PLAYER_JOINED
    char opponent[MAX_PLAYER_NAME];
} notify_player_joined_t;

// NOTIFY_GAME_START: La partita inizia
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_GAME_START
    uint8_t your_symbol;    // 'X' o 'O'
    uint8_t first_player;   // 'X' o 'O' (chi inizia)
    char opponent[MAX_PLAYER_NAME];
} notify_game_start_t;

// NOTIFY_YOUR_TURN: È il tuo turno
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_YOUR_TURN
    char board[BOARD_SIZE]; // Stato attuale ('X', 'O', '_')
} notify_your_turn_t;

// NOTIFY_MOVE_MADE: L'avversario ha mosso
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_MOVE_MADE
    uint8_t pos;            // 1-9
    uint8_t symbol;         // 'X' o 'O'
    char board[BOARD_SIZE]; // Stato aggiornato
} notify_move_made_t;

// NOTIFY_GAME_END: Partita terminata
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_GAME_END
    uint8_t result;         // RESULT_WIN, RESULT_LOSE, RESULT_DRAW
    char board[BOARD_SIZE]; // Stato finale
} notify_game_end_t;

// NOTIFY_OPPONENT_LEFT: Avversario ha abbandonato
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_OPPONENT_LEFT
} notify_opponent_left_t;

// ============================================================================
// FUNZIONI DI UTILITÀ - HEADER
// ============================================================================

/**
 * Inizializza un header del protocollo
 */
void protocol_init_header(protocol_header_t *header, uint8_t msg_type, 
                         uint16_t length, uint32_t seq_id);

/**
 * Converte header da host a network byte order
 */
void protocol_header_to_network(protocol_header_t *header);

/**
 * Converte header da network a host byte order
 */
void protocol_header_to_host(protocol_header_t *header);

// ============================================================================
// FUNZIONI DI INVIO/RICEZIONE
// ============================================================================

/**
 * Invia un messaggio completo (header + payload)
 * @return numero di byte inviati, -1 se errore
 */
ssize_t protocol_send(int sockfd, uint8_t msg_type, const void *payload, 
                     size_t payload_size, uint32_t seq_id);

/**
 * Riceve l'header di un messaggio
 * @return numero di byte ricevuti, -1 se errore, 0 se connessione chiusa
 */
ssize_t protocol_recv_header(int sockfd, protocol_header_t *header);

/**
 * Riceve il payload di un messaggio
 * @return numero di byte ricevuti, -1 se errore
 */
ssize_t protocol_recv_payload(int sockfd, void *buffer, size_t length);

// ============================================================================
// FUNZIONI DI VALIDAZIONE
// ============================================================================

/**
 * Valida un nome giocatore
 * @return 1 se valido, 0 altrimenti
 */
int protocol_validate_name(const char *name);

/**
 * Valida coordinate mossa
 * @return 1 se valide, 0 altrimenti
 */
int protocol_validate_move(uint8_t pos);

/**
 * Valida game_id
 * @return 1 se valido, 0 altrimenti
 */
int protocol_validate_game_id(const char *game_id);

// ============================================================================
// FUNZIONI DI DEBUG
// ============================================================================

/**
 * Restituisce il nome del tipo di messaggio
 */
const char* protocol_msg_type_str(uint8_t msg_type);

/**
 * Restituisce il nome del tipo di notifica
 */
const char* protocol_notify_type_str(uint8_t notify_type);

/**
 * Restituisce la descrizione di un errore
 */
const char* protocol_error_str(error_code_t error);

/**
 * Stampa un header per debug
 */
void protocol_print_header(const protocol_header_t *header);

#endif // PROTOCOL_H