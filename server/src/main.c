#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Funzione eseguita da ogni thread (gestisce un client)
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg); // liberiamo memoria allocata per il socket descriptor
    char buffer[BUFFER_SIZE];

    printf("Nuovo client connesso! FD=%d\n", client_fd);

    // Loop per comunicazione continua
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (valread <= 0) {
            printf("Client FD=%d disconnesso.\n", client_fd);
            close(client_fd);
            pthread_exit(NULL);
        }

        buffer[valread] = '\0'; // assicuriamo stringa terminata
        printf("Ricevuto dal client FD=%d: %s\n", client_fd, buffer);

        // Semplice protocollo di comandi
        if (strncmp(buffer, "HELLO", 5) == 0) {
            char *msg = "SERVER: Ciao, benvenuto!\n";
            send(client_fd, msg, strlen(msg), 0);
        } else if (strncmp(buffer, "QUIT", 4) == 0) {
            char *msg = "SERVER: Connessione chiusa.\n";
            send(client_fd, msg, strlen(msg), 0);
            close(client_fd);
            printf("Client FD=%d ha chiuso la connessione.\n", client_fd);
            pthread_exit(NULL);
        } else {
            char *msg = "SERVER: Comando non riconosciuto.\n";
            send(client_fd, msg, strlen(msg), 0);
        }
    }
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Crea socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket fallita");
        exit(EXIT_FAILURE);
    }

    // Opzione per riutilizzare porta
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind fallito");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 10) < 0) {
        perror("Listen fallito");
        exit(EXIT_FAILURE);
    }

    printf("Server in ascolto sulla porta %d...\n", PORT);

    while (1) {
        int *client_fd = malloc(sizeof(int)); // allochiamo per passare al thread
        if ((*client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept fallito");
            free(client_fd);
            continue;
        }

        // Creiamo thread per gestire il client
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) != 0) {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
        }

        // Non servono join qui: lasciamo i thread staccati
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
