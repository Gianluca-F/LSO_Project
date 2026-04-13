#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

// Stato globale del client
client_state_t client_state = {
    .socket_fd = -1,
    .username = {0},
    .state = CLIENT_DISCONNECTED,
    .current_game_id = {0},
    .my_symbol = '\0',
    .notification_thread = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .running = false,
    .seq_id = 0,
    .last_request_type = 0,
    .last_move_pos = 0
};

// ============================================================================
// FUNZIONI DI CONNESSIONE
// ============================================================================

int client_connect(const char *host, int port) {
    if (client_state.socket_fd >= 0) {
        LOG_WARN("Client già connesso");
        return -1;
    }
    
    // Risolvi hostname/IP usando getaddrinfo (supporta sia IP che nomi DNS)
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP
    
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    int ret = getaddrinfo(host, port_str, &hints, &result);
    if (ret != 0) {
        LOG_ERROR("Errore risoluzione hostname %s: %s", host, gai_strerror(ret));
        return -1;
    }
    
    // Prova a connettersi a uno degli indirizzi risolti
    int sock = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) {
            continue;
        }
        
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; // Successo!
        }
        
        close(sock);
        sock = -1;
    }
    
    freeaddrinfo(result);
    
    if (sock < 0) {
        LOG_ERROR("Impossibile connettersi a %s:%d", host, port);
        return -1;
    }
    
    LOG_INFO("Connesso al server %s:%d", host, port);
    
    // Aggiorna stato client
    pthread_mutex_lock(&client_state.mutex);
    client_state.socket_fd = sock;
    client_state.state = CLIENT_CONNECTED;
    client_state.running = true;
    pthread_mutex_unlock(&client_state.mutex);
    
    LOG_INFO("Connesso al server %s:%d (fd=%d)", host, port, sock);
    return 0;
}

void client_disconnect(void) {
    pthread_mutex_lock(&client_state.mutex);
    
    if (client_state.socket_fd < 0) {
        pthread_mutex_unlock(&client_state.mutex);
        return;
    }
    
    // Ferma il thread di notifiche se attivo
    if (client_state.running) {
        client_state.running = false;
        pthread_mutex_unlock(&client_state.mutex);
        
        // Aspetta che il thread termini
        if (client_state.notification_thread) {
            pthread_join(client_state.notification_thread, NULL);
        }
        
        pthread_mutex_lock(&client_state.mutex);
    }
    
    // Chiudi socket
    close(client_state.socket_fd);
    LOG_INFO("Disconnesso dal server (fd=%d)", client_state.socket_fd);
    
    // Reset stato
    client_state.socket_fd = -1;
    client_state.state = CLIENT_DISCONNECTED;
    client_state.username[0] = '\0';
    client_state.current_game_id[0] = '\0';
    client_state.my_symbol = '\0';
    client_state.notification_thread = 0;
    
    pthread_mutex_unlock(&client_state.mutex);
}

// ============================================================================
// FUNZIONI PER INVIARE RICHIESTE AL SERVER
// ============================================================================

int send_register_request(const char *username) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    payload_register_t payload;
    memset(&payload, 0, sizeof(payload));
    strncpy(payload.player_name, username, MAX_PLAYER_NAME - 1);
    
    pthread_mutex_lock(&client_state.mutex);
    // Salva l'username nello stato del client
    strncpy(client_state.username, username, MAX_PLAYER_NAME - 1);
    client_state.username[MAX_PLAYER_NAME - 1] = '\0';
    client_state.last_request_type = MSG_REGISTER;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_REGISTER, 
                           &payload, sizeof(payload), seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_REGISTER");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_REGISTER: username='%s' seq=%u", username, seq);
    return 0;
}

int send_create_game_request(void) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_CREATE_GAME;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_CREATE_GAME, 
                           NULL, 0, seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_CREATE_GAME");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_CREATE_GAME seq=%u", seq);
    return 0;
}

int send_list_games_request(void) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_LIST_GAMES;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_LIST_GAMES, 
                           NULL, 0, seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_LIST_GAMES");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_LIST_GAMES seq=%u", seq);
    return 0;
}

int send_join_game_request(const char *game_id) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    payload_join_game_t payload;
    memset(&payload, 0, sizeof(payload));
    strncpy(payload.game_id, game_id, MAX_GAME_ID_LEN - 1);
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_JOIN_GAME;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_JOIN_GAME, 
                           &payload, sizeof(payload), seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_JOIN_GAME");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_JOIN_GAME: game_id='%s' seq=%u", game_id, seq);
    return 0;
}

int send_accept_join_request(bool accept) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    payload_accept_join_t payload;
    payload.accept = accept ? 1 : 0;  
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_ACCEPT_JOIN;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_ACCEPT_JOIN, 
                           &payload, sizeof(payload), seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_ACCEPT_JOIN");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_ACCEPT_JOIN: accept=%d seq=%u", payload.accept, seq);
    return 0;
}

int send_make_move_request(int pos) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    payload_make_move_t payload;
    payload.pos = (uint8_t)pos;
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_MAKE_MOVE;
    client_state.last_move_pos = pos;  // Salva la posizione per aggiornarla dopo
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_MAKE_MOVE, 
                           &payload, sizeof(payload), seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_MAKE_MOVE");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_MAKE_MOVE: pos=%d seq=%u", pos, seq);
    return 0;
}

int send_message_request(char *message) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    payload_send_message_t payload;
    memset(&payload, 0, sizeof(payload));
    strncpy(payload.message, message, MAX_CHAT_MESSAGE_LEN - 1);
    payload.message[MAX_CHAT_MESSAGE_LEN - 1] = '\0'; // Assicurati che sia null-terminated

    size_t msg_len = strnlen(payload.message, MAX_CHAT_MESSAGE_LEN);
    if (msg_len == 0) {
        LOG_WARN("Messaggio vuoto, non inviato");
        return -1;
    }
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_SEND_MESSAGE;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_SEND_MESSAGE, 
                           &payload, sizeof(payload), seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_SEND_MESSAGE");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_SEND_MESSAGE: message='%s' seq=%u", payload.message, seq);
    return 0;
}

int send_get_chat_history_request(void) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }

    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_GET_CHAT_HISTORY;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);

    int ret = protocol_send(client_state.socket_fd, MSG_GET_CHAT_HISTORY,
                           NULL, 0, seq);

    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_GET_CHAT_HISTORY");
        return -1;
    }

    LOG_DEBUG("Inviato MSG_GET_CHAT_HISTORY seq=%u", seq);
    return 0;
}

int send_get_stats_request(void) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }

    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_GET_STATS;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);

    int ret = protocol_send(client_state.socket_fd, MSG_GET_STATS,
                           NULL, 0, seq);

    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_GET_STATS");
        return -1;
    }

    LOG_DEBUG("Inviato MSG_GET_STATS seq=%u", seq);
    return 0;
}

int send_leave_game_request(void) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_LEAVE_GAME;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_LEAVE_GAME, 
                           NULL, 0, seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_LEAVE_GAME");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_LEAVE_GAME seq=%u", seq);
    return 0;
}

int send_rematch_request(void) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_REMATCH;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_REMATCH, 
                           NULL, 0, seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_REMATCH");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_REMATCH seq=%u", seq);
    return 0;
}

int send_quit_request(void) {
    if (client_state.socket_fd < 0) {
        LOG_ERROR("Non connesso al server");
        return -1;
    }
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.last_request_type = MSG_QUIT;
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    int ret = protocol_send(client_state.socket_fd, MSG_QUIT, 
                           NULL, 0, seq);
    
    if (ret < 0) {
        LOG_ERROR("Errore invio MSG_QUIT");
        return -1;
    }
    
    LOG_DEBUG("Inviato MSG_QUIT seq=%u", seq);
    return 0;
}

// ============================================================================
// THREAD PER NOTIFICHE ASINCRONE
// ============================================================================

void *notification_thread_func(void *arg) {
    (void)arg;  // Unused
    
    LOG_INFO("Thread notifiche avviato");
    
    while (client_state.running) {
        protocol_header_t header;
        
        // Ricevi header
        ssize_t ret = protocol_recv_header(client_state.socket_fd, &header);
        if (ret < 0) {
            if (client_state.running) {
                LOG_ERROR("Errore ricezione header, chiudo connessione");
                pthread_mutex_lock(&client_state.mutex);
                client_state.running = false;
                pthread_mutex_unlock(&client_state.mutex);
            }
            break;
        }
        
        // Leggi payload se presente
        void *payload = NULL;
        if (header.length > 0) {
            payload = malloc(header.length);
            if (!payload) {
                LOG_ERROR("Errore allocazione memoria per payload");
                break;
            }
            
            ret = protocol_recv_payload(client_state.socket_fd, payload, header.length);
            if (ret < 0) {
                LOG_ERROR("Errore ricezione payload");
                free(payload);
                break;
            }
        }
        
        // Gestisci il messaggio in base al tipo
        if (header.msg_type == MSG_RESPONSE) {
            // Risposta sincrona a una richiesta
            // Tutte le risposte hanno status ed error_code come primi due byte
            uint8_t status = ((uint8_t *)payload)[0];
            uint8_t error_code = ((uint8_t *)payload)[1];
            
            if (status == STATUS_OK) {
                LOG_DEBUG("Ricevuta risposta OK (seq=%u)", header.seq_id);
                
                // Gestisci in base al tipo di richiesta inviata
                pthread_mutex_lock(&client_state.mutex);
                uint8_t last_req = client_state.last_request_type;
                pthread_mutex_unlock(&client_state.mutex);
                
                switch (last_req) {
                    case MSG_REGISTER:
                        handle_response_register(payload);
                        break;
                    case MSG_CREATE_GAME:
                        handle_response_create_game(payload);
                        break;
                    case MSG_LIST_GAMES:
                        handle_response_list_games(payload);
                        break;
                    case MSG_JOIN_GAME:
                        handle_response_join_game(payload);
                        break;
                    case MSG_ACCEPT_JOIN:
                        handle_response_accept_join(payload);
                        break;
                    case MSG_MAKE_MOVE:
                        handle_response_make_move(payload);
                        break;
                    case MSG_SEND_MESSAGE:
                        handle_response_send_message(payload);
                        break;
                    case MSG_GET_CHAT_HISTORY:
                        handle_response_chat_history(payload);
                        break;
                    case MSG_GET_STATS:
                        handle_response_get_stats(payload);
                        break;
                    case MSG_LEAVE_GAME:
                        handle_response_leave_game(payload);
                        break;
                    case MSG_REMATCH:
                        handle_response_rematch(payload);
                        break;
                    case MSG_QUIT:
                        handle_response_quit(payload);
                        break;
                    default:
                        printf("\n✅ Operazione completata.");
                        fflush(stdout);
                        break;
                }
            } else {
                LOG_WARN("Ricevuta risposta ERROR: %d (seq=%u)", error_code, header.seq_id);
                handle_response_error(error_code);
            }
        }
        else if (header.msg_type == MSG_NOTIFY) {
            // Notifica asincrona
            if (!payload) {
                LOG_WARN("Notifica senza payload");
                continue;
            }
            
            uint8_t *notify_type = (uint8_t *)payload;
            
            switch (*notify_type) {
                case NOTIFY_GAME_CREATED:
                    handle_game_created_notification((notify_game_created_t *)payload);
                    break;
                case NOTIFY_JOIN_REQUEST:
                    handle_join_request_notification((notify_join_request_t *)payload);
                    break;
                case NOTIFY_JOIN_CANCELLATION:
                    handle_join_cancellation_notification((notify_join_cancellation_t *)payload);
                    break;
                case NOTIFY_JOIN_RESPONSE:
                    handle_join_response_notification((notify_join_response_t *)payload);
                    break;
                case NOTIFY_GAME_START:
                    handle_game_start_notification((notify_game_start_t *)payload);
                    break;
                case NOTIFY_MOVE_MADE:
                    handle_move_made_notification((notify_move_made_t *)payload);
                    break;
                case NOTIFY_MESSAGE_SENT:
                    handle_message_sent_notification((notify_message_sent_t *)payload);
                    break;
                case NOTIFY_GAME_END:
                    handle_game_over_notification((notify_game_end_t *)payload);
                    break;
                case NOTIFY_OPPONENT_LEFT:
                    handle_opponent_left_notification((notify_opponent_left_t *)payload);
                    break;
                case NOTIFY_REMATCH_REQUEST:
                    handle_rematch_request_notification((notify_rematch_request_t *)payload);
                    break;
                case NOTIFY_NO_REMATCH:
                    handle_no_rematch_notification((notify_no_rematch_t *)payload);
                    break;
                default:
                    LOG_WARN("Tipo di notifica sconosciuto: %d", *notify_type);
                    break;
            }
        }
        else {
            LOG_WARN("Tipo di messaggio sconosciuto: %d", header.msg_type);
        }

        pthread_mutex_lock(&client_state.mutex);
        bool running = client_state.running;
        pthread_mutex_unlock(&client_state.mutex);
        if (running) {
            printf("\n\n> ");
            fflush(stdout);
        }
        
        if (payload) {
            free(payload);
        }
    }
    
    LOG_INFO("Thread notifiche terminato");
    
    return NULL;
}

// ============================================================================
// GESTORI DELLE RISPOSTE
// ============================================================================

void handle_response_register(const void *payload) {
    (void)payload;  // Non utilizzato per questa risposta
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.state = CLIENT_REGISTERED;
    pthread_mutex_unlock(&client_state.mutex);
    
    printf("\n✅ Registrazione completata con successo!"
           "\n   Ora puoi creare una partita con 'create' o vedere le partite con 'list'.");
    fflush(stdout);
}

void handle_response_create_game(const void *payload) {
    const response_create_game_t *resp = (const response_create_game_t *)payload;
    
    pthread_mutex_lock(&client_state.mutex);
    strncpy(client_state.current_game_id, resp->game_id, MAX_GAME_ID_LEN - 1);
    client_state.current_game_id[MAX_GAME_ID_LEN - 1] = '\0';
    client_state.state = CLIENT_IN_LOBBY;
    client_state.my_symbol = FIRST_PLAYER_SYMBOL;
    pthread_mutex_unlock(&client_state.mutex);
    
    printf("\n✅ Partita creata con successo!");
    fflush(stdout);
}

void handle_response_list_games(const void *payload) {
    const response_list_games_t *resp = (const response_list_games_t *)payload;
    
    if (resp->game_count == 0) {
        printf("\n📋 Nessuna partita disponibile al momento..."
               "\n   Puoi crearne una con 'create'.");
    } else {
        printf("\n📋 Partite disponibili: %d\n", resp->game_count);
        printf("──────────────────────────────────────────────────────────────\n");
        
        // Lista partite (dopo i primi 4 byte ci sono i game_info_t)
        const game_info_t *games = (const game_info_t *)((const uint8_t *)payload + 4);
        for (int i = 0; i < resp->game_count; i++) {
            printf("  [%d] ID: %s | Creatore: %s\t| Giocatori: %d/2\n",
                   i + 1, games[i].game_id, games[i].creator, 
                   games[i].players_count);
        }
        printf("──────────────────────────────────────────────────────────────\n");
        printf("Usa 'join <game_id>' per unirti a una partita.");
    }
    fflush(stdout);
}

void handle_response_join_game(const void *payload) {
    const response_join_game_t *resp = (const response_join_game_t *)payload;
    
    pthread_mutex_lock(&client_state.mutex);
    strncpy(client_state.current_game_id, resp->game_id, MAX_GAME_ID_LEN - 1);
    client_state.current_game_id[MAX_GAME_ID_LEN - 1] = '\0';
    client_state.state = CLIENT_REQUESTING_JOIN;
    client_state.my_symbol = SECOND_PLAYER_SYMBOL;
    pthread_mutex_unlock(&client_state.mutex);
    
    printf("\n✅ Richiesta di join avvenuta! (Scrivere \"leave\" per abbandonare)"
           "\n   In attesa che il creatore accetti la tua richiesta...");
    fflush(stdout);
    LOG_INFO("In attesa di accettazione per partita '%s'", resp->game_id);
}

void handle_response_accept_join(const void *payload) {
    (void)payload;  // Non utilizzato per questa risposta
    
    printf("\n✅ Risposta inviata al giocatore.");
    fflush(stdout);
}

void handle_response_make_move(const void *payload) {
    (void)payload;  // Non utilizzato per questa risposta
    
    pthread_mutex_lock(&client_state.mutex);
    // Aggiorna la board locale SOLO se la mossa è stata accettata
    int board_idx = client_state.last_move_pos - 1;
    client_state.local_game_state.board[board_idx] = client_state.my_symbol;
    client_state.local_game_state.move_count++;
    client_state.local_game_state.current_player = 
        (client_state.local_game_state.current_player + 1) % 2;
    pthread_mutex_unlock(&client_state.mutex);

    printf("\n✅ Mossa accettata.\n\n");
    
    // Stampa la board aggiornata dopo la tua mossa
    pthread_mutex_lock(&client_state.mutex);
    game_print_board(&client_state.local_game_state);
    pthread_mutex_unlock(&client_state.mutex);
    
    printf("\nIn attesa della mossa dell'avversario..."
           "\nSe vuoi mandare un messaggio, utilizza send <msg>.");
    fflush(stdout);
}

void handle_response_send_message(const void *payload) {
    (void)payload;  // Non utilizzato per questa risposta
    
    printf("\n✅ Messaggio inviato con successo.");
    fflush(stdout);
}

void handle_response_chat_history(const void *payload) {
    const response_chat_history_t *resp = (const response_chat_history_t *)payload;
    const chat_message_entry_t *messages = (const chat_message_entry_t *)
        ((const uint8_t *)payload + sizeof(response_chat_history_t));

    printf("\n----------------------------------------\n");
    printf("Chat partita\n");
    printf("----------------------------------------\n");

    if (resp->message_count == 0) {
        printf("(Nessun messaggio ancora)\n");
    } else {
        for (uint8_t i = 0; i < resp->message_count; i++) {
            printf("%s: %s\n", messages[i].player, messages[i].message);
        }
    }

    printf("----------------------------------------\n");
    fflush(stdout);
}

void handle_response_get_stats(const void *payload) {
    const response_get_stats_t *resp = (const response_get_stats_t *)payload;
    uint32_t total_games = resp->wins + resp->losses + resp->draws;
    
    printf("\n----------------------------------------\n");
    printf("Statistiche di gioco\n");
    printf("----------------------------------------\n");
    printf("Partite giocate: %d\n", total_games);
    printf("Partite vinte: %d (%.2f%%)\n", resp->wins, total_games > 0 ? (float)resp->wins / total_games * 100 : 0);
    printf("Partite perse: %d (%.2f%%)\n", resp->losses, total_games > 0 ? (float)resp->losses / total_games * 100 : 0);
    printf("Partite pareggiate: %d (%.2f%%)\n", resp->draws, total_games > 0 ? (float)resp->draws / total_games * 100 : 0);
    printf("----------------------------------------\n");
    fflush(stdout);
}

void handle_response_leave_game(const void *payload) {
    (void)payload;  // Non utilizzato per questa risposta
    
    pthread_mutex_lock(&client_state.mutex);
    client_state.state = CLIENT_REGISTERED;
    client_state.current_game_id[0] = '\0';
    pthread_mutex_unlock(&client_state.mutex);
    
    printf("\n✅ Hai abbandonato la partita."
           "\n   Sei tornato al menu principale.");
    fflush(stdout);
}

void handle_response_rematch(const void *payload) {
    (void)payload;  // Non utilizzato per questa risposta
    printf("\n✅ Richiesta di rematch inviata." 
           "\n   In attesa della risposta dell'avversario...");
    fflush(stdout);
}

void handle_response_quit(const void *payload) {
    (void)payload;  // Non utilizzato per questa risposta
    
    // Ferma il thread di notifiche
    pthread_mutex_lock(&client_state.mutex);
    client_state.running = false;
    pthread_mutex_unlock(&client_state.mutex);
    
    printf("\n✅ Disconnessione confermata. Arrivederci!\n");
    fflush(stdout);
}

void handle_response_error(uint8_t error_code) {
    printf("\n❌ Errore: ");
    
    switch (error_code) {
        case ERR_GAME_NOT_FOUND:
            printf("partita non trovata.");
            break;
        case ERR_GAME_FULL:
            printf("partita piena.");
            break;
        case ERR_REQUEST_PENDING:
            printf("richiesta di join già in sospeso.");
            break;
        case ERR_NO_PENDING_JOIN:
            printf("nessuna richiesta di join in sospeso.");
            break;
        case ERR_PENDING_JOIN_EXISTS:
            printf("la partita ha già una richiesta di join in sospeso.");
            break;
        case ERR_NOT_IN_LOBBY:
            printf("non sei in una lobby.");
            break;
        case ERR_ALREADY_IN_GAME:
            printf("sei già in una partita.");
            break;
        case ERR_NOT_IN_GAME:
            printf("non sei in una partita.");
            break;
        case ERR_NOT_YOUR_TURN:
            printf("non è il tuo turno.");
            break;
        case ERR_INVALID_MOVE:
            printf("mossa non valida. Assicurati che la mossa\n"
                   "   sia un numero intero valido tra 1 e 9.");
            break;
        case ERR_CELL_OCCUPIED:
            printf("cella già occupata.");
            break;
        case ERR_GAME_NOT_FINISHED:
            printf("la partita non è ancora finita. Non puoi richiedere un rematch.");
            break;
        case ERR_NOT_REGISTERED:
            printf("non sei registrato.");
            break;
        case ERR_ALREADY_REGISTERED:
            printf("sei già registrato.");
            break;
        case ERR_INVALID_NAME:
            printf("nome utente non valido. Usa solo lettere,\n"
                   "   numeri e underscore (max 32 caratteri).");
            break;
        case ERR_NAME_TAKEN:
            printf("nome utente già in uso.");
            break;
        case ERR_SERVER_FULL:
            // Cancella la riga corrente
            printf("\r\033[K");
            // Cancella la riga precedente (una riga sopra)
            printf("\033[A\033[2K");
            printf("❌ Errore: server pieno. Impossibile connettersi.\n");
            printf("   Premere un tasto per uscire...");
            fflush(stdout);
            pthread_mutex_lock(&client_state.mutex);
            client_state.running = false;  // Ferma il thread
            pthread_mutex_unlock(&client_state.mutex);
            return;  // Evita il fflush finale
        case ERR_INVALID_PAYLOAD:
            printf("payload non valido.");
            break;
        default:  // ERR_INTERNAL o sconosciuto
            printf("sconosciuto (%d).", error_code);
            break;
    }
    fflush(stdout);
}

// ============================================================================
// GESTORI DELLE NOTIFICHE
// ============================================================================

void handle_game_created_notification(const notify_game_created_t *notify) {
    printf("[NOTIFICA] Partita creata! ID: %s <", notify->game_id);
    printf("\n  In attesa di un avversario...");
    LOG_INFO("Partita creata: %s", notify->game_id);
}

void handle_join_request_notification(const notify_join_request_t *notify) {
    printf("[NOTIFICA] %s vuole unirsi alla tua partita! <", notify->opponent);
    printf("\n  Accetti? Invia il comando 'accept' per accettare o 'reject' per rifiutare.");
    fflush(stdout);
    LOG_INFO("Richiesta join da: %s", notify->opponent);
}

void handle_join_cancellation_notification(const notify_join_cancellation_t *notify) {
    pthread_mutex_lock(&client_state.mutex);

    if (notify->is_cancelled_by_joiner) {
        // Joiner ha annullato la richiesta, mando notifica al creatore
        printf("[NOTIFICA] %s ha annullato la richiesta di join. <", notify->opponent);
    } else {
        // Creatore ha annullato la richiesta, torno a REGISTERED
        client_state.state = CLIENT_REGISTERED;
        client_state.current_game_id[0] = '\0';
        printf("[NOTIFICA] Il creatore ha abbandonato la partita. <"
                "\n   Sei tornato al menu principale.");
    }
    pthread_mutex_unlock(&client_state.mutex);
    
    fflush(stdout);
    LOG_INFO("Join cancellato da: %s", notify->is_cancelled_by_joiner ? "joiner" : "creatore");
}

void handle_join_response_notification(const notify_join_response_t *notify) {
    pthread_mutex_lock(&client_state.mutex);
    
    if (notify->accepted) {
        // Se accettato, mantieni lo stato corrente
        // Arriverà subito NOTIFY_GAME_START che cambierà lo stato in CLIENT_IN_GAME
        pthread_mutex_unlock(&client_state.mutex);
        printf("[NOTIFICA] Il creatore ha accettato la tua richiesta! <");
        fflush(stdout);
        LOG_INFO("Join accettato dal creatore");
    } else {
        // Se rifiutato, torna a REGISTERED
        client_state.state = CLIENT_REGISTERED;
        client_state.current_game_id[0] = '\0';
        pthread_mutex_unlock(&client_state.mutex);
        printf("[NOTIFICA] La tua richiesta di join è stata rifiutata. :( <");
        fflush(stdout);
        LOG_INFO("Join rifiutato dal creatore");
    }
}

void handle_game_start_notification(const notify_game_start_t *notify) {
    pthread_mutex_lock(&client_state.mutex);
    
    client_state.state = CLIENT_IN_GAME;
    client_state.my_symbol = (char)notify->your_symbol;
    
    // Inizializza lo stato locale del gioco
    memset(&client_state.local_game_state, 0, sizeof(game_state_t));
    strcpy(client_state.local_game_state.game_id, client_state.current_game_id);
    
    // Imposta i giocatori
    if (client_state.my_symbol == FIRST_PLAYER_SYMBOL) {
        strcpy(client_state.local_game_state.players[0], client_state.username);
        strcpy(client_state.local_game_state.players[1], notify->opponent);
    } else {
        strcpy(client_state.local_game_state.players[0], notify->opponent);
        strcpy(client_state.local_game_state.players[1], client_state.username);
    }
    
    // Inizializza board vuota
    for (int i = 0; i < BOARD_SIZE; i++) {
        client_state.local_game_state.board[i] = '_';
    }
    
    client_state.local_game_state.current_player = 0; 
    client_state.local_game_state.status = GAME_IN_PROGRESS;
    client_state.local_game_state.winner = -1;
    
    pthread_mutex_unlock(&client_state.mutex);
    
    printf("\r \r");
    printf("========================================\n");
    printf("     LA PARTITA STA PER INIZIARE!\n");
    printf("========================================\n");
    printf("Tu sei: %c\n", notify->your_symbol);
    printf("Avversario: %s\n", notify->opponent);
    printf("Inizia: %c\n", notify->first_player);
    printf("========================================\n\n");
    
    game_print_board(&client_state.local_game_state);
    
    if (notify->first_player == client_state.my_symbol) {
        printf("\nÈ il tuo turno! Usa 'move <pos>' per giocare (1-9)."
               "\nSe vuoi mandare un messaggio, utilizza send <msg>.");
    } else {
        printf("\nIn attesa della mossa dell'avversario...");
    }
    
    LOG_INFO("Partita iniziata: tu=%c avversario=%s", 
             notify->your_symbol, notify->opponent);
}

void handle_move_made_notification(const notify_move_made_t *notify) {
    pthread_mutex_lock(&client_state.mutex);
    
    // Aggiorna board locale
    memcpy(client_state.local_game_state.board, notify->board, BOARD_SIZE);
    client_state.local_game_state.move_count++;
    
    // Cambia turno
    client_state.local_game_state.current_player = 
        (client_state.local_game_state.current_player + 1) % 2;
    
    pthread_mutex_unlock(&client_state.mutex);
    
    printf("\r \r");
    printf("\n[MOSSA] %s ha giocato in posizione < %d >\n", 
           notify->player, notify->pos);
    
    game_print_board(&client_state.local_game_state);
    
    // Questa notifica arriva SOLO quando l'avversario gioca
    // Quindi dopo la sua mossa è SEMPRE il tuo turno
    printf("\nÈ il tuo turno! Usa 'move <pos>' per giocare (1-9)."
           "\nSe vuoi mandare un messaggio, utilizza send <msg>.");
    
    LOG_DEBUG("Mossa ricevuta: pos=%d player=%s", notify->pos, notify->player);
}

void handle_message_sent_notification(const notify_message_sent_t *notify) {
    printf("[NOTIFICA] Nuovo messaggio da %s. <", notify->player);
    fflush(stdout);
    LOG_INFO("Messaggio ricevuto da %s.", notify->player);
}

void handle_game_over_notification(const notify_game_end_t *notify) {
    pthread_mutex_lock(&client_state.mutex);
    
    // Aggiorna board finale
    memcpy(client_state.local_game_state.board, notify->board, BOARD_SIZE);
    client_state.local_game_state.status = GAME_FINISHED;
    
    // Reset stato client
    client_state.state = CLIENT_REGISTERED;
    char old_game_id[MAX_GAME_ID_LEN];
    strcpy(old_game_id, client_state.current_game_id);
    client_state.current_game_id[0] = '\0';
    
    printf("\r \r");
    printf("========================================\n");
    printf("          PARTITA TERMINATA!\n");
    printf("========================================\n");
    
    game_print_board(&client_state.local_game_state);
    pthread_mutex_unlock(&client_state.mutex);
    
    switch (notify->result) {
        case RESULT_WIN:
            printf("\n🎉 HAI VINTO! Complimenti! 🎉"
                   "\n Puoi creare tu una partita o vedere quelle in attesa di giocatori!");
            break;
        case RESULT_LOSE:
            printf("\n😢 Hai perso. Ritenta!"
                   "\n Crea una partita o guarda quelle disponibili per una rivincita!");
            break;
        case RESULT_DRAW:
            printf("\n🤝 PAREGGIO! Partita equilibrata."
                   "\n Desideri una rivincita? Scrivi 'rematch'!");
            break;
        default:
            printf("\nPartita terminata.");
            break;
    }
    
    printf("\n\n========================================\n");

    LOG_INFO("Partita %s terminata: result=%d", old_game_id, notify->result);
}

void handle_opponent_left_notification(const notify_opponent_left_t *notify) {
    LOG_INFO("L'avversario ha abbandonato la partita");
    printf("\r \r");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  ⚠️  L'AVVERSARIO HA ABBANDONATO LA PARTITA!  ⚠️   ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n\t🎉 HAI VINTO! Complimenti! 🎉\n\n");
    pthread_mutex_lock(&client_state.mutex);
    uint8_t type = notify->notify_type; // Per togliere warning unused
    (void)type;
    client_state.state = CLIENT_REGISTERED;
    client_state.current_game_id[0] = '\0';
    pthread_mutex_unlock(&client_state.mutex);
    printf("\nSei tornato al menu principale.\n");
}

void handle_rematch_request_notification(const notify_rematch_request_t *notify) {
    LOG_INFO("L'avversario ha richiesto un rematch");
    printf("[NOTIFICA] %s vuole continuare a sfidarti! <\n", notify->player);
    fflush(stdout);
}

void handle_no_rematch_notification(const notify_no_rematch_t *notify) {
    LOG_INFO("L'avversario ha rifiutato il rematch");
    uint8_t type = notify->notify_type; // Per togliere warning unused
    (void)type;
    printf("[NOTIFICA] L'avversario ha abbandonato la lobby. <");
    printf("\n   Sei tornato al menu principale.\n");
}

// ============================================================================
// MENU INTERATTIVO
// ============================================================================

void client_run(void) {
    char input[256];
    bool still_running = true;
    
    printf("\n========================================\n");
    printf("   CLIENT TRIS - Menu Principale\n");
    printf("========================================\n\n");
    
    printf("Comandi disponibili:\n");
    printf("  register <nome>       - Registra il tuo nome\n");
    printf("  create                - Crea una nuova partita\n");
    printf("  list                  - Mostra lista partite\n");
    printf("  join <game_id>        - Unisciti a una partita\n");
    printf("  accept                - Accetta richiesta di join\n");
    printf("  reject                - Rifiuta richiesta di join\n");
    printf("  move <pos>            - Fai una mossa (pos: 1-9)\n");
    printf("  chat                  - Apri la chat della partita\n");
    printf("  send <msg>            - Invia un messaggio in partita\n");
    printf("  stats                 - Mostra statistiche del giocatore\n");
    printf("  leave                 - Abbandona la partita corrente\n");
    printf("  quit                  - Esci dal client\n");
    printf("  help                  - Mostra questo menu\n");
    printf("\n========================================\n");
    printf("\n> ");
    
    while (still_running) {
        fflush(stdout);

        pthread_mutex_lock(&client_state.mutex);
        still_running = client_state.running;
        pthread_mutex_unlock(&client_state.mutex);
        
        if (!still_running) {
            printf("\r \r");
            break;
        }
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        // Rimuovi newline
        input[strcspn(input, "\n")] = 0;
        
        // Parse comando
        char cmd[32];
        char arg[224];
        int parsed = sscanf(input, "%31s %223[^\n]", cmd, arg);
        
        if (parsed < 1) {
            printf("\n> ");
            continue;
        }

        printf("\n");
        
        // === REGISTER ===
        if (strcmp(cmd, "register") == 0) {
            if (parsed < 2) {
                printf("Uso: register <nome>\n");
                printf("\n> ");
                continue;
            }
            
            if (client_state.state != CLIENT_CONNECTED) {
                printf("❌ Errore: sei già registrato.\n");
                printf("\n> ");
                continue;
            }

            if (!protocol_validate_name(arg)) {
                printf("❌ Errore: nome utente non valido. Usa solo lettere,\n"
                       "   numeri e underscore (max 32 caratteri).\n");
                printf("\n> ");
                continue;
            }
            
            if (send_register_request(arg) == 0) {
                printf("Richiesta di registrazione inviata...\n");
            } else {
                printf("Errore nell'invio della richiesta.\n");
            }
        }
        // === CREATE ===
        else if (strcmp(cmd, "create") == 0) {
            if (client_state.state != CLIENT_REGISTERED) {
                printf("❌ Errore: devi essere registrato e non in partita.\n"
                       "   Puoi creare solo una partita alla volta.\n");
                printf("\n> ");
                continue;
            }
            
            if (send_create_game_request() == 0) {
                printf("Richiesta di creazione partita inviata...\n");
            } else {
                printf("Errore nell'invio della richiesta.\n");
            }
        }
        // === LIST ===
        else if (strcmp(cmd, "list") == 0) {
            if (client_state.state != CLIENT_REGISTERED &&
                client_state.state != CLIENT_REQUESTING_JOIN) {
                printf("❌ Errore: per richiedere la lista, devi essere\n"
                       "   registrato e non già in lobby o in partita.\n");
                printf("\n> ");
                continue;
            }
            
            if (send_list_games_request() == 0) {
                printf("Richiesta lista partite inviata...\n");
            } else {
                printf("Errore nell'invio della richiesta.\n");
            }
        }
        // === JOIN ===
        else if (strcmp(cmd, "join") == 0) {
            if (parsed < 2) {
                printf("Uso: join <game_id>\n");
                printf("\n> ");
                continue;
            }
            
            if (client_state.state != CLIENT_REGISTERED) {
                printf("❌ Errore: per unirti a una partita, devi essere\n"
                       "   registrato e non già in lobby o in partita.\n");
                printf("\n> ");
                continue;
            }
            
            if (send_join_game_request(arg) == 0) {
                printf("Richiesta di join inviata...\n");
            } else {
                printf("Errore nell'invio della richiesta.\n");
            }
        }
        // === ACCEPT ===
        else if (strcmp(cmd, "accept") == 0) {
            if (client_state.state != CLIENT_IN_LOBBY) {
                printf("❌ Errore: devi essere in lobby per accettare\n"
                       "   una richiesta di join.\n");
                printf("\n> ");
                continue;
            }
            
            if (send_accept_join_request(true) == 0) {
                printf("Accettazione inviata...\n");
            } else {
                printf("Errore nell'invio della risposta.\n");
            }
        }
        // === REJECT ===
        else if (strcmp(cmd, "reject") == 0) {
            if (client_state.state != CLIENT_IN_LOBBY) {
                printf("❌ Errore: devi essere in lobby per rifiutare\n"
                       "   una richiesta di join.\n");
                printf("\n> ");
                continue;
            }
            
            if (send_accept_join_request(false) == 0) {
                printf("Rifiuto inviato...\n");
            } else {
                printf("Errore nell'invio della risposta.\n");
            }
        }
        // === MOVE ===
        else if (strcmp(cmd, "move") == 0) {
            if (parsed < 2) {
                printf("Uso: move <pos>  (pos: 1-9)\n");
                printf("\n> ");
                continue;
            }
            
            if (client_state.state != CLIENT_IN_GAME) {
                printf("❌ Errore: non sei in una partita.\n");
                printf("\n> ");
                continue;
            }
            
            int pos = atoi(arg);
            if (!protocol_validate_move(pos)) {
                printf("❌ Posizione non valida: usa numeri da 1 a 9.\n");
                printf("\n> ");
                continue;
            }
            
            if (send_make_move_request(pos) == 0) {
                printf("Mossa inviata...\n");
            } else {
                printf("Errore nell'invio della mossa.\n");
            }
        }
        // === SEND ===
        else if (strcmp(cmd, "send") == 0) {
            if (parsed < 2) {
                printf("Uso: send <messaggio>\n");
                printf("\n> ");
                continue;
            }
            
            if (client_state.state != CLIENT_IN_GAME) {
                printf("❌ Errore: puoi inviare messaggi solo durante una partita.\n");
                printf("\n> ");
                continue;
            }
            
            if (send_message_request(arg) == 0) {
                printf("Messaggio inviato...\n");
            } else {
                printf("Errore nell'invio del messaggio.\n");
            }
        }
        // === CHAT ===
        else if (strcmp(cmd, "chat") == 0) {
            if (client_state.state != CLIENT_IN_GAME) {
                printf("❌ Errore: puoi aprire la chat solo durante una partita.\n");
                printf("\n> ");
                continue;
            }

            if (send_get_chat_history_request() == 0) {
                printf("Richiesta cronologia chat inviata...\n");
            } else {
                printf("Errore nell'invio della richiesta cronologia chat.\n");
            }
        }
        // === STATS ===
        else if (strcmp(cmd, "stats") == 0) {
            if (client_state.state == CLIENT_CONNECTED) {
                printf("❌ Errore: devi essere registrato per vedere le statistiche.\n");
                printf("\n> ");
                continue;
            }

            if (send_get_stats_request() == 0) {
                printf("Richiesta statistiche inviata...\n");
            } else {
                printf("Errore nell'invio della richiesta statistiche.\n");
            }
        }
        // === LEAVE ===
        else if (strcmp(cmd, "leave") == 0) {
            if (client_state.state != CLIENT_IN_GAME  && 
                client_state.state != CLIENT_IN_LOBBY &&
                client_state.state != CLIENT_REQUESTING_JOIN) {
                printf("❌ Errore: non sei in una partita.\n");
                printf("\n> ");
                continue;
            }
            
            if (send_leave_game_request() == 0) {
                printf("Richiesta di abbandono inviata...\n");
            } else {
                printf("Errore nell'invio della richiesta.\n");
            }
        }
        // === REMATCH ===
        else if (strcmp(cmd, "rematch") == 0) {
            if (client_state.state != CLIENT_REGISTERED) {
                printf("❌ Errore: puoi richiedere un rematch solo dopo\n"
                       "   che una partita è terminata.\n");
                printf("\n> ");
                continue;
            }
            
            if (send_rematch_request() == 0) {
                printf("Richiesta di rematch inviata...\n");
            } else {
                printf("Errore nell'invio della richiesta.\n");
            }
        }
        // === QUIT ===
        else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            printf("Disconnessione...\n");

            if (send_quit_request() == 0) {
                // Aspetta che il thread di notifiche termini
                pthread_join(client_state.notification_thread, NULL);
            } else {
                printf("Errore nell'invio della richiesta di disconnessione.\n");
            }
        }
        // === HELP ===
        else if (strcmp(cmd, "help") == 0) {
            printf("\n========================================\n");
            printf("   CLIENT TRIS - Menu Principale\n");
            printf("========================================\n\n");
            
            printf("Comandi disponibili:\n");
            printf("  register <nome>       - Registra il tuo nome\n");
            printf("  create                - Crea una nuova partita\n");
            printf("  list                  - Mostra lista partite\n");
            printf("  join <game_id>        - Unisciti a una partita\n");
            printf("  accept                - Accetta richiesta di join\n");
            printf("  reject                - Rifiuta richiesta di join\n");
            printf("  move <pos>            - Fai una mossa (pos: 1-9)\n");
            printf("  chat                  - Apri la chat della partita\n");
            printf("  send <msg>            - Invia un messaggio in partita\n");
            printf("  leave                 - Abbandona la partita corrente\n");
            printf("  quit                  - Esci dal client\n");
            printf("  help                  - Mostra questo menu\n");
            printf("\n========================================\n");
            printf("\n> ");
        }
        // === UNKNOWN ===
        else {
            printf("Comando sconosciuto: %s.\n", cmd);
            printf("Usa 'help' per vedere i comandi disponibili.\n");
            printf("\n> ");
        }
    }
}
