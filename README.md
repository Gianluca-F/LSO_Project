# 🎮 Tris Online - Client-Server Game

Sistema client-server in C per giocare al **Tris (Tic-Tac-Toe)** online.  
Progetto sviluppato per il corso di **Laboratorio di Sistemi Operativi** - Università di Napoli Federico II.

---

## 📋 Caratteristiche

- ✅ **Server multi-thread** con gestione concorrente di client e partite
- ✅ **Protocollo binario custom** per comunicazione efficiente
- ✅ **Sistema di notifiche asincrone** per eventi in tempo reale
- ✅ **Game logic separata** e riutilizzabile
- ✅ **Thread-safety** garantita con mutex
- ✅ **Logging completo** per debugging
- ✅ **Configurazione flessibile** tramite file `.conf`

---

## 🚀 Quick Start

### Prerequisiti

- Sistema Linux/Unix
- Compilatore GCC
- GNU Make

### Installazione e Avvio

```bash
# 1. Clone del progetto
cd LSO_Project

# 2. Compilazione
make

# 3. Avvia il server (in un terminale)
make run-server
# Output: Server in ascolto sulla porta 90...

# 4. Avvia uno o più client (in terminali separati)
make run-client
```

### Primi Passi

**Client 1** (Alice):
```
1. Connetti al server (127.0.0.1:90)
2. Registra username: "Alice"
3. Crea nuova partita
   → Game ID: G12345
   → In attesa di avversario...
```

**Client 2** (Bob):
```
1. Connetti al server
2. Registra username: "Bob"
4. Elenca partite disponibili
   → G12345 - Creatore: Alice
5. Unisciti a partita: G12345
```

**Entrambi**:
```
→ La partita inizia!
→ Alice è X, Bob è O
→ X inizia per primo
8. Fai mosse (1-9)
```

---

## 📂 Struttura del Progetto

```
LSO_Project/
├── client/              # Applicazione client
│   ├── bin/                 # Eseguibile
│   ├── config/              # Configurazione
│   ├── include/             # Header file
│   ├── logs/                # Log dell'esecuzione
│   ├── obj/                 # File oggetto
│   ├── src/                 # Sorgenti
│   └── Makefile
│
├── server/              # Applicazione server
│   ├── bin/                 # Eseguibile
│   ├── config/              # Configurazione
│   ├── include/             # Header file
│   ├── logs/                # Log dell'esecuzione
│   ├── obj/                 # File oggetto
│   ├── src/                 # Sorgenti
│   └── Makefile
│
├── shared/              # Codice condiviso
│   ├── include/             # Protocol, game logic, logging
│   └── src/                 # Implementazioni
│
├── docs/                # Documentazione
│   ├── architecture.md      # Architettura sistema
│   ├── protocol.md          # Specifica protocollo
│   ├── INDEX.md             # Indice documentazione
│   └── developer_guide.md   # Guida sviluppatore
│
├── Makefile             # Build principale
└── README.md            # Questo file
```

---

## 🎯 Funzionalità Implementate

### Client

- [x] Connessione TCP al server
- [x] Registrazione username
- [x] Creazione partite
- [x] Join a partite esistenti
- [x] Accettazione/rifiuto richieste join
- [x] Esecuzione mosse con validazione
- [x] Abbandono partita
- [x] Notifiche asincrone in tempo reale
- [x] Menu interattivo intuitivo

### Server

- [x] Gestione multi-thread per client multipli
- [x] Autenticazione giocatori
- [x] Gestione sessioni di gioco
- [x] Validazione mosse con game logic
- [x] Broadcasting notifiche
- [x] Gestione disconnessioni improvvise
- [x] Configurazione limiti (max client, max partite)
- [x] Logging completo di tutte le operazioni

### Protocollo

- [x] Formato binario efficiente
- [x] Header fisso 7 bytes + payload variabile
- [x] Network byte order per interoperabilità
- [x] 9 tipi di richieste client
- [x] 8 tipi di notifiche asincrone
- [x] Gestione errori con codici specifici

---

## 🔨 Comandi Make

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

---

## ⚙️ Configurazione

Prima di avviare server e client, è bene scrivere i rispettivi file di configurazione (togliendo il <.example> finale).  
Di seguito, degli esempi sul tipo di configurazione che potrebbero avere, ma si è liberi di cambiare i dati come si preferisce.


### Server (`server/config/server.conf.example`)
```ini
server_ip=127.0.0.1
port=90
max_clients=7       # Massimo 7 client simultanei
max_games=4         # Massimo 4 partite simultanee
log_level=INFO      # DEBUG < INFO < WARN < ERROR
log_file=logs/server.log
```

### Client (`client/config/client.conf.example`)

```ini
server_ip=127.0.0.1
server_port=90
log_level=DEBUG      # DEBUG < INFO < WARN < ERROR
log_file=logs/client.log
```

---

## 📖 Documentazione

Documentazione completa disponibile in `docs/`:

- **[architecture.md](docs/architecture.md)** - Architettura del sistema, componenti, strutture dati
- **[protocol.md](docs/protocol.md)** - Specifica dettagliata del protocollo di comunicazione
- **[developer_guide.md](docs/developer_guide.md)** - Guida per sviluppatori, API, estensioni

---

## 🧪 Testing

### Esecuzione Manuale

1. **Avvia il server**
2. **Connetti 2 client**
3. **Esegui scenari di test**:
   - Creazione partita
   - Join e accettazione
   - Partita completa
   - Abbandono
   - Disconnessioni

### Debug con Valgrind

```bash
# Memory leak check
make clean
make CFLAGS="-g -O0"
valgrind --leak-check=full ./server/bin/server
```

### Debug con GDB

```bash
gdb ./server/bin/server
(gdb) break server.c:handle_client
(gdb) run
```

---

## 🛡️ Sicurezza

- ✅ Validazione di tutti gli input utente
- ✅ Controllo lunghezze buffer (no overflow)
- ✅ Gestione thread-safe dello stato condiviso
- ✅ Validazione mosse con game logic
- ✅ Gestione errori completa

---

## 📊 Limiti e Scalabilità

| Parametro | Default | Modificabile in |
|-----------|---------|-----------------|
| Max client simultanei | 7 | `server.conf: max_clients` |
| Max partite simultanee | 4 | `server.conf: max_games` |
| Max lunghezza nome | 32 | `constants.h: MAX_PLAYER_NAME` |
| Max dimensione messaggio | 4096 | `constants.h: MAX_MESSAGE_SIZE` |

---

## 🔮 Possibili Estensioni

- [ ] Autenticazione con password
- [ ] Persistenza partite (database)
- [ ] Statistiche giocatori (win/loss ratio)
- [ ] Chat in-game
- [ ] Spectator mode
- [ ] Matchmaking automatico
- [ ] Client web (WebSocket)
- [ ] Replay partite

---

## 🐛 Troubleshooting

### "Connection refused"
- Verifica che il server sia in esecuzione
- Controlla IP e porta in `client.conf`

### "Address already in use"
```bash
# Trova processo
sudo lsof -i :90
# Termina
kill -9 <PID>
```

### "Server full"
- Server ha raggiunto `max_clients`
- Aumenta il limite in `server.conf`

---

## 📝 Note Tecniche

- **Linguaggio**: C (standard C99)
- **Threading**: POSIX Threads (pthread)
- **Network**: Berkeley Sockets (TCP/IP)
- **Build**: GNU Make
- **Platform**: Linux/Unix

---

## 👥 Autori

Progetto sviluppato da 3 studenti del corso di **Laboratorio di Sistemi Operativi**:  
- Luigi Dota;
- Gianluca Fiorentino;
- Vittorio Emanuele Testa. 
  
Università di Napoli Federico II - Anno Accademico 2024/2025

---

## 📄 Licenza

Progetto didattico - uso esclusivamente accademico.

---

## 🙏 Ringraziamenti

- Allesandra Rossi, prof.ssa del corso LSO per le specifiche del progetto
- Comunità POSIX per la documentazione eccellente
- Compagni di corso per il testing

---

**Per maggiori dettagli, consulta la documentazione completa in `docs/`.** 
