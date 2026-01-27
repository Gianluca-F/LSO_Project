#ifndef CONSTANTS_H
#define CONSTANTS_H

// ============================================================================
// DIMENSIONI FISSE DEL GIOCO
// ============================================================================

#define BOARD_SIZE 9                    // Tabellone 3x3 (regola del Tris)
#define MAX_PLAYERS_PER_GAME 2          // Tris è sempre 1v1

// ============================================================================
// LUNGHEZZE MASSIME STRINGHE
// ============================================================================

#define MAX_PLAYER_NAME 32              // Nome giocatore
#define MAX_GAME_ID_LEN 16              // ID partita
#define MAX_MESSAGE_SIZE 4096           // Messaggio protocollo

// ============================================================================
// STATI DEL GIOCO
// ============================================================================

#define GAME_CREATED 0                  // Partita creata //NOTE: non usato
#define GAME_WAITING 1                  // In attesa del secondo giocatore
#define GAME_IN_PROGRESS 2              // Partita in corso
#define GAME_FINISHED 3                 // Partita terminata

// ============================================================================
// RISULTATI PARTITA
// ============================================================================

#define RESULT_NONE 0                   // Nessun risultato //NOTE: non usato
#define RESULT_WIN 1                    // Vittoria
#define RESULT_LOSE 2                   // Sconfitta
#define RESULT_DRAW 3                   // Pareggio

// ============================================================================
// SIMBOLI GIOCATORI
// ============================================================================

#define PLAYER_X 'X'                    // Primo giocatore
#define PLAYER_O 'O'                    // Secondo giocatore
#define EMPTY_CELL ' '                  // Cella vuota

// ============================================================================
// CODICI ANSI PER FORMATTAZIONE OUTPUT
// ============================================================================

#define BOLD "\x1b[1m"                  // Testo in grassetto
#define RESET "\x1b[0m"                 // Reset formattazione
#define COLOR_RED "\x1b[31m"            // Colore rosso 
#define COLOR_BLUE "\x1b[34m"           // Colore blu 

#endif
