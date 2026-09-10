# ============================================================
# VISIONE DEL PROGETTO DOMOMANAGER
# ============================================================


## 1. Perché creare un nuovo manager domotico?

La domanda è legittima: perché sviluppare un sistema come DomoManager
quando esistono già piattaforme diffuse come Home Assistant, Hubitat,
OpenHAB, Shelly Cloud, Tuya, Zigbee2MQTT e decine di ecosistemi RF?

La risposta è semplice:

**perché DomoManager nasce con un obiettivo differente.**

Non vuole essere soltanto una piattaforma di integrazione o una
raccolta di dispositivi intelligenti.

Nasce per costruire una base domotica:

- deterministica
- industriale
- cablata
- locale
- prevedibile
- silenziosa
- diagnosticabile
- indipendente dal cloud

La domotica moderna è diventata spesso un insieme di gadget:

- dispositivi che parlano principalmente via RF
- cloud obbligatori o fortemente integrati
- automazioni difficili da prevedere
- firmware chiusi
- ecosistemi frammentati
- comportamenti variabili sotto carico
- dipendenza dalla rete Wi-Fi

DomoManager nasce per ribaltare questo paradigma.


---

## 2. La nostra visione

DomoManager vuole riportare la domotica nel suo contesto naturale:

**impiantistica, affidabile, industriale, cablata e prevedibile.**

La casa non è un giocattolo.

Non è un insieme di lampadine smart.

È un sistema complesso che merita strumenti professionali.

La nostra visione si basa su alcuni principi fondamentali:

- prima il cavo, poi la radio
- determinismo prima della complessità
- locale prima del cloud
- buffer come fonte unica di verità
- separazione tra logica applicativa e comunicazione
- esecuzione non bloccante
- diagnostica come parte dell'architettura
- risorse hardware considerate parte del progetto software


---

## 3. Pilastro 1 — Minimizzare le emissioni RF

La casa moderna è invasa da:

- Zigbee
- Wi-Fi
- Bluetooth
- Thread
- Sub-GHz proprietari
- mesh
- repeater
- bridge wireless

Ogni dispositivo wireless aggiunge una componente di complessità:

- interferenze
- latenza
- dipendenza dalla qualità del segnale
- gestione delle batterie
- failure point aggiuntivi
- maggiore difficoltà diagnostica

DomoManager segue un principio opposto:

**prima il cavo, poi il resto.**

Preferiamo:

- Modbus cablato
- RS485
- ingressi digitali
- ingressi analogici
- sensori a filo
- attuatori a relè
- Ethernet

La radio può essere utilizzata quando è realmente utile,
ma non deve essere necessariamente la fondazione dell'impianto.


---

## 4. Pilastro 2 — Portare la domotica verso un modello industriale

DomoManager eredita concetti tipici dei sistemi industriali e dei PLC:

- cicli deterministici
- watchdog
- gestione degli errori
- fallback
- failover
- Hot Standby
- diagnostica
- controllo delle risorse
- separazione delle responsabilità
- gestione esplicita dello stato

Il sistema utilizza un modello nel quale:

**il comportamento applicativo non deve dipendere dal comportamento
di un singolo servizio di comunicazione.**

Un problema di MQTT, WebAPI o Bridge non deve trasformarsi
automaticamente in un blocco del ciclo applicativo.

Questo porta a un principio fondamentale:

> **Nessun servizio secondario deve poter bloccare il ciclo principale
> dell'applicazione.**

La casa deve continuare a funzionare anche quando:

- manca internet
- il Wi-Fi non è disponibile
- un servizio cloud è offline
- un'integrazione esterna non risponde
- una comunicazione presenta errori


---

## 5. Pilastro 3 — Buffer come unica fonte di verità

Il cuore dell'architettura è il **Buffer Engine**.

Il Buffer rappresenta lo stato corrente del sistema.

Non devono esistere copie arbitrarie dello stesso stato sparse
nei vari moduli.

Il modello è:

```text
DEVICE
   │
   ▼
BACKEND
   │
   ▼
BUFFER
   │
   ├──────────────┐
   ▼              ▼
APPLICATION    AUTOMATION
   │              │
   └──────┬───────┘
          ▼
       BUFFER
          │
          ▼
      OUTPUT
```

Il Buffer contiene:

- aree tipizzate
- timestamp
- change tracking
- virtual areas
- reverse
- split
- toggle
- eventi interni

Il Buffer è quindi:

**Single Source of Truth.**

Questo permette a:

- Backend
- Task Engine
- Automation Engine
- HVAC
- Security
- Power
- Weather
- MQTT
- WebAPI
- Bridge

di lavorare su una rappresentazione coerente del sistema.


---

## 6. Pilastro 4 — Separare applicazione e comunicazione

Uno dei principi più importanti della nuova architettura è la
separazione tra:

**Application Runtime**

e

**Communication Runtime.**

Il Task Engine gestisce i task applicativi:

- Security / Sensors
- HVAC
- Averages
- Power
- Weather
- altre funzioni applicative

La comunicazione viene invece gestita dal:

**Communication Scheduler.**

Il modello è:

```text
                    TASK ENGINE
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
           HVAC       SECURITY    AVERAGES
             │           │           │
             └───────────┼───────────┘
                         │
                         ▼
                      BUFFER
                         │
                         ▼
               COMMUNICATION TASK
                         │
                         ▼
              COMMUNICATION SCHEDULER
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
          BRIDGE        MQTT       WEBAPI
```

Questo permette di impedire che una comunicazione secondaria
possa monopolizzare il ciclo applicativo.


---

## 7. Pilastro 5 — Scheduling deterministico

Il Task Engine registra ed esegue i task secondo:

- intervallo
- stato enabled / disabled
- ordine controllato
- ciclo di esecuzione

La comunicazione utilizza invece un:

**Communication Scheduler a timed round-robin.**

Ogni servizio può avere:

- interval
- priority
- enabled / disabled
- callback dedicata

Il modello impedisce che un singolo servizio di comunicazione
possa occupare indefinitamente il runtime.

La comunicazione viene quindi trattata come una risorsa schedulata,
non come una funzione privilegiata rispetto all'applicazione.


---

## 8. Pilastro 6 — Non-blocking by design

La filosofia di DomoManager non considera il non-blocking
un'ottimizzazione opzionale.

È un requisito architetturale.

Il principio è:

> **La comunicazione deve avanzare attraverso piccoli passi
> deterministici, senza bloccare il ciclo principale.**

Questo vale per:

- Modbus
- RS485
- Bridge
- MQTT
- WebAPI
- gestione delle connessioni
- retry
- timeout
- cooldown

Il runtime deve continuare a eseguire le proprie responsabilità
anche quando una comunicazione è lenta o temporaneamente
non disponibile.


---

## 9. Pilastro 7 — Le risorse hardware fanno parte dell'architettura

In un sistema embedded le risorse non sono infinite.

Devono essere considerati esplicitamente:

- memoria
- CPU
- Ethernet
- socket TCP
- client simultanei
- connessioni MQTT
- connessioni Modbus
- timeout
- watchdog

Una risorsa hardware esaurita non deve diventare un comportamento
misterioso.

Deve poter essere:

1. rilevata
2. registrata
3. diagnosticata
4. gestita
5. resa visibile

Per questo il resource management e la diagnostica fanno parte
dell'architettura runtime.


---

## 10. Pilastro 8 — Ridurre la complessità, aumentare la leggibilità

DomoManager evita la cosiddetta "magia" software.

Il sistema privilegia:

- buffer centrale
- automazioni dichiarative
- condizioni esplicite
- scene leggibili
- sequenze strutturate
- JSON comprensibile
- diagnostica integrata
- flussi dati tracciabili

Ogni decisione deve essere spiegabile.

Ogni valore deve avere una posizione precisa.

Ogni errore deve poter essere ricondotto a una causa.


---

## 11. L'utente come architetto, non come programmatore

DomoManager è pensato per chi progetta impianti e automazioni,
non esclusivamente per chi sviluppa software.

Per questo privilegia:

- automazioni dichiarative
- scene
- regole
- condizioni
- sequenze
- configurazioni JSON
- diagnostica leggibile

L'obiettivo è permettere a un tecnico di costruire logiche
complesse senza dover trasformare ogni esigenza in codice.


---

## 12. Robustezza industriale, flessibilità domestica

DomoManager combina concetti industriali con strumenti moderni.

### Industrial

- watchdog
- fallback
- error handling
- Hot Standby
- failover
- diagnostica
- cicli deterministici
- gestione delle risorse

### Moderno

- MQTT
- WebAPI
- Bridge
- Home Assistant
- automazioni dinamiche
- JSON
- integrazioni esterne

Il risultato desiderato è:

**solido come un sistema industriale, flessibile come un framework
moderno.**


---

## 13. Il tempo come fondamento

Il Time Manager non è un semplice servizio di data e ora.

È il riferimento temporale del sistema.

Gestisce:

- RTC
- NTP
- epoch
- sincronizzazione temporale
- callback periodiche
- secondi
- minuti
- ore
- giorni

Il tempo alimenta:

- automazioni
- HVAC
- Power
- Security
- Weather
- scheduled rules

Il tempo non è semplicemente un valore.

**È una dimensione del sistema.**


---

## 14. La diagnostica come sistema immunitario

Un sistema affidabile non deve limitarsi a funzionare.

Deve anche spiegare quando qualcosa non funziona.

La diagnostica osserva:

- Buffer
- Backend
- Frontend
- Task Engine
- Communication Scheduler
- dispositivi
- Modbus
- comunicazioni
- Ethernet
- socket
- RTC
- Security
- HVAC
- Power
- Hot Standby
- watchdog
- overload

L'obiettivo non è soltanto:

**"cosa non funziona?"**

ma:

**"perché non funziona?"**

La diagnostica è quindi parte integrante del runtime.


---

## 15. Hot Standby

La continuità operativa può essere estesa attraverso Hot Standby.

Il sistema distingue:

- MASTER
- SLAVE

Il MASTER gestisce il normale runtime operativo e le comunicazioni.

Il comportamento del nodo SLAVE viene mantenuto separato per
evitare conflitti sulle risorse di rete.

Sono quindi parte del modello:

- heartbeat
- failover
- sincronizzazione
- state replication
- process replication
- gestione Ethernet
- cambio di ruolo

Il passaggio MASTER / SLAVE è parte del modello di affidabilità
del sistema.


---

## 16. La casa come ecosistema

La casa non è un insieme di oggetti intelligenti.

È un organismo.

Il modello concettuale è:

```text
DEVICE       → organi
BUFFER       → memoria / stato centrale
TIME MANAGER → ritmo
TASK ENGINE  → attività del sistema
AUTOMATION   → comportamento
COMMUNICATION→ sistema di comunicazione
DIAGNOSTICS  → sistema immunitario
HOT STANDBY  → continuità operativa
```

DomoManager è il sistema nervoso che coordina questi elementi.


# ============================================================
# 2. PANORAMICA GENERALE DEL SISTEMA
# ============================================================


DomoManager è costruito come un sistema a livelli, nel quale ogni
componente possiede una responsabilità precisa.

Il modello generale è:

```text
HARDWARE
   │
   ▼
BACKEND
   │
   ▼
BUFFER
   │
   ├───────────────┐
   ▼               ▼
TASK ENGINE    AUTOMATION
   │               │
   └───────┬───────┘
           ▼
        BUFFER
           │
           ▼
COMMUNICATION TASK
           │
           ▼
COMMUNICATION SCHEDULER
           │
      ┌────┼────┐
      ▼    ▼    ▼
   BRIDGE MQTT WEBAPI
```

I principali livelli sono:


### 2.1 Hardware

- Opta / Arduino
- sensori
- attuatori
- I/O digitali
- I/O analogici
- Modbus TCP
- Modbus RTU
- RS485
- Ethernet
- RTC


### 2.2 Backend

- DomoManager Core
- Device Manager
- Modbus Engine
- Buffer Engine
- Time Manager

Il Backend acquisisce e normalizza lo stato del mondo fisico.


### 2.3 Buffer

Il Buffer rappresenta lo stato centrale del sistema.

È:

- tipizzato
- timestamped
- monitorabile
- tracciabile
- accessibile dai vari Engine

È la:

**Single Source of Truth.**


### 2.4 Automation

L'Automation Engine interpreta lo stato del sistema e applica
le logiche dichiarative:

- scene
- rules
- sequences
- scheduled rules
- composite conditions
- trends
- debounce
- dynamic automations


### 2.5 Frontend / Application Runtime

Il Task Engine orchestra le attività applicative.

Tra queste:

- Security
- HVAC
- Averages
- Power
- Weather

Ogni task legge lo stato necessario dal Buffer, esegue la propria
logica e produce eventuali aggiornamenti.


### 2.6 Communication Runtime

La comunicazione è gestita separatamente attraverso:

**Communication Task → Communication Scheduler**

I servizi principali sono:

- Bridge
- MQTT
- WebAPI

Il Communication Scheduler utilizza scheduling temporizzato
e round-robin.


### 2.7 Diagnostica

La diagnostica osserva il sistema nel suo complesso:

- stato
- performance
- errori
- timing
- risorse
- comunicazioni
- watchdog
- Hot Standby


# ============================================================
# 3. ARCHITETTURA TECNICA DETTAGLIATA
# ============================================================


## 3.1 Buffer Engine

Il Buffer Engine è il centro dello stato applicativo.

Funzioni principali:

- aree tipizzate
- timestamp
- change tracking
- virtual areas
- reverse
- split
- toggle
- eventi interni

Il Buffer costituisce la lingua comune tra i vari Engine.


## 3.2 Device Manager

Il Device Manager gestisce la rappresentazione dei dispositivi.

Responsabilità:

- device profiles
- mapping area → register
- priorità
- error handling
- cooldown
- routing


## 3.3 Modbus Engine

Il Modbus Engine gestisce la comunicazione con dispositivi
Modbus in modo deterministico.

Funzioni:

- polling round-robin
- read
- write
- timeout
- retry
- cooldown
- gestione errori
- gestione del ciclo di connessione

Il principio fondamentale è il non-blocking.


## 3.4 Time Manager

Gestisce il riferimento temporale:

- RTC hardware
- NTP
- epoch
- fallback temporale
- callback periodici
- sincronizzazione

Il Time Manager fornisce la base temporale a:

- HVAC
- automazioni
- Power
- Security
- Weather
- scheduled rules


## 3.5 Automation Engine

L'Automation Engine implementa il comportamento dichiarativo.

Supporta:

- scene
- rules
- conditions
- sequences
- scheduled rules
- composite logic
- trends
- debounce
- dynamic automations
- JSON Builder

L'Automation Engine legge dal Buffer e scrive nel Buffer.


## 3.6 Task Engine

Il Task Engine costituisce il livello centrale di orchestrazione
dell'applicazione.

Gestisce:

- registrazione dei task
- callback
- intervalli
- enabled / disabled
- esecuzione temporizzata
- ciclo frontend
- sincronizzazione del ciclo

I principali task applicativi comprendono:

- Security / Sensors
- HVAC
- Averages
- Power
- Weather
- Communication


## 3.7 Communication Scheduler

Il Communication Scheduler è dedicato ai servizi di comunicazione.

Gestisce:

- Bridge
- MQTT
- WebAPI

Ogni servizio può avere:

- interval
- priority
- enabled / disabled
- callback

L'esecuzione avviene secondo un modello:

**timed round-robin.**

Il suo obiettivo è impedire che un singolo servizio di comunicazione
possa monopolizzare il runtime.


## 3.8 Frontend Engines

I motori applicativi comprendono:

### HVAC Engine

- zone
- setpoint
- fan coil
- valvole
- ACS
- anti-legionella
- defrost
- protezioni


### Power Engine

- carichi
- priorità
- limiti
- forecast solare
- protezioni
- auto-tuning


### Weather Engine

- temperatura
- pioggia
- vento
- luce
- moving averages
- eventi


### Security Engine

- PIR
- door
- window
- smoke
- flood
- zone
- alarm states
- eventi
- bitmask


## 3.9 Communication Services

### Bridge

- AEE
- UDP
- JSON
- eventi

### MQTT

- Home Assistant
- Zigbee2MQTT
- Shelly
- eventi
- stato

### WebAPI

- GET
- POST
- parsing
- buffer mapping
- request / response

Questi servizi vengono eseguiti dal Communication Scheduler.


## 3.10 RS485

RS485 gestisce comunicazioni seriali deterministiche attraverso:

- frame
- ACK / NACK
- retry
- state machine
- timeout
- gestione degli errori

Le operazioni devono essere non-blocking.


## 3.11 Hot Standby

Gestisce:

- MASTER
- SLAVE
- heartbeat
- failover
- state replication
- process synchronization
- Ethernet lifecycle


## 3.12 Diagnostica

La diagnostica analizza:

- Buffer
- task
- scheduler
- comunicazioni
- dispositivi
- Modbus
- RTC
- Security
- HVAC
- Power
- Hot Standby
- Ethernet
- socket
- watchdog
- overload


# ============================================================
# 4. DIAGRAMMA ASCII – DIPENDENZE FILE
# ============================================================


```text
domomanager.ino
│
├── DMUsb.hpp
│
├── DMFrontend.hpp
│   │
│   ├── DMFrontendEngines.hpp
│   │
│   ├── DMAutomation.hpp
│   │
│   ├── DMBridge.hpp
│   │
│   ├── DMPLC.hpp
│   │
│   ├── DMEquipment.hpp
│   │
│   ├── DMBuffers.hpp
│   │
│   ├── DMWeather.hpp
│   │
│   ├── DMSetup.hpp
│   │
│   ├── DMOptaRTC.hpp
│   │
│   ├── DMDiagnostic.hpp
│   │
│   ├── DMFncs.hpp
│   │
│   └── DMLogger.hpp
│
└── DMLogger.hpp
```

Il diagramma rappresenta le dipendenze architetturali principali.

Le dipendenze effettive tra file devono essere considerate
secondo gli `#include` presenti nel codice sorgente.


# ============================================================
# 5. DIAGRAMMA ASCII – ARCHITETTURA A BLOCCHI
# ============================================================


```text
                         ┌──────────────────────┐
                         │       HARDWARE       │
                         │  Opta / Arduino I/O  │
                         │  Modbus / RS485      │
                         │  Ethernet            │
                         └──────────┬───────────┘
                                    │
                                    ▼

┌────────────────────────────────────────────────────────────┐
│                     DOMOMANAGER CORE                       │
│                                                            │
│ Setup / Backend / Validation / Routing / Watchdog          │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                       BUFFER ENGINE                        │
│                  SINGLE SOURCE OF TRUTH                    │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                      DEVICE MANAGER                        │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                       MODBUS ENGINE                        │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                        TIME MANAGER                        │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                     AUTOMATION ENGINE                      │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                       TASK ENGINE                           │
│                                                            │
│  Security / HVAC / Averages / Power / Weather              │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

                    COMMUNICATION TASK
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                 COMMUNICATION SCHEDULER                    │
│                                                            │
│                   TIMED ROUND-ROBIN                        │
└──────────────────────────┬─────────────────────────────────┘
                           │
                ┌──────────┼──────────┐
                ▼          ▼          ▼
             BRIDGE       MQTT      WEBAPI


                ┌───────────────────────────┐
                │       DIAGNOSTICS         │
                │                           │
                │ Observes the whole       │
                │ runtime                  │
                └───────────────────────────┘
```


# ============================================================
# 6. DIAGRAMMA ASCII – FLUSSO DATI
# ============================================================


```text
                         MONDO FISICO
                              │
                              ▼
                         DEVICE LAYER
                              │
                              ▼
                    BACKEND / MODBUS / I/O
                              │
                              ▼
                    ╔════════════════════╗
                    ║       BUFFER       ║
                    ║ SINGLE SOURCE OF   ║
                    ║      TRUTH         ║
                    ╚════════╤═══════════╝
                             │
                 ┌───────────┼───────────┐
                 │           │           │
                 ▼           ▼           ▼
               TASKS     AUTOMATION  COMMUNICATION
                 │           │           │
                 │           │           ▼
                 │           │    COMMUNICATION
                 │           │      SCHEDULER
                 │           │           │
                 │           │      ┌────┼────┐
                 │           │      ▼    ▼    ▼
                 │           │   BRIDGE MQTT WEBAPI
                 │           │
                 └───────────┼───────────┘
                             │
                             ▼
                       BUFFER UPDATE
                             │
                             ▼
                       DEVICE OUTPUT
                             │
                             ▼
                        MONDO FISICO
```


# ============================================================
# 7. DIAGRAMMA ASCII – CICLO DI ESECUZIONE
# ============================================================


```text
LOOP()
  │
  ▼
BACKEND CYCLE
  │
  ├── Time Manager
  ├── Device Manager
  ├── Modbus / I/O
  └── Buffer Update
  │
  ▼
BACKEND CYCLE COMPLETED
  │
  ▼
FRONTEND / TASK ENGINE
  │
  ├── Security
  ├── HVAC
  ├── Averages
  ├── Power
  └── Weather
  │
  ▼
FRONTEND CYCLE COMPLETED
  │
  ├──────────────► AUTOMATION
  │                     │
  │                     ▼
  │                   BUFFER
  │
  ▼
COMMUNICATION TASK
  │
  ▼
COMMUNICATION SCHEDULER
  │
  ├── Bridge
  ├── MQTT
  └── WebAPI
  │
  ▼
FULL CYCLE
  │
  ▼
NEXT LOOP()
```

Il ciclo applicativo e il ciclo di comunicazione sono logicamente
separati.

Il Communication Scheduler non deve poter bloccare il ciclo
applicativo.


# ============================================================
# 8. MODELLO DI ESECUZIONE
# ============================================================


Il runtime segue quattro concetti fondamentali:

```text
BACKEND
   │
   ▼
BUFFER
   │
   ▼
APPLICATION
   │
   ▼
COMMUNICATION
```

con un ciclo di feedback:

```text
PHYSICAL WORLD
      │
      ▼
    BACKEND
      │
      ▼
    BUFFER
      │
      ▼
 APPLICATION
      │
      ▼
 AUTOMATION
      │
      ▼
    BUFFER
      │
      ▼
   OUTPUT
      │
      ▼
PHYSICAL WORLD
```

Le comunicazioni esterne rappresentano una diramazione controllata:

```text
BUFFER
   │
   ▼
COMMUNICATION TASK
   │
   ▼
COMMUNICATION SCHEDULER
   │
   ├── Bridge
   ├── MQTT
   └── WebAPI
```


# ============================================================
# 9. PRINCIPI OPERATIVI
# ============================================================


### 9.1 Determinismo

Il runtime deve avere tempi e responsabilità prevedibili.


### 9.2 Non-blocking

Nessun servizio secondario deve bloccare il ciclo principale.


### 9.3 Buffer-centric

Il Buffer è la fonte unica di verità.


### 9.4 Service isolation

Un servizio di comunicazione non deve poter monopolizzare
l'esecuzione.


### 9.5 Resource awareness

CPU, memoria, Ethernet, socket e connessioni sono risorse
finite e devono essere gestite esplicitamente.


### 9.6 Observability

Ogni comportamento significativo deve poter essere osservato
e diagnosticato.


### 9.7 Local-first

Le funzioni fondamentali della casa devono continuare a
funzionare senza cloud.


### 9.8 Embedded-first

L'architettura deve rispettare i limiti reali dell'hardware.


# ============================================================
# 10. OBIETTIVO FINALE
# ============================================================


DomoManager nasce per costruire una domotica:

- affidabile come un PLC
- pulita come un impianto elettrico
- leggibile come un manuale tecnico
- estendibile come un framework moderno
- prevedibile come un sistema industriale
- locale come un dispositivo embedded
- diagnosticabile in ogni sua parte

Non vuole sostituire necessariamente Home Assistant.

Può invece rappresentare un livello più basso e più deterministico
sul quale costruire integrazioni e interfacce superiori.

L'obiettivo è creare la base di una casa intelligente che continui
a funzionare anche quando i servizi esterni non funzionano.


# ============================================================
# DOMOMANAGER
# THE OPERATING SYSTEM FOR THE HOME
# ============================================================