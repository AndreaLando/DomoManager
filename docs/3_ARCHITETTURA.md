# DomoManager - Architettura tecnica dettagliata

## 1. Scopo

DomoManager è un runtime embedded per l'infrastruttura domestica. L'architettura separa:

- acquisizione fisica;
- stato applicativo condiviso;
- stato tecnico interno;
- propagazione degli eventi;
- logica applicativa;
- automazione;
- modellazione strutturale e semantica;
- integrazione e comunicazione esterna;
- diagnostica e protezione del runtime.

Il principio fondamentale è:

> **Ogni componente deve conoscere la propria responsabilità, le dipendenze ammesse, il momento di esecuzione, le risorse possedute e il modo in cui rende osservabile il proprio comportamento.**

Questa architettura non impone che ogni sottosistema utilizzi lo stesso meccanismo interno. Impone invece contratti coerenti, ownership esplicita e separazione delle responsabilità.

## 2. Modello architetturale generale

```text
MONDO FISICO
     |
     v
+----------------------------------+
|             BACKEND              |
| Device Manager / Modbus / I/O    |
| RS485 / driver / field protocols |
+----------------+-----------------+
                 |
                 v
+-------------------------------+     +-------------------------------+
|            BUFFER             |     |        RUNTIME STORE          |
| stato numerico condiviso      |     | stato tipizzato condiviso     |
| area -> long                  |     | handle -> typed value         |
+---------------+---------------+     +---------------+---------------+
                |                                     |
                +------------------+------------------+
                                   |
                                   v
+---------------------------------------------------------------------+
|                         EVENT / RUNTIME LAYER                       |
| EventManager / task pesati / scheduler / timing / watchdog         |
+----------------------+----------------------+-----------------------+
                       |                      |
                       v                      v
              +----------------+      +-------------------+
              | DOMAIN ENGINES |      | AUTOMATION ENGINE |
              +--------+-------+      +---------+---------+
                       |                        |
                       +------------+-----------+
                                    |
                                    v
                      BUFFER / RUNTIME STORE
                                    |
               +--------------------+--------------------+
               |                                         |
               v                                         v
      +-------------------+                     +---------------------+
      |  INTROSPECTION    |                     | ORCHESTRATORS       |
      | structural model  |                     | protocol lifecycle  |
      +---------+---------+                     +----------+----------+
                |                                          |
                v                                          v
      +-------------------+                      MQTT / WEB API / BRIDGE
      | SEMANTIC RESOLVER |
      +---------+---------+
                |
                v
        EXTERNAL PROJECTIONS
```

Buffer e RuntimeStore rappresentano lo stato applicativo condiviso con modelli differenti:

- il **Buffer** governa il dominio numerico delle aree;
- il **RuntimeStore** governa variabili tipizzate non riconducibili naturalmente a un'area numerica.

Gli engine mantengono inoltre stato tecnico interno quando necessario. Tale stato non è una seconda fonte arbitraria di verità applicativa: appartiene al componente che ne possiede lifecycle e diagnostica.

## 3. Principi architetturali

### 3.1 Determinismo

Il comportamento deve rimanere prevedibile attraverso:

- scheduling esplicito;
- intervalli e condizioni definiti;
- timeout e retry controllati;
- stati osservabili;
- lifecycle prevedibili;
- ownership delle risorse;
- assenza di dipendenze implicite da servizi esterni.

### 3.2 Non-blocking

Networking, MQTT, Web API, Bridge, RS485, Modbus e driver dedicati devono avanzare per passi controllati quando il protocollo lo consente.

> **Nessun servizio secondario deve bloccare indefinitamente il ciclo applicativo.**

Il requisito non-blocking non implica un unico scheduler universale. Ogni sottosistema può utilizzare macchine a stati, pacing, intervalli, code o orchestratori dedicati.

### 3.3 Separazione delle responsabilità

```text
Device Manager         -> dispositivi, canali e profili
Backend / Modbus       -> acquisizione e attuazione
Physical I/O Manager   -> I/O locali, polling e debounce
Buffer                  -> stato numerico condiviso
DMRuntimeStore          -> stato tipizzato condiviso
EventManager            -> propagazione differita degli eventi
DMRuntime               -> coordinamento del backend e dei task runtime
Automation Engine       -> comportamento dichiarativo
Domain Engines          -> Security, HVAC, Power, Solar, Weather, Averages
DomoIntrospection       -> modello strutturale
DomoSemanticResolver    -> modello semantico
DMHomeAssistant         -> protocollo e discovery Home Assistant
DMHAOrchestrator        -> lifecycle dell'integrazione Home Assistant
DMNetworkManager        -> coordinamento dei protocolli di rete
SocketManager           -> ownership e lifecycle dei socket
Diagnostics             -> osservabilità
Watchdog                -> rilevazione della mancata progressione
```

### 3.4 Ownership esplicita

Ogni risorsa e ogni fase di inizializzazione devono avere un owner unico.

Questo principio si applica a:

- pin e periferiche hardware;
- bus;
- socket;
- connessioni;
- code;
- timer;
- registrazione delle entity;
- topic e subscription;
- stato applicativo condiviso;
- stato tecnico interno.

L'inizializzazione hardware non deve essere duplicata tra entry point, runtime e driver. Il composition root configura il sistema; il componente proprietario inizializza e gestisce la risorsa.

### 3.5 Nessuna duplicazione della configurazione

L'Introspection conserva riferimenti alla configurazione e agli oggetti del dominio. I layer di comunicazione non devono introdurre mappe parallele di aree, bit, entity o segnali già descritti dal componente proprietario.

### 3.6 Simmetria read/write

Quando un dato è comandabile, codifica e decodifica devono condividere la stessa semantica.

Per bitmask, word di stato e comandi composti:

- la sequenza dei bit appartiene al dominio proprietario;
- il frontend usa il contratto del dominio;
- il protocollo esterno non mantiene una seconda tabella indipendente.

### 3.7 Stato applicativo e stato tecnico

Lo stato si divide in due categorie:

- **stato applicativo condiviso**, pubblicato tramite Buffer o RuntimeStore;
- **stato tecnico interno**, incapsulato nell'engine proprietario.

Esempi di stato tecnico interno:

- debounce;
- timeout;
- retry;
- code;
- richieste pendenti;
- cursori;
- connessioni;
- snapshot di protocollo;
- stato delle macchine a stati.

Lo stato tecnico deve essere esposto alla diagnostica quando influenza l'operatività.

## 4. Composition root e lifecycle di inizializzazione

La configurazione viene composta prima dell'avvio del runtime. Il composition root definisce:

- target hardware;
- rete;
- dispositivi;
- aree;
- I/O locali;
- automazioni;
- frontend;
- integrazioni;
- parametri diagnostici.

Il lifecycle logico è:

```text
BOOT
  |
  v
COMPOSITION ROOT
  | costruzione configurazione
  v
FRONTEND SETUP
  | rete e servizi frontend
  v
DOMOMANAGER SETUP
  | Buffer, device, runtime e backend
  v
RUNTIME REGISTRATION
  | event source, I/O, RuntimeStore, engine
  v
INTROSPECTION BUILD
  v
SEMANTIC RESOLUTION
  v
PROTOCOL / ORCHESTRATOR SETUP
  v
POWER-ON PROGRESSION
  v
NORMAL OPERATION
```

L'ordine concreto può includere fasi condizionali in base ai sottosistemi abilitati. Non deve essere interpretato come una pipeline rigida identica per ogni configurazione.

## 5. Backend, Device Manager e Physical I/O

### 5.1 Device Manager

Il Device Manager governa:

- dispositivi programmabili;
- profili;
- canali;
- associazione tra canali e aree;
- stato di errore;
- stato parked;
- diagnostica dei dispositivi.

### 5.2 Modbus

Il sottosistema Modbus comprende:

- Modbus TCP e RTU;
- selezione per IP;
- priorità;
- round-robin;
- pacing;
- ripresa delle letture interrotte;
- scritture pendenti;
- connessioni persistenti;
- timeout;
- cooldown;
- esclusione;
- integrazione con DMNetworkManager e SocketManager.

La connessione persistente possiede esplicitamente:

- IP corrente;
- stato di connessione;
- ultima attività;
- owner del socket;
- slot assegnato;
- condizioni di rilascio e riconnessione.

### 5.3 Physical I/O Manager

Il Physical I/O Manager gestisce:

- inizializzazione degli I/O locali;
- polling degli ingressi;
- debounce non bloccante;
- pubblicazione degli eventi;
- comando delle uscite tramite EventManager.

L'acquisizione degli ingressi non aggiorna direttamente in modo arbitrario tutti i consumer. Pubblica un evento che viene elaborato nel normale ciclo del runtime.

## 6. Buffer

Il Buffer conserva lo stato numerico condiviso:

- aree;
- valori `long`;
- timestamp;
- change tracking;
- aree virtuali;
- reverse;
- mapping di scrittura;
- frequenza di variazione;
- diagnostica delle inizializzazioni.

Route, split e toggle operano sul dominio delle aree, ma appartengono ai rispettivi manager logici.

Gli engine applicativi non devono aggirare il Buffer quando il dato appartiene al dominio numerico condiviso.

Il Buffer non deve essere presentato come deposito di tutto lo stato del sistema. Connessioni, retry, code e macchine a stati restano nei componenti proprietari.

## 7. DMRuntimeStore

DMRuntimeStore gestisce variabili condivise non riconducibili naturalmente a `area -> long`.

Supporta:

- handle O(1);
- ID testuali usati durante setup e discovery;
- `BOOL`;
- `INT32`;
- `FLOAT`;
- `STRING32` e `STRING64`;
- `JSON128` e `JSON256`;
- accesso `READ_ONLY`, `WRITE_ONLY` e `READ_WRITE`;
- pubblicazione opzionale;
- persistenza dichiarata;
- flag sensitive;
- metadati semantici;
- minimo, massimo, step e precisione;
- sorgente dell'ultima scrittura;
- revisioni e dirty tracking;
- controllo dell'integrità.

Lo storage è statico e usa un pool testuale condiviso. Gli handle devono essere conservati nel normale percorso runtime, evitando ricerche ripetute per ID.

Le entity Runtime vengono indicizzate dall'Introspection e possono essere proiettate sui topic:

```text
<prefix>/runtime/<id>/state
<prefix>/runtime/<id>/set
```

## 8. EventManager e propagazione

EventManager disaccoppia la produzione degli eventi dal loro consumo.

Le sorgenti comprendono, tra le altre:

- Modbus;
- I/O fisici;
- route;
- toggle;
- split;
- automazioni;
- eventi interni;
- comandi provenienti dai frontend.

Il modello è:

```text
PRODUCER
   |
   v
EVENT QUEUE
   |
   v
BOUNDED PROCESSING PER LOOP
   |
   +--> Buffer consumers
   +--> Physical outputs
   +--> Route / Toggle / Split
   +--> Frontend publication
```

Le callback non devono costruire catene sincrone ricorsive non governate. Il numero di eventi processati per ciclo può essere limitato per preservare la progressione del runtime.

## 9. DMRuntime e scheduling

DMRuntime coordina:

- EventManager;
- Physical I/O;
- Modbus;
- logica route, toggle, split e threshold;
- Automation Engine;
- scheduler asincrono;
- medie;
- watchdog;
- diagnostica delle aree;
- callback applicative;
- completamento del ciclo backend.

Il runtime non usa un solo modello temporale. Combina:

- task pesati;
- contatori di rotazione;
- intervalli interni;
- prerequisiti di ciclo;
- callback;
- pacing di rete.

Ogni sottosistema deve dichiarare il proprio criterio di avanzamento.

## 10. Automation Engine

L'Automation Engine supporta:

- scene;
- regole a soglia;
- trend;
- bitmask;
- condizioni multiple;
- regole temporali;
- sequenze;
- regole composite;
- configurazione JSON;
- validazione di aree e parametri.

Il flusso logico è:

```text
BUFFER / TIME / AVERAGES
          |
          v
      CONDITIONS
          |
          v
       DECISION
          |
          v
        ACTION
          |
          v
BUFFER EVENT
```

L'Automation Engine non deve conoscere il protocollo concreto dell'attuatore.

## 11. Domain Engines

### 11.1 Security e Intrusion

Il dominio comprende:

```text
Security orchestration
  +-- Wired sensors
  +-- zones and channel aggregation
  +-- alarm panel abstraction
  +-- command and status areas
  +-- event and diagnostic reporting
```

Gestisce:

- canali RT, H24, MASK e LEN;
- zone;
- startup inhibit;
- armamento;
- allarmi;
- tamper e trouble;
- bypass;
- memoria e tacitazione;
- fault di comunicazione;
- partizioni;
- eventi e audit trail;
- word e bitmask di stato.

Il percorso read/write deve usare codec e helper condivisi del dominio Security. Home Assistant e gli altri frontend non devono mantenere una seconda mappa dei bit.

### 11.2 HVAC

Il dominio HVAC gestisce:

- zone;
- setpoint;
- temperature;
- fan coil;
- pompe di calore;
- circolazione;
- ACS;
- valvola a tre vie;
- sbrinamento;
- anti-legionella;
- protezioni legate alle aperture.

### 11.3 Averages

Ogni gruppo:

1. legge i sensori secondo la scala configurata;
2. calcola il valore aggregato;
3. scrive nell'area di uscita secondo `outScale`.

La proiezione semantica deve rispettare la relazione:

```text
valore pubblicato = valore Buffer * Entity::scale
Entity::scale     = 1 / outScale
```

Le divisioni devono essere validate contro valori di scala nulli o non validi.

### 11.4 Power e Power Supervisor

Il dominio Power comprende le funzionalità effettivamente configurate e implementate, tra cui:

- carichi;
- limiti;
- priorità;
- suggerimenti di attacco o distacco;
- supervisione della tensione;
- soglie con isteresi;
- stato 24 V;
- fault;
- funzionamento a batteria;
- segnali extra;
- allarmi e diagnostica.

Funzioni future o sperimentali non devono essere presentate come capacità operative senza un'implementazione verificata.

### 11.5 Solar

Il sottosistema Solar comprende:

- configurazione di più inverter;
- trasporto UART o TCP;
- discovery asincrona dei comandi;
- polling con pacing;
- snapshot strutturato;
- CRC;
- diagnostica;
- associazione alle aree Introspection;
- gruppo semantico `SOLAR`.

### 11.6 Weather

Il dominio Weather gestisce:

- temperatura;
- pioggia;
- vento;
- luminosità;
- medie mobili;
- transizioni giorno/notte;
- eventi;
- allarmi con debounce.

I getter e le medie devono gestire esplicitamente lo stato precedente al primo campione valido.

### 11.7 AEE e Bridge

AEE gestisce:

- definizione delle variabili;
- registry;
- direzioni di sincronizzazione;
- applicazione degli aggiornamenti;
- serializzazione dello schema.

Il Bridge gestisce:

- peer monitor;
- heartbeat;
- ACK;
- ritrasmissione;
- allineamento;
- snapshot iniziale;
- trasferimento multiframe dello schema;
- pacing di rete.

### 11.8 Access Control e Web API

Il controllo accessi gestisce:

- lettori;
- normalizzazione dei codici;
- autorizzazione;
- relay;
- callback;
- variabili RuntimeStore associate.

Web API gestisce:

- client e server HTTP;
- dispositivi multipli;
- richieste accodate;
- correlazione delle risposte;
- timeout;
- autenticazione;
- parsing key/value e JSON;
- body chunked;
- webhook.

## 12. DomoIntrospection

DomoIntrospection costruisce una vista strutturale del sistema senza duplicare la configurazione proprietaria.

### 12.1 Context

Il Context aggrega riferimenti ai componenti necessari, tra cui:

- configurazione;
- DeviceProfiles;
- Automation Engine;
- AEERegistry;
- Buffer;
- RuntimeStore;
- AverageCalculator;
- TimeManager;
- MQTT;
- Web API;
- HVAC;
- Averages;
- Solar.

### 12.2 Entity

Ogni Entity può descrivere:

- area;
- kind;
- origin;
- group;
- I/O type;
- riferimenti alla configurazione e al device;
- indice di device e canale;
- runtime handle ed entry;
- metadati semantici;
- accesso read/write;
- visibilità;
- relazioni strutturali;
- flag di provenienza;
- scale;
- configurazione Number e Climate.

### 12.3 EntityOrigin

Le origini disponibili includono:

```text
AREA
DERIVED
SECURITY
HVAC
AVERAGE
SOLAR
POWER
AEE
RUNTIME
EXTERNAL_SOURCE
```

### 12.4 EntityGroup

```text
ROOT             -> controller DomoManager
PHYSICAL_DEVICE  -> dispositivo fisico
SECURITY         -> sistema di sicurezza
SOLAR            -> inverter solare
```

### 12.5 Relation

Le relazioni descrivono collegamenti come:

- route;
- toggle;
- split;
- input e output di automazione;
- binding AEE;
- MQTT;
- Web API.

### 12.6 Finalizzazione e visibilità

`finalize()` identifica entity tecniche, incomplete o prive di una configurazione pubblicabile.

`implementationOnly` impedisce la pubblicazione esterna, mantenendo l'entity disponibile nel modello diagnostico.

## 13. DomoSemanticResolver

DomoSemanticResolver trasforma il modello strutturale in un modello semantico pubblicabile.

Determina:

- `EntityKind`;
- unità;
- device class;
- state class;
- accesso;
- gruppo;
- visibilità finale.

La risoluzione avviene in più passaggi:

1. semantica individuale;
2. semantica derivata da route, split e toggle;
3. calcolo del risultato e delle statistiche.

L'ordine concettuale di precedenza è:

- semantica specialistica della sorgente;
- struttura fisica e relazioni;
- dizionario lessicale della label;
- fallback `UNKNOWN`.

Il resolver non deve contenere ID `AREA_*` specifici dell'installazione.

## 14. FrontendStatus

FrontendStatus rappresenta lo stato tecnico comune dei frontend tramite una word a 16 bit.

I bit comuni definiti sono:

```text
bit 0  Enabled
bit 1  Running
bit 2  Initialized
bit 3  Connected
bit 4  Active
bit 5  Recovering
bit 6  Degraded
bit 7  Fault
bit 8..15 Reserved
```

La classe supporta:

- sanitizzazione dei bit riservati;
- aggiornamenti singoli o mascherati;
- stato operativo;
- stato di salute;
- change detection;
- consumo della word modificata.

`Connected` non è requisito universale di `IsOperational()`, perché alcuni frontend possono essere operativi senza un client connesso.

## 15. Home Assistant

### 15.1 DMHomeAssistant

Responsabilità:

- generazione MQTT Discovery;
- topic delle aree e del RuntimeStore;
- payload e metadati;
- Device Registry;
- availability;
- pubblicazione degli stati.

### 15.2 DMHAOrchestrator

Responsabilità:

- validazione del trasporto;
- subscription dei command topic;
- rilevazione della connessione;
- recovery dopo riconnessione;
- availability;
- discovery;
- snapshot iniziale;
- coda `PendingState`;
- routing dei comandi verso Buffer o RuntimeStore;
- feedback sui topic di stato.

### 15.3 HAMQTTTransport

HAMQTTTransport adatta l'orchestratore al client MQTT effettivo tramite callback per:

- stato di connessione;
- publish;
- subscribe.

Il layer Home Assistant non assume direttamente la proprietà del client MQTT.

### 15.4 Topic

```text
<prefix>/status
<prefix>/area/<area>/state
<prefix>/area/<area>/set
<prefix>/runtime/<id>/state
<prefix>/runtime/<id>/set
```

### 15.5 Sequenza di riconnessione

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
PUBLISH INCREMENTAL STATES
```

Le flag di subscription, availability, discovery e snapshot vengono reinizializzate quando la connessione viene persa.

### 15.6 Device Registry

Gli identificatori stabili aggregano le entity sotto:

- controller DomoManager;
- dispositivi fisici;
- sistema Security;
- inverter Solar.

I device secondari possono usare `via_device` verso il controller root.

## 16. MQTT

Il layer MQTT definisce un messaggio inbound normalizzato utilizzabile da mapping, backend e Home Assistant.

Le origini includono:

- raw;
- mapping numerico;
- mapping testuale;
- comando Home Assistant per area;
- comando Home Assistant per RuntimeStore.

Il messaggio trasporta:

- client e backend di origine;
- topic e payload;
- device e mapping;
- valore normalizzato;
- area;
- runtime handle e ID;
- tipo e sensibilità del dato.

Le callback sono sincrone rispetto alla ricezione MQTT. I consumer che devono conservare topic o payload devono copiarli.

## 17. Communication Runtime

La comunicazione non attraversa un singolo scheduler universale. Il modello effettivo è composto da:

```text
DMNetworkManager
  +-- protocol registration
  +-- pacing and arbitration
  +-- protocol state
  +-- SocketManager

Protocol engines
  +-- Modbus
  +-- MQTT
  +-- Web API
  +-- Bridge
  +-- Solar TCP

Frontend orchestrators
  +-- Home Assistant
  +-- Security
  +-- Power
  +-- Weather
  +-- Access Control
```

Ogni protocollo deve dichiarare:

- owner;
- intervallo o criterio di avanzamento;
- priorità o pacing;
- timeout;
- retry;
- risorse di rete;
- condizioni di rilascio;
- diagnostica.

## 18. DMNetworkManager e SocketManager

DMNetworkManager coordina i protocolli che condividono la rete.

Le responsabilità comprendono:

- registrazione dei protocolli;
- arbitraggio;
- `tryAcquire()` e `release()`;
- stato del protocollo;
- pacing;
- integrazione con SocketManager.

SocketManager assegna slot a owner espliciti e distingue i tipi di socket. Il componente che acquisisce uno slot deve conservarne l'identità e rilasciarlo quando:

- termina l'operazione;
- cambia owner o contesto;
- cade la connessione;
- cambia endpoint;
- viene superato il timeout previsto.

## 19. Diagnostica

La diagnostica deve osservare:

- Buffer e RuntimeStore;
- device e profili;
- aree mai inizializzate o inizializzate più volte;
- relazioni Introspection;
- entity `UNKNOWN`, hidden e implementation-only;
- origin, group e scaling;
- EventManager;
- automazioni e scheduler;
- Modbus e timing;
- Ethernet, protocolli e socket;
- FrontendStatus;
- Security, HVAC, Solar, Power e Weather;
- watchdog;
- Hot Standby.

La diagnostica deve distinguere almeno:

```text
CONFIGURATION ERROR
DEVICE ERROR
PROTOCOL ERROR
TIMEOUT / RETRY
DEGRADED FRONTEND
RESOURCE EXHAUSTION
SLOW TASK
MISSING PROGRESSION
STATE INCONSISTENCY
```

## 20. Watchdog

Il watchdog analizza condizioni come:

- blocked;
- overload;
- unstable;
- inactive;
- spike dei tempi di esecuzione.

Il runtime misura callback e cicli rilevanti, aggiorna tempi medi e massimi e può reagire alle condizioni gravi.

Il watchdog non sostituisce il controllo degli errori dei protocolli. Protegge la progressione complessiva del runtime.

## 21. Hot Standby

Il supporto Hot Standby può includere:

- heartbeat;
- rilevazione del peer;
- cambio di ruolo;
- failover;
- failback;
- replica dello stato;
- replica dei dati di processo;
- sincronizzazione temporale.

L'implementazione è dipendente dalla piattaforma e dal trasporto. La disponibilità del supporto non implica che sia abilitato in ogni build o configurazione.

## 22. Regole per nuovi moduli

Ogni nuovo modulo deve dichiarare:

- responsabilità;
- owner;
- stato applicativo letto o prodotto;
- stato tecnico interno;
- uso del Buffer o del RuntimeStore;
- eventi prodotti e consumati;
- criterio di scheduling o avanzamento;
- comportamento non-blocking;
- timeout, retry e fallback;
- risorse consumate;
- dipendenze ammesse;
- lifecycle;
- diagnostica;
- eventuale modello Introspection e semantico;
- eventuali topic o endpoint pubblici.

### Checklist di integrazione

Prima dell'integrazione, verificare:

- [ ] ownership unica dell'inizializzazione;
- [ ] assenza di configurazioni duplicate;
- [ ] distinzione tra stato condiviso e stato tecnico;
- [ ] validazione degli indici e dei limiti;
- [ ] gestione del primo campione e dello stato non inizializzato;
- [ ] assenza di divisioni per zero;
- [ ] rilascio delle risorse;
- [ ] recovery dopo disconnessione;
- [ ] pubblicazione dello snapshot iniziale, se necessaria;
- [ ] change tracking coerente;
- [ ] diagnostica sufficiente;
- [ ] compatibilità con Opta ed ESP32, quando richiesta.

## 23. Sintesi

DomoManager separa acquisizione, stato, eventi, logica, semantica e comunicazione.

- il Buffer governa lo stato numerico condiviso;
- RuntimeStore governa lo stato tipizzato condiviso;
- EventManager governa la propagazione differita;
- DMRuntime coordina il backend e i task;
- gli engine possiedono lo stato tecnico dei rispettivi domini;
- Introspection descrive la struttura;
- Semantic Resolver produce la semantica;
- gli orchestratori governano i lifecycle dei frontend;
- DMNetworkManager e SocketManager governano le risorse di rete;
- la diagnostica rende osservabile l'intero sistema.

> **L'architettura non elimina la complessità: le assegna confini, owner, contratti e strumenti di osservazione.**

---

**DOMOMANAGER**  
**ARCHITETTURA TECNICA DETTAGLIATA**  
**DETERMINISTIC. NON-BLOCKING. OWNERSHIP-DRIVEN.**
