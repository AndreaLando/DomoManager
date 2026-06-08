#ifndef DMHVAC_H
#define DMHVAC_H

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
#include <vector>

#include "DMEquipment.hpp"

// ============================================================================
//   HEAT PUMP CONTROLLER (motore HVAC puro)
// ============================================================================

class HeatPumpController {
public:
    enum class Mode { OFF, MANUAL, HEATING, COOLING, AUTO, DEFROST };
    enum class FanSpeed { LOW, MEDIUM, HIGH };

    struct HVACTime {
        uint8_t dayOfWeek;
        uint8_t hour;
        uint8_t minute;
    };

    enum class HydraulicTarget {
        SPACE_HEATING,
        DHW
    };

    struct Config {
        float hysteresis = 0.5f;
        float fanHysteresis = 0.3f;

        float diffLow = 0.5f;
        float diffMed = 1.5f;

        float defrostThreshold = 3.0f;
        unsigned long defrostDurationMs = 300000;

        unsigned long maxOnTimeMs = 7200000;
        unsigned long minSwitchDelayMs = 120000;
        unsigned long minOffTimeMs = 180000;

        float minOutdoorTemp = -7.0f;
        float maxOutdoorTemp = 45.0f;

        unsigned long windowOpenTimeoutMs = 15000;

        unsigned long postCirculationMs = 60000;
        unsigned long minCirculationCycleMs = 5000;
    };

    // -------------------------
    //   ZONE HVAC
    // -------------------------
    class ZoneHVAC {
    public:
        ZoneHVAC(String name, float setpoint, int fanCoilId, const Config& cfg)
            : name(name),
              temperature(20.0f),
              setpoint(setpoint),
              fanCoilNumber(fanCoilId),
              heatRequest(false),
              coolRequest(false),
              previousFanCoilState(false),
              cfg(cfg) {}

        void updateTemperature(float t) {
            temperature = t;
            updateRequests();
        }

        void setSetpoint(float sp) {
            setpoint = sp;
            updateRequests();
        }

        bool needsHeating()  { return heatRequest; }
        bool needsCooling()  { return coolRequest; }

        int   getFanCoilNumber() const { return fanCoilNumber; }
        float getTemperature()   const { return temperature; }
        float getSetpoint()      const { return setpoint; }
        String getName()         const { return name; }

        bool previousFanCoilState = false;

    private:
        void updateRequests() {
            heatRequest  = (temperature < setpoint - cfg.hysteresis);
            coolRequest  = (temperature > setpoint + cfg.hysteresis);
        }

        String name;
        float  temperature;
        float  setpoint;
        int    fanCoilNumber;

        bool heatRequest;
        bool coolRequest;

        const Config& cfg;
    };

public:
    HeatPumpController(Mode mode, float setpoint, const Config& cfg);

    void setMode(Mode m);
    void setSetpoint(float sp);

    void updateIndoorTemperature(float t);
    void updateOutdoorTemperature(float t);

    ZoneHVAC* createZone(const String& name, float setpoint, int fanCoilId);
    void updateZone(const String& name, float temperature);
    void setWindowOpen(bool state, uint32_t now);

    void setCompressorCallback(std::function<void(bool)> cb) { compressorCallback = cb; }
    void setFanCoilCallback(std::function<void(String,int,bool)> cb) { fanCoilCallback = cb; }
    void setCirculationPumpCallback(std::function<void(bool)> cb) { circulationCallback = cb; }

    void setThreeWayValveCallback(std::function<void(HydraulicTarget)> cb) {
        threeWayValveCallback = cb;
    }

    void selectHydraulicTarget(HydraulicTarget t) {
        if (hydraulicTarget != t) {
            hydraulicTarget = t;
            if (threeWayValveCallback) threeWayValveCallback(t);
        }
    }

    HydraulicTarget getHydraulicTarget() const { return hydraulicTarget; }

    // -------------------------
    //   ACS CONFIG
    // -------------------------
    void enableACS(bool en) { acsEnabled = en; }
    void setACSPriority(bool en) { acsPriority = en; }
    void setDhwSetpoint(float sp) { dhwSetpoint = sp; }

    void setDhwTempReader(std::function<float()> fn) { readDhwTemp = fn; }

    void enableAntiLegionella(bool en) { antiLegionellaEnabled = en; }
    void setAntiLegionellaSchedule(uint8_t day, uint8_t hour) {
        antiLegionellaDay = day;
        antiLegionellaHour = hour;
    }
    void setAntiLegionellaParams(float sp, unsigned long maxMs) {
        antiLegionellaSetpoint = sp;
        antiLegionellaMaxDurationMs = maxMs;
    }

    // -------------------------
    //   GETTERS
    // -------------------------
    bool      isCompressorActive() const;
    FanSpeed  getFanSpeed()        const;
    Mode      getMode()            const;
    float     getSetpoint()        const;
    float     getIndoorTemperature()  const;
    float     getOutdoorTemperature() const;
    const std::vector<ZoneHVAC*>& getZoneList() const { return zones; }

    // -------------------------
    //   PUBLIC HVAC INTERNALS
    // -------------------------
    void controlLoop(uint32_t now);
    void handleACS(uint32_t now, const HVACTime& t);

    class Diagnostic {
    public:
        static void Report(const HeatPumpController& hp);
    };

private:
    void heatingControl(uint32_t now);
    void coolingControl(uint32_t now);

    void evaluateZoneRequests(bool& heatReq, bool& coolReq);
    void updateFanCoils(Mode effectiveMode);

    void updateFanSpeed(Mode effectiveMode);
    void setFanSpeed(FanSpeed v);

    bool minimumDelayElapsed(uint32_t now) const;
    bool canTurnOn(uint32_t now) const;

    void activateCompressor(uint32_t now);
    void deactivateCompressor(uint32_t now);
    void forceCompressorOff();

    void startDefrost(uint32_t now);
    void handleDefrost(uint32_t now);

    void handleCirculationPump(uint32_t now);
    void activateCirculationPump(uint32_t now);
    void deactivateCirculationPump(uint32_t now);

    void handleNormalDhw(uint32_t now, float dhwTemp);
    void handleAntiLegionella(uint32_t now, const HVACTime& t, float dhwTemp);

private:
    Mode  mode;
    float setpoint;

    float indoorTemperature;
    float outdoorTemperature;

    bool     compressorActive;
    FanSpeed fanSpeed;

    bool safetyActive;

    unsigned long lastStateChangeTime;
    unsigned long compressorOnTime;

    unsigned long defrostStartTime;

    bool          windowOpen = false;
    unsigned long windowOpenStartTime = 0;

    bool          circulationActive = false;
    unsigned long lastCirculationChangeTime = 0;

    std::vector<ZoneHVAC*> zones;

    std::function<void(bool)>               compressorCallback;
    std::function<void(String,int,bool)>    fanCoilCallback;
    std::function<void(bool)>               circulationCallback;
    std::function<void(HydraulicTarget)>    threeWayValveCallback;

    HydraulicTarget hydraulicTarget = HydraulicTarget::SPACE_HEATING;

    bool acsEnabled = false;
    bool acsPriority = true;

    float dhwSetpoint = 50.0f;

    bool antiLegionellaEnabled = false;
    uint8_t antiLegionellaDay = 0;
    uint8_t antiLegionellaHour = 3;
    float antiLegionellaSetpoint = 60.0f;
    unsigned long antiLegionellaMaxDurationMs = 90UL * 60UL * 1000UL;

    unsigned long antiLegionellaStart = 0;
    bool antiLegionellaActive = false;

    std::function<float()> readDhwTemp;

    Config cfg;
};

// ============================================================================
//   DMHVAC WRAPPER (alto livello) — usato da DomoManager e Task_HVAC
// ============================================================================

class DMHVAC {
public:

    // ----------------------------------------------------
    //   CONFIGURAZIONE HVAC (PUBLIC)
    // ----------------------------------------------------
    struct HVACConfig {
        bool enabled = false;
        uint32_t intervalMs = 10000;

        struct Zone {
            const char* name;
            float setpoint;
            int temperatureArea;
            int fanCoilId;
        };

        Zone* zones = nullptr;
        size_t zoneCount = 0;

        std::function<float()> readIndoorTemp;
        std::function<float()> readOutdoorTemp;
        std::function<bool()>  readWindowOpen;

        HeatPumpController::Mode initialMode = HeatPumpController::Mode::OFF;
        float initialSetpoint = 20.0f;
        HeatPumpController::Config hpConfig;

        struct ACSConfig {
            bool enabled = false;

            Valve* threeWayValve = nullptr;

            bool dhwPriority = true;
            float dhwSetpoint = 50.0f;

            bool antiLegionellaEnabled = false;
            uint8_t antiLegionellaDay = 0;
            uint8_t antiLegionellaHour = 3;
            float antiLegionellaSetpoint = 60.0f;
            uint32_t antiLegionellaMaxDurationMs = 90UL * 60UL * 1000UL;

            std::function<float()> readDhwTemp;
        } acs;
    };

    // ----------------------------------------------------
    //   COSTRUTTORE
    // ----------------------------------------------------
    DMHVAC() : hp(HeatPumpController::Mode::OFF, 20.0f, defaultCfg) {}

    // ----------------------------------------------------
    //   SETUP DA FRONTEND
    // ----------------------------------------------------
    void setup(const HVACConfig& cfg) {

        config = cfg;

        hp = HeatPumpController(cfg.initialMode, cfg.initialSetpoint, cfg.hpConfig);

        for (size_t i = 0; i < cfg.zoneCount; i++)
            hp.createZone(cfg.zones[i].name, cfg.zones[i].setpoint, cfg.zones[i].fanCoilId);

        hp.enableACS(cfg.acs.enabled);
        hp.setACSPriority(cfg.acs.dhwPriority);
        hp.setDhwSetpoint(cfg.acs.dhwSetpoint);
        hp.setDhwTempReader(cfg.acs.readDhwTemp);

        hp.enableAntiLegionella(cfg.acs.antiLegionellaEnabled);
        hp.setAntiLegionellaSchedule(cfg.acs.antiLegionellaDay, cfg.acs.antiLegionellaHour);
        hp.setAntiLegionellaParams(cfg.acs.antiLegionellaSetpoint,
                                   cfg.acs.antiLegionellaMaxDurationMs);

        if (cfg.acs.threeWayValve) {
            Valve* valve = cfg.acs.threeWayValve;

            hp.setThreeWayValveCallback([valve](HeatPumpController::HydraulicTarget t){
                unsigned long now = millis();
                if (t == HeatPumpController::HydraulicTarget::DHW)
                    valve->commandOpen(now);
                else
                    valve->commandClose(now);
            });
        }
    }

    // ----------------------------------------------------
    //   LOOP PRINCIPALE
    // ----------------------------------------------------
    void loop(
        uint32_t now,
        const HeatPumpController::HVACTime& t,
        float* zoneTemps,
        float indoor,
        float outdoor,
        bool windowOpen
    ) {
        auto& zones = hp.getZoneList();
        for (size_t i = 0; i < zones.size(); i++)
            zones[i]->updateTemperature(zoneTemps[i]);

        hp.updateIndoorTemperature(indoor);
        hp.updateOutdoorTemperature(outdoor);
        hp.setWindowOpen(windowOpen, now);

        hp.handleACS(now, t);
        hp.controlLoop(now);
    }

    HVACConfig& getConfig() { return config; }
    const HVACConfig& getConfig() const { return config; }

private:
    HVACConfig config;
    HeatPumpController hp;
    HeatPumpController::Config defaultCfg;
public:
    HeatPumpController& getHP() { return hp; }
    const HeatPumpController& getHP() const { return hp; }
};

#endif
