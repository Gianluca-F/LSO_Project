# Architettura del Progetto LSO

## Panoramica
Questo progetto implementa un'architettura client-server in C per il corso di Laboratorio di Sistemi Operativi.

## Struttura del Progetto

```
LSO_Project/
├── shared/          # Codice condiviso tra client e server
├── server/          # Codice del server
├── client/          # Codice del client
├── docs/            # Documentazione
└── Makefile         # Build dell'intero progetto
```

## Componenti Principali

### Server
- **Porta di default**: 8080
- **Architettura**: Multi-thread per gestire client multipli
- **Configurazione**: `server/config/server.conf`
- **Log**: `server/logs/server.log`

### Client
- **Configurazione**: `client/config/client.conf`
- **Connessione**: TCP socket al server

### Protocollo di Comunicazione
Definito in `shared/include/protocol.h`:
- Messaggi strutturati con tipo e payload
- Serializzazione/deserializzazione dei dati

## Come Compilare

```bash
# Compila tutto
make

# Compila solo il server
make server

# Compila solo il client
make client
```

## Come Eseguire

```bash
# Esegui il server
make run-server

# Esegui il client (in un altro terminale)
make run-client
```

## Note per lo Sviluppo
- Tutti i file header condivisi vanno in `shared/include/`
- Configurazioni specifiche vanno nelle rispettive cartelle `config/`
- I log del server vengono salvati in `server/logs/`
