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

// ============================================================================
// LIVELLI DI LOG
// ============================================================================

/**
 * Livelli di severità per i messaggi di log
 * 
 * I livelli sono ordinati per gravità crescente. Il sistema di logging
 * può essere configurato per mostrare solo messaggi di un certo livello
 * o superiore.
 */
typedef enum {
    LOG_DEBUG = 0,    // Informazioni dettagliate per debugging
    LOG_INFO = 1,     // Informazioni generali sul funzionamento
    LOG_WARN = 2,     // Situazioni anomale ma gestibili
    LOG_ERROR = 3     // Errori critici che richiedono attenzione
} LogLevel;

// ============================================================================
// CONFIGURAZIONE LOGGING
// ============================================================================

/**
 * Struttura per la configurazione del sistema di logging
 * 
 * Contiene i parametri che controllano il comportamento del logging,
 * incluso il livello minimo di severità e il percorso del file di log.
 */
typedef struct {
    char log_level[20];    // Livello minimo: "DEBUG", "INFO", "WARN", "ERROR"
    char log_file[256];    // Percorso del file di log
} LogConfig;

// ============================================================================
// FUNZIONI DI INIZIALIZZAZIONE
// ============================================================================

/**
 * Inizializza il sistema di logging
 * 
 * Crea la directory "logs/" se non esiste. Questa funzione dovrebbe
 * essere chiamata all'avvio dell'applicazione prima di qualsiasi
 * operazione di logging.
 * 
 * La directory viene creata con permessi 0755 (rwx r-x r-x).
 * Se la creazione fallisce, viene stampato un messaggio di errore su stderr.
 */
void init_logging(void);

// ============================================================================
// FUNZIONI DI LOGGING
// ============================================================================

/**
 * Scrive un messaggio di log
 * 
 * Scrive un messaggio formattato nel file di log specificato nella
 * configurazione. Il messaggio viene scritto solo se il suo livello
 * è uguale o superiore al livello minimo configurato.
 * 
 * Il formato del messaggio è:
 * [YYYY-MM-DD HH:MM:SS] [LEVEL] messaggio
 * 
 * THREAD-SAFE: Utilizza un mutex globale per garantire scritture atomiche.
 * 
 * @param config Puntatore alla configurazione del logging
 * @param level Livello di severità del messaggio
 * @param format Stringa di formato (stile printf)
 * @param ... Argomenti variabili per il formato
 */
void log_message(const LogConfig* config, LogLevel level, const char* format, ...);

// ============================================================================
// FUNZIONI DI CONVERSIONE
// ============================================================================

/**
 * Converte una stringa in livello di log
 * 
 * Converte una rappresentazione testuale del livello (es. "DEBUG", "INFO")
 * nel corrispondente valore enum LogLevel.
 * 
 * @param level_str Stringa del livello ("DEBUG", "INFO", "WARN", "ERROR")
 * @return Livello di log corrispondente, LOG_INFO se stringa non riconosciuta
 */
LogLevel get_log_level_from_string(const char* level_str);

/**
 * Converte un livello di log in stringa
 * 
 * Converte un valore enum LogLevel nella sua rappresentazione testuale.
 * 
 * @param level Livello di log da convertire
 * @return Stringa corrispondente ("DEBUG", "INFO", "WARN", "ERROR")
 */
const char* get_log_level_string(LogLevel level);

// ============================================================================
// MACRO DI LOGGING
// ============================================================================

/**
 * Macro per semplificare l'uso del logging
 * 
 * Queste macro forniscono un'interfaccia semplificata per scrivere
 * messaggi di log ai vari livelli di severità.
 * 
 * NOTA IMPORTANTE: Queste macro richiedono che sia definita una variabile
 * 'log_config' di tipo LogConfig nel scope corrente.
 * 
 * Esempio di utilizzo:
 * 
 *   LogConfig log_config = {
 *       .log_level = "INFO",
 *       .log_file = "logs/app.log"
 *   };
 * 
 *   LOG_INFO("Server avviato sulla porta %d", port);
 *   LOG_ERROR("Errore connessione: %s", strerror(errno));
 */
#define LOG_ERROR(...) log_message(&log_config, LOG_ERROR, __VA_ARGS__)
#define LOG_WARN(...)  log_message(&log_config, LOG_WARN, __VA_ARGS__)
#define LOG_INFO(...)  log_message(&log_config, LOG_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) log_message(&log_config, LOG_DEBUG, __VA_ARGS__)

#endif