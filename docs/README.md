# DomoManager

**Deterministic. Non-blocking. Local-first. Embedded-first.**

DomoManager è un runtime embedded per l'infrastruttura domestica. Coordina dispositivi, stato applicativo, automazioni, sicurezza, climatizzazione, energia e integrazioni esterne mantenendo responsabilità, lifecycle e risorse espliciti.

Non nasce come alternativa grafica a Home Assistant, né come semplice gateway tra protocolli. Il suo obiettivo è governare l'impianto locale con criteri tipici dei sistemi industriali:

- comportamento prevedibile;
- funzionamento locale;
- comunicazioni non bloccanti;
- stato condiviso coerente;
- ownership delle risorse;
- diagnostica integrata;
- recovery controllato;
- supporto multipiattaforma embedded.

> **DomoManager porta nella casa la disciplina dei sistemi industriali senza rinunciare alla flessibilità delle integrazioni moderne.**

## Indice

- [Visione](#visione)
- [Principi architetturali](#principi-architetturali)
- [Architettura generale](#architettura-generale)
- [Stato applicativo](#stato-applicativo)
- [Runtime ed eventi](#runtime-ed-eventi)
- [Backend e protocolli](#backend-e-protocolli)
- [Domain engine](#domain-engine)
- [Introspection e semantica](#introspection-e-semantica)
- [Home Assistant](#home-assistant)
- [Networking e risorse](#networking-e-risorse)
- [Diagnostica e watchdog](#diagnostica-e-watchdog)
- [Piattaforme](#piattaforme)
- [Struttura della documentazione](#struttura-della-documentazione)
- [Regole per contribuire](#regole-per-contribuire)
- [Stato del progetto](#stato-del-progetto)

## Visione

La domotica non deve essere un insieme di dispositivi indipendenti collegati a servizi esterni. Deve comportarsi come un sistema coerente capace di:

1. acquisire il mondo fisico;
2. validare e rappresentare lo stato;
3. elaborare condizioni;
4. prendere decisioni;
5. comandare dispositivi e attuatori;
6. esporre il proprio modello a sistemi esterni;
7. spiegare il proprio comportamento attraverso diagnostica e log.

DomoManager privilegia:

- cablaggio, Ethernet, RS485 e Modbus per le funzioni critiche;
- radio e cloud quando aggiungono valore, non come requisito operativo;
- logica locale;
- configurazione dichiarativa;
- protocolli e frontend separati dalla logica applicativa.

Home Assistant è una proiezione e un livello di supervisione. DomoManager mantiene il controllo locale dello stato, delle automazioni e dell'impianto.

## Principi architetturali

### Determinismo

Ogni componente deve dichiarare:

- quando è eleggibile;
- quale lavoro esegue;
- quali risorse possiede;
- quali timeout e retry applica;
- come segnala il proprio stato;
- come recupera dopo un errore.

### Non-blocking

Le comunicazioni devono avanzare per passi controllati. MQTT, Web API, Bridge, Modbus, RS485 e trasporti dedicati non devono monopolizzare il ciclo applicativo.

### Local-first

Le funzioni essenziali devono continuare a operare anche in assenza di Internet, cloud o integrazioni esterne.

### Embedded-first

CPU, RAM, socket, bus, connessioni e tempo di ciclo sono risorse architetturali. Ogni risorsa limitata deve avere:

- owner;
- lifecycle;
- limiti;
- diagnostica;
- politica di rilascio e recovery.

### Ownership esplicita

Configurazione e inizializzazione non devono essere duplicate. Il composition root definisce il sistema; il componente proprietario inizializza e gestisce la risorsa.

### Nessuna duplicazione semantica

Bitmask, mapping, unità, scaling e regole read/write appartengono al dominio proprietario. I frontend ne consumano il modello senza mantenere tabelle parallele.

## Architettura generale

```text
MONDO FISICO
     |
     v
BACKEND / DEVICE / PHYSICAL I/O
     |
     +---------------------+
     |                     |
     v                     v
  BUFFER            DMRUNTIMESTORE
 area -> long       handle -> typed value
     |                     |
     +----------+----------+
                |
                v
       EVENTMANAGER / DMRUNTIME
                |
      +---------+----------+
      |                    |
      v                    v
DOMAIN ENGINES       AUTOMATION ENGINE
      |                    |
      +---------+----------+
                |
                v
       SHARED STATE / COMMANDS
                |
      +---------+----------+
      |                    |
      v                    v
INTROSPECTION       PROTOCOL ORCHESTRATORS
      |                    |
      v                    v
SEMANTIC RESOLVER   MQTT / WEB API / BRIDGE
      |                    |
      v                    v
FRONTEND MODELS     DMNETWORKMANAGER
                           |
                           v
                     SOCKETMANAGER
```

L'architettura distingue sei responsabilità:

1. **acquisizione e attuazione**;
2. **stato applicativo condiviso**;
3. **eventi e coordinamento runtime**;
4. **logica dei domini e automazioni**;
5. **modello strutturale e semantico**;
6. **frontend, protocolli e risorse di rete**.

## Stato applicativo

### Buffer

Il Buffer è la fonte autorevole dello stato numerico condiviso delle aree.

Gestisce:

- valori numerici;
- timestamp;
- change tracking;
- aree virtuali;
- mapping di scrittura;
- informazioni diagnostiche;
- changed map.

Route, toggle e split lavorano sul dominio delle aree, ma appartengono ai rispettivi manager logici.

### DMRuntimeStore

`DMRuntimeStore` completa il Buffer per dati non rappresentabili naturalmente come `area -> long`.

Supporta:

- `BOOL`;
- `INT32`;
- `FLOAT`;
- `STRING32` e `STRING64`;
- `JSON128` e `JSON256`;
- accesso `READ_ONLY`, `WRITE_ONLY` e `READ_WRITE`;
- metadati semantici;
- persistenza dichiarata;
- flag sensitive;
- revisioni e dirty tracking;
- controllo dell'integrità;
- accesso O(1) tramite handle.

Il percorso runtime usa gli handle. Gli ID testuali sono riservati principalmente a setup, introspezione e parsing dei comandi esterni.

### Stato tecnico interno

Timer, debounce, code, richieste pendenti, connessioni, retry, cooldown e macchine a stati rimangono nel componente proprietario. Quando incidono sull'operatività devono essere osservabili dalla diagnostica.

## Runtime ed eventi

### EventManager

`EventManager` separa la produzione degli eventi dal loro consumo.

Le sorgenti comprendono:

- Modbus;
- I/O fisici;
- route;
- toggle;
- split;
- automazioni;
- comandi frontend;
- servizi interni.

Gli eventi vengono processati con un budget limitato per iterazione, evitando catene ricorsive e monopolizzazione del loop.

### DMRuntime

`DMRuntime` coordina:

- `PhysicalIOManager`;
- ciclo Modbus;
- `EventManager`;
- route, toggle, split e threshold;
- `AutomationEngine`;
- scheduler asincrono;
- medie;
- callback applicative;
- watchdog;
- diagnostica;
- completamento dei cicli backend.

Il runtime combina:

- task pesati;
- contatori di rotazione;
- intervalli temporali;
- prerequisiti di ciclo;
- callback;
- pacing di rete;
- macchine a stati.

Non esiste un unico Task Engine uniforme che esegue integralmente tutti i sottosistemi a ogni iterazione.

## Backend e protocolli

### Device Manager

Il Device Manager governa:

- dispositivi programmabili;
- profili hardware;
- canali;
- associazioni tra device e aree;
- errori visibili;
- stato parked;
- diagnostica dei dispositivi.

### Modbus

Il motore Modbus supporta:

- Modbus TCP e RTU;
- selezione degli IP;
- round-robin;
- scritture pendenti;
- pacing;
- timeout;
- retry;
- cooldown ed esclusione;
- connessioni persistenti;
- integrazione con il runtime di rete.

Le scritture possono ricevere priorità controllata senza aggirare IP manager, arbitraggio o lifecycle delle risorse.

### Physical I/O

`PhysicalIOManager` gestisce:

- inizializzazione degli I/O locali;
- polling;
- conversione del livello attivo;
- debounce non bloccante;
- pubblicazione degli eventi;
- comando delle uscite.

L'inizializzazione hardware deve avere un unico owner.

### Web API

Il motore Web API fornisce:

- client e server HTTP;
- richieste accodate;
- correlazione delle risposte;
- timeout;
- autenticazione;
- parsing key/value e JSON;
- gestione dei body chunked;
- profili applicativi e webhook.

### Bridge e AEE

AEE descrive variabili sincronizzabili, direzioni e schema. Il Bridge gestisce heartbeat, ACK, ritrasmissione, snapshot iniziale, allineamento e trasferimento multiframe.

### Solar

Il sottosistema Solar comprende:

- trasporti UART e TCP;
- discovery asincrona dei comandi;
- polling con pacing;
- parsing e CRC;
- snapshot strutturato;
- aggiornamento delle aree;
- diagnostica.

## Domain engine

### Security

Il dominio Security comprende:

- sensori cablati;
- canali RT, H24, MASK e LEN;
- zone;
- startup inhibit;
- armamento;
- allarmi;
- tamper e trouble;
- bypass;
- memoria e tacitazione;
- partizioni;
- word e bitmask di stato;
- comandi e diagnostica.

Lettura e scrittura condividono lo stesso codec e lo stesso ordine canonico dei bit.

### HVAC

Il dominio HVAC comprende zone, setpoint, pompe di calore, fan coil, circolazione, ACS, sbrinamento, valvola a tre vie, anti-legionella e protezioni legate alle aperture.

### Power

Il dominio Power comprende gestione dei carichi e supervisione elettrica:

- limiti e priorità;
- suggerimenti di attacco o distacco;
- tensione principale;
- soglie con isteresi;
- stato 24 V;
- fault;
- funzionamento a batteria;
- segnali extra;
- allarmi e snapshot diagnostici.

### Weather

Il dominio Weather gestisce temperatura, vento, pioggia, luminosità, medie mobili, transizioni giorno/notte, raffiche, eventi e allarmi con debounce.

Il primo campione e lo stato non inizializzato devono essere gestiti senza divisioni per zero.

### Averages

I gruppi di media applicano scala di ingresso, aggregazione e scala di uscita. La pubblicazione semantica segue:

```text
published value = Buffer value * Entity::scale
Entity::scale   = 1 / outScale
```

Scale e denominatori devono essere validati.

### Access Control

Il controllo accessi coordina lettori, normalizzazione dei codici, autorizzazione, relay, callback e variabili runtime correlate.

## Automation Engine

L'Automation Engine opera sul modello applicativo e supporta:

- scene;
- regole a soglia;
- trend;
- bitmask;
- condizioni temporali;
- condizioni multiple;
- regole composite;
- sequenze;
- configurazione JSON;
- validazione.

Le azioni vengono trasformate in aggiornamenti di stato ed eventi. L'engine non deve conoscere il protocollo concreto dell'attuatore.

## Introspection e semantica

### DomoIntrospection

`DomoIntrospection` costruisce una vista strutturale del sistema conservando riferimenti agli oggetti originali.

Il modello descrive:

- entity;
- relazioni;
- origine;
- gruppo;
- tipo fisico;
- riferimenti a device e configurazione;
- runtime handle;
- accesso;
- visibilità;
- scaling;
- provenienza.

Le entity tecniche possono essere marcate `implementationOnly`: restano diagnosticabili ma non vengono pubblicate.

### DomoSemanticResolver

`DomoSemanticResolver` arricchisce il modello strutturale determinando:

- `EntityKind`;
- unità;
- device class;
- state class;
- accesso;
- gruppo;
- visibilità finale.

La risoluzione applica semantica specialistica, struttura e relazioni, dizionario lessicale e fallback `UNKNOWN`.

I moduli generici non devono contenere identificatori `AREA_*` specifici dell'installazione.

## Home Assistant

L'integrazione è divisa in tre responsabilità.

### DMHomeAssistant

Genera:

- MQTT Discovery;
- topic;
- payload;
- availability;
- Device Registry;
- proiezione semantica delle entity.

### DMHAOrchestrator

Gestisce:

- connection lifecycle;
- subscription dei command topic;
- availability;
- discovery;
- snapshot iniziale;
- `PendingState`;
- aggiornamenti incrementali;
- routing dei comandi;
- recovery dopo riconnessione.

### HAMQTTTransport

Adatta l'orchestratore al client MQTT tramite callback di connessione, publish e subscribe. Non trasferisce al layer Home Assistant la proprietà del client.

### Topic

```text
<prefix>/status
<prefix>/area/<area>/state
<prefix>/area/<area>/set
<prefix>/runtime/<id>/state
<prefix>/runtime/<id>/set
```

### Sequenza di connessione

```text
CONNECTED
   |
   v
SUBSCRIBE COMMAND TOPICS
   |
   v
PUBLISH AVAILABILITY
   |
   v
PUBLISH DISCOVERY
   |
   v
PUBLISH INITIAL SNAPSHOT
   |
   v
PUBLISH INCREMENTAL CHANGES
```

In caso di disconnessione, subscription, availability, discovery e snapshot devono poter ripartire. Un errore iniziale dello snapshot non deve impedire il recovery successivo.

### Comandi

I comandi area attraversano validazione dell'entity, parsing, callback e owner del dominio. I comandi RuntimeStore verificano ID, handle, tipo e accesso.

Un comando esterno non raggiunge direttamente un attuatore e non aggira il modello interno.

## Networking e risorse

DomoManager non utilizza un Communication Scheduler universale per tutti i protocolli.

```text
PROTOCOL ENGINES
  +-- Modbus
  +-- MQTT
  +-- Web API
  +-- Bridge
  +-- Solar TCP
          |
          v
DMNETWORKMANAGER
  +-- protocol registration
  +-- arbitration
  +-- pacing
  +-- protocol state
          |
          v
SOCKETMANAGER
  +-- owner
  +-- kind
  +-- exact slot
  +-- lifecycle
          |
          v
PLATFORM NETWORK
```

Ogni protocollo deve dichiarare:

- owner;
- criterio di avanzamento;
- priorità o pacing;
- timeout;
- retry;
- risorse richieste;
- condizioni di rilascio;
- diagnostica.

## FrontendStatus

`FrontendStatus` rappresenta lo stato tecnico comune dei frontend tramite una word a 16 bit:

```text
bit 0   Enabled
bit 1   Running
bit 2   Initialized
bit 3   Connected
bit 4   Active
bit 5   Recovering
bit 6   Degraded
bit 7   Fault
bit 8-15 Reserved
```

`Connected` non è un requisito universale di operatività, perché alcuni frontend possono essere operativi senza un client connesso.

## Diagnostica e watchdog

La diagnostica osserva:

- Buffer e RuntimeStore;
- device e profili;
- aree mai inizializzate o inizializzate più volte;
- EventManager;
- automazioni e scheduler;
- timing e spike;
- Modbus e RS485;
- Ethernet, protocolli e socket;
- MQTT e recovery;
- Introspection e Semantic Resolver;
- entity `UNKNOWN`, hidden e implementation-only;
- Security, HVAC, Power, Weather e Solar;
- FrontendStatus;
- Hot Standby.

Il watchdog protegge la progressione complessiva del runtime. Non sostituisce la gestione degli errori dei protocolli.

## Hot Standby

Il supporto Hot Standby può comprendere:

- heartbeat;
- rilevazione del peer;
- ruolo master/slave;
- failover e failback;
- replica dello stato;
- replica dei dati di processo;
- sincronizzazione temporale.

La disponibilità del supporto non implica che sia abilitato in ogni build o configurazione.

## Piattaforme

Il backend di piattaforma astrae:

- Arduino Opta;
- ESP32-S3;
- GPIO;
- LED diagnostici;
- RTC;
- Ethernet;
- W5500;
- expander I/O;
- client e server TCP;
- API Modbus comuni.

Le differenze di piattaforma devono rimanere sotto `DMPlatform`, `DMPlatformNetwork` e `DMPlatformModbus`, evitando condizioni hardware disperse nei domain engine.

## Struttura della documentazione

La documentazione tecnica è organizzata in documenti progressivi:

1. `1_FILOSOFIA.md`  
   Principi, obiettivi e criteri di progettazione.

2. `2_PANORAMICA.md`  
   Vista generale di componenti, domini e integrazioni.

3. `3_ARCHITETTURA.md`  
   Architettura tecnica dettagliata, lifecycle e ownership.

4. `4_DIAGRAMMA_DIPENDENZE_FILES.md`  
   Dipendenze fisiche, architetturali e runtime.

5. `5_DIAGRAMMA_ARCHITETTURA.md`  
   Diagrammi a blocchi dell'architettura.

6. `6_DIAGRAMMA_FLUSSO_DATI.md`  
   Percorsi dei dati, eventi, comandi e pubblicazione.

7. `7_DIAGRAMMA_CICLO_EXEC.md`  
   Ciclo incrementale, task pesati, protocolli e recovery.

Il README è il punto di ingresso. I documenti successivi aumentano progressivamente il dettaglio.

## Regole per contribuire

Ogni nuovo modulo deve dichiarare:

- responsabilità;
- owner;
- dipendenze;
- stato applicativo letto o prodotto;
- stato tecnico interno;
- eventi prodotti e consumati;
- criterio di scheduling;
- timeout e retry;
- risorse utilizzate;
- lifecycle;
- recovery;
- diagnostica;
- eventuale modello Introspection e semantico.

### Checklist

- [ ] ownership unica dell'inizializzazione;
- [ ] nessuna configurazione duplicata;
- [ ] stato condiviso distinto dallo stato tecnico;
- [ ] Buffer o RuntimeStore scelto correttamente;
- [ ] lavoro limitato per iterazione;
- [ ] validazione di indici, tipi e scale;
- [ ] gestione dello stato non inizializzato;
- [ ] nessuna divisione per zero;
- [ ] codec read/write condiviso;
- [ ] socket e connessioni rilasciati correttamente;
- [ ] recovery dopo disconnessione;
- [ ] snapshot iniziale quando necessario;
- [ ] flag di completamento aggiornati dopo progressione reale;
- [ ] diagnostica sufficiente;
- [ ] nessun `AREA_*` installazione-specifico nei moduli generici;
- [ ] compatibilità con le piattaforme dichiarate.

## Stato del progetto

DomoManager è un progetto in evoluzione. Architettura, firmware e documentazione devono avanzare insieme.

Prima di considerare una funzione stabile è necessario verificare:

- comportamento sulla piattaforma target;
- recovery dopo errori e disconnessioni;
- uso delle risorse;
- diagnostica;
- coerenza con Buffer, RuntimeStore ed EventManager;
- integrazione con Introspection e frontend;
- assenza di regressioni nei cicli esistenti.

## Obiettivo finale

DomoManager vuole offrire una base domotica:

- affidabile come un sistema industriale;
- leggibile come un framework moderno;
- locale;
- prevedibile sotto carico;
- estendibile;
- diagnosticabile;
- consapevole delle risorse embedded.

> **La complessità non deve essere scaricata sull'utente. Deve essere governata dall'architettura.**

---

**DOMOMANAGER**  
**THE OPERATING SYSTEM FOR THE HOME**  
**STATE. EVENTS. OWNERSHIP. PROGRESSION.**
