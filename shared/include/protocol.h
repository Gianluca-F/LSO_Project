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
// STATI DELLA CONNESSIONE CLIENT-SERVER
// ============================================================================

typedef enum {
    CLIENT_DISCONNECTED = 0,      // Non connesso
    CLIENT_CONNECTED = 1,         // Connesso ma non autenticato
    CLIENT_REGISTERED = 2,        // Registrato, disponibile per giocare
    CLIENT_IN_LOBBY = 3,          // Ha creato una partita, attende richiesta di join
    CLIENT_REQUESTING_JOIN = 4,   // Ha richiesto join, attende accept/reject
    CLIENT_IN_GAME = 5            // In partita attiva
} client_status_t;

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
    ERR_REQUEST_PENDING = 3,
    ERR_NO_PENDING_JOIN = 4,
    ERR_PENDING_JOIN_EXISTS = 5,
    ERR_NOT_IN_LOBBY = 6,
    ERR_ALREADY_IN_GAME = 7,
    ERR_NOT_IN_GAME = 8,
    ERR_NOT_YOUR_TURN = 9,
    ERR_INVALID_MOVE = 10,
    ERR_CELL_OCCUPIED = 11,
    ERR_NOT_REGISTERED = 20,
    ERR_ALREADY_REGISTERED = 21,
    ERR_INVALID_NAME = 22,
    ERR_NAME_TAKEN = 23,
    ERR_SERVER_FULL = 90,
    ERR_INVALID_PAYLOAD = 91,
    ERR_INTERNAL = 99,
} error_code_t;

// Risposta generica
typedef struct __attribute__((packed)) {
    uint8_t status;         // STATUS_OK o STATUS_ERROR
    uint8_t error_code;     // error_code_t (se status == STATUS_ERROR)
} response_generic_t;

// Risposta a MSG_REGISTER (alias per chiarezza)
typedef response_generic_t response_register_t;

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
    uint8_t your_symbol;            // 'X' o 'O' (se OK)
    char opponent[MAX_PLAYER_NAME]; // Nome avversario (se OK)
    char game_id[MAX_GAME_ID_LEN];  // ID della partita (se OK)
} response_join_game_t;

// Risposta a MSG_ACCEPT_JOIN (alias per chiarezza)
typedef response_generic_t response_accept_join_t;

// Risposta a MSG_MAKE_MOVE (alias per chiarezza)
typedef response_generic_t response_make_move_t;

// Risposta a MSG_LEAVE_GAME (alias per chiarezza)
typedef response_generic_t response_leave_game_t;

// Risposta a MSG_NEW_GAME
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
    char game_id[MAX_GAME_ID_LEN];  // ID della nuova partita (se OK)
} response_new_game_t;

// Risposta a MSG_QUIT (alias per chiarezza)
typedef response_generic_t response_quit_t;

// ============================================================================
// PAYLOADS: SERVER -> CLIENT - NOTIFICHE
// ============================================================================

// Tipi di notifica
typedef enum {
    NOTIFY_GAME_CREATED = 100,   // Partita creata
    NOTIFY_JOIN_REQUEST = 101,   // Richiesta di join da un giocatore
    NOTIFY_JOIN_RESPONSE = 102,  // Risposta a richiesta di join (accettata/rifiutata)
    NOTIFY_GAME_START = 103,     // Partita iniziata
    NOTIFY_MOVE_MADE = 104,      // L'avversario ha fatto una mossa (equivalente a YOUR_TURN)
    NOTIFY_GAME_END = 105,       // Partita terminata
    NOTIFY_OPPONENT_LEFT = 106,  // Avversario ha abbandonato
} notify_type_t;

// NOTIFY_GAME_CREATED: Partita creata (messaggio broadcast a tutti i client registrati)
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_GAME_CREATED
    char game_id[MAX_GAME_ID_LEN];
    char creator[MAX_PLAYER_NAME];
} notify_game_created_t;

// NOTIFY_JOIN_REQUEST: Richiesta di join da un giocatore
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_JOIN_REQUEST
    char opponent[MAX_PLAYER_NAME];
} notify_join_request_t;

// NOTIFY_JOIN_RESPONSE: Risposta a richiesta di join
typedef struct __attribute__((packed)) {
    uint8_t notify_type;    // NOTIFY_JOIN_RESPONSE
    uint8_t accepted;       // 1 = accettato, 0 = rifiutato
    char game_id[MAX_GAME_ID_LEN];
} notify_join_response_t;

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
 * 
 * Imposta i campi dell'header e converte automaticamente
 * length e seq_id in network byte order.
 * 
 * @param header Puntatore all'header da inizializzare
 * @param msg_type Tipo di messaggio (MSG_*)
 * @param length Lunghezza del payload in bytes
 * @param seq_id ID sequenziale del messaggio
 */
void protocol_init_header(protocol_header_t *header, uint8_t msg_type, 
                         uint16_t length, uint32_t seq_id);

/**
 * Converte header da host a network byte order
 * 
 * Converte i campi multi-byte (length, seq_id) utilizzando
 * htons/htonl per garantire compatibilità di rete.
 * 
 * @param header Puntatore all'header da convertire
 */
void protocol_header_to_network(protocol_header_t *header);

/**
 * Converte header da network a host byte order
 * 
 * Converte i campi multi-byte (length, seq_id) utilizzando
 * ntohs/ntohl dopo la ricezione dalla rete.
 * 
 * @param header Puntatore all'header da convertire
 */
void protocol_header_to_host(protocol_header_t *header);

// ============================================================================
// FUNZIONI DI INVIO/RICEZIONE
// ============================================================================

/**
 * Invia un messaggio completo (header + payload)
 * 
 * Crea e inizializza l'header del protocollo, lo invia insieme
 * al payload opzionale. Gestisce automaticamente la conversione
 * in network byte order e l'invio atomico.
 * 
 * @param sockfd File descriptor del socket
 * @param msg_type Tipo di messaggio (MSG_*)
 * @param payload Puntatore al payload (NULL se nessun payload)
 * @param payload_size Dimensione del payload in bytes
 * @param seq_id ID sequenziale del messaggio
 * @return Numero di byte inviati totali (header + payload), -1 se errore
 */
ssize_t protocol_send(int sockfd, uint8_t msg_type, const void *payload, 
                     size_t payload_size, uint32_t seq_id);

/**
 * Riceve l'header di un messaggio
 * 
 * Legge esattamente sizeof(protocol_header_t) bytes dal socket
 * e converte automaticamente in host byte order.
 * 
 * @param sockfd File descriptor del socket
 * @param header Puntatore all'header dove salvare i dati ricevuti
 * @return sizeof(protocol_header_t) se successo, -1 se errore, 0 se connessione chiusa
 */
ssize_t protocol_recv_header(int sockfd, protocol_header_t *header);

/**
 * Riceve il payload di un messaggio
 * 
 * Legge esattamente 'length' bytes dal socket nel buffer fornito.
 * Deve essere chiamata dopo protocol_recv_header() per leggere
 * il payload indicato dall'header.
 * 
 * @param sockfd File descriptor del socket
 * @param buffer Buffer dove salvare il payload ricevuto
 * @param length Numero di bytes da ricevere (da header.length)
 * @return Numero di byte ricevuti, -1 se errore
 */
ssize_t protocol_recv_payload(int sockfd, void *buffer, size_t length);

// ============================================================================
// FUNZIONI DI VALIDAZIONE
// ============================================================================

/**
 * Valida un nome giocatore
 * 
 * Verifica che il nome non sia vuoto, non contenga caratteri
 * non validi e rispetti la lunghezza massima.
 * 
 * @param name Nome giocatore da validare
 * @return 1 se valido, 0 altrimenti
 */
int protocol_validate_name(const char *name);

/**
 * Valida una posizione di mossa
 * 
 * Verifica che la posizione sia compresa tra 1 e 9 (inclusi).
 * 
 * @param pos Posizione della mossa (1-9)
 * @return 1 se valida, 0 altrimenti
 */
int protocol_validate_move(uint8_t pos);

/**
 * Valida un ID di partita
 * 
 * Verifica che il game_id non sia vuoto e rispetti
 * la lunghezza massima.
 * 
 * @param game_id ID della partita da validare
 * @return 1 se valido, 0 altrimenti
 */
int protocol_validate_game_id(const char *game_id);

// ============================================================================
// FUNZIONI DI DEBUG
// ============================================================================

/**
 * Restituisce il nome testuale del tipo di messaggio
 * 
 * Converte i codici MSG_* in stringhe leggibili per logging.
 * 
 * @param msg_type Codice del tipo di messaggio
 * @return Stringa descrittiva (es. "MSG_REGISTER", "MSG_RESPONSE")
 */
const char* protocol_msg_type_str(uint8_t msg_type);

/**
 * Restituisce il nome testuale del tipo di notifica
 * 
 * Converte i codici NOTIFY_* in stringhe leggibili per logging.
 * 
 * @param notify_type Codice del tipo di notifica
 * @return Stringa descrittiva (es. "NOTIFY_GAME_START")
 */
const char* protocol_notify_type_str(uint8_t notify_type);

/**
 * Restituisce la descrizione testuale di un codice di errore
 * 
 * Converte i codici ERR_* in messaggi leggibili dall'utente.
 * 
 * @param error Codice di errore (error_code_t)
 * @return Stringa descrittiva dell'errore
 */
const char* protocol_error_str(error_code_t error);

/**
 * Stampa un header del protocollo per debug
 * 
 * Visualizza tutti i campi dell'header in formato leggibile
 * con informazioni sul tipo di messaggio e lunghezza payload.
 * 
 * @param header Puntatore all'header da stampare
 */
void protocol_print_header(const protocol_header_t *header);

#endif // PROTOCOL_H