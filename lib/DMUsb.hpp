#ifndef DMUsb_HPP
#define DMUsb_HPP

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

#include <Arduino_UnifiedStorage.h>
#include "DMDeclares.h"

class USBConfigParser {
public:

    // ---------------------------------------------------------
    // WEATHER
    // ---------------------------------------------------------
    static void parseWeather(JsonObject root, FrontendConfig& cfg) {
        JsonObject w = root["weather"].as<JsonObject>();
        if (w.isNull()) return;

        cfg.weather.enabled     = w["enabled"]     | cfg.weather.enabled;
        cfg.weather.intervalMs  = w["intervalMs"]  | cfg.weather.intervalMs;
        cfg.weather.autoCloseWindows = w["autoCloseWindows"] | cfg.weather.autoCloseWindows;

        JsonObject t = w["thresholds"].as<JsonObject>();
        if (!t.isNull()) {
            cfg.weather.config.lightDayThreshold   = t["lightDay"]   | cfg.weather.config.lightDayThreshold;
            cfg.weather.config.lightNightThreshold = t["lightNight"] | cfg.weather.config.lightNightThreshold;
            cfg.weather.config.rainStartThreshold  = t["rainStart"]  | cfg.weather.config.rainStartThreshold;
            cfg.weather.config.rainStopThreshold   = t["rainStop"]   | cfg.weather.config.rainStopThreshold;
            cfg.weather.config.gustDebounce        = t["gustDebounce"] | cfg.weather.config.gustDebounce;
            cfg.weather.config.rainStartDebounce   = t["rainStartDebounce"] | cfg.weather.config.rainStartDebounce;
            cfg.weather.config.rainStopDebounce    = t["rainStopDebounce"] | cfg.weather.config.rainStopDebounce;
        }
    }

    // ---------------------------------------------------------
    // POWER
    // ---------------------------------------------------------
    static void parsePower(JsonObject root, FrontendConfig& cfg) {
        JsonObject p = root["power"].as<JsonObject>();
        if (p.isNull()) return;

        cfg.power.enabled     = p["enabled"]     | cfg.power.enabled;
        cfg.power.intervalMs  = p["intervalMs"]  | cfg.power.intervalMs;
        cfg.power.limitSoft   = p["limitSoft"]   | cfg.power.limitSoft;
        cfg.power.limitHard   = p["limitHard"]   | cfg.power.limitHard;
        cfg.power.autoTune    = p["autoTune"]    | cfg.power.autoTune;
        cfg.power.tuneIntervalMs = p["tuneIntervalMs"] | cfg.power.tuneIntervalMs;
    }

    // ---------------------------------------------------------
    // HVAC
    // ---------------------------------------------------------
    static void parseHVAC(JsonObject root, FrontendConfig& cfg) {
        JsonObject h = root["hvac"].as<JsonObject>();
        if (h.isNull()) return;

        cfg.hvac.enabled     = h["enabled"]     | cfg.hvac.enabled;
        cfg.hvac.intervalMs  = h["intervalMs"]  | cfg.hvac.intervalMs;
    }

    // ---------------------------------------------------------
    // SECURITY
    // ---------------------------------------------------------
    static void parseSecurity(JsonObject root, FrontendConfig& cfg) {
        JsonObject s = root["security"].as<JsonObject>();
        if (s.isNull()) return;

        cfg.security.enabled     = s["enabled"]     | cfg.security.enabled;
        cfg.security.intervalMs  = s["intervalMs"]  | cfg.security.intervalMs;
    }

    // ---------------------------------------------------------
    // MEDIE
    // ---------------------------------------------------------
    static void parseMedie(JsonObject root, FrontendConfig& cfg) {
        JsonObject m = root["medie"].as<JsonObject>();
        if (m.isNull()) return;

        cfg.averages.enabled     = m["enabled"]     | cfg.averages.enabled;
        cfg.averages.intervalMs  = m["intervalMs"]  | cfg.averages.intervalMs;
    }

    // ---------------------------------------------------------
    // WATCH
    // ---------------------------------------------------------
    static void parseWatch(JsonObject root, FrontendConfig& cfg) {
        JsonObject w = root["watch"].as<JsonObject>();
        if (w.isNull()) return;

        cfg.watch.enabled = w["enabled"] | cfg.watch.enabled;
        cfg.watch.count   = w["count"]   | cfg.watch.count;
    }
};

class USBConfigLoader {
public:

    static void Init(void (*onLoaded)(bool)) {
        if (onLoaded) onLoaded(true);
    }

    // ---------------------------------------------------------
    // LOAD FROM USB: /config.json
    // ---------------------------------------------------------
    static bool LoadFromUSB(FrontendConfig& cfg) {
        USBStorage usb;
        if (!usb.begin()) {
            LOG_EF("USB", "USB not mounted");
            return false;
        }

        Folder root = usb.getRootFolder();

        // createFile() apre il file se esiste, lo crea se non esiste
        UFile f = root.createFile("config.json", FileMode::READ);

        // Test di validità: proviamo a leggere 1 byte
        uint8_t test;
        int n = f.read(&test, 1);

        if (n <= 0) {
            LOG_EF("USB", "config.json empty or unreadable");
            f.close();
            usb.unmount();
            return false;
        }

        // Riavvolgi il file
        f.seek(0);

        StaticJsonDocument<8192> doc;
        auto err = deserializeJson(doc, f);
        f.close();
        usb.unmount();

        if (err) {
            LOG_EF("USB", "JSON parse error");
            return false;
        }

        JsonObject rootObj = doc.as<JsonObject>();

        USBConfigParser::parseWeather(rootObj, cfg);
        USBConfigParser::parsePower(rootObj, cfg);
        USBConfigParser::parseHVAC(rootObj, cfg);
        USBConfigParser::parseSecurity(rootObj, cfg);
        USBConfigParser::parseMedie(rootObj, cfg);
        USBConfigParser::parseWatch(rootObj, cfg);

        LOG_IF("USB", "Config loaded from USB");
        return true;
    }

    // ---------------------------------------------------------
    // SAVE TO USB: /config.json
    // ---------------------------------------------------------
    static bool SaveToUSB(const FrontendConfig& cfg) {
        USBStorage usb;
        if (!usb.begin()) {
            LOG_EF("USB", "USB not mounted");
            return false;
        }

        Folder root = usb.getRootFolder();
        UFile f = root.createFile("config.json", FileMode::WRITE);

        StaticJsonDocument<4096> doc;

        auto w = doc.createNestedObject("weather");
        w["enabled"] = cfg.weather.enabled;
        w["intervalMs"] = cfg.weather.intervalMs;

        auto wt = w.createNestedObject("thresholds");
        wt["lightDay"] = cfg.weather.config.lightDayThreshold;
        wt["lightNight"] = cfg.weather.config.lightNightThreshold;
        wt["rainStart"] = cfg.weather.config.rainStartThreshold;
        wt["rainStop"] = cfg.weather.config.rainStopThreshold;

        auto p = doc.createNestedObject("power");
        p["enabled"] = cfg.power.enabled;
        p["intervalMs"] = cfg.power.intervalMs;
        p["limitSoft"] = cfg.power.limitSoft;
        p["limitHard"] = cfg.power.limitHard;

        serializeJsonPretty(doc, f);
        f.close();
        usb.unmount();

        LOG_IF("USB", "Config saved to USB");
        return true;
    }
};

#endif
