# DOMOMANAGER - CICLO DI ESECUZIONE

## 1. Sequenza completa

```text
LOOP
 |
 +-- 1. BACKEND CYCLE
 |     +-- Time Manager
 |     +-- Device selection
 |     +-- Modbus / Local I/O / RS485 step
 |     +-- Buffer update
 |     +-- Event generation
 |
 +-- 2. APPLICATION CYCLE
 |     +-- Security
 |     +-- HVAC
 |     +-- Averages
 |     +-- Power
 |     +-- Solar
 |     +-- Weather
 |
 +-- 3. AUTOMATION CYCLE
 |     +-- Conditions
 |     +-- Scenes / Rules / Sequences
 |     +-- Buffer updates
 |
 +-- 4. COMMUNICATION TASK
 |     +-- Communication Scheduler
 |           +-- Bridge step
 |           +-- MQTT step
 |           +-- WebAPI step
 |
 +-- 5. DIAGNOSTICS / WATCHDOG / CYCLE COMPLETION
```

## 2. Backend cycle

```text
TIME UPDATE
    |
    v
DEVICE MANAGER
 profile, area/register mapping, priority, cooldown
    |
    v
PROTOCOL STEP
 deterministic read/write, timeout, retry
    |
    v
BUFFER UPDATE
 timestamp, change tracking, virtual areas, events
```

Il Backend deve avanzare senza introdurre attese indefinite.

## 3. Application cycle

Il Task Engine esegue soltanto i task abilitati e scaduti.

```text
TASK ENGINE
    +-- Security task
    +-- HVAC task
    +-- Averages task
    +-- Power task
    +-- Solar task
    +-- Weather task
```

Ogni task legge il Buffer, elabora e produce aggiornamenti tramite Buffer o Event Manager.

## 4. Security cycle

```text
SecurityOrchestrator::Loop
        |
        +-- sensor/zone/system changes
        +-- alarm synchronization
        +-- SecurityHmiInterface update
        +-- status bitmask codec
        +-- status area updates
        +-- alarm callback
```

I comandi seguono il percorso inverso dalle command areas verso il codec e gli handler Security.

## 5. Automation cycle

```text
CURRENT BUFFER STATE
        |
        v
AUTOMATION ENGINE
 conditions / trend / debounce / composite / bitmask
        |
        v
ACTIONS / SCENES / SEQUENCES
        |
        v
BUFFER EVENTS
```

## 6. Introspection lifecycle

Introspection e Semantic Resolver non devono essere ricostruiti a ogni iterazione ordinaria.

```text
CONFIGURATION READY
       |
       v
DomoIntrospection::build
       |
       v
DomoSemanticResolver::resolve
       |
       v
IMMUTABLE/CONTROLLED MODEL VIEW
       |
       v
Diagnostics and communication consumers
```

Una ricostruzione avviene soltanto quando cambia il modello configurato o i metadati registrati.

## 7. Home Assistant connection cycle

```text
MQTT CONNECTED?
   | no -> reset connection state and retry later
   v yes
SUBSCRIBE AREA/RUNTIME COMMAND TOPICS
   |
   v
PUBLISH AVAILABILITY
   |
   v
PUBLISH DISCOVERY
   |
   v
INITIAL SNAPSHOT
   +-- Buffer area entities -> PendingState
   +-- Runtime entities -> direct typed publish
   |
   v
PUBLISH PENDING STATES
```

Dopo lo snapshot, gli eventi incrementali aggiornano la coda. In caso di disconnessione, availability, discovery e snapshot vengono ristabiliti secondo il lifecycle dell'orchestratore.

## 8. Command cycle

### Area

```text
MQTT MESSAGE
 -> parse topic
 -> find Entity
 -> verify writable / visible
 -> parse payload
 -> command callback
 -> Buffer / engine owner
 -> later state feedback
```

### RuntimeStore

```text
MQTT MESSAGE
 -> parse Runtime ID
 -> find handle and entry
 -> verify type and access
 -> write RuntimeStore
 -> normalized notification
 -> immediate state feedback
```

## 9. Communication cycle

```text
COMMUNICATION TASK
        |
        v
TIMED ROUND-ROBIN
        |
        +-- one eligible Bridge step
        +-- one eligible MQTT step
        +-- one eligible WebAPI step
```

L'implementazione effettiva puo scegliere il servizio eleggibile secondo priorita e intervallo, ma nessun servizio deve monopolizzare il ciclo.

## 10. Full-cycle synchronization

```text
BACKEND COMPLETED
        |
        v
APPLICATION COMPLETED
        |
        v
AUTOMATION PROCESSED
        |
        v
COMMUNICATION ADVANCED
        |
        v
FULL CYCLE / WATCHDOG PROGRESS
```

I flag di completamento devono riflettere progressione reale, non soltanto l'ingresso nelle funzioni.

## 11. Diagnostics

La diagnostica osserva in parallelo:

- durata dei cicli;
- task lenti;
- backlog di stati;
- errori device e protocollo;
- stato MQTT e recovery;
- risorse Ethernet e socket;
- unknown/hidden entity;
- Security, HVAC, Solar, Power e Weather;
- RuntimeStore;
- watchdog e overload.

## 12. Principi di esecuzione

- Backend acquisisce e attua.
- Buffer conserva lo stato autorevole delle aree.
- RuntimeStore conserva i tag runtime.
- Task Engine esegue la logica applicativa.
- Automation Engine produce comportamento.
- Introspection e Semantic Resolver costruiscono il modello.
- Communication Scheduler espone e riceve dati.
- Nessun servizio di comunicazione deve bloccare il ciclo principale.
- Le risorse hardware devono essere osservabili.
- Diagnostica e watchdog verificano progressione e anomalie.
