#include "DMHVAC.h"

// =====================================================
//               STRING HELPERS
// =====================================================
static const char* modeToString(HeatPumpController::Mode m) {
    switch (m) {
        case HeatPumpController::Mode::OFF:     return "OFF";
        case HeatPumpController::Mode::MANUAL:  return "MANUAL";
        case HeatPumpController::Mode::HEATING: return "HEATING";
        case HeatPumpController::Mode::COOLING: return "COOLING";
        case HeatPumpController::Mode::AUTO:    return "AUTO";
        case HeatPumpController::Mode::DEFROST: return "DEFROST";
    }
    return "UNKNOWN";
}

static const char* fanSpeedToString(HeatPumpController::FanSpeed v) {
    switch (v) {
        case HeatPumpController::FanSpeed::LOW:    return "LOW";
        case HeatPumpController::FanSpeed::MEDIUM: return "MEDIUM";
        case HeatPumpController::FanSpeed::HIGH:   return "HIGH";
    }
    return "UNKNOWN";
}

// =====================================================
//               CONSTRUCTOR
// =====================================================
HeatPumpController::HeatPumpController(Mode m, float sp, const Config& c)
    : mode(m),
      setpoint(sp),
      indoorTemperature(20.0f),
      outdoorTemperature(10.0f),
      compressorActive(false),
      fanSpeed(FanSpeed::LOW),
      safetyActive(false),
      lastStateChangeTime(0),
      compressorOnTime(0),
      defrostStartTime(0),
      cfg(c)
{}

// =====================================================
//               BASIC SETTERS
// =====================================================
void HeatPumpController::setMode(Mode m) {
    mode = m;
}

void HeatPumpController::setSetpoint(float sp) {
    setpoint = sp;
}

void HeatPumpController::updateIndoorTemperature(float t) {
    indoorTemperature = t;
    controlLoop(millis());
}

void HeatPumpController::updateOutdoorTemperature(float t) {
    outdoorTemperature = t;
    controlLoop(millis());
}

void HeatPumpController::setWindowOpen(bool state, uint32_t now) {
    if (state && !windowOpen)
        windowOpenStartTime = now;
    windowOpen = state;
}

// =====================================================
//               ZONE MANAGEMENT
// =====================================================
HeatPumpController::ZoneHVAC* HeatPumpController::createZone(
    const String& name, float sp, int fanCoilId)
{
    zones.emplace_back(new ZoneHVAC(name, sp, fanCoilId, cfg));
    return zones.back();
}

void HeatPumpController::updateZone(const String& name, float temperature) {
    for (auto* z : zones) {
        if (z->getName() == name) {
            z->updateTemperature(temperature);
            return;
        }
    }
}

// =====================================================
//               MAIN CONTROL LOOP
// =====================================================
void HeatPumpController::controlLoop(uint32_t now) {

    // ---------------------------
    // Outdoor temperature safety
    // ---------------------------
    if (outdoorTemperature < cfg.minOutdoorTemp ||
        outdoorTemperature > cfg.maxOutdoorTemp)
    {
        safetyActive = true;
        forceCompressorOff();
        activateCirculationPump(now);
        return;
    }

    if (safetyActive &&
        outdoorTemperature > cfg.minOutdoorTemp + 1 &&
        outdoorTemperature < cfg.maxOutdoorTemp - 1)
    {
        safetyActive = false;
    }

    // ---------------------------
    // Window open logic
    // ---------------------------
    if (windowOpen) {
        if (now - windowOpenStartTime > cfg.windowOpenTimeoutMs) {
            forceCompressorOff();
            setFanSpeed(FanSpeed::LOW);
            activateCirculationPump(now);
            return;
        }
    }

    // ---------------------------
    // OFF mode
    // ---------------------------
    if (mode == Mode::OFF) {
        forceCompressorOff();
        setFanSpeed(FanSpeed::LOW);
        deactivateCirculationPump(now);
        return;
    }

    // ---------------------------
    // MANUAL mode
    // ---------------------------
    if (mode == Mode::MANUAL) {
        activateCompressor(now);
        setFanSpeed(FanSpeed::HIGH);
        activateCirculationPump(now);
        return;
    }

    // ---------------------------
    // DEFROST
    // ---------------------------
    if (mode != Mode::DEFROST && outdoorTemperature < cfg.defrostThreshold) {
        startDefrost(now);
    }

    if (mode == Mode::DEFROST) {
        handleDefrost(now);
        handleCirculationPump(now);
        return;
    }

    // ---------------------------
    // AUTO mode → evaluate zones
    // ---------------------------
    Mode effectiveMode = mode;

    bool heatReq = false;
    bool coolReq = false;
    evaluateZoneRequests(heatReq, coolReq);

    if (mode == Mode::AUTO) {
        if (heatReq)  effectiveMode = Mode::HEATING;
        if (coolReq)  effectiveMode = Mode::COOLING;
    }

    // ---------------------------
    // Max ON time protection
    // ---------------------------
    if (compressorActive &&
        (now - compressorOnTime > cfg.maxOnTimeMs))
    {
        deactivateCompressor(now);
    }

    // ---------------------------
    // Heating / Cooling logic
    // ---------------------------
    if (effectiveMode == Mode::HEATING)
        heatingControl(now);
    else if (effectiveMode == Mode::COOLING)
        coolingControl(now);

    // ---------------------------
    // Fan speed
    // ---------------------------
    updateFanSpeed(effectiveMode);

    // ---------------------------
    // Fan coils
    // ---------------------------
    updateFanCoils(effectiveMode);

    // ---------------------------
    // Circulation pump
    // ---------------------------
    handleCirculationPump(now);
}

// =====================================================
//               HEATING / COOLING
// =====================================================
void HeatPumpController::heatingControl(uint32_t now) {
    if (indoorTemperature < setpoint - cfg.hysteresis)
        activateCompressor(now);
    else if (indoorTemperature > setpoint + cfg.hysteresis)
        deactivateCompressor(now);
}

void HeatPumpController::coolingControl(uint32_t now) {
    if (indoorTemperature > setpoint + cfg.hysteresis)
        activateCompressor(now);
    else if (indoorTemperature < setpoint - cfg.hysteresis)
        deactivateCompressor(now);
}

// =====================================================
//               ZONE REQUESTS
// =====================================================
void HeatPumpController::evaluateZoneRequests(bool &heatReq, bool &coolReq) {
    heatReq = false;
    coolReq = false;

    for (ZoneHVAC* z : zones) {
        if (z->needsHeating())  heatReq = true;
        if (z->needsCooling())  coolReq = true;
    }
}

void HeatPumpController::updateFanCoils(Mode effectiveMode) {
    for (ZoneHVAC* z : zones) {
        bool state = false;

        if (effectiveMode == Mode::HEATING && z->needsHeating())
            state = true;
        else if (effectiveMode == Mode::COOLING && z->needsCooling())
            state = true;

        if (fanCoilCallback && state != z->previousFanCoilState)
            fanCoilCallback(z->getName(), z->getFanCoilNumber(), state);

        z->previousFanCoilState = state;
    }
}

// =====================================================
//               FAN SPEED
// =====================================================
void HeatPumpController::updateFanSpeed(Mode effectiveMode) {
    float diff = 0.0f;

    if (effectiveMode == Mode::HEATING)
        diff = setpoint - indoorTemperature;
    else if (effectiveMode == Mode::COOLING)
        diff = indoorTemperature - setpoint;

    FanSpeed newSpeed = fanSpeed;

    if (diff < cfg.diffLow - cfg.fanHysteresis)
        newSpeed = FanSpeed::LOW;
    else if (diff < cfg.diffMed - cfg.fanHysteresis)
        newSpeed = FanSpeed::MEDIUM;
    else if (diff > cfg.diffMed + cfg.fanHysteresis)
        newSpeed = FanSpeed::HIGH;

    setFanSpeed(newSpeed);
}

void HeatPumpController::setFanSpeed(FanSpeed v) {
    if (fanSpeed != v)
        fanSpeed = v;
}

// =====================================================
//               COMPRESSOR
// =====================================================
bool HeatPumpController::minimumDelayElapsed(uint32_t now) const {
    return (now - lastStateChangeTime) > cfg.minSwitchDelayMs;
}

bool HeatPumpController::canTurnOn(uint32_t now) const {
    return (now - lastStateChangeTime) > cfg.minOffTimeMs;
}

void HeatPumpController::activateCompressor(uint32_t now) {
    if (!compressorActive && minimumDelayElapsed(now) && canTurnOn(now)) {
        compressorActive = true;
        lastStateChangeTime = now;
        compressorOnTime = now;
        if (compressorCallback) compressorCallback(true);
    }
}

void HeatPumpController::deactivateCompressor(uint32_t now) {
    if (compressorActive && minimumDelayElapsed(now)) {
        compressorActive = false;
        lastStateChangeTime = now;
        if (compressorCallback) compressorCallback(false);
    }
}

void HeatPumpController::forceCompressorOff() {
    if (compressorActive) {
        compressorActive = false;
        if (compressorCallback) compressorCallback(false);
    }
}

// =====================================================
//               DEFROST
// =====================================================
void HeatPumpController::startDefrost(uint32_t now) {
    mode = Mode::DEFROST;
    defrostStartTime = now;
    forceCompressorOff();
    setFanSpeed(FanSpeed::MEDIUM);
    activateCirculationPump(now);
}

void HeatPumpController::handleDefrost(uint32_t now) {
    if (now - defrostStartTime > cfg.defrostDurationMs) {
        mode = Mode::AUTO;
        return;
    }
    forceCompressorOff();
    setFanSpeed(FanSpeed::MEDIUM);
    activateCirculationPump(now);
}

// =====================================================
//               CIRCULATION PUMP
// =====================================================
void HeatPumpController::handleCirculationPump(uint32_t now) {

    if (compressorActive) {
        activateCirculationPump(now);
        return;
    }

    if (!compressorActive &&
        now - lastStateChangeTime < cfg.postCirculationMs)
    {
        activateCirculationPump(now);
        return;
    }

    deactivateCirculationPump(now);
}

void HeatPumpController::activateCirculationPump(uint32_t now) {
    if (!circulationActive &&
        now - lastCirculationChangeTime > cfg.minCirculationCycleMs)
    {
        circulationActive = true;
        lastCirculationChangeTime = now;
        if (circulationCallback) circulationCallback(true);
    }
}

void HeatPumpController::deactivateCirculationPump(uint32_t now) {
    if (circulationActive &&
        now - lastCirculationChangeTime > cfg.minCirculationCycleMs)
    {
        circulationActive = false;
        lastCirculationChangeTime = now;
        if (circulationCallback) circulationCallback(false);
    }
}

// =====================================================
//               ACS LOGIC
// =====================================================
void HeatPumpController::handleACS(uint32_t now, const HVACTime& t) {

    float dhwTemp = readDhwTemp ? readDhwTemp() : 0.0f;

    // Anti-legionella first
    if (antiLegionellaEnabled)
        handleAntiLegionella(now, t, dhwTemp);

    // Normal DHW
    if (!antiLegionellaActive)
        handleNormalDhw(now, dhwTemp);
}

void HeatPumpController::handleNormalDhw(uint32_t now, float dhwTemp) {

    bool needDhw = (dhwTemp < dhwSetpoint - 2.0f);

    if (needDhw) {
        selectHydraulicTarget(HydraulicTarget::DHW);
        setMode(Mode::HEATING);

        if (acsPriority) {
            // eventuale logica per bloccare zone
        }
    } else {
        selectHydraulicTarget(HydraulicTarget::SPACE_HEATING);
    }
}

void HeatPumpController::handleAntiLegionella(
    uint32_t now, const HVACTime& t, float dhwTemp)
{
    if (!antiLegionellaActive) {
        bool rightDay  = (t.dayOfWeek == antiLegionellaDay);
        bool rightHour = (t.hour == antiLegionellaHour);
        bool rightMin  = (t.minute == 0);

        if (rightDay && rightHour && rightMin) {
            antiLegionellaActive = true;
            antiLegionellaStart = now;
        }
    }

    if (antiLegionellaActive) {

        selectHydraulicTarget(HydraulicTarget::DHW);
        setMode(Mode::HEATING);

        if (dhwTemp >= antiLegionellaSetpoint ||
            (now - antiLegionellaStart) > antiLegionellaMaxDurationMs)
        {
            antiLegionellaActive = false;
            selectHydraulicTarget(HydraulicTarget::SPACE_HEATING);
        }
    }
}

// =====================================================
//               GETTERS
// =====================================================
bool HeatPumpController::isCompressorActive() const { return compressorActive; }
HeatPumpController::FanSpeed HeatPumpController::getFanSpeed() const { return fanSpeed; }
HeatPumpController::Mode HeatPumpController::getMode() const { return mode; }
float HeatPumpController::getSetpoint() const { return setpoint; }
float HeatPumpController::getIndoorTemperature() const { return indoorTemperature; }
float HeatPumpController::getOutdoorTemperature() const { return outdoorTemperature; }

// =====================================================
//               DIAGNOSTIC
// =====================================================
void HeatPumpController::Diagnostic::Report(const HeatPumpController& hp) {

    Serial.println("\n===== HVAC DIAGNOSTIC =====");

    Serial.print("Mode: ");
    Serial.println(modeToString(hp.getMode()));

    Serial.print("Indoor Temp: ");
    Serial.print(hp.getIndoorTemperature());
    Serial.println(" °C");

    Serial.print("Outdoor Temp: ");
    Serial.print(hp.getOutdoorTemperature());
    Serial.println(" °C");

    Serial.print("Setpoint: ");
    Serial.print(hp.getSetpoint());
    Serial.println(" °C");

    Serial.print("Compressor: ");
    Serial.println(hp.isCompressorActive() ? "ON" : "OFF");

    Serial.print("Fan: ");
    Serial.println(fanSpeedToString(hp.getFanSpeed()));

    Serial.print("Hydraulic Target: ");
    Serial.println(
        hp.getHydraulicTarget() == HeatPumpController::HydraulicTarget::DHW ?
        "DHW" : "SPACE_HEATING"
    );

    Serial.println("\nZones:");
    for (auto* z : hp.getZoneList()) {
        Serial.print(" - ");
        Serial.print(z->getName());
        Serial.print(" | T=");
        Serial.print(z->getTemperature());
        Serial.print("°C | SP=");
        Serial.print(z->getSetpoint());
        Serial.print("°C | ");

        if (z->needsHeating())      Serial.print("HEAT");
        else if (z->needsCooling()) Serial.print("COOL");
        else                        Serial.print("NONE");

        Serial.print(" | FanCoil=");
        Serial.println(z->previousFanCoilState ? "ON" : "OFF");
    }

    Serial.println("===== END HVAC DIAGNOSTIC =====\n");
}