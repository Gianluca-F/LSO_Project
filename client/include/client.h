#ifndef CLIENT_H
#define CLIENT_H

#include <pthread.h>
#include <stdbool.h>
#include "../../shared/include/constants.h"
#include "../../shared/include/protocol.h"
#include "../../shared/include/game_logic.h"
#include "utils.h"

// ============================================================================
// STRUTTURE DATI CLIENT
// ============================================================================

/**
 * Stato globale del client
 * 
 * Contiene tutte le informazioni necessarie per gestire la connessione
 * al server, lo stato della partita corrente e la sincronizzazione thread.
 */
typedef struct {
    int socket_fd;                          // File descriptor del socket
    char username[MAX_PLAYER_NAME];         // Nome utente del client
    client_status_t state;                  // Stato della connessione
    
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
    int last_move_pos;                      // Ultima posizione mossa inviata (1-9)
} client_state_t;

// Stato globale del client (dichiarato extern, definito in client.c)
extern client_state_t client_state;

// ============================================================================
// FUNZIONI DI CONNESSIONE
// ============================================================================

/**
 * Connette il client al server
 * 
 * Crea un socket TCP, si connette all'indirizzo specificato e
 * inizializza lo stato del client a CLIENT_CONNECTED.
 * 
 * @param host Indirizzo IP o hostname del server
 * @param port Porta del server
 * @return 0 se successo, -1 se errore
 */
int client_connect(const char *host, int port);

/**
 * Disconnette il client dal server
 * 
 * Chiude il socket, ferma il thread di notifiche e resetta
 * lo stato del client a CLIENT_DISCONNECTED.
 */
void client_disconnect(void);

// ============================================================================
// FUNZIONI PER INVIARE RICHIESTE AL SERVER
// ============================================================================

/**
 * Invia richiesta di registrazione al server
 * 
 * @param username Nome utente da registrare (validato con protocol_validate_name)
 * @return 0 se successo, -1 se errore
 */
int send_register_request(const char *username);

/**
 * Invia richiesta di creazione nuova partita
 * 
 * @return 0 se successo, -1 se errore
 */
int send_create_game_request(void);

/**
 * Invia richiesta per ottenere la lista delle partite disponibili
 * 
 * @return 0 se successo, -1 se errore
 */
int send_list_games_request(void);

/**
 * Invia richiesta per unirsi a una partita esistente
 * 
 * @param game_id ID univoco della partita a cui unirsi
 * @return 0 se successo, -1 se errore
 */
int send_join_game_request(const char *game_id);

/**
 * Invia risposta a una richiesta di join (accetta o rifiuta)
 * 
 * @param accept true per accettare, false per rifiutare
 * @return 0 se successo, -1 se errore
 */
int send_accept_join_request(bool accept);

/**
 * Invia una mossa di gioco al server
 * 
 * @param pos Posizione della mossa (1-9)
 * @return 0 se successo, -1 se errore
 */
int send_make_move_request(int pos);

/**
 * Invia richiesta per abbandonare la partita corrente
 * 
 * @return 0 se successo, -1 se errore
 */
int send_leave_game_request(void);

/**
 * Invia richiesta di disconnessione dal server
 * 
 * @return 0 se successo, -1 se errore
 */
int send_quit_request(void);

// ============================================================================
// THREAD PER NOTIFICHE ASINCRONE
// ============================================================================

/**
 * Thread che riceve messaggi dal server in background
 * 
 * Loop che riceve continuamente header e payload dal server,
 * gestisce risposte sincrone e notifiche asincrone, e aggiorna
 * lo stato del client di conseguenza.
 * 
 * @param arg Non utilizzato (NULL)
 * @return NULL
 */
void *notification_thread_func(void *arg);

// ============================================================================
// GESTORI DELLE NOTIFICHE
// ============================================================================

/**
 * Gestisce notifica di partita creata con successo
 * 
 * @param notify Puntatore alla notifica NOTIFY_GAME_CREATED
 */
void handle_game_created_notification(const notify_game_created_t *notify);

/**
 * Gestisce notifica di richiesta di join da un altro giocatore
 * 
 * @param notify Puntatore alla notifica NOTIFY_JOIN_REQUEST
 */
void handle_join_request_notification(const notify_join_request_t *notify);

/**
 * Gestisce notifica di cancellazione della richiesta di join
 * 
 * @param notify Puntatore alla notifica NOTIFY_JOIN_CANCELLATION
 */
void handle_join_cancellation_notification(const notify_join_cancellation_t *notify);

/**
 * Gestisce risposta alla propria richiesta di join (accettato/rifiutato)
 * 
 * @param notify Puntatore alla notifica NOTIFY_JOIN_RESPONSE
 */
void handle_join_response_notification(const notify_join_response_t *notify);

/**
 * Gestisce notifica di inizio partita
 * 
 * Inizializza lo stato locale del gioco, imposta i giocatori
 * e il simbolo assegnato, e visualizza il tabellone iniziale.
 * 
 * @param notify Puntatore alla notifica NOTIFY_GAME_START
 */
void handle_game_start_notification(const notify_game_start_t *notify);

/**
 * Gestisce notifica di mossa effettuata
 * 
 * Aggiorna il tabellone locale con la nuova mossa e visualizza
 * lo stato aggiornato della partita.
 * 
 * @param notify Puntatore alla notifica NOTIFY_MOVE_MADE
 */
void handle_move_made_notification(const notify_move_made_t *notify);

/**
 * Gestisce notifica di fine partita
 * 
 * Visualizza il risultato finale (vittoria/sconfitta/pareggio)
 * e resetta lo stato del client a CLIENT_REGISTERED.
 * 
 * @param notify Puntatore alla notifica NOTIFY_GAME_END
 */
void handle_game_over_notification(const notify_game_end_t *notify);

/**
 * Gestisce notifica di abbandono partita da parte dell'avversario
 * 
 * Notifica l'utente che l'avversario ha lasciato la partita
 * e resetta lo stato del client a CLIENT_REGISTERED.
 * 
 */
void handle_opponent_left_notification(const notify_opponent_left_t *notify);

// ============================================================================
// MENU INTERATTIVO
// ============================================================================

/**
 * Loop principale del client con menu interattivo
 * 
 * Gestisce l'input dell'utente, esegue i comandi disponibili
 * e coordina l'interazione con il server.
 */
void client_run(void);

#endif 