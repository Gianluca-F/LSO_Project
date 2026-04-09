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

// ============================================================================
// STATO GLOBALE DEL SERVER
// ============================================================================

server_state_t server_state;

static void append_chat_message(game_session_t *game, const char *player, const char *message) {
    if (!game || !player || !message) {
        return;
    }

    uint8_t index;
    if (game->chat_count < MAX_CHAT_HISTORY_MESSAGES) {
        index = (uint8_t)((game->chat_start + game->chat_count) % MAX_CHAT_HISTORY_MESSAGES);
        game->chat_count++;
    } else {
        index = game->chat_start;
        game->chat_start = (uint8_t)((game->chat_start + 1) % MAX_CHAT_HISTORY_MESSAGES);
    }

    strncpy(game->chat_history[index].player, player, MAX_PLAYER_NAME - 1);
    game->chat_history[index].player[MAX_PLAYER_NAME - 1] = '\0';

    strncpy(game->chat_history[index].message, message, MAX_CHAT_MESSAGE_LEN - 1);
    game->chat_history[index].message[MAX_CHAT_MESSAGE_LEN - 1] = '\0';
}

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
        server_state.games[i].last_result = RESULT_NONE;
        server_state.games[i].rematch_requested[0] = 0;
        server_state.games[i].rematch_requested[1] = 0;
        server_state.games[i].chat_count = 0;
        server_state.games[i].chat_start = 0;
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

        if (is_full) { // Server pieno: rifiuta immediatamente senza creare thread
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
            LOG_ERROR("Creazione thread fallita per FD=%d: %s", new_client_fd, strerror(errno));
            perror("pthread_create");
            close(new_client_fd);
            free(client_fd);
        } else {
            LOG_DEBUG("Thread creato per gestire client FD=%d", new_client_fd);
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
    
    // Questo non dovrebbe mai accadere perché controlliamo prima in start_server
    if (client_idx == -1) {
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
            pthread_mutex_lock(&server_state.mutex);
            // Auto-cleanup se il client è in una partita finita per pareggio
            auto_cleanup_finished_draw_game(client_fd);
            cleanup_notify_data_t notify_data = cleanup_client_from_game_state(client_fd);
            pthread_mutex_unlock(&server_state.mutex);
            send_notify_after_cleanup_client(notify_data);
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
                handle_register(client_fd, payload, header.length, header.seq_id);
                break;
                
            case MSG_CREATE_GAME:
                handle_create_game(client_fd, header.seq_id);
                break;
                
            case MSG_LIST_GAMES:
                handle_list_games(client_fd, header.seq_id);
                break;
                
            case MSG_JOIN_GAME:
                handle_join_game(client_fd, payload, header.length, header.seq_id);
                break;
                
            case MSG_ACCEPT_JOIN:
                handle_accept_join(client_fd, payload, header.length, header.seq_id);
                break;
                
            case MSG_MAKE_MOVE:
                handle_make_move(client_fd, payload, header.length, header.seq_id);
                break;

            case MSG_SEND_MESSAGE:
                handle_send_message(client_fd, payload, header.length, header.seq_id);
                break;

            case MSG_GET_CHAT_HISTORY:
                handle_get_chat_history(client_fd, header.seq_id);
                break;
                
            case MSG_LEAVE_GAME:
                handle_leave_game(client_fd, header.seq_id);
                break;

            case MSG_REMATCH:
                handle_rematch(client_fd, header.seq_id);
                break;
                
            case MSG_QUIT:
                handle_quit(client_fd, header.seq_id);
                free(payload);
                should_run = false; 
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
    server_state.clients[slot].seq_id = 0;
    
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

    LOG_INFO("Rimozione client FD=%d, totale client rimanenti=%d", 
             fd, server_state.num_clients);
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

int find_game_by_client_fd(int fd) { //NOTE: future proof
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
            game_session_t *game = &server_state.games[i];
            
            // Genera un game_id univoco usando timestamp + indice
            char game_id[MAX_GAME_ID_LEN];
            snprintf(game_id, MAX_GAME_ID_LEN, "G%06ld%02d", time(NULL) % 1000000, i % 100);
            
            // Inizializza il game state (da game_logic.h)
            game_init(&game->state, game_id, creator_name);
            
            // Imposta i FD dei giocatori
            game->player_fds[0] = creator_fd;
            game->player_fds[1] = -1;  // Ancora nessun secondo giocatore
            
            // Nessun pending join inizialmente
            game->pending_join_fd = -1;
            game->pending_join_name[0] = '\0';
            
            // Marca come attiva
            game->active = 1;
            game->state.status = GAME_WAITING;
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
    game->pending_join_name[0] = '\0';
    game->last_result = RESULT_NONE;
    game->rematch_requested[0] = 0;
    game->rematch_requested[1] = 0;
    game->chat_count = 0;
    game->chat_start = 0;
    server_state.num_games--;
    
    LOG_INFO("Partita pulita, totale partite rimanenti=%d", server_state.num_games);
}

int auto_cleanup_finished_draw_game(int client_fd) {
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) return 0;
    
    client_info_t *client = &server_state.clients[client_idx];
    
    // Il client deve essere in partita
    if (client->status != CLIENT_IN_GAME || client->game_index == -1) {
        return 0;
    }
    
    game_session_t *game = &server_state.games[client->game_index];
    
    // Verifica se la partita è finita per pareggio
    if (!game->active || game->last_result != RESULT_DRAW) {
        return 0;
    }
    
    LOG_INFO("Auto-cleanup partita finita per pareggio: client '%s' (FD=%d) esce dalla partita",
             client->name, client_fd);
    
    // Determina l'avversario
    int opponent_fd = -1;
    for (int i = 0; i < 2; i++) {
        if (game->player_fds[i] != client_fd) {
            opponent_fd = game->player_fds[i];
            break;
        }
    }
    
    // Cleanup della partita
    cleanup_game(game);
    
    // Notifica l'avversario che non ci sarà rematch (l'altro giocatore ha fatto un altro comando)
    if (opponent_fd > 0) {
        notify_no_rematch_t notify;
        notify.notify_type = NOTIFY_NO_REMATCH;
        
        protocol_send(opponent_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
        LOG_DEBUG("NO_REMATCH inviato a FD=%d (avversario ha lasciato partita finita)", opponent_fd);
    }
    
    return 1;
}

// ============================================================================
// HANDLER MESSAGGI PROTOCOLLO
// ============================================================================

void handle_register(int client_fd, const void *payload, uint16_t length, uint32_t req_seq_id) {
    LOG_DEBUG("handle_register chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);
    
    response_register_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova il client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_ERROR("Client FD=%d non trovato in handle_register", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];
    
    // Controlla se è già registrato
    if (client->status != CLIENT_CONNECTED) {
        LOG_WARN("Client FD=%d già registrato con nome '%s'", client_fd, client->name);
        response.error_code = ERR_ALREADY_REGISTERED;  
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    // Valida il payload
    if (length < sizeof(payload_register_t)) {
        LOG_ERROR("Payload MSG_REGISTER invalido: length=%d, expected=%zu", 
                 length, sizeof(payload_register_t));
        response.error_code = ERR_INVALID_PAYLOAD;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    const payload_register_t *reg = (const payload_register_t*)payload;
    
    // Valida il nome
    if (!protocol_validate_name(reg->player_name)) {
        LOG_WARN("Nome giocatore non valido: '%s'", reg->player_name);
        response.error_code = ERR_INVALID_NAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    // Controlla se il nome è già usato
    if (find_client_by_name(reg->player_name) != -1) {
        LOG_WARN("Nome '%s' già in uso", reg->player_name);
        response.error_code = ERR_NAME_TAKEN;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
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
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
}

void handle_create_game(int client_fd, uint32_t req_seq_id) {
    LOG_DEBUG("handle_create_game chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);
    
    response_create_game_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    response.game_id[0] = '\0';
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Auto-cleanup se il client è in una partita finita per pareggio
    auto_cleanup_finished_draw_game(client_fd);
    
    // Trova il client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_ERROR("Client FD=%d non trovato in handle_create_game", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
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
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    // Crea la partita
    int game_index = create_game(client->name, client_fd);
    if (game_index == -1) {
        LOG_ERROR("Impossibile creare partita per client FD=%d", client_fd);
        response.error_code = ERR_SERVER_FULL;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
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
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
    
    // Broadcast ai client registrati
    broadcast_to_registered_clients(MSG_NOTIFY, &notify, sizeof(notify));
    
    LOG_INFO("Broadcast GAME_CREATED inviato per partita '%s'", game->game_id);
}

void handle_list_games(int client_fd, uint32_t req_seq_id) {
    LOG_DEBUG("handle_list_games chiamato da FD=%d, req_seq=%u", client_fd, req_seq_id);
    
    pthread_mutex_lock(&server_state.mutex);

    // Auto-cleanup se il client è in una partita finita per pareggio
    auto_cleanup_finished_draw_game(client_fd);

    // Trova il client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_ERROR("Client FD=%d non trovato in handle_list_games", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        send_list_games_error(client_fd, ERR_INTERNAL, req_seq_id);
        return;
    }

    client_info_t *client = &server_state.clients[client_idx];

    // Controlla se è registrato o in attesa di join
    if (client->status != CLIENT_REGISTERED && client->status != CLIENT_REQUESTING_JOIN) {
        LOG_WARN("Client FD=%d non registrato o già in partita (status=%d)", 
                 client_fd, client->status);
        error_code_t error = (client->status == CLIENT_CONNECTED) ? 
                             ERR_NOT_REGISTERED : ERR_ALREADY_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        send_list_games_error(client_fd, error, req_seq_id);
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
        send_list_games_error(client_fd, ERR_INTERNAL, req_seq_id);
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
    protocol_send(client_fd, MSG_RESPONSE, response_buffer, response_size, req_seq_id);
    free(response_buffer);
}

void handle_join_game(int client_fd, const void *payload, uint16_t length, uint32_t req_seq_id) {
    LOG_DEBUG("handle_join_game chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);
    
    response_join_game_t response;
    memset(&response, 0, sizeof(response));  // Valgrind si arrabbia
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    response.your_symbol = SECOND_PLAYER_SYMBOL;
    response.opponent[0] = '\0';
    response.game_id[0] = '\0';
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Auto-cleanup se il client è in una partita finita per pareggio
    auto_cleanup_finished_draw_game(client_fd);
    
    // Trova il client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_WARN("Client FD=%d non trovato", client_fd);
        response.error_code = ERR_INTERNAL;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];

    // Deve essere registrato
    if (client->status != CLIENT_REGISTERED) {
        LOG_WARN("Client FD=%d non registrato o già in partita", client_fd);
        response.error_code = (client->status == CLIENT_IN_GAME) ? 
                             ERR_ALREADY_IN_GAME : ERR_INTERNAL;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    // Valida payload
    if (length < sizeof(payload_join_game_t)) {
        LOG_ERROR("Payload MSG_JOIN_GAME invalido");
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    const payload_join_game_t *join_req = (const payload_join_game_t*)payload;
    
    // Trova la partita
    int game_idx = find_game_by_id(join_req->game_id);
    if (game_idx == -1) {
        LOG_WARN("Partita '%s' non trovata", join_req->game_id);
        response.error_code = ERR_GAME_NOT_FOUND;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    game_session_t *game = &server_state.games[game_idx];
    
    // Controlla se è in attesa
    if (game->state.status != GAME_WAITING) {
        LOG_WARN("Partita '%s' non in attesa (status=%d)", join_req->game_id, game->state.status);
        response.error_code = ERR_GAME_FULL;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }

    // Controlla se c'è già una richiesta pendente
    if (game->pending_join_fd > 0) {
        LOG_WARN("Partita '%s' ha già una richiesta pendente", join_req->game_id);
        response.error_code = ERR_PENDING_JOIN_EXISTS;  
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
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

    // --- Prepara dati per notifica al creatore ---
    int creator_fd = game->player_fds[0];
    char joiner_name[MAX_PLAYER_NAME];
    strncpy(joiner_name, client->name, MAX_PLAYER_NAME - 1);
    joiner_name[MAX_PLAYER_NAME - 1] = '\0';
    // --- (notify non deve essere dentro mutex) ---

    pthread_mutex_unlock(&server_state.mutex);
    
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
    
    // Notifica al creatore della richiesta di join
    notify_join_request(creator_fd, joiner_name);
}

void handle_accept_join(int client_fd, const void *payload, uint16_t length, uint32_t req_seq_id) {
    LOG_DEBUG("handle_accept_join chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);
    
    response_accept_join_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova il client (creatore della partita)
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_WARN("Client FD=%d non trovato", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];

    // Deve essere in lobby
    if (client->status != CLIENT_IN_LOBBY) {
        LOG_WARN("Client FD=%d non in lobby", client_fd);
        response.error_code = ERR_NOT_IN_LOBBY;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    game_session_t *game = &server_state.games[client->game_index];

    // Controlla pending join
    if (!game->active || game->pending_join_fd <= 0) {
        LOG_WARN("Nessuna richiesta di join pendente per partita '%s'", game->state.game_id);
        response.error_code = ERR_NO_PENDING_JOIN;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    // Valida payload
    if (length < sizeof(payload_accept_join_t)) {
        LOG_ERROR("Payload MSG_ACCEPT_JOIN invalido");
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
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

            // -------- Prepara dati unicamente per le notifiche --------
            int player_fds[2] = { game->player_fds[0], game->player_fds[1] };
            char player_names[2][MAX_PLAYER_NAME];
            strncpy(player_names[0], game->state.players[0], MAX_PLAYER_NAME - 1);
            player_names[0][MAX_PLAYER_NAME - 1] = '\0';
            strncpy(player_names[1], game->state.players[1], MAX_PLAYER_NAME - 1);
            player_names[1][MAX_PLAYER_NAME - 1] = '\0';

            char game_id[MAX_GAME_ID_LEN];
            strncpy(game_id, game->state.game_id, MAX_GAME_ID_LEN - 1);
            game_id[MAX_GAME_ID_LEN - 1] = '\0';
            // ------ (notify non deve essere dentro mutex) ------
            
            pthread_mutex_unlock(&server_state.mutex);
            
            // Invia risposte
            response.status = STATUS_OK;
            response.error_code = ERR_NONE;
            protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
            
            // Notifica al joiner: accettato
            notify_join_response(joiner_fd, game_id, 1);
            
            // Notifica inizio partita a entrambi
            const char *player_names_ptrs[2] = {player_names[0], player_names[1]};
            notify_game_start(player_fds, player_names_ptrs);
        } else {
            LOG_ERROR("Errore aggiunta giocatore alla partita");
            pthread_mutex_unlock(&server_state.mutex);
            protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
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

        // ------ Prepara dati unicamente per le notifiche ------
        char game_id[MAX_GAME_ID_LEN];
        strncpy(game_id, game->state.game_id, MAX_GAME_ID_LEN - 1);
        game_id[MAX_GAME_ID_LEN - 1] = '\0';
        // ------ (notify non deve essere dentro mutex) ------
        
        pthread_mutex_unlock(&server_state.mutex);
        
        // Invia risposte
        response.status = STATUS_OK;
        response.error_code = ERR_NONE;
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        
        // Notifica al joiner: rifiutato
        notify_join_response(joiner_fd, game_id, 0);
    }
}

void handle_make_move(int client_fd, const void *payload, uint16_t length, uint32_t req_seq_id) {
    LOG_DEBUG("handle_make_move chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);
    
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
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];
    
    // Deve essere in partita
    if (client->status != CLIENT_IN_GAME) {
        LOG_WARN("Client FD=%d non in partita", client_fd);
        response.error_code = ERR_NOT_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    game_session_t *game = &server_state.games[client->game_index];

    // Controlla se la partita è attiva
    if (!game->active) {
        LOG_ERROR("Partita non attiva per client FD=%d", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    // Valida payload
    if (length < sizeof(payload_make_move_t)) {
        LOG_ERROR("Payload MSG_MAKE_MOVE invalido");
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    const payload_make_move_t *move = (const payload_make_move_t*)payload;
    
    // Valida posizione
    if (!protocol_validate_move(move->pos)) {
        LOG_WARN("Posizione invalida: %d", move->pos);
        response.error_code = ERR_INVALID_MOVE;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    // Controlla se è il turno del giocatore
    if (!game_is_player_turn(&game->state, client->name)) {
        LOG_WARN("Non è il turno di '%s'", client->name);
        response.error_code = ERR_NOT_YOUR_TURN;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    // Effettua la mossa
    if (!game_make_move(&game->state, client->player_index, move->pos)) {
        LOG_WARN("Mossa non valida per '%s' pos=%d", client->name, move->pos);
        response.error_code = ERR_CELL_OCCUPIED;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
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
    
    // --- Copia dati di gioco per notifiche prima di rilasciare il lock ---
    char board_str[BOARD_SIZE];
    memcpy(board_str, game->state.board, BOARD_SIZE);
    
    int player_fds[2] = {game->player_fds[0], game->player_fds[1]};
    int winner = game->state.winner;
    int is_finished = game_is_finished(&game->state);

    int move_pos = move->pos;
    char player_name[MAX_PLAYER_NAME];
    strncpy(player_name, client->name, MAX_PLAYER_NAME - 1);
    player_name[MAX_PLAYER_NAME - 1] = '\0';
    // --- (notifiche non devono essere dentro mutex) ---
    
    pthread_mutex_unlock(&server_state.mutex);
    
    // Controlla se la partita è finita
    if (is_finished) {
        LOG_INFO("Partita terminata");
        
        // Notifica fine partita a entrambi
        for (int i = 0; i < 2; i++) {
            notify_game_end_t notify;
            notify.notify_type = NOTIFY_GAME_END;
            memcpy(notify.board, board_str, BOARD_SIZE);
            
            // Determina risultato per questo giocatore
            if (winner == 2) {
                notify.result = RESULT_DRAW;
            } else if (winner == i) {
                notify.result = RESULT_WIN;
            } else {
                notify.result = RESULT_LOSE;
            }
            
            protocol_send(player_fds[i], MSG_NOTIFY, &notify, sizeof(notify), 0);
            LOG_DEBUG("GAME_END inviato a FD=%d, result=%d", player_fds[i], notify.result);
        }
        
        pthread_mutex_lock(&server_state.mutex);
        
        // Se pareggio: mantieni partita per rematch
        if (winner == 2) {
            game->last_result = RESULT_DRAW;
            game->rematch_requested[0] = 0;
            game->rematch_requested[1] = 0;
            LOG_INFO("Pareggio: partita mantenuta per rematch (game_id='%s')", game->state.game_id);
        } else {
            // Vittoria/sconfitta: cleanup immediato
            cleanup_game(game);
        }
        
        pthread_mutex_unlock(&server_state.mutex);
    } else {
        // Invia risposta al giocatore
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
    
        // Partita continua: notifica mossa all'avversario
        notify_move_made_t notify_move;
        notify_move.notify_type = NOTIFY_MOVE_MADE;
        notify_move.pos = move_pos;
        strncpy(notify_move.player, player_name, MAX_PLAYER_NAME - 1);
        notify_move.player[MAX_PLAYER_NAME - 1] = '\0';
        memcpy(notify_move.board, board_str, BOARD_SIZE);
        
        protocol_send(opponent_fd, MSG_NOTIFY, &notify_move, sizeof(notify_move), 0);
        LOG_DEBUG("MOVE_MADE inviato a FD=%d", opponent_fd);
    }

}

void handle_send_message(int client_fd, const void *payload, uint16_t length, uint32_t req_seq_id) {
    LOG_DEBUG("handle_send_message chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);
    
    response_send_message_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova client e partita
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_WARN("Client FD=%d non trovato", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    client_info_t *client = &server_state.clients[client_idx];
    
    // Deve essere in partita
    if (client->status != CLIENT_IN_GAME) {
        LOG_WARN("Client FD=%d non in partita", client_fd);
        response.error_code = ERR_NOT_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    game_session_t *game = &server_state.games[client->game_index];

    // Controlla se la partita è attiva
    if (!game->active) {
        LOG_ERROR("Partita non attiva per client FD=%d", client_fd);
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    // Valida payload
    if (length < sizeof(payload_send_message_t)) {
        LOG_ERROR("Payload MSG_SEND_MESSAGE invalido");
        response.error_code = ERR_INVALID_PAYLOAD;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    const payload_send_message_t *msg_req = (const payload_send_message_t*)payload;

    char sender_name[MAX_PLAYER_NAME];
    strncpy(sender_name, client->name, MAX_PLAYER_NAME - 1);
    sender_name[MAX_PLAYER_NAME - 1] = '\0';

    char message_text[MAX_CHAT_MESSAGE_LEN];
    strncpy(message_text, msg_req->message, MAX_CHAT_MESSAGE_LEN - 1);
    message_text[MAX_CHAT_MESSAGE_LEN - 1] = '\0';

    append_chat_message(game, sender_name, message_text);

    // Invia risposta di successo al mittente
    response.status = STATUS_OK;
    response.error_code = ERR_NONE;
    
    int opponent_idx = 1 - client->player_index;
    int opponent_fd = game->player_fds[opponent_idx];
    
    pthread_mutex_unlock(&server_state.mutex);
    
    notify_message_sent_t notify_msg;
    notify_msg.notify_type = NOTIFY_MESSAGE_SENT;
    strncpy(notify_msg.player, sender_name, MAX_PLAYER_NAME - 1);
    notify_msg.player[MAX_PLAYER_NAME - 1] = '\0';

    protocol_send(opponent_fd, MSG_NOTIFY, &notify_msg, sizeof(notify_msg), 0);
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
    LOG_DEBUG("MESSAGE_SENT inviato a FD=%d", opponent_fd);
}

void handle_get_chat_history(int client_fd, uint32_t req_seq_id) {
    LOG_DEBUG("handle_get_chat_history chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);

    pthread_mutex_lock(&server_state.mutex);

    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        pthread_mutex_unlock(&server_state.mutex);
        response_chat_history_t error_response;
        error_response.status = STATUS_ERROR;
        error_response.error_code = ERR_INTERNAL;
        error_response.message_count = 0;
        error_response.reserved = 0;
        protocol_send(client_fd, MSG_RESPONSE, &error_response, sizeof(error_response), req_seq_id);
        return;
    }

    client_info_t *client = &server_state.clients[client_idx];
    if (client->status != CLIENT_IN_GAME || client->game_index < 0) {
        pthread_mutex_unlock(&server_state.mutex);
        response_chat_history_t error_response;
        error_response.status = STATUS_ERROR;
        error_response.error_code = ERR_NOT_IN_GAME;
        error_response.message_count = 0;
        error_response.reserved = 0;
        protocol_send(client_fd, MSG_RESPONSE, &error_response, sizeof(error_response), req_seq_id);
        return;
    }

    game_session_t *game = &server_state.games[client->game_index];
    if (!game->active) {
        pthread_mutex_unlock(&server_state.mutex);
        response_chat_history_t error_response;
        error_response.status = STATUS_ERROR;
        error_response.error_code = ERR_GAME_NOT_FOUND;
        error_response.message_count = 0;
        error_response.reserved = 0;
        protocol_send(client_fd, MSG_RESPONSE, &error_response, sizeof(error_response), req_seq_id);
        return;
    }

    uint8_t message_count = game->chat_count;
    size_t payload_size = sizeof(response_chat_history_t) +
                          ((size_t)message_count * sizeof(chat_message_entry_t));
    uint8_t *payload = (uint8_t *)malloc(payload_size);
    if (!payload) {
        pthread_mutex_unlock(&server_state.mutex);
        response_chat_history_t error_response;
        error_response.status = STATUS_ERROR;
        error_response.error_code = ERR_INTERNAL;
        error_response.message_count = 0;
        error_response.reserved = 0;
        protocol_send(client_fd, MSG_RESPONSE, &error_response, sizeof(error_response), req_seq_id);
        return;
    }

    response_chat_history_t *response = (response_chat_history_t *)payload;
    response->status = STATUS_OK;
    response->error_code = ERR_NONE;
    response->message_count = message_count;
    response->reserved = 0;

    chat_message_entry_t *messages = (chat_message_entry_t *)(payload + sizeof(response_chat_history_t));
    for (uint8_t i = 0; i < message_count; i++) {
        uint8_t ring_idx = (uint8_t)((game->chat_start + i) % MAX_CHAT_HISTORY_MESSAGES);
        messages[i] = game->chat_history[ring_idx];
    }

    pthread_mutex_unlock(&server_state.mutex);

    protocol_send(client_fd, MSG_RESPONSE, payload, payload_size, req_seq_id);
    free(payload);
}

void handle_leave_game(int client_fd, uint32_t req_seq_id) {
    LOG_DEBUG("handle_leave_game chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);
    
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
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }

    client_info_t *client = &server_state.clients[client_idx];

    // Deve essere in partita, in lobby o richiedendo join
    if (client->status != CLIENT_IN_GAME && 
        client->status != CLIENT_IN_LOBBY && 
        client->status != CLIENT_REQUESTING_JOIN) {
        LOG_WARN("Client FD=%d non in partita/lobby/requesting (status=%d)", 
                 client_fd, client->status);
        response.error_code = ERR_NOT_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    LOG_INFO("Client '%s' (FD=%d) lascia volontariamente (status=%d)", 
             client->name, client_fd, client->status);
    
    cleanup_notify_data_t notify_data = cleanup_client_from_game_state(client_fd);
    
    pthread_mutex_unlock(&server_state.mutex);

    // Invia risposta di successo
    response.status = STATUS_OK;
    response.error_code = ERR_NONE;
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);

    // Invia notifica all'eventuale client avversario
    send_notify_after_cleanup_client(notify_data);
}

void handle_rematch(int client_fd, uint32_t req_seq_id) {
    LOG_DEBUG("handle_rematch chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);
    
    response_rematch_t response;
    response.status = STATUS_ERROR;
    response.error_code = ERR_INTERNAL;
    memset(response.game_id, 0, MAX_GAME_ID_LEN);
    
    pthread_mutex_lock(&server_state.mutex);
    
    // Trova client
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_WARN("Client FD=%d non trovato", client_fd);
        response.error_code = ERR_NOT_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }

    client_info_t *client = &server_state.clients[client_idx];

    // Deve essere in partita
    if (client->status != CLIENT_IN_GAME || client->game_index == -1) {
        LOG_WARN("Client FD=%d non in partita", client_fd);
        response.error_code = ERR_NOT_IN_GAME;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    game_session_t *game = &server_state.games[client->game_index];
    
    // Deve essere una partita finita con pareggio
    if (!game->active || !(game_is_finished(&game->state)) || game->last_result != RESULT_DRAW) {
        LOG_WARN("Partita non disponibile per rematch per client FD=%d (result=%d)", client_fd, game->last_result);
        response.error_code = ERR_GAME_NOT_FINISHED;
        pthread_mutex_unlock(&server_state.mutex);
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        return;
    }
    
    int player_idx = client->player_index;
    int opponent_idx = 1 - player_idx;
    int opponent_fd = game->player_fds[opponent_idx];
    
    LOG_INFO("Rematch richiesto da '%s' (FD=%d, player=%d) in partita '%s'", 
             client->name, client_fd, player_idx, game->state.game_id);
    
    // Marca che questo giocatore ha richiesto rematch
    game->rematch_requested[player_idx] = 1;
    
    // Copia game_id per la risposta
    strncpy(response.game_id, game->state.game_id, MAX_GAME_ID_LEN - 1);
    response.game_id[MAX_GAME_ID_LEN - 1] = '\0';
    
    // Verifica se entrambi hanno richiesto rematch
    if (game->rematch_requested[0] && game->rematch_requested[1]) {
        LOG_INFO("Entrambi i giocatori hanno accettato il rematch, riavvio partita '%s'", game->state.game_id);
        
        // Inverto chi inizia la nuova partita
        int player0_fd = game->player_fds[1];
        int player1_fd = game->player_fds[0];
        game->player_fds[0] = player0_fd; 

        char player0_name[MAX_PLAYER_NAME];
        strncpy(player0_name, game->state.players[1], MAX_PLAYER_NAME - 1);
        player0_name[MAX_PLAYER_NAME - 1] = '\0';

        char player1_name[MAX_PLAYER_NAME];
        strncpy(player1_name, game->state.players[0], MAX_PLAYER_NAME - 1); 
        player1_name[MAX_PLAYER_NAME - 1] = '\0';

        // Resetta lo stato della partita
        game->last_result = RESULT_NONE;
        game->rematch_requested[0] = 0;
        game->rematch_requested[1] = 0;

        game_init(&game->state, game->state.game_id, player0_name);
         
        if (game_add_player(&game->state, player1_name)) {  
            game->player_fds[1] = player1_fd;
            
            // Aggiorna stato players
            int player0_idx = find_client_by_fd(player0_fd);
            if (player0_idx != -1) {
                client_info_t *player0 = &server_state.clients[player0_idx];
                player0->player_index = 0;
            }

            int player1_idx = find_client_by_fd(player1_fd);
            if (player1_idx != -1) {
                client_info_t *player1 = &server_state.clients[player1_idx];
                player1->player_index = 1;
            }
            
            // -------- Prepara dati unicamente per le notifiche --------
            int player_fds[2] = { player0_fd, player1_fd };
            char player_names[2][MAX_PLAYER_NAME];
            strncpy(player_names[0], game->state.players[0], MAX_PLAYER_NAME - 1);
            player_names[0][MAX_PLAYER_NAME - 1] = '\0';
            strncpy(player_names[1], game->state.players[1], MAX_PLAYER_NAME - 1);
            player_names[1][MAX_PLAYER_NAME - 1] = '\0';

            char game_id[MAX_GAME_ID_LEN];
            strncpy(game_id, game->state.game_id, MAX_GAME_ID_LEN - 1);
            game_id[MAX_GAME_ID_LEN - 1] = '\0';
            // ------ (notify non deve essere dentro mutex) ------
            
            pthread_mutex_unlock(&server_state.mutex);
            
            // Invia risposta
            response.status = STATUS_OK;
            response.error_code = ERR_NONE;
            protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
            
            // Notifica inizio partita a entrambi
            const char *player_names_ptrs[2] = {player_names[0], player_names[1]};
            notify_game_start(player_fds, player_names_ptrs);
        } else {
            LOG_ERROR("Errore aggiunta giocatore alla partita");
            pthread_mutex_unlock(&server_state.mutex);
            protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
            return;
        }
    } else {
        // Solo un giocatore ha richiesto: notifica l'avversario
        LOG_INFO("In attesa che l'avversario (FD=%d) accetti il rematch", opponent_fd);
        
        pthread_mutex_unlock(&server_state.mutex);
        
        // Invia risposta al richiedente
        response.status = STATUS_OK;
        response.error_code = ERR_NONE;
        protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);
        
        // Notifica l'avversario che è stato richiesto un rematch
        notify_rematch_request_t notify;
        notify.notify_type = NOTIFY_REMATCH_REQUEST;
        strncpy(notify.player, client->name, MAX_PLAYER_NAME - 1);
        notify.player[MAX_PLAYER_NAME - 1] = '\0';
        
        protocol_send(opponent_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
        LOG_DEBUG("REMATCH_REQUEST inviato a FD=%d da '%s'", opponent_fd, client->name);
    }
}

void handle_quit(int client_fd, uint32_t req_seq_id) {
    LOG_DEBUG("handle_quit chiamato per FD=%d, req_seq=%u", client_fd, req_seq_id);
    
    response_quit_t response;
    response.status = STATUS_OK;
    response.error_code = ERR_NONE;

    pthread_mutex_lock(&server_state.mutex);
    
    // Auto-cleanup se il client è in una partita finita per pareggio
    auto_cleanup_finished_draw_game(client_fd);
    
    cleanup_notify_data_t notify_data = cleanup_client_from_game_state(client_fd);
    pthread_mutex_unlock(&server_state.mutex);

    // Invia risposta al client che sta quittando
    protocol_send(client_fd, MSG_RESPONSE, &response, sizeof(response), req_seq_id);

    // Invia notifica all'eventuale client avversario
    send_notify_after_cleanup_client(notify_data);
}

// ============================================================================
// HELPER PER GLI HANDLER
// ============================================================================

void send_list_games_error(int client_fd, error_code_t error, uint32_t req_seq_id) {
    response_list_games_t err_response;
    err_response.status = STATUS_ERROR;
    err_response.error_code = error;
    err_response.game_count = 0;
    err_response.reserved = 0;
    protocol_send(client_fd, MSG_RESPONSE, &err_response, sizeof(err_response), req_seq_id);
}

int find_creator_for_join_cancellation(int joiner_fd) {
    for (int i = 0; i < server_state.max_games; i++) {
        game_session_t *game = &server_state.games[i];
        if (game->active && game->pending_join_fd == joiner_fd) {
            return game->player_fds[0];
        }
    }
    return -1;
}

void cleanup_pending_join(int client_fd) {
    for (int i = 0; i < server_state.max_games; i++) {
        game_session_t *game = &server_state.games[i];
        if (game->active && game->pending_join_fd == client_fd) {
            game->pending_join_fd = -1;
            game->pending_join_name[0] = '\0';
            break;
        }
    }
}

cleanup_notify_data_t cleanup_client_from_game_state(int client_fd) {
    cleanup_notify_data_t result = {CLEANUP_NOTIFY_NONE, -1, ""};
    
    int client_idx = find_client_by_fd(client_fd);
    if (client_idx == -1) {
        LOG_WARN("cleanup_client_from_game_state: Client FD=%d non trovato", client_fd);
        return result;
    }
    
    client_info_t *client = &server_state.clients[client_idx];
    strncpy(result.opponent_name, client->name, MAX_PLAYER_NAME - 1);
    result.opponent_name[MAX_PLAYER_NAME - 1] = '\0';
    
    // Caso 1: Client in attesa di risposta (joiner che cancella)
    if (client->status == CLIENT_REQUESTING_JOIN) {
        int creator_fd = find_creator_for_join_cancellation(client_fd);
        
        cleanup_pending_join(client_fd);
        client->status = CLIENT_REGISTERED;
        
        LOG_INFO("Client '%s' (FD=%d) rimosso da richiesta join pendente",
                 client->name, client_fd);
        
        if (creator_fd > 0) {
            result.type = CLEANUP_NOTIFY_JOIN_CANCELLED_BY_JOINER;
            result.target_fd = creator_fd;
        }
    }
    // Caso 2: Client in lobby (creatore che lascia)
    else if (client->status == CLIENT_IN_LOBBY) {
        game_session_t *game = &server_state.games[client->game_index];
        
        if (!game->active) {
            LOG_ERROR("Partita non attiva per client FD=%d", client_fd);
            client->status = CLIENT_REGISTERED;
            return result;
        }
        
        // Prepara notifica al joiner pendente
        if (game->pending_join_fd > 0) {
            result.type = CLEANUP_NOTIFY_JOIN_CANCELLED_BY_CREATOR;
            result.target_fd = game->pending_join_fd;
            
            // Reset stato joiner
            client_info_t *joiner = &server_state.clients[find_client_by_fd(game->pending_join_fd)];
            joiner->status = CLIENT_REGISTERED;
        }
        
        cleanup_game(game); // Mutex già acquisito
        LOG_INFO("Client '%s' (FD=%d) rimosso da lobby", client->name, client_fd);
    }
    // Caso 3: Client in partita attiva (IN_GAME)
    else if (client->status == CLIENT_IN_GAME) {
        game_session_t *game = &server_state.games[client->game_index];
        
        if (!game->active) {
            LOG_ERROR("Partita non attiva per client FD=%d", client_fd);
            client->status = CLIENT_REGISTERED;
            return result;
        }
        
        // Trova l'avversario prima del cleanup
        int opponent_idx = 1 - client->player_index;
        int opponent_fd = game->player_fds[opponent_idx];
        
        cleanup_game(game); // Mutex già acquisito
        
        LOG_INFO("Client '%s' (FD=%d) rimosso da partita attiva, opponent_fd=%d",
                    client->name, client_fd, opponent_fd);
        
        result.type = CLEANUP_NOTIFY_OPPONENT_LEFT;
        result.target_fd = opponent_fd;
    }

    return result;
}

void send_notify_after_cleanup_client(cleanup_notify_data_t notify_data) {
    // Invia notifica all'eventuale avversario
    if (notify_data.type == CLEANUP_NOTIFY_OPPONENT_LEFT) {
        notify_opponent_left_t notify;
        notify.notify_type = NOTIFY_OPPONENT_LEFT;
        protocol_send(notify_data.target_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
        LOG_INFO("OPPONENT_LEFT inviato a FD=%d (client disconnesso)", notify_data.target_fd);
    } 
    // Notifica (al creatore) cancellazione join da parte del joiner
    else if (notify_data.type == CLEANUP_NOTIFY_JOIN_CANCELLED_BY_JOINER) {
        notify_join_cancellation_t notify;
        notify.notify_type = NOTIFY_JOIN_CANCELLATION;
        notify.is_cancelled_by_joiner = 1;
        strncpy(notify.opponent, notify_data.opponent_name, MAX_PLAYER_NAME - 1);
        notify.opponent[MAX_PLAYER_NAME - 1] = '\0';
        protocol_send(notify_data.target_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
        LOG_INFO("JOIN_CANCELLATION (by joiner) inviato a FD=%d (client disconnesso)", notify_data.target_fd);
    }
    // Notifica (al joiner) cancellazione join da parte del creatore
    else if (notify_data.type == CLEANUP_NOTIFY_JOIN_CANCELLED_BY_CREATOR) {
        notify_join_cancellation_t notify;
        notify.notify_type = NOTIFY_JOIN_CANCELLATION;
        notify.is_cancelled_by_joiner = 0;
        strncpy(notify.opponent, notify_data.opponent_name, MAX_PLAYER_NAME - 1);
        notify.opponent[MAX_PLAYER_NAME - 1] = '\0';
        protocol_send(notify_data.target_fd, MSG_NOTIFY, &notify, sizeof(notify), 0);
        LOG_INFO("JOIN_CANCELLATION (by creator) inviato a FD=%d (client disconnesso)", notify_data.target_fd);
    }
}

// ============================================================================
// FUNZIONI DI NOTIFICA
// ============================================================================

void broadcast_to_registered_clients(uint8_t msg_type, const void *payload, size_t payload_size) {
    // Copia snapshot dei client registrati (evita I/O sotto lock)
    pthread_mutex_lock(&server_state.mutex);
    
    int registered_fds[server_state.max_clients];
    int count = 0;
    
    for (int i = 0; i < server_state.num_clients; i++) {
        if (server_state.clients[i].status == CLIENT_REGISTERED) {
            registered_fds[count++] = server_state.clients[i].fd;
        }
    }
    
    pthread_mutex_unlock(&server_state.mutex);
    
    // Invia messaggi senza lock
    for (int i = 0; i < count; i++) {
        ssize_t sent = protocol_send(registered_fds[i], msg_type, payload, payload_size, 0);
        if (sent > 0) {
            LOG_DEBUG("Broadcast inviato a client FD=%d", registered_fds[i]);
        } else {
            LOG_WARN("Errore invio broadcast a FD=%d", registered_fds[i]);
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

void notify_game_start(int player_fds[2], const char *player_names[2]) {
    for (int i = 0; i < 2; i++) {
        notify_game_start_t notify;
        notify.notify_type = NOTIFY_GAME_START;
        notify.your_symbol = (i == 0) ? FIRST_PLAYER_SYMBOL : SECOND_PLAYER_SYMBOL;
        notify.first_player = FIRST_PLAYER_SYMBOL;
        
        int opponent_idx = 1 - i;
        strncpy(notify.opponent, player_names[opponent_idx], MAX_PLAYER_NAME - 1);
        notify.opponent[MAX_PLAYER_NAME - 1] = '\0';
        
        protocol_send(player_fds[i], MSG_NOTIFY, &notify, sizeof(notify), 0);
        LOG_INFO("Notifica GAME_START inviata a FD=%d: symbol='%c', opponent='%s'",
                    player_fds[i], notify.your_symbol, notify.opponent);
    }
}