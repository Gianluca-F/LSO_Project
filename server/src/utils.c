#include "../include/utils.h"

// Variabili globali
ServerConfig server_config;
LogConfig log_config;

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

// Inizializza la configurazione di logging dal server config
void init_server_logging(void) {
    // Copia le impostazioni di logging dal server_config al log_config
    strncpy(log_config.log_level, server_config.log_level, sizeof(log_config.log_level) - 1);
    log_config.log_level[sizeof(log_config.log_level) - 1] = '\0';
    
    strncpy(log_config.log_file, server_config.log_file, sizeof(log_config.log_file) - 1);
    log_config.log_file[sizeof(log_config.log_file) - 1] = '\0';
    
    // Inizializza il sistema di logging condiviso
    init_logging();
}