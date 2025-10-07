#include "../include/logging.h"

// Mutex globale per il logging thread-safe
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Converte stringa livello log in enum
LogLevel get_log_level_from_string(const char* level_str) {
    if (strcmp(level_str, "DEBUG") == 0) return LOG_DEBUG;
    if (strcmp(level_str, "INFO") == 0) return LOG_INFO;
    if (strcmp(level_str, "WARN") == 0) return LOG_WARN;
    if (strcmp(level_str, "ERROR") == 0) return LOG_ERROR;
    return LOG_INFO; // Default
} 

// Converte enum livello log in stringa
const char* get_log_level_string(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        default:        return "INFO";
    }
}

// Inizializza il sistema di logging
void init_logging(void) {
    // Crea la directory logs se non esiste
    struct stat st = {0};
    if (stat("logs", &st) == -1) {
        if (mkdir("logs", 0755) == -1) {
            perror("Impossibile creare directory logs");
        }
    }
}

// Funzione principale per scrivere nel log
void log_message(const LogConfig* config, LogLevel level, const char* format, ...) {
    if (!config) {
        fprintf(stderr, "ERRORE: Configurazione logging non valida\n");
        return;
    }
    
    // Controlla se il livello è abbastanza alto per essere loggato
    LogLevel min_level = get_log_level_from_string(config->log_level);
    if (level < min_level) {
        return; // Non logga se il livello è troppo basso
    }
    
    // Thread-safe logging
    pthread_mutex_lock(&log_mutex);
    
    // Ottieni timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Apri il file di log in append
    FILE* log_file = fopen(config->log_file, "a");
    if (!log_file) {
        // Se non riesce ad aprire il file, scrivi su stderr
        fprintf(stderr, "ERRORE: Impossibile aprire file di log %s: %s\n", 
                config->log_file, strerror(errno));
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    
    // Scrivi timestamp e livello
    fprintf(log_file, "[%s] [%s] ", timestamp, get_log_level_string(level));
    
    // Scrivi il messaggio formattato
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    // Aggiungi newline se non c'è
    fprintf(log_file, "\n");
    
    // Flush per assicurarsi che venga scritto subito
    fflush(log_file);
    fclose(log_file);
    
    pthread_mutex_unlock(&log_mutex);
}