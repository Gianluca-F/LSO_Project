#ifndef CONSTANTS_H
#define CONSTANTS_H

// Dimensioni fisse del gioco Tris
#define BOARD_SIZE 9                    // Tabellone 3x3 (regola del Tris)
#define MAX_PLAYERS_PER_GAME 2          // Tris è sempre 1v1

// Lunghezze massime stringhe
#define MAX_PLAYER_NAME 32              // Nome giocatore
#define MAX_GAME_ID_LEN 16              // ID partita
#define MAX_MESSAGE_SIZE 1024           // Messaggio protocollo

// Stati del gioco
#define GAME_NEW 0                      // Partita appena creata
#define GAME_WAITING 1                  // In attesa di giocatori
#define GAME_IN_PROGRESS 2              // Partita in corso
#define GAME_FINISHED 3                 // Partita terminata

// Stati connessione giocatore
#define PLAYER_CONNECTED 0              // Connesso ma non registrato
#define PLAYER_REGISTERED 1             // Registrato ma non in partita
#define PLAYER_IN_GAME 2                // In partita

// Risultati partita
#define RESULT_NONE 0                   // Nessun risultato
#define RESULT_WIN 1                    // Vittoria
#define RESULT_LOSE 2                   // Sconfitta
#define RESULT_DRAW 3                   // Pareggio

// Simboli giocatori
#define PLAYER_X 'X'                    // Primo giocatore
#define PLAYER_O 'O'                    // Secondo giocatore
#define EMPTY_CELL ' '                  // Cella vuota

// Return codes
#define SUCCESS 0
#define ERROR_SOCKET -1
#define ERROR_BIND -2
#define ERROR_LISTEN -3
#define ERROR_ACCEPT -4
#define ERROR_CONNECT -5

#endif
