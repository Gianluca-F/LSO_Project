#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include "../../shared/include/constants.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/game_logic.h"
#include "utils.h"

// ============================================================================
// STRUTTURE DATI SERVER
// ============================================================================

/**
 * Informazioni su ogni client connesso
 */
typedef struct {
    int fd;                             // Socket file descriptor
    char name[MAX_PLAYER_NAME];         // Nome giocatore (se registrato)
    client_status_t status;             // Stato corrente del client
    int game_index;                     // Indice in games[] (-1 se non in partita)
    int player_index;                   // 0 o 1 nella partita (quale giocatore è)

    //NOTE: Potrebbero essere aggiunti altri campi in futuro
    //uint32_t seq_id;                  // Sequence ID per messaggi
    //pthread_t thread_id;              // ID del thread che gestisce questo client
} client_info_t;

/**
 * Informazioni su ogni partita attiva
 */
typedef struct {
    game_state_t state;                 // Stato del gioco (da game_logic.h)
    int player_fds[2];                  // Socket dei due giocatori [0]=creatore, [1]=joiner
    int active;                         // 1 se partita attiva, 0 se slot libero
    
    // Gestione pending join (giocatore in attesa di accept)
    int pending_join_fd;                // FD del giocatore che vuole joinare (-1 se nessuno)
    char pending_join_name[MAX_PLAYER_NAME]; // Nome del giocatore in attesa
} game_session_t;

/**
 * Stato globale del server
 */
typedef struct {
    client_info_t *clients;             // Array dinamico di client (allocato con malloc)
    game_session_t *games;              // Array dinamico di partite (allocato con malloc)
    int max_clients;                    // Capacità massima client (da config)
    int max_games;                      // Capacità massima partite (da config)
    int num_clients;                    // Numero di client attualmente connessi
    int num_games;                      // Numero di partite attualmente attive
    pthread_mutex_t mutex;              // Mutex per proteggere lo stato condiviso
} server_state_t;

// Stato globale del server (dichiarato extern, definito in server.c)
extern server_state_t server_state;

// ============================================================================
// FUNZIONI PER LA GESTIONE DEL SERVER
// ============================================================================

/**
 * Inizializza lo stato globale del server
 * 
 * Alloca dinamicamente gli array per client e partite in base
 * ai limiti configurati (max_clients, max_games).
 */
void init_server_state();

/**
 * Inizializza e configura il socket del server
 * 
 * Crea il socket, configura le opzioni (SO_REUSEADDR), effettua
 * il bind su una porta disponibile (con fallback automatico) e
 * si mette in ascolto.
 * 
 * @param port Porta desiderata per il server
 * @return File descriptor del socket server, o -1 in caso di errore
 */
int init_server(int port);

/**
 * Avvia il server e gestisce le connessioni client
 * 
 * Loop infinito che accetta connessioni, crea un thread per ogni
 * client connesso e fa il detach del thread.
 * 
 * @param server_fd File descriptor del socket server
 */
void start_server(int server_fd);

/**
 * Thread handler per ogni client connesso
 * 
 * Gestisce l'intero ciclo di vita del client: ricezione messaggi,
 * dispatch agli handler appropriati, cleanup alla disconnessione.
 * 
 * @param arg Puntatore a int contenente il file descriptor del client
 * @return NULL
 */
void *handle_client(void *arg);

/**
 * Tenta il bind su porte successive fino a trovarne una libera
 * 
 * @param server_fd File descriptor del socket server
 * @param address Struttura sockaddr_in da configurare
 * @param starting_port Prima porta da provare
 * @return Porta utilizzata con successo, o -1 se nessuna porta disponibile
 */
int bind_to_available_port(int server_fd, struct sockaddr_in *address, int starting_port);

// ============================================================================
// FUNZIONI DI GESTIONE CLIENT
// ============================================================================

/**
 * Trova un client per file descriptor
 * 
 * @param fd File descriptor da cercare
 * @return Indice nell'array clients, o -1 se non trovato
 * @note Richiede che server_state.mutex sia già acquisito dal chiamante
 */
int find_client_by_fd(int fd);

/**
 * Trova un client per nome giocatore
 * 
 * @param name Nome del giocatore da cercare
 * @return Indice nell'array clients, o -1 se non trovato
 * @note Richiede che server_state.mutex sia già acquisito dal chiamante
 */
int find_client_by_name(const char *name);

/**
 * Aggiunge un nuovo client all'array
 * 
 * Usa num_clients come indice diretto (O(1)) e inizializza
 * lo stato del client a CLIENT_CONNECTED.
 * 
 * @param fd File descriptor del socket client
 * @return Indice nell'array clients, o -1 se array pieno
 * @note Richiede che server_state.mutex sia già acquisito dal chiamante
 */
int add_client(int fd);

/**
 * Rimuove un client e fa cleanup completo
 * 
 * Se il client è in una partita, notifica l'avversario e pulisce
 * la partita. Usa swap con l'ultimo elemento per rimozione O(1).
 * 
 * @param fd File descriptor del client da rimuovere
 * @note Richiede che server_state.mutex sia già acquisito dal chiamante
 */
void remove_client(int fd);

// ============================================================================
// FUNZIONI DI GESTIONE PARTITE
// ============================================================================

/**
 * Trova una partita per game_id
 * 
 * @param game_id ID univoco della partita da cercare
 * @return Indice nell'array games, o -1 se non trovata
 * @note Richiede che server_state.mutex sia già acquisito dal chiamante
 */
int find_game_by_id(const char *game_id);

/**
 * Trova la partita di un client dato il suo FD
 * 
 * @param fd File descriptor del client
 * @return Indice nell'array games, o -1 se il client non è in partita
 * @note Richiede che server_state.mutex sia già acquisito dal chiamante
 */
int find_game_by_client_fd(int fd); //NOTE: not used

/**
 * Crea una nuova partita
 * 
 * Genera un game_id univoco, inizializza lo stato di gioco e
 * imposta il creatore come player 0.
 * 
 * @param creator_name Nome del giocatore creatore
 * @param creator_fd File descriptor del creatore
 * @return Indice nell'array games, o -1 se array pieno
 * @note Richiede che server_state.mutex sia già acquisito dal chiamante
 */
int create_game(const char *creator_name, int creator_fd);

/**
 * Pulisce una partita terminata
 * 
 * Resetta lo stato dei client coinvolti a CLIENT_REGISTERED,
 * marca la partita come non attiva e decrementa il contatore.
 * 
 * @param game Puntatore alla partita da pulire
 */
void cleanup_game(game_session_t *game);

// ============================================================================
// HANDLER MESSAGGI PROTOCOLLO
// ============================================================================

/**
 * Handler per MSG_REGISTER - Registrazione giocatore
 * 
 * @param client_fd File descriptor del client
 * @param payload Puntatore a payload_register_t
 * @param length Lunghezza del payload in bytes
 */
void handle_register(int client_fd, const void *payload, uint16_t length);

/**
 * Handler per MSG_CREATE_GAME - Creazione nuova partita
 * 
 * @param client_fd File descriptor del client creatore
 */
void handle_create_game(int client_fd);

/**
 * Handler per MSG_LIST_GAMES - Lista partite disponibili
 * 
 * @param client_fd File descriptor del client richiedente
 */
void handle_list_games(int client_fd);

/**
 * Handler per MSG_JOIN_GAME - Richiesta join a partita
 * 
 * @param client_fd File descriptor del client che vuole joinare
 * @param payload Puntatore a payload_join_game_t
 * @param length Lunghezza del payload in bytes
 */
void handle_join_game(int client_fd, const void *payload, uint16_t length);

/**
 * Handler per MSG_ACCEPT_JOIN - Accetta/rifiuta join
 * 
 * @param client_fd File descriptor del creatore della partita
 * @param payload Puntatore a payload_accept_join_t
 * @param length Lunghezza del payload in bytes
 */
void handle_accept_join(int client_fd, const void *payload, uint16_t length);

/**
 * Handler per MSG_MAKE_MOVE - Esegui mossa
 * 
 * @param client_fd File descriptor del giocatore
 * @param payload Puntatore a payload_make_move_t
 * @param length Lunghezza del payload in bytes
 */
void handle_make_move(int client_fd, const void *payload, uint16_t length);

/**
 * Handler per MSG_LEAVE_GAME - Abbandona partita
 * 
 * @param client_fd File descriptor del giocatore che abbandona
 */
void handle_leave_game(int client_fd);

/**
 * Handler per MSG_QUIT - Disconnessione client
 * 
 * @param client_fd File descriptor del client che si disconnette
 */
void handle_quit(int client_fd);

// ============================================================================
// FUNZIONI DI NOTIFICA
// ============================================================================

/**
 * Invia notifica broadcast a tutti i client registrati
 * 
 * Invia solo ai client con status CLIENT_REGISTERED (non in partita).
 * 
 * @param msg_type Tipo di messaggio (es. MSG_NOTIFY)
 * @param payload Puntatore ai dati da inviare
 * @param payload_size Dimensione del payload in bytes
 */
void broadcast_to_registered_clients(uint8_t msg_type, const void *payload, size_t payload_size);

/**
 * Notifica al creatore che qualcuno vuole joinare
 * 
 * @param creator_fd File descriptor del creatore della partita
 * @param joiner_name Nome del giocatore che vuole joinare
 */
void notify_join_request(int creator_fd, const char *joiner_name);

/**
 * Notifica al joiner se è stato accettato o rifiutato
 * 
 * @param joiner_fd File descriptor del giocatore che ha richiesto join
 * @param game_id ID della partita
 * @param accepted 1 se accettato, 0 se rifiutato
 */
void notify_join_response(int joiner_fd, const char *game_id, int accepted);

/**
 * Notifica a entrambi i giocatori l'inizio della partita
 * 
 * Invia NOTIFY_GAME_START con il simbolo assegnato e nome avversario.
 * 
 * @param game Puntatore alla partita che sta iniziando
 */
void notify_game_start(game_session_t *game);

#endif