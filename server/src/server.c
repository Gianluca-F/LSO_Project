#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#define MAX_PORT_ATTEMPTS 10
#define BUFFER_SIZE 1024

// Tenta il bind su porte successive fino a trovarne una libera
// Ritorna la porta utilizzata o -1 se nessuna porta è disponibile
int bind_to_available_port(int server_fd, struct sockaddr_in *address, int start_port) {
    int current_port;

    for (int attempt = 0; attempt < MAX_PORT_ATTEMPTS; attempt++) {
        if (attempt < 5) {
            current_port = start_port + attempt;
        } else {
            current_port = start_port + 5 + (attempt - 5) * 10;
        }
        
        // Evita porte fuori range
        if (current_port > 65535) {
            LOG_WARN("Porta %d fuori range, interrompo ricerca", current_port);
            break;
        }
        
        // Configura la porta
        address->sin_port = htons(current_port);
        
        // Tenta il bind direttamente
        if (bind(server_fd, (struct sockaddr *)address, sizeof(*address)) == 0) {
            if (current_port != start_port) {
                printf("ATTENZIONE: Porta %d occupata, utilizzo porta alternativa %d\n", 
                       start_port, current_port);
                LOG_WARN("Porta %d occupata, utilizzo porta alternativa %d", 
                         start_port, current_port);
            }
            LOG_DEBUG("Bind completato con successo sulla porta %d", current_port);
            return current_port;
        }
        
        LOG_DEBUG("Porta %d non disponibile (tentativo %d/%d)", 
            current_port, attempt + 1, MAX_PORT_ATTEMPTS);
    }
    
    LOG_ERROR("Impossibile trovare una porta libera a partire da %d dopo %d tentativi", 
              start_port, MAX_PORT_ATTEMPTS);
    return -1;
}

// Funzione eseguita da ogni thread (gestisce un client)
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg); // liberiamo memoria allocata per il socket descriptor
    char buffer[BUFFER_SIZE];

    printf("Nuovo client connesso! FD=%d\n", client_fd);
    LOG_INFO("Nuova connessione client, FD=%d", client_fd);

    // Loop per comunicazione continua
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (valread <= 0) {
            printf("Client FD=%d disconnesso.\n", client_fd);
            LOG_WARN("Client FD=%d disconnesso inaspettatamente (valread=%d)", client_fd, valread);
            close(client_fd);
            pthread_exit(NULL);
        }

        buffer[valread] = '\0'; // assicuriamo stringa terminata
        printf("Ricevuto dal client FD=%d: %s\n", client_fd, buffer);
        LOG_DEBUG("Messaggio ricevuto da FD=%d: '%s'", client_fd, buffer);

        // Semplice protocollo di comandi
        if (strncmp(buffer, "HELLO", 5) == 0) {
            char *msg = "SERVER: Ciao, benvenuto!\n";
            send(client_fd, msg, strlen(msg), 0);
            LOG_DEBUG("Comando HELLO processato per FD=%d", client_fd);
        } else if (strncmp(buffer, "QUIT", 4) == 0) {
            char *msg = "SERVER: Connessione chiusa.\n";
            send(client_fd, msg, strlen(msg), 0);
            close(client_fd);
            printf("Client FD=%d ha chiuso la connessione.\n", client_fd);
            LOG_INFO("Client FD=%d ha chiuso la connessione normalmente", client_fd);
            pthread_exit(NULL);
        } else {
            char *msg = "SERVER: Comando non riconosciuto.\n";
            send(client_fd, msg, strlen(msg), 0);
            LOG_WARN("Comando non riconosciuto da FD=%d: '%s'", client_fd, buffer);
        }
    }
}

// Inizializza e configura il socket del server con fallback automatico delle porte
int init_server(int requested_port) {
    int server_fd;
    struct sockaddr_in address;
    int current_port;

    // Crea socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR("Creazione socket fallita: %s", strerror(errno));
        perror("Socket fallita");
        return -1;
    }
    LOG_DEBUG("Socket creato con successo, FD=%d", server_fd);

    // Opzione per riutilizzare porta
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    // Configura l'indirizzo di base
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    
    // Tenta il bind su una porta disponibile
    if ((current_port = bind_to_available_port(server_fd, &address, requested_port)) == -1) {
        LOG_ERROR("Impossibile trovare una porta libera a partire da %d", requested_port);
        printf("ERRORE: Impossibile trovare una porta libera a partire da %d\n", requested_port);
        close(server_fd);
        return -1;
    }

    // Listen - usa max_clients dalla configurazione
    if (listen(server_fd, global_config.max_clients) < 0) {
        LOG_ERROR("Listen fallito: %s", strerror(errno));
        perror("Listen fallito");
        close(server_fd);
        return -1;
    }

    printf("Server in ascolto sulla porta %d...\n", current_port);
    LOG_INFO("Server in ascolto sulla porta %d, max client: %d", current_port, global_config.max_clients);

    // Aggiorna la configurazione globale con la porta effettivamente utilizzata
    global_config.port = current_port;

    return server_fd;
}

// Avvia il server e gestisce le connessioni client
void start_server(int server_fd) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    while (1) {
        int *client_fd = malloc(sizeof(int)); // allochiamo per passare al thread
        if ((*client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            LOG_ERROR("Accept fallito: %s", strerror(errno));
            perror("Accept fallito");
            free(client_fd);
            continue;
        }

        // Creiamo thread per gestire il client
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) != 0) {
            LOG_ERROR("Creazione thread fallita per FD=%d: %s", *client_fd, strerror(errno));
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
        } else {
            LOG_DEBUG("Thread creato per gestire client FD=%d", *client_fd);
        }

        // Non servono join qui: lasciamo i thread staccati
        pthread_detach(tid);
    }
}