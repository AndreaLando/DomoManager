# DomoManager - Flusso dei dati

## 1. Scopo

Questo documento descrive il percorso dei dati attraverso DomoManager, dalla sorgente fisica fino agli engine applicativi, alle automazioni, agli attuatori e alle proiezioni esterne.

Le viste distinguono:

- acquisizione fisica;
- stato applicativo condiviso;
- eventi e change tracking;
- elaborazione applicativa;
- automazioni;
- attuazione;
- costruzione del modello strutturale e semantico;
- pubblicazione esterna;
- comandi inbound;
- lifecycle delle comunicazioni.

> **Il flusso dei dati non è una singola pipeline lineare: Buffer, RuntimeStore, EventManager, domain engine e orchestratori cooperano mantenendo ownership distinti.**

## 2. Tipi di stato

DomoManager utilizza due storage applicativi complementari:

```text
BUFFER
  area numerica -> long
  timestamp, change tracking, virtual areas

DMRUNTIMESTORE
  handle -> valore tipizzato
  BOOL, INT32, FLOAT, STRING, JSON
  metadata, access control, revision, dirty tracking
```

Gli engine possono inoltre mantenere stato tecnico interno, per esempio:

- debounce;
- timeout;
- retry;
- connessioni;
- code;
- richieste pendenti;
- cursori di polling;
- snapshot di protocollo;
- macchine a stati.

Lo stato tecnico non sostituisce lo stato applicativo condiviso e rimane sotto l'ownership del componente che lo utilizza.

## 3. Acquisizione fisica

```text
SENSORI / DISPOSITIVI
         |
         v
+-----------------------------+
| DRIVER E BACKEND            |
| Modbus, RS485, Local I/O    |
| Web API, Solar transport    |
+-------------+---------------+
              |
              v
+-----------------------------+
| VALIDAZIONE E NORMALIZZAZIONE|
| mapping, scale, type, error |
+-------------+---------------+
              |
      +-------+--------+
      |                |
      v                v
   BUFFER         DMRUNTIMESTORE
      |                |
      +-------+--------+
              |
              v
        CHANGE / EVENT
```

Il backend deve:

- acquisire il valore;
- validare il risultato;
- applicare mapping e scala del dominio;
- aggiornare lo storage corretto;
- produrre change tracking o eventi quando necessario;
- mantenere separati gli errori di trasporto dai valori applicativi.

### 3.1 Modbus

```text
MODBUS DEVICE
      |
      v
MODBUS TRANSACTION
      |
      v
REGISTER DECODING
      |
      v
DEVICE / CHANNEL MAPPING
      |
      v
BUFFER WRITE
      |
      v
FIELD CHANGE CALLBACK / EVENT
```

La connessione, il cursore, il pacing, il retry e il cooldown appartengono al runtime Modbus. Il valore decodificato appartiene allo stato applicativo condiviso.

### 3.2 Physical I/O

```text
GPIO INPUT
    |
    v
PHYSICALIOMANAGER
    |
    +--> polling
    +--> active-level conversion
    +--> debounce
    |
    v
EVENTMANAGER PUSH
    |
    v
CENTRAL CONSUMER
    |
    v
BUFFER UPDATE
```

Una coda piena non deve far perdere definitivamente il valore: lo stato pubblicato viene aggiornato soltanto dopo una push riuscita, consentendo un nuovo tentativo al ciclo successivo.

### 3.3 RuntimeStore

```text
APPLICATION / EXTERNAL SOURCE
             |
             v
      RESOLVE HANDLE
             |
             v
    TYPE AND ACCESS CHECK
             |
             v
      DMRUNTIMESTORE WRITE
             |
             +--> source
             +--> changedAt
             +--> revision
             +--> dirty flag
```

Nel percorso runtime ordinario si usa l'handle. La ricerca tramite ID testuale è riservata a setup, introspezione e parsing di comandi non ancora risolti.

## 4. Propagazione degli eventi

```text
EVENT PRODUCERS
  +-- Modbus
  +-- Physical I/O
  +-- MQTT commands
  +-- Route / Toggle / Split
  +-- Automation
  +-- Internal services
          |
          v
+----------------------------+
|        EVENTMANAGER        |
| source, area, value, ID    |
+-------------+--------------+
              |
              v
+----------------------------+
| BOUNDED PROCESSING         |
| eventi limitati per loop   |
+-------------+--------------+
              |
   +----------+-----------+----------------+
   |                      |                |
   v                      v                v
BUFFER CONSUMERS    PHYSICAL OUTPUTS   DOMAIN LOGIC
```

Il processamento differito evita catene di callback ricorsive e limita il lavoro eseguito in una singola iterazione.

## 5. Elaborazione applicativa

```text
        BUFFER / DMRUNTIMESTORE
                  |
    +-------------+--------------+
    |             |              |
    v             v              v
 SECURITY        HVAC          AVERAGES
    |             |              |
    +------+------+------+-------+
           |             |
           v             v
         POWER         WEATHER
           |             |
           +------+------+------+
                  |             |
                  v             v
               SOLAR      ACCESS CONTROL
                  |
                  v
       STATE UPDATE / EVENT / ACTION
```

Gli engine:

- leggono lo stato condiviso necessario;
- mantengono internamente il proprio stato tecnico;
- producono risultati, comandi o eventi tramite i contratti del sistema;
- non accedono direttamente allo stato privato di altri engine;
- non dipendono dai frontend che ne pubblicano i dati.

## 6. Automazioni

```text
BUFFER / TIME / AVERAGES / EVENTS
                 |
                 v
+--------------------------------+
| CONDITIONS                     |
| threshold, trend, bitmask,     |
| time, multi, composite         |
+---------------+----------------+
                |
                v
+--------------------------------+
| AUTOMATION ENGINE              |
| rules, scenes, sequences       |
+---------------+----------------+
                |
                v
+--------------------------------+
| ACTION                         |
| target area, value, scheduling |
+---------------+----------------+
                |
                v
       BUFFER WRITE + EVENT
```

L'Automation Engine decide sul modello applicativo. Non deve conoscere il protocollo concreto dell'attuatore.

## 7. Route, toggle e split

```text
SOURCE AREA EVENT
        |
        +--> ROUTE MANAGER ----> TARGET AREA EVENT
        |
        +--> TOGGLE MANAGER ---> TOGGLED VALUE EVENT
        |
        +--> SPLIT MANAGER ----> OUTPUT AREA EVENTS
```

Route, toggle e split operano nel dominio delle aree, ma appartengono ai rispettivi manager logici. Non sono funzionalità interne dello storage Buffer.

## 8. Uscita verso il mondo fisico

```text
APPLICATION COMMAND
        |
        v
BUFFER WRITE / EVENT
        |
        v
OWNER VALIDATION
        |
   +----+---------+-----------+
   |              |           |
   v              v           v
MODBUS WRITE   RS485 WRITE   LOCAL OUTPUT
   |              |           |
   +--------------+-----------+
                  |
                  v
         ATTUATORE / DISPOSITIVO
```

Il percorso di uscita deve:

- validare area e valore;
- individuare il componente proprietario;
- applicare la codifica del dominio;
- utilizzare il protocollo corretto;
- aggiornare lo stato osservato soltanto secondo il contratto del dispositivo;
- distinguere comando richiesto e stato realmente confermato.

## 9. Costruzione del modello strutturale

```text
CONFIGURAZIONE E RUNTIME OBJECTS
  +-- aree e device
  +-- DeviceProfiles
  +-- route / toggle / split
  +-- Automation
  +-- HVAC / Averages / Power / Solar
  +-- MQTT / AEE / Web API
  +-- Buffer
  +-- DMRuntimeStore
              |
              v
+--------------------------------+
| DOMOINTROSPECTION              |
| Entity, Relation, Origin,      |
| Group, IOType, source refs     |
+---------------+----------------+
                |
                v
+--------------------------------+
| FINALIZATION                  |
| validation, implementationOnly|
+--------------------------------+
```

Introspection descrive il sistema senza duplicare la configurazione e senza diventare owner degli oggetti osservati.

## 10. Risoluzione semantica con DomoSemanticResolver

```text
STRUCTURAL MODEL
       |
       v
SPECIALIZED SOURCE SEMANTICS
       |
       v
PHYSICAL AND DERIVED RELATIONS
       |
       v
LABEL DICTIONARY FALLBACK
       |
       v
SEMANTIC MODEL
  +-- kind
  +-- unit
  +-- device class
  +-- state class
  +-- access
  +-- group
  +-- visibility
```

Il resolver semantico interpreta il modello strutturale. Non costruisce payload MQTT e non deve contenere identificatori `AREA_*` specifici dell'installazione.

## 11. Esposizione Home Assistant

```text
SEMANTIC MODEL
      |
      v
DMHOMEASSISTANT
  discovery, topic, payload,
  availability, device registry
      |
      v
DMHAORCHESTRATOR
  connection lifecycle,
  snapshot, PendingState
      |
      v
MQTT TRANSPORT
      |
      v
MQTT BROKER
      |
      v
HOME ASSISTANT
```

### 11.1 Flusso area in uscita

```text
BUFFER CHANGE / EVENT
        |
        v
DMHAORCHESTRATOR::publishEvent
        |
        v
PENDING STATE
        |
        v
SCALE AND TYPE CONVERSION
        |
        v
<prefix>/area/<area>/state
```

La pubblicazione deve usare la scala e la semantica dell'entity risolta.

### 11.2 Flusso RuntimeStore in uscita

```text
DIRTY RUNTIME ENTRY
        |
        v
INTROSPECTION RUNTIME ENTITY
        |
        v
TYPE-SPECIFIC READ
        |
        v
SENSITIVE VALUE POLICY
        |
        v
<prefix>/runtime/<id>/state
```

Le entry non leggibili o non pubblicabili non devono essere esposte come stato.

### 11.3 Snapshot iniziale

```text
MQTT CONNECTION
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
QUEUE INITIAL SNAPSHOT
      |
      v
PUBLISH CURRENT STATES
```

Lo snapshot riallinea Home Assistant allo stato corrente dopo avvio o riconnessione.

## 12. Comandi esterni

Un comando esterno non raggiunge direttamente un attuatore e non aggira lo stato applicativo o il dominio proprietario.

### 12.1 Comando area

```text
HOME ASSISTANT
      |
      v
MQTT BROKER
      |
      v
TRANSPORT CALLBACK
      |
      v
DMHAORCHESTRATOR
      |
      v
PARSE TOPIC AND PAYLOAD
      |
      v
VALIDATE ENTITY / ACCESS / TYPE
      |
      v
COMMAND CALLBACK
      |
      v
BUFFER WRITE / EVENTMANAGER
      |
      v
OWNER ENGINE / BACKEND
      |
      v
STATE FEEDBACK
```

### 12.2 Comando RuntimeStore

```text
<prefix>/runtime/<id>/set
       |
       v
DMHAORCHESTRATOR
       |
       v
LOOKUP HANDLE
       |
       v
TYPE AND ACCESS CHECK
       |
       v
DMRUNTIMESTORE WRITE
       |
       v
NORMALIZED INBOUND MESSAGE
       |
       v
STATE FEEDBACK IF READABLE
```

### 12.3 Messaggio MQTT normalizzato

```text
MQTT RAW MESSAGE
       |
       v
BACKEND DECODING
       |
       v
MQTTINBOUNDMESSAGE
  +-- origin
  +-- value type
  +-- client / backend
  +-- topic / payload
  +-- device / mapping
  +-- area
  +-- runtime handle / ID
       |
       v
APPLICATION CALLBACK
```

Topic e payload sono validi soltanto durante la callback sincrona. Un consumer che deve conservarli deve copiarli.

## 13. Security bitmask read/write

La sequenza dei bit appartiene al dominio Security e deve essere condivisa dai percorsi di lettura e scrittura.

### 13.1 Stato in uscita

```text
PANEL / SENSOR / ZONE STATE
             |
             v
SECURITY CODEC / HELPERS
  canonical bit order
             |
             v
STATUS WORDS / AREAS
             |
             v
BUFFER / INTROSPECTION
             |
             v
EXTERNAL PROJECTION
```

### 13.2 Comando in ingresso

```text
EXTERNAL COMMAND
       |
       v
ENTITY VALIDATION
       |
       v
SECURITY CODEC / HELPERS
  same canonical bit order
       |
       v
COMMAND AREA / EVENT
       |
       v
SECURITY ENGINE
       |
       v
PANEL / ZONE COMMAND
```

Il frontend non deve duplicare offset, descrittori o ordine dei bit.

## 14. Averages e scaling

```text
SOURCE BUFFER VALUES
        |
        v
INPUT SCALE
        |
        v
GROUP AGGREGATION
        |
        v
OUTPUT SCALE
        |
        v
DESTINATION BUFFER AREA
        |
        v
ENTITY SCALE FOR PUBLICATION
```

Relazione semantica:

```text
published value = Buffer value * Entity::scale
Entity::scale   = 1 / outScale
```

Ogni divisione deve validare scala e denominatore.

## 15. Networking e risorse

Non esiste un singolo Communication Scheduler universale. Ogni protocollo usa il proprio lifecycle, coordinandosi con le risorse comuni.

```text
PROTOCOL ENGINES
  +-- Modbus
  +-- MQTT
  +-- Web API
  +-- Bridge
  +-- Solar TCP
          |
          v
+-----------------------------+
| DMNETWORKMANAGER            |
| registration, arbitration,  |
| pacing, protocol state      |
+-------------+---------------+
              |
              v
+-----------------------------+
| SOCKETMANAGER               |
| owner, socket kind, slot    |
+-------------+---------------+
              |
              v
      PLATFORM NETWORK
```

### 15.1 Lifecycle dello slot

```text
PROTOCOL REQUEST
       |
       v
TRY ACQUIRE
       |
       +--> unavailable -> retry in later cycle
       |
       v
STORE OWNER AND SLOT
       |
       v
CONNECT / TRANSACT
       |
       +--> success
       +--> failure
       +--> timeout
       +--> endpoint change
       |
       v
STOP IF REQUIRED
       |
       v
RELEASE EXACT SLOT
```

Retry e recovery devono distribuire il lavoro nel tempo senza monopolizzare il runtime.

## 16. FrontendStatus

```text
FRONTEND LIFECYCLE
       |
       v
+-------------------------------+
| FRONTENDSTATUS                |
| Enabled / Running             |
| Initialized / Connected       |
| Active / Recovering           |
| Degraded / Fault              |
+---------------+---------------+
                |
                v
CHANGE DETECTION AND PUBLICATION
```

La word di stato distingue stato operativo, stato di salute e fase di recovery. `Connected` non è un requisito universale per tutti i frontend.

## 17. Recovery dopo disconnessione

```text
CONNECTED
   |
   v
NORMAL PUBLICATION
   |
   v
CONNECTION LOST
   |
   +--> mark disconnected
   +--> reset subscription state
   +--> reset availability state
   +--> reset discovery state
   +--> reset snapshot state
   |
   v
RECONNECT ATTEMPTS
   |
   v
CONNECTED AGAIN
   |
   v
SUBSCRIBE -> AVAILABILITY -> DISCOVERY -> SNAPSHOT
```

Un errore iniziale di snapshot non deve impedire il riallineamento dopo una successiva riconnessione.

## 18. Flusso completo

```text
                         MONDO FISICO
                              |
                              v
                     BACKEND / DRIVER
                              |
                 +------------+-------------+
                 |                          |
                 v                          v
              BUFFER                 DMRUNTIMESTORE
                 |                          |
                 +------------+-------------+
                              |
                              v
                         EVENTMANAGER
                              |
                 +------------+-------------+
                 |                          |
                 v                          v
          DOMAIN ENGINES              AUTOMATION
                 |                          |
                 +------------+-------------+
                              |
                              v
                   SHARED STATE / COMMANDS
                              |
               +--------------+---------------+
               |                              |
               v                              v
       INTROSPECTION / SEMANTICS       OWNER / BACKEND OUTPUT
               |                              |
               v                              v
       EXTERNAL PROJECTIONS              MONDO FISICO
```

## 19. Invarianti del flusso dati

Il sistema deve mantenere le seguenti invarianti:

- Buffer è autorevole per lo stato numerico condiviso delle aree;
- RuntimeStore è autorevole per lo stato tipizzato condiviso;
- lo stato tecnico appartiene all'engine proprietario;
- EventManager disaccoppia produzione e consumo;
- Introspection descrive senza possedere;
- Semantic Resolver interpreta senza codificare protocolli;
- i frontend proiettano lo stato senza diventare owner della logica;
- i comandi esterni attraversano validazione e dominio proprietario;
- codec e bit order non vengono duplicati;
- socket e connessioni hanno owner e lifecycle espliciti;
- retry e recovery non bloccano il ciclo principale;
- snapshot e subscription vengono riallineati dopo riconnessione.

## 20. Checklist per nuovi flussi

Prima di introdurre un nuovo flusso dati, verificare:

- [ ] sorgente identificata;
- [ ] owner del dato definito;
- [ ] tipo e scala dichiarati;
- [ ] Buffer o RuntimeStore scelto correttamente;
- [ ] stato tecnico separato dallo stato applicativo;
- [ ] validazione prima della pubblicazione;
- [ ] evento prodotto solo quando necessario;
- [ ] consumer e source ID documentati;
- [ ] comando distinto dallo stato confermato;
- [ ] accesso read/write verificato;
- [ ] codec condiviso tra lettura e scrittura;
- [ ] recovery dopo errore o disconnessione;
- [ ] risorse di rete acquisite e rilasciate;
- [ ] diagnostica disponibile;
- [ ] nessuna dipendenza diretta dal frontend nei domain engine.

## 21. Regola fondamentale

> **Il Buffer è autorevole per le aree numeriche. RuntimeStore è autorevole per le variabili tipizzate. EventManager propaga le modifiche. Gli engine decidono. Introspection descrive. Semantic Resolver interpreta. Gli orchestratori governano i lifecycle. I protocolli trasportano i dati senza diventare proprietari della logica.**

---

**DOMOMANAGER**  
**FLUSSO DEI DATI**  
**ACQUIRE. VALIDATE. PUBLISH. DECIDE. ACT. OBSERVE.**
