# ============================================================
# DOMOMANAGER – CHANGELOG
# ============================================================

# Beta – Architecture & Runtime Stabilization

Questa release rappresenta un'importante evoluzione della **Beta pubblica di DomoManager**.

L'obiettivo principale di questa fase non è soltanto aggiungere nuove funzionalità, ma consolidare l'architettura runtime rendendo il sistema:

- più deterministico
- più prevedibile
- più non bloccante
- più efficiente nell'utilizzo delle risorse
- più facilmente diagnosticabile
- più adatto all'esecuzione su hardware embedded con risorse limitate

Particolare attenzione è stata dedicata alla gestione del ciclo frontend e delle comunicazioni Ethernet, con l'introduzione di una schedulazione separata dei servizi di comunicazione.

---

# 1. Novità principali

## 1.1 Task Engine – nuova architettura di orchestrazione

È stata introdotta una struttura centralizzata per la gestione dei task frontend.

Il nuovo modello separa:

- definizione dei task
- configurazione
- scheduling
- esecuzione
- rilevamento del completamento del ciclo frontend

### Task standard gestiti

- Security / Sensors
- HVAC
- Averages
- Communication

Ogni task può essere configurato tramite:

- intervallo di esecuzione
- stato enabled/disabled
- callback dedicata

### Vantaggi

La logica applicativa non dipende più direttamente dal `loop()` principale.

Il frontend può quindi eseguire più servizi con una politica di scheduling uniforme e prevedibile.

---

## 1.2 Communication Scheduler

È stato introdotto un **Communication Scheduler dedicato** ai servizi di comunicazione.

I servizi vengono trattati come attività indipendenti:

- Bridge
- MQTT
- WebAPI

Ogni servizio dispone di:

- intervallo minimo
- priorità
- stato enabled
- callback di esecuzione

Il scheduler utilizza un modello **round-robin temporizzato**, evitando che un singolo servizio possa monopolizzare il ciclo frontend.

### Esempio

La comunicazione viene ora organizzata concettualmente come:

```text
Frontend Task
     |
     v
Communication Scheduler
     |
     +---- Bridge
     |
     +---- MQTT
     |
     +---- WebAPI
```

anziché eseguire tutti i servizi direttamente all'interno dello stesso percorso applicativo.

### Impatto

Questa modifica riduce il rischio che un servizio di comunicazione occupi eccessivamente il runtime disponibile agli altri componenti.

È particolarmente importante su piattaforme embedded con risorse Ethernet/socket limitate.

---

## 1.3 Comunicazioni non bloccanti

L'architettura dei servizi di comunicazione è stata ulteriormente orientata verso l'esecuzione **non bloccante**.

L'obiettivo è evitare che:

- Bridge
- MQTT
- WebAPI
- HMI
- Modbus

possano ostacolarsi reciprocamente durante l'esecuzione del ciclo frontend.

Le comunicazioni vengono quindi distribuite nel tempo dal scheduler invece di essere considerate un'unica attività monolitica.

### Obiettivo architetturale

```text
Backend
   |
   v
Buffer
   |
   v
Frontend Tasks
   |
   +--> HVAC
   +--> Security
   +--> Averages
   |
   v
Communication Scheduler
   |
   +--> Bridge
   +--> MQTT
   +--> WebAPI
```

Il principio fondamentale rimane:

> Nessun servizio di comunicazione deve poter bloccare il ciclo applicativo principale.

---

## 1.4 Gestione delle risorse Ethernet

La nuova architettura tiene conto esplicitamente dei vincoli hardware delle piattaforme embedded.

In particolare, l'utilizzo simultaneo di:

- HMI
- MQTT
- Modbus TCP
- Bridge
- WebAPI

può comportare una forte pressione sulle risorse socket disponibili.

La schedulazione dei servizi permette di ridurre la concorrenza temporale delle operazioni di rete e di rendere più controllabile l'utilizzo delle connessioni.

### Obiettivo

Evitare scenari nei quali un servizio secondario possa causare:

- timeout HMI
- perdita di connessioni
- retry eccessivi
- saturazione dei socket
- degrado generale del frontend

La gestione delle risorse di rete diventa quindi parte integrante dell'architettura runtime.

---

# 2. Evoluzione del ciclo frontend

## 2.1 Ciclo frontend deterministico

Il ciclo frontend è ora organizzato secondo una sequenza più esplicita:

```text
Backend Cycle
      |
      v
Buffer Update
      |
      v
Frontend Tasks
      |
      v
Communication Scheduling
      |
      v
Frontend Cycle Complete
```

Il completamento del ciclo frontend continua a essere esposto attraverso:

- `hasFrontendCycleCompleted()`
- `resetFrontendCycleFlag()`

Questo consente al runtime superiore di sincronizzare in modo deterministico backend e frontend.

---

## 2.2 Separazione tra logica applicativa e comunicazione

La logica applicativa non deve più occuparsi direttamente della politica di scheduling delle comunicazioni.

Ad esempio:

- HVAC gestisce HVAC
- Security gestisce sicurezza
- Averages gestisce medie
- Bridge gestisce bridge
- MQTT gestisce MQTT
- WebAPI gestisce WebAPI

La decisione di **quando** eseguire i servizi di comunicazione viene delegata al `CommunicationScheduler`.

Questa separazione rende il codice più modulare e facilita future ottimizzazioni.

---

# 3. Evoluzione dei Frontend Engines

## 3.1 HVAC Engine

Il task HVAC è stato integrato nel nuovo sistema di scheduling frontend.

Gestisce:

- temperatura delle zone
- temperatura interna
- temperatura esterna
- stato finestre
- calendario HVAC
- gestione delle zone
- ACS
- anti-legionella
- defrost
- protezioni operative

Le temperature delle zone vengono recuperate direttamente dal Buffer Engine.

Questo mantiene il principio:

```text
Backend → Buffer → HVAC
```

evitando dipendenze dirette non necessarie tra il motore HVAC e i driver hardware.

---

## 3.2 Security / Sensors Engine

Il task Sensors è stato integrato nel nuovo Task Engine.

Il ciclo:

1. attende il completamento del power-on cycle
2. esegue il Security Sensor Engine
3. verifica se lo stato è cambiato
4. aggiorna lo stato aggregato della sicurezza
5. genera l'evento interno corrispondente

L'aggiornamento del sistema di sicurezza avviene quindi solo quando necessario.

Questo riduce eventi e scritture ridondanti nel Buffer.

---

## 3.3 Averages Engine

Il calcolo delle medie è stato integrato nello scheduler frontend.

Il task:

- legge le misure dal Buffer
- applica il relativo factor
- aggiorna i gruppi
- calcola la media
- genera l'output configurato

Il calcolo rimane indipendente dal ciclo di comunicazione.

---

# 4. Communication Services

## 4.1 Bridge Engine

Il Bridge viene ora eseguito attraverso il Communication Scheduler.

Non viene più considerato una componente privilegiata del ciclo principale.

Configurazione prevista:

- interval
- priority
- enabled

Questo consente di limitarne l'impatto sul resto del sistema.

---

## 4.2 MQTT Engine

MQTT viene eseguito come servizio schedulato indipendente.

La sua frequenza di esecuzione viene separata dalla frequenza dei task applicativi.

Continua a supportare l'integrazione con ecosistemi quali:

- Home Assistant
- Zigbee2MQTT
- Shelly

L'obiettivo è mantenere MQTT come livello di integrazione e non come elemento dominante del runtime.

---

## 4.3 WebAPI Engine

Il WebAPI Engine è stato integrato nello stesso modello di scheduling.

Le operazioni HTTP rimangono orientate a un modello non bloccante, con gestione separata rispetto ai task applicativi.

Sono mantenuti:

- GET
- POST
- profili messaggi
- correlazione request/response
- gestione asincrona del traffico

---

# 5. Buffer Engine

Il Buffer Engine rimane il punto centrale dello scambio dati.

Il principio architetturale resta:

```text
Device
   ↓
Backend
   ↓
Buffer
   ↓
Frontend
   ↓
Communication
```

Il Buffer continua a fornire:

- aree tipizzate
- timestamp
- change tracking
- virtual areas
- reverse
- split
- toggle
- lookup rapido
- diagnostica delle aree

La nuova architettura frontend rafforza ulteriormente il ruolo del Buffer come unica fonte di verità del sistema.

---

# 6. Backend

Il backend mantiene il modello deterministico introdotto nella Beta.

## Device Manager

Gestione di:

- priorità
- cooldown
- retry
- errori
- stato dispositivo

## Modbus Engine

Gestione di:

- polling round-robin
- retry deterministici
- scritture
- timeout
- gestione errori

## Time Manager

Gestione di:

- RTC
- fallback
- cron
- callback periodiche

---

# 7. Automation Engine

L'Automation Engine mantiene l'architettura dichiarativa introdotta nella Beta.

Supporta:

- scene
- regole
- condizioni multiple
- sequenze
- scheduled rules
- trend
- debounce
- composite rules
- configurazione JSON
- validazione

La nuova organizzazione del runtime consente alle automazioni di rimanere indipendenti dai dettagli di scheduling della comunicazione.

---

# 8. Diagnostica e osservabilità

La diagnostica viene ulteriormente integrata nel modello runtime.

Sono disponibili controlli e informazioni relativi a:

- task
- scheduler
- comunicazioni
- buffer
- split
- dispositivi
- RTC
- sicurezza
- HVAC
- Power
- Weather
- watchdog
- aree monitorate

Particolare importanza viene data alla possibilità di distinguere:

- task applicativo lento
- comunicazione lenta
- timeout
- saturazione delle risorse
- errore dispositivo
- problema di scheduling

Questo rende più semplice individuare la causa reale di un degrado del sistema.

---

# 9. Robustezza e stabilità

## 9.1 Riduzione dei blocchi

È stata perseguita una riduzione sistematica delle operazioni potenzialmente bloccanti.

In particolare:

- comunicazioni distribuite nel tempo
- task indipendenti
- scheduler dedicato
- riduzione delle operazioni sincrone
- separazione tra logica applicativa e networking

---

## 9.2 Gestione del Power-On Cycle

I task che dipendono da dati validi attendono il completamento del power-on cycle.

Questo evita l'esecuzione prematura di logiche che potrebbero utilizzare dati non ancora inizializzati.

Il comportamento è particolarmente importante per:

- Security
- Communication
- logiche dipendenti dal Buffer

---

## 9.3 Hot Standby

Il modello Master/Slave rimane integrato nell'architettura.

Il nodo Slave non esegue normalmente le attività di comunicazione previste per il Master.

Quando il nodo diventa Master:

```text
Slave
  ↓
Become Master
  ↓
Ethernet enabled
  ↓
Communication services active
```

Quando torna Slave:

```text
Master
  ↓
Become Slave
  ↓
Ethernet disabled
```

Questo mantiene separati i ruoli dei nodi e riduce il rischio di comunicazioni duplicate.

---

# 10. Performance

Sono state mantenute e consolidate le ottimizzazioni introdotte nella Beta.

### Buffer

- lookup O(1)
- change tracking
- accesso rapido ai valori

### Weather

- medie mobili O(1)

### Frontend

- scheduling per intervallo
- task indipendenti
- riduzione delle esecuzioni inutili

### Communication

- round-robin
- intervalli configurabili
- priorità
- esecuzione distribuita

L'obiettivo non è massimizzare il numero di operazioni per secondo, ma ottenere il miglior equilibrio tra:

**reattività + determinismo + consumo delle risorse.**

---

# 11. Compatibilità e comportamento

La nuova architettura mantiene la compatibilità concettuale con il modello Beta precedente.

Le principali modifiche sono interne al runtime.

Le applicazioni continuano a utilizzare:

- FrontendConfig
- TaskEngine
- Buffer
- Automation Engine
- Frontend Engines
- Communication Engines

ma l'esecuzione interna è ora maggiormente orchestrata.

---

# 12. Funzionalità deprecate o rimosse

## 12.1 Comunicazioni direttamente nel ciclo principale

Le comunicazioni non dovrebbero essere più gestite direttamente dal ciclo applicativo quando possono essere inserite nel Communication Scheduler.

---

## 12.2 Logica hardcoded

Rimane valido il principio introdotto nella Beta:

- niente automazioni applicative rigide
- configurazione dichiarativa
- comportamento controllato dalla configurazione

---

## 12.3 Dipendenze cloud obbligatorie

DomoManager rimane:

- local-first
- autonomo
- senza cloud obbligatorio

Le integrazioni esterne rimangono opzionali.

---

# 13. Limitazioni note

La release rimane una **Beta**.

Sono ancora possibili miglioramenti relativi a:

- tuning delle priorità del Communication Scheduler
- ottimizzazione dell'utilizzo delle socket Ethernet
- gestione avanzata della concorrenza tra servizi
- diagnostica dettagliata dei tempi di esecuzione
- profili WebAPI
- auto-tuning Power
- test estesi HotStandby
- test su configurazioni con molti servizi Ethernet simultanei
- strumenti grafici di configurazione

In particolare, le configurazioni che combinano simultaneamente:

- HMI
- MQTT
- Modbus TCP
- Bridge
- WebAPI

richiedono test specifici in funzione delle risorse disponibili sulla piattaforma hardware.

---

# 14. Obiettivi verso RC1

Le principali attività previste per la prossima fase comprendono:

- ulteriore tuning del Communication Scheduler
- diagnostica dei tempi per task
- diagnostica delle risorse Ethernet
- monitoraggio socket
- miglioramento gestione timeout
- editor JSON ufficiale
- ulteriori profili WebAPI
- dashboard diagnostica
- miglioramento forecast solare
- ottimizzazione ACS / anti-legionella
- supporto a più modelli di pompe di calore
- test estesi RS485
- test estesi HotStandby
- test di carico Ethernet

---

# 15. Filosofia architetturale

La direzione di sviluppo di DomoManager rimane basata su alcuni principi fondamentali:

### Determinismo

Il sistema deve comportarsi in modo prevedibile.

### Non-blocking

Nessun servizio secondario deve poter bloccare il sistema principale.

### Buffer-centric

Il Buffer rappresenta la fonte di verità dello stato del sistema.

### Modularità

Ogni Engine deve avere una responsabilità precisa.

### Diagnostica

Un problema deve poter essere osservato, localizzato e spiegato.

### Local-first

Il sistema deve poter funzionare senza dipendere da servizi cloud.

### Embedded-first

L'architettura deve tenere conto dei limiti reali dell'hardware.

---

# 16. Contributi

Vuoi contribuire a DomoManager?

Consulta:

- **CONTRIBUTING.md**
- **CODE_OF_CONDUCT.md**
- **ROADMAP.md**

I contributi dovrebbero rispettare i principi architetturali del progetto:

> Deterministico. Non bloccante. Diagnostico. Modulare.

---

# ============================================================
# FINE CHANGELOG
# ============================================================