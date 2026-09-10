# ============================================================
# DOMOMANAGER – DIAGRAMMA DELLE DIPENDENZE DEI FILE
# ============================================================

## 1. Scopo del documento

Questo documento descrive la struttura delle dipendenze tra i file
principali di DomoManager.

Il punto di partenza è:

```text
domomanager.ino
```

Il diagramma non rappresenta soltanto gli `#include`.

Rappresenta anche la suddivisione logica del progetto:

```text
APPLICATION
     │
     ▼
RUNTIME
     │
     ├── BUFFER
     ├── TASK ENGINE
     ├── AUTOMATION
     ├── FRONTEND ENGINES
     └── COMMUNICATION
              │
              ▼
          HARDWARE
```

L'obiettivo architetturale è evitare dipendenze circolari e mantenere
una direzione chiara delle dipendenze.

---

# 2. Entry Point

Il punto di ingresso del firmware è:

```text
domomanager.ino
```

Il file `.ino` inizializza e avvia il runtime DomoManager.

Dipendenze principali:

```text
domomanager.ino
│
├── DMUsb.hpp
├── DMFrontend.hpp
└── DMLogger.hpp
```

Le dipendenze specifiche vengono successivamente risolte dalla catena
degli header.

---

# 3. Albero principale delle dipendenze

```text
domomanager.ino
│
├── DMUsb.hpp
│   │
│   ├── DMLogger.hpp
│   │   ├── DMDeclares.h
│   │   └── Arduino.h
│   │
│   ├── DMFrontend.hpp
│   └── DMDeclares.h
│
├── DMFrontend.hpp
│   │
│   ├── DMFrontendEngines.hpp
│   │   │
│   │   ├── DMHVAC.hpp
│   │   │   ├── DMHVAC.cpp
│   │   │   ├── DMDeclares.h
│   │   │   ├── DMLogger.hpp
│   │   │   └── DMFncs.hpp
│   │   │
│   │   ├── DMPower.hpp
│   │   │   ├── DMDeclares.h
│   │   │   ├── DMLogger.hpp
│   │   │   └── DMFncs.hpp
│   │   │
│   │   ├── DMWeather.hpp
│   │   │   ├── DMDeclares.h
│   │   │   ├── DMLogger.hpp
│   │   │   └── DMFncs.hpp
│   │   │
│   │   ├── DMWiredSensors.hpp
│   │   │   ├── DMDeclares.h
│   │   │   ├── DMLogger.hpp
│   │   │   └── DMFncs.hpp
│   │   │
│   │   ├── DMMQTTEngine.hpp
│   │   │   ├── DMDeclares.h
│   │   │   ├── DMLogger.hpp
│   │   │   └── DMFncs.hpp
│   │   │
│   │   ├── DMWebAPI.hpp
│   │   │   ├── DMWebAPIDefs.hpp
│   │   │   ├── DMDeclares.h
│   │   │   ├── DMLogger.hpp
│   │   │   └── DMFncs.hpp
│   │   │
│   │   ├── DMRS485Node.hpp
│   │   │   ├── DMDeclares.h
│   │   │   ├── DMLogger.hpp
│   │   │   └── DMFncs.hpp
│   │   │
│   │   └── DMAdapters.hpp
│   │       ├── DMDeclares.h
│   │       ├── DMLogger.hpp
│   │       └── DMFncs.hpp
│   │
│   ├── DMAutomation.hpp
│   │   │
│   │   ├── DMAutomationBuilder.hpp
│   │   │   ├── DMDeclares.h
│   │   │   ├── DMLogger.hpp
│   │   │   └── DMFncs.hpp
│   │   │
│   │   ├── DMDeclares.h
│   │   ├── DMLogger.hpp
│   │   └── DMFncs.hpp
│   │
│   ├── DMBridge.hpp
│   │   ├── DMLogger.hpp
│   │   ├── DMDeclares.h
│   │   └── DMFncs.hpp
│   │
│   ├── DMPLC.hpp
│   │   │
│   │   ├── MgsModbus.hpp
│   │   │   ├── Ethernet.h
│   │   │   ├── Arduino.h
│   │   │   └── DMLogger.hpp
│   │   │
│   │   ├── DMDeclares.h
│   │   ├── DMLogger.hpp
│   │   └── DMFncs.hpp
│   │
│   ├── DMEquipment.hpp
│   │   ├── DMDeclares.h
│   │   ├── DMLogger.hpp
│   │   └── DMFncs.hpp
│   │
│   ├── DMBuffers.hpp
│   │   ├── DMBuffers.cpp
│   │   ├── DMDeclares.h
│   │   ├── DMLogger.hpp
│   │   └── DMFncs.hpp
│   │
│   ├── DMWeather.hpp
│   ├── DMSetup.hpp
│   ├── DMOptaRTC.hpp
│   ├── DMDiagnostic.hpp
│   ├── DMFncs.hpp
│   ├── DMLogger.hpp
│   └── DMDeclares.h
│
├── DMLogger.hpp
│   ├── DMDeclares.h
│   └── Arduino.h
│
└── External / Platform Dependencies
    ├── Arduino Core
    ├── Ethernet
    ├── Wire
    ├── SPI
    └── Opta / Arduino libraries
```

---

# 4. DMFrontend

`DMFrontend.hpp` rappresenta uno dei principali punti di aggregazione
del sistema.

La sua responsabilità è mettere a disposizione il runtime frontend e
i relativi componenti.

```text
DMFrontend.hpp
│
├── Frontend Engines
├── Automation
├── Bridge
├── PLC / Modbus
├── Equipment
├── Buffer
├── Setup
├── RTC
├── Diagnostics
└── Common utilities
```

Questo file rappresenta quindi una **facade del sottosistema
frontend**.

---

# 5. Frontend Engines

Il gruppo `DMFrontendEngines.hpp` raccoglie gli Engine specializzati.

```text
DMFrontendEngines.hpp
│
├── DMHVAC.hpp
├── DMPower.hpp
├── DMWeather.hpp
├── DMWiredSensors.hpp
├── DMMQTTEngine.hpp
├── DMWebAPI.hpp
├── DMRS485Node.hpp
└── DMAdapters.hpp
```

Il principio architetturale è:

```text
Frontend Engine
      │
      ▼
    Buffer
      │
      ▼
 Application State
```

Gli Engine non dovrebbero diventare dipendenti direttamente gli uni
dagli altri.

---

# 6. HVAC

```text
DMHVAC.hpp
│
├── DMHVAC.cpp
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

Il modulo HVAC utilizza le definizioni comuni e gli strumenti
funzionali del progetto.

La sua responsabilità rimane confinata alla gestione HVAC.

Il modulo non dovrebbe contenere dipendenze dirette verso MQTT,
WebAPI o altri Engine di comunicazione.

---

# 7. Power

```text
DMPower.hpp
│
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

Il Power Engine gestisce la logica energetica.

Il suo modello di dipendenza deve rimanere orientato verso:

```text
Buffer
  │
  ▼
Power Engine
  │
  ▼
Buffer
```

e non verso specifici protocolli esterni.

---

# 8. Weather

```text
DMWeather.hpp
│
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

Il Weather Engine elabora i dati meteorologici e produce stato ed
eventi utilizzabili dagli altri componenti.

La dipendenza ideale rimane:

```text
Weather
   │
   ▼
Buffer
```

---

# 9. Wired Sensors / Security

```text
DMWiredSensors.hpp
│
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

Il modulo gestisce l'acquisizione e l'elaborazione dei sensori cablati.

La logica di sicurezza viene successivamente resa disponibile al
resto del sistema attraverso il Buffer e il sistema eventi.

---

# 10. MQTT

```text
DMMQTTEngine.hpp
│
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

MQTT appartiene al **Communication Layer**.

La sua dipendenza concettuale principale è:

```text
Buffer
   │
   ▼
MQTT Engine
   │
   ▼
MQTT Broker
```

e nel percorso inverso:

```text
MQTT Broker
   │
   ▼
MQTT Engine
   │
   ▼
Buffer
```

MQTT non dovrebbe diventare una dipendenza della logica HVAC,
Security, Power o Automation.

---

# 11. WebAPI

```text
DMWebAPI.hpp
│
├── DMWebAPIDefs.hpp
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

Il WebAPI Engine appartiene anch'esso al Communication Layer.

```text
Buffer
   │
   ▼
WebAPI Engine
   │
   ▼
HTTP
```

La presenza di `DMWebAPIDefs.hpp` permette di mantenere separate le
definizioni specifiche del protocollo dal resto delle dichiarazioni
comuni.

---

# 12. RS485

```text
DMRS485Node.hpp
│
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

RS485 rappresenta il livello di comunicazione industriale.

Il modello operativo è basato su una macchina a stati:

```text
IDLE
 │
 ▼
SEND
 │
 ▼
WAIT
 │
 ├── ACK ──► COMPLETE
 │
 └── TIMEOUT ──► RETRY
```

Il protocollo non deve richiedere un blocco del runtime.

---

# 13. Adapters

```text
DMAdapters.hpp
│
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

Gli Adapter rappresentano il livello di adattamento tra componenti
che utilizzano interfacce differenti.

Il loro ruolo è evitare che la logica applicativa debba conoscere i
dettagli implementativi dell'interfaccia sottostante.

---

# 14. Automation

```text
DMAutomation.hpp
│
├── DMAutomationBuilder.hpp
│   ├── DMDeclares.h
│   ├── DMLogger.hpp
│   └── DMFncs.hpp
│
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

L'Automation Engine utilizza il Buffer come modello dello stato.

La configurazione viene trasformata dal Builder in strutture interne
utilizzabili dal runtime.

```text
JSON / CONFIG
     │
     ▼
AutomationBuilder
     │
     ▼
Automation Runtime
     │
     ▼
Buffer
```

---

# 15. Buffer

```text
DMBuffers.hpp
│
├── DMBuffers.cpp
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

Il Buffer è una delle componenti centrali dell'architettura.

La dipendenza concettuale dovrebbe essere:

```text
Backend
   │
   ▼
Buffer
   │
   ├── Automation
   ├── HVAC
   ├── Power
   ├── Weather
   ├── Security
   ├── MQTT
   └── WebAPI
```

Il Buffer deve quindi essere mantenuto il più possibile indipendente
dai singoli Engine applicativi.

---

# 16. PLC / Modbus

```text
DMPLC.hpp
│
└── MgsModbus.hpp
    │
    ├── Ethernet.h
    ├── Arduino.h
    └── DMLogger.hpp
```

Questo ramo collega il runtime al livello hardware e di rete.

La catena è:

```text
Device Manager
      │
      ▼
    DMPLC
      │
      ▼
 MgsModbus
      │
      ▼
 Ethernet / Hardware
```

Il livello Modbus è responsabile della comunicazione con i dispositivi,
non della logica applicativa superiore.

---

# 17. Equipment

```text
DMEquipment.hpp
│
├── DMDeclares.h
├── DMLogger.hpp
└── DMFncs.hpp
```

Il modulo Equipment rappresenta il livello di gestione delle
apparecchiature.

Può essere utilizzato dal runtime per mantenere la separazione tra
identificazione/configurazione del dispositivo e logica applicativa.

---

# 18. Setup

```text
DMSetup.hpp
```

Il Setup rappresenta la fase di costruzione del runtime.

Il flusso concettuale è:

```text
BOOT
 │
 ▼
CONFIG
 │
 ▼
VALIDATION
 │
 ▼
BUFFER
 │
 ▼
DEVICES
 │
 ▼
ENGINES
 │
 ▼
TASKS
 │
 ▼
COMMUNICATION
 │
 ▼
RUNTIME
```

Il Setup non dovrebbe contenere la logica operativa dei singoli Engine.

---

# 19. RTC

```text
DMOptaRTC.hpp
```

Il modulo RTC fornisce l'accesso alla sorgente temporale hardware.

La dipendenza concettuale è:

```text
RTC Hardware
     │
     ▼
Opta RTC
     │
     ▼
Time Manager
     │
     ├── Task Engine
     ├── Automation
     ├── HVAC
     ├── Power
     └── Diagnostics
```

Il tempo è quindi una dipendenza trasversale, non un sottosistema
applicativo.

---

# 20. Diagnostics

```text
DMDiagnostic.hpp
```

La diagnostica è trasversale a tutti i domini.

```text
                    Diagnostics
                         │
       ┌─────────────────┼─────────────────┐
       ▼                 ▼                 ▼
     Buffer             Tasks        Communication
       │                 │                 │
       ▼                 ▼                 ▼
   Devices           Scheduler          Network
       │                 │                 │
       └─────────────────┼─────────────────┘
                         ▼
                       REPORT
```

La diagnostica deve poter osservare il sistema senza diventare
dipendente dalla logica interna di ogni Engine.

---

# 21. Common Definitions

## DMDeclares.h

`DMDeclares.h` contiene definizioni condivise utilizzate da più moduli.

Rappresenta uno dei livelli più bassi delle dipendenze applicative.

```text
DMDeclares.h
      ▲
      │
 ┌────┼────┬─────────┐
 │    │    │         │
HVAC Power Weather Security ...
```

È importante evitare che questo file diventi un contenitore indiscriminato
di logica o dipendenze.

---

# 22. DMFncs.hpp

`DMFncs.hpp` contiene funzionalità comuni utilizzate da più componenti.

Il suo ruolo dovrebbe rimanere quello di fornire utility generiche.

```text
DMFncs.hpp
      ▲
      │
 ┌────┼───────────────┐
 │    │               │
HVAC Automation     MQTT ...
```

Le funzioni inserite in questo livello dovrebbero rimanere indipendenti
dalla logica specifica degli Engine.

---

# 23. DMLogger

```text
DMLogger.hpp
│
├── DMDeclares.h
└── Arduino.h
```

Il Logger rappresenta un servizio trasversale.

È utilizzato da:

- Buffer
- Engine
- Automation
- Modbus
- MQTT
- WebAPI
- Diagnostics
- Setup

Il Logger non dovrebbe dipendere dagli Engine applicativi.

La direzione corretta è:

```text
Engine
   │
   ▼
Logger
```

non:

```text
Logger
   │
   ▼
Engine
```

---

# 24. Dipendenze hardware

Le dipendenze verso Arduino e le librerie hardware si trovano
alla base del sistema.

```text
Arduino Core
│
├── Arduino.h
├── Wire
├── SPI
└── Ethernet
```

Queste librerie vengono utilizzate dai livelli che necessitano
dell'accesso hardware.

La direzione generale è:

```text
APPLICATION
     │
     ▼
DOMOMANAGER
     │
     ▼
HARDWARE ABSTRACTION
     │
     ▼
ARDUINO / LIBRARIES
     │
     ▼
HARDWARE
```

---

# 25. Architettura delle dipendenze

Il modello desiderato può essere rappresentato così:

```text
                    APPLICATION
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
     AUTOMATION       FRONTEND       SERVICES
          │              │              │
          └──────────────┼──────────────┘
                         ▼
                       BUFFER
                         ▲
                         │
                    BACKEND
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
       DEVICES        MODBUS           I/O
                         │
                         ▼
                      HARDWARE
```

Il Communication Layer opera lateralmente:

```text
                         BUFFER
                            │
                            ▼
                COMMUNICATION SCHEDULER
                            │
             ┌──────────────┼──────────────┐
             ▼              ▼              ▼
           MQTT          WebAPI          Bridge
```

Questo è importante perché impedisce al Communication Layer di
diventare una dipendenza della logica applicativa.

---

# 26. Task Engine e dipendenze

Il Task Engine rappresenta un livello di orchestrazione.

```text
TaskEngineOrchestrator
        │
        ├── Security
        ├── HVAC
        ├── Averages
        └── Communication
```

Il Task Engine non deve contenere la logica degli Engine.

Deve invece sapere:

- quale task eseguire
- quando eseguirlo
- se è abilitato
- quale callback chiamare

Questo mantiene separati:

**scheduling**

da:

**business logic**.

---

# 27. Communication Scheduler e dipendenze

Il Communication Scheduler rappresenta un secondo livello di
orchestrazione.

```text
CommunicationScheduler
        │
        ├── Communication_Bridge
        ├── Communication_MQTT
        └── Communication_WebAPI
```

Il suo compito è gestire:

- interval
- priority
- enabled
- ordine di esecuzione
- distribuzione temporale

Non implementa direttamente MQTT, WebAPI o Bridge.

Li orchestra.

---

# 28. Dipendenze runtime

Le dipendenze di compilazione non rappresentano necessariamente le
dipendenze runtime.

Per esempio:

```text
DMMQTTEngine.hpp
```

può essere incluso da un header comune, ma questo non significa che
HVAC debba dipendere runtime da MQTT.

È quindi importante distinguere:

### Compile-time dependency

Un file include un altro file.

### Architectural dependency

Un modulo conosce e utilizza un altro modulo.

### Runtime dependency

Un componente deve essere attivo affinché un altro possa funzionare.

Questa distinzione è fondamentale per mantenere l'architettura
comprensibile.

---

# 29. Regola anti-circular-dependency

Il progetto dovrebbe evitare strutture del tipo:

```text
HVAC
  ↓
MQTT
  ↓
Automation
  ↓
HVAC
```

o:

```text
Buffer
  ↓
Engine
  ↓
Buffer
```

quando questo introduce dipendenze di compilazione circolari.

Il modello preferenziale è:

```text
             BUFFER
            ▲     ▲
            │     │
        FRONTEND  AUTOMATION
            │     │
            └──┬──┘
               │
          COMMUNICATION
```

Il Buffer rappresenta il punto comune, non un componente che dipende
dagli Engine.

---

# 30. Dependency Layers

La struttura ideale del progetto può essere rappresentata in livelli.

```text
LAYER 0
────────────────────────────────
Arduino / Hardware Libraries


LAYER 1
────────────────────────────────
DMDeclares
DMFncs
DMLogger


LAYER 2
────────────────────────────────
Buffer
Device Manager
Modbus
RTC
Equipment


LAYER 3
────────────────────────────────
Automation
Frontend Engines


LAYER 4
────────────────────────────────
Task Engine
Communication Scheduler


LAYER 5
────────────────────────────────
DomoManager Core
Setup
Application


LAYER 6
────────────────────────────────
domomanager.ino
```

La comunicazione può essere considerata un dominio laterale:

```text
              COMMUNICATION
                    │
        ┌───────────┼───────────┐
        ▼           ▼           ▼
      Bridge       MQTT       WebAPI
                    │
                    ▼
                  Buffer
```

---

# 31. Dipendenze complessive

Il progetto può quindi essere letto secondo questa catena:

```text
domomanager.ino
       │
       ▼
DomoManager Core
       │
       ├───────────────┐
       ▼               ▼
     Setup          Runtime
                       │
          ┌────────────┼────────────┐
          ▼            ▼            ▼
       Backend       Buffer       Services
          │            │            │
          │            │            ├── MQTT
          │            │            ├── WebAPI
          │            │            └── Bridge
          │            │
          └──────┬─────┘
                 │
                 ▼
             Task Engine
                 │
        ┌────────┼────────┐
        ▼        ▼        ▼
      HVAC    Security  Averages
        │        │        │
        └────────┼────────┘
                 ▼
            Automation
                 │
                 ▼
               Buffer
```

---

# 32. Principio finale delle dipendenze

L'obiettivo dell'architettura non è avere il minor numero possibile di
`#include`.

L'obiettivo è avere **dipendenze comprensibili**.

La regola fondamentale è:

> **un modulo può dipendere dalle fondamenta del sistema, ma non deve
> conoscere inutilmente i dettagli dei moduli che gli stanno sopra o
> lateralmente.**

In particolare:

```text
Hardware
   ↓
Backend
   ↓
Buffer
   ↓
Logic
   ↓
Scheduling
   ↓
Communication
```

deve rimanere concettualmente distinto da:

```text
Logger
Diagnostics
Time
Watchdog
HotStandby
```

che operano trasversalmente.

---

# 33. Obiettivo architetturale

La struttura ideale delle dipendenze DomoManager è:

```text
             ┌──────────────────────┐
             │      APPLICATION     │
             └──────────┬───────────┘
                        │
                        ▼
             ┌──────────────────────┐
             │        RUNTIME       │
             │ Task / Automation   │
             └──────────┬───────────┘
                        │
                        ▼
             ┌──────────────────────┐
             │        BUFFER        │
             └──────────┬───────────┘
                        │
            ┌───────────┴───────────┐
            ▼                       ▼
       ┌──────────┐          ┌──────────────┐
       │ BACKEND  │          │ COMMUNICATION│
       └────┬─────┘          └──────┬───────┘
            │                       │
            ▼                       ▼
        HARDWARE                 EXTERNAL
```

Attorno a questi livelli:

```text
       TIME
       DIAGNOSTICS
       WATCHDOG
       HOT STANDBY
```

forniscono i servizi trasversali necessari al funzionamento del
runtime.

---

# ============================================================
# DOMOMANAGER
# FILE DEPENDENCY ARCHITECTURE
# ============================================================