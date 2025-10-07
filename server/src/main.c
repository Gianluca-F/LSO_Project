#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/utils.h"
#include "server.h"

int main() {
    // Carica la configurazione
    load_config("config/server.conf", &server_config);
    print_config(&server_config);
    
    // Inizializza il sistema di logging
    init_server_logging();
    LOG_INFO("Server avviato, caricamento configurazione completato");

    // Inizializza il server
    int server_fd = init_server(server_config.port);
    if (server_fd < 0) {
        LOG_ERROR("Inizializzazione server fallita");
        exit(EXIT_FAILURE);
    }

    // Avvia il server (loop principale)
    start_server(server_fd);

    // Questo punto non dovrebbe mai essere raggiunto
    close(server_fd);
    LOG_INFO("Server terminato");
    return 0;
}
