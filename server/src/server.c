#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>

#define MAX_PORT_ATTEMPTS 10
#define BUFFER_SIZE 1024

// ============================================================================
// STATO GLOBALE DEL SERVER
// ============================================================================

server_state_t server_state;

// ============================================================================
// FUNZIONI PER LA GESTIONE DEL SERVER
// ============================================================================

void init_server_state() {
    pthread_mutex_init(&server_state.mutex, NULL);
    
    // Leggi i limiti dalla configurazione
    server_state.max_clients = server_config.max_clients;
    server_state.max_games = server_config.max_games;
    
    LOG_INFO("Inizializzazione stato server: max_clients=%d, max_games=%d", 
             server_state.max_clients, server_state.max_games);
    
    // Alloca array dinamici
    server_state.clients = (client_info_t*)malloc(server_state.max_clients * sizeof(client_info_t));
    if (!server_state.clients) {
        LOG_ERROR("ERRORE CRITICO: Impossibile allocare memoria per client array");
        fprintf(stderr, "ERRORE: Impossibile allocare memoria per %d client\n", server_state.max_clients);
        exit(EXIT_FAILURE);
    }

    server_state.games = (game_session_t*)malloc(server_state.max_games * sizeof(game_session_t));
    if (!server_state.games) {
        LOG_ERROR("ERRORE CRITICO: Impossibile allocare memoria per games array");
        fprintf(stderr, "ERRORE: Impossibile allocare memoria per %d partite\n", server_state.max_games);
        free(server_state.clients);
        exit(EXIT_FAILURE);
    }
    
    // Inizializza tutti i client come non attivi
    for (int i = 0; i < server_state.max_clients; i++) {
        server_state.clients[i].fd = -1;
        server_state.clients[i].game_index = -1;
    }
    
    // Inizializza tutte le partite come non attive
    for (int i = 0; i < server_state.max_games; i++) {
        server_state.games[i].active = 0;
        server_state.games[i].pending_join_fd = -1;
    }
    
    server_state.num_clients = 0;
    server_state.num_games = 0;
    
    LOG_INFO("Stato server inizializzato con successo: %d client, %d partite", 
             server_state.max_clients, server_state.max_games);
}

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
    if (listen(server_fd, server_config.max_clients) < 0) {
        LOG_ERROR("Listen fallito: %s", strerror(errno));
        perror("Listen fallito");
        close(server_fd);
        return -1;
    }

    printf("Server in ascolto sulla porta %d...\n", current_port);
    LOG_INFO("Server in ascolto sulla porta %d, max client: %d", current_port, server_config.max_clients);

    // Aggiorna la configurazione globale con la porta effettivamente utilizzata
    server_config.port = current_port;

    return server_fd;
}

void start_server(int server_fd) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    while (1) {
        int new_client_fd;
        if ((new_client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            LOG_ERROR("Accept fallito: %s", strerror(errno));
            perror("Accept fallito");
            continue;
        }

        // Controlla se il server è pieno PRIMA di allocare risorse
        pthread_mutex_lock(&server_state.mutex);
        bool is_full = (server_state.num_clients >= server_state.max_clients);
        pthread_mutex_unlock(&server_state.mutex);

        if (is_full) {
            // Server pieno: rifiuta immediatamente senza creare thread
            LOG_WARN("Server pieno (%d/%d client), rifiuto connessione FD=%d", 
                     server_state.num_clients, server_state.max_clients, new_client_fd);
            printf("⚠️  Connessione rifiutata (server pieno %d/%d): FD=%d\n",
                   server_state.num_clients, server_state.max_clients, new_client_fd);
            
            response_register_t error_response;
            error_response.status = STATUS_ERROR;
            error_response.error_code = ERR_SERVER_FULL;
            protocol_send(new_client_fd, MSG_RESPONSE, &error_response, sizeof(error_response), 0);
            
            usleep(500000); // 500ms per assicurarsi che il messaggio venga ricevuto dal client
            close(new_client_fd);
            continue;
        }

        // Server non pieno: procedi con allocazione e creazione thread
        int *client_fd = malloc(sizeof(int));
        if (!client_fd) {
            LOG_ERROR("Errore allocazione memoria per client FD=%d", new_client_fd);
            close(new_client_fd);
            continue;
        }
        *client_fd = new_client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) != 0) {
            LOG_ERROR("Creazione thread fallita per FD=%d: %s", *client_fd, strerror(errno));
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
        } else {
            LOG_DEBUG("Thread creato per gestire client FD=%d", *client_fd);
            // Non servono join qui: lasciamo i thread staccati
            pthread_detach(tid);
        }
    }
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg); // liberiamo memoria allocata per il socket descriptor
    
    printf("Nuovo client connesso! FD=%d\n", client_fd);
    LOG_INFO("Nuova connessione client, FD=%d", client_fd);
    
    // Aggiungi il client allo stato del server
    pthread_mutex_lock(&server_state.mutex);
    int client_idx = add_client(client_fd);
    pthread_mutex_unlock(&server_state.mutex);
    
    if (client_idx == -1) {
        // Questo non dovrebbe mai accadere perché controlliamo prima in start_server
        LOG_ERROR("ERRORE CRITICO: Impossibile aggiungere client FD=%d nonostante controllo preventivo", client_fd);
        close(client_fd);
        pthread_exit(NULL);
    }
    
    bool should_run = true;
    // Loop principale: ricevi e gestisci messaggi
    while (should_run) {
        protocol_header_t header;
        
        // Ricevi l'header
        ssize_t received = protocol_recv_header(client_fd, &header);
        if (received <= 0) {
            if (received == 0) {
                LOG_INFO("Client FD=%d disconnesso (connessione chiusa)", client_fd);
            } else {
                LOG_WARN("Errore ricezione header da FD=%d: %s", client_fd, strerror(errno));
            }
            break;
        }
        
        LOG_DEBUG("Header ricevuto da FD=%d: type=%d, length=%d, seq=%d",
                 client_fd, header.msg_type, header.length, header.seq_id);
        
        // Ricevi il payload se presente
        void *payload = NULL;
        if (header.length > 0) {
            // Limita dimensione payload per sicurezza
            if (header.length > MAX_MESSAGE_SIZE) {
                LOG_ERROR("Payload troppo grande da FD=%d: %d bytes", client_fd, header.length);
                break;
            }
            
            payload = malloc(header.length);
            if (!payload) {
                LOG_ERROR("Errore allocazione memoria per payload (%d bytes)", header.length);
                break;
            }
            
            received = protocol_recv_payload(client_fd, payload, header.length);
            if (received <= 0) {
                LOG_ERROR("Errore ricezione payload da FD=%d", client_fd);
                free(payload);
                break;
            }
        }
        
        // Dispatch al handler appropriato
        switch (header.msg_type) {
            case MSG_REGISTER:
                handle_register(client_fd, payload, header.length);
                break;
                
            case MSG_CREATE_GAME:
                handle_create_game(client_fd);
                break;
                
            case MSG_LIST_GAMES:
                handle_list_games(client_fd);
                break;
                
            case MSG_JOIN_GAME:
                handle_join_game(client_fd, payload, header.length);
                break;
                
            case MSG_ACCEPT_JOIN:
                handle_accept_join(client_fd, payload, header.length);
                break;
                
            case MSG_MAKE_MOVE:
                handle_make_move(client_fd, payload, header.length);
                break;
                
            case MSG_LEAVE_GAME:
                handle_leave_game(client_fd);
                break;

            //NOTE: manca -> case MSG_NEW_GAME:
                
            case MSG_QUIT:
                handle_quit(client_fd);
                free(payload);
                should_run = false;  // Esci dal loop
                break;
                
            default:
                LOG_WARN("Tipo messaggio sconosciuto da FD=%d: %d", client_fd, header.msg_type);
                break;
        }
        
        if (payload) {
            free(payload);
        }
    }
    
    pthread_mutex_lock(&server_state.mutex);
    remove_client(client_fd);
    pthread_mutex_unlock(&server_state.mutex);
    
    close(client_fd);
    printf("Client FD=%d disconnesso e rimosso.\n", client_fd);
    LOG_INFO("Client FD=%d disconnesso e rimosso", client_fd);
    
    pthread_exit(NULL);
}

int bind_to_available_port(int server_fd, struct sockaddr_in *address, int starting_port) {
    int current_port;

    for (int attempt = 0; attempt < MAX_PORT_ATTEMPTS; attempt++) {
        if (attempt < 5) {
            current_port = starting_port + attempt;
        } else {
            current_port = starting_port + 5 + (attempt - 5) * 10;
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
            if (current_port != starting_port) {
                printf("ATTENZIONE: Porta %d occupata, utilizzo porta alternativa %d\n", 
                       starting_port, current_port);
                LOG_WARN("Porta %d occupata, utilizzo porta alternativa %d", 
                         starting_port, current_port);
            }
            LOG_DEBUG("Bind completato con successo sulla porta %d", current_port);
            return current_port;
        }
        
        LOG_DEBUG("Porta %d non disponibile (tentativo %d/%d)", 
            current_port, attempt + 1, MAX_PORT_ATTEMPTS);
    }
    
    LOG_ERROR("Impossibile trovare una porta libera a partire da %d dopo %d tentativi", 
              starting_port, MAX_PORT_ATTEMPTS);
    return -1;
}

// ============================================================================
// FUNZIONI DI GESTIONE CLIENT
// ============================================================================

int find_client_by_fd(int fd) {
    for (int i = 0; i < server_state.num_clients; i++) {
        if (server_state.clients[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

int find_client_by_name(const char *name) {
    if (!name) return -1;
    
    for (int i = 0; i < server_state.num_clients; i++) {
        if (server_state.clients[i].status != CLIENT_CONNECTED &&
            strcmp(server_state.clients[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int add_client(int fd) {
    // Controlla se c'è spazio (per robustezza)
    if (server_state.num_clients >= server_state.max_clients) {
        LOG_ERROR("Impossibile aggiungere client FD=%d: array pieno (max_clients=%d)", 
                  fd, server_state.max_clients);
        return -1;
    }
    
    // Usa num_clients come indice diretto (O(1))
    int slot = server_state.num_clients;
    
    // Inizializza il client
    server_state.clients[slot].fd = fd;
    server_state.clients[slot].name[0] = '\0';
    server_state.clients[slot].status = CLIENT_CONNECTED;
    server_state.clients[slot].game_index = -1;
    server_state.clients[slot].player_index = -1;
    
    server_state.num_clients++;
    
    LOG_INFO("Client aggiunto: FD=%d, slot=%d, totale client=%d", 
             fd, slot, server_state.num_clients);
    
    return slot;
}

void remove_client(int fd) {
    int client_idx = find_client_by_fd(fd);
    
    if (client_idx == -1) {
        LOG_WARN("Tentativo di rimuovere client FD=%d non trovato", fd);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];
    
    // Se il client era in una partita, gestisci la disconnessione
    if (client->game_index >= 0) {
        game_session_t *game = &server_state.games[client->game_index];
        
        if (game->active) {
            // Trova l'avversario
            int opponent_fd = -1;
            if (game->player_fds[0] == fd) {
                opponent_fd = game->player_fds[1];
            } else if (game->player_fds[1] == fd) {
                opponent_fd = game->player_fds[0];
            }
            
            // Notifica l'avversario che il giocatore ha abbandonato
            if (opponent_fd > 0) {
                notify_opponent_left_t notify;
                notify.notify_type = NOTIFY_OPPONENT_LEFT;
                protocol_send(opponent_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
                
                LOG_INFO("Notifica OPPONENT_LEFT inviata a FD=%d", opponent_fd);
            }
            
            // Pulisci la partita
            cleanup_game(game);
        }
    }
    
    LOG_INFO("Rimozione client FD=%d, nome='%s', status=%d, totale client rimanenti=%d", 
             fd, client->name, client->status, server_state.num_clients - 1);
    
    // Swap con l'ultimo client (O(1)) - se non è già l'ultimo
    int last_idx = server_state.num_clients - 1;
    if (client_idx != last_idx) {
        // Copia l'ultimo client nella posizione da rimuovere
        server_state.clients[client_idx] = server_state.clients[last_idx];
        
        LOG_DEBUG("Client swappato: slot %d <- slot %d (FD=%d)", 
                 client_idx, last_idx, server_state.clients[client_idx].fd);
    }
    
    // Decrementa il contatore
    server_state.num_clients--;
}

// ============================================================================
// FUNZIONI DI GESTIONE PARTITE
// ============================================================================

int find_game_by_id(const char *game_id) {
    if (!game_id) return -1;
    
    for (int i = 0; i < server_state.max_games; i++) {
        if (server_state.games[i].active && 
            strcmp(server_state.games[i].state.game_id, game_id) == 0) {
            return i;
        }
    }
    return -1;
}

int find_game_by_client_fd(int fd) { //NOTE: not used
    for (int i = 0; i < server_state.max_games; i++) {
        if (server_state.games[i].active &&
            (server_state.games[i].player_fds[0] == fd ||
             server_state.games[i].player_fds[1] == fd)) {
            return i;
        }
    }
    return -1;
}

int create_game(const char *creator_name, int creator_fd) {
    if (!creator_name) return -1;
    
    // Trova uno slot libero
    for (int i = 0; i < server_state.max_games; i++) {
        if (!server_state.games[i].active) {
            game_session_t *game_session = &server_state.games[i];
            
            // Genera un game_id univoco usando timestamp + indice //NOTE: c'è un modo più semplice?
            char game_id[MAX_GAME_ID_LEN];
            snprintf(game_id, MAX_GAME_ID_LEN, "G%06ld%02d", time(NULL) % 1000000, i % 100);
            
            // Inizializza il game state (da game_logic.h)
            game_init(&game_session->state, game_id, creator_name);
            
            // Imposta i FD dei giocatori
            game_session->player_fds[0] = creator_fd;
            game_session->player_fds[1] = -1;  // Ancora nessun secondo giocatore
            
            // Nessun pending join inizialmente
            game_session->pending_join_fd = -1;
            game_session->pending_join_name[0] = '\0';
            
            // Marca come attiva
            game_session->active = 1;
            server_state.num_games++;
            
            LOG_INFO("Partita creata: game_id='%s', creatore='%s', FD=%d, slot=%d, totale partite=%d",
                     game_id, creator_name, creator_fd, i, server_state.num_games);
            
            return i;
        }
    }
    
    LOG_ERROR("Impossibile creare partita: array pieno (max_games=%d)", server_state.max_games);
    return -1;
}

void cleanup_game(game_session_t *game) {
    if (!game || !game->active) return;
    
    LOG_INFO("Cleanup partita: game_id='%s'", game->state.game_id);
    
    // Trova i client associati e resetta il loro stato
    for (int i = 0; i < 2; i++) {
        if (game->player_fds[i] > 0) {
            int client_idx = find_client_by_fd(game->player_fds[i]);
            if (client_idx != -1) {
                client_info_t *client = &server_state.clients[client_idx];
                client->game_index = -1;
                client->player_index = -1;
                client->status = CLIENT_REGISTERED;
                LOG_DEBUG("Client FD=%d rimosso dalla partita, status -> REGISTERED", 
                         client->fd);
            }
        }
    }
    
    // Marca la partita come non attiva
    game->active = 0;
    game->pending_join_fd = -1;
    server_state.num_games--;
    
    LOG_INFO("Partita pulita, totale partite rimanenti=%d", server_state.num_games);
}

// ============================================================================
// HANDLER MESSAGGI PROTOCOLLO
// ============================================================================

void handle_register(int client_fd, const void *payload, uint16_t length) {
    LOG_DEBUG("handle_register chiamato per FD=%d", client_fd);
    
    response_register_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova il client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_ERROR("Client FD=%d non trovato in handle_register", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];
    
    // Controlla se è già registrato
    if (client->status != CLIENT_CONNECTED) {
        LOG_WARN("Client FD=%d già registrato con nome '%s'", client_fd, client->name);
        response.error_code = ERR_ALREADY_REGISTERED;  
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Valida il payload
    if (length < sizeof(payload_register_t)) {
        LOG_ERROR("Payload MSG_REGISTER invalido: length=%d, expected=%zu", 
                 length, sizeof(payload_register_t));
        response.error_code = ERR_INVALID_PAYLOAD;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    const payload_register_t *reg = (const payload_register_t*)payload;
    
    // Valida il nome
    if (!protocol_validate_name(reg->player_name)) {
        LOG_WARN("Nome giocatore non valido: '%s'", reg->player_name);
        response.error_code = ERR_INVALID_NAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Controlla se il nome è già usato
    if (find_client_by_name(reg->player_name) != -1) {
        LOG_WARN("Nome '%s' già in uso", reg->player_name);
        response.error_code = ERR_NAME_TAKEN;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Registra il client
    strncpy(client->name, reg->player_name, MAX_PLAYER_NAME - 1);
    client->name[MAX_PLAYER_NAME - 1] = '\0';
    client->status = CLIENT_REGISTERED;
    
    LOG_INFO("Client FD=%d registrato con nome '%s'", client_fd, client->name);
    
    pthread_mutex_unlock(&server_state.mutex);
    
    // Invia risposta di successo
    response.status = STATUS_OK;
    response.error_code = ERR_NONE;
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
}

void handle_create_game(int client_fd) {
    LOG_DEBUG("handle_create_game chiamato per FD=%d", client_fd);
    
    response_create_game_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    response.game_id[0] = '\0';
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova il client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_ERROR("Client FD=%d non trovato in handle_create_game", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];
    
    // Deve essere registrato
    if (client->status != CLIENT_REGISTERED) {
        LOG_WARN("Client FD=%d non registrato o già in partita (status=%d)", 
                 client_fd, client->status);
        response.error_code = (client->status == CLIENT_CONNECTED) ? 
                             ERR_NOT_REGISTERED : ERR_ALREADY_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Crea la partita
    int game_index = create_game(client->name, client_fd);
    if (game_index == -1) {
        LOG_ERROR("Impossibile creare partita per client FD=%d", client_fd);
        response.error_code = ERR_SERVER_FULL;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    game_state_t *game = &server_state.games[game_index].state;
    
    // Aggiorna lo stato del client
    client->game_index = game_index;
    client->player_index = 0;  // Il creatore è sempre player 0
    client->status = CLIENT_IN_LOBBY;  // In attesa che qualcuno faccia join
    
    LOG_INFO("Partita '%s' creata da client '%s' (FD=%d)", 
             game->game_id, client->name, client_fd);
    
    // Prepara risposta di successo
    response.status = STATUS_OK;
    response.error_code = ERR_NONE;
    strncpy(response.game_id, game->game_id, MAX_GAME_ID_LEN - 1);
    response.game_id[MAX_GAME_ID_LEN - 1] = '\0';
    
    // Prepara notifica broadcast
    notify_game_created_t notify;
    notify.notify_type = NOTIFY_GAME_CREATED;
    strncpy(notify.game_id, game->game_id, MAX_GAME_ID_LEN - 1);
    notify.game_id[MAX_GAME_ID_LEN - 1] = '\0';
    strncpy(notify.creator, client->name, MAX_PLAYER_NAME - 1);
    notify.creator[MAX_PLAYER_NAME - 1] = '\0';
    
    pthread_mutex_unlock(&server_state.mutex);
    
    // Invia risposta al creatore
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
    
    // Broadcast ai client registrati
    pthread_mutex_lock(&server_state.mutex);
    broadcast_to_registered_clients(MSG_NOTIFY, &notify, sizeof(notify));
    pthread_mutex_unlock(&server_state.mutex);
    
    LOG_INFO("Broadcast GAME_CREATED inviato per partita '%s'", game->game_id);
}

// Helper locale per inviare errore list_games
static inline void send_list_games_error(int client_fd, error_code_t error) {
    response_list_games_t err_response;
    err_response.status = STATUS_ERROR;
    err_response.error_code = error;
    err_response.game_count = 0;
    protocol_send(client_fd, MSG_RESPONSE, &err_response, sizeof(err_response), 0);
}

void handle_list_games(int client_fd) {
    LOG_DEBUG("handle_list_games chiamato da FD=%d", client_fd);
    
    pthread_mutex_lock(&server_state.mutex);

    // Trova il client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_ERROR("Client FD=%d non trovato in handle_list_games", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        send_list_games_error(client_fd, ERR_INTERNAL);
        return;
    }

    client_info_t *client = &server_state.clients[client_idx];

    // Controlla se è registrato
    if (client->status != CLIENT_REGISTERED && client->status != CLIENT_REQUESTING_JOIN) {
        LOG_WARN("Client FD=%d non registrato o già in partita (status=%d)", 
                 client_fd, client->status);
        error_code_t error = (client->status == CLIENT_CONNECTED) ? 
                             ERR_NOT_REGISTERED : ERR_ALREADY_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        send_list_games_error(client_fd, error);
        return;
    }

    // Conta partite in attesa (GAME_WAITING)
    int waiting_count = 0;
    for (int i = 0; i < server_state.max_games; i++) {
        if (server_state.games[i].active && 
            server_state.games[i].state.status == GAME_WAITING) {
            waiting_count++;
        }
    }
    
    // Calcola dimensione risposta
    size_t response_size = sizeof(response_list_games_t) + (waiting_count * sizeof(game_info_t));
    uint8_t *response_buffer = malloc(response_size);
    if (!response_buffer) {
        LOG_ERROR("Errore allocazione memoria per list_games");
        pthread_mutex_unlock(&server_state.mutex);
        send_list_games_error(client_fd, ERR_INTERNAL);
        return;
    }
    
    // Prepara risposta
    response_list_games_t *response = (response_list_games_t*)response_buffer;
    response->status = STATUS_OK;
    response->error_code = ERR_NONE;
    response->game_count = waiting_count;
    response->reserved = 0;
    
    // Riempi array partite     // Aritmetica dei puntatori
    game_info_t *games_array = (game_info_t*)(response_buffer + sizeof(response_list_games_t));
    int idx = 0;
    for (int i = 0; i < server_state.max_games && idx < waiting_count; i++) {
        if (server_state.games[i].active && 
            server_state.games[i].state.status == GAME_WAITING) {
            
            strncpy(games_array[idx].game_id, server_state.games[i].state.game_id, MAX_GAME_ID_LEN - 1);
            games_array[idx].game_id[MAX_GAME_ID_LEN - 1] = '\0';
            
            strncpy(games_array[idx].creator, server_state.games[i].state.players[0], MAX_PLAYER_NAME - 1);
            games_array[idx].creator[MAX_PLAYER_NAME - 1] = '\0';
            
            games_array[idx].status = GAME_WAITING; // Conosco per certo lo status
            games_array[idx].players_count = 1;  // Solo il creatore
            
            idx++;
        }
    }
    
    LOG_INFO("Lista partite per FD=%d: %d partite in attesa", client_fd, waiting_count);
    
    pthread_mutex_unlock(&server_state.mutex);
    
    // Invia risposta
    protocol_send(client_fd, MSG_RESPONSE, response_buffer, response_size, 0);
    free(response_buffer);
}

void handle_join_game(int client_fd, const void *payload, uint16_t length) {
    LOG_DEBUG("handle_join_game chiamato per FD=%d", client_fd);
    
    response_join_game_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    response.your_symbol = 'O';
    response.opponent[0] = '\0';
    response.game_id[0] = '\0';
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova il client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_WARN("Client FD=%d non trovato", client_fd);
        response.error_code = ERR_INTERNAL;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];

    // Deve essere registrato
    if (client->status != CLIENT_REGISTERED) {
        LOG_WARN("Client FD=%d non registrato o già in partita", client_fd);
        response.error_code = (client->status == CLIENT_IN_GAME) ? 
                             ERR_ALREADY_IN_GAME : ERR_INTERNAL;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Valida payload
    if (length < sizeof(payload_join_game_t)) {
        LOG_ERROR("Payload MSG_JOIN_GAME invalido");
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    const payload_join_game_t *join_req = (const payload_join_game_t*)payload;
    
    // Trova la partita
    int game_idx = find_game_by_id(join_req->game_id);
    if (game_idx == -1) {
        LOG_WARN("Partita '%s' non trovata", join_req->game_id);
        response.error_code = ERR_GAME_NOT_FOUND;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    game_session_t *game = &server_state.games[game_idx];
    
    // Controlla se è in attesa
    if (game->state.status != GAME_WAITING) {
        LOG_WARN("Partita '%s' non in attesa (status=%d)", join_req->game_id, game->state.status);
        response.error_code = ERR_GAME_FULL;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }

    // Controlla se c'è già una richiesta pendente
    if (game->pending_join_fd > 0) {
        LOG_WARN("Partita '%s' ha già una richiesta pendente", join_req->game_id);
        response.error_code = ERR_PENDING_JOIN_EXISTS;  
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Salva pending join
    game->pending_join_fd = client_fd;
    strncpy(game->pending_join_name, client->name, MAX_PLAYER_NAME - 1);
    game->pending_join_name[MAX_PLAYER_NAME - 1] = '\0';
    
    // Aggiorna stato client
    client->status = CLIENT_REQUESTING_JOIN;
    
    LOG_INFO("Client '%s' (FD=%d) vuole joinare partita '%s', in attesa di accept",
             client->name, client_fd, game->state.game_id);
    
    // Invia risposta OK al joiner
    response.status = STATUS_OK;
    response.error_code = ERR_NONE;
    strncpy(response.opponent, game->state.players[0], MAX_PLAYER_NAME - 1);
    response.opponent[MAX_PLAYER_NAME - 1] = '\0';
    strncpy(response.game_id, game->state.game_id, MAX_GAME_ID_LEN - 1);
    response.game_id[MAX_GAME_ID_LEN - 1] = '\0';
    
    pthread_mutex_unlock(&server_state.mutex);
    
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
    
    // Notifica al creatore
    pthread_mutex_lock(&server_state.mutex);
    notify_join_request(game->player_fds[0], client->name);
    pthread_mutex_unlock(&server_state.mutex);
}

void handle_accept_join(int client_fd, const void *payload, uint16_t length) {
    LOG_DEBUG("handle_accept_join chiamato per FD=%d", client_fd);
    
    response_accept_join_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova il client (creatore della partita)
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_WARN("Client FD=%d non trovato", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];

    // Deve essere in lobby
    if (client->status != CLIENT_IN_LOBBY) {
        LOG_WARN("Client FD=%d non in lobby", client_fd);
        response.error_code = ERR_NOT_IN_LOBBY;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    game_session_t *game = &server_state.games[client->game_index];

    // Controlla pending join
    if (!game->active || game->pending_join_fd <= 0) {
        LOG_WARN("Nessuna richiesta di join pendente per partita '%s'", game->state.game_id);
        response.error_code = ERR_NO_PENDING_JOIN;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Valida payload
    if (length < sizeof(payload_accept_join_t)) {
        LOG_ERROR("Payload MSG_ACCEPT_JOIN invalido");
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    const payload_accept_join_t *accept_req = (const payload_accept_join_t*)payload;
    int joiner_fd = game->pending_join_fd;
    char joiner_name[MAX_PLAYER_NAME];
    strncpy(joiner_name, game->pending_join_name, MAX_PLAYER_NAME - 1);
    joiner_name[MAX_PLAYER_NAME - 1] = '\0';
    
    if (accept_req->accept == 1) {
        // ACCETTA: aggiungi secondo giocatore
        if (game_add_player(&game->state, joiner_name)) {
            game->player_fds[1] = joiner_fd;
            
            // Aggiorna stato joiner
            int joiner_idx = find_client_by_fd(joiner_fd);
            if (joiner_idx != -1) {
                client_info_t *joiner = &server_state.clients[joiner_idx];
                joiner->game_index = client->game_index;
                joiner->player_index = 1;
                joiner->status = CLIENT_IN_GAME;
            }
            
            // Aggiorna stato creatore (da IN_LOBBY a IN_GAME)
            client->status = CLIENT_IN_GAME;
            
            LOG_INFO("Join accettato: partita '%s' ora con 2 giocatori", game->state.game_id);
            
            // Pulisci pending join
            game->pending_join_fd = -1;
            
            pthread_mutex_unlock(&server_state.mutex);
            
            // Invia risposte
            response.status = STATUS_OK;
            response.error_code = ERR_NONE;
            protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
            
            // Notifica al joiner: accettato
            pthread_mutex_lock(&server_state.mutex);
            notify_join_response(joiner_fd, game->state.game_id, 1);
            
            // Notifica inizio partita a entrambi
            notify_game_start(game);
            pthread_mutex_unlock(&server_state.mutex);
        } else {
            LOG_ERROR("Errore aggiunta giocatore alla partita");
            pthread_mutex_unlock(&server_state.mutex);
            protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        }
    } else {
        // RIFIUTA
        LOG_INFO("Join rifiutato da creatore per partita '%s'", game->state.game_id);
        
        // Resetta stato joiner
        int joiner_idx = find_client_by_fd(joiner_fd);
        if (joiner_idx != -1) {
            server_state.clients[joiner_idx].status = CLIENT_REGISTERED;
        }
        
        game->pending_join_fd = -1;
        
        pthread_mutex_unlock(&server_state.mutex);
        
        // Invia risposte
        response.status = STATUS_OK;
        response.error_code = ERR_NONE;
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        
        // Notifica al joiner: rifiutato
        pthread_mutex_lock(&server_state.mutex);
        notify_join_response(joiner_fd, game->state.game_id, 0);
        pthread_mutex_unlock(&server_state.mutex);
    }
}

void handle_make_move(int client_fd, const void *payload, uint16_t length) {
    LOG_DEBUG("handle_make_move chiamato per FD=%d", client_fd);
    
    response_make_move_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova client e partita
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_WARN("Client FD=%d non trovato", client_fd);
        response.error_code = ERR_NOT_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];
    
    if (client->game_index < 0) {
        LOG_WARN("Client FD=%d non in partita", client_fd);
        response.error_code = ERR_NOT_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    game_session_t *game = &server_state.games[client->game_index];
    if (!game->active) {
        LOG_ERROR("Partita non attiva per client FD=%d", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Valida payload
    if (length < sizeof(payload_make_move_t)) {
        LOG_ERROR("Payload MSG_MAKE_MOVE invalido");
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    const payload_make_move_t *move = (const payload_make_move_t*)payload;
    
    // Valida posizione
    if (!protocol_validate_move(move->pos)) {
        LOG_WARN("Posizione invalida: %d", move->pos);
        response.error_code = ERR_INVALID_MOVE;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Controlla se è il turno del giocatore
    if (!game_is_player_turn(&game->state, client->name)) {
        LOG_WARN("Non è il turno di '%s'", client->name);
        response.error_code = ERR_NOT_YOUR_TURN;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    // Effettua la mossa
    if (!game_make_move(&game->state, client->player_index, move->pos)) {
        LOG_WARN("Mossa non valida per '%s' pos=%d", client->name, move->pos);
        response.error_code = ERR_CELL_OCCUPIED;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    LOG_INFO("Mossa effettuata: giocatore='%s', pos=%d, partita='%s'",
             client->name, move->pos, game->state.game_id);
    
    // Mossa OK
    response.status = STATUS_OK;
    response.error_code = ERR_NONE;
    
    // Trova l'avversario
    int opponent_idx = 1 - client->player_index;
    int opponent_fd = game->player_fds[opponent_idx];
    
    // Prepara board per notifiche
    char board_str[BOARD_SIZE];
    game_get_board_string(&game->state, board_str);
    
    pthread_mutex_unlock(&server_state.mutex);
    
    // Invia risposta al giocatore
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Controlla se la partita è finita
    if (game_is_finished(&game->state)) {
        LOG_INFO("Partita '%s' terminata", game->state.game_id);
        
        // Notifica fine partita a entrambi
        for (int i = 0; i < 2; i++) {
            notify_game_end_t notify;
            notify.notify_type = NOTIFY_GAME_END;
            memcpy(notify.board, board_str, BOARD_SIZE);
            
            // Determina risultato per questo giocatore
            if (game->state.winner == 2) {
                notify.result = RESULT_DRAW;
            } else if (game->state.winner == i) {
                notify.result = RESULT_WIN;
            } else {
                notify.result = RESULT_LOSE;
            }
            
            protocol_send(game->player_fds[i], MSG_NOTIFY, &notify, sizeof(notify), 0);
            LOG_DEBUG("GAME_END inviato a FD=%d, result=%d", game->player_fds[i], notify.result);
        }
        
        // Cleanup partita
        cleanup_game(game);
    } else {
        // Partita continua: notifica mossa all'avversario
        notify_move_made_t notify_move;
        notify_move.notify_type = NOTIFY_MOVE_MADE;
        notify_move.pos = move->pos;
        notify_move.symbol = game_get_player_symbol(&game->state, client->player_index);
        memcpy(notify_move.board, board_str, BOARD_SIZE);
        
        protocol_send(opponent_fd, MSG_NOTIFY, &notify_move, sizeof(notify_move), 0);
        LOG_DEBUG("MOVE_MADE inviato a FD=%d", opponent_fd);
    }
    
    pthread_mutex_unlock(&server_state.mutex);
}

void handle_leave_game(int client_fd) {
    LOG_DEBUG("handle_leave_game chiamato per FD=%d", client_fd);
    
    response_leave_game_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_WARN("Client FD=%d non trovato", client_fd);
        response.error_code = ERR_NOT_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }

    client_info_t *client = &server_state.clients[client_idx];
    if (client->game_index < 0) {
        LOG_WARN("Client FD=%d non in partita", client_fd);
        response.error_code = ERR_NOT_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    game_session_t *game = &server_state.games[client->game_index];
    if (!game->active) {
        LOG_ERROR("Partita non attiva");
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
        return;
    }
    
    LOG_INFO("Client '%s' (FD=%d) abbandona partita '%s'", 
             client->name, client_fd, game->state.game_id);
    
    // Trova avversario
    int opponent_idx = 1 - client->player_index;
    int opponent_fd = game->player_fds[opponent_idx];
    
    // Pulisci la partita (cleanup_game resetta anche l'avversario a REGISTERED)
    cleanup_game(game);
    
    pthread_mutex_unlock(&server_state.mutex);
    
    // Invia risposta
    response.status = STATUS_OK;
    response.error_code = ERR_NONE;
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
    
    // Notifica avversario
    if (opponent_fd > 0) {
        notify_opponent_left_t notify;
        notify.notify_type = NOTIFY_OPPONENT_LEFT;
        protocol_send(opponent_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
        LOG_INFO("OPPONENT_LEFT inviato a FD=%d", opponent_fd);
    }
}

void handle_quit(int client_fd) {
    LOG_DEBUG("handle_quit chiamato per FD=%d", client_fd);
    
    response_quit_t response;
    response.status = STATUS_OK;
    response.error_code = ERR_NONE;
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova il client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx != -1) {
        client_info_t *client = &server_state.clients[client_idx];
        
        // Se il client è in una partita, notifica l'avversario PRIMA di disconnettersi
        if (client->game_index >= 0) {
            game_session_t *game = &server_state.games[client->game_index];
            
            if (game->active) {
                // Trova l'avversario
                int opponent_fd = -1;
                if (game->player_fds[0] == client_fd) {
                    opponent_fd = game->player_fds[1];
                } else if (game->player_fds[1] == client_fd) {
                    opponent_fd = game->player_fds[0];
                }
                
                // Notifica l'avversario che il giocatore ha abbandonato
                if (opponent_fd > 0) {
                    notify_opponent_left_t notify;
                    notify.notify_type = NOTIFY_OPPONENT_LEFT;
                    protocol_send(opponent_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
                    
                    LOG_INFO("QUIT: Notifica OPPONENT_LEFT inviata a FD=%d (client '%s' ha quitato)",
                             opponent_fd, client->name);
                }
            }
        }
    }
    
    pthread_mutex_unlock(&server_state.mutex);
    
    // Invia risposta prima di chiudere
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), 0);
    
    LOG_INFO("Client FD=%d ha richiesto disconnessione", client_fd);
    
    // Il cleanup sarà fatto da remove_client() nel chiamante
}

// ============================================================================
// FUNZIONI DI NOTIFICA
// ============================================================================

void broadcast_to_registered_clients(uint8_t msg_type, const void *payload, size_t payload_size) {
    for (int i = 0; i < server_state.max_clients; i++) {
        if (server_state.clients[i].status == CLIENT_REGISTERED) {
            ssize_t sent = protocol_send(server_state.clients[i].fd, msg_type, 
                                        payload, payload_size, 0);
            if (sent > 0) {
                LOG_DEBUG("Broadcast inviato a client FD=%d (%s)", 
                         server_state.clients[i].fd, server_state.clients[i].name);
            } else {
                LOG_WARN("Errore invio broadcast a FD=%d", server_state.clients[i].fd);
            }
        }
    }
}

void notify_join_request(int creator_fd, const char *joiner_name) {
    notify_join_request_t notify;
    notify.notify_type = NOTIFY_JOIN_REQUEST;
    strncpy(notify.opponent, joiner_name, MAX_PLAYER_NAME - 1);
    notify.opponent[MAX_PLAYER_NAME - 1] = '\0';
    
    protocol_send(creator_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
    LOG_INFO("Notifica JOIN_REQUEST inviata a FD=%d: joiner='%s'", 
             creator_fd, joiner_name);
}

void notify_join_response(int joiner_fd, const char *game_id, int accepted) {
    notify_join_response_t notify;
    notify.notify_type = NOTIFY_JOIN_RESPONSE;
    notify.accepted = accepted ? 1 : 0;
    strncpy(notify.game_id, game_id, MAX_GAME_ID_LEN - 1);
    notify.game_id[MAX_GAME_ID_LEN - 1] = '\0';
    
    protocol_send(joiner_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
    LOG_INFO("Notifica JOIN_RESPONSE inviata a FD=%d: game_id='%s', accepted=%d",
             joiner_fd, game_id, accepted);
}

void notify_game_start(game_session_t *game) {
    if (!game || !game->active) return;
    
    for (int i = 0; i < 2; i++) {
        if (game->player_fds[i] <= 0) continue;
        
        notify_game_start_t notify;
        notify.notify_type = NOTIFY_GAME_START;
        notify.your_symbol = game_get_player_symbol(&game->state, i);
        notify.first_player = PLAYER_X;  // X inizia sempre
        
        // Imposta il nome dell'avversario
        int opponent_idx = 1 - i;
        strncpy(notify.opponent, game->state.players[opponent_idx], MAX_PLAYER_NAME - 1);
        notify.opponent[MAX_PLAYER_NAME - 1] = '\0';
        
        protocol_send(game->player_fds[i], MSG_NOTIFY, &notify, sizeof(notify), 0);
        LOG_INFO("Notifica GAME_START inviata a FD=%d: symbol='%c', opponent='%s'",
                 game->player_fds[i], notify.your_symbol, notify.opponent);
    }
}