#ifndef CLIENT_H
#define CLIENT_H

#include <pthread.h>
#include <stdbool.h>
#include "../../shared/include/constants.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/game_logic.h"
#include "utils.h"

// Stato della connessione del client
typedef enum {
    CLIENT_DISCONNECTED,
    CLIENT_CONNECTED,
    CLIENT_REGISTERED,
    CLIENT_IN_LOBBY,
    CLIENT_IN_GAME,
    CLIENT_WAITING_ACCEPT
} client_connection_state_t;

// Struttura che rappresenta lo stato del client
typedef struct {
    int socket_fd;                          // File descriptor del socket
    char username[MAX_PLAYER_NAME];         // Nome utente del client
    client_connection_state_t state;        // Stato della connessione
    
    // Informazioni sulla partita corrente
    char current_game_id[MAX_GAME_ID_LEN];  // ID della partita corrente
    char my_symbol;                         // Il mio simbolo ('X' o 'O')
    game_state_t local_game_state;          // Copia locale dello stato del gioco
    
    // Thread e sincronizzazione
    pthread_t notification_thread;          // Thread per ricevere notifiche
    pthread_mutex_t mutex;                  // Mutex per proteggere lo stato
    bool running;                           // Flag per terminare il thread
    
    // Sequenza messaggi
    uint32_t seq_id;                        // ID sequenziale per i messaggi
    uint8_t last_request_type;              // Ultimo tipo di richiesta inviata
} client_state_t;

// Variabile globale dello stato del client
extern client_state_t client_state;

// === Funzioni di connessione ===
int client_connect(const char *host, int port);
void client_disconnect(void);

// === Funzioni per inviare richieste al server ===
int send_register_request(const char *username);
int send_create_game_request(void);
int send_list_games_request(void);
int send_join_game_request(const char *game_id);
int send_accept_join_request(void);
int send_make_move_request(int row, int col);
int send_leave_game_request(void);
int send_quit_request(void);

// === Thread per gestire le notifiche ===
void *notification_thread_func(void *arg);

// === Gestori delle notifiche ===
void handle_game_created_notification(const notify_game_created_t *notify);
void handle_join_request_notification(const notify_join_request_t *notify);
void handle_join_response_notification(const notify_join_response_t *notify);
void handle_game_start_notification(const notify_game_start_t *notify);
void handle_move_made_notification(const notify_move_made_t *notify);
void handle_game_over_notification(const notify_game_end_t *notify);

// === Menu interattivo ===
void client_run(void);

#endif 