# DOMOMANAGER - ARCHITETTURA A BLOCCHI

## Modello completo

```text
+------------------------------------------------------------------+
|                         MONDO FISICO                              |
| Sensori, attuatori, Modbus, RS485, Local I/O, Ethernet, RTC      |
+-------------------------------+----------------------------------+
                                |
                                v
+------------------------------------------------------------------+
| BACKEND / DEVICE LAYER                                           |
| Device Manager, DeviceProfiles, Modbus Engine, Local I/O         |
| Routing, polling, retry, timeout, cooldown, error handling        |
+-------------------------------+----------------------------------+
                                |
                                v
+------------------------------------------------------------------+
| BUFFER ENGINE                                                     |
| Single Source of Truth per le aree                               |
| Typed areas, timestamp, change tracking, virtual areas, events   |
+-----------+---------------------+----------------------+-----------+
            |                     |                      |
            v                     v                      v
+---------------------+  +---------------------+  +-----------------+
| APPLICATION ENGINES |  | AUTOMATION ENGINE   |  | INTROSPECTION   |
| Security            |  | Scenes              |  | Entity          |
| HVAC                 |  | Rules               |  | Relation        |
| Averages             |  | Sequences           |  | Automation      |
| Power                |  | Dynamic automation  |  | Validation      |
| Solar                |  | Trends / Debounce   |  | Origin / Group  |
| Weather              |  | Composite / Bitmask |  | IOType          |
+----------+----------+  +----------+----------+  +--------+--------+
           |                        |                      |
           +------------+-----------+                      v
                        |                         +------------------+
                        v                         | SEMANTIC RESOLVER |
                    +-------+                     | Kind, unit,      |
                    |BUFFER |                     | class, access,   |
                    +---+---+                     | visibility       |
                        |                         +--------+---------+
                        |                                  |
                        +------------------+---------------+
                                           |
                                           v
+------------------------------------------------------------------+
| COMMUNICATION MODEL                                               |
|                                                                  |
| DMHomeAssistant -> discovery, topic, payload, Device Registry    |
| DMHAOrchestrator -> lifecycle, snapshot, queue, inbound commands |
| HAMQTTTransport -> adattamento al client MQTT                     |
+-------------------------------+----------------------------------+
                                |
                                v
+------------------------------------------------------------------+
| COMMUNICATION SCHEDULER                                           |
| Timed round-robin, interval, priority, service isolation         |
+-------------------+----------------------+-----------------------+
                    |                      |
          +---------+---------+     +------+------+
          |                   |     |             |
          v                   v     v             v
        MQTT                WEBAPI BRIDGE        RS485
```

## RuntimeStore

```text
+------------------------------------------------------------------+
| DMRUNTIMESTORE                                                    |
| Variabili tipizzate non naturalmente riconducibili ad aree       |
| BOOL, INT32, FLOAT, STRING, JSON                                  |
| READ_ONLY, WRITE_ONLY, READ_WRITE, persistence, metadata          |
+-------------------------------+----------------------------------+
                                |
                +---------------+----------------+
                |                                |
                v                                v
        DomoIntrospection                Application consumers
                |
                v
        DomoSemanticResolver
                |
                v
        Home Assistant projection
```

## Security read/write

```text
+-----------------------+
| Panel / Sensor state  |
+-----------+-----------+
            |
            v
+-----------------------+
| Security codec/helper |
| sequenza bit unica    |
+-----------+-----------+
            |
            v
+-----------------------+
| Status areas / Buffer |
+-----------+-----------+
            ^
            |
+-----------+-----------+
| Command areas         |
+-----------+-----------+
            |
            v
+-----------------------+
| Security commands     |
+-----------------------+
```

## Servizi trasversali

```text
+-------------------+  +-------------------+  +-------------------+
| TIME MANAGER      |  | DIAGNOSTICS       |  | WATCHDOG          |
| RTC, NTP, epoch   |  | Model, runtime,   |  | Progress, spike,  |
| callbacks         |  | network, resource |  | overload          |
+-------------------+  +-------------------+  +-------------------+

+-------------------+  +-------------------+
| RESOURCE CONTROL  |  | HOT STANDBY       |
| Socket, TCP, RAM, |  | Master, Slave,    |
| CPU, bus budget   |  | heartbeat, role   |
+-------------------+  +-------------------+
```

## Principio

Nessun servizio secondario deve bloccare il ciclo applicativo. Le risorse hardware e di comunicazione sono parte dell'architettura e devono essere gestite e diagnosticate esplicitamente.
