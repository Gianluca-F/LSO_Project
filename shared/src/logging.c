#include "../include/logging.h"

// ============================================================================
// STATO GLOBALE
// ============================================================================

/**
 * Mutex globale per garantire thread-safety del logging
 * 
 * Questo mutex protegge l'accesso concorrente al file di log quando
 * più thread tentano di scrivere simultaneamente. Garantisce che i
 * messaggi non vengano interlacciati o corrotti.
 */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// ============================================================================
// FUNZIONI DI INIZIALIZZAZIONE
// ============================================================================

void init_logging(void) {
    struct stat st = {0};
    
    // Controlla se la directory logs esiste
    if (stat("logs", &st) == -1) {
        // Directory non esiste, prova a crearla
        if (mkdir("logs", 0755) == -1) {
            perror("Impossibile creare directory logs");
        }
    }
}

// ============================================================================
// FUNZIONI DI LOGGING
// ============================================================================

void log_message(const LogConfig* config, LogLevel level, const char* format, ...) {
    // Validazione configurazione
    if (!config) {
        fprintf(stderr, "ERRORE: Configurazione logging non valida\n");
        return;
    }
    
    // Filtraggio per livello: scarta messaggi sotto il livello minimo
    LogLevel min_level = get_log_level_from_string(config->log_level);
    if (level < min_level) {
        return; // Non logga se il livello è troppo basso
    }
    
    // Acquisisce il mutex per garantire thread-safety
    pthread_mutex_lock(&log_mutex);
    
    // Genera timestamp corrente
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Apri il file di log in modalità append
    FILE* log_file = fopen(config->log_file, "a");
    if (!log_file) {
        // Fallback: se non può aprire il file, scrivi su stderr
        fprintf(stderr, "ERRORE: Impossibile aprire file di log %s: %s\n", 
                config->log_file, strerror(errno));
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    
    // Scrivi timestamp e livello di severità
    fprintf(log_file, "[%s] [%s] ", timestamp, get_log_level_string(level));
    
    // Scrivi il messaggio formattato con argomenti variabili
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    // Aggiungi newline finale
    fprintf(log_file, "\n");
    
    // Flush per garantire scrittura immediata su disco
    // (importante per debugging in caso di crash)
    fflush(log_file);
    
    // Chiudi il file
    fclose(log_file);
    
    // Rilascia il mutex
    pthread_mutex_unlock(&log_mutex);
}

// ============================================================================
// FUNZIONI DI CONVERSIONE
// ============================================================================

LogLevel get_log_level_from_string(const char* level_str) {
    if (strcmp(level_str, "DEBUG") == 0) return LOG_DEBUG;
    if (strcmp(level_str, "INFO") == 0) return LOG_INFO;
    if (strcmp(level_str, "WARN") == 0) return LOG_WARN;
    if (strcmp(level_str, "ERROR") == 0) return LOG_ERROR;
    return LOG_INFO; // Default: livello INFO se non riconosciuto
} 

const char* get_log_level_string(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        default:        return "INFO"; // Default per valori non validi
    }
}
