# DomoManager - Dipendenze dei file e dei moduli

## 1. Scopo

Questo documento descrive tre categorie distinte di dipendenza:

1. **dipendenze fisiche di compilazione**, espresse principalmente dagli `#include`;
2. **dipendenze architetturali**, cioè la conoscenza ammessa tra moduli;
3. **dipendenze runtime**, cioè i servizi e gli oggetti che devono essere disponibili durante l'esecuzione.

Le tre viste non sono equivalenti. Un file può includerne un altro senza possederne il lifecycle; allo stesso modo, un componente può ricevere a runtime un'interfaccia senza dipendere direttamente dall'implementazione concreta.

L'obiettivo non è minimizzare il numero degli include, ma mantenere dipendenze:

- comprensibili;
- prevalentemente unidirezionali;
- coerenti con l'ownership;
- prive di cicli non necessari;
- verificabili nel repository;
- compatibili con il runtime embedded.

> **File, classi, engine e servizi architetturali devono essere rappresentati in viste separate.**

## 2. Convenzioni

Nel documento vengono utilizzate le seguenti notazioni:

```text
A --> B     A dipende da B
A ..> B     dipendenza runtime o tramite contratto
A o-- B     A aggrega B senza possederne necessariamente lo storage
A *-- B     A possiede il lifecycle di B
```

I nomi dei file sono riportati con estensione coerente con il corpus analizzato. Dove il repository utilizza file `.cpp` come unità includibili tramite guard o `#pragma once`, il documento conserva il nome effettivo invece di inventare un corrispondente `.hpp`.

## 3. Vista fisica dei file

### 3.1 Entry point e composition root

```text
DomoManager.cpp
  |
  +--> DMFrontend.*
  +--> DMSetup.cpp
  +--> DMPlatform.cpp
  +--> DMLogger.*
```

Responsabilità:

- `DomoManager.cpp` contiene l'entry point Arduino e la configurazione finale;
- `DMSetup.cpp` compone aree, dispositivi, engine e frontend;
- `DMPlatform.cpp` astrae l'hardware specifico di Opta ed ESP32;
- il frontend avvia e coordina il sistema configurato.

L'entry point non deve duplicare l'inizializzazione posseduta dal runtime o dal backend di piattaforma.

### 3.2 Core e runtime

```text
DMRuntime.cpp
  |
  +--> DMModbus.cpp
  +--> DMNetwork.*
  +--> DMAutomationBuilder.*
  +--> DMDeclares.h
  +--> DMOPTAHStandby.cpp
  +--> DMPlatform.cpp
  +--> DMLogger.*

DMPLC.h
  |
  +--> device abstractions
  +--> DeviceManager
  +--> DeviceProfiles
  +--> area mappings
```

Il **Physical I/O Manager** possiede polling, debounce e pubblicazione degli eventi degli I/O locali.

`DMRuntime.cpp` aggrega il ciclo operativo, ma non deve assorbire la logica interna dei protocolli o dei domain engine.

### 3.3 Stato condiviso

```text
Buffer engine
  +--> numeric areas
  +--> timestamps
  +--> change tracking
  +--> virtual areas

DMRuntimeStore.cpp
  +--> typed entries
  +--> static text pool
  +--> access control
  +--> dirty tracking
  +--> integrity validation
```

Buffer e RuntimeStore sono complementari:

```text
Buffer          : area   --> long
RuntimeStore    : handle --> typed value
```

Nessuno dei due deve dipendere dai frontend che ne pubblicano una proiezione.

### 3.4 Modello strutturale e semantico

```text
DMIntrospection.cpp
  |
  +--> configuration references
  +--> Buffer
  +--> DMRuntimeStore.cpp
  +--> DeviceProfiles
  +--> Automation
  +--> domain registrations

DMSemanticResolver.cpp
  |
  +--> DMIntrospection.cpp
  +--> semantic dictionary
```

La direzione consentita è:

```text
CONFIGURATION / RUNTIME OBJECTS
              |
              v
      DMIntrospection.cpp
              |
              v
    DMSemanticResolver.cpp
```

Il resolver semantico non deve dipendere dai client MQTT o dai payload Home Assistant.

### 3.5 MQTT e Home Assistant

```text
DMMQTT.cpp
  +--> MQTT client library
  +--> frontend MQTT configuration
  +--> normalized inbound message
  +--> backend implementations

DMHomeAssistant.cpp
  +--> DomoIntrospection model
  +--> semantic metadata
  +--> RuntimeStore metadata
  +--> MQTT topic and payload generation

DMHAOrchestrator
  +--> DMHomeAssistant
  +--> transport callbacks
  +--> Buffer command routing
  +--> RuntimeStore command routing
  +--> connection lifecycle
```

Il percorso di pubblicazione è:

```text
BUFFER / RUNTIME STORE
          |
          v
    INTROSPECTION
          |
          v
  SEMANTIC RESOLVER
          |
          v
 DMHOMEASSISTANT
          |
          v
  MQTT TRANSPORT
```

Il percorso dei comandi è:

```text
MQTT BROKER
    |
    v
TRANSPORT CALLBACK
    |
    v
DMHA ORCHESTRATOR
    |
    +--> validate area ------> Buffer / EventManager
    |
    +--> validate runtime ---> DMRuntimeStore
```

### 3.6 Web API e controllo accessi

```text
DMWebAPI.cpp
  +--> HTTP client/server state machines
  +--> request queue
  +--> response correlation
  +--> authentication
  +--> parsers

DMWebAPIDefs.cpp
  +--> DMWebAPI.cpp contracts
  +--> device profiles
  +--> AccessControlService
  +--> Fanvil / AxessTMC / Reolink definitions
```

Le definizioni applicative possono dipendere dal motore Web API. Il motore generico non deve dipendere dai profili applicativi concreti.

### 3.7 Security e sensori cablati

```text
DMIntrusion.cpp
  +--> AlarmPanelInterface
  +--> partitions
  +--> zones
  +--> events and diagnostics

DMWiredSensors.cpp
  +--> wired channel engine
  +--> debounce
  +--> startup inhibit
  +--> zone aggregation
  +--> AlarmBitmaskManager
```

Il dominio Security possiede:

- codec delle bitmask;
- word di stato;
- comandi;
- semantica delle zone e delle partizioni.

I frontend consumano questi contratti senza replicare le mappe dei bit.

### 3.8 Power, Weather, HVAC e Solar

```text
DMPower.cpp
  +--> PowerSupervisor
  +--> thresholds and hysteresis
  +--> alarm state
  +--> diagnostics

DMWeather.cpp
  +--> acquisition readers
  +--> moving averages
  +--> day/night hysteresis
  +--> weather events

DMHVAC.h
  +--> HeatPumpController
  +--> zones and setpoints
  +--> ACS / defrost / anti-legionella

DMSolarInverter.cpp
  +--> UART transport
  +--> TCP transport
  +--> command discovery
  +--> paced polling
  +--> inverter snapshot
```

Questi engine dipendono dai servizi di base necessari, non dai frontend che ne espongono lo stato.

### 3.9 Piattaforma e networking

```text
DMPlatform.cpp
  +--> Arduino core
  +--> platform-specific GPIO
  +--> RTC support
  +--> Wire / expander
  +--> status LEDs

DMPlatformNetwork.cpp
  +--> Ethernet backend
  +--> W5500 on ESP32
  +--> network recovery
  +--> TCP client/server factories

DMPlatformModbus.cpp
  +--> ArduinoModbus
  +--> common Modbus API

DMNetworkManager
  +--> protocol registration
  +--> arbitration and pacing
  +--> protocol state
  +--> SocketManager
```

Il backend di piattaforma deve rimanere al di sotto del runtime e dei protocolli applicativi.

## 4. Layer architetturali

```text
LAYER 0 - PLATFORM
  Arduino core
  DMPlatform
  DMPlatformNetwork
  DMPlatformModbus

LAYER 1 - FOUNDATION
  DMDeclares
  DMLogger
  common types
  base utilities
  time primitives

LAYER 2 - STATE AND RESOURCES
  Buffer
  DMRuntimeStore
  EventManager
  DeviceProfiles
  DeviceManager
  DMNetworkManager
  SocketManager

LAYER 3 - BACKEND AND PROTOCOL ENGINES
  Modbus
  Physical I/O
  RS485
  Bridge
  MQTT
  Web API
  Solar transports

LAYER 4 - DOMAIN LOGIC
  Automation
  Security
  HVAC
  Power
  Weather
  Solar model
  Averages
  Access Control

LAYER 5 - STRUCTURAL AND SEMANTIC MODEL
  DomoIntrospection
  DomoSemanticResolver

LAYER 6 - FRONTEND ORCHESTRATION
  DMHomeAssistant
  DMHAOrchestrator
  frontend engines
  lifecycle coordinators

LAYER 7 - COMPOSITION AND APPLICATION
  DMSetup
  DomoManager entry point
  installation configuration
```

I layer descrivono la direzione architetturale, non obbligano a creare un file per ogni livello.

## 5. Direzione ammessa delle dipendenze

```text
APPLICATION / COMPOSITION
            |
            v
FRONTEND ORCHESTRATION
            |
     +------+------+----------------+
     |             |                |
     v             v                v
DOMAIN LOGIC    MODEL         PROTOCOL ENGINES
     |             |                |
     +------+------+----------------+
            |
            v
STATE / EVENTS / RESOURCES
            |
            v
FOUNDATION / PLATFORM
```

Regole:

- il livello superiore può dipendere dai contratti inferiori;
- i domain engine non dipendono da Home Assistant;
- i frontend non possiedono la semantica interna dei domini;
- Introspection osserva riferimenti autorizzati, senza diventare owner degli engine;
- Semantic Resolver dipende dal modello strutturale;
- Buffer e RuntimeStore non dipendono dai protocolli di pubblicazione;
- il backend di piattaforma non dipende dalla configurazione dell'installazione.

## 6. Dipendenze architetturali tra moduli

### 6.1 Stato e runtime

```text
DMRuntime *-- EventManager
DMRuntime *-- PhysicalIOManager
DMRuntime *-- runtime task table
DMRuntime o-- Buffer
DMRuntime o-- DeviceManager
DMRuntime o-- DMNetworkManager
DMRuntime o-- TimeManager
DMRuntime o-- AutomationEngine
```

`DMRuntime` coordina questi componenti, ma l'ownership effettiva dello storage può appartenere al composition root o al manager principale.

### 6.2 Introspection

```text
DomoIntrospection ..> Buffer
DomoIntrospection ..> DMRuntimeStore
DomoIntrospection ..> DeviceProfiles
DomoIntrospection ..> AutomationEngine
DomoIntrospection ..> domain configuration
DomoSemanticResolver --> DomoIntrospection
Diagnostics --> DomoIntrospection
DMHomeAssistant --> DomoIntrospection
```

Introspection produce una read-only view strutturale. Non comanda direttamente i domini osservati.

### 6.3 Networking

```text
ProtocolEngine ..> DMNetworkManager
ProtocolEngine ..> SocketManager
DMNetworkManager *-- protocol runtime state
SocketManager *-- socket ownership table
PlatformNetwork --> Ethernet implementation
```

Chi acquisisce un socket deve conservarne owner e slot, quindi rilasciarlo secondo il lifecycle del protocollo.

## 7. Dipendenze runtime

Le dipendenze runtime esprimono disponibilità, non necessariamente inclusione diretta.

### 7.1 DMRuntime

Durante l'esecuzione richiede riferimenti validi a:

- DomoManager;
- TimeManager;
- DeviceManager;
- DMNetworkManager;
- IpManager;
- Buffer;
- LedController;
- DeviceProfiles;
- configurazione.

### 7.2 Home Assistant

L'orchestratore richiede:

- trasporto MQTT valido;
- stato di connessione;
- modello Introspection risolto;
- callback verso Buffer/EventManager;
- RuntimeStore per le entity runtime;
- metadati e topic coerenti.

### 7.3 Modbus TCP

Il runtime Modbus richiede:

- client Modbus di piattaforma;
- IP manager;
- DeviceManager;
- Buffer;
- DMNetworkManager;
- owner e slot SocketManager;
- timestamp corrente.

### 7.4 Web API

Il motore Web API richiede:

- client o server TCP;
- configurazioni dei dispositivi;
- coda e stato delle richieste;
- timeout;
- callback applicative;
- arbitraggio di rete quando condiviso.

## 8. Dipendenze vietate e preferite

### 8.1 Da evitare

```text
HVAC                 --> MQTT implementation
Power                --> Home Assistant internals
Security             --> duplicated frontend bit tables
DMHomeAssistant      --> installation-specific AREA_* constants
DomoIntrospection    --> MQTT payload encoding
DMRuntimeStore       --> DMHAOrchestrator
Buffer               --> frontend engines
Logger               --> domain engines
Platform backend     --> installation configuration
Protocol engine A    --> private state of protocol engine B
```

### 8.2 Da preferire

```text
HVAC                 --> Buffer / domain contracts
Security             --> Buffer / EventManager / Security codec
Power                --> Buffer / callbacks / domain contracts
Home Assistant       --> Introspection / semantic model
MQTT command         --> Orchestrator --> Buffer or RuntimeStore
Diagnostics          --> read-only views
Cross-engine signal  --> state, event or shared interface
Network protocol     --> DMNetworkManager / SocketManager
```

## 9. Regola anti-circolare

Le collaborazioni laterali devono passare attraverso:

- Buffer;
- RuntimeStore;
- EventManager;
- interfacce comuni;
- callback;
- Introspection in sola lettura;
- servizi di orchestrazione autorizzati.

Un engine non deve includere un altro engine soltanto per accedere al suo stato privato.

Quando emerge un ciclo di include, verificare nell'ordine:

1. se la dipendenza è realmente necessaria;
2. se è sufficiente una forward declaration;
3. se il contratto deve essere estratto in un tipo comune;
4. se la collaborazione deve passare attraverso un evento;
5. se uno dei due moduli sta violando l'ownership dell'altro.

## 10. Servizi trasversali

I servizi trasversali includono:

- TimeManager;
- Logger;
- Diagnostics;
- Watchdog;
- DMNetworkManager;
- SocketManager;
- FrontendStatus;
- Hot Standby.

La trasversalità non autorizza dipendenze inverse arbitrarie. Un servizio comune deve dipendere da contratti stabili o read-only view, non dalle implementazioni concrete dei domain engine.

## 11. Verifica delle dipendenze fisiche

Il diagramma fisico deve essere rigenerato periodicamente dagli `#include` reali del repository.

La procedura di verifica deve:

1. acquisire tutti i file sorgente e header;
2. estrarre gli `#include` locali;
3. normalizzare estensioni e alias;
4. distinguere include di sistema e include del progetto;
5. rilevare file mancanti;
6. costruire il grafo diretto;
7. individuare cicli e strongly connected components;
8. confrontare il grafo fisico con i layer ammessi.

I nomi non verificati non devono essere presentati come file esistenti. Possono comparire soltanto come componenti architetturali esplicitamente marcati.

## 12. Checklist per nuovi file

Prima di aggiungere un nuovo file o modulo, verificare:

- [ ] nome coerente con il dominio;
- [ ] responsabilità unica;
- [ ] layer architetturale identificato;
- [ ] include locali minimi e giustificati;
- [ ] assenza di dipendenze verso layer superiori;
- [ ] forward declaration usata quando sufficiente;
- [ ] ownership dello storage definita;
- [ ] stato applicativo distinto dallo stato tecnico;
- [ ] dipendenze runtime documentate;
- [ ] risorse di rete e socket dichiarate;
- [ ] diagnostica disponibile;
- [ ] assenza di mappe duplicate;
- [ ] nessun ID installazione-specifico nei moduli generici;
- [ ] compatibilità con le piattaforme previste.

## 13. Sintesi

DomoManager deve mantenere tre viste coerenti ma separate:

- il **grafo fisico**, derivato dagli include;
- il **grafo architetturale**, derivato dalle responsabilità;
- il **grafo runtime**, derivato dai riferimenti e dai servizi necessari durante l'esecuzione.

Il principio finale è:

> **Un modulo può dipendere dalle fondamenta del sistema, ma non deve conoscere dettagli dei moduli superiori o laterali quando la collaborazione può avvenire tramite stato condiviso, eventi, Introspection o interfacce dedicate.**

---

**DOMOMANAGER**  
**FILE AND MODULE DEPENDENCIES**  
**UNIDIRECTIONAL. EXPLICIT. VERIFIABLE.**
