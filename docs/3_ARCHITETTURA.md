# DOMOMANAGER - ARCHITETTURA TECNICA DETTAGLIATA

## 1. Scopo

DomoManager e un runtime embedded per l'infrastruttura domestica. L'architettura separa acquisizione fisica, stato, logica applicativa, modellazione semantica e comunicazione esterna.

Principio fondamentale:

> Ogni componente deve conoscere la propria responsabilita, le dipendenze ammesse, il momento di esecuzione e il modo in cui rende osservabile il proprio comportamento.

## 2. Modello generale

```text
MONDO FISICO
     |
     v
BACKEND / DEVICE MANAGER / MODBUS / LOCAL I/O
     |
     v
BUFFER ENGINE  <-----------------------------+
     |                                        |
     +----------+-------------+---------------+
                |             |
                v             v
        APPLICATION       AUTOMATION
        ENGINES            ENGINE
                |             |
                +------+------+
                       |
                       v
                     BUFFER
                       |
        +--------------+----------------+
        |                               |
        v                               v
DomoIntrospection                COMMUNICATION TASK
        |                               |
        v                               v
DomoSemanticResolver          COMMUNICATION SCHEDULER
        |                               |
        v                     +---------+---------+
PROIEZIONI ESTERNE             |         |         |
        |                      v         v         v
        +------------------> MQTT     WEBAPI    BRIDGE
```

Il Buffer e la fonte unica di verita per le aree. `DMRuntimeStore` estende il modello con variabili tipizzate che non appartengono naturalmente al dominio delle aree.

## 3. Principi architetturali

### 3.1 Determinismo

- scheduling esplicito;
- intervalli definiti;
- timeout e retry controllati;
- stati osservabili;
- lifecycle prevedibili;
- nessuna dipendenza implicita da servizi esterni.

### 3.2 Non-blocking

Networking, MQTT, WebAPI, Bridge, RS485 e Modbus devono avanzare per passi controllati. Nessun servizio secondario deve bloccare il ciclo applicativo.

### 3.3 Separazione delle responsabilita

```text
Device Manager          -> dispositivi e profili
Backend / Modbus        -> acquisizione e attuazione
Buffer                   -> stato delle aree
DMRuntimeStore           -> variabili runtime tipizzate
Task Engine              -> scheduling applicativo
Automation Engine        -> comportamento dichiarativo
Application Engines      -> Security, HVAC, Power, Solar, Weather, Averages
DomoIntrospection        -> modello strutturale
DomoSemanticResolver     -> modello semantico
DMHomeAssistant          -> protocollo e Discovery Home Assistant
DMHAOrchestrator         -> lifecycle Home Assistant
Communication Scheduler  -> scheduling comunicazioni
Diagnostics              -> osservabilita
```

### 3.4 Nessuna duplicazione della configurazione

L'Introspection conserva riferimenti agli oggetti originali. I layer di comunicazione non devono introdurre mappe parallele di aree, bit o segnali gia descritti dal dominio proprietario.

### 3.5 Read/write simmetrico

Quando un dato e comandabile, codifica e decodifica devono condividere la stessa semantica. Per bitmask e word di stato, la sequenza dei bit deve appartenere al modulo che possiede il dominio.

## 4. Backend, Device Manager e Buffer

Il Backend collega il mondo fisico al modello interno mediante:

- Device Manager;
- profili dispositivo;
- Modbus TCP/RTU;
- Local I/O;
- RS485;
- routing delle letture e delle scritture;
- retry, timeout, cooldown ed error handling.

Il Buffer Engine conserva:

- aree tipizzate;
- timestamp;
- change tracking;
- aree virtuali;
- reverse, route, split e toggle;
- eventi interni;
- valori aggregati e bitmask.

Gli engine applicativi non devono aggirare il Buffer quando lo stato appartiene al dominio delle aree.

## 5. Automation Engine

L'Automation Engine supporta:

- scene;
- regole statiche e dinamiche;
- sequenze;
- scheduled rules;
- condizioni semplici, trend, debounce, bitmask e composite;
- input, output e stati tecnici.

Il flusso resta Buffer-driven:

```text
BUFFER -> CONDIZIONI -> DECISIONE -> AZIONI -> BUFFER
```

## 6. Application Engines

### 6.1 Security

Il sottosistema comprende:

```text
SecurityEngine
    +-- SecurityOrchestrator
    +-- WiredSensorsManager
    +-- SecurityHmiInterface
    +-- DomoManagerAlarmPanel
```

Gestisce sensori cablati, canali RT/H24, zone, armamento, eventi pannello, memoria allarme, tacitazione, fault di comunicazione e word di stato.

Il percorso read/write corretto e:

```text
PanelState / Security state
          |
          v
Security codec/helper condiviso
          |
          +--> status areas nel Buffer
          |
command areas dal Buffer
          |
          v
Security codec/helper condiviso
          |
          v
Security commands
```

La mappa dei bit deve avere una sola definizione nel dominio Security. Home Assistant non deve contenere tabelle parallele dei bit.

### 6.2 HVAC

Gestisce zone, setpoint, temperature, umidita, fan coil, pompe di calore, valvole, ACS, anti-legionella, defrost e protezioni.

### 6.3 Averages

Ogni gruppo legge i sensori con la propria scala, calcola la media e scrive nell'area di uscita usando `outScale`.

```text
valore pubblicato = valore Buffer * Entity::scale
Entity::scale = 1 / outScale
```

### 6.4 Power

Gestisce limiti, carichi, priorita, minOn/minOff, distacco, protezioni e auto-tuning.

### 6.5 Solar

`FrontendConfig::Solar` registra le aree dell'inverter. L'Introspection assegna origine e gruppo `SOLAR` alle entity relative a tensione, corrente, potenza, batteria, temperatura, stato inverter e stato comunicazione.

### 6.6 Weather

Gestisce temperatura, pioggia, vento, luce, medie mobili ed eventi meteorologici.

## 7. DomoIntrospection

`DomoIntrospection` costruisce una vista strutturale senza copiare la configurazione.

### 7.1 Context

Il `Context` raccoglie riferimenti a configurazione, DeviceProfiles, AutomationEngine, AEERegistry, Buffer, RuntimeStore, AverageCalculator, TimeManager, MQTT, WebAPI, HVAC, Averages e Solar.

### 7.2 Entity

Ogni `Entity` descrive:

- `area`;
- `kind`;
- `origin`;
- `group`;
- `ioType`;
- riferimenti ad AreaConfig e device;
- indice del device e del canale;
- Runtime handle ed entry;
- metadati semantici;
- accesso read/write;
- visibilita;
- relazioni strutturali;
- flag di provenienza;
- `scale`, configurazione Number e Climate.

### 7.3 EntityOrigin

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

### 7.4 EntityGroup

```text
ROOT             -> DomoManager
PHYSICAL_DEVICE  -> dispositivo fisico
SECURITY         -> Sistema antifurto
SOLAR            -> Inverter solare
```

### 7.5 Relation

Le relazioni rappresentano Route, Toggle, Split, input/output di automazione, binding AEE, MQTT e WebAPI.

### 7.6 Finalize e visibility

`finalize()` identifica entity prive di configurazione pubblica e stati tecnici. `implementationOnly` impedisce la pubblicazione esterna senza rimuovere l'entity dal modello diagnostico.

## 8. DomoSemanticResolver

Il resolver trasforma il modello strutturale in semantica pubblicabile.

Determina:

- `EntityKind`;
- `unit`;
- `deviceClass`;
- `stateClass`;
- accesso;
- gruppo;
- visibilita finale.

Ordine di precedenza:

1. semantica specialistica della sorgente;
2. struttura fisica e relazioni;
3. dizionario lessicale della label;
4. fallback `UNKNOWN`.

Il resolver non deve contenere ID `AREA_*` dell'installazione.

## 9. DMRuntimeStore

`DMRuntimeStore` gestisce variabili non naturalmente riconducibili ad aree Buffer.

Supporta:

- handle e ID stabili;
- BOOL, INT32, FLOAT, STRING e JSON;
- READ_ONLY, WRITE_ONLY e READ_WRITE;
- persistenza;
- pubblicazione opzionale;
- metadati semantici;
- min, max, step e precisione;
- sorgente della scrittura.

Le entity Runtime vengono indicizzate dall'Introspection e pubblicate usando l'ID:

```text
<prefix>/runtime/<id>/state
<prefix>/runtime/<id>/set
```

## 10. Home Assistant

### 10.1 DMHomeAssistant

Responsabilita:

- generazione MQTT Discovery;
- topic area e RuntimeStore;
- payload e metadati;
- Device Registry;
- pubblicazione availability e stati.

### 10.2 DMHAOrchestrator

Responsabilita:

- setup del trasporto;
- subscription dei command topic;
- recovery dopo riconnessione;
- availability;
- Discovery;
- snapshot iniziale;
- coda `PendingState`;
- command routing area/RuntimeStore;
- feedback sullo state topic.

### 10.3 HAMQTTTransport

Adatta l'orchestratore al client MQTT reale senza trasferire nel layer Home Assistant la gestione del protocollo fisico.

### 10.4 Topic

```text
<prefix>/status
<prefix>/area/<area>/state
<prefix>/area/<area>/set
<prefix>/runtime/<id>/state
<prefix>/runtime/<id>/set
```

### 10.5 Snapshot

Alla connessione:

1. subscription;
2. availability;
3. Discovery;
4. snapshot delle entity area;
5. snapshot delle entity RuntimeStore;
6. pubblicazione incrementale degli eventi successivi.

### 10.6 Device Registry

Identificatori stabili aggregano le entity sotto:

- DomoManager;
- device fisici;
- Sistema antifurto;
- Inverter solare.

I device secondari usano `via_device` verso il controller root.

## 11. Communication Runtime

```text
COMMUNICATION TASK
        |
        v
COMMUNICATION SCHEDULER
        +-- MQTT
        +-- WebAPI
        +-- Bridge
```

Il scheduler applica intervallo, priorita, timed round-robin, isolamento dei servizi e budget delle risorse di rete.

## 12. Lifecycle

```text
BOOT
 -> hardware init
 -> config load e validation
 -> Buffer e device setup
 -> engine setup
 -> task setup
 -> RuntimeStore registration
 -> Introspection build
 -> Semantic Resolver
 -> communication setup
 -> power-on cycle
 -> normal operation
```

## 13. Diagnostica

La diagnostica deve osservare:

- Buffer, device e profili;
- relazioni Introspection;
- entity unknown e hidden;
- origin, group e scaling;
- automazioni e task;
- comunicazioni, Ethernet e socket;
- RuntimeStore;
- Security, HVAC, Solar, Power e Weather;
- watchdog e Hot Standby.

## 14. Regole per nuovi moduli

Ogni nuovo modulo deve dichiarare:

- responsabilita;
- stato usato o prodotto;
- scheduler di appartenenza;
- comportamento non-blocking;
- errori, retry e fallback;
- diagnostica;
- risorse consumate;
- dipendenze ammesse;
- eventuale modello Introspection e semantico.

## 15. Sintesi

DomoManager separa acquisizione, stato, logica, semantica e comunicazione. Il Buffer governa le aree, RuntimeStore governa le variabili runtime, Introspection descrive il sistema, Semantic Resolver lo rende interpretabile e i protocolli esterni ne espongono proiezioni controllate.
