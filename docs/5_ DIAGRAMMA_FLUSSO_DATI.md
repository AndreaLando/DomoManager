# ============================================================
# DOMOMANAGER – DIAGRAMMA ASCII DEL FLUSSO DATI
# ============================================================

                         MONDO FISICO
                              │
                              ▼

┌────────────────────────────────────────────────────────────┐
│                     SENSORI / ATTUATORI                    │
│                                                            │
│  - Digital / Analog I/O                                    │
│  - Modbus TCP / RTU                                        │
│  - RS485                                                   │
│  - Dispositivi esterni                                     │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

# ============================================================
# BACKEND – ACQUISIZIONE E NORMALIZZAZIONE
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                      DEVICE MANAGER                        │
│                                                            │
│  - Device profiles                                         │
│  - Area → register mapping                                 │
│  - Priorities                                              │
│  - Error handling                                          │
│  - Cooldown                                                │
│  - Device state                                            │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                       MODBUS ENGINE                        │
│                                                            │
│  - Round-robin polling                                     │
│  - Deterministic reads                                     │
│  - Buffer → device writes                                  │
│  - Timeout                                                 │
│  - Retry                                                   │
│  - Cooldown                                                │
│  - Error management                                        │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

# ============================================================
# BUFFER – SINGLE SOURCE OF TRUTH
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                       BUFFER ENGINE                        │
│                                                            │
│  - Typed Areas                                             │
│  - Timestamp                                               │
│  - Change Tracking                                         │
│  - Virtual Areas                                           │
│  - Reverse                                                 │
│  - Split                                                   │
│  - Toggle                                                  │
│  - Internal events                                         │
│                                                            │
│              SINGLE SOURCE OF TRUTH                        │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           │
             ┌─────────────┴─────────────┐
             │                           │
             ▼                           ▼

# ============================================================
# FRONTEND – ELABORAZIONE E DECISIONE
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                      TASK ENGINE                           │
│                                                            │
│  Centralized application task orchestration                │
│                                                            │
│  - Task registration                                       │
│  - Execution interval                                      │
│  - Enabled / Disabled                                      │
│  - Deterministic scheduling                                │
│  - Frontend cycle management                               │
└──────────────────────────┬─────────────────────────────────┘
                           │
          ┌────────────────┼────────────────┐
          │                │                │
          ▼                ▼                ▼

┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
│   SECURITY TASK  │ │    HVAC TASK     │ │  AVERAGES TASK   │
│                  │ │                  │ │                  │
│ - Sensors        │ │ - Zones          │ │ - Groups         │
│ - Security state │ │ - Setpoints      │ │ - Factors        │
│ - Bitmask        │ │ - Fan coils      │ │ - Averages       │
│ - Events         │ │ - DHW            │ │ - Buffer update  │
│                  │ │ - Defrost        │ │                  │
└──────────────────┘ └──────────────────┘ └──────────────────┘


┌────────────────────────────────────────────────────────────┐
│                       POWER ENGINE                          │
│                                                            │
│  - Load management                                         │
│  - Priorities                                              │
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


                           │
                           │
                           ▼

# ============================================================
# AUTOMATION – COMPORTAMENTO DEL SISTEMA
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                     AUTOMATION ENGINE                      │
│                                                            │
│  Reads system state from the Buffer and produces           │
│  deterministic decisions and state changes.                │
│                                                            │
│  - Scenes                                                  │
│  - Rules                                                   │
│  - Conditions                                              │
│  - Sequences                                               │
│  - Scheduled rules                                         │
│  - Composite conditions                                    │
│  - Trends                                                  │
│  - Debounce                                                │
│  - Dynamic automations                                     │
│  - JSON Builder                                            │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           │
                           ▼
                    ┌───────────────┐
                    │    BUFFER     │
                    │               │
                    │ State changes │
                    └───────┬───────┘
                            │
                            ▼

# ============================================================
# OUTPUT – BACKEND / DISPOSITIVI
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                  DEVICE / PROTOCOL LAYER                   │
│                                                            │
│  Buffer changes are translated into physical actions       │
│  through the appropriate device and protocol engine.       │
└──────────────────────────┬─────────────────────────────────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              ▼            ▼            ▼
           MODBUS        RS485        LOCAL I/O
              │            │            │
              └────────────┼────────────┘
                           ▼

┌────────────────────────────────────────────────────────────┐
│                    ATTUATORI & DISPOSITIVI                 │
│                                                            │
│  - Valvole                                                │
│  - Pompe                                                  │
│  - Fan Coil                                               │
│  - Relè                                                   │
│  - Carichi                                                │
│  - Luci                                                   │
│  - Dispositivi Modbus                                     │
│  - Dispositivi RS485                                      │
└────────────────────────────────────────────────────────────┘


# ============================================================
# COMMUNICATION – ESPOSIZIONE DELLO STATO
# ============================================================

                           BUFFER
                              │
                              ▼

┌────────────────────────────────────────────────────────────┐
│                  COMMUNICATION TASK                        │
│                                                            │
│  Executed by the Task Engine                               │
│                                                            │
│  - Master only                                             │
│  - Power-on cycle gating                                   │
│  - Non-blocking execution                                  │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│                COMMUNICATION SCHEDULER                     │
│                                                            │
│              TIMED ROUND-ROBIN                             │
│                                                            │
│  - Interval                                               │
│  - Priority                                               │
│  - Enabled / Disabled                                     │
│  - Service isolation                                      │
│  - Non-blocking execution                                 │
└──────────────────────────┬─────────────────────────────────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              ▼            ▼            ▼

┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
│      BRIDGE      │ │       MQTT       │ │      WEBAPI      │
│                  │ │                  │ │                  │
│ - AEE            │ │ - Events        │ │ - GET            │
│ - UDP / JSON     │ │ - State         │ │ - POST           │
│ - Events         │ │ - Home Assistant│ │ - Buffer mapping │
│                  │ │ - Zigbee2MQTT   │ │ - Responses      │
│                  │ │ - Shelly        │ │                  │
└──────────────────┘ └──────────────────┘ └──────────────────┘


# ============================================================
# BIDIRECTIONAL DATA FLOW
# ============================================================

                         ┌───────────────┐
                         │     BUFFER    │
                         └───────┬───────┘
                                 │
              ┌──────────────────┼──────────────────┐
              │                  │                  │
              ▼                  ▼                  ▼
          APPLICATION       AUTOMATION        COMMUNICATION
              │                  │                  │
              │                  │                  │
              ▼                  ▼                  ▼
           DECISION          DECISION          EXTERNAL
              │                  │              INTEGRATION
              │                  │                  │
              └──────────┬───────┴──────────────────┘
                         │
                         ▼
                    BUFFER UPDATE
                         │
                         ▼
                  PHYSICAL OUTPUT


# ============================================================
# COMPLETE SYSTEM DATA FLOW
# ============================================================

                     ┌────────────────────┐
                     │    MONDO FISICO    │
                     └─────────┬──────────┘
                               │
                               ▼
                     ┌────────────────────┐
                     │  DEVICE MANAGER    │
                     └─────────┬──────────┘
                               │
                               ▼
                     ┌────────────────────┐
                     │   MODBUS / I/O     │
                     └─────────┬──────────┘
                               │
                               ▼
                ╔══════════════════════════════╗
                ║            BUFFER            ║
                ║     SINGLE SOURCE OF TRUTH   ║
                ╚══════════════╤═══════════════╝
                               │
                 ┌─────────────┼─────────────┐
                 │             │             │
                 ▼             ▼             ▼
              TASKS        AUTOMATION   COMMUNICATION
                 │             │             │
                 │             │             ▼
                 │             │      COMMUNICATION
                 │             │        SCHEDULER
                 │             │             │
                 │             │      ┌──────┼──────┐
                 │             │      ▼      ▼      ▼
                 │             │   BRIDGE   MQTT   WEBAPI
                 │             │
                 └──────┬──────┘
                        │
                        ▼
                 BUFFER CHANGES
                        │
                        ▼
                  DEVICE LAYER
                        │
                        ▼
                  MONDO FISICO


# ============================================================
# ARCHITECTURAL PRINCIPLE
# ============================================================

The data flow is not a simple linear pipeline.

The Buffer is the central state repository and the
Single Source of Truth.

All major subsystems:

    BACKEND
    TASK ENGINE
    AUTOMATION ENGINE
    COMMUNICATION SERVICES

read from and/or write to the Buffer according to their role.

The Task Engine manages application execution.

The Communication Scheduler manages external communication
services independently from the application logic.

The complete runtime therefore follows the principle:

                    PHYSICAL WORLD
                           │
                           ▼
                        BACKEND
                           │
                           ▼
                         BUFFER
                           │
             ┌─────────────┼─────────────┐
             ▼             ▼             ▼
           TASKS       AUTOMATION   COMMUNICATION
             │             │             │
             └─────────────┼─────────────┘
                           ▼
                     BUFFER CHANGES
                           │
                           ▼
                      OUTPUT LAYER
                           │
                           ▼
                    PHYSICAL WORLD


# ============================================================
# CORE RULE
# ============================================================

> The Buffer is the truth.
>
> The Task Engine executes the application.
>
> The Automation Engine defines behavior.
>
> The Communication Scheduler exposes the system.
>
> No secondary communication service must be able to block
> the main application cycle.
>
> Hardware resources and communication resources are part
> of the runtime architecture and must be observable and
> diagnosable.

# ============================================================
# DOMOMANAGER
# THE OPERATING SYSTEM FOR THE HOME
# ============================================================