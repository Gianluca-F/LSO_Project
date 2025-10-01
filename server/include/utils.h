#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// Livelli di log
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3
} LogLevel;

// Struttura per memorizzare la configurazione del server
typedef struct {
    int port;
    int max_clients;
    char log_level[20];
    char log_file[256];
} ServerConfig;

// Variabile globale per la configurazione (usata dal logging)
extern ServerConfig global_config;
extern pthread_mutex_t log_mutex;

// Funzioni per gestire la configurazione
int load_config(const char* config_file, ServerConfig* config);
void print_config(const ServerConfig* config);

// Funzioni per il logging
void init_logging(void);
void log_message(LogLevel level, const char* format, ...);
LogLevel get_log_level_from_string(const char* level_str);
const char* get_log_level_string(LogLevel level);

// Macro per semplificare l'uso del logging
#define LOG_ERROR(...) log_message(LOG_ERROR, __VA_ARGS__)
#define LOG_WARN(...)  log_message(LOG_WARN, __VA_ARGS__)
#define LOG_INFO(...)  log_message(LOG_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) log_message(LOG_DEBUG, __VA_ARGS__)

#endif