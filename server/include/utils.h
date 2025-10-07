#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "../../shared/include/logging.h"

// Struttura per memorizzare la configurazione del server
typedef struct {
    // Configurazioni di rete
    char server_ip[16];
    int port;
    int backlog_size;
    
    // Limiti server
    int max_clients;
    int max_games;
    
    // Timeout //NOTE: Non usati al momento
    int connection_timeout;
    int read_timeout;
    
    // Logging
    char log_level[20];
    char log_file[256];
} ServerConfig;

// Variabile globale per la configurazione
extern ServerConfig server_config;

// Configurazione per il logging (compatibile con il sistema condiviso)
extern LogConfig log_config;

// Funzioni per gestire la configurazione
int load_config(const char* config_file, ServerConfig* config);
void print_config(const ServerConfig* config);

// Funzione per inizializzare la configurazione di logging dal server config
void init_server_logging(void);

#endif