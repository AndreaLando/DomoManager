# DomoManager

**Deterministic. Non-blocking. Local-first. Embedded-first.**

DomoManager is an embedded runtime for residential infrastructure. It coordinates devices, shared application state, automation, security, HVAC, energy management, and external integrations while keeping responsibilities, lifecycles, and resource ownership explicit.

It is not intended to replace Home Assistant or act merely as a protocol gateway. Its purpose is to govern the local installation using principles derived from industrial systems:

- predictable behavior;
- local operation;
- non-blocking communication;
- coherent shared state;
- explicit resource ownership;
- integrated diagnostics;
- controlled recovery;
- support for resource-constrained embedded platforms.

> **DomoManager brings industrial-system discipline to the home without giving up modern integration flexibility.**

## Contents

- [Official links](#official-links)
- [Vision](#vision)
- [Architectural principles](#architectural-principles)
- [Architecture overview](#architecture-overview)
- [Application state](#application-state)
- [Runtime and events](#runtime-and-events)
- [Backend and protocols](#backend-and-protocols)
- [Domain engines](#domain-engines)
- [Automation Engine](#automation-engine)
- [Introspection and semantics](#introspection-and-semantics)
- [Home Assistant](#home-assistant)
- [Networking and resources](#networking-and-resources)
- [Diagnostics and watchdog](#diagnostics-and-watchdog)
- [Platforms](#platforms)
- [Documentation structure](#documentation-structure)
- [Contributing rules](#contributing-rules)
- [Project status](#project-status)

## Official links

- [Website](https://www.domo-manager.it)
- [Public documentation](https://www.domo-manager.it/biblioteca-documenti/)
- [Facebook page](https://www.facebook.com/people/DOMO-Manager/61573260488406)
- [Facebook group](https://www.facebook.com/groups/1310697874371245/)
- Contact: `mail@domo-manager.it`

## Vision

Home automation should not be a collection of independent devices tied to external services. It should operate as a coherent system that can:

1. acquire information from the physical world;
2. validate and represent application state;
3. evaluate conditions;
4. make decisions;
5. control devices and actuators;
6. expose a structured model to external systems;
7. explain its behavior through diagnostics and logs.

DomoManager prioritizes:

- wired I/O, Ethernet, RS485, and Modbus for critical functions;
- wireless and cloud services when they add value, not as operational requirements;
- local processing;
- declarative configuration;
- separation between application logic, frontends, and protocols.

Home Assistant is a projection and supervision layer. DomoManager retains local control of installation state, automation, and actuation.

## Architectural principles

### Determinism

Each component must declare:

- when it is eligible to run;
- what bounded work it performs;
- which resources it owns;
- which timeouts and retry policies it applies;
- how it reports state;
- how it recovers after failure.

### Non-blocking execution

Communication services advance through controlled steps. MQTT, Web API, Bridge, Modbus, RS485, and dedicated transports must not monopolize the application cycle.

### Local-first

Essential functions must continue to operate without Internet connectivity, cloud services, or external integrations.

### Embedded-first

CPU, RAM, sockets, buses, connections, and cycle time are architectural resources. Every limited resource requires an owner, lifecycle, limits, diagnostics, and a release or recovery policy.

### Explicit ownership

Configuration and initialization must not be duplicated. The composition root defines the system; the owning component initializes and manages the resource.

### No duplicated semantics

Bitmasks, mappings, units, scaling, and read/write rules belong to the owning domain. Frontends consume that model instead of maintaining parallel tables.

## Architecture overview

```text
PHYSICAL WORLD
      |
      v
BACKEND / DEVICE / PHYSICAL I/O
      |
      +---------------------+
      |                     |
      v                     v
   BUFFER             DMRUNTIMESTORE
  area -> long        handle -> typed value
      |                     |
      +----------+----------+
                 |
                 v
        EVENTMANAGER / DMRUNTIME
                 |
       +---------+----------+
       |                    |
       v                    v
 DOMAIN ENGINES       AUTOMATION ENGINE
       |                    |
       +---------+----------+
                 |
                 v
        SHARED STATE / COMMANDS
                 |
       +---------+----------+
       |                    |
       v                    v
 INTROSPECTION       PROTOCOL ORCHESTRATORS
       |                    |
       v                    v
 SEMANTIC RESOLVER   MQTT / WEB API / BRIDGE
       |                    |
       v                    v
 FRONTEND MODELS     DMNETWORKMANAGER
                            |
                            v
                      SOCKETMANAGER
```

The architecture separates:

1. acquisition and actuation;
2. shared application state;
3. events and runtime coordination;
4. domain logic and automation;
5. structural and semantic modeling;
6. frontend, protocol, and network-resource lifecycles.

## Application state

### Buffer

The Buffer is authoritative for shared numeric area state. It provides numeric values, timestamps, change tracking, virtual areas, write mapping, diagnostics, and the changed-area map.

Route, Toggle, and Split operate on areas but belong to their respective logic managers rather than to Buffer storage itself.

### DMRuntimeStore

`DMRuntimeStore` complements the Buffer for values that do not naturally fit `area -> long`.

It supports:

- `BOOL`, `INT32`, and `FLOAT`;
- `STRING32` and `STRING64`;
- `JSON128` and `JSON256`;
- `READ_ONLY`, `WRITE_ONLY`, and `READ_WRITE` access;
- semantic metadata;
- declared persistence and sensitivity;
- revision and dirty tracking;
- integrity checks;
- O(1) access through handles.

Runtime code uses handles. Text IDs are mainly used during setup, introspection, discovery, and external-command parsing.

### Internal technical state

Timers, debounce state, queues, pending requests, connections, retry counters, cooldowns, snapshots, and state machines remain inside the owning engine. They must be diagnosable whenever they affect operation.

## Runtime and events

### EventManager

`EventManager` decouples event production from consumption. Sources include Modbus, physical I/O, Route, Toggle, Split, automation, frontend commands, and internal services.

A bounded number of events is processed per loop iteration to avoid recursive callback chains and runtime monopolization.

### DMRuntime

`DMRuntime` coordinates:

- `PhysicalIOManager`;
- the Modbus cycle;
- `EventManager`;
- Route, Toggle, Split, and threshold logic;
- `AutomationEngine`;
- the asynchronous scheduler;
- averages;
- application callbacks;
- watchdog and diagnostics;
- backend-cycle completion.

The runtime combines weighted tasks, rotation counters, time intervals, cycle prerequisites, callbacks, network pacing, and state machines. It is not a single uniform Task Engine that fully executes every subsystem on every iteration.

## Backend and protocols

### Device Manager

The Device Manager owns programmable devices, hardware profiles, channels, device-to-area associations, visible errors, parked state, and device diagnostics.

### Modbus

The Modbus engine supports RTU and TCP, IP selection, round-robin polling, pending writes, pacing, timeouts, retries, cooldown or exclusion, persistent connections, and integration with the network runtime.

Pending writes may receive controlled priority without bypassing IP policy, network arbitration, or resource lifecycle rules.

### Physical I/O

`PhysicalIOManager` owns local-I/O initialization, polling, active-level conversion, non-blocking debounce, input-event publication, and output control. Hardware initialization must have one owner.

### Web API

The Web API engine provides HTTP client and server state machines, queued requests, response correlation, timeout handling, authentication, key/value and JSON parsing, chunked-body handling, application profiles, and webhooks.

### Bridge and AEE

AEE describes synchronized variables, direction, and schema. Bridge manages heartbeat, acknowledgements, retransmission, initial snapshots, alignment, and multiframe schema transfer.

### Solar

The Solar subsystem includes UART and TCP transports, asynchronous command discovery, paced polling, parsing and CRC, structured snapshots, area updates, and diagnostics.

## Domain engines

### Security

Security covers wired sensors, RT/H24/MASK/LEN channels, zones, startup inhibition, arming, alarms, tamper and trouble, bypass, memory and silencing, partitions, status words and bitmasks, commands, and diagnostics.

Read and write paths share the same codec and canonical bit order.

### HVAC

HVAC covers zones, setpoints, heat pumps, fan coils, circulation, domestic hot water, defrost, three-way valves, anti-legionella cycles, and opening-related protections.

### Power

Power covers load management and electrical supervision, including limits, priorities, attach/detach suggestions, main-voltage classification, hysteresis, 24 V state, fault, battery operation, extra signals, alarms, and diagnostic snapshots.

### Weather

Weather handles temperature, wind, rain, light, moving averages, day/night transitions, gusts, events, and debounced alarms. The uninitialized state and first sample must be handled without division by zero.

### Averages

Average groups apply input scaling, aggregation, and output scaling. Published values follow:

```text
published value = Buffer value * Entity::scale
Entity::scale   = 1 / outScale
```

Scales and denominators must be validated.

### Access Control

Access Control coordinates readers, code normalization, authorization, relays, callbacks, and associated runtime variables.

## Automation Engine

The Automation Engine operates on application state and supports scenes, threshold rules, trends, bitmasks, time conditions, multiple conditions, composite rules, sequences, JSON configuration, and validation.

Actions become state updates and events. The engine does not need to know the concrete actuator protocol.

## Introspection and semantics

### DomoIntrospection

`DomoIntrospection` builds a structural read-only model while retaining references to the original objects. It describes entities, relations, origin, group, physical type, device and configuration references, runtime handles, access, visibility, scaling, and provenance.

Technical entities may be marked `implementationOnly`: they remain diagnosable but are not externally published.

### DomoSemanticResolver

`DomoSemanticResolver` enriches the structural model with `EntityKind`, unit, device class, state class, access, group, and final visibility.

Resolution applies specialized source semantics, physical structure and relations, label dictionaries, and an `UNKNOWN` fallback. Generic modules must not embed installation-specific `AREA_*` identifiers.

## Home Assistant

The integration is divided into three responsibilities.

### DMHomeAssistant

Generates MQTT Discovery, topics, payloads, availability, Device Registry entries, and semantic entity projections.

### DMHAOrchestrator

Manages connection lifecycle, command subscriptions, availability, discovery, initial snapshots, `PendingState`, incremental updates, command routing, and reconnection recovery.

### HAMQTTTransport

Adapts the orchestrator to the MQTT client through connection, publish, and subscribe callbacks without transferring client ownership to the Home Assistant layer.

### Topics

```text
<prefix>/status
<prefix>/area/<area>/state
<prefix>/area/<area>/set
<prefix>/runtime/<id>/state
<prefix>/runtime/<id>/set
```

### Connection sequence

```text
CONNECTED
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

After disconnection, subscriptions, availability, discovery, and snapshot state must restart. An initial snapshot failure must not prevent later recovery.

Area commands pass through entity validation, parsing, callbacks, and the owning domain. RuntimeStore commands validate ID, handle, type, and access. External commands do not directly bypass the internal model to reach actuators.

## Networking and resources

DomoManager does not use one universal Communication Scheduler for all protocols.

```text
PROTOCOL ENGINES
  +-- Modbus
  +-- MQTT
  +-- Web API
  +-- Bridge
  +-- Solar TCP
          |
          v
DMNETWORKMANAGER
  +-- protocol registration
  +-- arbitration
  +-- pacing
  +-- protocol state
          |
          v
SOCKETMANAGER
  +-- owner
  +-- kind
  +-- exact slot
  +-- lifecycle
          |
          v
PLATFORM NETWORK
```

Each protocol declares its owner, progress criterion, pacing or priority, timeout, retry policy, requested resources, release conditions, and diagnostics.

## FrontendStatus

`FrontendStatus` represents common frontend technical state through a 16-bit word:

```text
bit 0   Enabled
bit 1   Running
bit 2   Initialized
bit 3   Connected
bit 4   Active
bit 5   Recovering
bit 6   Degraded
bit 7   Fault
bit 8-15 Reserved
```

`Connected` is not a universal operational requirement because some frontends can be operational without a connected client.

## Diagnostics and watchdog

Diagnostics cover Buffer and RuntimeStore, devices and profiles, initialization anomalies, EventManager, automation and scheduling, execution timing and spikes, Modbus and RS485, Ethernet and socket ownership, MQTT recovery, Introspection and semantic resolution, `UNKNOWN` and hidden entities, domain engines, FrontendStatus, and Hot Standby.

The watchdog protects overall runtime progression. It does not replace protocol-specific error handling.

## Hot Standby

Hot Standby support may include heartbeat, peer detection, master/slave roles, failover and failback, state replication, process-data replication, and time synchronization. Availability does not mean the feature is enabled in every build or configuration.

## Platforms

The platform backend abstracts Arduino Opta and ESP32-S3 differences, including GPIO, diagnostic LEDs, RTC, Ethernet, W5500, I/O expanders, TCP client/server creation, and common Modbus APIs.

Platform-specific conditions should remain under `DMPlatform`, `DMPlatformNetwork`, and `DMPlatformModbus` rather than being distributed across domain engines.

## Documentation structure

1. `1_FILOSOFIA.md`  
   Project philosophy and design criteria.

2. `2_PANORAMICA.md`  
   General overview of components, domains, and integrations.

3. `3_ARCHITETTURA.md`  
   Detailed architecture, lifecycle, and ownership.

4. `4_DIAGRAMMA_DIPENDENZE_FILES.md`  
   Physical, architectural, and runtime dependencies.

5. `5_DIAGRAMMA_ARCHITETTURA.md`  
   Architecture block diagrams.

6. `6_DIAGRAMMA_FLUSSO_DATI.md`  
   Data, event, command, and publication flows.

7. `7_DIAGRAMMA_CICLO_EXEC.md`  
   Incremental execution, weighted tasks, protocol progression, and recovery.

The README is the entry point. The following documents progressively increase technical depth.

## Contributing rules

Each new module should declare responsibility, owner, dependencies, application state, internal technical state, produced and consumed events, scheduling criterion, timeouts, retries, resource use, lifecycle, recovery, diagnostics, and optional Introspection or semantic integration.

### Checklist

- [ ] unique initialization ownership;
- [ ] no duplicated configuration;
- [ ] shared state separated from internal technical state;
- [ ] correct choice between Buffer and RuntimeStore;
- [ ] bounded work per iteration;
- [ ] validated indices, types, and scales;
- [ ] explicit handling of uninitialized state;
- [ ] no division by zero;
- [ ] shared read/write codecs;
- [ ] correct socket and connection release;
- [ ] recovery after disconnection;
- [ ] initial snapshots where required;
- [ ] completion flags updated only after real progression;
- [ ] sufficient diagnostics;
- [ ] no installation-specific `AREA_*` identifiers in generic modules;
- [ ] compatibility with declared platforms.

## Project status

DomoManager is evolving. Firmware, architecture, and documentation must progress together.

Before a feature is considered stable, verify behavior on the target platform, recovery after failures, resource use, diagnostics, consistency with Buffer, RuntimeStore, and EventManager, frontend integration, and absence of regressions.

## Final objective

DomoManager aims to provide a home-automation foundation that is:

- as reliable as an industrial system;
- as readable as a modern framework;
- local;
- predictable under load;
- extensible;
- diagnosable;
- aware of embedded-resource limitations.

> **Complexity should not be transferred to the user. It should be governed by the architecture.**

---

**DOMOMANAGER**  
**THE OPERATING SYSTEM FOR THE HOME**  
**STATE. EVENTS. OWNERSHIP. PROGRESSION.**
