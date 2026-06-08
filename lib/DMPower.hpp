#ifndef DMPower_HPP
#define DMPower_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑03‑26
   Note:
                    • Release iniziale

   ============================================================================ */

/*===============================================================================
DMPower — Modular Energy Management Framework
===============================================================================
DMPower is a fully modular, real‑time energy‑management framework designed for
Opta‑class embedded controllers with photovoltaic systems, battery storage and
smart loads.

The system is composed of independent managers, each responsible for a specific
domain, coordinated by a lightweight PowerManager orchestrator.

-------------------------------------------------------------------------------
CORE MANAGERS
-------------------------------------------------------------------------------

PVStringManager:
    - Manages multiple independent photovoltaic strings.
    - Per‑string nominal power, real‑time power, lux/temperature factors.
    - Solar forecast engine with sunrise/sunset curves, lux correction,
      temperature derating, volatility analysis and forecast‑error EMA.
    - Provides total PV power and per‑string diagnostics.

BatteryManager:
    - Tracks SOC, SOH, temperature and instantaneous charge/discharge power.
    - Provides battery‑aware behaviors for optimization and auto‑tuning.
    - Supports fallback operation when battery is not present.

LoadManager:
    - Manages normal and thermal loads with min‑on/min‑off timers.
    - Priority‑based attach/detach logic with hysteresis.
    - Suggestion engine with debounce and optional auto‑execution.
    - Thermal‑load controller with solar/battery‑aware boosting.

LimitManager:
    - Grid‑limit protection with warning and exceeded callbacks.
    - Battery‑compensated net‑power evaluation.
    - Dynamic limit integration for optimization modes.

AutoTuneManager:
    - Adaptive hysteresis based on cycle count, lux volatility and forecast error.
    - Battery‑aware tuning of hysteresis and min‑on/min‑off timers.
    - Cycle‑time learning and smoothing.

DiagnosticManager:
    - Structured reporting for PV strings, battery, loads and thermal loads.
    - Non‑intrusive, suitable for Serial, MQTT or Web UI diagnostics.

-------------------------------------------------------------------------------
POWERMANAGER (ORCHESTRATOR)
-------------------------------------------------------------------------------
PowerManager coordinates all managers without embedding domain logic:

    - Collects PV forecast and real power.
    - Computes net grid power and battery compensation.
    - Applies optimization mode (self‑consumption, economic, comfort,
      grid‑protection, balanced).
    - Delegates:
        * limit checks to LimitManager
        * load control to LoadManager
        * auto‑tuning to AutoTuneManager
        * PV processing to PVStringManager
        * battery logic to BatteryManager

This architecture ensures:
    - High modularity and testability
    - Clear separation of responsibilities
    - Easy extension (Modbus inverter, MQTT, UI, logging)
    - Industrial‑grade stability and maintainability

-------------------------------------------------------------------------------
Together, these components form a complete, extensible and production‑ready
energy‑management layer for embedded photovoltaic and battery systems.
=============================================================================== */


#include <Arduino.h>
#include <math.h>

#include "DMBaseClass.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class PowerSupervisor {
public:
    // ============================================================
    // NOMI ALLARMI (PUBBLICI)
    // ============================================================
    static constexpr const char* ALARM_MAIN_POWER   = "MainPower";
    static constexpr const char* ALARM_24V_OK       = "24V_OK";
    static constexpr const char* ALARM_FAULT        = "Fault";
    static constexpr const char* ALARM_BATTERY_MODE = "BatteryMode";

    // ============================================================
    // STRUTTURA LETTURA MAIN POWER
    // ============================================================
    struct PowerValue {
        float value;
        bool valid;
    };

    static constexpr long INVALID_VALUE = -2147483647L;

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================
    struct Config {
        std::function<PowerValue()> readMainPower;
        std::function<bool()>       read24vOk;
        std::function<bool()>       readFault;
        std::function<bool()>       readBatteryMode;

        float mainPowerLow  = 210.0f;
        float mainPowerHigh = 250.0f;
    };

    // ============================================================
    // EXTRA SIGNAL
    // ============================================================
    struct Signal {
        String name;
        std::function<long()> readFn;
        long lastValue = INVALID_VALUE;
        bool alarm = false;
    };

    using AlarmCallback = std::function<void(const String&, long)>;

    // ============================================================
    // COSTRUTTORE
    // ============================================================
    explicit PowerSupervisor(const Config& cfg)
        : cfg(cfg)
    {}

    // ============================================================
    // GETTER
    // ============================================================
    float getMainPower() const { return mainPower; }
    bool  isMainPowerValid() const { return mainPowerValid; }

    bool  is24vOk() const { return power24vOk; }
    bool  isFault() const { return fault; }
    bool  isBatteryMode() const { return batteryMode; }

    size_t getExtraSignalCount() const { return extra.size(); }
    const Signal& getExtraSignal(size_t i) const { return extra[i]; }

    // ============================================================
    // EXTRA SIGNALS
    // ============================================================
    void addExtraSignal(const String& name, std::function<long()> fn) {
        Signal s;
        s.name = name;
        s.readFn = fn;
        s.lastValue = INVALID_VALUE;
        s.alarm = false;
        extra.push_back(s);
    }

    // ============================================================
    // CALLBACK ALLARMI
    // ============================================================
    void onAlarm(AlarmCallback cb) {
        alarmCb = cb;
    }

    // ============================================================
    // SETUP
    // ============================================================
    void setup() {
        
    }

    // ============================================================
    // LOOP NON-BLOCCANTE
    // ============================================================
    void loop(unsigned long now) {
        // -------------------------
        // MAIN POWER
        // -------------------------
        PowerValue pv = safeRead(cfg.readMainPower, PowerValue{0.0f, false});
        mainPowerValid = pv.valid;

        if (pv.valid) {
            LOG_DF("PowerSupervisor", "pv= %d",pv.value);
            if (pv.value < cfg.mainPowerLow || pv.value > cfg.mainPowerHigh) {
                triggerAlarm(ALARM_MAIN_POWER, pv.value);
            }

            mainPower = pv.value;
        } else LOG_DF("PowerSupervisor", "pv not valid");

        // -------------------------
        // 24V OK
        // -------------------------
        bool p24 = safeRead(cfg.read24vOk, false);
        if (power24vOk && !p24) triggerAlarm(ALARM_24V_OK, 0);
        power24vOk = p24;

        // -------------------------
        // FAULT
        // -------------------------
        bool f = safeRead(cfg.readFault, false);
        if (!fault && f) triggerAlarm(ALARM_FAULT, 1);
        fault = f;

        // -------------------------
        // BATTERY MODE
        // -------------------------
        bool b = safeRead(cfg.readBatteryMode, false);
        if (!batteryMode && b) triggerAlarm(ALARM_BATTERY_MODE, 1);
        batteryMode = b;

        // -------------------------
        // EXTRA SIGNALS
        // -------------------------
        for (auto& s : extra) {
            long val = safeRead(s.readFn, 0L);

            if (s.lastValue == INVALID_VALUE) {
                s.lastValue = val;
                continue;
            }

            if (val != s.lastValue) {
                triggerAlarm(s.name, val);
            }

            s.lastValue = val;
        }
    }

    // ============================================================
    // DIAGNOSTICA
    // ============================================================
    class Diagnostic {
    public:
        static void Report(const PowerSupervisor& ps) {
            Serial.println("\n===== POWER SUPERVISOR =====");

            // MAIN POWER
            Serial.print("MainPower: ");
            Serial.print(ps.mainPower);
            Serial.print(" V   (");
            Serial.print(ps.mainPowerValid ? "VALID" : "INVALID");
            Serial.println(")");

            // 24V OK
            Serial.print("24V OK: ");
            Serial.println(ps.power24vOk ? "YES" : "NO");

            // FAULT
            Serial.print("Fault: ");
            Serial.println(ps.fault ? "YES" : "NO");

            // BATTERY MODE
            Serial.print("Battery Mode: ");
            Serial.println(ps.batteryMode ? "YES" : "NO");

            // EXTRA SIGNALS
            Serial.println("Extra signals:");
            if (ps.extra.empty()) {
                Serial.println(" - None");
            } else {
                for (auto& s : ps.extra) {
                    Serial.print(" - ");
                    Serial.print(s.name);
                    Serial.print(" = ");
                    Serial.println(s.lastValue);
                }
            }

            Serial.println("===== END POWER SUPERVISOR =====\n");
        }
    };

private:
    struct AlarmState {
        String name;
        long lastValue;
    };
    std::vector<AlarmState> alarmStates;

    inline void triggerAlarm(const String& name, long value) {
        LOG_IF("PowerSupervisor", "triggerAlarm");
        // Cerca se esiste già uno stato per questo segnale
        for (auto& s : alarmStates) {
            if (s.name == name) {
                // Se il valore è identico → ignora
                if (s.lastValue == value)
                    return;

                // Valore cambiato → aggiorna e notifica
                s.lastValue = value;
                if (alarmCb) alarmCb(name, value);
                return;
            }
        }

        // Primo allarme per questo segnale → aggiungi e notifica
        alarmStates.push_back({name, value});
        if (alarmCb) alarmCb(name, value);
    }

    // ============================================================
    // VARIABILI INTERNE
    // ============================================================
    Config cfg;
    AlarmCallback alarmCb = nullptr;

    float mainPower = 0;
    bool  mainPowerValid = false;

    bool power24vOk = false;
    bool fault = false;
    bool batteryMode = false;

    std::vector<Signal> extra;
};

class BatteryManager {
public:

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================
    struct Config {
        bool present = false;

        float minSOC = 20.0f;
        float maxSOC = 95.0f;

        float maxChargeW = 3000.0f;
        float maxDischargeW = 3000.0f;

        float nominalSolarW = 3000.0f;

        float minTemp = 0.0f;
        float maxTemp = 45.0f;
    };

    // ============================================================
    // COSTRUTTORE
    // ============================================================
    explicit BatteryManager(const Config& cfg)
        : cfg(cfg)
    {}

    // ============================================================
    // API DI RUNTIME
    // ============================================================
    void setSOC(float soc) { SOC = constrain(soc, 0.0f, 100.0f); }
    float getSOC() const { return SOC; }

    void setSOH(float soh) { SOH = constrain(soh, 0.0f, 100.0f); }
    float getSOH() const { return SOH; }

    void setTemperature(float t) { temperature = t; }

    float getPower() const { return batteryPower; }

    // ============================================================
    // LOGICA PRINCIPALE
    // ============================================================
    float update(float netGridPower, float forecastW, unsigned long now) {
        if (!cfg.present) return 0.0f;

        // Protezione temperatura
        if (temperature < cfg.minTemp || temperature > cfg.maxTemp) {
            batteryPower = 0;
            return batteryPower;
        }

        // Protezione SOC
        if (SOC <= cfg.minSOC) {
            batteryPower = 0;
            return batteryPower;
        }
        if (SOC >= cfg.maxSOC) {
            batteryPower = 0;
            return batteryPower;
        }

        // Surplus → carica
        if (netGridPower < 0) {
            float surplus = -netGridPower;
            float charge = min(surplus, cfg.maxChargeW);
            batteryPower = -charge;
        }
        // Deficit → scarica
        else {
            float deficit = netGridPower;
            float discharge = min(deficit, cfg.maxDischargeW);
            batteryPower = discharge;
        }

        // Forecast: anticipa carica
        if (forecastW > cfg.nominalSolarW * 0.5f && SOC < cfg.maxSOC - 5) {
            batteryPower = -cfg.maxChargeW * 0.5f;
        }

        // Anti oscillazione
        if (now - lastCycleTime < 30000) return batteryPower;
        lastCycleTime = now;

        return batteryPower;
    }

    bool isPresent() const { return cfg.present; }

    float getTemperature() const { return temperature; }

private:
    Config cfg;

    float SOC = 50.0f;
    float SOH = 100.0f;
    float temperature = 25.0f;

    float batteryPower = 0.0f;

    unsigned long lastCycleTime = 0;
};

class PVStringManager {
public:

    // ============================
    // STRUTTURA STRINGA FV
    // ============================
    struct PVString {
        String name;          // Nome della stringa (es. "Tetto Sud")
        float nominalWatt;    // Potenza nominale
        float luxFactor;      // Fattore di luce (1.0 default)
        float tempFactor;     // Fattore temperatura (1.0 default)
        float realPower;      // Potenza reale letta
    };

    // ============================
    // COSTRUTTORE
    // ============================
    PVStringManager() {
        for (int i = 0; i < LUX_HISTORY_SIZE; i++)
            luxHistory[i] = 0.0f;
    }

    // ============================
    // AGGIUNTA STRINGA
    // ============================
    void addString(const String& name,
                   float nominalWatt,
                   float luxFactor = 1.0f,
                   float tempFactor = 1.0f)
    {
        strings.push_back({ name, nominalWatt, luxFactor, tempFactor, 0.0f });
    }

    // ============================
    // POTENZA REALE PER STRINGA
    // ============================
    void setStringPower(size_t index, float watt) {
        if (index < strings.size())
            strings[index].realPower = watt;
    }

    // ============================
    // POTENZA TOTALE FV
    // ============================
    float getTotalPower() const {
        float tot = 0.0f;
        for (auto& s : strings) tot += s.realPower;
        return tot;
    }

    // ============================
    // AGGIORNAMENTO LUX E TEMPERATURA
    // ============================
    void updateLux(float lux) {
        luxHistory[luxIndex] = lux;
        luxIndex = (luxIndex + 1) % LUX_HISTORY_SIZE;
        if (luxIndex == 0) luxFilled = true;
        luxValue = lux;
    }

    void setTempExt(float t) {
        temperatureExt = t;
    }

    // ============================
    // VOLATILITÀ LUX
    // ============================
    float getLuxVolatility() const {
        if (!luxFilled) return 0.0f;

        float mean = 0.0f;
        for (int i = 0; i < LUX_HISTORY_SIZE; i++)
            mean += luxHistory[i];
        mean /= LUX_HISTORY_SIZE;

        float var = 0.0f;
        for (int i = 0; i < LUX_HISTORY_SIZE; i++) {
            float d = luxHistory[i] - mean;
            var += d * d;
        }

        return sqrt(var / LUX_HISTORY_SIZE);
    }

    // ============================
    // FORECAST MULTI‑STRINGA
    // ============================
    float getForecast(int month, int hour, int minute) const {
        int t = hour * 60 + minute;
        DayInfo d = months[month - 1];

        if (t < d.sunrise || t > d.sunset)
            return 0.0f;

        float x = float(t - d.sunrise) / float(d.sunset - d.sunrise);
        float total = 0.0f;

        for (auto& s : strings) {

            float theoretical = s.nominalWatt * sin(PI * x);

            float lightFactor = min(luxValue / 100000.0f, 1.0f);
            float tempFactor = max(1.0f - 0.0045f * (temperatureExt - 25.0f), 0.0f);

            float base = theoretical * lightFactor * tempFactor;

            total += base * s.luxFactor * s.tempFactor;
        }

        return total;
    }

    // ============================
    // FORECAST ERROR EMA
    // ============================
    void updateForecastError(float forecastW, float actualW) {
        float err = 0.0f;
        if (forecastW > 0.0f)
            err = fabs(forecastW - actualW) / forecastW;

        forecastErrorMA = forecastErrorAlpha * err +
                          (1.0f - forecastErrorAlpha) * forecastErrorMA;
    }

    float getForecastError() const {
        return forecastErrorMA;
    }

    // ============================
    // ACCESSORI
    // ============================
    size_t count() const { return strings.size(); }
    const PVString& get(size_t i) const { return strings[i]; }

    float getTotalNominalPower() const {
        float tot = 0.0f;
        for (auto& s : strings) tot += s.nominalWatt;
        return tot;
    }

private:

    // ============================
    // DATI INTERNI
    // ============================
    std::vector<PVString> strings;

    float luxValue = 0.0f;
    float temperatureExt = 25.0f;

    // Forecast error
    float forecastErrorMA = 0.0f;
    float forecastErrorAlpha = 0.1f;

    // Storico lux
    static const int LUX_HISTORY_SIZE = 60;
    float luxHistory[LUX_HISTORY_SIZE];
    int luxIndex = 0;
    bool luxFilled = false;

    // Tabella alba/tramonto
    struct DayInfo { int sunrise; int sunset; };
    DayInfo months[12] = {
        {480,1020},{450,1050},{420,1080},{390,1110},
        {360,1140},{360,1140},{390,1110},{420,1080},
        {450,1050},{480,1020},{510,990},{540,960}
    };
};


class LoadManager {
private:
    float forecastPeak = 3000.0f; // default, sovrascritto dal PM

public:

    // ============================
    // STRUTTURE CARICHI
    // ============================
    struct Load {
        String name;
        float nominalPower;
        bool state;
        unsigned long minOnMs;
        unsigned long minOffMs;
        unsigned long lastChange;
        int priority = 0;
        bool suggestedOn;
        bool suggestedOff;

        // auto-tuning
        unsigned int cyclesCount = 0;
        unsigned long lastCycleTimestamp = 0;
        float avgCycleTimeSec = 0.0f;
    };

    struct ThermalLoad {
        String name;
        bool heatingMode;
        float baseTarget;
        float comfortMin;
        float comfortMax;
        bool state;
        unsigned long minOnMs;
        unsigned long minOffMs;
        unsigned long lastChange;
        bool suggestedOn;
        bool suggestedOff;
    };

    // ============================
    // CALLBACKS
    // ============================
    using LoadCb = std::function<void(const String&, bool)>;
    using SuggestionCb = std::function<void(const String&, int, const String&)>;

    void setCallbacks(LoadCb cbLoad, SuggestionCb cbSuggest) {
        onLoadChange = cbLoad;
        onSuggestion = cbSuggest;
    }

    // ============================
    // AGGIUNTA CARICHI
    // ============================
    void addLoad(const String& name, float nominalPower,
                 unsigned long minOnSec = 5, unsigned long minOffSec = 5)
    {
        loads.push_back({
            name, nominalPower, false,
            minOnSec * 1000UL, minOffSec * 1000UL,
            0, false, false
        });
    }

    void addThermalLoad(const String& name, bool heatingMode,
                        float baseTarget, float comfortMin, float comfortMax,
                        unsigned long minOnSec = 30, unsigned long minOffSec = 30)
    {
        thermal.push_back({
            name, heatingMode, baseTarget, comfortMin, comfortMax,
            false, minOnSec * 1000UL, minOffSec * 1000UL,
            0, false, false
        });
    }

    // ============================
    // LOGICA CARICHI NORMALI
    // ============================
    void updateLoads(float netWithBattery, float dynamicLimit, unsigned long now) {

        // Evita suggerimenti inutili all’avvio
        bool allOff = true;
        for (auto& l : loads) {
            if (l.state) { allOff = false; break; }
        }
        if (allOff && netWithBattery < dynamicLimit)
            return;

        // -------------------------
        // ATTACCO CARICHI
        // -------------------------
        if (netWithBattery <= dynamicLimit - hysteresisOn) {

            for (auto& l : loads) {
                if (!l.state && !l.suggestedOn) {

                    if (!canChange(l.lastChange, l.minOffMs))
                        continue;

                    l.suggestedOn = true;
                    l.suggestedOff = false;

                    suggest("attacca:" + l.name, 0, "margine disponibile", now);
                }
            }
            return;
        }

        // -------------------------
        // STACCO CARICHI
        // -------------------------
        if (netWithBattery >= dynamicLimit + hysteresisOff) {

            for (auto& l : loads) {
                if (l.state && !l.suggestedOff) {

                    if (!canChange(l.lastChange, l.minOnMs))
                        continue;

                    l.suggestedOff = true;
                    l.suggestedOn = false;

                    suggest("stacca:" + l.name, 2, "superamento limite", now);
                }
            }
        }
    }

    // ============================
    // LOGICA CARICHI TERMICI
    // ============================
    void updateThermal(float indoorTemp, float forecast, unsigned long now) {

        for (auto& t : thermal) {

            float boost = 0.0f;

            // Il thermal boost deve essere deciso dal PowerManager
            // LoadManager NON deve conoscere la potenza FV nominale
            if (forecast > 0.6f * forecastPeak)
                boost += t.heatingMode ? 1.0f : -1.0f;

            float target = t.baseTarget + boost;

            if (t.heatingMode) {
                if (indoorTemp < target && indoorTemp < t.comfortMax) {
                    suggest("attacca:" + t.name, 1, "thermal control", now);
                } else if (indoorTemp > target + 0.5f) {
                    suggest("stacca:" + t.name, 1, "thermal control", now);
                }
            } else {
                if (indoorTemp > target && indoorTemp > t.comfortMin) {
                    suggest("attacca:" + t.name, 1, "thermal control", now);
                } else if (indoorTemp < target - 0.5f) {
                    suggest("stacca:" + t.name, 1, "thermal control", now);
                }
            }
        }
    }

    // ============================
    // ACCESSORI
    // ============================
    const std::vector<Load>& getLoads() const { return loads; }
    const std::vector<ThermalLoad>& getThermalLoads() const { return thermal; }

    void setHysteresis(float hOn, float hOff) {
        hysteresisOn = hOn;
        hysteresisOff = hOff;
    }

    void setForecastPeak(float w) { forecastPeak = w; }

    std::vector<Load>& getLoadsMutable() { return loads; }
    std::vector<ThermalLoad>& getThermalLoadsMutable() { return thermal; }

private:

    // ============================
    // DATI INTERNI
    // ============================
    std::vector<Load> loads;
    std::vector<ThermalLoad> thermal;

    float hysteresisOn = 200.0f;
    float hysteresisOff = 200.0f;

    LoadCb onLoadChange = nullptr;
    SuggestionCb onSuggestion = nullptr;

    // ============================
    // FUNZIONI INTERNE
    // ============================
    bool canChange(unsigned long last, unsigned long minTime) const {
        return (millis() - last) >= minTime;
    }

    void suggest(const String& s, int severity, const String& reason, unsigned long now) {
        if (onSuggestion)
            onSuggestion(s, severity, reason);
    }
};


class LimitManager {
public:

    using LimitCb     = std::function<void(float netPower, float limit)>;
    using HardLimitCb = std::function<void(float netPower, float limit)>;

    void setCallbacks(LimitCb warnCb, LimitCb exceedCb) {
        onWarn   = warnCb;
        onExceed = exceedCb;
    }

    void setHardLimitCallback(HardLimitCb cb) {
        onHardLimit = cb;
    }

    void setLimit(float limitWatt) {
        gridLimit = limitWatt;
    }

    void setHardLimit(float limitWatt) {
        hardLimit = limitWatt;
    }

    void setWarningThreshold(float pct) {
        if (pct > 0.0f && pct < 1.0f)
            warningPct = pct;
    }

    float getLimit() const { return gridLimit; }
    float getHardLimit() const { return hardLimit; }
    float getWarningThreshold() const { return warningPct; }

    void check(float netPowerWithBattery, float dynamicLimit) {

        if (netPowerWithBattery >= dynamicLimit * warningPct &&
            netPowerWithBattery < dynamicLimit)
        {
            if (onWarn)
                onWarn(netPowerWithBattery, dynamicLimit);
        }

        if (netPowerWithBattery >= dynamicLimit) {
            if (onExceed)
                onExceed(netPowerWithBattery, dynamicLimit);
        }

        if (hardLimit > 0 && netPowerWithBattery >= hardLimit) {
            if (onHardLimit)
                onHardLimit(netPowerWithBattery, hardLimit);
        }
    }

private:
    float gridLimit = 3000.0f;
    float hardLimit = 0.0f;
    float warningPct = 0.9f;

    LimitCb onWarn = nullptr;
    LimitCb onExceed = nullptr;
    HardLimitCb onHardLimit = nullptr;
};



class AutoTuneManager {
public:

    // ============================
    // CONFIGURAZIONE
    // ============================
    void configure(float hOn, float hOff,
                   float stepPct = 0.05f,
                   float hMin = 50.0f,
                   float hMax = 2000.0f,
                   unsigned long minOnMin = 5,
                   unsigned long minOnMax = 3600,
                   unsigned long minOffMin = 5,
                   unsigned long minOffMax = 3600)
    {
        hysteresisOn = hOn;
        hysteresisOff = hOff;
        tuneStepPercent = stepPct;

        hysteresisMin = hMin;
        hysteresisMax = hMax;

        minOnMinSec = minOnMin;
        minOnMaxSec = minOnMax;
        minOffMinSec = minOffMin;
        minOffMaxSec = minOffMax;
    }

    // ============================
    // UPDATE PRINCIPALE
    // ============================
    void update(float luxVol,
                float forecastErr,
                std::vector<LoadManager::Load>& loads,
                const BatteryManager& battery)
    {
        // ============================
        // 1) METRICHE CARICHI
        // ============================
        float avgCycles = 0.0f;
        int cnt = 0;

        for (auto& l : loads) {
            avgCycles += (float)l.cyclesCount;
            cnt++;
        }
        if (cnt > 0) avgCycles /= cnt;

        // ============================
        // 2) DECISIONE ISTERESI
        // ============================
        bool increaseHyst =
            (avgCycles > 3.0f) ||
            (luxVol > 20000.0f) ||
            (forecastErr > 0.25f);

        bool decreaseHyst =
            (avgCycles < 1.0f) &&
            (luxVol < 5000.0f) &&
            (forecastErr < 0.1f);

        float deltaOn = hysteresisOn * tuneStepPercent;
        float deltaOff = hysteresisOff * tuneStepPercent;

        if (increaseHyst) {
            hysteresisOn  = min(hysteresisOn  + deltaOn,  hysteresisMax);
            hysteresisOff = min(hysteresisOff + deltaOff, hysteresisMax);
        }
        else if (decreaseHyst) {
            hysteresisOn  = max(hysteresisOn  - deltaOn,  hysteresisMin);
            hysteresisOff = max(hysteresisOff - deltaOff, hysteresisMin);
        }

        // ============================
        // 3) INTEGRAZIONE BATTERIA
        // ============================
        if (battery.isPresent()) {

            float soc = battery.getSOC();
            float bp  = battery.getPower(); // >0 scarica, <0 carica

            // Batteria scarica → isteresi più piccole
            if (soc < 25.0f) {
                hysteresisOn  = max(hysteresisOn  * (1.0f - tuneStepPercent), hysteresisMin);
                hysteresisOff = max(hysteresisOff * (1.0f - tuneStepPercent), hysteresisMin);
            }

            // Batteria molto carica → isteresi più grandi
            if (soc > 90.0f) {
                hysteresisOn  = min(hysteresisOn  * (1.0f + tuneStepPercent), hysteresisMax);
                hysteresisOff = min(hysteresisOff * (1.0f + tuneStepPercent), hysteresisMax);
            }

            // Batteria che SCARICA forte → minOn/minOff più piccoli
            if (bp > 500.0f) {
                for (auto& l : loads) {
                    l.minOnMs  = max((unsigned long)(l.minOnMs  * (1.0f - tuneStepPercent)), minOnMinSec  * 1000UL);
                    l.minOffMs = max((unsigned long)(l.minOffMs * (1.0f - tuneStepPercent)), minOffMinSec * 1000UL);
                }
            }

            // Batteria che CARICA forte → minOn/minOff più grandi
            if (bp < -500.0f) {
                for (auto& l : loads) {
                    l.minOnMs  = min((unsigned long)(l.minOnMs  * (1.0f + tuneStepPercent)), minOnMaxSec  * 1000UL);
                    l.minOffMs = min((unsigned long)(l.minOffMs * (1.0f + tuneStepPercent)), minOffMaxSec * 1000UL);
                }
            }
        }

        // ============================
        // 4) AUTO‑TUNING CARICHI (logica originale)
        // ============================
        for (auto& l : loads) {

            if (l.cyclesCount >= 3 && l.avgCycleTimeSec < 120.0f) {

                unsigned long newMinOn =
                    min((unsigned long)(l.minOnMs * (1.0f + tuneStepPercent)),
                        minOnMaxSec * 1000UL);

                unsigned long newMinOff =
                    min((unsigned long)(l.minOffMs * (1.0f + tuneStepPercent)),
                        minOffMaxSec * 1000UL);

                l.minOnMs = newMinOn;
                l.minOffMs = newMinOff;
            }
            else if (l.cyclesCount == 0 && l.avgCycleTimeSec > 600.0f) {

                unsigned long newMinOn =
                    max((unsigned long)(l.minOnMs * (1.0f - tuneStepPercent)),
                        minOnMinSec * 1000UL);

                unsigned long newMinOff =
                    max((unsigned long)(l.minOffMs * (1.0f - tuneStepPercent)),
                        minOffMinSec * 1000UL);

                l.minOnMs = newMinOn;
                l.minOffMs = newMinOff;
            }

            // reset contatori
            l.cyclesCount = 0;
            l.avgCycleTimeSec = 0.0f;
            l.lastCycleTimestamp = 0;
        }
    }

    // ============================
    // ACCESSORI
    // ============================
    float getHystOn() const { return hysteresisOn; }
    float getHystOff() const { return hysteresisOff; }

private:

    // ============================
    // PARAMETRI
    // ============================
    float hysteresisOn  = 200.0f;
    float hysteresisOff = 200.0f;

    float tuneStepPercent = 0.05f;

    float hysteresisMin = 50.0f;
    float hysteresisMax = 2000.0f;

    unsigned long minOnMinSec  = 5;
    unsigned long minOnMaxSec  = 3600;
    unsigned long minOffMinSec = 5;
    unsigned long minOffMaxSec = 3600;
};

class DiagnosticManager {
public:

    // ============================
    // REPORT STRINGHE FV
    // ============================
    static void ReportPV(const PVStringManager& pv) {
        Serial.println("\n===== PV STRINGS =====");

        if (pv.count() == 0) {
            Serial.println("No PV strings defined.");
            return;
        }

        for (size_t i = 0; i < pv.count(); i++) {
            const auto& s = pv.get(i);

            Serial.println("------------------------------");
            Serial.print("String #"); Serial.println(i);

            Serial.print("Name: "); Serial.println(s.name);
            Serial.print("Nominal: "); Serial.print(s.nominalWatt); Serial.println(" W");
            Serial.print("Real Power: "); Serial.print(s.realPower); Serial.println(" W");

            Serial.print("Lux Factor: "); Serial.println(s.luxFactor);
            Serial.print("Temp Factor: "); Serial.println(s.tempFactor);
        }

        Serial.println("------------------------------");
        Serial.print("Total PV Power: ");
        Serial.print(pv.getTotalPower());
        Serial.println(" W");

        Serial.print("Lux Volatility: ");
        Serial.println(pv.getLuxVolatility());

        Serial.print("Forecast Error EMA: ");
        Serial.println(pv.getForecastError());
    }

    // ============================
    // REPORT BATTERIA
    // ============================
    static void ReportBattery(const BatteryManager& b) {
        Serial.println("\n===== BATTERY =====");

        if (!b.isPresent()) {
            Serial.println("Battery not present.");
            return;
        }

        Serial.print("SOC: "); Serial.print(b.getSOC()); Serial.println(" %");
        Serial.print("SOH: "); Serial.print(b.getSOH()); Serial.println(" %");
        Serial.print("Temperature: "); Serial.print(b.getTemperature()); Serial.println(" °C");
        Serial.print("Power: "); Serial.print(b.getPower()); Serial.println(" W");
    }

    // ============================
    // REPORT CARICHI NORMALI
    // ============================
    static void ReportLoads(const LoadManager& lm) {
        Serial.println("\n===== LOADS =====");

        const auto& loads = lm.getLoads();

        if (loads.empty()) {
            Serial.println("No loads defined.");
            return;
        }

        for (size_t i = 0; i < loads.size(); i++) {
            const auto& l = loads[i];

            Serial.println("------------------------------");
            Serial.print("Load #"); Serial.println(i);

            Serial.print("Name: "); Serial.println(l.name);
            Serial.print("Nominal Power: "); Serial.print(l.nominalPower); Serial.println(" W");

            Serial.print("State: "); Serial.println(l.state ? "ON" : "OFF");

            Serial.print("Min ON: "); Serial.print(l.minOnMs); Serial.println(" ms");
            Serial.print("Min OFF: "); Serial.print(l.minOffMs); Serial.println(" ms");

            Serial.print("Cycles: "); Serial.println(l.cyclesCount);
            Serial.print("Avg cycle time: "); Serial.print(l.avgCycleTimeSec); Serial.println(" s");

            Serial.print("Suggested ON: "); Serial.println(l.suggestedOn ? "YES" : "NO");
            Serial.print("Suggested OFF: "); Serial.println(l.suggestedOff ? "YES" : "NO");
        }
    }

    // ============================
    // REPORT CARICHI TERMICI
    // ============================
    static void ReportThermal(const LoadManager& lm) {
        Serial.println("\n===== THERMAL LOADS =====");

        const auto& th = lm.getThermalLoads();

        if (th.empty()) {
            Serial.println("No thermal loads defined.");
            return;
        }

        for (size_t i = 0; i < th.size(); i++) {
            const auto& t = th[i];

            Serial.println("------------------------------");
            Serial.print("Thermal #"); Serial.println(i);

            Serial.print("Name: "); Serial.println(t.name);
            Serial.print("Mode: "); Serial.println(t.heatingMode ? "HEATING" : "COOLING");

            Serial.print("Comfort range: ");
            Serial.print(t.comfortMin); Serial.print(" - "); Serial.println(t.comfortMax);

            Serial.print("State: "); Serial.println(t.state ? "ON" : "OFF");

            Serial.print("Min ON: "); Serial.print(t.minOnMs); Serial.println(" ms");
            Serial.print("Min OFF: "); Serial.print(t.minOffMs); Serial.println(" ms");

            Serial.print("Suggested ON: "); Serial.println(t.suggestedOn ? "YES" : "NO");
            Serial.print("Suggested OFF: "); Serial.println(t.suggestedOff ? "YES" : "NO");
        }
    }

    // ============================
    // REPORT COMPLETO
    // ============================
    static void FullReport(const PVStringManager& pv,
                           const BatteryManager& b,
                           const LoadManager& lm)
    {
        Serial.println("\n====================================");
        Serial.println("          SYSTEM DIAGNOSTIC         ");
        Serial.println("====================================");

        ReportPV(pv);
        ReportBattery(b);
        ReportLoads(lm);
        ReportThermal(lm);

        Serial.println("====================================\n");
    }
};

class PowerManager {
public:
    struct Config {
        bool autoTune = false;
        unsigned long tuneIntervalMs = 60000;
    };

private:
    LoadManager::LoadCb onLoadChange = nullptr;
    LimitManager::LimitCb onLimitWarning = nullptr;
    LimitManager::LimitCb onLimitExceeded = nullptr;
    std::function<void(int,const String&)> onError = nullptr;
    LoadManager::SuggestionCb onSuggestion = nullptr;
    Config cfg;

    float solarPower = 0;
    float batteryPower = 0;
    float forecastW = 0;

    float hardLimit = 0.0f;

public:

    enum class OptimizationMode {
        MASSIMO_AUTOCONSUMO,
        RISPARMIO_ECONOMICO,
        MASSIMO_COMFORT,
        PROTEZIONE_RETE,
        BILANCIATO
    };

    // ============================================================
    // COSTRUTTORE
    // ============================================================
    PowerManager(float gridLimitWatt,
                 const BatteryManager::Config& batteryCfg)
        : battery(batteryCfg)
    {
        limits.setLimit(gridLimitWatt);
    }

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================
    void setOptimizationMode(OptimizationMode m) { mode = m; }

    void setEnvironmental(float lux, float tempExt) {
        pv.updateLux(lux);
        pv.setTempExt(tempExt);
    }

    void setGridPower(float w) { gridPower = w; }
    void setStringPower(size_t idx, float w) { pv.setStringPower(idx, w); }

    PVStringManager& pvManager() { return pv; }
    LoadManager& loadManager() { return loads; }
    BatteryManager& batteryManager() { return battery; }
    LimitManager& limitManager() { return limits; }
    AutoTuneManager& autoTuneManager() { return autotune; }

    // ============================================================
    // UPDATE PRINCIPALE (da chiamare nel loop)
    // ============================================================
    void update(int month, int hour, int minute,
                float indoorTemp,
                unsigned long now)
    {
        // 1) FV
        float solarForecast = pv.getForecast(month, hour, minute);
        float solarReal = pv.getTotalPower();

        // 🔥 Passiamo al LoadManager la potenza FV nominale totale
        loads.setForecastPeak(pv.getTotalNominalPower());

        // 2) Net power
        float netGrid = gridPower - solarReal;

        // 3) Batteria
        float batteryAction = battery.update(netGrid, solarForecast, now);
        float netWithBattery = netGrid - batteryAction;

        // 4) Limiti
        float dynamicLimit = applyOptimizationMode(limits.getLimit() + solarForecast);
        limits.check(netWithBattery, dynamicLimit);

        // 5) Carichi
        loads.setHysteresis(autotune.getHystOn(), autotune.getHystOff());
        loads.updateLoads(netWithBattery, dynamicLimit, now);
        loads.updateThermal(indoorTemp, solarForecast, now);

        // 6) Auto‑tuning
        autotune.update(
            pv.getLuxVolatility(),
            pv.getForecastError(),
            loads.getLoadsMutable(),
            battery
        );
    }

    // ============================================================================
    // FRONTEND CALLBACK API (compatibile con DMFrontendEngines)
    // ============================================================================

    // --- LOAD CHANGE ---
    void setOnLoadChange(LoadManager::LoadCb cb) {
        onLoadChange = cb;
        loads.setCallbacks(onLoadChange, onSuggestion);
    }

    // --- LIMIT WARNING ---
    void setOnLimitWarning(LimitManager::LimitCb cb) {
        onLimitWarning = cb;
        limits.setCallbacks(onLimitWarning, onLimitExceeded);
    }

    // --- LIMIT EXCEEDED ---
    void setOnLimitExceeded(LimitManager::LimitCb cb) {
        onLimitExceeded = cb;
        limits.setCallbacks(onLimitWarning, onLimitExceeded);
    }

    // --- ERROR CALLBACK ---
    void setOnError(std::function<void(int,const String&)> cb) {
        onError = cb;
    }

    // --- SUGGESTION CALLBACK ---
    void setOnSuggestion(LoadManager::SuggestionCb cb) {
        onSuggestion = cb;
        loads.setCallbacks(onLoadChange, onSuggestion);
    }

    float getGridPower() const { return gridPower; }
    float getSolarPower() const { return pv.getTotalPower(); }

    void setConfig(const Config& c) { cfg = c; }
    const Config& getConfig() const { return cfg; }

    void setSolarPower(float w) { solarPower = w; }
    void setBatteryPower(float w) { batteryPower = w; }
    void setForecast(float w) { forecastW = w; }

    void setHardLimit(float w) { hardLimit = w; }
    float getHardLimit() const { return hardLimit; }

    void fireSuggestion(const String& name, int severity, const String& reason) {
        if (onSuggestion)
            onSuggestion(name, severity, reason);
    }

    class Diagnostic {
    public:
        static void FullReport(PowerManager& pm) {
            DiagnosticManager::FullReport(
                pm.pvManager(),
                pm.batteryManager(),
                pm.loadManager()
            );
        }
    };

private:

    // ============================================================
    // MANAGER SPECIALIZZATI
    // ============================================================
    PVStringManager pv;
    BatteryManager battery;
    LoadManager loads;
    LimitManager limits;
    AutoTuneManager autotune;

    // ============================================================
    // PARAMETRI DI SISTEMA
    // ============================================================
    float gridPower = 0.0f;
    OptimizationMode mode = OptimizationMode::BILANCIATO;

    // ============================================================
    // LOGICA DI OTTIMIZZAZIONE (alto livello)
    // ============================================================
    float applyOptimizationMode(float dynamicLimit) {
        float out = dynamicLimit;

        switch (mode) {
            case OptimizationMode::MASSIMO_AUTOCONSUMO:
                out *= 1.2f;
                break;

            case OptimizationMode::RISPARMIO_ECONOMICO:
                out *= 0.8f;
                break;

            case OptimizationMode::MASSIMO_COMFORT:
                out += 500.0f;
                break;

            case OptimizationMode::PROTEZIONE_RETE:
                out *= 0.6f;
                break;

            case OptimizationMode::BILANCIATO:
            default:
                break;
        }

        // Ottimizzazione batteria
        if (battery.isPresent()) {
            float soc = battery.getSOC();

            if (soc <= 22.0f)
                out *= 0.7f;

            if (soc >= 93.0f)
                out *= 1.2f;
        }

        return out;
    }
};

#endif
