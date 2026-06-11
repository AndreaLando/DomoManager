#ifndef DMSignal_HPP
#define DMSignal_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑03‑24
   Note:
                    • Nessuna

   ============================================================================ */

   
#include <Arduino.h>

class ToggleSignal {
public:
    ToggleSignal() : _oldStatus(0) {}

    inline uint8_t getOldValue() const {
        return _oldStatus;
    }

    inline bool change(bool statusIn, long &value) {
        // Se non c’è variazione → niente toggle
        if (_oldStatus == statusIn)
            return false;

        _oldStatus = statusIn;

        // Toggle solo sul fronte di salita
        if (statusIn) {
            value ^= 1;   // toggle più veloce di (value==0?1:0)
            return true;
        }

        return false;
    }

private:
    uint8_t _oldStatus;   // 1 byte invece di 4
};


//---------------------------------------------------------
// Base class for all timers
//---------------------------------------------------------
class TimerBase {
public:
    enum TimeFMT {
        Milliseconds,
        Seconds,
        Minutes,
        Hours
    };

    TimerBase() :
        _preset(0),
        _scale(1.0f),
        _startMillis(0),
        _running(false),
        _Q(false)
    {}

    TimerBase(float preset, TimeFMT fmt) {
        SetPreset(preset, fmt);
    }

    inline void SetPreset(float preset, TimeFMT fmt) {
        _preset = preset;
        _scale  = scaleFromFormat(fmt);
    }

    inline void Start(uint32_t now) {
        _running = true;
        _startMillis = now;
        _Q = false;
    }

    inline void Start() {
        Start(millis());
    }

    inline void Stop() {
        _running = false;
        _Q = false;
    }

    inline float ET(uint32_t now) const {
        if (!_running) return 0.0f;
        return (now - _startMillis) / _scale;
    }

    inline float ET() const {
        return ET(millis());
    }

    inline bool Q() const {
        return _Q;
    }

protected:
    float _preset;
    float _scale;
    unsigned long _startMillis;
    bool _running;
    bool _Q;

    static constexpr float scaleFromFormat(TimeFMT fmt) {
        switch (fmt) {
            case Milliseconds: return 1.0f;
            case Seconds:      return 1000.0f;
            case Minutes:      return 60000.0f;
            case Hours:        return 3600000.0f;
        }
        return 1.0f;
    }
};



//---------------------------------------------------------
// TON — On‑Delay Timer
//---------------------------------------------------------
class TON : public TimerBase {
public:
    TON(float preset = 0, TimeFMT fmt = Milliseconds)
        : TimerBase(preset, fmt) {}

    inline void Run(bool in) {
        if (in) {
            if (!_running) Start();
            _Q = (ET() >= _preset);
        } else {
            Stop();
        }
    }

    inline void Run(bool in, uint32_t now) {
        if (in) {
            if (!_running) Start(now);
            _Q = (ET(now) >= _preset);
        } else {
            Stop();
        }
    }
};


//---------------------------------------------------------
// TOF — Off‑Delay Timer
//---------------------------------------------------------
class TOF : public TimerBase {
public:
    TOF(float preset = 0, TimeFMT fmt = Milliseconds)
        : TimerBase(preset, fmt), _timing(false), _initialized(false), _everTrue(false)
    {
        _Q = false;
    }

    inline void Run(bool in) {

        // Primo ciclo → Q=0
        if (!_initialized) {
            _initialized = true;
            _timing = false;
            _running = false;
            _Q = false;
            return;
        }

        // 🔥 Se IN non è mai stato true → Q deve essere sempre 0
        if (!_everTrue && !in) {
            _Q = false;
            return;
        }

        // Se IN diventa true almeno una volta
        if (in) {
            _everTrue = true;
            _timing = false;
            _Q = true;
        } else {
            if (!_timing) {
                _timing = true;
                Start();
            }
            _Q = (ET() < _preset);
        }
    }

private:
    bool _timing;
    bool _initialized;
    bool _everTrue;   // <--- NUOVO
};


//---------------------------------------------------------
// TP — Pulse Timer
//---------------------------------------------------------
class TP : public TimerBase {
public:
    TP(float preset = 0, TimeFMT fmt = Milliseconds)
        : TimerBase(preset, fmt), _active(false) {}

    inline void Run(bool in) {
        if (in && !_active) {
            _active = true;
            Start();
            _Q = true;
        }

        if (_active && ET() >= _preset) {
            _Q = false;
            _active = false;
        }
    }

private:
    bool _active;
};


class Debounce {
public:
    TON ton;
    bool stable = false;

    Debounce(unsigned long ms = 20)
        : ton(ms, TimerBase::Milliseconds) {}

    inline bool update(bool raw) {
        if (raw == stable) {
            ton.Run(false);   // nessun cambiamento → reset timer
        } else {
            ton.Run(true);    // cambiamento → avvia TON
            if (ton.Q()) {
                stable = raw; // nuovo stato stabile
            }
        }
        return stable;
    }
};


class FastDebounce {
public:
    FastDebounce(uint16_t t) : time(t), last(0), stable(false) {}

    inline bool update(bool input) {
        return update(input, millis());
    }

    inline bool update(bool input, uint32_t now ) {
        if (input != stable) {
            // cambiamento rilevato → aspetta che sia stabile per time ms
            if (now - last >= time) {
                stable = input;
            }
        } else {
            // stato stabile → aggiorna il riferimento temporale
            last = now;
        }

        return stable;
    }

private:
    uint16_t time;
    uint32_t last;
    bool stable;
};


class Edge {
public:
    bool last = false;

    bool Rising(bool in) {
        bool r = (!last && in);
        last = in;
        return r;
    }

    bool Falling(bool in) {
        bool f = (last && !in);
        last = in;
        return f;
    }

    bool Change(bool in) {
        bool c = (last != in);
        last = in;
        return c;
    }
};

#endif
