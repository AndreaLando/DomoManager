#ifndef DMWeather_HPP
#define DMWeather_HPP

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

 /*===============================================================================
DMWeather — Sensor fusion, filtering, thresholds and event generation
===============================================================================
This module provides a complete weather‑processing engine for embedded systems:
- Multi‑sensor acquisition: temperature, wind, rain, light.
- Sliding‑window filtering: O(1) moving averages using incremental sums.
- Scaled readings: configurable ADC factors for each sensor type.
- Day/Night detection: hysteresis‑based light thresholds with event callbacks.
- Rain detection: start/stop logic with independent debounce counters.
- Wind gust detection: delta‑based gust recognition with debounce.
- Alarm engine: TempLow, TempHigh, WindHigh, RainHigh with per‑alarm debounce.
- Event callbacks: RainStart, RainStop, WindGustStart, WindGustEnd,
                   DayStart, NightStart.
- Diagnostic reporter: prints filtered values, states and thresholds.

Designed for:
- Zero recurring allocations
- Deterministic timing
- Non‑blocking update loop
- Reliable environmental state tracking on Opta‑class hardware
=============================================================================== */

#include <Arduino.h>

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

enum class WeatherEvent {
    RainStart,
    RainStop,
    WindGustStart,
    WindGustEnd,
    DayStart,
    NightStart
};

enum class WeatherAlarm {
    TempLow,
    TempHigh,
    WindHigh,
    RainHigh
};

class WeatherStation {
public:
    struct CommonConfig {
        // Sensori
        std::function<int()> readTemp;
        std::function<int()> readWind;
        std::function<int()> readRain;
        std::function<int()> readLight;

        // Scale
        float tempFactor;
        float windFactor;
        float rainFactor;
        float lightFactor;

        // Soglie allarmi
        float lowTempThreshold;
        float highTempThreshold;
        float highWindThreshold;
        float highRainThreshold;

        // Raffiche vento (soglia)
        float windGustDelta;

        // Debounce generale
        int alarmDebounce;
    };

    struct Config {
        CommonConfig common;

        // Giorno / notte
        float lightDayThreshold;
        float lightNightThreshold;

        // Pioggia
        float rainStartThreshold;
        float rainStopThreshold;
        int rainStartDebounce;
        int rainStopDebounce;

        // Raffiche vento (debounce)
        int gustDebounce;
    };

private:
    friend class Diagnostic;
    static inline Config cfg;

    /* ============================================================
       Buffer circolari per media mobile
       ============================================================ */
    static const int WINDOW = 10;
    int tempBuf[WINDOW];
    int windBuf[WINDOW];
    int rainBuf[WINDOW];
    int lightBuf[WINDOW];

    int index = 0;
    bool filled = false;

    /* ============================================================
       somme incrementali per media O(1)
       ============================================================ */
    long tempSum = 0;   // Somma corrente dei 10 valori temperatura
    long windSum = 0;   // Somma corrente dei 10 valori vento
    long rainSum = 0;   // Somma corrente dei 10 valori pioggia
    long lightSum = 0;  // Somma corrente dei 10 valori luce

    /* ============================================================
       Debounce allarmi
       ============================================================ */
    int debounceThreshold = 3;

    int debounceCountTempLow  = 0;
    int debounceCountTempHigh = 0;
    int debounceCountWindHigh = 0;
    int debounceCountRainHigh = 0;

    bool prevTempLow  = false;
    bool prevTempHigh = false;
    bool prevWindHigh = false;
    bool prevRainHigh = false;

    /* ============================================================
       Giorno / Notte con isteresi
       ============================================================ */
    
    bool isCurrentlyDay = true;
    bool prevDayState = true;

    /* ============================================================
       Pioggia inizio/fine
       ============================================================ */
    bool isRaining = false;
    
    int rainStartCounter = 0;
    int rainStopCounter  = 0;

    /* ============================================================
       Raffiche di vento
       ============================================================ */
    bool isGust = false;
    int gustCounter = 0;
    float lastWindValue = 0.0;

    /* ============================================================
       Callback
       ============================================================ */
    void (*eventCallback)(WeatherEvent) = nullptr;
    void (*alarmCallback)(const WeatherAlarm *, int) = nullptr;

    /* ============================================================
       ⭐ INLINE: debounce ottimizzato
       ============================================================ */
    inline bool debounceAlarm(bool condition, int &counter) {
        if (condition) {
            counter++;
            if (counter >= cfg.common.alarmDebounce) {
                counter = cfg.common.alarmDebounce;
                return true;
            }
        } else {
            counter = 0;
        }
        return false;
    }

    /* ============================================================
       checkAlarms usa valori filtrati una sola volta
       ============================================================ */
    void checkAlarms() {
        float t = getTemperature();
        float w = getWind();
        float r = getRain();

        bool tLow  = debounceAlarm(t <= cfg.common.lowTempThreshold,  debounceCountTempLow);
        bool tHigh = debounceAlarm(t >= cfg.common.highTempThreshold, debounceCountTempHigh);
        bool wHigh = debounceAlarm(w >= cfg.common.highWindThreshold, debounceCountWindHigh);
        bool rHigh = debounceAlarm(r >= cfg.common.highRainThreshold, debounceCountRainHigh);

        bool changed =
            (tLow  != prevTempLow)  ||
            (tHigh != prevTempHigh) ||
            (wHigh != prevWindHigh) ||
            (rHigh != prevRainHigh);

        if (changed && alarmCallback) {
            WeatherAlarm active[4];
            int count = 0;

            if (tLow)  active[count++] = WeatherAlarm::TempLow;
            if (tHigh) active[count++] = WeatherAlarm::TempHigh;
            if (wHigh) active[count++] = WeatherAlarm::WindHigh;
            if (rHigh) active[count++] = WeatherAlarm::RainHigh;

            alarmCallback(active, count);
        }

        prevTempLow  = tLow;
        prevTempHigh = tHigh;
        prevWindHigh = wHigh;
        prevRainHigh = rHigh;
    }

    /* ============================================================
       (Altre funzioni private invariate)
       ============================================================ */
    void updateDayNightState() {
        float light = getLight();

        if (light >= cfg.lightDayThreshold)
            isCurrentlyDay = true;
        else if (light <= cfg.lightNightThreshold)
            isCurrentlyDay = false;
    }

    void checkDayNightEvent() {
        if (isCurrentlyDay != prevDayState) {
            if (eventCallback) {
                eventCallback(isCurrentlyDay ? WeatherEvent::DayStart
                                             : WeatherEvent::NightStart);
            }
        }
        prevDayState = isCurrentlyDay;
    }

    /* ============================================================
   Rilevamento pioggia inizio/fine
   ============================================================ */
    void checkRainEvents() {
        float rain = getRain();

        if (!isRaining) {
            if (rain >= cfg.rainStartThreshold) {
                rainStartCounter++;
                if (rainStartCounter >= cfg.rainStartDebounce) {
                    isRaining = true;
                    rainStartCounter = 0;
                    if (eventCallback) eventCallback(WeatherEvent::RainStart);
                }
            } else {
                rainStartCounter = 0;
            }
        } else {
            if (rain <= cfg.rainStopThreshold) {
                rainStopCounter++;
                if (rainStopCounter >= cfg.rainStopDebounce) {
                    isRaining = false;
                    rainStopCounter = 0;
                    if (eventCallback) eventCallback(WeatherEvent::RainStop);
                }
            } else {
                rainStopCounter = 0;
            }
        }
    }

    /* ============================================================
   Rilevamento raffiche di vento
   ============================================================ */
    void checkWindGust() {
        float wind = getWind();
        float delta = wind - lastWindValue;

        if (!isGust) {
            if (delta >= cfg.common.windGustDelta) {
                gustCounter++;
                if (gustCounter >= cfg.gustDebounce) {
                    isGust = true;
                    gustCounter = 0;
                    if (eventCallback) eventCallback(WeatherEvent::WindGustStart);
                }
            } else {
                gustCounter = 0;
            }
        } else {
            if (delta <= 0) {
                gustCounter++;
                if (gustCounter >= cfg.gustDebounce) {
                    isGust = false;
                    gustCounter = 0;
                    if (eventCallback) eventCallback(WeatherEvent::WindGustEnd);
                }
            } else {
                gustCounter = 0;
            }
        }

        lastWindValue = wind;
    }

public:

    WeatherStation(const Config& config) {
        cfg = config;

        for (int i = 0; i < WINDOW; i++) {
            tempBuf[i] = windBuf[i] = rainBuf[i] = lightBuf[i] = 0;
        }
    }

    void setEventCallback(void (*cb)(WeatherEvent)) {
        eventCallback = cb;
    }

    void setAlarmCallback(void (*cb)(const WeatherAlarm *, int)) {
        alarmCallback = cb;
    }

    /* ============================================================
       ⭐ MODIFICATO: getX() usa somma incrementale 
       ============================================================ */
    float getTemperature() const {
        int count = filled ? WINDOW : index;
        return (tempSum / (float)count) * cfg.common.tempFactor;
    }

    float getWind() const {
        int count = filled ? WINDOW : index;
        return (windSum / (float)count) * cfg.common.windFactor;
    }

    float getRain() const {
        int count = filled ? WINDOW : index;
        return (rainSum / (float)count) * cfg.common.rainFactor;
    }

    float getLight() const {
        int count = filled ? WINDOW : index;
        return (lightSum / (float)count) * cfg.common.lightFactor;
    }


    bool isDay() const { return isCurrentlyDay; }
    bool isRainingNow() const { return isRaining; }
    bool isGustActive() const { return isGust; }

    /* ============================================================
       ⭐ MODIFICATO: update() con media O(1)
       ============================================================ */
    
    void update() {

        // Rimuovi vecchi valori
        tempSum  -= tempBuf[index];
        windSum  -= windBuf[index];
        rainSum  -= rainBuf[index];
        lightSum -= lightBuf[index];

        // --- LETTURE GREZZE ---
        int t = cfg.common.readTemp();
        int w = cfg.common.readWind();
        int r = cfg.common.readRain();
        int l = cfg.common.readLight();
        
        // Inserimento nei buffer
        tempBuf[index]  = t;
        windBuf[index]  = w;
        rainBuf[index]  = r;
        lightBuf[index] = l;

        // Aggiorna somme
        tempSum  += t;
        windSum  += w;
        rainSum  += r;
        lightSum += l;

        // Avanza indice
        index++;
        if (index >= WINDOW) {
            index = 0;
            filled = true;
        }

        // --- VALORI FILTRATI ---
        float Tf = getTemperature();
        float Wf = getWind();
        float Rf = getRain();
        float Lf = getLight();

        // --- LOGICA METEO ---
        LOG_D("METEO-UPD", "updateDayNightState()");
        updateDayNightState();

        LOG_D("METEO-UPD", "checkRainEvents()");
        checkRainEvents();

        LOG_D("METEO-UPD", "checkWindGust()");
        checkWindGust();

        LOG_D("METEO-UPD", "checkDayNightEvent()");
        checkDayNightEvent();

        LOG_D("METEO-UPD", "checkAlarms()");
        checkAlarms();
    }

    class Diagnostic {
    public:
        static void Report(const WeatherStation& ws) {

            Serial.println("\n===== WEATHER STATION =====");

            // Letture filtrate
            Serial.print("Temperature: ");
            Serial.print(ws.getTemperature());
            Serial.println(" °C");

            Serial.print("Wind: ");
            Serial.print(ws.getWind());
            Serial.println(" m/s");

            Serial.print("Rain: ");
            Serial.print(ws.getRain());
            Serial.println(" mm/h");

            Serial.print("Light: ");
            Serial.print(ws.getLight());
            Serial.println(" lux");

            // Stati logici
            Serial.print("Day/Night: ");
            Serial.println(ws.isDay() ? "DAY" : "NIGHT");

            Serial.print("Raining: ");
            Serial.println(ws.isRainingNow() ? "YES" : "NO");

            Serial.print("Wind Gust: ");
            Serial.println(ws.isGustActive() ? "ACTIVE" : "NO");

            // Soglie
            Serial.println("\nThresholds:");
            Serial.print(" - Temp Low: "); Serial.println(cfg.common.lowTempThreshold);
            Serial.print(" - Temp High: "); Serial.println(cfg.common.highTempThreshold);
            Serial.print(" - Wind High: "); Serial.println(cfg.common.highWindThreshold);
            Serial.print(" - Rain High: "); Serial.println(cfg.common.highRainThreshold);

            Serial.print(" - Day Threshold: "); Serial.println(cfg.lightDayThreshold);
            Serial.print(" - Night Threshold: "); Serial.println(cfg.lightNightThreshold);

            Serial.println("===== END WEATHER STATION =====\n");
        }
    };

};

#endif
