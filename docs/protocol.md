# Specifica del Protocollo di Comunicazione

## 📋 Panoramica

Il protocollo di comunicazione client-server è un **protocollo binario custom** progettato per efficienza e affidabilità, basato su connessioni **TCP/IP**.

### Caratteristiche

- **Formato binario** - Minimo overhead, massima efficienza
- **Header fisso** - Parsing veloce e deterministico
- **Network byte order** - Compatibilità multi-piattaforma
- **Strutture packed** - Allineamento garantito
- **Sequence ID** - Tracking messaggi e debugging
- **Type-safe** - Validazione tipo messaggio

---

## 🔧 Formato dei Messaggi

### Struttura Generale

Ogni messaggio è composto da due parti:

```
┌─────────────────────────────────────────────────────────┐
│                   MESSAGE STRUCTURE                     │
├───────────────────────────┬─────────────────────────────┤
│    HEADER (7 bytes)       │   PAYLOAD (0-4096 bytes)    │
│         FISSO             │        VARIABILE            │
└───────────────────────────┴─────────────────────────────┘
```

### Header del Protocollo (7 bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;    // Tipo di messaggio (1 byte)
    uint16_t length;      // Lunghezza payload in bytes (2 bytes)
    uint32_t seq_id;      // ID sequenza messaggio (4 bytes)
} protocol_header_t;
```

#### Campi dell'Header

| Campo | Dimensione | Byte Order | Descrizione |
|-------|-----------|------------|-------------|
| `msg_type` | 1 byte | - | Identifica il tipo di messaggio (vedi tabella sotto) |
| `length` | 2 bytes | Network | Lunghezza del payload (0-4096) |
| `seq_id` | 4 bytes | Network | ID sequenziale per tracking (incrementale) |

**Nota**: `length` e `seq_id` sono inviati in **network byte order** (big-endian) usando `htons()` e `htonl()`.

---

## 📨 Tipi di Messaggio

### Client → Server (Request)

| Codice | Nome | Payload | Descrizione |
|--------|------|---------|-------------|
| 1 | `MSG_REGISTER` | `payload_register_t` | Registrazione giocatore |
| 2 | `MSG_CREATE_GAME` | (nessuno) | Crea nuova partita |
| 3 | `MSG_LIST_GAMES` | (nessuno) | Richiesta lista partite |
| 4 | `MSG_JOIN_GAME` | `payload_join_game_t` | Join a partita esistente |
| 5 | `MSG_ACCEPT_JOIN` | `payload_accept_join_t` | Accetta/rifiuta join request |
| 6 | `MSG_MAKE_MOVE` | `payload_make_move_t` | Effettua mossa |
| 7 | `MSG_LEAVE_GAME` | (nessuno) | Abbandona partita |
| 8 | `MSG_NEW_GAME` | (nessuno) | Richiesta nuova partita |
| 9 | `MSG_QUIT` | (nessuno) | Disconnessione |

### Server → Client (Response/Notify)

| Codice | Nome | Payload | Descrizione |
|--------|------|---------|-------------|
| 50 | `MSG_RESPONSE` | (varia) | Risposta a richiesta |
| 51 | `MSG_NOTIFY` | (varia) | Notifica asincrona |

---

## 📤 Messaggi Client → Server

### 1. MSG_REGISTER (1)

**Scopo**: Registra un giocatore con un username.

**Payload**:
```c
typedef struct __attribute__((packed)) {
    char player_name[MAX_PLAYER_NAME];  // 32 bytes
} payload_register_t;
```

**Esempio**:
```
Header: [01][00 20][00 00 00 01]
         │    │        └─ seq_id = 1
         │    └─ length = 32
         └─ msg_type = MSG_REGISTER
         
Payload: "Alice\0\0\0...\0" (32 bytes total)
```

**Risposta**: `response_register_t`

---

### 2. MSG_CREATE_GAME (2)

**Scopo**: Crea una nuova partita e attende che un altro giocatore si unisca.

**Payload**: Nessuno (solo header)

**Esempio**:
```
Header: [02][00 00][00 00 00 02]
         │    │        └─ seq_id = 2
         │    └─ length = 0
         └─ msg_type = MSG_CREATE_GAME
```

**Risposta**: `response_create_game_t` (contiene il `game_id` generato)

**Effetti collaterali**:
- Broadcast `NOTIFY_GAME_CREATED` a tutti i client registrati

---

### 3. MSG_LIST_GAMES (3)

**Scopo**: Richiede la lista di tutte le partite disponibili per il join.

**Payload**: Nessuno

**Esempio**:
```
Header: [03][00 00][00 00 00 03]
```

**Risposta**: `response_list_games_t` + array di `game_info_t`

---

### 4. MSG_JOIN_GAME (4)

**Scopo**: Richiede di unirsi a una partita specifica.

**Payload**:
```c
typedef struct __attribute__((packed)) {
    char game_id[MAX_GAME_ID_LEN];  // 16 bytes
} payload_join_game_t;
```

**Esempio**:
```
Header: [04][00 10][00 00 00 04]
Payload: "G12345\0\0...\0" (16 bytes)
```

**Risposta**: `response_join_game_t`

**Effetti collaterali**:
- Invia `NOTIFY_JOIN_REQUEST` al creatore della partita

---

### 5. MSG_ACCEPT_JOIN (5)

**Scopo**: Risposta del creatore a una richiesta di join (accetta/rifiuta).

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t accept;  // 1 = accetta, 0 = rifiuta
} payload_accept_join_t;
```

**Esempio**:
```
Header: [05][00 01][00 00 00 05]
Payload: [01]  (accetta)
```

**Risposta**: `response_accept_join_t`

**Effetti collaterali**:
- Se accettato: invia `NOTIFY_GAME_START` a entrambi i giocatori
- Se rifiutato: invia `NOTIFY_JOIN_RESPONSE` (accepted=0) al joiner

---

### 6. MSG_MAKE_MOVE (6)

**Scopo**: Effettua una mossa sulla griglia del Tris.

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t pos;  // Posizione 1-9
} payload_make_move_t;
```

**Posizioni della griglia**:
```
 1 | 2 | 3
-----------
 4 | 5 | 6
-----------
 7 | 8 | 9
```

**Esempio**:
```
Header: [06][00 01][00 00 00 06]
Payload: [05]  (mossa al centro)
```

**Risposta**: `response_make_move_t`

**Effetti collaterali**:
- Invia `NOTIFY_MOVE_MADE` all'avversario
- Se partita terminata: invia `NOTIFY_GAME_END` a entrambi

---

### 7. MSG_LEAVE_GAME (7)

**Scopo**: Abbandona la partita corrente.

**Payload**: Nessuno

**Risposta**: `response_leave_game_t`

**Effetti collaterali**:
- Invia `NOTIFY_OPPONENT_LEFT` all'avversario

---

### 8. MSG_NEW_GAME (8)

**Scopo**: Richiede una nuova partita dopo che una è terminata (stesso avversario).

**Payload**: Nessuno

**Risposta**: `response_new_game_t`

---

### 9. MSG_QUIT (9)

**Scopo**: Disconnessione pulita dal server.

**Payload**: Nessuno

**Risposta**: `response_quit_t`

---

## 📥 Messaggi Server → Client

### Risposte (MSG_RESPONSE = 50)

Tutte le risposte hanno header con `msg_type = MSG_RESPONSE` e payload che varia in base alla richiesta originale.

#### Struttura Base Risposta

```c
typedef struct __attribute__((packed)) {
    uint8_t status;      // STATUS_OK (0) o STATUS_ERROR (1)
    uint8_t error_code;  // Codice errore (0 se nessun errore)
} response_generic_t;
```

#### Codici di Errore

| Codice | Nome | Descrizione |
|--------|------|-------------|
| 0 | `ERR_NONE` | Nessun errore |
| 1 | `ERR_GAME_NOT_FOUND` | Partita non esistente |
| 2 | `ERR_GAME_FULL` | Partita già piena |
| 3 | `ERR_REQUEST_PENDING` | Richiesta già pendente |
| 4 | `ERR_NO_PENDING_JOIN` | Nessuna richiesta da accettare |
| 7 | `ERR_ALREADY_IN_GAME` | Già in una partita |
| 8 | `ERR_NOT_IN_GAME` | Non in una partita |
| 9 | `ERR_NOT_YOUR_TURN` | Non è il tuo turno |
| 10 | `ERR_INVALID_MOVE` | Mossa non valida |
| 11 | `ERR_CELL_OCCUPIED` | Cella già occupata |
| 20 | `ERR_NOT_REGISTERED` | Non registrato |
| 21 | `ERR_ALREADY_REGISTERED` | Già registrato |
| 22 | `ERR_INVALID_NAME` | Nome non valido |
| 23 | `ERR_NAME_TAKEN` | Nome già in uso |
| 90 | `ERR_SERVER_FULL` | Server pieno |
| 99 | `ERR_INTERNAL` | Errore interno server |

---

#### response_register_t

```c
typedef response_generic_t response_register_t;
```

**Esempio successo**:
```
Header: [32][00 02][00 00 00 01]  (msg_type=50, length=2, seq_id=1)
Payload: [00][00]  (status=OK, error_code=NONE)
```

**Esempio errore**:
```
Header: [32][00 02][00 00 00 01]
Payload: [01][17]  (status=ERROR, error_code=ERR_NAME_TAKEN)
```

---

#### response_create_game_t

```c
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
    char game_id[MAX_GAME_ID_LEN];  // 16 bytes
} response_create_game_t;
```

**Esempio**:
```
Header: [32][00 12][00 00 00 02]  (18 bytes payload)
Payload: [00][00]["G12345\0\0...\0"]
         status=OK, error=NONE, game_id
```

---

#### response_list_games_t

```c
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
    uint8_t game_count;     // Numero di partite
    uint8_t reserved;       // Padding
    // Seguito da: game_info_t games[game_count]
} response_list_games_t;

typedef struct __attribute__((packed)) {
    char game_id[MAX_GAME_ID_LEN];     // 16 bytes
    char creator[MAX_PLAYER_NAME];     // 32 bytes
    uint8_t status;                    // GAME_WAITING, etc.
    uint8_t players_count;             // 0-2
} game_info_t;  // 50 bytes totali
```

**Esempio** (2 partite):
```
Header: [32][00 68][00 00 00 03]  (104 bytes = 4 + 50*2)
Payload:
  [00][00][02][00]  (status=OK, err=NONE, count=2, reserved)
  [game_info_t #1]  (50 bytes)
  [game_info_t #2]  (50 bytes)
```

---

#### response_join_game_t

```c
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t error_code;
    uint8_t your_symbol;               // 'X' o 'O' (se accettato)
    char opponent[MAX_PLAYER_NAME];    // 32 bytes
    char game_id[MAX_GAME_ID_LEN];     // 16 bytes
} response_join_game_t;
```

---

### Notifiche (MSG_NOTIFY = 51)

Le notifiche sono messaggi **asincroni** inviati dal server quando accadono eventi.

Tutte le notifiche hanno header con `msg_type = MSG_NOTIFY` e il primo byte del payload indica il tipo di notifica.

#### Tipi di Notifica

| Codice | Nome | Descrizione |
|--------|------|-------------|
| 100 | `NOTIFY_GAME_CREATED` | Nuova partita disponibile |
| 101 | `NOTIFY_JOIN_REQUEST` | Qualcuno vuole joinare |
| 102 | `NOTIFY_JOIN_CANCELLATION` | Join annullato |
| 103 | `NOTIFY_JOIN_RESPONSE` | Risposta a tua richiesta join |
| 104 | `NOTIFY_GAME_START` | Partita inizia |
| 105 | `NOTIFY_MOVE_MADE` | Avversario ha mosso |
| 106 | `NOTIFY_GAME_END` | Partita terminata |
| 107 | `NOTIFY_OPPONENT_LEFT` | Avversario ha abbandonato |

---

#### NOTIFY_GAME_CREATED (100)

**Quando**: Un altro client crea una partita

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t notify_type;               // 100
    char game_id[MAX_GAME_ID_LEN];     // 16 bytes
    char creator[MAX_PLAYER_NAME];     // 32 bytes
} notify_game_created_t;  // 49 bytes
```

---

#### NOTIFY_JOIN_REQUEST (101)

**Quando**: Sei il creatore e qualcuno vuole joinare

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t notify_type;          // 101
    char opponent[MAX_PLAYER_NAME];  // 32 bytes
} notify_join_request_t;  // 33 bytes
```

---

#### NOTIFY_JOIN_CANCELLATION (102)

**Quando**: Il joiner ha cancellato o il creatore si è disconnesso

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t notify_type;               // 102
    uint8_t is_cancelled_by_joiner;    // 1=joiner, 0=creatore
    char opponent[MAX_PLAYER_NAME];
} notify_join_cancellation_t;
```

---

#### NOTIFY_JOIN_RESPONSE (103)

**Quando**: Il creatore ha accettato/rifiutato la tua richiesta

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t notify_type;            // 103
    uint8_t accepted;               // 1=accettato, 0=rifiutato
    char game_id[MAX_GAME_ID_LEN];
} notify_join_response_t;
```

---

#### NOTIFY_GAME_START (104)

**Quando**: Partita sta per iniziare

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t notify_type;            // 104
    uint8_t your_symbol;            // 'X' o 'O'
    uint8_t first_player;           // 'X' o 'O' (chi inizia)
    char opponent[MAX_PLAYER_NAME];
} notify_game_start_t;
```

**Note**: Il giocatore 'X' inizia sempre per primo

---

#### NOTIFY_MOVE_MADE (105)

**Quando**: L'avversario ha fatto una mossa

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t notify_type;                // 105
    uint8_t pos;                        // Posizione 1-9
    char player[MAX_PLAYER_NAME];       // Chi ha mosso
    char board[BOARD_SIZE];             // Stato aggiornato (9 bytes)
} notify_move_made_t;
```

**board[9]**: Contiene 'X', 'O', o ' ' (spazio per cella vuota)

---

#### NOTIFY_GAME_END (106)

**Quando**: Partita terminata (vittoria/pareggio/abbandono)

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t notify_type;     // 106
    uint8_t result;          // RESULT_WIN (1), RESULT_LOSE (2), RESULT_DRAW (3)
    char board[BOARD_SIZE];  // Stato finale (9 bytes)
} notify_game_end_t;
```

---

#### NOTIFY_OPPONENT_LEFT (107)

**Quando**: L'avversario ha abbandonato la partita

**Payload**:
```c
typedef struct __attribute__((packed)) {
    uint8_t notify_type;  // 107
} notify_opponent_left_t;  // 1 byte
```

---

## 🔄 Sequence ID

Ogni messaggio ha un `seq_id` incrementale che serve per:

1. **Debugging** - Tracciare la sequenza di messaggi nei log
2. **Correlazione** - Collegare richieste e risposte
3. **Diagnostica** - Rilevare messaggi persi o fuori ordine

**Gestione**:
- Il client mantiene un contatore `client_state.seq_id`
- Incrementato ad ogni invio: `seq_id++`
- Il server può rispondere con lo stesso `seq_id` della richiesta

---

## 🌐 Network Byte Order

### Conversioni

**Invio** (host → network):
```c
header.length = htons(payload_size);  // host to network short
header.seq_id = htonl(sequence);      // host to network long
```

**Ricezione** (network → host):
```c
uint16_t len = ntohs(header.length);
uint32_t seq = ntohl(header.seq_id);
```

### Perché?

Diversi processori usano diverse convenzioni (little-endian vs big-endian). Il protocollo TCP/IP usa **big-endian** (network byte order) per garantire interoperabilità.

---

## 📡 Funzioni di Invio/Ricezione

### protocol_send()

Invia un messaggio completo (header + payload) atomicamente.

```c
ssize_t protocol_send(int sockfd, uint8_t msg_type, 
                     const void *payload, size_t payload_size,
                     uint32_t seq_id);
```

**Implementazione**:
1. Inizializza header con `msg_type`, `payload_size`, `seq_id`
2. Converte `length` e `seq_id` in network byte order
3. Invia header (7 bytes)
4. Se `payload != NULL`, invia payload
5. Ritorna bytes totali inviati o -1 se errore

---

### protocol_recv_header()

Riceve esattamente 7 bytes (header) dal socket.

```c
ssize_t protocol_recv_header(int sockfd, protocol_header_t *header);
```

**Implementazione**:
1. Loop fino a leggere 7 bytes (gestisce ricezione parziale)
2. Converte `length` e `seq_id` in host byte order
3. Ritorna 7 se successo, 0 se connessione chiusa, -1 se errore

---

### protocol_recv_payload()

Riceve esattamente `length` bytes dal socket.

```c
ssize_t protocol_recv_payload(int sockfd, void *buffer, size_t length);
```

**Implementazione**:
1. Loop fino a leggere tutti i bytes richiesti
2. Gestisce ricezione parziale con `recv()` multipli
3. Ritorna bytes letti o -1 se errore

---

## 🔍 Validazione

### protocol_validate_name()

Valida un nome giocatore.

**Regole**:
- Non vuoto
- Max 32 caratteri (incluso `\0`)
- Solo alfanumerici e underscore
- Primo carattere deve essere lettera

```c
int protocol_validate_name(const char *name);
// Ritorna: 1 se valido, 0 altrimenti
```

---

### protocol_validate_move()

Valida una posizione di mossa.

**Regole**:
- Deve essere tra 1 e 9 (inclusi)

```c
int protocol_validate_move(uint8_t pos);
// Ritorna: 1 se valida, 0 altrimenti
```

---

## 📊 Diagrammi di Sequenza

### Scenario: Creazione e Join Partita

```
Alice (Client 1)       Server            Bob (Client 2)
     │                   │                      │
     ├──MSG_REGISTER────►│                      │
     │      "Alice"      │                      │
     │◄──RESPONSE (OK)───┤                      │
     │- - - - - - - - - -│- - - - - - - - - - - │
     │                   │◄─────MSG_REGISTER────┤
     │                   │         "Bob"        │
     │                   ├───RESPONSE (OK)─────►│
     │- - - - - - - - - -│- - - - - - - - - - - │
     ├──MSG_CREATE_GAME─►│                      │
     │◄────RESPONSE──────┤                      │
     │   game_id="G123"  │                      │
     │                   ├─NOTIFY_GAME_CREATED─►│
     │                   │      (broadcast)     │
     │- - - - - - - - - -│- - - - - - - - - - - │
     │                   │◄────MSG_JOIN_GAME────┤
     │                   │      "G123"          │
     │                   ├───RESPONSE (OK)─────►│
     │◄──NOTIFY_JOIN_REQ─┤                      │
     │   opponent="Bob"  │                      │
     │- - - - - - - - - -│- - - - - - - - - - - │
     ├──MSG_ACCEPT_JOIN─►│                      │
     │     accept=1      │                      │
     │◄──RESPONSE (OK)───┤                      │
     │                   ├──NOTIFY_JOIN_RESP───►│
     │                   │                      │
     │◄─NOTIFY_GAME_START├──NOTIFY_GAME_START──►│
     │   symbol=X        │   symbol=O           │
     │   first=X         │   first=X            │
```

---

### Scenario: Svolgimento Partita

```
Alice (X)              Server              Bob (O)
  │                      │                   │
  ├─MSG_MAKE_MOVE───────►│                   │
  │     pos=5            │ [Valida mossa]    │
  │◄─RESPONSE (OK)───────┤                   │
  │                      ├─NOTIFY_MOVE_MADE─►│
  │                      │  pos=5, board[]   │
  │- - - - - - - - - - - │- - - - - - - - - -│
  │                      │                   │
  │                      │◄──MSG_MAKE_MOVE───┤
  │   [Valida mossa]     │       pos=1       │
  │                      ├─RESPONSE (OK)────►│
  │◄───NOTIFY_MOVE_MADE──┤                   │
  │    pos=1, board[]    │                   │
  │- - - - - - - - - - - │- - - - - - - - - -│
  │                      │                   │
  │   ... (continua fino a fine partita) ... │
  │                      │                   │
  │- - - - - - - - - - - │- - - - - - - - - -│
  │◄───NOTIFY_GAME_END───┼─NOTIFY_GAME_END──►│
  │  result=WIN          │  result=LOSE      │
```

---

## 🛡️ Gestione Errori

### Errori di Rete

**Connessione persa durante invio**:
- `protocol_send()` ritorna -1
- Client deve disconnettersi e potenzialmente riconnettersi

**Connessione persa durante ricezione**:
- `protocol_recv_header()` ritorna 0 (EOF)
- Server rimuove client e notifica avversario (`NOTIFY_OPPONENT_LEFT`)

---

### Errori Applicativi

**Stato non valido**:
```
Client: MSG_MAKE_MOVE (ma non è in partita)
Server: RESPONSE { status=ERROR, error_code=ERR_NOT_IN_GAME }
```

**Mossa non valida**:
```
Client: MSG_MAKE_MOVE { pos=5 } (cella occupata)
Server: RESPONSE { status=ERROR, error_code=ERR_CELL_OCCUPIED }
```

---

## 🔧 Esempio di Implementazione

### Invio Messaggio

```c
// Client: Invia richiesta di registrazione
payload_register_t payload;
strncpy(payload.player_name, "Alice", MAX_PLAYER_NAME);

uint32_t seq = client_state.seq_id++;
ssize_t sent = protocol_send(client_state.socket_fd, 
                            MSG_REGISTER, 
                            &payload, 
                            sizeof(payload), 
                            seq);

if (sent < 0) {
    LOG_ERROR("Errore invio MSG_REGISTER");
    return -1;
}
```

---

### Ricezione Messaggio

```c
// Server: Ricevi e processa messaggio
protocol_header_t header;
ssize_t received = protocol_recv_header(client_fd, &header);

if (received <= 0) {
    LOG_INFO("Client disconnesso");
    return;
}

// Alloca buffer per payload
char payload_buffer[MAX_MESSAGE_SIZE];
if (header.length > 0) {
    protocol_recv_payload(client_fd, payload_buffer, header.length);
}

// Dispatching per tipo
switch (header.msg_type) {
    case MSG_REGISTER:
        handle_register(client_fd, &header, payload_buffer);
        break;
    case MSG_CREATE_GAME:
        handle_create_game(client_fd, &header);
        break;
    // ...
}
```

---

## 📏 Limiti e Costanti

| Costante | Valore | Descrizione |
|----------|--------|-------------|
| `MAX_PLAYER_NAME` | 32 | Lunghezza massima nome giocatore |
| `MAX_GAME_ID_LEN` | 16 | Lunghezza massima ID partita |
| `MAX_MESSAGE_SIZE` | 4096 | Dimensione massima payload |
| `BOARD_SIZE` | 9 | Dimensione tabellone Tris (3x3) |

---

## 🎯 Best Practices

### Performance

1. **Minimizza invii** - Raggruppa dati quando possibile
2. **Usa buffer adeguati** - Evita allocazioni eccessive
3. **Non bloccare** - Usa thread separati per ricezione

### Robustezza

1. **Valida sempre** - Input utente, dimensioni payload, tipi messaggio
2. **Gestisci timeout** - Non aspettare indefinitamente
3. **Log tutto** - Debugging diventa triviale

### Sicurezza

1. **Controlla lunghezze** - Evita buffer overflow
2. **Valida stato** - Non fidarti del client
3. **Sanitizza input** - Previeni injection

---

**Versione Protocollo**: 1.5  
**Ultima Modifica**: 04 Febbraio 2026  
**Compatibilità**: Retrocompatibile con versione iniziale
