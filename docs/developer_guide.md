# Guida Sviluppatore - Client-Server Tris

## 👨‍💻 Introduzione

Questa guida è rivolta a sviluppatori che vogliono comprendere, modificare o estendere il sistema client-server Tris.

---

## 🏗️ Setup Ambiente di Sviluppo

### Prerequisiti

- **Sistema Operativo**: Linux/Unix (testato su Ubuntu 22.04)
- **Compilatore**: GCC 9.0 o superiore
- **Build tool**: GNU Make
- **Debugger**: GDB (opzionale ma consigliato)
- **Memory checker**: Valgrind (opzionale)

### Editor Consigliati

- **VS Code** con estensioni C/C++
- **CLion** 
- **Vim/Neovim** con plugin per C

### Clonare e Compilare

```bash
# Clone del progetto
cd LSO_Project

# Compilazione completa
make clean && make

# Compilazione con debug symbols
make clean
cd server && make clean && make CFLAGS="-Wall -Wextra -g -pthread"
cd ../client && make clean && make CFLAGS="-Wall -Wextra -g -pthread"
```

### Setup Docker

#### Prerequisiti Docker

- **Docker** installato ([Guida installazione](https://docs.docker.com/get-docker/))
- **Docker Compose** installato (incluso in Docker Desktop)

Verifica l'installazione:
```bash
docker --version
docker compose version
```

#### Avvio Rapido con Docker

```bash
# 1. Build delle immagini
docker compose build

# 2. Avvia server in background
docker compose up -d server

# 3. Verifica che sia attivo
docker compose ps

# 4. Visualizza log del server (terminale 0)
docker compose logs -f server

# 5. Avvia client 1 (terminale 1)
docker compose run --rm client ./bin/client

# 6. Avvia client 2 (terminale 2)
docker compose run --rm client ./bin/client

# 7. Entra in un container in esecuzione 
docker exec -it tris-server /bin/bash

# 8. Gioca!

# 9. Ferma tutto quando hai finito
docker compose down
```

---

## 📁 Organizzazione del Codice

### Principi di Design

1. **Separazione delle responsabilità**
   - `shared/`: Codice riutilizzabile
   - `server/`: Logica server-side
   - `client/`: Logica client-side

2. **Modularità**
   - Ogni modulo ha un file `.h` (interfaccia) e `.c` (implementazione)
   - Dipendenze minime tra moduli

3. **Thread-safety**
   - Tutti gli accessi a stato condiviso sono protetti da mutex
   - Documentazione chiara su quali funzioni sono thread-safe

---

## 🔧 Moduli Principali

### 1. Protocol Layer (`shared/src/protocol.c`)

**Responsabilità**: Serializzazione e deserializzazione messaggi di rete.

#### Funzioni Chiave

```c
// Inizializza header del protocollo
void protocol_init_header(protocol_header_t *header, 
                         uint8_t msg_type,
                         uint16_t length, 
                         uint32_t seq_id);

// Invia messaggio completo (header + payload)
ssize_t protocol_send(int sockfd, uint8_t msg_type,
                     const void *payload, size_t payload_size,
                     uint32_t seq_id);

// Riceve header
ssize_t protocol_recv_header(int sockfd, protocol_header_t *header);

// Riceve payload
ssize_t protocol_recv_payload(int sockfd, void *buffer, size_t length);
```

#### Aggiungere un Nuovo Tipo di Messaggio

**Esempio**: Aggiungere un messaggio `MSG_CHAT` per chat in-game.

**1. Definisci il tipo in `protocol.h`:**
```c
// Nella sezione "Client → Server"
#define MSG_CHAT 10

// Payload
typedef struct __attribute__((packed)) {
    char message[256];
} payload_chat_t;
```

**2. Definisci la risposta:**
```c
// Nella sezione "Server → Client - Risposte"
typedef response_generic_t response_chat_t;

// Notifica per broadcast agli altri
typedef struct __attribute__((packed)) {
    uint8_t notify_type;  // NOTIFY_CHAT_MESSAGE
    char sender[MAX_PLAYER_NAME];
    char message[256];
} notify_chat_message_t;
```

**3. Implementa l'handler nel server (`server/src/server.c`):**
```c
void handle_chat(int client_fd, protocol_header_t *header, void *payload) {
    payload_chat_t *msg = (payload_chat_t*)payload;
    
    // Validazione
    if (!validate_chat_message(msg->message)) {
        send_error_response(client_fd, ERR_INVALID_PAYLOAD, header->seq_id);
        return;
    }
    
    // Ottieni info client
    client_info_t *client = find_client_by_fd(client_fd);
    
    // Broadcast a tutti in partita
    broadcast_chat_message(client->game_index, client->name, msg->message);
    
    // Risposta OK
    response_chat_t resp = { .status = STATUS_OK, .error_code = ERR_NONE };
    protocol_send(client_fd, MSG_RESPONSE, &resp, sizeof(resp), header->seq_id);
}
```

**4. Aggiungi al dispatcher:**
```c
// In handle_client() switch statement
case MSG_CHAT:
    handle_chat(client_fd, &header, payload_buffer);
    break;
```

**5. Implementa nel client (`client/src/client.c`):**
```c
int send_chat_message(const char *message) {
    if (!message || strlen(message) == 0) return -1;
    
    payload_chat_t payload;
    strncpy(payload.message, message, sizeof(payload.message) - 1);
    
    pthread_mutex_lock(&client_state.mutex);
    uint32_t seq = client_state.seq_id++;
    pthread_mutex_unlock(&client_state.mutex);
    
    return protocol_send(client_state.socket_fd, MSG_CHAT,
                        &payload, sizeof(payload), seq);
}
```

---

### 2. Game Logic (`shared/src/game_logic.c`)

**Responsabilità**: Logica del gioco Tris, indipendente dalla rete.

#### Struttura Dati

```c
typedef struct {
    char game_id[MAX_GAME_ID_LEN];
    char players[2][MAX_PLAYER_NAME];
    char board[9];           // Indici 0-8
    int current_player;      // 0 o 1
    int status;              // GAME_WAITING, IN_PROGRESS, FINISHED
    int move_count;
    int winner;              // -1, 0, 1, 2
} game_state_t;
```

#### Funzioni Critiche

```c
// Inizializza partita
void game_init(game_state_t *game, const char *game_id, const char *creator);

// Valida e esegue mossa
int game_make_move(game_state_t *game, int player_idx, int position);

// Controlla vincitore
int game_check_winner(const game_state_t *game);
```

#### Algoritmo di Controllo Vittoria

```c
int game_check_winner(const game_state_t *game) {
    // Combinazioni vincenti (righe, colonne, diagonali)
    const int WINNING_COMBINATIONS[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8},  // Righe
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8},  // Colonne
        {0, 4, 8}, {2, 4, 6}              // Diagonali
    };
    
    for (int i = 0; i < 8; i++) {
        char a = game->board[WINNING_COMBINATIONS[i][0]];
        char b = game->board[WINNING_COMBINATIONS[i][1]];
        char c = game->board[WINNING_COMBINATIONS[i][2]];
        
        if (a != EMPTY_CELL && a == b && b == c) {
            // Trova quale giocatore ha vinto
            return (a == FIRST_PLAYER_SYMBOL) ? 0 : 1;
        }
    }
    
    // Pareggio se tutte le celle sono piene
    if (game->move_count == 9) {
        return 2;  // Draw
    }
    
    return -1;  // Nessun vincitore ancora
}
```

#### Modificare le Regole del Gioco

**Esempio**: Cambiare dimensione griglia da 3x3 a 4x4.

**1. Modifica costanti in `constants.h`:**
```c
#define BOARD_SIZE 16  // Era 9 (3x3)
```

**2. Modifica `game_logic.c`:**
```c
// Aggiorna combinazioni vincenti per 4x4
const int WINNING_COMBINATIONS[][4] = {
    // Righe
    {0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14, 15},
    // Colonne
    {0, 4, 8, 12}, {1, 5, 9, 13}, {2, 6, 10, 14}, {3, 7, 11, 15},
    // Diagonali
    {0, 5, 10, 15}, {3, 6, 9, 12}
};

// Aggiorna controllo vittoria
for (int i = 0; i < num_combinations; i++) {
    char a = board[combo[i][0]];
    char b = board[combo[i][1]];
    char c = board[combo[i][2]];
    char d = board[combo[i][3]];
    if (a != EMPTY && a == b && b == c && c == d) {
        // Vittoria
    }
}
```

---

### 3. Server (`server/src/server.c`)

#### Architettura Multi-Thread

**Main Thread**:
```c
int main() {
    // 1. Carica configurazione
    load_server_config("config/server.conf");
    
    // 2. Inizializza stato server
    init_server_state();
    
    // 3. Crea socket e bind
    int server_fd = init_server(config.port);
    
    // 4. Loop accept
    start_server(server_fd);
    
    return 0;
}
```

**Thread per Client**:
```c
void *handle_client(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    // Aggiungi client allo stato globale (thread-safe)
    pthread_mutex_lock(&server_state.mutex);
    int idx = add_client(client_fd);
    pthread_mutex_unlock(&server_state.mutex);
    
    // Loop messaggi
    while (1) {
        protocol_header_t header;
        if (protocol_recv_header(client_fd, &header) <= 0) {
            break;  // Disconnesso
        }
        
        // Ricevi payload
        char buffer[MAX_MESSAGE_SIZE];
        if (header.length > 0) {
            protocol_recv_payload(client_fd, buffer, header.length);
        }
        
        // Dispatching
        switch (header.msg_type) {
            case MSG_REGISTER:
                handle_register(client_fd, &header, buffer);
                break;
            // ...
        }
    }
    
    // Cleanup
    cleanup_client(client_fd);
    pthread_exit(NULL);
}
```

#### Gestione Stato Condiviso

**Pattern Thread-Safe**:
```c
void some_operation_that_modifies_state() {
    pthread_mutex_lock(&server_state.mutex);
    
    // Operazioni critiche
    server_state.num_clients++;
    server_state.clients[idx].status = CLIENT_REGISTERED;
    
    pthread_mutex_unlock(&server_state.mutex);
}
```

**⚠️ ATTENZIONE**: Non fare operazioni bloccanti (I/O, sleep) dentro la sezione critica!

**❌ Sbagliato**:
```c
pthread_mutex_lock(&mutex);
protocol_send(fd, ...);  // Può bloccarsi!
pthread_mutex_unlock(&mutex);
```

**✅ Corretto**:
```c
pthread_mutex_lock(&mutex);
int fd_copy = client->fd;
pthread_mutex_unlock(&mutex);

protocol_send(fd_copy, ...);  // OK, fuori dalla sezione critica
```

#### Broadcasting

```c
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
```

---

### 4. Client (`client/src/client.c`)

#### Architettura Dual-Thread

**Main Thread**:
```c
int main() {
    // Carica la configurazione
    load_config("config/client.conf", &client_config);
    
    // Inizializza il sistema di logging
    init_client_logging();
    
    if (client_connect(client_config.server_ip, client_config.port) < 0) {
        fprintf(stderr, "Errore: impossibile connettersi al server\n");
        return -1;
    }
    
    // Avvia il thread per le notifiche
    client_state.running = true;
    if (pthread_create(&client_state.notification_thread, NULL, 
                      notification_thread_func, NULL) != 0) {
        fprintf(stderr, "Errore: impossibile avviare il thread di notifiche\n");
        client_disconnect();
        return -1;
    }
    
    // Avvia il menu interattivo (blocca fino a quit)
    client_run();
    
    // Cleanup
    client_disconnect();
    
    return 0;
}
```

**Menu interattivo** (interazione utente):
```c
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
    printf("  leave                 - Abbandona la partita corrente\n");
    printf("  quit                  - Esci dal client\n");
    printf("  help                  - Mostra questo menu\n");
    printf("\n========================================\n");
    printf("\n> ");
    
    while (still_running) {
        fflush(stdout);
        
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
        // ...
    }
}
```

**Notification Thread** (ricezione asincrona):
```c
void *client_notify_handler(void *arg) {
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
                    // ...
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
                // ...
            }
        }
    }
    
    return NULL;
}
```

---

## 🧪 Testing e Debugging

### Unit Testing

Attualmente non ci sono unit test. Ecco come aggiungerli:

**1. Installare framework (esempio: Check)**:
```bash
sudo apt-get install check
```

**2. Creare `tests/test_game_logic.c`**:
```c
#include <check.h>
#include "../shared/include/game_logic.h"

START_TEST(test_game_init) {
    game_state_t game;
    game_init(&game, "G123", "Alice");
    
    ck_assert_str_eq(game.game_id, "G123");
    ck_assert_str_eq(game.players[0], "Alice");
    ck_assert_int_eq(game.status, GAME_WAITING);
}
END_TEST

START_TEST(test_check_winner_row) {
    game_state_t game;
    game_init(&game, "G123", "Alice");
    game_add_player(&game, "Bob");
    
    // Simula X vince prima riga
    game.board[0] = 'X';
    game.board[1] = 'X';
    game.board[2] = 'X';
    game.move_count = 5;
    
    int winner = game_check_winner(&game);
    ck_assert_int_eq(winner, 0);  // Player 0 (X)
}
END_TEST

Suite *game_logic_suite(void) {
    Suite *s = suite_create("Game Logic");
    TCase *tc = tcase_create("Core");
    
    tcase_add_test(tc, test_game_init);
    tcase_add_test(tc, test_check_winner_row);
    
    suite_add_tcase(s, tc);
    return s;
}

int main() {
    Suite *s = game_logic_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
```

**3. Compilare e eseguire**:
```bash
gcc -o test_game_logic tests/test_game_logic.c shared/src/game_logic.c -lcheck -lm
./test_game_logic
```

---

### Debugging con GDB

**1. Compila con simboli di debug**:
```bash
make clean
make CFLAGS="-g -O0"
```

**2. Avvia con GDB**:
```bash
gdb ./server/bin/server
```

**3. Comandi utili**:
```gdb
(gdb) break server.c:handle_client     # Breakpoint
(gdb) run                              # Esegui
(gdb) backtrace                        # Stack trace
(gdb) print client_fd                  # Stampa variabile
(gdb) info threads                     # Mostra tutti i thread
(gdb) thread 2                         # Passa al thread 2
(gdb) continue                         # Continua esecuzione
```

**4. Debug multi-thread**:
```gdb
(gdb) set scheduler-locking on        # Esegui solo thread corrente
(gdb) thread apply all backtrace      # Stack trace di tutti i thread
```

---

### Memory Leak con Valgrind

```bash
# Compila con debug
make clean && make CFLAGS="-g -O0"

# Esegui con Valgrind
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./server/bin/server

# In un altro terminale, connetti client e fai operazioni
# Poi termina server con Ctrl+C

# Valgrind mostrerà:
# - Blocchi definitivamente persi (LEAK!)
# - Blocchi ancora raggiungibili (OK se puliti con cleanup)
# - Accessi a memoria non inizializzata
```

**Esempio output**:
```
==12345== LEAK SUMMARY:
==12345==    definitely lost: 256 bytes in 1 blocks
==12345==    indirectly lost: 0 bytes in 0 blocks
==12345==      possibly lost: 0 bytes in 0 blocks
==12345==    still reachable: 1,024 bytes in 2 blocks
==12345==         suppressed: 0 bytes in 0 blocks
```

---

### Logging Avanzato

**Aumentare verbosità**:

`server/config/server.conf`:
```ini
log_level=DEBUG
```

**Log condizionali**:
```c
#ifdef DEBUG_MODE
    LOG_DEBUG("Dettaglio: client_fd=%d, seq=%u", fd, seq);
#endif
```

**Compilare con debug mode**:
```bash
make CFLAGS="-DDEBUG_MODE -g"
```

---

## 🔐 Sicurezza e Best Practices

### Validazione Input

**Sempre validare**:
```c
int handle_register(int client_fd, protocol_header_t *header, void *payload) {
    payload_register_t *reg = (payload_register_t*)payload;
    
    // 1. Controllo lunghezza
    if (header->length != sizeof(payload_register_t)) {
        send_error(client_fd, ERR_INVALID_PAYLOAD, header->seq_id);
        return -1;
    }
    
    // 2. Validazione nome
    if (!protocol_validate_name(reg->player_name)) {
        send_error(client_fd, ERR_INVALID_NAME, header->seq_id);
        return -1;
    }
    
    // 3. Controllo duplicati
    if (find_client_by_name(reg->player_name) != NULL) {
        send_error(client_fd, ERR_NAME_TAKEN, header->seq_id);
        return -1;
    }
    
    // OK, procedi
}
```

---

### Buffer Overflow Protection

**❌ Pericoloso**:
```c
strcpy(dest, src);  // Nessun controllo lunghezza!
```

**✅ Sicuro**:
```c
strncpy(dest, src, sizeof(dest) - 1);
dest[sizeof(dest) - 1] = '\0';  // Garantisce null-termination
```

---

### Race Condition Prevention

**Problema**:
```c
// Thread 1
if (server_state.num_clients < MAX_CLIENTS) {
    // Thread 2 può eseguire qui!
    server_state.num_clients++;
}
```

**Soluzione**:
```c
pthread_mutex_lock(&server_state.mutex);
if (server_state.num_clients < MAX_CLIENTS) {
    server_state.num_clients++;  // Atomico
}
pthread_mutex_unlock(&server_state.mutex);
```

---

## 📦 Estensioni Proposte

### 1. Persistenza con Database

Salvare partite e statistiche giocatori.

**Schema SQLite**:
```sql
CREATE TABLE players (
    id INTEGER PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    wins INTEGER DEFAULT 0,
    losses INTEGER DEFAULT 0,
    draws INTEGER DEFAULT 0
);

CREATE TABLE games (
    id INTEGER PRIMARY KEY,
    game_id TEXT UNIQUE,
    player1_id INTEGER,
    player2_id INTEGER,
    winner_id INTEGER,
    final_board TEXT,
    created_at TIMESTAMP,
    FOREIGN KEY (player1_id) REFERENCES players(id),
    FOREIGN KEY (player2_id) REFERENCES players(id)
);
```

**Integrazione**:
```c
#include <sqlite3.h>

int save_game_result(const game_state_t *game) {
    sqlite3 *db;
    sqlite3_open("tris.db", &db);
    
    const char *sql = "INSERT INTO games (game_id, player1_id, player2_id, winner_id, final_board) "
                     "VALUES (?, ?, ?, ?, ?)";
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, game->game_id, -1, SQLITE_STATIC);
    // ... bind altri parametri
    
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return 0;
}
```

---

### 2. Autenticazione con Password

**Strutture**:
```c
typedef struct {
    char username[MAX_PLAYER_NAME];
    char password_hash[64];  // SHA-256
    char salt[32];
} user_credentials_t;
```

**Implementazione**:
```c
#include <openssl/sha.h>

void hash_password(const char *password, const char *salt, char *output) {
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", password, salt);
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)combined, strlen(combined), hash);
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
}

int verify_password(const char *username, const char *password) {
    // Carica credentials da DB
    user_credentials_t creds = load_credentials(username);
    
    char hash[65];
    hash_password(password, creds.salt, hash);
    
    return (strcmp(hash, creds.password_hash) == 0);
}
```

---

### 3. WebSocket per Client Web

Permettere connessione da browser.

**Libreria**: libwebsockets

**Server**:
```c
#include <libwebsockets.h>

static int callback_tris(struct lws *wsi,
                        enum lws_callback_reasons reason,
                        void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_RECEIVE:
            // Ricevi messaggio WebSocket
            handle_websocket_message(wsi, in, len);
            break;
        // ...
    }
    return 0;
}
```

**Client HTML/JS**:
```javascript
const ws = new WebSocket('ws://localhost:8080');

ws.onopen = () => {
    // Invia MSG_REGISTER
    const msg = {
        type: 'register',
        username: 'Alice'
    };
    ws.send(JSON.stringify(msg));
};

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    // Gestisci risposte e notifiche
};
```

---

## 📊 Performance Optimization

### Profiling con gprof

```bash
# Compila con profiling
make CFLAGS="-pg -O2"

# Esegui normalmente
./server/bin/server

# Genera report (dopo terminazione)
gprof ./server/bin/server gmon.out > analysis.txt

# Analizza funzioni più costose
less analysis.txt
```

---

### Connection Pooling

Riutilizzare connessioni invece di crearne di nuove.

```c
typedef struct {
    int fd;
    bool in_use;
    time_t last_used;
} connection_t;

connection_t pool[MAX_CONNECTIONS];

int get_connection() {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!pool[i].in_use) {
            pool[i].in_use = true;
            pool[i].last_used = time(NULL);
            return pool[i].fd;
        }
    }
    return -1;  // Pool esaurito
}

void release_connection(int fd) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (pool[i].fd == fd) {
            pool[i].in_use = false;
            break;
        }
    }
}
```

---

## 🔧 Troubleshooting Comune

### "Address already in use"

**Causa**: Porta ancora in uso dopo crash server.

**Soluzione**:
```bash
# Trova processo che usa la porta
sudo lsof -i :90

# Termina processo
kill -9 <PID>

# Oppure abilita SO_REUSEADDR (già fatto nel codice)
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

---

### Deadlock

**Sintomo**: Server/client si blocca senza output.

**Debug**:
```bash
# Attacca GDB al processo in esecuzione
gdb -p <PID>

# Mostra tutti i thread
(gdb) info threads

# Controlla cosa stanno aspettando
(gdb) thread apply all backtrace
```

**Causa comune**: Lock annidati acquisiti in ordine diverso.

**Prevenzione**: Sempre acquisire lock nello stesso ordine.

---

## 📚 Risorse Utili

### Documentazione

- **POSIX Threads**: https://man7.org/linux/man-pages/man7/pthreads.7.html
- **Berkeley Sockets**: https://man7.org/linux/man-pages/man7/socket.7.html
- **GCC Manual**: https://gcc.gnu.org/onlinedocs/

### Libri Consigliati

- *Unix Network Programming* - W. Richard Stevens
- *The Linux Programming Interface* - Michael Kerrisk
- *Programming with POSIX Threads* - David Butenhof

---

## 🤝 Contribuire

### Code Style

- **Indentazione**: 4 spazi
- **Naming**:
  - Variabili: `snake_case`
  - Funzioni: `snake_case`
  - Costanti: `UPPER_CASE`
  - Struct: `snake_case_t`
- **Commenti**: Doxygen-style per funzioni pubbliche

### Pull Request Guidelines

1. Crea branch feature: `git checkout -b feature/chat-support`
2. Scrivi test per nuovo codice
3. Verifica memory leak con Valgrind
4. Aggiorna documentazione
5. Crea PR con descrizione dettagliata

---

**Buon sviluppo!** 💻
