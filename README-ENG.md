<img width="953" height="229" alt="DomoManager Logo" src="https://github.com/user-attachments/assets/eb342960-48e6-4788-a0da-986a7873fbc5" />

# DomoManager

**The operating system for the home, designed with an industrial philosophy.**

DomoManager is designed with a clear objective: to move home automation from the world of **wireless gadgets** toward a more **reliable, deterministic, wired, and diagnosable infrastructure**.

It is designed for homes that require:

- reliability
- deterministic behavior
- wired infrastructure
- minimal dependence on RF
- clear and traceable operation
- extensibility
- cloud independence

DomoManager is not simply a home automation controller.

It is an **embedded runtime for residential infrastructure**, designed to coordinate devices, automation, energy, HVAC, security, weather, and communication within a single architecture.

---

# 🔗 Official Links

### 🌐 Website

**https://www.domo-manager.it**

### 📧 Contact

**mail@domo-manager.it**

### 📘 Facebook

**https://www.facebook.com/people/DOMO-Manager/61573260488406**

**https://www.facebook.com/groups/1310697874371245/**

### 📄 Documentation

**https://www.domo-manager.it/biblioteca-documenti/**

---

# 🎯 Mission

> **Transform home automation from a collection of wireless gadgets into a reliable, wired, industrial-grade infrastructure.**

The philosophy behind DomoManager is simple:

**the fundamental functions of a home should not depend on the cloud, Wi-Fi, or a chain of external services.**

For this reason, DomoManager prioritizes:

- wired I/O
- RS485
- Modbus RTU
- Modbus TCP
- Ethernet
- deterministic protocols
- local processing

Wireless technologies can be used when they are useful, but they are not intended to be the foundation of the system.

---

# 🧠 What makes DomoManager different?

## Deterministic by design

DomoManager is built around predictable execution cycles and clearly separated responsibilities.

The runtime minimizes, wherever possible:

- implicit behavior
- recurring dynamic allocations
- prolonged blocking operations
- hidden dependencies between services

The goal is not simply to perform as many operations as possible, but to execute them in a **predictable and controllable way**.

---

## Buffer-centric architecture

The **Buffer Engine** represents the central state of the system.

The fundamental model is:

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

The Buffer provides:

- typed areas
- timestamps
- change tracking
- virtual areas
- reverse
- split
- toggle
- fast access
- diagnostics

This allows the different Engines to operate on a consistent representation of the system state.

---

# ⚙️ Runtime and Task Engine

The frontend uses a centralized task orchestration system.

Application tasks are registered with:

- callback
- execution interval
- enabled/disabled state

Standard tasks include:

- Security / Sensors
- HVAC
- Averages
- Communication

This separation allows application logic and networking to remain independent.

---

# 🔄 Communication Scheduler

Communication services are managed through a dedicated scheduler.

```text
             COMMUNICATION SCHEDULER
                       │
          ┌────────────┼────────────┐
          ▼            ▼            ▼
       BRIDGE        MQTT        WebAPI
```

Each service can have:

- execution interval
- priority
- enabled state
- dedicated callback

The **timed round-robin model** prevents a single communication service from monopolizing the frontend cycle.

This is particularly important for embedded systems where CPU, memory, network connections, and Ethernet sockets are limited resources.

### Core principle

> **No secondary service should be able to block the main application cycle.**

---

# 🏗 Architecture

The overall architecture can be represented as follows:

```text
                         PHYSICAL WORLD
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

The architectural principle is:

**Backend → Buffer → Frontend → Automation → Communication**

with a clear separation between application processing and communication services.

---

# 🏠 Frontend Engines

## HVAC Engine

Advanced HVAC management:

- heat pumps
- multiple zones
- domestic hot water
- anti-legionella cycles
- defrost
- indoor/outdoor temperature
- outdoor protection
- window management
- HVAC schedules

---

## ⚡ Power Engine

Intelligent energy management:

- prioritized loads
- grid protection
- solar forecasting
- dynamic load management
- auto-tuning

---

## 🌦 Weather Engine

Weather data processing:

- temperature
- rainfall
- wind
- light levels
- weather events
- alarms
- O(1) moving averages

---

## 🛡 Security Engine

Security management:

- wired sensors
- zones
- alarm states
- state aggregation
- events
- diagnostics

---

# 🤖 Automation Engine

Automations are declarative and configuration-driven.

Supported features include:

- scenes
- rules
- multiple conditions
- timed sequences
- scheduled rules
- trends
- debounce
- composite rules
- JSON configuration
- validation

The goal is to avoid hardcoded automation logic and make system behavior:

**readable, predictable, and diagnosable.**

---

# 🌐 Integrations

## MQTT

DomoManager can integrate with external ecosystems through MQTT, including:

- Home Assistant
- Zigbee2MQTT
- Shelly

DomoManager does not aim to replace Home Assistant.

Its purpose is to provide a **local industrial-grade backend** that can work alongside Home Assistant.

---

## WebAPI

The WebAPI Engine provides HTTP communication following a non-blocking execution model.

It supports:

- GET
- POST
- message profiles
- request/response correlation

---

## Modbus

Support for:

- Modbus RTU
- Modbus TCP

with:

- round-robin polling
- deterministic retries
- timeout handling
- controlled writes
- error management

---

## RS485

Communication designed for embedded industrial systems:

- frames
- ACK/NACK
- retries
- state machines
- non-blocking execution

---

# 🔁 Hot Standby

DomoManager supports a Master/Slave architecture.

The system can manage:

- Master state
- Slave state
- failover
- state replication
- process replication
- timestamps

The active node handles operational communications while the secondary node remains ready to take over.

---

# 🔍 Industrial Diagnostics

Diagnostics are an integral part of the architecture.

DomoManager provides analysis and monitoring for:

- Buffer
- automations
- scheduler
- tasks
- communication services
- split operations
- devices
- RTC
- security
- HVAC
- Power
- Weather
- watchdog
- watch areas

The objective is not only to answer:

**"What is not working?"**

but also:

**"Why is it not working?"**

---

# 📊 Performance Principles

DomoManager is designed for embedded hardware with limited resources.

The architecture makes use of:

- O(1) Buffer lookups
- O(1) moving averages
- timed scheduling
- round-robin execution
- reduction of redundant operations
- controlled dynamic allocation
- service separation
- non-blocking communication

Performance is considered together with:

**determinism + reliability + predictability.**

---

# 🧰 Hardware and Embedded Philosophy

DomoManager is designed with embedded controllers and resource-constrained systems in mind.

This requires particular attention to:

- memory
- CPU usage
- Ethernet sockets
- concurrent connections
- timeouts
- watchdog behavior
- error handling
- failover behavior

Hardware resource availability is therefore considered part of the software architecture.

---

# 🧪 Project Status

## BETA

DomoManager is currently in **Beta**.

The core architecture has been consolidated, but several areas are still evolving.

Current development areas include:

- HVAC / Power documentation
- additional WebAPI profiles
- Power auto-tuning
- Communication Scheduler tuning
- advanced Ethernet resource diagnostics
- Hot Standby testing
- load testing
- graphical configuration tools

The Beta should therefore be considered a solid foundation toward **version 1.0**, rather than a final production release.

---

# 🗺 Roadmap

The main objectives toward RC1 include:

- official JSON editor
- diagnostic dashboard
- execution-time diagnostics
- Ethernet resource diagnostics
- socket monitoring
- additional WebAPI profiles
- improved solar forecasting
- optimized DHW / anti-legionella management
- support for additional heat-pump models
- extended RS485 testing
- extended Hot Standby testing
- Ethernet load testing

For further details, see **ROADMAP.md**.

---

# 📚 Documentation

Technical documentation covers:

- project philosophy
- general architecture
- runtime architecture
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
- Hot Standby
- diagnostics

Public documentation is available on the official website:

**https://www.domo-manager.it/biblioteca-documenti/**

---

# 🤝 Contributing

Interested in contributing to DomoManager?

Please read:

- **CONTRIBUTING.md**
- **CODE_OF_CONDUCT.md**
- **ROADMAP.md**

Contributions are welcome when they are consistent with the project's architectural principles.

### Core principles

> **Deterministic.**  
> **Non-blocking.**  
> **Modular.**  
> **Diagnosable.**  
> **Embedded-first.**  
> **Local-first.**

---

# 📜 License

For licensing information, see the **LICENSE** file included in the repository.

---

# ============================================================
# DOMOMANAGER
# THE OPERATING SYSTEM FOR THE HOME
# ============================================================