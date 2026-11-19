#pragma once
/* ============================================================================
   DOMOMANAGER - CONFIGURAZIONE INIZIALE

   Tutte le strutture di configurazione esistono e sono vuote.
   All'interno di ogni struttura resta un solo elemento reale della
   configurazione originale, completamente commentato come riferimento.
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
#include "DMOnline.hpp"
#include "DMRuntimeStore.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================================
// IO AREA IDs
// ============================================================================
// Nessuna area attiva.
// DEFINE_AREA(AREA_PMETER_VOLTAGE, 10)

// ============================================================================
// PHYSICAL DEVICES
// ============================================================================
static const DomoManagerConfig::Devices mainDevicesConfig = {
    {
        // { "Lettore consumi", IPAddress(192,168,12,203), 1, "LE_01MQ",
        //   { 10, 11, 12, 13, 14 }, 3, Low,
        //   "Camera da letto", "Quadro P1" }
    }
};

// ============================================================================
// LOCAL IO
// ============================================================================
static const DomoManagerConfig::LocalIO mainLocalIOConfig = {
    0,
    {
        // { DMPlatform::I1, 62, 10 }
    },
    {
        // { DMPlatform::DO4, 66 }
    }
};

// ============================================================================
// MODBUS RTU IO AREAS
// ============================================================================
static const DomoManagerBufferEngine::AreasConfig mainAreasConfig = {
    {
        // { AREA_PMETER_VOLTAGE, 0, "Lettura tensione", {false}, {50,200} }
    }
};

// ============================================================================
// ROUTES
// ============================================================================
static const DomoManagerRouteEngine::RoutesConfig mainRoutesConfig = {
    {
        // {
        //     "Bagno - Interruttore luce",
        //     BAGNO_IN_P1,
        //     {
        //         { 1, { { RELAY_LED_BAGNO, 1 } } }
        //     }
        // }
    }
};

// ============================================================================
// SPLITS
// ============================================================================
static const DomoManagerSplitEngine::SplitsConfig mainSplitsConfig = {
    {
        // { 78, {114,115}, 30000 }
    }
};

// ============================================================================
// TOGGLES
// ============================================================================
static const DomoManagerToggleEngine::TogglesConfig mainTogglesConfig = {
    {
        // { 54, { BAGNO_IN_P2 } }
    }
};

// ============================================================================
// WATCHDOG
// ============================================================================
static Watchdog::Params wdConfig = {
    // .activity_block_ms = 4000,
    // .activity_avg_ms = 140,
    // .max_spikes = 8,
    // .spikes_window_s = 45,
    // .rate_limit_min_interval = 50,
    // .rate_limit_allowed_factor = 2,
    // .update_slow_ms = 130,
    // .update_avg_ms = 90,
    // .spikeThresholdFactor_fp = 8 * 256
};

// ============================================================================
// AUTOMATIONS
// ============================================================================
static const char* AUTOMATION_JSON = R"json(
{
  "scenes": [
    // {
    //   "name": "CappaOn",
    //   "actions": [
    //     { "area": "AREA_ESTRATTORE_CUCINA", "value": 1 }
    //   ]
    // }
  ],
  "rules": [
    // {
    //   "name": "RegolaBagnoUmido",
    //   "type": "trend",
    //   "intervalMs": 20000,
    //   "sceneTrue": "BagnoUmido",
    //   "sceneFalse": "BagnoNormale"
    // }
  ],
  "sequences": [
    // Esempio sequenza da inserire qui.
  ]
}
)json";

// ============================================================================
// DOMOMANAGER CONFIGURATION LOADER
// ============================================================================
inline void ConfigureDomoManager(DomoManagerConfig& cfg)
{
    cfg = DomoManagerConfig{};

    // HMI inizializzata ma disabilitata.
    cfg.hmi.enabled = false;
    cfg.hmi.port = 0;
    cfg.hmi.pollingMs = 0;
    cfg.hmi.maxClients = 0;

    cfg.watchdog = wdConfig;
    cfg.devices = mainDevicesConfig;
    cfg.routes = mainRoutesConfig;
    cfg.areas = mainAreasConfig;
    cfg.toggles = mainTogglesConfig;
    cfg.splits = mainSplitsConfig;
    cfg.automation.json = AUTOMATION_JSON;
    cfg.localIO = mainLocalIOConfig;
}

// ============================================================================
// RUNTIME STORE
// ============================================================================
namespace RuntimeHandles
{
    inline DMRuntimeStore::Handle room1Card =
        DMRuntimeStore::INVALID_HANDLE;
}

static bool RegisterRuntimeTags(DomoManager& manager)
{
    (void)manager;

    // auto& rs = manager.getRuntimeStore();
    // DMRuntimeStore::Metadata m {};
    // m.id = "access.room01.card_code";
    // m.name = "Room 01 card code";
    // m.access = DMRuntimeStore::Access::READ_WRITE;
    // m.persistent = true;
    // m.publish = true;
    // m.sensitive = false;
    // RuntimeHandles::room1Card = rs.defineString32(m, "0085903398");

    return true;
}

// ============================================================================
// WIRED SENSORS
// ============================================================================
static WiredSensorsManager::WiredSensorConfig WIRED_SENSOR_CONFIG[] = {
    // { "PIR Cucina", "Cucina",
    //   { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
    //   { WiredSensorsManager::MakeReader(AREA_CUCINA_PIR_ALM,
    //       [](int area) {
    //           return DomoManager::instance->getBuffer().getValueFast(area) != 0;
    //       }) },
    //   SensorCategory::PIR,
    //   AREA_CUCINA_PIR_CMD,
    //   AREA_CUCINA_PIR_STATUS }
};
static constexpr size_t WIRED_SENSOR_CONFIG_COUNT = 0;

// ============================================================================
// SECURITY ZONES
// ============================================================================
static const FrontendConfig::SecurityZoneConfig SECURITY_ZONE_CONFIG[] = {
    // { "Cucina", AREA_CUCINA_SECURITY_STATUS, AREA_CUCINA_SECURITY_CMD }
};
static constexpr size_t SECURITY_ZONE_CONFIG_COUNT = 0;

// ============================================================================
// MQTT ENUM VALUES
// ============================================================================
static constexpr FrontendConfig::MQTT::EnumValue EGLO99099_Actions[] = {
    // { "on", 1 }
};
static constexpr size_t EGLO99099_ACTION_COUNT = 0;

// ============================================================================
// MQTT DEVICES
// ============================================================================
static const FrontendConfig::MQTT::Device MQTT_SMHub_Devices[] = {
    // {
    //     "4DI4DOPergola",
    //     "Comando Pergola",
    //     {
    //         { "state_l1",
    //           FrontendConfig::MQTT::Mapping::Direction::READ_WRITE,
    //           FrontendConfig::MQTT::Mapping::DataType::BOOL,
    //           AREA_CUCINA_LUCE_1,
    //           1.0f }
    //     }
    // }
};
static constexpr size_t MQTT_SMHUB_DEVICE_COUNT = 0;

// ============================================================================
// MQTT CLIENTS
// ============================================================================
static const FrontendConfig::MQTT::Client MQTT_CLIENTS[] = {
    // { true,
    //   IPAddress(192,168,12,212),
    //   1883,
    //   FrontendConfig::MQTT::Client::Backend::ZIGBEE2MQTT,
    //   "SMHub",
    //   "opta_domotica",
    //   MQTT_SMHub_Devices,
    //   MQTT_SMHUB_DEVICE_COUNT }
};
static constexpr size_t MQTT_CLIENT_COUNT = 0;

// ============================================================================
// GENERIC SENSOR CONFIGURATION
// ============================================================================
static GenericSensor::Config tempSensorCfg = {
    // GenericSensor::Config::Type::BUFFER,
    // AREA_PMETER_CURRENT
};
static GenericSensor::Config windSensorCfg = {};
static GenericSensor::Config rainSensorCfg = {};
static GenericSensor::Config lightSensorCfg = {};

// ============================================================================
// WEATHER
// ============================================================================
static WeatherStation::CommonConfig weatherCommon = {
    // .readTemp = []() {
    //     return GenericSensor::read(
    //         tempSensorCfg,
    //         DomoManager::instance->getBuffer());
    // },
    // .tempFactor = 100.0f,
    // .lowTempThreshold = 5.0f,
    // .highTempThreshold = 40.0f,
    // .alarmDebounce = 3
};

static WeatherStation::Config weatherConfig = {
    // .common = weatherCommon,
    // .lightDayThreshold = 300.0f,
    // .lightNightThreshold = 200.0f
};

static FrontendConfig::Weather WEATHER_PARAMS = {
    // .enabled = false,
    // .intervalMs = 5000,
    // .config = weatherConfig,
    // .autoCloseWindows = true
};

// ============================================================================
// POWER
// ============================================================================
static FrontendConfig::Power POWER_PARAMS = {
    // .enabled = true,
    // .reporting = true,
    // .intervalMs = 15000,
    // .limitSoft = 2800.0f,
    // .limitHard = 3500.0f,
    // .loads = {
    //     { "Volano termico", 4, 2500.0f, 5, 5 }
    // },
    // .loadCount = 1
};

// ============================================================================
// HVAC ZONES
// ============================================================================
static FrontendConfig::HVAC::Zone HVAC_ZONES[] = {
    // { "Giorno",
    //   AREA_SETPOINT_TEMP_GIORNO,
    //   SENSORE_CUCINA_TEMP,
    //   SENSORE_CUCINA_HUM,
    //   1 }
};
static constexpr size_t HVAC_ZONE_COUNT = 0;

// ============================================================================
// HEAT PUMP
// ============================================================================
static const HeatPumpController::Config HEAT_PUMP_PARAMS = {
    // .hysteresis = 0.5f,
    // .fanHysteresis = 0.3f,
    // .diffLow = 0.5f,
    // .diffMed = 1.5f,
    // .defrostThreshold = 3.0f,
    // .defrostDurationMs = 300000,
    // .maxOnTimeMs = 7200000,
    // .minSwitchDelayMs = 120000,
    // .minOffTimeMs = 180000,
    // .minOutdoorTemp = -7.0f,
    // .maxOutdoorTemp = 45.0f,
    // .windowOpenTimeoutMs = 15000,
    // .postCirculationMs = 60000,
    // .minCirculationCycleMs = 5000
};

// ============================================================================
// AVERAGES
// ============================================================================
static const FrontendConfig::Averages::Sensore sensoriTemp[] = {
    // { SENSORE_CAMERA_TEMP, 0.1f }
};
static constexpr size_t SENSORI_TEMP_COUNT = 0;

static const FrontendConfig::Averages::Sensore sensoriHum[] = {
    // { SENSORE_CAMERA_HUM, 0.1f }
};
static constexpr size_t SENSORI_HUM_COUNT = 0;

static const FrontendConfig::Averages::Gruppo MEAN_GROUPS[] = {
    // { "Temperature",
    //   10.0f,
    //   AREA_MEAN_TEMPS,
    //   sensoriTemp,
    //   SENSORI_TEMP_COUNT }
};
static constexpr size_t MEAN_GROUP_COUNT = 0;

// ============================================================================
// AEE VARIABLES
// ============================================================================
static const AEEVarDef AEE_VARS[] = {
    // { "gridPower",
    //   AEEDirection::FrontendToModule,
    //   AEEVarSourceType::BufferArea,
    //   AREA_PMETER_POWER,
    //   -1,
    //   {},
    //   nullptr, nullptr, nullptr,
    //   AEEVarType::FLOAT,
    //   50.0f,
    //   0.01f }
};
static constexpr size_t AEE_VAR_COUNT = 0;

// ============================================================================
// ACCESS READERS
// ============================================================================
static const AccessReader ACCESS_READERS[] = {
    // { .name = "FanvilIngresso",
    //   .def = &FanvilWebAPI::ReaderDefinition(),
    //   .readerAddress = nullptr,
    //   .readCardCode = []() -> String { return "2953393440"; },
    //   .enabled = true,
    //   .doorIndex = 1,
    //   .area = AREA_CUCINA_LUCE_1,
    //   .authorizedMessageKey = "RELAY_OPEN" }
};
static constexpr size_t ACCESS_READER_COUNT = 0;

// ============================================================================
// WEB API DEVICES
// ============================================================================
static const WebApiDevice WEB_API_DEVICES[] = {
    // { "FanvilIngresso",
    //   &FanvilWebAPI::Profile(),
    //   "192.168.12.216",
    //   80,
    //   0,
    //   nullptr,
    //   true,
    //   { "admin", "admin" } }
};
static constexpr size_t WEB_API_DEVICE_COUNT = 0;

// ============================================================================
// FINE CONFIGURAZIONE INIZIALE
// ============================================================================
