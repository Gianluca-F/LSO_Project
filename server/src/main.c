#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils.h"
#include "server.h"

int main() {
    // Carica la configurazione
    load_config("config/server.conf", &global_config);
    print_config(&global_config);
    
    // Inizializza il sistema di logging
    init_logging();
    LOG_INFO("Server avviato, caricamento configurazione completato");

    // Inizializza il server
    int server_fd = init_server(global_config.port);
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
