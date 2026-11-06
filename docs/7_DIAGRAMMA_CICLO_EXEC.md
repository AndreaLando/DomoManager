# DomoManager - Ciclo di esecuzione

## 1. Scopo

Questo documento descrive il ciclo di esecuzione effettivo di DomoManager distinguendo:

- operazioni eseguite a ogni iterazione;
- operazioni subordinate al completamento del ciclo backend;
- task pesati e distribuiti nel tempo;
- scheduler e macchine a stati indipendenti;
- lifecycle dei frontend;
- completamento reale dei cicli;
- diagnostica e watchdog.

> **DomoManager non utilizza una sequenza monolitica composta da backend, applicazione, automazione e comunicazione eseguiti integralmente a ogni `loop()`. Il runtime avanza per passi, eventi, prerequisiti e task pesati.**

## 2. Entry point

```text
ARDUINO loop()
      |
      v
DomoManagerFrontendEngine::loop()
      |
      +--> frontend services
      |
      +--> DomoManager / DMRuntime loop
      |
      +--> protocol and domain orchestrators
```

L'entry point delega il lavoro ai componenti proprietari. Non deve duplicare il ciclo interno dei singoli engine.

## 3. Sequenza generale di una iterazione DMRuntime

```text
DMRuntime::loop(client)
        |
        +--> 1. acquire current time
        |
        +--> 2. PhysicalIOManager::update(now)
        |
        +--> 3. network protocol available?
        |        |
        |        +--> yes: Update(Modbus client, now)
        |        |
        |        +--> no: mark backend prerequisites ready
        |
        +--> 4. EventManager::process(bounded count)
        |
        +--> 5. backend/IP cycle completed?
        |        |
        |        +--> no: return
        |        |
        |        +--> yes: continue
        |
        +--> 6. execute one weighted runtime task
        |
        +--> 7. advance task index
        |
        +--> 8. end of task table?
                 |
                 +--> no: wait for next loop
                 |
                 +--> yes: mark backend cycle completion flags
```

Il ciclo distribuisce il lavoro e preserva la progressione dei protocolli non bloccanti.

## 4. Physical I/O cycle

```text
CURRENT TIME
     |
     v
POLLING INTERVAL ELAPSED?
     |
     +--> no: return
     |
     v
READ CONFIGURED INPUTS
     |
     v
DETECT RAW CHANGE
     |
     v
DEBOUNCE ELAPSED?
     |
     +--> no: continue
     |
     v
VALUE ALREADY PUBLISHED?
     |
     +--> yes: continue
     |
     v
EVENTMANAGER PUSH
     |
     +--> failed: preserve previous published state
     |             and retry on a later cycle
     |
     +--> success: update published state
```

Gli ingressi locali non aggiornano direttamente i consumer. Pubblicano eventi con una source dedicata.

## 5. Modbus backend cycle

```text
RUN MODBUS CYCLE
      |
      v
IP LIST AVAILABLE?
      |
      +--> no: return ERROR / not executed
      |
      v
PENDING WRITE FOR CURRENT OR NEXT IP?
      |
      +--> yes: select write target
      |
      +--> no: select normal round-robin target
      |
      v
IP MANAGER ALLOWS QUERY?
      |
      +--> no: advance or retry later
      |
      v
DMNETWORKMANAGER::tryAcquire()
      |
      +--> failed: retry later
      |
      v
MODBUS RunClient()
      |
      +--> update protocol state
      +--> report success or error
      +--> update IP cooldown/exclusion state
      |
      v
DMNETWORKMANAGER::release()
      |
      v
RETURN STATE + EXECUTED FLAG
```

L'esecuzione misura il tempo del ciclo Modbus soltanto quando una transazione è stata effettivamente avanzata.

### 5.1 Scritture pendenti

Le scritture hanno precedenza controllata sul normale round-robin:

```text
CURRENT IP HAS PENDING WRITE?
      |
      +--> yes: service current IP
      |
      +--> no: next IP has pending write?
                    |
                    +--> yes: service next IP
                    |
                    +--> no: normal read cycle
```

La priorità non deve aggirare `ShouldQuery()`, l'arbitraggio di rete o il rilascio delle risorse.

## 6. EventManager cycle

```text
EVENT QUEUE
     |
     v
PROCESS UP TO BOUNDED LIMIT
     |
     +--> Modbus consumer
     +--> Physical output consumer
     +--> Route manager
     +--> Toggle manager
     +--> Split manager
     +--> Automation or frontend consumers
     |
     v
RETURN TO MAIN LOOP
```

Il limite per iterazione impedisce che un backlog di eventi monopolizzi il runtime. Gli eventi residui restano disponibili per i cicli successivi.

## 7. Weighted task rotation

Il runtime non esegue tutti i task applicativi a ogni iterazione. Utilizza una tabella pesata con contatori.

```text
CURRENT TASK
     |
     v
COUNTER == 0?
     |
     +--> no: decrement counter
     |
     +--> yes:
            +--> execute task
            +--> reload counter with weight
     |
     v
ADVANCE TASK INDEX
     |
     +--> end of table: wrap to zero
```

La tabella corrente comprende task equivalenti a:

```text
Task_Splits
Task_Automation
Task_ActivityLoop
Task_Scheduler
Task_WatchdogAndRunningT
Task_RestartIP
```

Il peso distribuisce la frequenza relativa. Non rappresenta una durata temporale esatta e non sostituisce gli intervalli interni dei task.

## 8. Split cycle

```text
Task_Splits
     |
     v
ACTIVE SPLITS PRESENT?
     |
     +--> no: return
     |
     v
SplitOutManager::update()
     |
     v
OUTPUT EVENTS / TIMEOUT HANDLING
```

Il task viene eseguito solo se esistono split attivi.

## 9. Automation cycle

```text
Task_Automation
      |
      v
IP/BACKEND CYCLE COMPLETED?
      |
      +--> no: return
      |
      v
AutomationEngine::update(now)
      |
      v
ACTIONS COLLECTION
      |
      v
for each action
      |
      v
forceEvent(targetArea, value, automationSource)
      |
      v
BUFFER CHANGE + EVENTMANAGER PUSH
```

Le azioni vengono trasformate in eventi applicativi. L'Automation Engine non scrive direttamente sul protocollo dell'attuatore.

## 10. Activity loop cycle

```text
Task_ActivityLoop
      |
      v
ACTIVITY PREPARE READY?
      |
      +--> no: return
      |
      v
CALLBACK CONFIGURED?
      |
      +--> no: return
      |
      v
RATE LIMIT INTERVAL ELAPSED?
      |
      +--> no: return
      |
      v
MEASURE CALLBACK EXECUTION
      |
      v
UPDATE WATCHDOG TIMING
      |
      v
CLEAR PREPARE READY FLAG
```

Il callback applicativo è rate-limited e misurato. L'esecuzione riuscita consuma la readiness corrente.

## 11. Asynchronous scheduler cycle

```text
Task_Scheduler
      |
      v
scheduler.run(now)
      |
      +--> due callback
      +--> delayed actions
      +--> sequence progression
```

Lo scheduler usa il tempo corrente del `TimeManager`. I job non scaduti non vengono eseguiti.

## 12. Watchdog e RunningT cycle

```text
Task_WatchdogAndRunningT
        |
        v
CHECK INTERVAL ELAPSED?
        |
        +--> no: return
        |
        v
WATCHDOG ENABLED AND POWER-ON CYCLE COMPLETED?
        |
        +--> yes: watchdog.check(now)
        |
        v
UPDATE CYCLE TIMING CHANGED?
        |
        +--> no: finish
        |
        v
forceInternalEvent(areaRunningT, updateCycle)
```

Il controllo watchdog viene attivato soltanto dopo la progressione minima richiesta dal power-on cycle.

## 13. Restart IP cycle

```text
Task_RestartIP
      |
      v
CHECK INTERVAL ELAPSED?
      |
      +--> no: return
      |
      v
INSPECT ALL IP STATES
      |
      v
EVERY IP IN COOLDOWN OR EXCLUDED
AND ERROR COUNT ABOVE THRESHOLD?
      |
      +--> no: continue normally
      |
      +--> yes: platform restart
```

Il reset è una reazione estrema allo stato globale degli IP, non alla singola transazione fallita.

## 14. SomethingChanged cycle

```text
BUFFER CHANGE NOTIFICATION
       |
       v
SomethingChangedNotify()
       |
       v
SET PENDING FLAG
       |
       v
SomethingChangedWrapper()
       |
       +--> inspect changed map
       +--> update watched-area diagnostics
       +--> execute application callback
       +--> measure callback duration
       +--> update watchdog timing
```

Le aree modificate vengono aggregate nella changed map. Il callback applicativo riceve l'insieme delle aree cambiate.

## 15. Security cycle

Il ciclo Security è governato dal relativo orchestratore e dalla configurazione frontend, non dalla tabella pesata interna di `DMRuntime` in modo implicito.

```text
SECURITY INTERVAL ELAPSED?
       |
       +--> no: return
       |
       v
READ WIRED SENSORS / PANEL STATE
       |
       v
DEBOUNCE AND STARTUP INHIBIT
       |
       v
UPDATE CHANNELS AND ZONES
       |
       v
SECURITY CODEC
       |
       v
STATUS AREAS / EVENTS / CALLBACKS
       |
       v
DIAGNOSTIC AND FRONTEND UPDATE
```

Il percorso dei comandi usa lo stesso codec canonico:

```text
COMMAND AREA / FRONTEND COMMAND
       |
       v
VALIDATION
       |
       v
SECURITY CODEC
       |
       v
PANEL / ZONE / SENSOR HANDLER
```

## 16. Power cycle

```text
POWER INTERVAL ELAPSED?
       |
       +--> no: return
       |
       v
READ MAIN POWER
READ 24V / FAULT / BATTERY MODE
READ EXTRA SIGNALS
       |
       v
VALIDATE READINGS
       |
       v
CLASSIFY MAIN POWER WITH HYSTERESIS
       |
       v
DETECT STATE TRANSITIONS
       |
       v
UPDATE ALARMS / SNAPSHOT / CALLBACKS
```

Una lettura non valida aggiorna lo stato di validità senza inventare un valore applicativo valido.

## 17. Weather cycle

```text
WEATHER INTERVAL ELAPSED?
       |
       +--> no: return
       |
       v
READ TEMPERATURE / WIND / RAIN / LIGHT
       |
       v
VALIDATE SAMPLE
       |
       v
UPDATE MOVING WINDOWS
       |
       v
DAY/NIGHT HYSTERESIS
RAIN / GUST DETECTION
       |
       v
DEBOUNCED EVENTS / ALARMS
```

Il primo campione deve essere gestito esplicitamente. Getter e medie non devono dividere per zero prima dell'inizializzazione.

## 18. Solar cycle

```text
SOLAR ENGINE LOOP
       |
       v
TRANSPORT STATE MACHINE
       |
       +--> UART
       +--> TCP
       |
       v
DISCOVERY REQUIRED?
       |
       +--> yes: advance command discovery
       |
       +--> no: paced polling
       |
       v
PARSE RESPONSE / CRC
       |
       v
UPDATE SNAPSHOT
       |
       v
COMPARE WITH PREVIOUS SNAPSHOT
       |
       v
AREA / FRONTEND UPDATE
```

Discovery e polling avanzano per passi. Pacing e timeout appartengono al transport/runtime Solar.

## 19. Introspection lifecycle

Introspection e Semantic Resolver non vengono ricostruiti a ogni iterazione ordinaria.

```text
CONFIGURATION AND REGISTRATIONS READY
             |
             v
DomoIntrospection::build()
             |
             v
DomoIntrospection::finalize()
             |
             v
DomoSemanticResolver::resolve()
             |
             v
CONTROLLED MODEL VIEW
             |
      +------+------+----------------+
      |             |                |
      v             v                v
DIAGNOSTICS   HOME ASSISTANT   OTHER CONSUMERS
```

La ricostruzione è necessaria quando cambia il modello configurato o vengono registrati metadati strutturali che devono entrare nel modello pubblicabile.

## 20. Home Assistant connection cycle in DMHAOrchestrator

```text
MQTT CONNECTED?
   |
   +--> no:
   |      +--> mark disconnected
   |      +--> reset subscriptions
   |      +--> reset availability
   |      +--> reset discovery
   |      +--> reset snapshot
   |      +--> retry later
   |
   +--> yes:
          |
          v
   SUBSCRIBE AREA AND RUNTIME COMMAND TOPICS
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
   PUBLISH PENDING STATES
          |
          v
   PROCESS INCREMENTAL CHANGES
```

Un fallimento iniziale dello snapshot non deve rendere permanente lo stato di errore. Dopo una disconnessione e successiva riconnessione, il lifecycle deve poter ripartire.

## 21. PendingState cycle

```text
STATE CHANGE
     |
     v
QUEUE OR UPDATE PENDING STATE
     |
     v
PUBLISH BUDGET AVAILABLE?
     |
     +--> no: keep pending
     |
     v
CONVERT SCALE / TYPE
     |
     v
MQTT PUBLISH
     |
     +--> failed: retain or recover according to policy
     |
     +--> success: remove or mark completed
```

La coda deve impedire che un burst di stati blocchi il ciclo MQTT o il runtime principale.

## 22. Command cycle

### 22.1 Area command

```text
MQTT MESSAGE
      |
      v
PARSE AREA TOPIC
      |
      v
FIND ENTITY
      |
      v
VERIFY VISIBLE AND WRITABLE
      |
      v
PARSE PAYLOAD BY ENTITY TYPE
      |
      v
COMMAND CALLBACK
      |
      v
BUFFER / EVENTMANAGER / OWNER ENGINE
      |
      v
LATER STATE FEEDBACK
```

### 22.2 RuntimeStore command

```text
MQTT MESSAGE
      |
      v
PARSE RUNTIME ID
      |
      v
FIND HANDLE AND ENTRY
      |
      v
VERIFY TYPE AND ACCESS
      |
      v
DMRUNTIMESTORE WRITE
      |
      v
NORMALIZED NOTIFICATION
      |
      v
STATE FEEDBACK IF READABLE
```

Il feedback immediato è ammesso soltanto quando il valore scritto coincide con lo stato applicativo autorevole. Per i comandi asincroni verso dispositivi, il feedback deve riflettere lo stato realmente confermato.

## 23. Communication runtime

Non esiste un unico Communication Scheduler che esegue Bridge, MQTT e Web API in timed round-robin.

Il modello è:

```text
FRONTEND / PROTOCOL LOOPS
  +-- MQTT lifecycle
  +-- Home Assistant orchestrator
  +-- Web API state machines
  +-- Bridge state machine
  +-- Modbus cycle
  +-- Solar transport
            |
            v
DMNETWORKMANAGER
  +-- protocol registration
  +-- pacing
  +-- arbitration
  +-- protocol state
            |
            v
SOCKETMANAGER
  +-- owner
  +-- kind
  +-- exact slot
  +-- release lifecycle
```

Ogni servizio avanza secondo il proprio intervallo, stato e prerequisito. La risorsa condivisa viene coordinata tramite ownership esplicita.

## 24. FrontendStatus cycle

```text
ORCHESTRATOR STATE CHANGE
         |
         v
UPDATE FRONTENDSTATUS BITS
         |
         v
RESERVED BITS SANITIZATION
         |
         v
STATUS WORD CHANGED?
         |
         +--> no: nothing to publish
         |
         v
CONSUME CHANGED WORD
         |
         v
BUFFER / DIAGNOSTIC PUBLICATION
```

I bit distinguono:

```text
Enabled
Running
Initialized
Connected
Active
Recovering
Degraded
Fault
```

`Connected` non è requisito universale di operatività per tutti i frontend.

## 25. Full-cycle synchronization

Il completamento deve indicare lavoro realmente avanzato.

```text
IP / BACKEND PROGRESSION
          |
          v
ipCycleCompleted
          |
          v
WEIGHTED TASK TABLE WRAPPED
          |
          v
backendCycleDone
backend1StCycleDone
          |
          v
POWER-ON CYCLE COMPLETED
          |
          v
WATCHDOG ELIGIBLE
```

Regole:

- entrare in una funzione non equivale a completare un ciclo;
- un protocollo non acquisito non deve risultare eseguito;
- il primo ciclo completo abilita controlli che richiedono un sistema stabilizzato;
- i flag one-shot devono essere consumati o azzerati dal relativo owner.

## 26. Timing e spike detection

```text
MEASURE EXECUTION TIME
         |
         v
UPDATE LAST VALUE
         |
         v
UPDATE FIXED-POINT MOVING AVERAGE
         |
         v
COMPUTE DYNAMIC THRESHOLD
         |
         v
EXECUTION > THRESHOLD?
         |
         +--> no: clear spike state
         |
         +--> yes:
                +--> update max spike
                +--> increment spike count
                +--> store timestamp
                +--> rate-limit warning log
```

La soglia dinamica deve mantenere un minimo operativo per evitare falsi positivi sui task molto rapidi.

## 27. Diagnostica parallela

La diagnostica osserva il runtime senza sostituire il suo controllo di flusso.

```text
DIAGNOSTIC INPUTS
  +-- cycle durations
  +-- callback timings
  +-- event backlog
  +-- pending state backlog
  +-- device and protocol errors
  +-- MQTT connection and recovery
  +-- Ethernet and socket ownership
  +-- unknown / hidden entities
  +-- Buffer / RuntimeStore integrity
  +-- Security / HVAC / Solar / Power / Weather
  +-- watchdog state
          |
          v
REPORTS / LOGS / STATUS AREAS
```

## 28. Invarianti del ciclo

Il runtime deve mantenere le seguenti invarianti:

- nessun protocollo secondario blocca indefinitamente il loop;
- gli ingressi fisici usano debounce non bloccante;
- EventManager processa un numero limitato di eventi per iterazione;
- i task pesati distribuiscono il lavoro, ma non sostituiscono gli intervalli;
- le scritture Modbus rispettano IP manager e arbitraggio;
- ogni acquisizione di rete ha un rilascio coerente;
- l'automazione agisce tramite stato ed eventi;
- Introspection non viene ricostruita inutilmente;
- Home Assistant ripete subscription, discovery e snapshot dopo riconnessione;
- lo snapshot fallito può essere recuperato;
- i flag di completamento rappresentano progressione reale;
- watchdog e diagnostica vengono attivati nel momento corretto;
- stato richiesto e stato confermato rimangono distinti.

## 29. Checklist per nuovi task

Prima di aggiungere un nuovo task verificare:

- [ ] owner definito;
- [ ] criterio di eleggibilità esplicito;
- [ ] intervallo o peso documentato;
- [ ] prerequisiti verificati;
- [ ] esecuzione non bloccante;
- [ ] budget di lavoro per iterazione;
- [ ] timeout e retry controllati;
- [ ] stato tecnico incapsulato;
- [ ] output tramite Buffer, RuntimeStore o EventManager;
- [ ] timing misurabile;
- [ ] comportamento al primo ciclo definito;
- [ ] recovery dopo errore;
- [ ] flag di completamento aggiornati solo dopo lavoro reale;
- [ ] diagnostica sufficiente;
- [ ] nessuna inizializzazione hardware duplicata.

## 30. Principio finale

> **Il ciclo di DomoManager è cooperativo e incrementale: ogni componente avanza soltanto quando è eleggibile, esegue un lavoro limitato, aggiorna stato e diagnostica, quindi restituisce il controllo al runtime.**

---

**DOMOMANAGER**  
**CICLO DI ESECUZIONE**  
**ELIGIBILITY. PROGRESSION. BOUNDED WORK. RECOVERY.**
