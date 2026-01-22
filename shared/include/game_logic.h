#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "constants.h"  

// ============================================================================
// STRUTTURE DATI DI GIOCO
// ============================================================================

/**
 * Struttura per rappresentare una partita
 */
typedef struct {
    char game_id[MAX_GAME_ID_LEN];      // ID univoco della partita
    char players[2][MAX_PLAYER_NAME];   // Nomi dei due giocatori
    char board[9];                      // Tabellone Tris 3x3, posizioni 0-8
    int current_player;                 // 0 o 1 (indice nel array players)
    int status;                         // GAME_WAITING, GAME_IN_PROGRESS, GAME_FINISHED
    int move_count;                     // Numero mosse effettuate
    int winner;                         // -1=nessuno, 0=player[0], 1=player[1], 2=pareggio
} game_state_t;

// ============================================================================
// FUNZIONI DI INIZIALIZZAZIONE
// ============================================================================

/**
 * Inizializza una nuova partita
 * 
 * Crea una nuova partita con l'ID specificato, imposta il giocatore
 * creatore e inizializza il tabellone vuoto.
 * 
 * @param game Puntatore alla struttura game_state_t da inizializzare
 * @param game_id ID univoco della partita
 * @param creator Nome del giocatore che crea la partita
 */
void game_init(game_state_t *game, const char *game_id, const char *creator);

/**
 * Aggiunge il secondo giocatore alla partita
 * 
 * @param game Puntatore alla struttura game_state_t
 * @param player_name Nome del giocatore che si unisce
 * @return 1 se successo, 0 se partita già piena
 */
int game_add_player(game_state_t *game, const char *player_name);

// ============================================================================
// FUNZIONI DI MOVIMENTO
// ============================================================================

/**
 * Effettua una mossa sulla griglia
 * 
 * Verifica che la mossa sia valida (cella libera, turno corretto)
 * e aggiorna lo stato del gioco.
 * 
 * @param game Puntatore alla struttura game_state_t
 * @param player_idx 0 o 1 (indice del giocatore)
 * @param position 1-9 (posizione sulla griglia)
 * @return 1 se mossa valida, 0 altrimenti
 */
int game_make_move(game_state_t *game, int player_idx, int position);

// ============================================================================
// FUNZIONI DI STATO
// ============================================================================

/**
 * Controlla se c'è un vincitore
 * 
 * Verifica tutte le combinazioni vincenti (righe, colonne, diagonali)
 * e determina se la partita è finita in pareggio.
 * 
 * @param game Puntatore alla struttura game_state_t
 * @return -1=nessuno, 0=player[0], 1=player[1], 2=pareggio
 */
int game_check_winner(const game_state_t *game);

/**
 * Verifica se la partita è terminata
 * 
 * @param game Puntatore alla struttura game_state_t
 * @return 1 se terminata, 0 altrimenti
 */
int game_is_finished(const game_state_t *game);

/**
 * Verifica se è il turno del giocatore specificato
 * 
 * @param game Puntatore alla struttura game_state_t
 * @param player_name Nome del giocatore da verificare
 * @return 1 se è il suo turno, 0 altrimenti
 */
int game_is_player_turn(const game_state_t *game, const char *player_name);

// ============================================================================
// FUNZIONI DI UTILITÀ
// ============================================================================

/**
 * Ottiene il simbolo del giocatore ('X' o 'O')
 * 
 * @param game Puntatore alla struttura game_state_t
 * @param player_idx Indice del giocatore (0 o 1)
 * @return 'X' per player[0], 'O' per player[1]
 */
char game_get_player_symbol(const game_state_t *game, int player_idx);

/**
 * Stampa il tabellone a terminale con formattazione ANSI
 * 
 * Visualizza la griglia 3x3 con i simboli X e O colorati e
 * i numeri delle posizioni disponibili.
 * 
 * @param game Puntatore alla struttura game_state_t
 */
void game_print_board(const game_state_t *game);

/**
 * Copia lo stato del tabellone in un array di caratteri
 * 
 * Utilizzata per serializzare lo stato del tabellone nel protocollo
 * di comunicazione client-server.
 * 
 * @param game Puntatore alla struttura game_state_t
 * @param board_str Array di 9 caratteri dove copiare il tabellone
 */
void game_get_board_string(const game_state_t *game, char board_str[9]);

#endif 