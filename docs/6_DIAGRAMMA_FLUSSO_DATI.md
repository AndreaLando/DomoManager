# DOMOMANAGER - FLUSSO DATI

## 1. Acquisizione fisica

```text
SENSORI / DISPOSITIVI
        |
        v
DEVICE MANAGER / PROFILES
        |
        v
MODBUS / RS485 / LOCAL I/O
        |
        v
BUFFER UPDATE
```

Il Backend normalizza i dati nel Buffer applicando mapping, timestamp, change tracking ed eventi interni.

## 2. Elaborazione applicativa

```text
                       BUFFER
                          |
       +------------------+------------------+
       |                  |                  |
       v                  v                  v
   SECURITY             HVAC              AVERAGES
       |                  |                  |
       +------------------+------------------+
                          |
                    POWER / SOLAR
                          |
                       WEATHER
                          |
                          v
                       BUFFER
```

Gli engine leggono lo stato corrente e scrivono risultati o comandi tramite il modello interno.

## 3. Automazioni

```text
BUFFER
  |
  v
CONDITIONS / EVENTS / TIME
  |
  v
AUTOMATION ENGINE
  |
  +-- scenes
  +-- rules
  +-- sequences
  +-- dynamic automations
  |
  v
BUFFER / INTERNAL EVENTS
```

## 4. Uscita verso il mondo fisico

```text
BUFFER COMMAND
      |
      v
DEVICE MANAGER / ROUTING
      |
      +--> MODBUS
      +--> RS485
      +--> LOCAL I/O
      |
      v
ATTUATORI / DISPOSITIVI
```

## 5. Costruzione del modello

```text
CONFIGURAZIONE
AREE / DEVICE / PROFILES
ROUTE / TOGGLE / SPLIT
AUTOMATION
HVAC / AVERAGES / SOLAR
MQTT / AEE / WEBAPI
RUNTIMESTORE
        |
        v
DomoIntrospection
        |
        v
DomoSemanticResolver
        |
        v
MODELLO PUBBLICABILE
```

L'Introspection descrive struttura e relazioni. Il resolver assegna semantica, accesso, gruppo, unita e classi.

## 6. Esposizione Home Assistant

```text
MODELLO SEMANTICO
       |
       v
DMHomeAssistant
 Discovery / topics / payload / Device Registry
       |
       v
DMHAOrchestrator
 availability / discovery / snapshot / PendingState
       |
       v
HAMQTTTransport
       |
       v
MQTT BROKER
       |
       v
HOME ASSISTANT
```

### Flusso area in uscita

```text
Buffer event
 -> DMHAOrchestrator::publishEvent
 -> PendingState
 -> scale / type conversion
 -> <prefix>/area/<area>/state
```

### Flusso RuntimeStore in uscita

```text
DMRuntimeStore entry
 -> Introspection runtime entity
 -> publishRuntimeState
 -> <prefix>/runtime/<id>/state
```

## 7. Comandi esterni

### Comando area

```text
HOME ASSISTANT
 -> MQTT broker
 -> HAMQTTTransport
 -> DMHAOrchestrator
 -> parse e validazione entity
 -> commandCallback
 -> Buffer / engine proprietario
 -> stato reale
 -> state feedback
```

### Comando RuntimeStore

```text
HOME ASSISTANT
 -> <prefix>/runtime/<id>/set
 -> DMHAOrchestrator
 -> lookup handle
 -> controllo type/access
 -> DMRuntimeStore write
 -> normalized inbound message
 -> state feedback
```

Un comando esterno non raggiunge direttamente un attuatore e non aggira il modello interno.

## 8. Security bitmask read/write

```text
PanelState / SensorState / ZoneState
                |
                v
       Security codec/helper
       sequenza segnali unica
                |
                v
          Status areas
                |
                v
              Buffer
                ^
                |
          Command areas
                |
                v
       Security codec/helper
                |
                v
       Panel / Zone / Sensor commands
```

La definizione dei bit appartiene a Security. Le proiezioni esterne consumano il modello risultante senza duplicare la mappa.

## 9. Communication Scheduler

```text
COMMUNICATION TASK
        |
        v
COMMUNICATION SCHEDULER
        |
        +-- Bridge
        +-- MQTT
        +-- WebAPI
```

Ogni servizio e eseguito secondo intervallo, priorita e timed round-robin. Retry e recovery non devono monopolizzare il runtime.

## 10. Flusso completo

```text
MONDO FISICO
     |
     v
BACKEND ------------+
     |               |
     v               |
BUFFER <--------- APPLICATION / AUTOMATION
     |
     +--> INTROSPECTION -> SEMANTICS -> EXTERNAL PROJECTIONS
     |
     +--> BACKEND OUTPUT -> MONDO FISICO

DMRuntimeStore -> INTROSPECTION -> SEMANTICS -> EXTERNAL PROJECTIONS
```

## Regola fondamentale

Il Buffer e la verita per le aree. RuntimeStore e la verita per i tag runtime. L'Introspection descrive, il Semantic Resolver interpreta, gli engine decidono e il Communication Scheduler espone il sistema senza diventare proprietario della logica.
