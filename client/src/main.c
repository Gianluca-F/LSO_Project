#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/utils.h"
#include "client.h"

int main() {
    // Carica la configurazione
    load_config("config/client.conf", &client_config);
    print_config(&client_config);
    
    // Inizializza il sistema di logging
    init_client_logging();
    LOG_INFO("Client avviato, caricamento configurazione completato");
    
    // Connetti al server
    printf("\nConnessione al server %s:%d...\n", 
           client_config.server_ip, client_config.port);
    
    if (client_connect(client_config.server_ip, client_config.port) < 0) {
        fprintf(stderr, "Errore: impossibile connettersi al server\n");
        return EXIT_FAILURE;
    }
    
    printf("Connesso con successo!\n");
    LOG_INFO("Connessione al server stabilita");
    
    // Avvia il thread per le notifiche
    client_state.running = true;
    if (pthread_create(&client_state.notification_thread, NULL, 
                      notification_thread_func, NULL) != 0) {
        fprintf(stderr, "Errore: impossibile avviare il thread di notifiche\n");
        client_disconnect();
        return EXIT_FAILURE;
    }
    
    LOG_INFO("Thread di notifiche avviato");
    
    // Avvia il menu interattivo (blocca fino a quit)
    client_run();
    
    // Cleanup
    LOG_INFO("Chiusura client...");
    client_disconnect();
    
    printf("\nClient terminato.\n");
    return EXIT_SUCCESS;
}