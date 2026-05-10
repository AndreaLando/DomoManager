# DomoManager
A modular, non‑blocking, and extensible automation framework that transforms Arduino Opta into a professional home automation controller.

DomoManager integrates seamlessly with:
- RS485 Bridge — multi‑device Modbus RTU communication with retry, ACK/NAK, routing, and hot‑standby
- UDP Bridge — lightweight communication with IoT modules and mobile apps
- Arduino Cloud Integration — remote dashboards and mobile control
- Modbus RTU — sensors, relays, power meters, T/H probes
- Modbus TCP Server — Weintek HMI panels
- Modbus TCP Client — Ethernet‑to‑RS485 gateways
- HTTP Web APIs — phones, intercoms, automation servers
- MQTT — Home Assistant, Node‑RED, Z2M, Shelly, etc.
- AEE Registry — dynamic variable/state mapping
- Wired Sensors Engine — zones, debounce, alarms, aggregation
- Automation Engine — rule‑based automations
- Async Jobs Engine — non‑blocking workflows
- RTC via SMTP — time synchronization without NTP

The system is designed for 24/7 operation, zero blocking, and high scalability.

Follow us on
  Website: https://www.domo-manager.it/
  Facebook: https://www.facebook.com/people/DOMO-Manager/61573260488406
  
Features
=========

RS485 Bridge
High-reliability Modbus RTU communication:
- Automatic retry and timeout handling
- ACK/NAK protocol
- Multi-device routing
- Hot-standby master/slave failover
- Packet integrity checks
- Non-blocking state machine

UDP Bridge
Ultra-light IoT communication:
- Remote commands
- State updates
- Event notifications
- Low-latency, low-overhead protocol
- Ideal for mobile apps and cloud relays

Arduino Cloud Integration
Native integration with Arduino IoT Cloud:
- Cloud-to-device commands
- Device-to-cloud state updates
- Mobile dashboard support
- Automatic variable synchronization
- Works alongside MQTT and WebAPI

Wired Sensors Engine
Advanced wired sensor management:
- Zone-based architecture
- Debounce and filtering
- Edge detection
- Alarm generation
- Aggregation (OR/AND logic)
- Non-blocking polling

Automation Engine
Rule-based automation system:
- IF/THEN logic
- Multi-condition rules
- Time-based triggers
- Sensor-based triggers
- Action chaining
- Fully non-blocking

Async Jobs Engine
Non-blocking workflow engine:
- Sequential steps
- Conditional branches
- Skip logic
- Completion callbacks
- Priority-based execution
- Ideal for complex automations

RTC via SMTP
Time synchronization without NTP:
- Extracts timestamp from SMTP server headers
- Works behind firewalls where NTP is blocked
- Updates Opta RTC
- Fully non-blocking

ModbusBuffer
Advanced buffering layer:
- Field / ToPanel / FromPanel separation
- Per-area change tracking
- Debounce and reverse logic
- Silent writes
- Initialization tracking
- Built-in diagnostics

DomoManager Core
Central orchestrator:
- Dynamic, priority-based Modbus polling
- Non-blocking device management
- Event routing and change detection
- Execution-time measurement
- Spike detection and watchdog
- Automatic device error handling

HVAC Engine
Full multi-zone HVAC controller:
- Heating / cooling / auto / defrost
- Zone temperature logic
- Fancoil control
- Safety logic (open windows, compressor protection, external limits)
- Comfort / Eco modes

PowerManager
Smart energy management:
- Solar production forecasting
- Load prioritization
- Dynamic hysteresis
- Auto-tuning
- Optimization modes (comfort, savings, self-consumption, grid protection)

WeatherStation
Environmental monitoring:
- Moving-average filtering
- Edge-triggered alarms
- Rain start/stop detection
- Wind gust detection
- Day/night detection with hysteresis

Logger
High-performance logging system:
- Multiple log levels
- Compile-time filtering
- Timestamped entries
- Serial and remote output
- Lightweight and Opta-optimized

New Modules (2026 Update)

WebAPIEngine
Asynchronous HTTP engine:
- Non-blocking GET/POST
- FIFO request queue
- Configurable timeouts
- Integrated with DeviceMessageEngine
- Automatic response handling

DeviceMessageEngine
Message processing engine:
- Dynamic AEE variable mapping
- Placeholder resolver
- WebAPI profiles (Fanvil, X1/X2, etc.)
- Error handling and retry logic

SimpleHttpTransport
Minimal, Opta-compatible HTTP transport:
- Fully non-blocking
- Incremental parsing
- Response callback support

Architecture Overview

DomoManager (Core Orchestrator)
Modules:
- Modbus RTU/TCP
- WebAPI Engine
- MQTT Engine
- UDP Bridge
- RS485 Bridge
- Automations and Jobs

Data flows to:
- Devices
- HTTP APIs
- Home Assistant
- IoT Cloud
- I/O Bus
- Logic Engine

Configuration System
Everything is fully parameterized:
- Modbus devices
- WebAPI groups
- HVAC thresholds
- Wired sensors
- Zones
- Energy loads
- AEE variable mapping
- Automations
- Jobs

Configuration can be loaded via:
- USB
- MQTT
- Web API
- JSON files

Build and Deployment
Compatible with:
- Arduino Opta
- Arduino IDE
- All Modbus RTU Devices

Diagnostics
Real-time diagnostics include:
- Execution timing
- Modbus errors
- HVAC state
- PowerManager state
- WebAPIEngine state
- RS485/UDP bridge status
- Arduino Cloud sync status
- Wired sensor states
- Automation engine status
- Watchdog and spike detection

License
=========
DomoManager is distributed under a Dual License:
- Free for personal, educational, and research use
- Commercial use requires a paid license or written authorization
See the LICENSE file for details.
