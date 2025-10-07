#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/utils.h"
#include "client.h"

int main() {
    // Carica la configurazione
    load_config("config/client.conf", &client_config);
    print_config(&client_config);
    
    // Inizializza il sistema di logging
    init_client_logging();
    LOG_INFO("Client avviato, caricamento configurazione completato");


    return 0;
}