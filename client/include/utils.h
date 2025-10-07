#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "../../shared/include/logging.h"

typedef struct {
    // Configurazioni di rete
    char server_ip[16];
    int port;
    
    // Timeout //NOTE: Non usati al momento
    int connection_timeout;
    int retry_attempts;

    // Logging
    char log_level[20];
    char log_file[256];
} ClientConfig;

// Variabile globale per la configurazione
extern ClientConfig client_config;

// Configurazione per il logging (compatibile con il sistema condiviso)
extern LogConfig log_config;

// Funzioni per gestire la configurazione
int load_config(const char* config_file, ClientConfig* config);
void print_config(const ClientConfig* config);

// Funzione per inizializzare la configurazione di logging dal client config
void init_client_logging(void);

#endif