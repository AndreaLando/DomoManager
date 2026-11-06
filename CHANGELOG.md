# Changelog

Questo documento registra le modifiche significative di DomoManager.

Le voci distinguono tra:

- funzionalità introdotte;
- modifiche architetturali;
- correzioni;
- elementi deprecati;
- limitazioni note;
- attività pianificate.

> **Il changelog descrive modifiche verificabili del progetto. Filosofia, panoramica e architettura completa restano nei documenti tecnici dedicati.**

## [Unreleased]

### Added

#### RuntimeStore e stato tipizzato

- introdotto `DMRuntimeStore` come storage complementare al Buffer numerico;
- aggiunto accesso O(1) tramite handle;
- aggiunto supporto per `BOOL`, `INT32`, `FLOAT`, `STRING32`, `STRING64`, `JSON128` e `JSON256`;
- aggiunti controllo accessi, metadati, revisioni, dirty tracking e memory guard;
- aggiunto supporto agli ID runtime nei modelli Introspection e Home Assistant.

#### Introspection e modello semantico

- introdotto `DomoIntrospection` per costruire il modello strutturale senza duplicare la configurazione;
- aggiunti origin, group, relation, riferimenti alla sorgente e flag `implementationOnly`;
- introdotto `DomoSemanticResolver` per assegnare kind, unità, device class, state class, accesso e visibilità;
- aggiunti gruppi logici per controller, device fisici, Security e Solar;
- aggiunta diagnostica per entity `UNKNOWN`, hidden e implementation-only.

#### Home Assistant (`DMHAOrchestrator`)

- aggiunta generazione MQTT Discovery dal modello semantico;
- aggiunto Device Registry per controller, dispositivi fisici, Security e Solar;
- aggiunti topic per aree Buffer e variabili RuntimeStore;
- aggiunti availability, subscription dei comandi, snapshot iniziale e aggiornamenti incrementali;
- aggiunta coda `PendingState`;
- aggiunto routing inbound verso Buffer, EventManager o RuntimeStore;
- aggiunto recovery completo dopo disconnessione.

#### Eventi e Physical I/O

- introdotto `PhysicalIOManager` per polling, active-level conversion e debounce non bloccante;
- aggiunte source dedicate per ingressi e consumer delle uscite;
- aggiunto retry implicito quando la push nella coda eventi non riesce;
- consolidato l'uso di `EventManager` per separare produzione e consumo degli eventi.

#### Networking e piattaforme

- aggiunto il backend comune `DMPlatform` per Opta ed ESP32-S3;
- aggiunti backend condivisi per Ethernet e Modbus;
- aggiunto supporto W5500 e TCA9554 sulla piattaforma ESP32-S3 prevista;
- introdotti `DMNetworkManager` e `SocketManager` per arbitraggio, pacing, ownership e lifecycle delle risorse di rete;
- aggiunto stato tecnico comune dei frontend tramite `FrontendStatus`.

#### Power, Weather, Solar e Web API

- aggiunto `PowerSupervisor` con validazione, isteresi, allarmi, segnali extra e snapshot;
- aggiunto engine Weather con medie mobili, giorno/notte, pioggia, raffiche e debounce degli eventi;
- aggiunto supporto Solar con trasporti UART/TCP, discovery, polling con pacing, CRC e snapshot;
- aggiunto motore Web API client/server non bloccante con code, correlazione delle risposte, autenticazione e parsing;
- aggiunti profili applicativi per controllo accessi e dispositivi Web API.

### Changed

#### Modello dello stato

- il Buffer è definito come fonte autorevole dello stato applicativo numerico condiviso;
- RuntimeStore è definito come fonte autorevole dello stato applicativo tipizzato;
- lo stato tecnico interno resta sotto l'ownership dei rispettivi engine;
- route, toggle e split sono attribuiti ai relativi manager logici e non allo storage Buffer.

#### Ciclo di esecuzione

- documentato `DMRuntime::loop()` come ciclo cooperativo e incrementale;
- separati polling I/O, avanzamento Modbus, elaborazione limitata degli eventi e task pesati;
- formalizzati `ipCycleCompleted`, `backendCycleDone` e `backend1StCycleDone`;
- limitato il lavoro per iterazione per preservare la progressione del runtime;
- aggiunte misurazione dei callback, media fixed-point e rilevazione degli spike.

#### Modbus

- aggiunta precedenza controllata per le scritture pendenti;
- mantenuti `ShouldQuery()`, pacing, cooldown e arbitraggio anche nei cicli di scrittura;
- aggiornato lo stato del protocollo attraverso `DMNetworkManager`;
- distinto ciclo avviato da ciclo realmente eseguito.

#### Security

- consolidate bitmask di stato, memoria, nuovi eventi e tacitazione;
- resi simmetrici i percorsi read/write tramite codec e helper condivisi;
- mantenuta nel dominio Security la definizione canonica dei bit;
- evitata la duplicazione delle mappe nel frontend Home Assistant.

#### MQTT

- introdotto un messaggio inbound normalizzato per mapping numerici, testuali, raw e comandi Home Assistant;
- aggiunti area, runtime handle, Runtime ID, tipo e origine del messaggio;
- chiarita la validità temporanea di topic e payload durante la callback sincrona.

#### Documentazione

- rifattorizzati filosofia, panoramica, architettura, dipendenze, diagrammi, flusso dati, ciclo di esecuzione e README;
- separati grafo fisico, grafo architetturale e dipendenze runtime;
- normalizzati heading, diagrammi, terminologia e ownership;
- sostituito il modello del Communication Scheduler universale con i lifecycle effettivi dei protocolli e degli orchestratori.

### Fixed

- corretta la rappresentazione architetturale del Buffer, evitando di attribuirgli tutto lo stato tecnico del sistema;
- corretta la rappresentazione uniforme del Task Engine, allineandola a task pesati, intervalli, callback e macchine a stati;
- corretta la rappresentazione delle comunicazioni, che non attraversano un unico scheduler universale;
- chiarito che un ciclo è completato solo dopo progressione reale;
- chiarita la necessità di ripristinare subscription, availability, discovery e snapshot dopo una disconnessione;
- chiarita la distinzione tra comando richiesto e stato realmente confermato;
- formalizzata l'ownership unica dell'inizializzazione hardware.

### Deprecated

- gestione diretta di comunicazioni complesse nel percorso applicativo quando è disponibile un engine o orchestratore dedicato;
- mappe Home Assistant parallele per bitmask o semantica già possedute dai domain engine;
- uso di ID `AREA_*` specifici dell'installazione nei moduli generici;
- uso di ricerche ripetute per Runtime ID nel percorso ordinario quando è disponibile un handle;
- assunzione che tutti i servizi di comunicazione condividano lo stesso scheduler timed round-robin.

### Known issues

- l'inizializzazione platform deve rimanere in un solo componente proprietario, evitando chiamate duplicate;
- i getter Weather devono gestire esplicitamente il periodo precedente al primo campione valido;
- la configurazione Weather deve utilizzare reader e aree effettivamente dedicate;
- credenziali, codici e segreti di installazione non devono essere inclusi nella documentazione pubblica o nel repository distribuito;
- i confronti degli snapshot Solar devono applicare una politica coerente per i valori floating-point;
- le configurazioni con più servizi Ethernet simultanei richiedono verifica sulla piattaforma target;
- le API basate su `std::initializer_list` richiedono storage con durata compatibile;
- le definizioni Web API devono utilizzare valori validi per i rispettivi enum e tipi.

### Planned

- consolidamento dei test Opta ed ESP32-S3;
- test di carico per configurazioni Ethernet concorrenti;
- test del recovery MQTT e dello snapshot Home Assistant;
- test estesi Hot Standby, RS485, Web API e Solar;
- diagnostica più dettagliata di socket, code, tempi e backlog;
- verifica automatica delle dipendenze fisiche dagli `#include` reali;
- hardening della configurazione e gestione sicura dei secret;
- ulteriore copertura dei test per Buffer, RuntimeStore, EventManager e domain engine.

## [Beta - Architecture and Runtime Stabilization]

Questa fase consolida l'architettura runtime della Beta pubblica, con particolare attenzione a determinismo, non-blocking, diagnostica e uso delle risorse embedded.

### Added

#### Orchestrazione frontend

- introdotta una struttura centralizzata per definizione, configurazione ed esecuzione dei task frontend;
- aggiunti task per Security/Sensors, HVAC, Averages e comunicazioni;
- aggiunti intervalli, abilitazione e callback dedicate;
- aggiunto il rilevamento del completamento del ciclo frontend.

#### Scheduling delle comunicazioni

- introdotto un modello dedicato di scheduling per Bridge, MQTT e Web API nella fase Beta;
- aggiunti intervallo minimo, priorità, abilitazione e callback;
- distribuita nel tempo l'esecuzione dei servizi di comunicazione;
- ridotta la concorrenza temporale sulle risorse Ethernet e socket.

#### Frontend engine

- integrato HVAC nel ciclo frontend;
- integrato il ciclo Security/Sensors con aggiornamenti solo in presenza di variazioni;
- integrato il calcolo delle medie nel ciclo frontend;
- mantenuta la separazione tra task applicativi e servizi di comunicazione.

### Changed

#### Ciclo frontend

- resa più esplicita la sequenza backend, aggiornamento Buffer, task frontend e completamento del ciclo;
- mantenute le API di completamento e reset del ciclo frontend;
- separata la politica di esecuzione delle comunicazioni dalla logica applicativa.

#### Servizi di comunicazione

- Bridge, MQTT e Web API sono trattati come servizi indipendenti;
- le operazioni HTTP mantengono code e correlazione request/response;
- MQTT rimane un livello di integrazione verso Home Assistant, Zigbee2MQTT e altri ecosistemi;
- rafforzato il requisito non-blocking per Modbus, MQTT, Web API, Bridge e HMI.

#### Resource management

- resa esplicita la pressione esercitata da HMI, MQTT, Modbus TCP, Bridge e Web API sulle risorse Ethernet;
- aggiunti obiettivi di controllo per timeout, retry, connessioni e saturazione dei socket;
- integrata la gestione delle risorse nella diagnostica runtime.

### Performance

- mantenuto lookup O(1) del Buffer;
- mantenuto change tracking delle aree;
- mantenute medie mobili O(1) nel dominio Weather;
- ridotte esecuzioni frontend non necessarie tramite intervalli e abilitazione;
- distribuite le comunicazioni nel tempo.

### Diagnostics

- aggiunti controlli per task, scheduler, comunicazioni, Buffer, dispositivi, RTC, Security, HVAC, Power, Weather e watchdog;
- distinta la diagnostica tra task lento, comunicazione lenta, timeout, errore dispositivo e pressione sulle risorse.

### Stability

- ridotte le operazioni potenzialmente bloccanti;
- subordinati i task dipendenti da dati validi al completamento del power-on cycle;
- mantenuta la separazione master/slave nel modello Hot Standby;
- mantenuta la distinzione tra funzioni locali e integrazioni cloud opzionali.

### Limitations

La fase rimane Beta. Le aree dichiarate per ulteriore lavoro comprendono:

- tuning dello scheduling delle comunicazioni;
- ottimizzazione delle socket Ethernet;
- diagnostica dettagliata dei tempi;
- profili Web API;
- auto-tuning Power;
- test Hot Standby;
- test con più servizi Ethernet simultanei;
- strumenti grafici di configurazione.

### Towards RC1

Gli obiettivi dichiarati verso RC1 comprendono:

- diagnostica dei tempi per task;
- diagnostica delle risorse Ethernet;
- monitoraggio socket;
- gestione dei timeout;
- editor JSON;
- ulteriori profili Web API;
- dashboard diagnostica;
- miglioramenti Solar e ACS;
- supporto a ulteriori pompe di calore;
- test RS485, Hot Standby ed Ethernet sotto carico.

## Convenzioni per le prossime release

Ogni nuova release dovrebbe utilizzare le sezioni pertinenti tra:

```text
Added
Changed
Fixed
Deprecated
Removed
Security
Known issues
Planned
```

Ogni voce dovrebbe:

- descrivere una modifica verificabile;
- evitare di duplicare il README o la documentazione architetturale;
- indicare breaking change o migrazioni necessarie;
- distinguere implementato, corretto, deprecato e pianificato;
- evitare date o versioni non confermate;
- mantenere nomi di classi, metodi e file in formato codice.

---

**DOMOMANAGER CHANGELOG**  
**CHANGES. COMPATIBILITY. LIMITATIONS. MIGRATION.**
