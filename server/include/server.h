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
// COSTANTI SERVER
// ============================================================================

// Stati del client
typedef enum {
    CLIENT_DISCONNECTED = 0,    // Disconnesso
    CLIENT_CONNECTED = 1,       // Connesso ma non registrato
    CLIENT_REGISTERED = 2,      // Registrato, non in partita
    CLIENT_IN_GAME = 3,         // Attualmente in partita
    CLIENT_WAITING_ACCEPT = 4   // In attesa di accept per join
} client_status_t;

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
    uint32_t seq_id;                    // Sequence ID per messaggi
    pthread_t thread_id;                // ID del thread che gestisce questo client
} client_info_t;

/**
 * Informazioni su ogni partita attiva
 */
typedef struct {
    game_state_t game;                  // Stato del gioco (da game_logic.h)
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

// Inizializzazione stato server (per array clients e games)
void init_server_state();

// Inizializzazione e avvio server
int init_server(int port);
void start_server(int server_fd);

// Thread handler per ogni client
void *handle_client(void *arg);

// Funzioni di utilità per le porte
int bind_to_available_port(int server_fd, struct sockaddr_in *address, int start_port);

// ============================================================================
// FUNZIONI DI GESTIONE CLIENT
// ============================================================================

/**
 * Trova un client per file descriptor
 * @return Indice nell'array clients, o -1 se non trovato
 * NOTA: Richiede che server_state.mutex sia già acquisito dal chiamante
 */
int find_client_by_fd(int fd);

/**
 * Trova un client per nome
 * @return Indice nell'array clients, o -1 se non trovato
 * NOTA: Richiede che server_state.mutex sia già acquisito dal chiamante
 */
int find_client_by_name(const char *name);

/**
 * Aggiunge un nuovo client
 * @return Indice nell'array clients, o -1 se array pieno
 * NOTA: Richiede che server_state.mutex sia già acquisito dal chiamante
 */
int add_client(int fd, pthread_t thread_id);

/**
 * Rimuove un client e fa cleanup
 * NOTA: Richiede che server_state.mutex sia già acquisito dal chiamante
 */
void remove_client(int fd);

// ============================================================================
// FUNZIONI DI GESTIONE PARTITE
// ============================================================================

/**
 * Trova una partita per game_id
 * @return Puntatore alla partita o NULL se non trovata
 */
game_session_t* find_game_by_id(const char *game_id);

/**
 * Trova la partita di un client
 * @return Puntatore alla partita o NULL se il client non è in partita
 */
game_session_t* find_game_by_client_fd(int fd); //NOTE: not used

/**
 * Crea una nuova partita
 * @return Puntatore alla partita creata o NULL se array pieno
 */
game_session_t* create_game(const char *creator_name, int creator_fd);

/**
 * Pulisce una partita terminata
 */
void cleanup_game(game_session_t *game);

// ============================================================================
// HANDLER MESSAGGI PROTOCOLLO
// ============================================================================

void handle_register(int client_fd, const void *payload, uint16_t length);
void handle_create_game(int client_fd);
void handle_list_games(int client_fd);
void handle_join_game(int client_fd, const void *payload, uint16_t length);
void handle_accept_join(int client_fd, const void *payload, uint16_t length);
void handle_make_move(int client_fd, const void *payload, uint16_t length);
void handle_leave_game(int client_fd);
void handle_quit(int client_fd);

// ============================================================================
// FUNZIONI DI NOTIFICA
// ============================================================================

/**
 * Invia notifica a tutti i client registrati (ma non in partita)
 */
void broadcast_to_registered_clients(uint8_t msg_type, const void *payload, size_t payload_size);

/**
 * Notifica al creatore che qualcuno vuole joinare
 */
void notify_player_joined(int creator_fd, const char *joiner_name);

/**
 * Notifica al joiner se è stato accettato o rifiutato
 */
void notify_join_response(int joiner_fd, const char *game_id, int accepted);

/**
 * Notifica a entrambi i giocatori l'inizio della partita
 */
void notify_game_start(game_session_t *game);

#endif