#include "game_logic.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// FUNZIONI DI INIZIALIZZAZIONE
// ============================================================================

void game_init(game_state_t *game, const char *game_id, const char *creator) {
    if (!game || !game_id || !creator) return;
    
    // Inizializza i campi base
    strncpy(game->game_id, game_id, MAX_GAME_ID_LEN - 1);
    game->game_id[MAX_GAME_ID_LEN - 1] = '\0';
    
    strncpy(game->players[0], creator, MAX_PLAYER_NAME - 1);
    game->players[0][MAX_PLAYER_NAME - 1] = '\0';
    game->players[1][0] = '\0';  // Secondo giocatore vuoto
    
    // Inizializza il tabellone (tutte le posizioni vuote)
    for (int i = 0; i < 9; i++) {
        game->board[i] = EMPTY_CELL;
    }
    
    game->current_player = 0;   // Il creatore inizia sempre
    game->status = GAME_WAITING;
    game->move_count = 0;
    game->winner = -1;          // Nessun vincitore
}

int game_add_player(game_state_t *game, const char *player_name) {
    if (!game || !player_name) return 0;
    
    // Controlla se c'è già un secondo giocatore
    if (game->players[1][0] != '\0') {
        return 0;  // Partita già piena
    }
    
    // Controlla che non sia lo stesso nome del creatore
    if (strcmp(game->players[0], player_name) == 0) {
        return 0;  // Stesso nome del creatore
    }
    
    // Aggiunge il secondo giocatore
    strncpy(game->players[1], player_name, MAX_PLAYER_NAME - 1);
    game->players[1][MAX_PLAYER_NAME - 1] = '\0';
    
    // Cambia stato a "in corso"
    game->status = GAME_IN_PROGRESS;
    
    return 1;  // Successo
}

// ============================================================================
// FUNZIONI DI MOVIMENTO
// ============================================================================

int game_make_move(game_state_t *game, int player_idx, int position) {
    if (!game) return 0;
    
    // Validazioni di base
    if (player_idx < 0 || player_idx > 1) return 0;
    if (position < 1 || position > 9) return 0;
    if (game->status != GAME_IN_PROGRESS) return 0;
    if (player_idx != game->current_player) return 0;  // Non è il suo turno
    
    // Converte posizione 1-9 in indice 0-8
    int board_idx = position - 1;
    
    // Controlla se la cella è libera
    if (game->board[board_idx] != EMPTY_CELL) {
        return 0;  // Cella già occupata
    }
    
    // Effettua la mossa
    char symbol = game_get_player_symbol(game, player_idx);
    game->board[board_idx] = symbol;
    game->move_count++;
    
    // Controlla se c'è un vincitore
    int winner = game_check_winner(game);
    if (winner != -1) {
        game->winner = winner;
        game->status = GAME_FINISHED;
    } else if (game->move_count == 9) {
        // Pareggio - tabellone pieno
        game->winner = 2;  // Indica pareggio
        game->status = GAME_FINISHED;
    } else {
        // Passa il turno all'altro giocatore
        game->current_player = 1 - game->current_player;
    }
    
    return 1;  // Mossa valida effettuata
}

// ============================================================================
// CONTROLLO VINCITORE
// ============================================================================

int game_check_winner(const game_state_t *game) {
    if (!game) return -1;
    
    char board[9];
    memcpy(board, game->board, 9);
    
    // Combinazioni vincenti (righe, colonne, diagonali)
    int winning_combinations[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8},  // Righe
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8},  // Colonne
        {0, 4, 8}, {2, 4, 6}              // Diagonali
    };
    
    // Controlla ogni combinazione vincente
    for (int i = 0; i < 8; i++) {
        int pos1 = winning_combinations[i][0];
        int pos2 = winning_combinations[i][1];
        int pos3 = winning_combinations[i][2];
        
        if (board[pos1] != EMPTY_CELL &&
            board[pos1] == board[pos2] &&
            board[pos2] == board[pos3]) {
            
            // Trova quale giocatore ha vinto
            char winning_symbol = board[pos1];
            for (int player = 0; player < 2; player++) {
                if (game_get_player_symbol(game, player) == winning_symbol) {
                    return player;
                }
            }
        }
    }
    
    return -1;  // Nessun vincitore
}

// ============================================================================
// FUNZIONI DI STATO
// ============================================================================

int game_is_finished(const game_state_t *game) {
    if (!game) return 0;
    return (game->status == GAME_FINISHED);
}

int game_is_player_turn(const game_state_t *game, const char *player_name) {
    if (!game || !player_name) return 0;
    if (game->status != GAME_IN_PROGRESS) return 0;
    
    return (strcmp(game->players[game->current_player], player_name) == 0);
}

char game_get_player_symbol(const game_state_t *game, int player_idx) {
    if (!game || player_idx < 0 || player_idx > 1) return '\0';
    
    // Il giocatore 0 (creatore) è sempre 'X', il giocatore 1 è sempre 'O'
    return (player_idx == 0) ? PLAYER_X : PLAYER_O;
}

// ============================================================================
// FUNZIONI DI UTILITÀ
// ============================================================================

void game_print_board(const game_state_t *game) {
    if (!game) {
        printf("Game is NULL\n");
        return;
    }
    
    printf("\n=== Partita: %s ===\n", game->game_id);
    printf("Giocatori: %s (%s%s%c%s) vs %s (%s%s%c%s)\n", 
           game->players[0], 
           COLOR_RED, BOLD, PLAYER_X, RESET,
           game->players[1][0] ? game->players[1] : "[In attesa]",
           COLOR_BLUE, BOLD, PLAYER_O, RESET);
    printf("Turno di: %s\n", 
           game->status == GAME_IN_PROGRESS ? game->players[game->current_player] : "Nessuno");
    
    for (int row = 0; row < 3; row++) {
        printf("\n     |     |     \n ");
        for (int col = 0; col < 3; col++) {
            int idx = row * 3 + col;
            char cell = game->board[idx];
            
            if (cell == PLAYER_X) {
                printf(" %s%s%c%s ", COLOR_RED, BOLD, cell, RESET);
            } else if (cell == PLAYER_O) {
                printf(" %s%s%c%s ", COLOR_BLUE, BOLD, cell, RESET);
            } else {
                printf("(%d)", idx + 1);  // Cella vuota normale
            }
            
            if (col < 2) printf(" | ");
        }
        printf(" \n");
        if (row < 2) printf("_____|_____|_____");
    }
    printf("     |     |     \n");
    
    // Stato della partita
    printf("\nStato: ");
    switch (game->status) {
        case GAME_NEW:         printf("Nuova"); break;
        case GAME_WAITING:     printf("In attesa di giocatori"); break;
        case GAME_IN_PROGRESS: printf("In corso (mossa %d)", game->move_count + 1); break;
        case GAME_FINISHED:    
            if (game->winner == 2) {
                printf("Finita - PAREGGIO");
            } else if (game->winner >= 0) {
                printf("Finita - Vince: %s", game->players[game->winner]);
            } else {
                printf("Finita");
            }
            break;
        default: printf("Sconosciuto"); break;
    }
    printf("\n");
}

void game_get_board_string(const game_state_t *game, char board_str[9]) {
    if (!game || !board_str) return;
    
    memcpy(board_str, game->board, 9);
}
