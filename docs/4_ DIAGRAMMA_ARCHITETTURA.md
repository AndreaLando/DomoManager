# ============================================================
# DOMOMANAGER – ARCHITETTURA A BLOCCHI
# ============================================================

                         ┌──────────────────────┐
                         │       HARDWARE       │
                         │  Opta / Arduino I/O  │
                         │  Sensori / Attuatori │
                         │  Modbus TCP / RTU    │
                         │  RS485 / Ethernet    │
                         └──────────┬───────────┘
                                    │
                                    ▼

# ============================================================
# BACKEND CORE
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                     DOMOMANAGER CORE                       │
│                                                            │
│  - Setup                                                   │
│  - Backend Loop                                            │
│  - Config validation                                       │
│  - Device routing                                          │
│  - Backend cycle management                                │
│  - Watchdog                                                │
│  - Global orchestration                                    │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                       BUFFER ENGINE                        │
│                                                            │
│  - Single Source of Truth                                  │
│  - Typed Areas                                             │
│  - Timestamps                                              │
│  - Change Tracking                                         │
│  - Virtual Areas                                           │
│  - Reverse                                                 │
│  - Split                                                   │
│  - Toggle                                                  │
│  - Fast / deterministic access                             │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                      DEVICE MANAGER                        │
│                                                            │
│  - Device profiles                                         │
│  - Area → register mapping                                 │
│  - Priorities                                              │
│  - Error handling                                          │
│  - Cooldown                                                │
│  - Device state management                                 │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                       MODBUS ENGINE                        │
│                                                            │
│  - Round-robin polling                                     │
│  - Deterministic reads / writes                            │
│  - Timeout handling                                        │
│  - Retry                                                   │
│  - Cooldown                                                │
│  - Error management                                        │
│  - Controlled connection lifecycle                         │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                        TIME MANAGER                         │
│                                                            │
│  - Hardware RTC                                            │
│  - NTP                                                     │
│  - Reliable epoch                                          │
│  - Second / minute / hour / day callbacks                  │
│  - Temporal synchronization                                │
│  - Time base for automation and HVAC                       │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                     AUTOMATION ENGINE                      │
│                                                            │
│  - Scenes                                                  │
│  - Rules                                                   │
│  - Sequences                                               │
│  - Scheduled rules                                         │
│  - Conditions                                              │
│  - Composite logic                                         │
│  - Trends                                                  │
│  - Debounce                                                │
│  - Dynamic automations                                     │
│  - JSON Builder                                            │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

# ============================================================
# FRONTEND RUNTIME
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                    FRONTEND ENGINE                         │
│                                                            │
│  - Frontend orchestration                                  │
│  - Task Engine                                              │
│  - Task registration                                       │
│  - Execution intervals                                     │
│  - Enable / Disable                                        │
│  - Frontend cycle management                               │
│  - Backend ↔ Frontend synchronization                      │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

# ============================================================
# APPLICATION TASKS
# ============================================================

        ┌──────────────────────────────────────────────┐
        │                  TASK ENGINE                 │
        │                                              │
        │  Centralized application task scheduling    │
        └──────────────────────┬───────────────────────┘
                               │
          ┌────────────────────┼────────────────────┐
          │                    │                    │
          ▼                    ▼                    ▼

┌──────────────────────┐ ┌──────────────────────┐ ┌──────────────────────┐
│    SECURITY TASK     │ │      HVAC TASK       │ │   AVERAGES TASK      │
│                      │ │                      │ │                      │
│ - Wired sensors      │ │ - Zones              │ │ - Sensor groups      │
│ - PIR / Door         │ │ - Setpoints           │ │ - Factors            │
│ - Window / Smoke     │ │ - Fan coils           │ │ - Moving averages    │
│ - Flood              │ │ - DHW / Anti-leg.     │ │ - Buffer updates     │
│ - Security states    │ │ - Defrost             │ │ - Internal events    │
│ - Bitmask            │ │ - Protection          │ │                      │
└──────────────────────┘ └──────────────────────┘ └──────────────────────┘


┌────────────────────────────────────────────────────────────┐
│                       POWER ENGINE                          │
│                                                            │
│  - Load management                                         │
│  - Priority management                                     │
│  - Grid limits                                             │
│  - Solar forecast                                          │
│  - Auto-tuning                                             │
└────────────────────────────────────────────────────────────┘


┌────────────────────────────────────────────────────────────┐
│                      WEATHER ENGINE                        │
│                                                            │
│  - Rain                                                   │
│  - Wind                                                   │
│  - Light                                                  │
│  - Temperature                                            │
│  - Moving averages                                        │
│  - Weather events                                         │
└────────────────────────────────────────────────────────────┘


# ============================================================
# COMMUNICATION RUNTIME
# ============================================================

                           ┌──────────────────────┐
                           │ COMMUNICATION TASK   │
                           │                      │
                           │ - Runs in TaskEngine │
                           │ - Master only        │
                           │ - Power-on gating     │
                           └──────────┬───────────┘
                                      │
                                      ▼

                    ┌────────────────────────────────┐
                    │   COMMUNICATION SCHEDULER      │
                    │                                │
                    │  Timed Round-Robin Scheduler  │
                    │                                │
                    │  - Interval                   │
                    │  - Priority                   │
                    │  - Enabled / Disabled         │
                    │  - Non-blocking execution     │
                    │  - Service isolation          │
                    └───────────────┬────────────────┘
                                    │
                  ┌─────────────────┼─────────────────┐
                  │                 │                 │
                  ▼                 ▼                 ▼

        ┌────────────────┐ ┌────────────────┐ ┌────────────────┐
        │     BRIDGE     │ │      MQTT      │ │     WEBAPI     │
        │                │ │                │ │                │
        │ - AEE          │ │ - Home         │ │ - GET / POST   │
        │ - UDP / JSON   │ │   Assistant    │ │ - Patterns     │
        │ - Events       │ │ - Zigbee2MQTT  │ │ - Buffer map   │
        │                │ │ - Shelly        │ │ - Responses    │
        └────────────────┘ └────────────────┘ └────────────────┘


# ============================================================
# OTHER COMMUNICATION / PROTOCOL SERVICES
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                       RS485 ENGINE                          │
│                                                            │
│  - Frames                                                  │
│  - ACK / NACK                                              │
│  - Retry                                                   │
│  - State machines                                          │
│  - Non-blocking communication                              │
└────────────────────────────────────────────────────────────┘


┌────────────────────────────────────────────────────────────┐
│                    MODBUS TCP / RTU                         │
│                                                            │
│  - Modbus TCP                                              │
│  - Modbus RTU                                              │
│  - Polling                                                 │
│  - Reads / Writes                                          │
│  - Retry / Timeout                                         │
│  - Connection management                                   │
└────────────────────────────────────────────────────────────┘


# ============================================================
# HOT STANDBY
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                     HOT STANDBY ENGINE                     │
│                                                            │
│  - MASTER / SLAVE                                          │
│  - Heartbeat                                               │
│  - Failover                                                │
│  - State replication                                       │
│  - Process synchronization                                 │
│  - Ethernet enable / disable                               │
│                                                            │
│  MASTER → communication active                             │
│  SLAVE  → Ethernet / communication disabled               │
└────────────────────────────────────────────────────────────┘


# ============================================================
# RESOURCE MANAGEMENT
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                 COMMUNICATION RESOURCES                    │
│                                                            │
│  - Ethernet sockets                                        │
│  - TCP clients                                              │
│  - MQTT connections                                        │
│  - Modbus connections                                      │
│  - Bridge connections                                      │
│  - Connection lifecycle                                    │
│  - Resource contention                                     │
│                                                            │
│  Hardware limits are part of the architecture.             │
└────────────────────────────────────────────────────────────┘


# ============================================================
# DIAGNOSTICS
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                    DIAGNOSTIC ENGINE                       │
│                                                            │
│  - Automation analysis                                     │
│  - Buffer analysis                                         │
│  - Device analysis                                         │
│  - Task analysis                                           │
│  - Scheduler analysis                                      │
│  - Communication analysis                                  │
│  - Ethernet / socket resources                             │
│  - RTC analysis                                            │
│  - Security analysis                                       │
│  - Power analysis                                          │
│  - HVAC analysis                                           │
│  - Hot Standby analysis                                    │
│  - Watchdog / overload analysis                            │
│  - Structured reports                                      │
└────────────────────────────────────────────────────────────┘


# ============================================================
# EXECUTION MODEL
# ============================================================

                         BACKEND
                            │
                            ▼
                         BUFFER
                            │
                            ▼
                      TASK ENGINE
                            │
             ┌──────────────┼──────────────┐
             ▼              ▼              ▼
           HVAC          SECURITY       AVERAGES
             │              │              │
             └──────────────┼──────────────┘
                            │
                            ▼
                 COMMUNICATION TASK
                            │
                            ▼
              COMMUNICATION SCHEDULER
                            │
             ┌──────────────┼──────────────┐
             ▼              ▼              ▼
          BRIDGE           MQTT           WEBAPI


# ============================================================
# CORE ARCHITECTURAL PRINCIPLE
# ============================================================

                    ┌──────────────────────┐
                    │     DETERMINISTIC    │
                    │       RUNTIME        │
                    └──────────┬───────────┘
                               │
          ┌────────────────────┼────────────────────┐
          ▼                    ▼                    ▼
     APPLICATION           COMMUNICATION         HARDWARE
       LOGIC                  SERVICES            RESOURCES
          │                    │                    │
          └────────────────────┼────────────────────┘
                               ▼
                         NON-BLOCKING
                           EXECUTION

The fundamental principle is:

> No secondary service must be able to block the main
> application cycle.

Application logic and communication are therefore separated,
scheduled, monitored and executed according to deterministic
timing rules.

Hardware resources such as Ethernet sockets are considered
part of the software architecture and must be explicitly
managed and diagnosed.


# ============================================================
# DOMOMANAGER
# THE OPERATING SYSTEM FOR THE HOME
# ============================================================