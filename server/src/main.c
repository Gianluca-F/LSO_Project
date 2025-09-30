#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include "utils.h"

#define BUFFER_SIZE 1024

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

int main() {
    // Carica la configurazione
    load_config("config/server.conf", &global_config);
    print_config(&global_config);
    
    // Inizializza il sistema di logging
    init_logging();
    LOG_INFO("Server avviato, caricamento configurazione completato");

    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Crea socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR("Creazione socket fallita: %s", strerror(errno));
        perror("Socket fallita");
        exit(EXIT_FAILURE);
    }
    LOG_DEBUG("Socket creato con successo, FD=%d", server_fd);

    // Opzione per riutilizzare porta
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(global_config.port);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        LOG_ERROR("Bind fallito sulla porta %d: %s", global_config.port, strerror(errno));
        perror("Bind fallito");
        exit(EXIT_FAILURE);
    }
    LOG_DEBUG("Bind completato sulla porta %d", global_config.port);

    // Listen - usa max_clients dalla configurazione
    if (listen(server_fd, global_config.max_clients) < 0) {
        LOG_ERROR("Listen fallito: %s", strerror(errno));
        perror("Listen fallito");
        exit(EXIT_FAILURE);
    }

    printf("Server in ascolto sulla porta %d...\n", global_config.port);
    LOG_INFO("Server in ascolto sulla porta %d, max client: %d", global_config.port, global_config.max_clients);

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

    close(server_fd);
    LOG_INFO("Server terminato");
    return 0;
}
