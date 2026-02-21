# 🎮 Tris Online - Client-Server Game

Sistema client-server in C per giocare al **Tris (Tic-Tac-Toe)** online.  
Progetto sviluppato per il corso di **Laboratorio di Sistemi Operativi** - Università di Napoli Federico II.  

>**Premessa**: prima di procedere oltre, si tenga presente che parte della documentazione è stata scritta mediante utilizzo di *Claude Sonnet 4.5*,  strumento ritenuto idoneo ai fini di fornire una lettura scorrevole e più "accattivante". Ciò detto, è garantita la supervisione di un occhio umano per correggere ed espandere ogni singola sotto-sezione, qualora ritenuto necessario. Buona lettura!

---

## 📋 Caratteristiche

- **Server multi-thread** con gestione concorrente di client e partite
- **Protocollo binario custom** per comunicazione efficiente
- **Sistema di notifiche asincrone** per eventi in tempo reale
- **Thread-safety** garantita tramite mutex
- **Game logic separata** per validazione mosse e gestione stato
- **Logging completo** per debugging e monitoring
- **Configurazione flessibile** tramite file `.conf`
- **Deployment Docker** con docker-compose

---

## 🚀 Quick Start

### Opzione 1: Docker (Raccomandato)

**Prerequisiti**: Docker e Docker Compose installati

```bash
# 1. Build delle immagini
docker-compose build

# 2. Avvia il server
docker-compose up -d server

# 3. Avvia i client (in terminali separati)
docker-compose run --rm client 
docker-compose run --rm client 
```

---

### Opzione 2: Build Nativo

**Prerequisiti**: Sistema Linux/Unix, GCC, GNU Make

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
│
├── client/              # Applicazione client
├── server/              # Applicazione server
├── shared/              # Codice condiviso
├── docs/                # Documentazione
│
├── docker-compose.yml   # File di configurazione docker stack
├── Dockerfile.client    # Build docker server
├── Dockerfile.server    # Build docker client
├── Makefile             # Build principale
└── README.md            # Questo file
```

Vedasi [questa sezione](docs/architecture.md#-struttura-del-progetto) per una visione completa del progetto, in cui vengono illustrate anche le sottocartelle.

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
- [x] 10 tipi di notifiche asincrone
- [x] Gestione errori con codici specifici

---

## ⚙️ Configurazione

Prima di avviare server e client, è bene scrivere i rispettivi file di configurazione (togliendo il <.example> finale, e, nel caso del client, scrivendo anche `client.docker.conf`).  
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

- **[INDEX.md](docs/INDEX.md)** - Indice della documentazione, da cui si consiglia caldamente il prosieguo della lettura
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

---

## 🛡️ Sicurezza

- Validazione di tutti gli input utente
- Controllo lunghezze buffer (no overflow)
- Gestione thread-safe dello stato condiviso
- Validazione mosse con game logic
- Gestione errori completa

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
- [ ] Matchmaking automatico
- [ ] Client web (WebSocket)

---

## 🐛 Troubleshooting

### "Connection refused"
- Verifica che il server sia in esecuzione
- Controlla IP e porta in `client.conf`
- Se stai utilizzando Docker, controlla `client.docker.conf`

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
- **Build**: GNU Make e Docker
- **Platform**: Linux/Unix (se con Make) / qualsiasi OS (se con Docker)

---

## 👥 Autori

Progetto sviluppato da 3 studenti del corso di **Laboratorio di Sistemi Operativi**:  
- Luigi Dota;
- Gianluca Fiorentino;
- Vittorio Emanuele Testa. 
  
Università di Napoli Federico II - Anno Accademico 2024/2025.

---

## 📄 Licenza

Progetto didattico - uso esclusivamente accademico.

---

## 🙏 Ringraziamenti

- Alessandra Rossi, prof.ssa del corso LSO per le specifiche del progetto
- Comunità POSIX per la documentazione eccellente
- Compagni di corso per il testing
 
