# ============================================================
# DOMOMANAGER – ARCHITETTURA TECNICA DETTAGLIATA
# ============================================================

# 1. Introduzione

L'architettura di DomoManager è progettata come quella di un
**runtime embedded per l'infrastruttura domestica**.

Il sistema è composto da moduli specializzati, ciascuno con una
responsabilità precisa, coordinati attraverso un modello comune di
stato e attraverso un runtime deterministico.

Il principio fondamentale è:

> **ogni componente deve sapere cosa deve fare, quando deve farlo,
> quali dati può utilizzare e come rendere osservabile il proprio
> comportamento.**

L'architettura non è quindi costruita attorno a un semplice `loop()`
nel quale tutti i componenti vengono eseguiti indistintamente.

È organizzata attraverso domini separati:

```text
                    MONDO FISICO
                         │
                         ▼
                  ┌─────────────┐
                  │   BACKEND   │
                  └──────┬──────┘
                         │
                         ▼
                  ┌─────────────┐
                  │    BUFFER   │
                  └──────┬──────┘
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
          TASK ENGINE AUTOMATION  FRONTEND
             │        ENGINE       ENGINES
             │           │           │
             └───────────┼───────────┘
                         │
                         ▼
              COMMUNICATION SCHEDULER
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
          BRIDGE       MQTT        WebAPI
```

A questi domini si aggiungono servizi trasversali:

- Time Manager
- Diagnostics
- Watchdog
- Hot Standby
- configurazione
- gestione delle risorse

---

# 2. Principi architetturali

L'intera implementazione segue alcuni principi fondamentali.

## 2.1 Determinismo

Il runtime deve mantenere un comportamento prevedibile.

Sono quindi privilegiate:

- esecuzioni schedulate
- intervalli espliciti
- stati espliciti
- timeout controllati
- retry controllati
- lifecycle definiti
- gestione strutturata degli errori

---

## 2.2 Non-blocking

Un componente non deve poter bloccare indefinitamente il resto del
sistema.

Questo vale in particolare per:

- networking
- MQTT
- WebAPI
- Bridge
- RS485
- Modbus
- HMI

Il sistema deve continuare a progredire anche quando un servizio
esterno è lento, non disponibile o in errore.

---

## 2.3 Buffer-centric

Il Buffer rappresenta la fonte comune dello stato.

Gli Engine non devono creare arbitrariamente copie dello stato globale
del sistema.

Il modello è:

```text
INPUT
  │
  ▼
BUFFER
  │
  ▼
LOGIC
  │
  ▼
BUFFER
  │
  ▼
OUTPUT
```

---

## 2.4 Separazione delle responsabilità

Ogni componente deve avere un ruolo preciso.

In particolare:

```text
Device Manager
    → dispositivi

Backend
    → acquisizione

Buffer
    → stato

Task Engine
    → scheduling applicativo

Automation Engine
    → comportamento

Frontend Engines
    → funzioni specialistiche

Communication Scheduler
    → scheduling comunicazioni

Communication Engines
    → protocolli

Diagnostics
    → osservabilità
```

---

# 3. Struttura logica del runtime

DomoManager può essere suddiviso in cinque grandi domini.

## 3.1 Hardware / Physical Layer

Rappresenta il mondo fisico:

- I/O
- sensori
- attuatori
- Modbus
- RS485
- Ethernet
- RTC

---

## 3.2 Backend Layer

Acquisisce e aggiorna i dati provenienti dal mondo fisico.

Comprende:

- Device Manager
- Modbus Engine
- protocolli industriali
- driver
- gestione dispositivi

---

## 3.3 State Layer

È costituito dal Buffer Engine.

Rappresenta lo stato corrente della casa.

---

## 3.4 Logic / Application Layer

Comprende:

- Task Engine
- Automation Engine
- HVAC
- Power
- Weather
- Security
- Averages

---

## 3.5 Communication Layer

Comprende:

- Communication Scheduler
- MQTT
- WebAPI
- Bridge
- servizi di integrazione

Questa separazione impedisce che la comunicazione esterna diventi
parte integrante della logica interna della casa.

---

# 4. Buffer Engine

Il Buffer Engine rappresenta il centro dello stato del sistema.

È il punto attraverso il quale i diversi domini condividono le
informazioni.

Il Buffer può contenere:

- stati
- valori analogici
- temperature
- potenze
- allarmi
- comandi
- bitmask
- dati virtuali
- dati HVAC
- dati Power
- dati Weather
- dati Security

Ogni area è caratterizzata da informazioni strutturate e può essere
associata a:

- tipo
- direzione
- timestamp
- stato di modifica
- configurazione

Le categorie principali comprendono:

- `Field`
- `FromPanel`
- `ToPanel`

---

## 4.1 Change Tracking

Il Buffer mantiene il concetto di cambiamento.

Un modulo può quindi determinare se un valore è stato modificato senza
dover elaborare continuamente tutto il sistema.

Questo permette di:

- ridurre elaborazioni inutili
- generare eventi solo quando necessario
- sincronizzare frontend
- ottimizzare comunicazioni
- migliorare la diagnostica

---

## 4.2 Virtual Areas

Il Buffer può rappresentare anche informazioni che non corrispondono
direttamente a un dispositivo fisico.

Questo permette di creare:

- stati derivati
- valori aggregati
- bitmask
- risultati di automazioni
- informazioni logiche

Il sistema fisico e il sistema logico possono quindi condividere lo
stesso modello di stato.

---

## 4.3 Split / Reverse / Toggle

Il Buffer supporta operazioni strutturate quali:

- split
- reverse
- toggle

Queste funzionalità consentono di trasformare e rappresentare
correttamente determinati segnali senza distribuire la relativa logica
in più moduli.

---

## 4.4 Principio di unicità dello stato

La regola architetturale è:

> **uno stato globale deve avere una rappresentazione coerente.**

Questo riduce il rischio di:

- stati fantasma
- copie non sincronizzate
- valori obsoleti
- comportamenti differenti tra Engine

---

# 5. Device Manager

Il Device Manager rappresenta il livello di gestione dei dispositivi.

Le sue responsabilità comprendono:

- profili
- identificazione dispositivi
- mappatura
- routing
- priorità
- cooldown
- gestione errori
- gestione dello stato di comunicazione

Il Device Manager non deve diventare il luogo nel quale vive la logica
applicativa della casa.

Il suo compito è gestire il dispositivo.

La decisione su **come utilizzare quel dispositivo** appartiene ai
livelli superiori.

---

# 6. Modbus Engine

Il Modbus Engine collega i dispositivi Modbus al modello di stato.

Supporta:

- Modbus RTU
- Modbus TCP
- polling
- scritture
- retry
- timeout
- gestione errori
- round-robin

Il polling viene organizzato in modo da evitare che un singolo
dispositivo possa monopolizzare il ciclo.

Il modello generale è:

```text
Device
  │
  ▼
Modbus
  │
  ▼
Buffer
```

Per una scrittura:

```text
Buffer
  │
  ▼
Modbus
  │
  ▼
Device
```

La comunicazione rimane separata dalla logica applicativa.

---

# 7. Time Manager

Il Time Manager fornisce il riferimento temporale comune.

Gestisce:

- RTC
- sincronizzazione
- epoch
- conversione temporale
- fallback
- callback periodiche
- validità temporale

Può fornire riferimenti per:

- secondi
- minuti
- ore
- giorni

Il tempo viene utilizzato da:

- automazioni
- HVAC
- ACS
- Power
- Security
- scheduler
- timeout
- retry
- diagnostica

Il principio è:

> **ogni comportamento temporale significativo deve utilizzare un
riferimento temporale coerente.**

---

# 8. Automation Engine

L'Automation Engine rappresenta il livello decisionale.

Riceve informazioni dal modello di stato e valuta:

- condizioni
- eventi
- orari
- trend
- sequenze
- stati aggregati

Può gestire:

- scene
- rules
- scheduled rules
- composite rules
- trend rules
- debounce rules
- bitmask rules
- sequenze temporizzate

Il flusso è:

```text
Buffer
   │
   ▼
Conditions
   │
   ▼
Automation Engine
   │
   ▼
Decision
   │
   ▼
Buffer
```

L'Automation Engine non dovrebbe dipendere direttamente dai driver
fisici.

Questo permette di cambiare il dispositivo senza dover riscrivere la
logica dell'automazione.

---

# 9. Automation Builder

La configurazione delle automazioni viene trasformata in strutture
interne attraverso il sistema di building e validazione.

Il Builder:

1. interpreta la configurazione
2. verifica la validità
3. costruisce la rappresentazione interna
4. prepara l'esecuzione runtime

L'obiettivo è separare:

```text
CONFIGURAZIONE
      │
      ▼
VALIDAZIONE
      │
      ▼
RAPPRESENTAZIONE RUNTIME
      │
      ▼
ESECUZIONE
```

In questo modo il costo della configurazione non viene trasferito
continuamente al ciclo operativo.

---

# 10. Frontend Engines

I Frontend Engines rappresentano le funzioni specialistiche del
sistema.

Tutti seguono il principio:

```text
READ BUFFER
     │
     ▼
PROCESS
     │
     ▼
DECIDE
     │
     ▼
WRITE BUFFER
```

---

## 10.1 HVAC Engine

Il motore HVAC gestisce:

- zone
- setpoint
- temperature
- fan coil
- compressori
- valvole
- ACS
- anti-legionella
- defrost
- protezioni
- finestre
- calendari

Il motore utilizza il Buffer come sorgente dei dati.

Le decisioni vengono successivamente riportate nel Buffer.

---

## 10.2 Power Engine

Il Power Engine gestisce:

- carichi prioritari
- limiti
- minOn
- minOff
- protezione della rete
- produzione solare
- forecast
- auto-tuning
- suggerimenti

L'obiettivo è ottimizzare il comportamento energetico mantenendo
vincoli di sicurezza e prevedibilità.

---

## 10.3 Weather Engine

Il Weather Engine gestisce:

- temperatura
- pioggia
- vento
- luce
- eventi meteorologici
- allarmi

Utilizza strutture efficienti per il calcolo delle medie mobili.

Gli eventi possono includere:

- RainStart
- WindGust
- DayStart

---

## 10.4 Security Engine

Il Security Engine gestisce sensori quali:

- PIR
- DOOR
- WINDOW
- SMOKE
- FLOOD
- TAMPER

Utilizza:

- zone
- bitmask
- callback
- startup inhibit
- aggregazione degli stati

Quando necessario, aggiorna lo stato aggregato nel Buffer.

---

## 10.5 Averages

Il sistema può eseguire calcoli aggregati sui valori presenti nel
Buffer.

Il task Averages:

- legge i gruppi configurati
- recupera i sensori
- applica i factor
- calcola le medie
- aggiorna le aree di destinazione

L'elaborazione rimane indipendente dalla comunicazione.

---

# 11. Task Engine

Il Task Engine coordina le attività applicative.

Un task è caratterizzato principalmente da:

- callback
- intervallo
- stato enabled
- tempo di ultima esecuzione

Il runtime può quindi determinare se un'attività deve essere eseguita
senza affidarsi a chiamate manuali sparse nel codice.

Il modello è:

```text
             TASK ENGINE
                  │
       ┌──────────┼──────────┐
       ▼          ▼          ▼
    Security    HVAC      Averages
```

---

# 12. TaskEngineOrchestrator

La gestione dei task è separata dall'implementazione dei singoli
Engine attraverso un orchestratore centrale.

Il `TaskEngineOrchestrator` gestisce:

- configurazione
- registrazione
- scheduling
- esecuzione
- stato dei task
- ciclo frontend

Questo permette al Task Engine di rappresentare un vero livello di
orchestrazione invece di essere semplicemente un contenitore di
callback.

La configurazione dei task rimane separata dalla loro implementazione.

---

# 13. Communication Scheduler

La comunicazione viene gestita separatamente dal normale scheduling
applicativo.

Il `CommunicationScheduler` coordina servizi quali:

- Bridge
- MQTT
- WebAPI

Il modello è:

```text
          COMMUNICATION SCHEDULER
                    │
       ┌────────────┼────────────┐
       ▼            ▼            ▼
    Bridge         MQTT        WebAPI
```

Ogni servizio può essere associato a:

- intervallo
- priorità
- enabled
- callback

L'esecuzione viene distribuita nel tempo.

---

# 14. Timed Round-Robin

Il Communication Scheduler utilizza un modello di scheduling
temporale con comportamento round-robin.

L'obiettivo è evitare che:

```text
Bridge → occupa tutto il runtime
```

oppure:

```text
MQTT → occupa tutto il runtime
```

oppure:

```text
WebAPI → occupa tutto il runtime
```

Il principio è:

```text
Bridge
  │
  ▼
MQTT
  │
  ▼
WebAPI
  │
  ▼
Bridge
  │
  ▼
...
```

con l'esecuzione effettiva determinata dagli intervalli configurati e
dallo stato dei servizi.

Questo migliora:

- prevedibilità
- fairness
- controllo del carico
- reattività del frontend

---

# 15. Comunicazione non-blocking

Il Communication Layer deve rispettare il principio:

> **la comunicazione non deve diventare il collo di bottiglia della
logica applicativa.**

Questo è particolarmente importante su hardware embedded.

Una comunicazione lenta può infatti introdurre:

- timeout
- retry
- consumo CPU
- occupazione socket
- ritardi nel frontend
- perdita di reattività

La separazione dello scheduler permette di contenere questi effetti.

---

# 16. MQTT Engine

MQTT rappresenta uno dei livelli di integrazione verso sistemi esterni.

Può gestire:

- pubblicazione
- ricezione
- mapping verso Buffer
- eventi
- stati
- comandi

Le integrazioni possono comprendere:

- Home Assistant
- Zigbee2MQTT
- Shelly

Il principio architetturale rimane:

```text
BUFFER
   │
   ▼
MQTT
   │
   ▼
EXTERNAL SYSTEM
```

e:

```text
EXTERNAL SYSTEM
   │
   ▼
MQTT
   │
   ▼
BUFFER
```

MQTT non rappresenta quindi la fonte primaria dello stato.

---

# 17. WebAPI Engine

Il WebAPI Engine gestisce comunicazioni HTTP.

Può utilizzare:

- GET
- POST
- profili
- correlazione
- parsing pattern
- mapping verso Buffer

Il modello è orientato all'esecuzione non-blocking.

Il protocollo HTTP viene quindi trattato come livello di comunicazione,
non come livello di logica applicativa.

---

# 18. Bridge AEE

Il Bridge AEE permette l'integrazione con sistemi frontend e pannelli
attraverso il protocollo AEE.

Il Bridge gestisce:

- traffico
- mapping
- sincronizzazione
- eventi

La sua esecuzione viene coordinata dal Communication Scheduler.

Questo impedisce che il Bridge diventi parte dominante del ciclo
frontend.

---

# 19. RS485

Il livello RS485 utilizza un modello esplicito a macchina a stati.

Può comprendere:

- frame
- ACK
- NACK
- retry
- timeout
- stato della comunicazione

Il principio è evitare sequenze bloccanti.

Il protocollo deve poter avanzare attraverso più cicli del runtime.

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

---

# 20. Frontend Cycle

Il runtime distingue il ciclo backend dal ciclo frontend.

Il principio generale è:

```text
BACKEND
   │
   ▼
BUFFER UPDATE
   │
   ▼
FRONTEND TASKS
   │
   ▼
COMMUNICATION SCHEDULING
   │
   ▼
FRONTEND CYCLE COMPLETE
```

Il completamento del ciclo frontend viene esposto al runtime attraverso
un flag dedicato.

Questo permette di sincronizzare correttamente i diversi livelli del
sistema.

---

# 21. Separazione Backend / Frontend

Il Backend si occupa principalmente di acquisire e aggiornare lo
stato.

Il Frontend interpreta quello stato.

```text
BACKEND
  │
  ▼
BUFFER
  │
  ▼
FRONTEND
```

Questa separazione impedisce che la logica applicativa venga inserita
direttamente nei driver.

Permette inoltre di cambiare:

- dispositivo
- protocollo
- driver
- metodo di acquisizione

senza dover necessariamente cambiare la logica della casa.

---

# 22. Event System

Gli eventi rappresentano un meccanismo di propagazione dei cambiamenti
significativi.

Il sistema può distinguere tra:

- variazione dello stato
- evento applicativo
- evento di comunicazione
- evento di sicurezza
- evento diagnostico

Il modello generale è:

```text
STATE CHANGE
     │
     ▼
BUFFER
     │
     ▼
EVENT
     │
     ├── Automation
     ├── Security
     ├── MQTT
     ├── HMI
     └── Diagnostics
```

Questo permette di evitare che ogni componente debba interrogare
continuamente l'intero sistema.

---

# 23. DomoManager Core

Il Core rappresenta il punto di coordinamento generale del sistema.

Le sue responsabilità comprendono:

- inizializzazione
- caricamento configurazione
- validazione
- inizializzazione del Buffer
- configurazione dispositivi
- registrazione protocolli
- setup Engine
- configurazione task
- configurazione comunicazioni
- gestione del ciclo
- watchdog
- diagnostica
- gestione Hot Standby

Il Core non dovrebbe diventare il luogo nel quale viene implementata
la logica specialistica.

Il suo ruolo è coordinare.

---

# 24. Configuration Layer

La configurazione rappresenta il contratto tra sistema e runtime.

Può definire:

- dispositivi
- aree
- protocolli
- task
- intervalli
- automazioni
- HVAC
- Power
- Weather
- Security
- MQTT
- WebAPI
- Hot Standby

Il principio è:

```text
CONFIGURATION
      │
      ▼
VALIDATION
      │
      ▼
RUNTIME SETUP
      │
      ▼
EXECUTION
```

Una configurazione non valida deve essere rilevata il prima possibile.

---

# 25. Lifecycle del sistema

Il lifecycle segue una sequenza controllata.

In forma semplificata:

```text
BOOT
 │
 ▼
HARDWARE INIT
 │
 ▼
CONFIG LOAD
 │
 ▼
CONFIG VALIDATION
 │
 ▼
BUFFER SETUP
 │
 ▼
DEVICE / PROTOCOL SETUP
 │
 ▼
ENGINE SETUP
 │
 ▼
TASK SETUP
 │
 ▼
COMMUNICATION SETUP
 │
 ▼
RUNTIME
```

Questa separazione rende più chiaro dove ogni componente viene
inizializzato e quali dipendenze devono essere già disponibili.

---

# 26. Power-On Cycle

Alcuni Engine non devono iniziare immediatamente a elaborare dati
applicativi.

Devono attendere che il sistema abbia completato il proprio ciclo
iniziale.

Questo è particolarmente importante per:

- Security
- Communication
- logiche che dipendono dal Buffer

Il modello è:

```text
BOOT
 │
 ▼
INITIALIZATION
 │
 ▼
POWER-ON CYCLE
 │
 ▼
VALID STATE
 │
 ▼
NORMAL OPERATION
```

Questo riduce il rischio di elaborare dati ancora incompleti o non
validi.

---

# 27. Gestione delle risorse embedded

L'architettura è progettata per hardware con risorse limitate.

Devono quindi essere considerate:

- RAM
- CPU
- memoria
- socket
- connessioni
- bus
- banda
- tempo di esecuzione

La concorrenza tra servizi deve essere controllata.

In particolare, più servizi Ethernet possono competere per le stesse
risorse.

Per questo il runtime privilegia:

- scheduling
- intervalli
- priorità
- timeout controllati
- retry limitati
- riduzione della concorrenza
- monitoraggio

La gestione delle risorse non è una funzione secondaria.

È parte dell'architettura.

---

# 28. Watchdog

Il watchdog rappresenta la protezione finale del runtime.

Può rilevare condizioni quali:

- mancata progressione
- blocco
- sovraccarico
- comportamento anomalo

Il watchdog non deve essere considerato un sostituto della
diagnostica.

Il suo ruolo è diverso:

```text
DIAGNOSTICS
    │
    └── cerca di spiegare il problema

WATCHDOG
    │
    └── impedisce che il problema lasci il sistema
        indefinitamente in uno stato non operativo
```

---

# 29. Diagnostica

La diagnostica è trasversale a tutta l'architettura.

Può analizzare:

- Buffer
- task
- scheduler
- automazioni
- dispositivi
- Modbus
- RS485
- RTC
- HVAC
- Power
- Weather
- Security
- comunicazioni
- watchdog
- Hot Standby

La diagnostica deve rendere distinguibili almeno tre categorie:

```text
LOGICA
  │
  ├── errore applicativo
  └── configurazione

COMUNICAZIONE
  │
  ├── timeout
  ├── retry
  └── protocollo

RISORSA
  │
  ├── CPU
  ├── memoria
  └── socket / connessioni
```

L'obiettivo è passare da:

> "non funziona"

a:

> "questo componente non ha potuto completare l'operazione per
questo motivo."

---

# 30. Hot Standby

Hot Standby introduce un secondo livello di resilienza.

Il sistema può operare in:

```text
MASTER
  │
  └── ACTIVE

SLAVE
  │
  └── READY
```

Il nodo Slave non deve comportarsi come un secondo Master indipendente.

Il ruolo viene esplicitamente determinato dal cluster.

Quando il nodo diventa Master:

```text
SLAVE
  │
  ▼
BECOME MASTER
  │
  ▼
ACTIVATE COMMUNICATION
```

Quando torna Slave:

```text
MASTER
  │
  ▼
BECOME SLAVE
  │
  ▼
DISABLE OPERATIONAL COMMUNICATION
```

Questo evita comunicazioni duplicate e riduce i conflitti.

---

# 31. Principio di dipendenza

La dipendenza tra moduli deve seguire il più possibile una direzione
chiara.

Il modello preferenziale è:

```text
PHYSICAL
   ↓
BACKEND
   ↓
BUFFER
   ↓
LOGIC
   ↓
COMMUNICATION
```

Non dovrebbe invece diventare:

```text
MQTT → HVAC → Modbus → Security → WebAPI → Buffer
```

perché una struttura di questo tipo genera dipendenze incrociate e
rende il sistema difficile da prevedere.

Il Buffer e il runtime costituiscono quindi il punto di coordinamento.

---

# 32. Principio di isolamento

Un Engine deve poter essere modificato senza richiedere la modifica
degli altri Engine quando la sua interfaccia non cambia.

Per esempio:

```text
HVAC
  │
  └── dipende dal modello di stato

MQTT
  │
  └── dipende dal modello di stato

Power
  │
  └── dipende dal modello di stato
```

Non:

```text
HVAC → MQTT
MQTT → Power
Power → Security
Security → WebAPI
```

L'isolamento riduce il costo evolutivo del sistema.

---

# 33. Flusso completo dei dati

Il percorso completo può essere rappresentato come:

```text
                 MONDO FISICO
                      │
                      ▼
               DEVICE MANAGER
                      │
                      ▼
                MODBUS / I/O
                      │
                      ▼
                   BUFFER
                      │
          ┌───────────┼───────────┐
          ▼           ▼           ▼
       TASKS      AUTOMATION   FRONTEND
          │        ENGINE       ENGINES
          │           │           │
          └───────────┼───────────┘
                      ▼
                  BUFFER
                      │
                      ▼
          COMMUNICATION SCHEDULER
                      │
          ┌───────────┼───────────┐
          ▼           ▼           ▼
       BRIDGE        MQTT        WebAPI
          │           │           │
          └───────────┼───────────┘
                      ▼
                MONDO ESTERNO
```

Il sistema è quindi un ciclo continuo:

**acquisizione → stato → elaborazione → decisione → comunicazione →
azione → nuova acquisizione.**

---

# 34. Esempio di cooperazione tra Engine

Consideriamo una situazione nella quale viene rilevata pioggia.

```text
Weather
   │
   ▼
RainStart
   │
   ▼
Buffer
   │
   ▼
Automation
   │
   ▼
Decision
   │
   ▼
Buffer
   │
   ├───────────────┐
   ▼               ▼
Security          HVAC
   │               │
   └───────┬───────┘
           ▼
       Communication
           │
           ▼
        MQTT / HMI
```

Nessun Engine deve necessariamente conoscere l'implementazione
interna degli altri.

La collaborazione avviene attraverso lo stato e gli eventi del
sistema.

---

# 35. Perché questa architettura è scalabile

La scalabilità di DomoManager non deriva soltanto dalla possibilità
di aggiungere nuovi dispositivi.

Deriva dalla possibilità di aggiungere **nuove responsabilità senza
rompere quelle esistenti**.

Un nuovo Engine dovrebbe poter:

1. definire il proprio stato
2. utilizzare il Buffer
3. implementare la propria logica
4. registrarsi nel Task Engine
5. utilizzare il Time Manager
6. utilizzare la diagnostica
7. utilizzare il Communication Scheduler se necessita di comunicazione

La struttura comune rimane invariata.

---

# 36. Regola di progettazione per nuovi moduli

Ogni nuovo modulo dovrebbe rispondere alle seguenti domande:

### Qual è la sua responsabilità?

Deve avere un compito chiaramente delimitato.

### Quale stato utilizza?

Lo stato deve essere rappresentato nel modello comune.

### Quando viene eseguito?

Deve essere integrato nel sistema di scheduling appropriato.

### Può bloccare?

Se sì, il design deve essere rivisto.

### Come gestisce gli errori?

Timeout, retry e fallback devono essere espliciti.

### Come viene diagnosticato?

Il comportamento deve essere osservabile.

### Quali risorse utilizza?

CPU, memoria, connessioni e socket devono essere considerati.

### Da quali componenti dipende?

Le dipendenze devono rimanere il più possibile unidirezionali.

---

# 37. Sintesi architetturale

L'architettura DomoManager può essere sintetizzata in questo modello:

```text
                 ┌──────────────────────┐
                 │      PHYSICAL        │
                 │       WORLD          │
                 └──────────┬───────────┘
                            │
                            ▼
                 ┌──────────────────────┐
                 │       BACKEND        │
                 │ Device / Modbus /    │
                 │ RS485 / I/O          │
                 └──────────┬───────────┘
                            │
                            ▼
                 ┌──────────────────────┐
                 │        BUFFER        │
                 │   SINGLE SOURCE OF   │
                 │       TRUTH          │
                 └──────────┬───────────┘
                            │
             ┌──────────────┼──────────────┐
             ▼              ▼              ▼
        ┌─────────┐   ┌────────────┐  ┌───────────┐
        │  TASK   │   │ AUTOMATION │  │ FRONTEND  │
        │ ENGINE  │   │   ENGINE   │  │  ENGINES  │
        └────┬────┘   └─────┬──────┘  └─────┬─────┘
             │              │               │
             └──────────────┼───────────────┘
                            ▼
                 ┌──────────────────────┐
                 │ COMMUNICATION        │
                 │ SCHEDULER            │
                 └──────────┬───────────┘
                            │
               ┌────────────┼────────────┐
               ▼            ▼            ▼
            Bridge        MQTT        WebAPI
               │            │            │
               └────────────┼────────────┘
                            ▼
                    EXTERNAL SYSTEMS
```

Attorno a tutto il sistema operano:

```text
              ┌─────────────────────┐
              │     TIME MANAGER    │
              ├─────────────────────┤
              │     DIAGNOSTICS     │
              ├─────────────────────┤
              │       WATCHDOG      │
              ├─────────────────────┤
              │     HOT STANDBY     │
              └─────────────────────┘
```

Questi componenti non rappresentano un singolo livello della
pipeline.

Sono **servizi trasversali del runtime**.

---

# 38. Principio finale

L'architettura DomoManager può essere riassunta attraverso una
sequenza fondamentale:

> **Il mondo fisico produce dati.  
> Il Backend li acquisisce.  
> Il Buffer rappresenta lo stato.  
> Il Task Engine coordina l'esecuzione.  
> Gli Engine interpretano lo stato.  
> L'Automation Engine prende decisioni.  
> Il Communication Scheduler coordina la comunicazione.  
> I protocolli trasferiscono informazioni e comandi.  
> La diagnostica osserva tutto il processo.**

Il risultato non è una collezione di moduli.

È un runtime nel quale:

**stato, tempo, logica, comunicazione e diagnostica seguono regole
comuni.**

Questa è la base tecnica sulla quale DomoManager può evolvere da
piattaforma embedded a **infrastruttura completa per la casa**.

---

# ============================================================
# DOMOMANAGER
# ARCHITETTURA TECNICA DETTAGLIATA
# ============================================================