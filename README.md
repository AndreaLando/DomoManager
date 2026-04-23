# DomoManager
Complete framework to get Arduino as Home controller for devices using:
- modbus RTU: to communicate with a wide range of I/O (Digital inputs, relays outputs, Power meters, Temperature and Humidity sensors); There is a built in library of compatible devices and can be added as your needs.
- modbus TCP Server: to communicate with Weintek touch-panels
- modbus TCP Client: to communicate with Ethernet to RS485 gateways
- ethernet UDP: to communicate to IOT module to control home by phone using Arduino Cloud

Modular, Non‑Blocking, Extensible Home Automation System for Arduino + Modbus
This repository contains a complete automation framework designed for advanced home control systems based on:

Arduino / STM32 / ESP32
Modbus TCP for Control Panels
Modbus RTU for I/O Devices
Environmental and security sensors
Multi‑zone HVAC control
Intelligent power management
Asynchronous scheduler
UDP‑based IoT integration
Runtime diagnostics and watchdog

The architecture is modular, scalable, and optimized for non‑blocking execution.

Features
=========

ModbusBuffer
- A robust buffering system providing:
- Field / ToPanel / FromPanel data separation
- Change‑tracking per area
- Reverse logic support
- Silent writes
- Compare and debounce logic
- Initialization tracking (never initialized / multiple initialization detection)

DomoManager, The core orchestrator:
- Dynamic priority‑based Modbus polling
- Non‑blocking device management
- Event routing and change detection
- Execution timing measurement
- Spike detection and watchdog
- Automatic device error handling

HVAC Engine
- Full heat‑pump and zone management:
- Heating / cooling / auto / defrost modes
- Multi‑zone temperature logic
- Fancoil control
- Safety logic (open windows, external temperature limits, compressor protection)

PowerManager
- Advanced load management:
- Solar production forecasting
- Load priorities
- Dynamic hysteresis
- Auto‑tuning of parameters
- Automatic suggestions (attach/detach loads)
- Optimization modes (comfort, savings, self‑consumption, grid protection)

WeatherStation
- Environmental monitoring with:
- Moving average filtering
- Edge‑triggered alarms
- Rain start/stop detection
- Wind gust detection
- Day/night detection with hysteresis
- Configurable debounce

IoT Integration
- UDP‑based communication:
- Remote commands
- State updates
- Event notifications

AsyncScheduler
- Non‑blocking job scheduler:
- Sequential steps
- Conditional branches
- Skip logic
- Completion callbacks
- Priority‑based execution
