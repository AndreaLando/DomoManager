# DomoManager - Architettura a blocchi

## 1. Scopo

Questo documento rappresenta l'architettura di DomoManager attraverso diagrammi a blocchi coerenti con il modello tecnico del progetto.

Le viste distinguono:

- acquisizione fisica;
- stato applicativo condiviso;
- propagazione degli eventi;
- coordinamento del runtime;
- domain engine e automazioni;
- introspezione e semantica;
- frontend e protocolli;
- networking e ownership delle risorse;
- servizi trasversali.

> **Il diagramma descrive responsabilità e flussi architetturali. Non sostituisce il grafo fisico degli `#include`.**

## 2. Modello completo

```text
+-----------------------------------------------------------------------+
|                         MONDO FISICO                                  |
| Sensori, attuatori, Modbus, RS485, I/O locali, Ethernet, RTC         |
+---------------------------------+-------------------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
|                    BACKEND E DEVICE LAYER                             |
| DeviceManager, DeviceProfiles, Modbus, PhysicalIOManager, driver     |
| acquisizione, attuazione, polling, timeout, retry, error handling    |
+-------------------------+---------------------------------------------+
                          |
                          v
+-------------------------+------------------+--------------------------+
|          BUFFER                            |      DMRUNTIMESTORE       |
| stato numerico condiviso                   | stato tipizzato condiviso|
| area -> long                               | handle -> typed value    |
| timestamp, change tracking, virtual areas  | metadata, access, dirty  |
+-------------------------+------------------+--------------------------+
                          |                  |
                          +--------+---------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                    EVENT E RUNTIME LAYER                              |
| EventManager, DMRuntime, scheduler asincrono, task pesati            |
| bounded event processing, timing, progressione del ciclo             |
+----------------------+----------------------+-------------------------+
                       |                      |
                       v                      v
+--------------------------------+  +-----------------------------------+
|        DOMAIN ENGINES          |  |        AUTOMATION ENGINE          |
| Security, HVAC, Power, Weather |  | scene, regole, sequenze, trend    |
| Solar, Averages, Access Control|  | soglie, bitmask, condizioni       |
+----------------+---------------+  +----------------+------------------+
                 |                                   |
                 +-----------------+-----------------+
                                   |
                                   v
                         BUFFER / RUNTIMESTORE
                                   |
                  +----------------+----------------+
                  |                                 |
                  v                                 v
+-----------------------------------+   +-------------------------------+
|       DOMOINTROSPECTION           |   |       ORCHESTRATORI           |
| entity, relation, origin, group   |   | lifecycle, recovery, queue    |
| riferimenti al modello originale  |   | routing dei comandi           |
+----------------+------------------+   +---------------+---------------+
                 |                                      |
                 v                                      v
+-----------------------------------+   +-------------------------------+
|     DOMOSEMANTICRESOLVER          |   |       PROTOCOL ENGINES        |
| kind, unit, class, state class    |   | MQTT, Web API, Bridge, RS485  |
| accesso, gruppo, visibilità       |   | Modbus TCP, Solar TCP/UART    |
+----------------+------------------+   +---------------+---------------+
                 |                                      |
                 v                                      v
+-----------------------------------+   +-------------------------------+
|      PROIEZIONI FRONTEND          |   | DMNETWORKMANAGER              |
| Home Assistant, diagnostica       |   | arbitraggio, pacing, stato    |
| modelli esterni read-only         |   | SocketManager e ownership     |
+-----------------------------------+   +-------------------------------+
```

Il diagramma non rappresenta Buffer e RuntimeStore come una singola struttura. I due storage sono complementari:

```text
Buffer          : area   -> long
DMRuntimeStore  : handle -> valore tipizzato
```

## 3. Ownership e flusso dello stato

```text
MONDO FISICO
     |
     v
BACKEND / DRIVER
     |
     v
VALIDAZIONE DEL DATO
     |
     +----------------------+
     |                      |
     v                      v
BUFFER                 DMRUNTIMESTORE
     |                      |
     +-----------+----------+
                 |
                 v
            EVENTMANAGER
                 |
                 v
       CONSUMER DEL RUNTIME
                 |
       +---------+----------+
       |                    |
       v                    v
DOMAIN ENGINE         FRONTEND UPDATE
```

Regole:

- il backend pubblica dati validati;
- Buffer governa lo stato numerico condiviso;
- RuntimeStore governa lo stato tipizzato condiviso;
- EventManager separa produzione e consumo;
- gli engine conservano soltanto lo stato tecnico di propria competenza;
- i frontend non diventano owner dello stato applicativo.

## 4. Percorso degli eventi

```text
PRODUCER
  +-- Modbus
  +-- Physical I/O
  +-- MQTT command
  +-- Automation
  +-- Route / Toggle / Split
  +-- Internal service
            |
            v
+---------------------------+
|       EVENTMANAGER        |
| queue e source identifier |
+-------------+-------------+
              |
              v
+---------------------------+
| PROCESSING LIMITATO       |
| per iterazione del loop   |
+-------------+-------------+
              |
    +---------+----------+----------------+
    |                    |                |
    v                    v                v
BUFFER CONSUMERS   PHYSICAL OUTPUTS   DOMAIN LOGIC
```

Il processamento limitato evita che una raffica di eventi monopolizzi l'intero ciclo.

## 5. Runtime e scheduling

DomoManager non utilizza un unico scheduler universale. Il coordinamento combina meccanismi diversi:

```text
+------------------------------------------------------------------+
|                         DMRUNTIME                                |
+------------------------------------------------------------------+
| Physical I/O polling                                             |
| Modbus cycle                                                     |
| EventManager processing                                         |
| weighted task rotation                                          |
| asynchronous scheduler                                          |
| automation cycle                                                |
| watchdog and diagnostics                                        |
+------------+----------------+----------------+-------------------+
             |                |                |
             v                v                v
       TASK PESATI       INTERVALLI       STATE MACHINES
             |                |                |
             +----------------+----------------+
                              |
                              v
                       PROGRESSIONE LOOP
```

Ogni sottosistema deve dichiarare:

- criterio di avanzamento;
- frequenza o intervallo;
- prerequisiti;
- timeout;
- retry;
- risorse possedute;
- stato diagnostico.

## 6. Introspection e Semantic Resolver

```text
CONFIGURAZIONE E OGGETTI RUNTIME
  +-- Buffer
  +-- DMRuntimeStore
  +-- DeviceProfiles
  +-- Automation
  +-- AEE
  +-- HVAC / Power / Solar / Averages
  +-- MQTT / Web API metadata
                 |
                 v
+--------------------------------+
|       DOMOINTROSPECTION        |
| modello strutturale read-only  |
+---------------+----------------+
                |
                v
+--------------------------------+
|    DOMOSEMANTICRESOLVER        |
| modello semantico pubblicabile |
+---------------+----------------+
                |
         +------+------+----------------+
         |             |                |
         v             v                v
  DIAGNOSTICA    HOME ASSISTANT   ALTRE PROIEZIONI
```

Introspection osserva i componenti registrati, ma non ne possiede il lifecycle e non implementa la logica dei protocolli frontend.

## 7. Home Assistant: pubblicazione

```text
BUFFER / DMRUNTIMESTORE
          |
          v
   DOMOINTROSPECTION
          |
          v
 DOMOSEMANTICRESOLVER
          |
          v
   DMHOMEASSISTANT
 discovery, topic, payload,
 availability, device registry
          |
          v
   DMHAORCHESTRATOR
 lifecycle, queue, snapshot
          |
          v
    MQTT TRANSPORT
          |
          v
      MQTT BROKER
```

## 8. Home Assistant: comandi

```text
MQTT BROKER
     |
     v
MQTT TRANSPORT CALLBACK
     |
     v
DMHAORCHESTRATOR
     |
     +--> valida topic area
     |         |
     |         v
     |    BUFFER / EVENTMANAGER
     |
     +--> valida runtime ID e handle
               |
               v
          DMRUNTIMESTORE
```

Il percorso inbound deve applicare:

- validazione dell'entity;
- controllo del tipo;
- controllo dell'accesso;
- conversione del payload;
- aggiornamento tramite owner corretto;
- feedback sul topic di stato quando previsto.

## 9. Sequenza di connessione Home Assistant

```text
MQTT CONNECTED
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
PUBLISH INITIAL SNAPSHOT
      |
      v
PUBLISH INCREMENTAL CHANGES
```

In caso di disconnessione, le flag relative a subscription, availability, discovery e snapshot devono essere ripristinate per consentire una nuova inizializzazione completa.

## 10. Security read/write

Il codec Security deve essere condiviso dai percorsi di lettura e scrittura.

```text
PANEL / WIRED SENSORS
          |
          v
+---------------------------+
| SECURITY CODEC / HELPERS  |
| bit order e semantica     |
+-------------+-------------+
              |
              v
     STATUS AREAS / BUFFER
              |
              v
      INTROSPECTION MODEL
              |
              v
          FRONTEND
```

Percorso dei comandi:

```text
FRONTEND COMMAND
       |
       v
ENTITY VALIDATION
       |
       v
+---------------------------+
| SECURITY CODEC / HELPERS  |
| stessa sequenza dei bit   |
+-------------+-------------+
              |
              v
     COMMAND AREAS / EVENT
              |
              v
       SECURITY ENGINE
              |
              v
      PANEL / ACTUATION
```

Il frontend non deve duplicare descrittori, offset o significato dei bit.

## 11. Networking e ownership dei socket

```text
PROTOCOL ENGINE
 MQTT / Modbus / Web API / Bridge / Solar TCP
                 |
                 v
+----------------------------------+
|        DMNETWORKMANAGER          |
| registrazione, arbitraggio,      |
| pacing, protocol state           |
+----------------+-----------------+
                 |
                 v
+----------------------------------+
|          SOCKETMANAGER           |
| owner, kind, slot, lifecycle     |
+----------------+-----------------+
                 |
                 v
+----------------------------------+
|       PLATFORM NETWORK           |
| Ethernet / W5500 / TCP / UDP     |
+----------------------------------+
```

Lifecycle minimo dello slot:

```text
REQUEST
   |
   v
ACQUIRE OWNER + SLOT
   |
   v
CONNECT / TRANSACT
   |
   +--> success ------------+
   |                         |
   +--> failure / timeout ---+
                             |
                             v
                       STOP IF NEEDED
                             |
                             v
                       RELEASE SLOT
```

Il protocollo che acquisisce uno slot deve conservarne l'identità e rilasciarlo quando termina l'operazione, cambia endpoint, perde la connessione o cambia owner.

## 12. FrontendStatus

```text
+---------------------------------------------------------+
|                   FRONTENDSTATUS                        |
+---------------------------------------------------------+
| bit 0   Enabled                                         |
| bit 1   Running                                         |
| bit 2   Initialized                                     |
| bit 3   Connected                                       |
| bit 4   Active                                          |
| bit 5   Recovering                                      |
| bit 6   Degraded                                        |
| bit 7   Fault                                           |
| bit 8-15 Reserved                                       |
+---------------------------------------------------------+
```

FrontendStatus permette di distinguere:

- abilitazione;
- esecuzione;
- inizializzazione;
- connessione;
- attività;
- recovery;
- degradazione;
- fault.

`Connected` non deve essere interpretato come requisito universale di operatività per ogni frontend.

## 13. Servizi trasversali

```text
+----------------------+  +----------------------+  +----------------------+
| TIME MANAGER         |  | DIAGNOSTICS          |  | WATCHDOG             |
| millis, epoch, RTC   |  | model, runtime,      |  | progress, spike,     |
| sync se disponibile |  | network, resources   |  | overload, inactivity |
+----------------------+  +----------------------+  +----------------------+

+----------------------+  +----------------------+  +----------------------+
| DMNETWORKMANAGER     |  | FRONTENDSTATUS       |  | HOT STANDBY          |
| pacing, arbitration |  | health and lifecycle |  | heartbeat, role,     |
| protocol state      |  | change detection     |  | state synchronization|
+----------------------+  +----------------------+  +----------------------+
```

Questi servizi sono trasversali, ma non devono accedere arbitrariamente allo stato privato dei domain engine.

## 14. Lifecycle di avvio

```text
BOOT
  |
  v
COMPOSITION ROOT
  | configuration and registrations
  v
PLATFORM / NETWORK SETUP
  v
CORE AND RUNTIME SETUP
  | Buffer, devices, EventManager, Physical I/O
  v
DOMAIN ENGINE SETUP
  v
RUNTIMESTORE REGISTRATION
  v
INTROSPECTION BUILD
  v
SEMANTIC RESOLUTION
  v
FRONTEND ORCHESTRATOR SETUP
  v
POWER-ON PROGRESSION
  v
NORMAL OPERATION
```

Il diagramma rappresenta l'ordine logico. Le fasi possono essere condizionali in base alla piattaforma e ai servizi abilitati.

L'inizializzazione della stessa risorsa non deve essere duplicata in fasi differenti.

## 15. Regole di dipendenza

```text
APPLICATION / COMPOSITION
            |
            v
FRONTEND ORCHESTRATION
            |
   +--------+--------+----------------+
   |                 |                |
   v                 v                v
DOMAIN ENGINES    MODEL        PROTOCOL ENGINES
   |                 |                |
   +--------+--------+----------------+
            |
            v
STATE / EVENTS / RESOURCES
            |
            v
FOUNDATION / PLATFORM
```

Regole:

- i domain engine non dipendono da Home Assistant;
- Buffer e RuntimeStore non dipendono dai frontend;
- Semantic Resolver dipende dal modello Introspection;
- i protocolli usano DMNetworkManager e SocketManager quando condividono risorse;
- le collaborazioni laterali passano attraverso stato, eventi o interfacce comuni;
- il backend di piattaforma non dipende dalla configurazione dell'installazione.

## 16. Checklist architetturale

Prima di aggiungere un nuovo blocco verificare:

- [ ] responsabilità unica;
- [ ] owner esplicito;
- [ ] stato applicativo distinto dallo stato tecnico;
- [ ] uso corretto di Buffer o RuntimeStore;
- [ ] eventi prodotti e consumati documentati;
- [ ] criterio di scheduling dichiarato;
- [ ] comportamento non-blocking;
- [ ] timeout e retry definiti;
- [ ] risorse di rete possedute e rilasciate;
- [ ] inizializzazione non duplicata;
- [ ] recovery dopo disconnessione;
- [ ] diagnostica disponibile;
- [ ] nessuna dipendenza verso layer superiori;
- [ ] nessuna mappa semantica duplicata nel frontend.

## 17. Principio finale

> **Nessun servizio secondario deve bloccare il ciclo applicativo. Stato, eventi, risorse e lifecycle devono avere owner espliciti ed essere diagnosticabili.**

L'architettura di DomoManager non obbliga tutti i componenti a utilizzare lo stesso meccanismo interno. Impone che ogni componente dichiari:

- cosa possiede;
- da cosa dipende;
- come avanza;
- come comunica;
- come recupera;
- come viene osservato.

---

**DOMOMANAGER**  
**ARCHITETTURA A BLOCCHI**  
**STATE. EVENTS. OWNERSHIP. PROGRESSION.**
