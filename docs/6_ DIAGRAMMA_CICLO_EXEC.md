# ============================================================
# DOMOMANAGER – DIAGRAMMA ASCII DEL CICLO DI ESECUZIONE
# ============================================================

                         ┌──────────────────────┐
                         │        LOOP()        │
                         │  domomanager.ino     │
                         └──────────┬───────────┘
                                    │
                                    ▼

# ============================================================
# 1. BACKEND CYCLE
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                    BACKEND RUNTIME                          │
│                                                            │
│  Gestisce acquisizione, aggiornamento dello stato e        │
│  comunicazione con il mondo fisico.                        │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│ 1.1 TIME MANAGER                                           │
│                                                            │
│  - Aggiornamento temporale                                 │
│  - Epoch                                                  │
│  - RTC validation                                         │
│  - Callback temporali                                     │
│  - Second / Minute / Hour / Day                           │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│ 1.2 DEVICE MANAGER                                         │
│                                                            │
│  - Selezione dispositivo                                  │
│  - Device profile                                         │
│  - Mapping area → register                                │
│  - Priorità                                               │
│  - Error handling                                         │
│  - Cooldown                                               │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│ 1.3 MODBUS / DEVICE COMMUNICATION                          │
│                                                            │
│  - Polling round-robin                                    │
│  - Read / Write                                           │
│  - Timeout                                                │
│  - Retry                                                  │
│  - Cooldown                                               │
│  - Deterministic state machine                            │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

┌────────────────────────────────────────────────────────────┐
│ 1.4 BUFFER UPDATE                                          │
│                                                            │
│  - Applica valori acquisiti                               │
│  - Aggiorna timestamp                                     │
│  - Change tracking                                        │
│  - Virtual areas                                          │
│  - Reverse                                                │
│  - Split                                                  │
│  - Toggle                                                 │
│  - Internal events                                        │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

                 BACKEND CYCLE COMPLETED
                           │
                           ▼

# ============================================================
# 2. FRONTEND CYCLE
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                    TASK ENGINE                              │
│                                                            │
│              CENTRAL TASK ORCHESTRATION                    │
│                                                            │
│  - Task registration                                      │
│  - Execution interval                                     │
│  - Enabled / Disabled                                     │
│  - Deterministic scheduling                               │
│  - Frontend cycle management                              │
└──────────────────────────┬─────────────────────────────────┘
                           │
             ┌─────────────┼─────────────┐
             │             │             │
             ▼             ▼             ▼

┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
│ SECURITY TASK    │ │    HVAC TASK     │ │  AVERAGES TASK   │
│                  │ │                  │ │                  │
│ - Sensors        │ │ - Zones          │ │ - Sensor groups  │
│ - Security       │ │ - Setpoints      │ │ - Factors        │
│ - Bitmask        │ │ - Fan coils      │ │ - Averages       │
│ - Events         │ │ - DHW            │ │ - Buffer updates │
│                  │ │ - Defrost        │ │                  │
└──────────────────┘ └──────────────────┘ └──────────────────┘


┌────────────────────────────────────────────────────────────┐
│                      POWER ENGINE                           │
│                                                            │
│  - Carichi                                                 │
│  - Priorità                                                │
│  - Limiti                                                  │
│  - Forecast solare                                         │
│  - Protezioni                                              │
│  - Auto-tuning                                             │
│  - Buffer updates                                          │
└────────────────────────────────────────────────────────────┘


┌────────────────────────────────────────────────────────────┐
│                     WEATHER ENGINE                          │
│                                                            │
│  - Pioggia                                                 │
│  - Vento                                                   │
│  - Luce                                                    │
│  - Temperatura                                             │
│  - Moving averages                                         │
│  - Eventi                                                  │
│  - Buffer updates                                          │
└────────────────────────────────────────────────────────────┘


                           │
                           ▼

                  FRONTEND CYCLE COMPLETED
                           │
                           ▼

# ============================================================
# 3. AUTOMATION PROCESSING
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                    AUTOMATION ENGINE                        │
│                                                            │
│  Reads current system state from Buffer and evaluates      │
│  automation logic.                                         │
│                                                            │
│  - Conditions                                              │
│  - Scenes                                                  │
│  - Rules                                                   │
│  - Sequences                                               │
│  - Scheduled rules                                         │
│  - Composite conditions                                    │
│  - Trends                                                  │
│  - Debounce                                                │
│  - Dynamic automations                                     │
│                                                            │
│  → Produce state changes                                   │
│  → Write results to Buffer                                 │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼

                      BUFFER UPDATE
                           │
                           │
                           └───────────────┐
                                           │
                                           ▼
                                  NEXT EXECUTION CYCLE


# ============================================================
# 4. COMMUNICATION CYCLE
# ============================================================

                    ┌──────────────────────┐
                    │ COMMUNICATION TASK   │
                    │                      │
                    │ Task Engine          │
                    │ Master only          │
                    │ Power-on gating      │
                    └──────────┬───────────┘
                               │
                               ▼

┌────────────────────────────────────────────────────────────┐
│                 COMMUNICATION SCHEDULER                    │
│                                                            │
│                 TIMED ROUND-ROBIN                          │
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

        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │  BRIDGE  │ │   MQTT   │ │  WEBAPI  │
        │          │ │          │ │          │
        │ AEE      │ │ Events   │ │ GET      │
        │ UDP/JSON │ │ States   │ │ POST     │
        └──────────┘ └──────────┘ └──────────┘

                           │
                           ▼

                  COMMUNICATION COMPLETED


# ============================================================
# 5. FULL CYCLE SYNCHRONIZATION
# ============================================================

                    BACKEND CYCLE
                          │
                          ▼
                  ┌───────────────┐
                  │ Backend done? │
                  └───────┬───────┘
                          │
                          ▼
                    FRONTEND CYCLE
                          │
                          ▼
                 ┌────────────────┐
                 │ Frontend done? │
                 └───────┬────────┘
                         │
                         ▼
                 ┌──────────────────┐
                 │   FULL CYCLE     │
                 │    COMPLETED     │
                 └────────┬─────────┘
                          │
                          ▼
                 Full Cycle Callback
                          │
                          ▼
                    NEXT LOOP()


# ============================================================
# 6. BUFFER-DRIVEN FEEDBACK LOOP
# ============================================================

                           BUFFER
                             │
            ┌────────────────┼────────────────┐
            │                │                │
            ▼                ▼                ▼
         TASKS          AUTOMATION      COMMUNICATION
            │                │                │
            │                │                │
            ▼                ▼                ▼
        DECISIONS        DECISIONS        EXTERNAL
            │                │             SYSTEMS
            │                │                │
            └────────────────┼────────────────┘
                             │
                             ▼
                       BUFFER UPDATE
                             │
                             ▼
                    DEVICE / OUTPUT LAYER
                             │
                             ▼
                       PHYSICAL WORLD
                             │
                             ▼
                       NEW INPUT DATA
                             │
                             └───────────────►
                                  BUFFER


# ============================================================
# 7. DIAGNOSTICS – PARALLEL OBSERVABILITY
# ============================================================

┌────────────────────────────────────────────────────────────┐
│                    DIAGNOSTIC ENGINE                       │
│                                                            │
│  Observes the runtime without becoming part of the        │
│  critical execution path.                                 │
│                                                            │
│  - Buffer                                                 │
│  - Backend cycle                                          │
│  - Frontend cycle                                         │
│  - Tasks                                                  │
│  - Communication Scheduler                                │
│  - Bridge / MQTT / WebAPI                                 │
│  - Devices                                                │
│  - Modbus                                                 │
│  - Ethernet / sockets                                     │
│  - RTC                                                    │
│  - Security                                               │
│  - HVAC                                                   │
│  - Power                                                  │
│  - Hot Standby                                            │
│  - Watchdog / overload                                    │
└────────────────────────────────────────────────────────────┘


# ============================================================
# 8. COMPLETE EXECUTION MODEL
# ============================================================

                         ┌───────────────┐
                         │    LOOP()     │
                         └───────┬───────┘
                                 │
                                 ▼
                         ┌───────────────┐
                         │    BACKEND    │
                         └───────┬───────┘
                                 │
                                 ▼
                         ┌───────────────┐
                         │    BUFFER     │
                         └───────┬───────┘
                                 │
                                 ▼
                         ┌───────────────┐
                         │  TASK ENGINE  │
                         └───────┬───────┘
                                 │
                  ┌──────────────┼──────────────┐
                  │              │              │
                  ▼              ▼              ▼
                HVAC          SECURITY       AVERAGES
                  │              │              │
                  └──────────────┼──────────────┘
                                 │
                                 ▼
                         ┌───────────────┐
                         │   AUTOMATION  │
                         │     ENGINE    │
                         └───────┬───────┘
                                 │
                                 ▼
                              BUFFER
                                 │
                                 ▼
                     ┌─────────────────────┐
                     │ COMMUNICATION TASK  │
                     └──────────┬──────────┘
                                │
                                ▼
                    ┌─────────────────────┐
                    │ COMMUNICATION       │
                    │ SCHEDULER           │
                    └──────────┬──────────┘
                               │
                    ┌──────────┼──────────┐
                    ▼          ▼          ▼
                 BRIDGE       MQTT       WEBAPI
                               │
                               ▼
                          NEXT LOOP()


# ============================================================
# EXECUTION PRINCIPLES
# ============================================================

1. BACKEND acquires and updates the physical state.

2. BUFFER stores the authoritative system state.

3. TASK ENGINE executes application logic according to
   configured intervals and enabled states.

4. AUTOMATION ENGINE evaluates behavior using the Buffer.

5. COMMUNICATION TASK executes communication services through
   the Communication Scheduler.

6. COMMUNICATION SCHEDULER uses timed round-robin execution
   to prevent one service from monopolizing the runtime.

7. APPLICATION LOGIC and COMMUNICATION SERVICES remain
   logically separated.

8. Communication services must be NON-BLOCKING.

9. A secondary communication service must never be allowed
   to block the main application cycle.

10. BACKEND and FRONTEND cycle completion are synchronized
    before the full-cycle callback is generated.

11. Hardware resources such as Ethernet sockets are treated
    as runtime resources and must be observable.

12. DIAGNOSTICS observe the execution model and expose
    anomalies, overloads and resource problems.


# ============================================================
# DOMOMANAGER
# THE OPERATING SYSTEM FOR THE HOME
# ============================================================