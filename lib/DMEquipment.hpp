#ifndef DMEquipment_HPP
#define DMEquipment_HPP

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑03‑24
   Note:
    • Introdotta EquipmentBase
    • Heater, Valve aggiornate
    • Aggiunti Pump e Shutter
    • Pensato per Opta: nessun virtual, nessuna alloc dinamica ricorrente

   ============================================================================ */

/*===============================================================================
DMEquipment — Actuator drivers for heating, valves, pumps, shutters, irrigation
===============================================================================
This module provides deterministic, zero‑alloc actuator primitives for Opta‑class
embedded systems:

- EquipmentBase: common state machine (Idle/Running/Error), timeout handling,
                 command timestamping and error reset.

- Heater: PID‑based thermal controller with soft‑start, adaptive ramp,
          sensor‑stuck detection, temperature limits and anti‑windup.

- Valve: motorized valve controller with open/close commands, end‑switch logic,
         timeout protection and full movement state tracking.

- Pump: pump driver with dry‑run protection, timeout handling and safe shutdown.

- Shutter: non‑blocking roller‑shutter controller with up/down motion,
           end‑switch detection and timeout‑based error handling.

- IrrigationSystem: multi‑zone irrigation sequencer with zone callbacks,
                    optional pump control, optional garden‑valve integration,
                    inhibit input, and fully non‑blocking timing.

All components are:
- Zero‑allocation (no recurring dynamic memory)
- Real‑time friendly (non‑blocking update loops)
- Inline and deterministic
- Designed for safe physical actuation on constrained embedded hardware
=============================================================================== */

#include <Arduino.h>

#include "DMSignal.hpp"
#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

/* ============================================================================
EquipmentBase — Base comune per tutti gli attuatori
============================================================================ */
class EquipmentBase {
public:
    enum State {
    Idle,
    Running,
    Error
    };

    EquipmentBase(unsigned long timeoutMs = 0)
    : _state(Idle),
    _timeout(timeoutMs),
    _lastCommand(0)
    {}

    inline State state() const {
    return _state;
    }

    inline void setTimeout(unsigned long timeoutMs) {
    _timeout = timeoutMs;
    }

    inline void resetError() {
    _state = Idle;
    }

protected:
    State _state;
    unsigned long _timeout;
    unsigned long _lastCommand;

    inline bool timedOut(unsigned long now) const {
    if (_timeout == 0) return false;
        return (now - _lastCommand) >= _timeout;
    }

    inline void markCommand(unsigned long now) {
        _lastCommand = now;
    }
};

/* ============================================================================
Heater — Controllo termico con PID e protezioni
============================================================================ */

class Heater : public EquipmentBase {
public:
    Heater(float kp, float ki, float kd, float maxPower = 255)
        : EquipmentBase(0),
        _kp(kp), _ki(ki), _kd(kd),
        _maxPower(maxPower),
        _setpoint(0),
        _maxTemp(120),
        _minTemp(-20),
        _lastTime(0),
        _lastError(0),
        _integral(0),
        _lastTemp(0),
        _lastTempChangeTime(0),
        _stuckThreshold(0.2f),
        _stuckTimeout(10),
        _softStartDuration(10),
        _adaptiveSoftStartEnabled(true),
        _adaptiveRange(20),
        _ambientTemp(20),
        _heatStartTime(0)
    {}

    // Safety configuration
    inline void setMaxTemp(float t) { _maxTemp = t; }
    inline void setMinTemp(float t) { _minTemp = t; }
    inline void setStuckDetection(float threshold, int timeoutSec) {
        _stuckThreshold = threshold;
        _stuckTimeout = timeoutSec;
    }

    // Soft-start configuration
    inline void setSoftStartDuration(int seconds) {
        _softStartDuration = seconds;
    }

    inline void enableAdaptiveSoftStart(bool enable) {
        _adaptiveSoftStartEnabled = enable;
    }

    inline void setAmbientTemperature(float t) {
        _ambientTemp = t;
    }

    inline void setAdaptiveRange(float r) {
        _adaptiveRange = r;
    }

    // Commands
    inline void setTarget(float temperature, unsigned long now) {
        _setpoint = temperature;
        if (_state != Error) {
            _state = Running;
            _heatStartTime = now;
            _lastTime = 0;
            _integral = 0;
            _lastError = 0;
        }
    }

    inline void stop() {
        _state = Idle;
    }

    inline void resetError() {
        EquipmentBase::resetError();
        _integral = 0;
        _lastError = 0;
        _lastTime = 0;
    }

    // Update loop: restituisce la potenza (0.._maxPower)
    int update(float currentTemp, unsigned long now) {
        if (_state == Idle) return 0;
        if (_state == Error) return 0;

        // SAFETY
        if (!isValidTemp(currentTemp) ||
        currentTemp > _maxTemp ||
        currentTemp < _minTemp ||
        isSensorStuck(currentTemp, now))
        {
            _state = Error;
            return 0;
        }

        float dt = (_lastTime == 0) ? 0.0f : (now - _lastTime) / 1000.0f;
        if (_lastTime == 0) {
            _lastTime = now;
            _lastError = _setpoint - currentTemp;
            _lastTemp = currentTemp;
            _lastTempChangeTime = now;
            return 0;
        }

        if (dt <= 0.0f) return 0;

        float error = _setpoint - currentTemp;

        // INTEGRAL
        _integral += error * dt;
        _integral = constrain(_integral, -1000.0f, 1000.0f);

        // DERIVATIVE
        float derivative = (error - _lastError) / dt;

        float output = _kp * error + _ki * _integral + _kd * derivative;

        // SOFT START
        float maxAllowed = softStartLimit(now);
        output = constrain(output, 0.0f, maxAllowed);

        // LIMIT
        output = constrain(output, 0.0f, _maxPower);

        // ANTI-WINDUP
        if (output == 0.0f || output == _maxPower)
        _integral -= error * dt;

        _lastError = error;
        _lastTime = now;

        return (int)output;
    }

private:
    float _kp, _ki, _kd;
    float _maxPower;
    float _setpoint;
    float _maxTemp;
    float _minTemp;

    unsigned long _lastTime;
    float _lastError;
    float _integral;

    // Stuck sensor detection
    float _lastTemp;
    unsigned long _lastTempChangeTime;
    float _stuckThreshold;
    int _stuckTimeout;

    // Soft start
    int _softStartDuration;
    bool _adaptiveSoftStartEnabled;
    float _adaptiveRange;
    float _ambientTemp;
    unsigned long _heatStartTime;

    inline bool isValidTemp(float t) {
        if (isnan(t)) return false;
        if (t < -100.0f || t > 300.0f) return false;
        return true;
    }

    inline bool isSensorStuck(float t, unsigned long now) {
        float delta = fabs(t - _lastTemp);
        if (delta > _stuckThreshold * 0.5f) {
            _lastTemp = t;
            _lastTempChangeTime = now;
            return false;
        }

        if ((now - _lastTempChangeTime) / 1000 > (unsigned long)_stuckTimeout)
        return true;

        return false;
    }

    inline float computeAdaptiveDuration() {
        if (!_adaptiveSoftStartEnabled)
            return (float)_softStartDuration;

        float diff = _setpoint - _ambientTemp;
        diff = constrain(diff, 0.0f, _adaptiveRange);

        float factor = 1.0f - (diff / _adaptiveRange);
        factor = constrain(factor, 0.2f, 1.0f);   // mai sotto 20%

        return _softStartDuration * factor;
    }

    inline float softStartLimit(unsigned long now) {
        float adaptiveDuration = computeAdaptiveDuration();
        float elapsed = (now - _heatStartTime) / 1000.0f;

        if (elapsed >= adaptiveDuration)
        return _maxPower;

        float ramp = (elapsed / adaptiveDuration) * _maxPower;
        return ramp;
    }
};

/* ============================================================================
Valve — Gestione valvole motorizzate con timeout
============================================================================ */

class Valve : public EquipmentBase {
public:
    enum MoveState {
    VIdle,
    Opening,
    Closing,
    Opened,
    Closed,
    StoppedByUser,
    VError
    };

    Valve(int address, unsigned long timeout = 40000)
        : EquipmentBase(timeout),
        _address(address),
        _moveState(VIdle),
        _startTime(0)
    {}

    inline void commandOpen(unsigned long now) {
        if (_moveState == Opening || _moveState == Opened) return;
        
        _moveState = Opening;
        _state = Running;
        _startTime = now;
        markCommand(now);
    }

    inline void commandClose(unsigned long now) {
        if (_moveState == Closing || _moveState == Closed) return;
        
        _moveState = Closing;
        _state = Running;
        _startTime = now;
        markCommand(now);
    }

    inline void commandStop() {
        _moveState = StoppedByUser;
        _state = Idle;
    }

    inline void resetError() {
        EquipmentBase::resetError();
        _moveState = VIdle;
    }

    // switchStatus: 0 = finecorsa chiuso, 1 = finecorsa aperto
    inline void update(int switchStatus, unsigned long now) {
    
        if (timedOut(now)) {
            _moveState = VError;
            _state = Error;
            return;
        }

        switch (_moveState) {
            case VIdle:
            if (switchStatus == 1) {
            _moveState = Opened;
            } else {
            _moveState = Closed;
            }
            _state = Idle;
            break;

            case Opening:
            if (switchStatus == 1) {
            _moveState = Opened;
            _state = Idle;
            }
            break;

            case Closing:
            if (switchStatus == 0) {
            _moveState = Closed;
            _state = Idle;
            }
            break;

            default:
            break;
        }
    }

    inline MoveState moveState() const { return _moveState; }

private:
    int _address;
    MoveState _moveState;
    unsigned long _startTime;
};

/* ============================================================================
Pump — Pompa con timeout e protezione livello
============================================================================ */

class Pump : public EquipmentBase {
public:
    Pump(unsigned long timeoutMs = 0)
        : EquipmentBase(timeoutMs),
        _isOn(false)
    {}

    inline void commandStart(unsigned long now) {
        if (_state == Error) return;
        
        _isOn = true;
        _state = Running;
        markCommand(now);
    }

    inline void stop() {
        _isOn = false;
        _state = Idle;
    }

    inline void resetError() {
        EquipmentBase::resetError();
        _isOn = false;
    }

    // levelOk: true se livello liquido è sufficiente (no dry-run)
    inline void update(bool levelOk, unsigned long now) {
        if (!_isOn) return;

        if (!levelOk) {
            _state = Error;
            _isOn = false;
            return;
        }

        if (timedOut(now)) {
            _state = Error;
            _isOn = false;
        }
    }

    inline bool isOn() const { return _isOn; }

private:
    bool _isOn;
};

/* ============================================================================
Shutter — Tapparella/serranda con finecorsa e timeout
============================================================================ */

class Shutter : public EquipmentBase {
public:
    enum MoveState {
        SIdle,
        MovingUp,
        MovingDown,
        Opened,
        Closed,
        SError
    };

    Shutter(unsigned long timeoutMs = 40000)
        : EquipmentBase(timeoutMs),
        _moveState(SIdle)
    {}

    inline void commandUp(unsigned long now) {
        _moveState = MovingUp;
        _state = Running;
        markCommand(now);
    }

    inline void commandDown(unsigned long now) {
        _moveState = MovingDown;
        _state = Running;
        markCommand(now);
    }

    inline void stop() {
        _moveState = SIdle;
        _state = Idle;
    }

    inline void resetError() {
        EquipmentBase::resetError();
        _moveState = SIdle;
    }

    // limitUp: true se finecorsa superiore attivo
    // limitDown: true se finecorsa inferiore attivo
    inline void update(bool limitUp, bool limitDown, unsigned long now) {

        if (timedOut(now)) {
            _moveState = SError;
            _state = Error;
            return;
        }

        if (_moveState == MovingUp && limitUp) {
            _moveState = Opened;
            _state = Idle;
            } else if (_moveState == MovingDown && limitDown) {
            _moveState = Closed;
            _state = Idle;
        }
    }

    inline MoveState moveState() const { return _moveState; }

private:
    MoveState _moveState;
};

/* ============================================================================
IrrigationSystem — Gestione impianto di irrigazione a zone sequenziali
===============================================================================
Funzionalità:
 • Numero di zone configurabili dinamicamente (caricate da cfg)
 • Ogni zona ha: nome, durata, stato attivo, timestamp di avvio
 • Sequenza automatica delle zone
 • Callback per comando zona (ON/OFF)
 • Callback opzionale per pompa (ON/OFF)
 • Callback opzionale per elettrovalvola giardino (usa Valve)
 • La pompa parte SOLO dopo apertura elettrovalvola
 • Inibizione generale tramite sensore esterno (generic sensor)
 • Nessuna alloc dinamica ricorrente, nessun virtual
============================================================================ */

class IrrigationSystem {
public:

    /* ------------------------------------------------------------------------
       Struttura zona
       ------------------------------------------------------------------------ */
    struct Zone {
        String name;               // Nome zona (da cfg)
        unsigned long durationSec; // Durata irrigazione
        unsigned long startTime;   // Timestamp avvio
        bool active;               // Stato attivo
    };

    /* ------------------------------------------------------------------------
       Callback esterne
       ------------------------------------------------------------------------ */
    typedef void (*ZoneCallback)(const Zone&, bool isOn);
    typedef void (*PumpCallback)(bool isOn);
    typedef void (*GardenValveCommand)(bool open, unsigned long now);

    /* ------------------------------------------------------------------------
       Stati macchina irrigazione
       ------------------------------------------------------------------------ */
    enum State {
        Idle,
        OpeningGardenValve,
        WaitingValveOpened,
        PumpRunning,
        RunningZones,
        Finished,
        Error
    };

    /* ------------------------------------------------------------------------
       Costruttore
       ------------------------------------------------------------------------ */
    IrrigationSystem()
        : _zones(nullptr),
          _zoneCount(0),
          _currentZone(0),
          _state(Idle),
          _inhibit(false),
          _pumpCb(nullptr),
          _zoneCb(nullptr),
          _gardenValveCb(nullptr),
          _gardenValve(nullptr)
    {}

    /* ------------------------------------------------------------------------
       Configurazione iniziale (da cfg)
       ------------------------------------------------------------------------ */
    inline void configure(Zone* zones, int count) {
        _zones = zones;
        _zoneCount = count;
    }

    /* ------------------------------------------------------------------------
       Inibizione generale (da generic sensor)
       ------------------------------------------------------------------------ */
    inline void setInhibit(bool inhibit) {
        _inhibit = inhibit;
        if (inhibit) stopAll();
    }

    /* ------------------------------------------------------------------------
       Set callback
       ------------------------------------------------------------------------ */
    inline void setZoneCallback(ZoneCallback cb) { _zoneCb = cb; }
    inline void setPumpCallback(PumpCallback cb) { _pumpCb = cb; }
    inline void setGardenValveCallback(GardenValveCommand cb) { _gardenValveCb = cb; }

    /* ------------------------------------------------------------------------
       Collegamento elettrovalvola giardino (classe Valve)
       ------------------------------------------------------------------------ */
    inline void attachGardenValve(Valve* v) { _gardenValve = v; }

    /* ------------------------------------------------------------------------
       Avvio sequenza irrigazione
       ------------------------------------------------------------------------ */
    inline void start(unsigned long now) {
        if (_inhibit || _zoneCount == 0) return;

        _currentZone = 0;
        _state = OpeningGardenValve;

        // Apri elettrovalvola se configurata
        if (_gardenValveCb)
            _gardenValveCb(true, now);
    }

    /* ------------------------------------------------------------------------
       Stop immediato di tutto
       ------------------------------------------------------------------------ */
    inline void stopAll() {
        if (_pumpCb) _pumpCb(false);

        if (_zoneCb) {
            for (int i = 0; i < _zoneCount; i++)
                _zoneCb(_zones[i], false);
        }

        _state = Idle;
    }

    /* ------------------------------------------------------------------------
       Loop principale
       ------------------------------------------------------------------------ */
    inline void update(unsigned long now) {
        if (_state == Idle || _state == Error) return;
        if (_inhibit) { stopAll(); return; }

        switch (_state) {

        /* ------------------------------------------------------------
           1) Apertura elettrovalvola giardino
           ------------------------------------------------------------ */
        case OpeningGardenValve:
            if (_gardenValve == nullptr) {
                // Nessuna elettrovalvola → passa direttamente alla pompa
                if (_pumpCb) _pumpCb(true);
                _state = PumpRunning;
            } else {
                _state = WaitingValveOpened;
            }
            break;

        /* ------------------------------------------------------------
           2) Attesa apertura elettrovalvola
           ------------------------------------------------------------ */
        case WaitingValveOpened:
            if (_gardenValve->moveState() == Valve::Opened) {
                if (_pumpCb) _pumpCb(true);
                _state = PumpRunning;
            }
            break;

        /* ------------------------------------------------------------
           3) Avvio pompa → avvio prima zona
           ------------------------------------------------------------ */
        case PumpRunning:
            _zones[_currentZone].startTime = now;
            _zones[_currentZone].active = true;

            if (_zoneCb)
                _zoneCb(_zones[_currentZone], true);

            _state = RunningZones;
            break;

        /* ------------------------------------------------------------
           4) Sequenza zone
           ------------------------------------------------------------ */
        case RunningZones: {
            Zone& z = _zones[_currentZone];

            if ((now - z.startTime) / 1000 >= z.durationSec) {
                // Fine zona
                if (_zoneCb) _zoneCb(z, false);
                z.active = false;

                _currentZone++;

                if (_currentZone >= _zoneCount) {
                    // Fine irrigazione
                    if (_pumpCb) _pumpCb(false);
                    _state = Finished;
                } else {
                    // Avvio zona successiva
                    _zones[_currentZone].startTime = now;
                    _zones[_currentZone].active = true;

                    if (_zoneCb)
                        _zoneCb(_zones[_currentZone], true);
                }
            }
        } break;

        /* ------------------------------------------------------------
           5) Fine sequenza
           ------------------------------------------------------------ */
        case Finished:
            if (_gardenValveCb)
                _gardenValveCb(false, now); // chiudi elettrovalvola

            _state = Idle;
            break;

        default:
            break;
        }
    }

private:
    /* ------------------------------------------------------------------------
       Variabili interne
       ------------------------------------------------------------------------ */
    Zone* _zones;
    int _zoneCount;
    int _currentZone;

    bool _inhibit;

    State _state;

    PumpCallback _pumpCb;
    ZoneCallback _zoneCb;
    GardenValveCommand _gardenValveCb;

    Valve* _gardenValve;
};


#endif
