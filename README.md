<img width="953" height="229" alt="DomoManager Logo" src="https://github.com/user-attachments/assets/eb342960-48e6-4788-a0da-986a7873fbc5" />

# DomoManager

**Il sistema operativo per la casa progettato con una filosofia industriale.**

DomoManager nasce con un obiettivo preciso: portare la domotica dal modello dei **gadget wireless** a quello di una **infrastruttura domestica affidabile, deterministica, cablata e diagnosticabile**.

È progettato per realizzare sistemi domestici:

- affidabili
- deterministici
- cablati
- a ridotta dipendenza dalla radio
- leggibili
- estendibili
- indipendenti dal cloud

DomoManager non vuole essere semplicemente un controller domotico.

È un **runtime embedded per l'impiantistica domestica**, progettato per coordinare dispositivi, automazioni, energia, climatizzazione, sicurezza e comunicazioni in un'unica architettura.

---

# 🔗 Link ufficiali

### 🌐 Sito web

**https://www.domo-manager.it**

### 📧 Contatti

**mail@domo-manager.it**

### 📘 Facebook

**https://www.facebook.com/people/DOMO-Manager/61573260488406**

**https://www.facebook.com/groups/1310697874371245/**

### 📄 Documentazione

**https://www.domo-manager.it/biblioteca-documenti/**

---

# 🎯 Missione

> **Trasformare la domotica da un insieme di gadget wireless a un'infrastruttura affidabile, cablata e industriale.**

La filosofia di DomoManager parte da un principio semplice:

**le funzioni fondamentali della casa non devono dipendere dal cloud, dal Wi-Fi o da una catena di servizi esterni.**

Per questo il progetto privilegia:

- I/O cablati
- RS485
- Modbus RTU
- Modbus TCP
- Ethernet
- protocolli deterministici
- elaborazione locale

La radio può essere utilizzata quando è utile, ma non rappresenta il fondamento dell'impianto.

---

# 🧠 Cosa rende DomoManager diverso?

## Determinismo

DomoManager è progettato attorno a cicli prevedibili e a una precisa separazione delle responsabilità.

Il runtime evita, dove possibile:

- comportamenti impliciti
- allocazioni dinamiche ricorrenti
- blocchi prolungati
- dipendenze nascoste tra servizi

L'obiettivo non è semplicemente eseguire più operazioni possibile, ma eseguirle in modo **prevedibile e controllabile**.

---

## Buffer-centric architecture

Il **Buffer Engine** rappresenta il punto centrale dello stato applicativo.

Il modello fondamentale è:

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
FRONTEND       AUTOMATION
   │              │
   └──────┬───────┘
          ▼
 COMMUNICATION
```

Il Buffer fornisce:

- aree tipizzate
- timestamp
- change tracking
- virtual areas
- reverse
- split
- toggle
- accesso rapido
- diagnostica

Questo permette ai vari Engine di lavorare sullo stesso stato coerente del sistema.

---

# ⚙️ Runtime e Task Engine

Il frontend utilizza un sistema di orchestrazione centralizzato.

I task applicativi vengono registrati con:

- callback
- intervallo
- stato enabled/disabled

Tra i task standard rientrano:

- Security / Sensors
- HVAC
- Averages
- Communication

La separazione tra task e comunicazioni permette di mantenere indipendenti la logica applicativa e il networking.

---

# 🔄 Communication Scheduler

I servizi di comunicazione vengono gestiti attraverso uno scheduler dedicato.

```text
             COMMUNICATION SCHEDULER
                       │
          ┌────────────┼────────────┐
          ▼            ▼            ▼
       BRIDGE        MQTT        WebAPI
```

Ogni servizio può avere:

- intervallo di esecuzione
- priorità
- stato enabled
- callback dedicata

Il modello **round-robin temporizzato** evita che un singolo servizio di comunicazione possa monopolizzare il ciclo frontend.

Questo è particolarmente importante in sistemi embedded dove CPU, memoria, connessioni e socket Ethernet sono risorse limitate.

### Principio fondamentale

> **Nessun servizio secondario deve poter bloccare il ciclo applicativo principale.**

---

# 🏗 Architettura

L'architettura generale può essere rappresentata così:

```text
                         MONDO FISICO
                              │
                              ▼
                      ┌────────────────┐
                      │ Device Manager │
                      └───────┬────────┘
                              │
                              ▼
                      ┌────────────────┐
                      │  Modbus /     │
                      │  RS485 / I/O  │
                      └───────┬────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │    BUFFER ENGINE   │
                    │  Single Source of  │
                    │       Truth        │
                    └─────────┬──────────┘
                              │
                 ┌────────────┴────────────┐
                 │                         │
                 ▼                         ▼
          ┌──────────────┐         ┌──────────────┐
          │ Task Engine  │         │ Automation   │
          │              │         │    Engine    │
          └──────┬───────┘         └──────┬───────┘
                 │                        │
        ┌────────┼────────┐               │
        ▼        ▼        ▼               │
      HVAC    Security  Averages          │
        │        │        │               │
        └────────┴────────┴───────┬───────┘
                                   ▼
                      ┌────────────────────┐
                      │ Communication      │
                      │    Scheduler       │
                      └─────────┬──────────┘
                                │
                   ┌────────────┼────────────┐
                   ▼            ▼            ▼
                Bridge        MQTT         WebAPI
```

Il principio è:

**Backend → Buffer → Frontend → Automation → Communication**

con una netta separazione tra elaborazione applicativa e servizi di comunicazione.

---

# 🏠 Frontend Engines

## HVAC Engine

Gestione avanzata della climatizzazione:

- pompe di calore
- zone multiple
- ACS
- anti-legionella
- defrost
- temperatura interna/esterna
- sicurezza outdoor
- gestione finestre
- calendari HVAC

---

## ⚡ Power Engine

Gestione intelligente dell'energia:

- carichi prioritari
- protezione della rete
- forecast solare
- gestione dinamica dei carichi
- auto-tuning

---

## 🌦 Weather Engine

Elaborazione dei dati meteorologici:

- temperatura
- pioggia
- vento
- luminosità
- eventi meteo
- allarmi
- medie mobili O(1)

---

## 🛡 Security Engine

Gestione della sicurezza:

- sensori cablati
- zone
- stati di allarme
- aggregazione dello stato
- eventi
- diagnostica

---

# 🤖 Automation Engine

Le automazioni sono dichiarative e configurabili.

Sono supportati:

- scene
- regole
- condizioni multiple
- sequenze temporizzate
- scheduled rules
- trend
- debounce
- composite rules
- configurazione JSON
- validazione

L'obiettivo è evitare automazioni hardcoded e rendere il comportamento del sistema:

**leggibile, prevedibile e diagnosticabile.**

---

# 🌐 Integrazioni

## MQTT

DomoManager può integrarsi con ecosistemi esterni attraverso MQTT, inclusi:

- Home Assistant
- Zigbee2MQTT
- Shelly

DomoManager non vuole sostituire Home Assistant.

L'obiettivo è fornire un **backend locale industriale** che possa lavorare insieme a Home Assistant.

---

## WebAPI

Il WebAPI Engine fornisce comunicazione HTTP con gestione orientata al modello non bloccante.

Supporta:

- GET
- POST
- profili messaggi
- correlazione request/response

---

## Modbus

Supporto a:

- Modbus RTU
- Modbus TCP

con:

- polling round-robin
- retry deterministici
- gestione timeout
- scritture controllate
- gestione errori

---

## RS485

Comunicazione seriale orientata a sistemi embedded industriali:

- frame
- ACK/NACK
- retry
- state machine
- gestione non bloccante

---

# 🔁 Hot Standby

DomoManager supporta un'architettura Master/Slave.

Il sistema può gestire:

- stato Master
- stato Slave
- failover
- replica dello stato
- replica dei processi
- timestamp

Il nodo attivo gestisce le comunicazioni operative mentre il nodo secondario rimane pronto al subentro.

---

# 🔍 Diagnostica industriale

La diagnostica è parte integrante dell'architettura.

DomoManager permette di analizzare:

- Buffer
- automazioni
- scheduler
- task
- comunicazioni
- split
- dispositivi
- RTC
- sicurezza
- HVAC
- Power
- Weather
- watchdog
- watch areas

L'obiettivo è poter rispondere non soltanto alla domanda:

**"Cosa non funziona?"**

ma soprattutto:

**"Perché non funziona?"**

---

# 📊 Principi di performance

DomoManager è progettato per hardware embedded con risorse limitate.

Tra le tecniche utilizzate:

- lookup O(1) nel Buffer
- medie mobili O(1)
- scheduling temporizzato
- round-robin
- riduzione delle operazioni ridondanti
- controllo delle allocazioni dinamiche
- separazione dei servizi
- gestione non bloccante delle comunicazioni

La performance viene considerata insieme a:

**determinismo + affidabilità + prevedibilità.**

---

# 🧰 Hardware e filosofia embedded

DomoManager è progettato pensando a controller embedded e sistemi con risorse limitate.

Questo implica una particolare attenzione a:

- memoria
- CPU
- socket Ethernet
- connessioni contemporanee
- timeout
- watchdog
- gestione degli errori
- comportamento durante il failover

La disponibilità delle risorse hardware deve essere considerata parte dell'architettura software.

---

# 🧪 Stato del progetto

## BETA

DomoManager è attualmente in fase **Beta**.

L'architettura principale è stata consolidata, ma alcune aree sono ancora in evoluzione.

In particolare:

- documentazione HVAC/Power
- profili WebAPI
- auto-tuning Power
- tuning del Communication Scheduler
- diagnostica avanzata delle risorse Ethernet
- test HotStandby
- test di carico
- strumenti grafici di configurazione

La Beta deve quindi essere considerata una base solida per lo sviluppo verso la **1.0**, non ancora una release finale.

---

# 🗺 Roadmap

Gli obiettivi principali verso RC1 comprendono:

- editor JSON ufficiale
- dashboard diagnostica
- diagnostica dei tempi di esecuzione
- diagnostica delle risorse Ethernet
- monitoraggio socket
- ulteriori profili WebAPI
- miglioramento forecast solare
- ottimizzazione ACS / anti-legionella
- supporto a ulteriori modelli di pompe di calore
- test estesi RS485
- test estesi HotStandby
- test di carico Ethernet

Per maggiori dettagli consultare **ROADMAP.md**.

---

# 📚 Documentazione

La documentazione tecnica comprende:

- filosofia del progetto
- architettura generale
- architettura runtime
- Buffer Engine
- Task Engine
- Communication Scheduler
- Automation Engine
- HVAC
- Power
- Weather
- Security
- Modbus
- RS485
- MQTT
- WebAPI
- HotStandby
- diagnostica

La documentazione pubblica è disponibile sul sito ufficiale:

**https://www.domo-manager.it/biblioteca-documenti/**

---

# 🤝 Contribuire

Vuoi contribuire allo sviluppo di DomoManager?

Consulta:

- **CONTRIBUTING.md**
- **CODE_OF_CONDUCT.md**
- **ROADMAP.md**

I contributi sono benvenuti quando coerenti con i principi architetturali del progetto.

### Principi fondamentali

> **Deterministico.**  
> **Non bloccante.**  
> **Modulare.**  
> **Diagnostico.**  
> **Embedded-first.**  
> **Local-first.**

---

# 📜 Licenza

Per informazioni sulla licenza consultare il file **LICENSE** presente nel repository.

---

# ============================================================
# DOMOMANAGER
# IL SISTEMA OPERATIVO PER LA CASA
# ============================================================