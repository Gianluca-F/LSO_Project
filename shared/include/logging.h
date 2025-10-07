#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

// Livelli di log
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3
} LogLevel;

// Struttura per la configurazione del logging
typedef struct {
    char log_level[20];
    char log_file[256];
} LogConfig;

// Funzioni per il logging
void init_logging(void);
void log_message(const LogConfig* config, LogLevel level, const char* format, ...);
LogLevel get_log_level_from_string(const char* level_str);
const char* get_log_level_string(LogLevel level);

// Macro per semplificare l'uso del logging
// Nota: Queste macro richiedono che sia definita una variabile 'log_config' nel scope
#define LOG_ERROR(...) log_message(&log_config, LOG_ERROR, __VA_ARGS__)
#define LOG_WARN(...)  log_message(&log_config, LOG_WARN, __VA_ARGS__)
#define LOG_INFO(...)  log_message(&log_config, LOG_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) log_message(&log_config, LOG_DEBUG, __VA_ARGS__)

#endif