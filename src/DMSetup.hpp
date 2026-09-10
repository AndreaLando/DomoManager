#ifndef DMSetup_HPP
#define DMSetup_HPP

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


#include <ArduinoJson.h>
#include "DM.hpp"
#include "DMHVAC.h"
#include "DMWeather.hpp"
#include "DMWiredSensors.hpp"
#include "DMDiagnostic.hpp"
#include "DMBridge.hpp"
#include "DMWebApiDefs.hpp"
#include "DMPower.hpp"
#include "DMFrontendEngines.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ------------------------------------------------------------
// CONFIGURAZIONE DOMOMANAGER
// ------------------------------------------------------------

// ************ IO AREA IDs*******************************
// 0..9 RESERVED
// Esempio: DEFINE_AREA(AREA_CAMERA_WIN1_ALM, 13) 


// ************ PHISICAL DEVICES *******************************
// Esempio: arduino::IPAddress WaveShareP1_Addr=IPAddress(192, 168, 12, 203);

static const DomoManagerConfig::Devices mainDevicesConfig = {
    {
        // --- Esempio: ---
        //{ "Lettore consumi - Quadro P1", WaveShareP1_Addr, 1, "LE_01MQ",
        //  { 10, 11, 12, 13, 14 }, 3, Low
        //}

    }
};


// ************ IO AREAS *******************************
static const DomoManagerBufferEngine::AreasConfig mainAreasConfig = {
    {
        // --- Esempio: ---
        //{ AREA_PMETER_VOLTAGE, 0, "Lettura tensione", {false}, {50,200} }
    
    }
};

// ************ ROUTES, SPLITS and TOGGLES *******************************
static const DomoManagerRouteEngine::RoutesConfig mainRoutesConfig = {
    {
        // --- Esempio: ---
        /*{
            "Bagno - Interruttore luce",
            BAGNO_IN_P1,
            {
                // Caso: value == 1
                {
                    1,
                    {
                        { RELAY_LED_BAGNO, 1 },
                        { BAGNO_OUT_LED01R, 0 },
                        { BAGNO_OUT_LED01G, 0 },
                        { BAGNO_OUT_LED01B, 0 },
                        { BAGNO_OUT_LED01W, 4095 },
                    }
                },

                // Caso: value == 0
                {
                    0,
                    {
                        { RELAY_LED_BAGNO, 0 },
                        { BAGNO_OUT_LED01R, 0 },
                        { BAGNO_OUT_LED01G, 0 },
                        { BAGNO_OUT_LED01B, 0 },
                        { BAGNO_OUT_LED01W, 0 },
                    }
                }
            }
        }*/
    }
};

static const DomoManagerSplitEngine::SplitsConfig mainSplitsConfig = {
    {
        // --- Esempio: ---
        //{ 78, {114,115}, 30000 }
    }
};

static const DomoManagerToggleEngine::TogglesConfig mainTogglesConfig = {
    {
        // --- Esempio: ---
        //{ 90, { 82, 55 } },
        //{ 63, {} }
    }
};

// ************ WATCHDOG *******************************
static Watchdog::Params wdConfig = {
    .activity_block_ms = 4000,
    .activity_avg_ms   = 140,
    .max_spikes        = 8,
    .spikes_window_s   = 45,
    .rate_limit_min_interval = 50,
    .rate_limit_allowed_factor = 2,
    .update_slow_ms = 110,
    .update_avg_ms  = 90,
    .spikeThresholdFactor_fp = 8 * 256
};

// ************ AUTOMATIONS *******************************
static const char* AUTOMATION_JSON = R"json(
{
    // --- Esempio: ---
    /*
    "scenes": [
        {
        "name": "CappaOn",
        "actions": [
            { "area": "AREA_ESTRATTORE_CUCINA", "value": 1 }
        ]
        },
        {
        "name": "CappaOff",
        "actions": [
            { "area": "AREA_ESTRATTORE_CUCINA", "value": 0 }
        ]
        },
    ],

    "rules": [
        {
            "name": "RegolaCappa",
            "type": "composite",
            "intervalMs": 20000,

            "composite": {
                "logic": "OR",

                "inputs": [
                {
                    "type": "debounce",
                    "name": "corrente",
                    "area": "SENSORE_CUCINA_CORRENTE",
                    "threshold": 1,
                    "debounceMs": 10000,
                    "tofMinutes": 10
                },
                {
                    "type": "trend",
                    "name": "umidita",
                    "area": "SENSORE_CUCINA_HUM",
                    "scale": 10,
                    "threshold": 65,
                    "trend": "rising",
                    "tofMinutes": 5
                },
                {
                    "type": "bitmask",
                    "name": "manuale",
                    "area": "AREA_CUCINA_ESTRATTORE_BIT",
                    "bitIndex": 2
                }
                ],

                "output": {
                "area": "AREA_CUCINA_ESTRATTORE_BIT",
                "bitIndex": 0
                }
        },

        "sceneTrue": "CappaOn",
        "sceneFalse": "CappaOff"
        }
    ],

    "sequences": [] */
}
)json";

// ************ LOADER *******************************
DomoManagerConfig makeDomoConfig() {
    DomoManagerConfig cfg;   // usa tutti i default della struct

    cfg.hmi.enabled = false;   // opzionale, è già default
    cfg.hmi.port = 502;        // default
    cfg.hmi.pollingMs = 250;   // default
    cfg.hmi.maxClients = 1;

    // --- Watchdog ---
    cfg.watchdog = wdConfig;

    // --- Devices ---
    cfg.devices = mainDevicesConfig;

    // --- Engines ---
    cfg.routes  = mainRoutesConfig;
    cfg.areas   = mainAreasConfig;
    cfg.toggles = mainTogglesConfig;
    cfg.splits  = mainSplitsConfig;

    // --- Automation JSON ---
    cfg.automation.json = AUTOMATION_JSON;

    return cfg;
}

static DomoManagerConfig domoConfig = makeDomoConfig();

// ------------------------------------------------------------
// CONFIGURAZIONE FRONTEND
// ------------------------------------------------------------



// ************ WIRED SENSORS DEFINITION *******************************
//
// The array below defines wired sensors used by the SecuritySensorEngine.
// Each entry contains:
//   - a location label (e.g., "Cucina", "Camera")
//   - a list of SensorChannel descriptors (timing/type)
//   - a list of reader lambdas that return boolean alarm/tamper states
//   - a SensorCategory (PIR, DOOR, FLOOD, SMOKE, WINDOW)
//
// Readers access the central Buffer via SecuritySensorEngine::getBuffer()

static WiredSensorsManager::WiredSensorConfig WIRED_SENSOR_CONFIG[] = {

    // --- Esempio: ---
    /*
    { "Cucina",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CUCINA_DOOR_ALM) != 0; }
        },
        SensorCategory::DOOR
    }
    */
};

// ============================================================================
// MQTT - CONFIGURAZIONE COMPLETA
// ============================================================================

// ============================================================================
// DEVICE - SMHUB / ZIGBEE2MQTT
// ============================================================================
static constexpr FrontendConfig::MQTT::EnumValue
    EGLO99099_Actions[] =
{
    { "on",                           1 },
    { "off",                          2 },

    { "red",                          3 },
    { "red_long",                     4 },

    { "refresh",                      5 },
    { "refresh_long",                 6 },

    { "refresh_colored",              7 },
    { "refresh_colored_long",         8 },

    { "blue",                         9 },
    { "blue_long",                   10 },

    { "green",                       11 },
    { "green_long",                  12 },

    { "brightness_step_up",           13 },
    { "brightness_step_down",         14 },
    { "brightness_move_to_level",     15 },

    { "color_temperature_step_up",    16 },
    { "color_temperature_step_down",  17 },
    { "color_temperature_move",       18 },

    { "recall_1",                     19 },
    { "recall_1_long",                20 },

    { "recall_2",                     21 },
    { "recall_2_long",                22 }
};


static const FrontendConfig::MQTT::Device
    MQTT_SMHub_Devices[] =
{
    
    /*
     * ============================================================
     * EGLO 99099 - Telecomando
     * ============================================================
     

    {
        "Telecomando",
        "Telecomando EGLO 99099",

        {
            {
                "action",
                FrontendConfig::MQTT::Mapping::Direction::READ,
                FrontendConfig::MQTT::Mapping::DataType::ENUM,
                500,
                1.0f,
                EGLO99099_Actions
            },

            {
                "action_group",
                FrontendConfig::MQTT::Mapping::Direction::READ,
                FrontendConfig::MQTT::Mapping::DataType::INT,
                501,
                1.0f
            },

            {
                "action_level",
                FrontendConfig::MQTT::Mapping::Direction::READ,
                FrontendConfig::MQTT::Mapping::DataType::INT,
                502,
                1.0f
            },

            {
                "action_color_temperature",
                FrontendConfig::MQTT::Mapping::Direction::READ,
                FrontendConfig::MQTT::Mapping::DataType::INT,
                503,
                1.0f
            }
        }
    }
    */
};


// ============================================================================
// CLIENT MQTT
// ============================================================================
//
// Per ora entrambi puntano allo stesso broker.
// In seguito puoi mettere broker diversi.
//
// Client 0 = Home Assistant
// Client 1 = SMHub / Zigbee2MQTT
// ============================================================================

static const FrontendConfig::MQTT::Client MQTT_CLIENTS[] =
{
    {
        true,
        IPAddress(192, 168, 12, 212),
        1883,
        FrontendConfig::MQTT::Client::Backend::ZIGBEE2MQTT,
        "SMHub",
        "opta_domotica",
        MQTT_SMHub_Devices,
        sizeof(MQTT_SMHub_Devices) /
        sizeof(MQTT_SMHub_Devices[0])
    }
};



// ------------------------------------------------------------
// GENERIC SENSOR CONFIGURATION
// These configs tell GenericSensor where to read values from.
// Type::BUFFER means read from Buffer area.
// ------------------------------------------------------------
static GenericSensor::Config tempSensorCfg  = { GenericSensor::Config::Type::BUFFER, 10 };
static GenericSensor::Config windSensorCfg  = { GenericSensor::Config::Type::BUFFER, 11 };
static GenericSensor::Config rainSensorCfg  = { GenericSensor::Config::Type::BUFFER, 12 };
static GenericSensor::Config lightSensorCfg = { GenericSensor::Config::Type::BUFFER, 13 };

// ======================================================
// WEATHER CONFIG
// - weatherCommon contains function hooks and scaling/thresholds
// - weatherConfig contains runtime thresholds and debounce settings
// - weatherParams is the FrontendConfig::Weather instance used at startup
// ======================================================

// Common readers use GenericSensor::read with the DomoManager buffer.

static WeatherStation::CommonConfig weatherCommon = {
    .readTemp  = [](){ return GenericSensor::read(tempSensorCfg,  DomoManager::instance->getBuffer()); },
    .readWind  = [](){ return GenericSensor::read(windSensorCfg,  DomoManager::instance->getBuffer()); },
    .readRain  = [](){ return GenericSensor::read(rainSensorCfg,  DomoManager::instance->getBuffer()); },
    .readLight = [](){ return GenericSensor::read(lightSensorCfg, DomoManager::instance->getBuffer()); },

    .tempFactor  = 100.0f,
    .windFactor  = 6.0f,
    .rainFactor  = 20.0f,
    .lightFactor = 100.0f,

    .lowTempThreshold  = 5.0f,
    .highTempThreshold = 40.0f,
    .highWindThreshold = 200.0f,
    .highRainThreshold = 300.0f,

    .windGustDelta = 3.0f,
    .alarmDebounce = 3
};

static WeatherStation::Config weatherConfig = {
    .common = weatherCommon,

    .lightDayThreshold   = 300.0f,
    .lightNightThreshold = 200.0f,

    .rainStartThreshold = 40.0f,
    .rainStopThreshold  = 20.0f,
    .rainStartDebounce  = 3,
    .rainStopDebounce   = 3,

    .gustDebounce = 2
};

static FrontendConfig::Weather WEATHER_PARAMS = {
    .enabled=false,
    .intervalMs=5000,
    .config = weatherConfig,
    .autoCloseWindows = true
};

// ======================================================
// POWER CONFIG (moved out of main logic)
// - powerParams contains power management settings and load definitions
// ======================================================
static FrontendConfig::Power POWER_PARAMS = {
    .enabled=false,
    .intervalMs=15000,
    .limitSoft = 3000.0f,
    .limitHard = 3500.0f,

    .autoTune = true,
    .tuneIntervalMs = 2 * 60 * 1000UL,

    // Carichi
    .loads = {
        { "Boiler acqua calda", 1, 1200.0f, 5, 5 },
        { "Forno", 0, 1800.0f, 30, 30 }
    },

    .loadCount = 2,

    // Thermal load
    .thermal = {
        "Pompa di Calore",
        true,
        20.0f,
        18.0f,
        23.0f,
        60,
        60
    }
};

// ------------------------------------------------------------
// HVAC ZONES
// Each zone: name, default setpoint, enabled flag, sensor area
// ------------------------------------------------------------
static FrontendConfig::HVAC::Zone HVAC_ZONES[] = {
    { "Giorno", 22.0, 10, 1  },
    { "Notte", 20.0, 11, 1  },
    { "Bagno", 23.0, 12, 1  }
};

// ======================================================
// HEAT PUMP STATIC CONFIGURATION (pdcParams)
// - hysteresis, defrost thresholds, timing limits
// ======================================================
static const HeatPumpController::Config HEAT_PUMP_PARAMS = {
    .hysteresis              = 0.5f,
    .fanHysteresis           = 0.3f,

    .diffLow                 = 0.5f,
    .diffMed                 = 1.5f,

    .defrostThreshold        = 3.0f,
    .defrostDurationMs       = 300000,

    .maxOnTimeMs             = 7200000,
    .minSwitchDelayMs        = 120000,
    .minOffTimeMs            = 180000,

    .minOutdoorTemp          = -7.0f,
    .maxOutdoorTemp          = 45.0f,

    .windowOpenTimeoutMs     = 15000,

    .postCirculationMs       = 60000,
    .minCirculationCycleMs   = 5000
};

// ======================================================
// AVERAGES (MEDIE)
// - groups of sensors used to compute aggregated values
// ======================================================

// Temperature sensors group (each sensor has a scale factor)
static const FrontendConfig::Averages::Sensore sensoriTemp[] = {
    { 15,  0.1f },
    { 16,   0.1f },
    { 17,  0.1f }
};


// Groups definition: name, outScale, output area, sensors array, sensor count
static const FrontendConfig::Averages::Gruppo MEAN_GROUPS[] = {
    {
        "Temperature",        // nome gruppo
        10.0f,                // outScale (scrittura *10)
        12,      // area di output
        sensoriTemp,          // array sensori
        sizeof(sensoriTemp) / sizeof(sensoriTemp[0])
    }
};

// ======================================================
// AEE VARIABLE DEFINITIONS (Bridge group)
// - maps buffer areas and functions to AEE variables exposed to frontend
// ======================================================
static const AEEVarDef AEE_VARS[] = {

    // ============================================================
    // SECURITY (buffer → bool)
    // ============================================================
    { "allarmeIntrusione", AEEDirection::FrontendToModule,
        AEEVarSourceType::BufferArea,
        11, 0,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::BOOL }
        
};


#endif
