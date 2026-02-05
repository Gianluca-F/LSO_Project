# Architettura del Sistema Client-Server Tris

## 📋 Panoramica

Sistema client-server in C per giocare al **Tris (Tic-Tac-Toe)** online, sviluppato per il corso di **Laboratorio di Sistemi Operativi** presso l'Università di Napoli Federico II.

Il progetto implementa un'architettura multi-threaded che permette a più giocatori di connettersi simultaneamente al server e giocare partite di Tris in modalità 1v1.

### Caratteristiche Principali

- **Server multi-thread** con gestione concorrente di client e partite
- **Protocollo binario custom** per comunicazione efficiente
- **Sistema di notifiche asincrone** per eventi in tempo reale
- **Game logic separata** per validazione mosse e gestione stato
- **Logging completo** per debugging e monitoring
- **Configurazione flessibile** tramite file `.conf`
- **Thread-safety** garantita tramite mutex

---

## 🏗️ Architettura del Sistema

### Diagramma ad Alto Livello

```
┌─────────────────────────────────────────────────────────────┐
│                      LAYER APPLICATIVO                      │
├──────────────────────┬─────────────────┬────────────────────┤
│   Client (main.c)    │                 │  Server (main.c)   │
│   Menu Interattivo   │                 │  Accept Loop       │
└──────────┬───────────┘                 └────────┬───────────┘
           │                                      │
┌──────────▼────────────┐              ┌──────────▼───────────┐
│   CLIENT LOGIC        │              │   SERVER LOGIC       │
│   (client.c/h)        │              │   (server.c/h)       │
│                       │              │                      │
│ • Gestione stato      │              │ • Gestione client    │
│ • Thread notifiche    │              │ • Thread per client  │
│ • Invio richieste     │              │ • Gestione partite   │
│ • Gestione risposte   │              │ • Broadcasting       │
└──────────┬────────────┘              └──────────┬───────────┘
           │                                      │
           └───────────────────┬──────────────────┘
                               │
                ┌──────────────▼───────────────┐
                │      LAYER PROTOCOLLO        │
                │      (protocol.c/h)          │
                │                              │
                │ • Serializzazione messaggi   │
                │ • Invio/ricezione affidabile │
                │ • Network byte order         │
                └──────────────┬───────────────┘
                               │
                ┌──────────────▼───────────────┐
                │      GAME LOGIC              │
                │      (game_logic.c/h)        │
                │                              │
                │ • Validazione mosse          │
                │ • Controllo vittoria         │
                │ • Gestione turni             │
                └──────────────┬───────────────┘
                               │
                ┌──────────────▼───────────────┐
                │      UTILITIES               │
                │                              │
                │ • Logging (logging.c/h)      │
                │ • Costanti (constants.h)     │
                └──────────────────────────────┘
```

---

## 📂 Struttura del Progetto

```
LSO_Project/
│
├── client/                      # Applicazione client
│   ├── bin/                     # Eseguibile compilato
│   │   └── client
│   ├── config/                  # File di configurazione
│   │   ├── client.conf          # Configurazione attiva
│   │   └── client.conf.example  # Template configurazione
│   ├── include/                 # Header file client
│   │   ├── client.h             # API e strutture client
│   │   └── utils.h              # Utility client
│   ├── logs/                    # File di log
│   ├── obj/                     # File oggetto compilati
│   ├── src/                     # Codice sorgente
│   │   ├── client.c             # Logica client
│   │   ├── main.c               # Entry point e menu
│   │   └── utils.c              # Funzioni di utilità
│   └── Makefile                 # Build script
│
├── server/                      # Applicazione server
│   ├── bin/                     # Eseguibile compilato
│   │   └── server
│   ├── config/                  # File di configurazione
│   │   ├── server.conf          # Configurazione attiva
│   │   └── server.conf.example  # Template configurazione
│   ├── include/                 # Header file server
│   │   ├── server.h             # API e strutture server
│   │   └── utils.h              # Utility server
│   ├── logs/                    # File di log
│   ├── obj/                     # File oggetto compilati
│   ├── src/                     # Codice sorgente
│   │   ├── server.c             # Logica server
│   │   ├── main.c               # Entry point
│   │   └── utils.c              # Funzioni di utilità
│   └── Makefile                 # Build script
│
├── shared/                      # Codice condiviso
│   ├── include/                 # Header condivisi
│   │   ├── constants.h          # Costanti del gioco
│   │   ├── game_logic.h         # API game logic
│   │   ├── logging.h            # Sistema di logging
│   │   └── protocol.h           # Definizione protocollo
│   └── src/                     # Implementazioni condivise
│       ├── game_logic.c         # Logica del Tris
│       ├── logging.c            # Implementazione logging
│       └── protocol.c           # Implementazione protocollo
│
├── docs/                        # Documentazione
│   ├── architecture.md          # Architettura sistema
│   └── protocol.md              # Specifica protocollo
│
├── Makefile                     # Build script principale
└── README.md                    # Introduzione progetto
```

---

## 🔧 Componenti Principali

### 1. Server (`server/`)

#### Responsabilità
- Accettare connessioni TCP dai client
- Gestire autenticazione/registrazione giocatori
- Creare e gestire sessioni di gioco
- Validare mosse e aggiornare stato partite
- Inviare notifiche asincrone agli utenti
- Logging di tutte le operazioni

#### Architettura Multi-Thread
```
Main Thread                Client Thread 1              Client Thread 2
     │                            │                            │
     │ accept()                   │                            │
     ├───────────────────────────►│                            │
     │                            │ handle_client()            │
     │                            │ • MSG_REGISTER             │
     │ accept()                   │ • MSG_CREATE_GAME          │
     ├────────────────────────────┼───────────────────────────►│
     │                            │                            │ handle_client()
     │                            │◄────MSG_JOIN_GAME──────────┤
     │                            │                            │
     │                      [Game Session]                     │
     │                            │                            │
     │                            │◄────NOTIFY_MOVE_MADE──────►│
```

#### Strutture Dati Principali

**`client_info_t`** - Informazioni per ogni client connesso
```c
typedef struct {
    int fd;                          // Socket descriptor
    char name[MAX_PLAYER_NAME];      // Nome giocatore
    client_status_t status;          // CONNECTED, REGISTERED, IN_GAME, etc.
    int game_index;                  // Indice partita (-1 se non in gioco)
    int player_index;                // 0 o 1 (primo o secondo giocatore)
    uint32_t seq_id;                 // ID sequenza messaggi
    pthread_t thread_id;             // Thread che gestisce il client
} client_info_t;
```

**`game_session_t`** - Stato di una partita
```c
typedef struct {
    game_state_t state;              // Stato del gioco (da game_logic.h)
    int player_fds[2];               // Socket dei due giocatori
    int active;                      // 1 se partita attiva
    int pending_join_fd;             // FD di chi vuole joinare (-1 se nessuno)
    char pending_join_name[MAX_PLAYER_NAME];  // Nome del joiner
} game_session_t;
```

**`server_state_t`** - Stato globale del server
```c
typedef struct {
    client_info_t *clients;          // Array dinamico di client
    game_session_t *games;           // Array dinamico di partite
    int max_clients;                 // Limite massimo client
    int max_games;                   // Limite massimo partite
    int num_clients;                 // Numero client attuali
    int num_games;                   // Numero partite attive
    pthread_mutex_t mutex;           // Mutex per accesso concorrente
} server_state_t;
```

#### Funzioni Chiave
- `init_server()` - Inizializza socket e bind
- `start_server()` - Loop di accept per nuove connessioni
- `handle_client()` - Thread per gestire comunicazioni con client
- `handle_register()` - Gestisce registrazione giocatore
- `handle_create_game()` - Crea nuova partita
- `handle_join_game()` - Gestisce richiesta di join
- `handle_accept_join()` - Accetta/rifiuta join e avvia partita
- `handle_make_move()` - Valida e applica mosse
- `broadcast_game_created()` - Notifica tutti i client registrati

---

### 2. Client (`client/`)

#### Responsabilità
- Connettersi al server via TCP
- Inviare richieste (registrazione, crea/join partita, mosse)
- Ricevere e processare risposte sincrone
- Gestire notifiche asincrone in thread separato
- Interfaccia utente testuale interattiva

#### Architettura Multi-Thread
```
Main Thread                         Notification Thread
     │                                      │
     │ client_connect()                     │
     ├─────────────────────────────────────►│
     │                                      │ Attende notifiche
     │ Menu interattivo                     │ asyncrone dal server
     │                                      │
     │ send_*_request()                     │
     ├──────────────►[Server]               │
     │                                      │
     │◄─────────────[Response]              │
     │                                      │
     │                          [Notify]───►│
     │                                      │ handle_notify()
     │                                      │ Stampa evento
```

#### Struttura Dati Principale

**`client_state_t`** - Stato del client
```c
typedef struct {
    int socket_fd;                       // Socket connessione
    char username[MAX_PLAYER_NAME];      // Nome giocatore
    client_status_t state;               // Stato corrente
    char current_game_id[MAX_GAME_ID_LEN];  // ID partita corrente
    char opponent[MAX_PLAYER_NAME];      // Nome avversario
    char my_symbol;                      // 'X' o 'O'
    pthread_t notification_thread;       // Thread per notifiche
    pthread_mutex_t mutex;               // Mutex per stato condiviso
    bool running;                        // Flag per terminazione thread
    uint32_t seq_id;                     // ID sequenza messaggi
} client_state_t;
```

#### Funzioni Chiave
- `client_connect()` - Connessione al server e avvio thread notifiche
- `send_register_request()` - Richiesta registrazione
- `send_create_game_request()` - Richiesta creazione partita
- `send_join_game_request()` - Richiesta join a partita esistente
- `send_make_move_request()` - Invio mossa
- `client_notify_handler()` - Thread per ricevere notifiche asincrone
- `handle_notify()` - Dispatcher notifiche per tipo

---

### 3. Protocollo di Comunicazione (`shared/include/protocol.h`)

Il protocollo è **binario** e **connection-oriented** (TCP).

#### Formato Messaggio

Ogni messaggio è composto da:
1. **Header fisso** (7 bytes)
2. **Payload variabile** (0-4096 bytes)

```
┌─────────────────────────────────────────────────┐
│              PROTOCOL HEADER (7 bytes)          │
├──────────┬──────────┬───────────────────────────┤
│ msg_type │  length  │         seq_id            │
│ (1 byte) │ (2 bytes)│       (4 bytes)           │
└──────────┴──────────┴┬──────────────────────────┘
                       │
          ┌────────────▼──────────────┐
          │   PAYLOAD (0-4096 bytes)  │
          │   Struttura varia in base │
          │   al tipo di messaggio    │
          └───────────────────────────┘
```

#### Header

```c
typedef struct __attribute__((packed)) {
    uint8_t msg_type;      // Tipo messaggio
    uint16_t length;       // Lunghezza payload (network byte order)
    uint32_t seq_id;       // ID sequenza (network byte order)
} protocol_header_t;
```

#### Tipi di Messaggio

**Client → Server**
- `MSG_REGISTER` (1) - Registrazione giocatore
- `MSG_CREATE_GAME` (2) - Creazione nuova partita
- `MSG_LIST_GAMES` (3) - Richiesta lista partite
- `MSG_JOIN_GAME` (4) - Join a partita
- `MSG_ACCEPT_JOIN` (5) - Accetta/rifiuta join
- `MSG_MAKE_MOVE` (6) - Effettua mossa
- `MSG_LEAVE_GAME` (7) - Abbandona partita
- `MSG_NEW_GAME` (8) - Richiesta nuova partita
- `MSG_QUIT` (9) - Disconnessione

**Server → Client**
- `MSG_RESPONSE` (50) - Risposta a richiesta
- `MSG_NOTIFY` (51) - Notifica asincrona

#### Notifiche Asincrone

Le notifiche sono messaggi inviati dal server **senza una richiesta esplicita** per comunicare eventi:

- `NOTIFY_GAME_CREATED` (100) - Nuova partita disponibile
- `NOTIFY_JOIN_REQUEST` (101) - Qualcuno vuole joinare la tua partita
- `NOTIFY_JOIN_CANCELLATION` (102) - Richiesta join annullata
- `NOTIFY_JOIN_RESPONSE` (103) - Risposta a tua richiesta join
- `NOTIFY_GAME_START` (104) - Partita inizia
- `NOTIFY_MOVE_MADE` (105) - Avversario ha fatto una mossa
- `NOTIFY_GAME_END` (106) - Partita terminata
- `NOTIFY_OPPONENT_LEFT` (107) - Avversario ha abbandonato

---

## 🔄 Thread Safety e Sincronizzazione

### Server

Tutte le operazioni sullo stato condiviso (`server_state`) sono protette dal mutex:

```c
pthread_mutex_lock(&server_state.mutex);
// Accesso/modifica a:
// - server_state.clients[]
// - server_state.games[]
// - server_state.num_clients
// - server_state.num_games
pthread_mutex_unlock(&server_state.mutex);
```

### Client

Il client usa un mutex per proteggere lo stato condiviso tra il thread principale e il thread delle notifiche:

```c
pthread_mutex_lock(&client_state.mutex);
// Accesso/modifica a:
// - client_state.state
// - client_state.current_game_id
// - client_state.opponent
pthread_mutex_unlock(&client_state.mutex);
```

### Race Conditions Evitate

1. **Aggiunta/rimozione client simultanea** - Protetta da mutex
2. **Creazione/join partite concorrenti** - Atomic check-and-set
3. **Accesso al tabellone durante mosse** - Validazione atomica
4. **Lettura/scrittura seq_id** - Operazioni atomiche

---

## ⚙️ Configurazione

### Server (`server/config/server.conf.example`)

```ini
# Rete
server_ip=127.0.0.1
port=90

# Limiti
max_clients=7          # Massimo 7 client simultanei
max_games=4            # Massimo 4 partite simultanee

# Timeout
connection_timeout=300
read_timeout=30

# Logging
log_level=INFO
log_file=logs/server.log
```

### Client (`client/config/client.conf.example`)

```ini
# Rete
server_ip=127.0.0.1
server_port=90

# Timeout
connection_timeout=30
retry_attempts=3

# Logging
log_level=DEBUG
log_file=logs/client.log
```

---

## 🔨 Build System

### Makefile Principale

```bash
make all                # Compila server e client
make server             # Solo server
make client             # Solo client
make clean              # Pulisce object files
make clean-all          # Pulisce tutto (inclusi log)
make run-server         # Compila ed esegue server
make run-client         # Compila ed esegue client
make valgrind-server    # Esegue il server con Valgrind per memory leak detection
make valgrind-helgrind	# Esegue il server con Helgrind per race condition detection
make install-deps 		# Installa eventuali dipendenze necessarie
make test       		# Esegue i test
make help       		# Mostra questo messaggio
```

### Dipendenze di Compilazione

**Server**:
```
server.c → server.h, protocol.h, game_logic.h, logging.h, utils.h
```

**Client**:
```
client.c → client.h, protocol.h, logging.h, utils.h
```

**Shared**:
```
protocol.c   → protocol.h, constants.h
game_logic.c → game_logic.h, constants.h
logging.c    → logging.h
```

