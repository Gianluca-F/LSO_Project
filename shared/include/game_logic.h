#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "constants.h"

// Codici ANSI per formattazione output
#define BOLD "\x1b[1m"
#define RESET "\x1b[0m"
#define COLOR_RED "\x1b[31m"
#define COLOR_BLUE "\x1b[34m"

// Struttura per rappresentare una partita
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    char players[2][MAX_PLAYER_NAME];
    char board[9];          // Tris 3x3, posizioni 0-8
    int current_player;     // 0 o 1 (indice nel array players)
    int status;             // GAME_NEW, GAME_WAITING, GAME_IN_PROGRESS, GAME_FINISHED
    int move_count;         // Numero mosse effettuate
    int winner;             // -1=nessuno, 0=player[0], 1=player[1], 2=pareggio
} game_state_t;

// === FUNZIONI PURE DELLA LOGICA DI GIOCO ===

/**
 * Inizializza una nuova partita
 */
void game_init(game_state_t *game, const char *game_id, const char *creator);

/**
 * Aggiunge il secondo giocatore alla partita
 * @return 1 se successo, 0 se partita già piena
 */
int game_add_player(game_state_t *game, const char *player_name);

/**
 * Effettua una mossa
 * @param player_idx: 0 o 1 (quale giocatore)
 * @param position: 1-9 (posizione sulla griglia)
 * @return 1 se mossa valida, 0 altrimenti
 */
int game_make_move(game_state_t *game, int player_idx, int position);

/**
 * Controlla se c'è un vincitore
 * @return -1=nessuno, 0=player[0], 1=player[1], 2=pareggio
 */
int game_check_winner(const game_state_t *game);

/**
 * Verifica se la partita è terminata
 */
int game_is_finished(const game_state_t *game);

/**
 * Verifica se è il turno del giocatore specificato
 */
int game_is_player_turn(const game_state_t *game, const char *player_name);

/**
 * Ottiene il simbolo del giocatore ('X' o 'O')
 */
char game_get_player_symbol(const game_state_t *game, int player_idx);

/**
 * Stampa il tabellone
 */
void game_print_board(const game_state_t *game);

/**
 * Copia lo stato del tabellone in un array di caratteri per il protocollo
 */
void game_get_board_string(const game_state_t *game, char board_str[9]);

#endif // GAME_LOGIC_H
