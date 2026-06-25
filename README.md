# Progetto Tris Multi-Client

Questo documento illustra le scelte architetturali e progettuali adottate per la realizzazione del progetto.

## 1. Modello di concorrenza: gestione di client multipli

Il requisito fondamentale del progetto prevede che il server sia in grado di gestire più client contemporaneamente, consentendo la creazione e la partecipazione a diverse partite simultanee. Per affrontare questo aspetto, è stata necessaria un'attenta riflessione sul modello di concorrenza da adottare.

### Scelta adottata: Architettura multi-thread
Con questa soluzione, il server genera un nuovo thread per ogni nuovo client connesso. Ogni thread gestisce in modo indipendente la comunicazione e la logica di gioco relative al proprio client.
*   **Pro:**
    *   Modello di programmazione lineare e intuitivo: ogni thread gestisce il proprio ciclo di vita (lettura/scrittura sul socket) in modo isolato, bloccandosi su istruzioni sincrone senza penalizzare gli altri giocatori.
    *   Pieno supporto al parallelismo e all'utilizzo delle CPU moderne multi-core.
    *   Isolamento degli errori a livello di singola partita.
*   **Contro:**
    *   Consumo di risorse (memoria per lo stack di ogni thread).
    *   **Problemi di concorrenza reali:** varie entità devono essere accessibili in lettura e scrittura da più thread simultaneamente. Senza controlli, modifiche simultanee causano stati inconsistenti.

### Motivazione della scelta e utilizzo dei mutex
Si è optato per un'architettura **multi-thread** perché la natura stocastica del gioco (i giocatori impiegano tempi variabili per pensare e muovere) si adatta poco a modelli di single-thread event loop.

Tuttavia, l'implementazione del multi-threading ha reso necessario l'uso dei **mutex (Mutual Exclusions)** ovunque siano presenti risorse condivise, in particolare:
- la lista globale dei giocatori;
- le strutture dati che gestiscono le singole partite (board di gioco, stato partita, partecipanti).

In caso di accesso sequenziale (senza mutex e multi-threading), non ci sarebbe mai il rischio di sovrascrittura, ma adottando il multi-threading, i mutex diventano **obbligatori**.
Un esempio pratico: quando un giocatore richiede l'accesso a una partita in stato di "attesa", il server verifica che ci sia un posto libero e, se disponibile, lo occupa. In assenza di mutex, due thread potrebbero leggere contemporaneamente "1 posto libero" e inserire entrambi il rispettivo giocatore, superando il limite della partita (2 giocatori). Un mutex sulla struttura della *partita* garantisce che il blocco *"controllo disponibilità + inserimento nuovo giocatore"* sia trattato come un'operazione **atomica**.

## 2. Architettura del software (modularità)

Nel progetto, la gestione formale dei moduli si riflette nella struttura delle directory (`client/`, `server/`, `shared/`).

*   **Modulo `shared/`:** requisito architetturale fondamentale; contiene il protocollo di rete (messaggi e relativa codifica/decodifica) e la logica del Tris. Centralizzare questa implementazione garantisce che, in caso di modifica della struttura di un messaggio o della definizione di vittoria/sconfitta, sia necessario intervenire in un solo punto, evitando disallineamenti tra Client e Server.
*   **Separazione netta:** la chiara distinzione tra `src` e `include` mantiene ordinata la build gestita dai Makefile dei sottomoduli e garantisce una migliore manutenibilità e un'immediata comprensione dell'architettura software.

## 3. Gestione e sincronizzazione degli stati di gioco

La traccia identifica esplicitamente numerosi stati per le partite: *"terminata, in corso, in attesa, nuova creazione"* e per l'esito rispetto ai giocatori: *"vittoria, sconfitta, pareggio"*.

*   **Scelta progettuale per l'identificazione della partita:** la traccia richiede "partite identificate in modo univoco". È stata implementata un'assegnazione di ID progressivo, anch'essa protetta da mutex durante la generazione (nella fase di "nuova creazione").
*   **Scambio messaggi e broadcast:** la problematica di generare messaggi diversi è stata risolta definendo due macro-categorie di messaggistica: *Punto-Punto* (tra thread server e socket client specifico) e *Broadcast*. 
Per implementare la notifica di "invito globale", il server deve attraversare la lista di tutti i giocatori registrati e liberi (altro motivo per cui è fondamentale un mutex sulla lista degli utenti) e inviare il messaggio sul loro socket per annunciare che è disponibile una partita "in attesa".

## 4. Utilizzo di Docker e containerizzazione

Per semplificare l'esecuzione, il testing e il deployment dell'applicazione, il progetto integra il supporto a **Docker** tramite i file `Dockerfile.client`, `Dockerfile.server` e `docker-compose.yml`. 

Le principali utilità e motivazioni dietro questa scelta includono:
* **Isolamento dell'ambiente e consistenza:** i container garantiscono che client e server vengano eseguiti in un ambiente isolato, contenente le esatte dipendenze richieste (es. il compilatore C). Questo elimina il classico problema legato alle differenze tra le varie configurazioni delle macchine host.
* **Orchestrazione semplificata tramite compose:** utilizzando `docker-compose`, è possibile predisporre e avviare l'infrastruttura (un'istanza server e una o più istanze client per i test) lanciando un unico comando da terminale.