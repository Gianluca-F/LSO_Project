#include "utils.h"
#include "../../shared/include/constants.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>

// Variabili globali
ServerConfig global_config;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Carica la configurazione dal file
int load_config(const char* config_file, ServerConfig* config) {
    FILE* file = fopen(config_file, "r");
    if (!file) {
        printf("Impossibile aprire il file di configurazione: %s\n", config_file);
        printf("Utilizzo configurazione di default.\n");
        return 0;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Ignora le righe vuote e i commenti
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        // Rimuovi il newline finale
        line[strcspn(line, "\n\r")] = 0;
        
        // Trova il carattere '='
        char* equals = strchr(line, '=');
        if (!equals) {
            continue;
        }
        
        // Separa chiave e valore
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Rimuovi spazi iniziali e finali
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        // Interpreta i parametri
        if (strcmp(key, "server_ip") == 0) {
            strncpy(config->server_ip, value, sizeof(config->server_ip) - 1);
        } else if (strcmp(key, "port") == 0) {
            config->port = atoi(value);
        } else if (strcmp(key, "backlog_size") == 0) {
            config->backlog_size = atoi(value);
        } else if (strcmp(key, "max_clients") == 0) {
            config->max_clients = atoi(value);
        } else if (strcmp(key, "max_games") == 0) {
            config->max_games = atoi(value);
        } else if (strcmp(key, "connection_timeout") == 0) {
            config->connection_timeout = atoi(value);
        } else if (strcmp(key, "read_timeout") == 0) {
            config->read_timeout = atoi(value);
        } else if (strcmp(key, "log_level") == 0) {
            strncpy(config->log_level, value, sizeof(config->log_level) - 1);
        } else if (strcmp(key, "log_file") == 0) {
            strncpy(config->log_file, value, sizeof(config->log_file) - 1);
        }
    }
    
    fclose(file);
    return 1;
}

// Stampa la configurazione corrente
void print_config(const ServerConfig* config) {
    printf("=== CONFIGURAZIONE SERVER ===\n");
    printf("Server IP: %s\n", config->server_ip);
    printf("Porta: %d\n", config->port);
    printf("Backlog: %d\n", config->backlog_size);
    printf("Max client: %d\n", config->max_clients);
    printf("Max partite: %d\n", config->max_games);
    printf("Timeout connessione: %d sec\n", config->connection_timeout);
    printf("Timeout lettura: %d sec\n", config->read_timeout);
    printf("Livello log: %s\n", config->log_level);
    printf("File di log: %s\n", config->log_file);
    printf("==================================\n");
}

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
void log_message(LogLevel level, const char* format, ...) {
    // Controlla se il livello è abbastanza alto per essere loggato
    LogLevel min_level = get_log_level_from_string(global_config.log_level);
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
    FILE* log_file = fopen(global_config.log_file, "a");
    if (!log_file) {
        // Se non riesce ad aprire il file, scrivi su stderr
        fprintf(stderr, "ERRORE: Impossibile aprire file di log %s: %s\n", 
                global_config.log_file, strerror(errno));
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