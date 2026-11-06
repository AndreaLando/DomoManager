# DOMOMANAGER - DIPENDENZE DEI FILE

## 1. Scopo

Il documento distingue:

- dipendenze di compilazione: `#include`;
- dipendenze architetturali: conoscenza tra moduli;
- dipendenze runtime: disponibilita necessaria durante l'esecuzione.

L'obiettivo non e minimizzare gli include, ma mantenere dipendenze comprensibili e prevalentemente unidirezionali.

## 2. Albero architetturale aggiornato

```text
DomoManager.ino
|
+-- DMUsb.hpp
+-- DMFrontend.hpp
|   |
|   +-- DMFrontendEngines.hpp
|   |   +-- SecurityEngine / SecurityOrchestrator
|   |   +-- DMWiredSensors.hpp
|   |   +-- DMHVAC.hpp
|   |   +-- DMPower.hpp
|   |   +-- DMWeather.hpp
|   |   +-- Averages
|   |   +-- Solar
|   |   +-- Access Control / WebAPI clients
|   |
|   +-- DMAutomation.hpp
|   +-- DMAutomationBuilder.hpp
|   +-- DMIntrospection.hpp
|   +-- DMSemanticResolver.hpp
|   +-- DMRuntimeStore.hpp
|   |
|   +-- DMHomeAssistant.hpp
|   +-- DMHAOrchestrator.hpp
|   +-- HAMQTTTransport
|   +-- DMMQTTEngine.hpp
|   |
|   +-- DMBridge.hpp
|   +-- DMWebAPI.hpp
|   +-- DMWebAPIDefs.hpp
|   +-- DMRS485Node.hpp
|   |
|   +-- DMDiagnostic.hpp
|   +-- DMSetup.hpp
|   +-- DMOptaRTC.hpp
|   +-- DMLogger.hpp
|   +-- DMFncs.hpp
|   +-- DMDeclares.h
|
+-- DomoManager Core
    +-- Device Manager
    +-- DeviceProfiles
    +-- Buffer Engine
    +-- Modbus Engine
    +-- Local I/O
    +-- Event Manager
    +-- Task Engine
    +-- Communication Scheduler
    +-- Time Manager
    +-- Watchdog
    +-- Hot Standby
```

I nomi effettivi degli header devono essere verificati contro gli `#include` del repository. Il diagramma rappresenta il contratto architetturale.

## 3. Dipendenze per layer

```text
LAYER 0 - PLATFORM
Arduino Core, Ethernet, Wire, SPI, Opta libraries

LAYER 1 - BASE
DMDeclares, DMFncs, DMLogger, tipi e interfacce comuni

LAYER 2 - STATE E HARDWARE
Buffer, RuntimeStore, DeviceProfiles, Device Manager,
Modbus, Local I/O, RTC, Event Manager

LAYER 3 - LOGIC
Automation, Security, HVAC, Averages, Power, Solar, Weather

LAYER 4 - MODEL
DomoIntrospection, DomoSemanticResolver

LAYER 5 - COMMUNICATION
MQTT, DMHomeAssistant, DMHAOrchestrator,
HAMQTTTransport, WebAPI, Bridge, RS485

LAYER 6 - ORCHESTRATION
TaskEngineOrchestrator, CommunicationScheduler,
DomoManager Core, Setup, Frontend

LAYER 7 - APPLICATION
domomanager.ino e configurazione installazione
```

## 4. Direzione delle dipendenze

```text
APPLICATION
    |
    v
ORCHESTRATION
    |
    +--> LOGIC --------+
    |                  |
    +--> MODEL         v
    |                STATE
    +--> COMMUNICATION |
                       v
                  HARDWARE / PLATFORM
```

Regole:

- gli engine applicativi dipendono da stato e interfacce comuni, non da Home Assistant;
- `DMHomeAssistant` dipende dal modello Introspection, non dagli engine Security o Solar;
- `DMHAOrchestrator` dipende da `DMHomeAssistant`, trasporto, manager e Introspection;
- `DomoSemanticResolver` dipende dal modello strutturale, non dal client MQTT;
- `DMRuntimeStore` non dipende da Home Assistant;
- Logger, Time, Diagnostics, Watchdog e risorse sono servizi trasversali.

## 5. Modello Introspection

```text
Configuration / Runtime metadata
             |
             v
      DomoIntrospection
             |
             v
    DomoSemanticResolver
             |
      +------+------+
      |             |
      v             v
Diagnostics    Communication projections
```

`DomoIntrospection` puo conoscere riferimenti a configurazioni originali, ma non deve possedere la logica del protocollo Home Assistant.

## 6. Home Assistant

```text
DomoIntrospection
       |
       v
DomoSemanticResolver
       |
       v
DMHomeAssistant
       |
       v
DMHAOrchestrator
       |
       v
HAMQTTTransport
       |
       v
MQTT client / broker
```

Il verso inverso dei comandi e:

```text
MQTT broker
  -> HAMQTTTransport
  -> DMHAOrchestrator
  -> validazione entity
  -> Buffer command callback oppure DMRuntimeStore
```

## 7. Security

```text
SecurityEngine
  +-- SecurityOrchestrator
  +-- WiredSensorsManager
  +-- SecurityHmiInterface
  +-- DomoManagerAlarmPanel
  +-- Buffer / Event Manager
```

Il codec delle bitmask appartiene a Security. `DMHomeAssistant` non deve dipendere direttamente da `SecurityEngine` e non deve duplicare descrittori dei bit.

## 8. RuntimeStore

```text
Application registration
          |
          v
    DMRuntimeStore
          |
   DomoIntrospection
          |
 DomoSemanticResolver
          |
 DMHomeAssistant / DMHAOrchestrator
```

Le scritture esterne tornano al RuntimeStore attraverso l'orchestratore, mantenendo controlli di tipo e accesso.

## 9. Dipendenze vietate

Da evitare:

```text
HVAC -> MQTT
Power -> Home Assistant
Security -> WebAPI
DMHomeAssistant -> AREA_* applicative
DomoIntrospection -> payload MQTT
DMRuntimeStore -> DMHAOrchestrator
Logger -> engine applicativi
```

Da preferire:

```text
HVAC -> Buffer
Security -> Buffer / Event Manager
Home Assistant -> Introspection model
MQTT commands -> Orchestrator -> Buffer/RuntimeStore
Diagnostics -> read-only views
```

## 10. Regola anti-circolare

Le dipendenze laterali tra engine devono passare attraverso stato, eventi o interfacce comuni. Un engine non deve includere un altro engine per accedere direttamente alla relativa implementazione.

## 11. Servizi trasversali

```text
Time Manager
Diagnostics
Logger
Watchdog
Resource Manager
Hot Standby
```

Questi servizi osservano o supportano piu layer, ma non devono introdurre dipendenze inverse verso la logica applicativa.

## 12. Principio finale

Un modulo puo dipendere dalle fondamenta del sistema, ma non deve conoscere dettagli dei moduli superiori o laterali quando la collaborazione puo avvenire tramite Buffer, RuntimeStore, eventi, Introspection o interfacce dedicate.
