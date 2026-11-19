/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it

   Versione modulo: 1.0.0
   Ultima modifica: 2026-11-18
   Note:
                    - Configurazione Frontend costruita a runtime in setup().
                    - Eliminata l'inizializzazione globale di FrontendConfig.

   ============================================================================ */

#include "DMFrontend.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

static const DiagnosticConfig diagnosticParams = {
    .reportAutomationEngine     = false,
    .reportScheduler            = false,
    .reportSplitAnalysis        = false,
    .reportMissingBufferDefs    = false,
    .reportDeviceAreas          = false,
    .reportIneffectiveRules     = false,
    .reportDeviceErrors         = true,
    .reportWatchdogs            = false,
    .reportDeviceProfiles       = false,
    .reportRTC                  = true,
    .reportHotStandby           = true,
    .reportVirtualAreas         = false,
    .reportNeverInitialized     = false,
    .reportMultipleInitialized  = true,
    .reportAutomationConfig     = false,
    .reportLogBuffer            = true,
    .reportModbusTiming         = true
};

static const int watchAreas[] = {
    AREA_CUCINA_FLOOD_ALM,
    AREA_BAGNO_FLOOD_ALM
};

// Prima di setup() esiste soltanto un puntatore nullo.
static FrontendConfig* mainConfig = nullptr;

static void ConfigureFrontend(
    FrontendConfig& c)
{
    // --- DomoManager ---
    ConfigureDomoManager(c.domoManager);

    // --- PINS ---
    c.pins.userButton = DMPlatform::UserButton;
    c.pins.leds = {
        DMPlatform::LedD0,
        DMPlatform::LedD1,
        DMPlatform::LedD2,
        DMPlatform::LedD3
    };

    // --- NETWORK ---
    c.net.mac     = {0x90, 0xA2, 0xDA, 0x0E, 0x94, 0xB5};
    c.net.ip      = IPAddress(192, 168, 12, 202);
    c.net.gateway = IPAddress(192, 168, 12, 1);
    c.net.subnet  = IPAddress(255, 255, 255, 0);

    // --- BRIDGES ---
    c.bridges.count = 2;

    auto& peer = c.bridges.channel[0];
    peer.enabled = true;
    peer.id = 0;
    peer.name = "AEE-PEER";
    peer.ip = IPAddress(192, 168, 12, 201);
    peer.localPort = 8888;
    peer.remotePort = 8888;
    peer.aee.vars = AEE_VARS;
    peer.aee.count = sizeof(AEE_VARS) / sizeof(AEE_VARS[0]);

    auto& knob = c.bridges.channel[1];
    knob.enabled = true;
    knob.id = 1;
    knob.name = "AEE-KNOB";
    knob.ip = IPAddress(0, 0, 0, 0);
    knob.localPort = 8889;
    knob.remotePort = 0;
    knob.aee.vars = nullptr;
    knob.aee.count = 0;

    // --- MQTT ---
    c.mqtt.enabled = false;
    c.mqtt.clients = MQTT_CLIENTS;
    c.mqtt.clientCount =
        sizeof(MQTT_CLIENTS) /
        sizeof(MQTT_CLIENTS[0]);

    // --- HVAC ---
    c.hvac.enabled = true;
    c.hvac.intervalMs = 10000;
    c.hvac.zones = HVAC_ZONES;
    c.hvac.zoneCount =
        sizeof(HVAC_ZONES) /
        sizeof(HVAC_ZONES[0]);
    c.hvac.initialSetpoint = 20.0f;
    c.hvac.initialMode = HeatPumpController::Mode::OFF;
    c.hvac.hpConfig = HEAT_PUMP_PARAMS;

    c.hvac.readOutdoorTemp = []() -> float {
        if (!DomoManager::instance) {
            return 0.0f;
        }
        return
            DomoManager::instance
                ->getBuffer()
                .getValueFast(5) /
            10.0f;
    };

    c.hvac.readIndoorTemp = []() -> float {
        if (!DomoManager::instance) {
            return 0.0f;
        }
        return
            DomoManager::instance
                ->getAverages()
                .groupAverage("Temperature");
    };

    c.hvac.readWindowOpen = []() -> bool {
        auto& sys =
            SecurityOrchestrator::getSystem().info.f;
        using SM = SecurityOrchestrator::SystemManager;

        return
            sys[SM::WINDOWS_OPEN].get() ||
            sys[SM::DOORS_OPEN].get();
    };

    // --- WEATHER ---
    c.weather = WEATHER_PARAMS;

    // --- POWER SUPERVISOR ---
    c.ps.enabled = false;
    c.ps.intervalMs = 5000;
    c.ps.mainPower = {
        GenericSensor::Config::Type::BUFFER,
        AREA_PMETER_VOLTAGE,
        0.01f
    };
    c.ps.i24vOk = {
        GenericSensor::Config::Type::DIGITAL,
        DMPlatform::I1
    };
    c.ps.fault = {
        GenericSensor::Config::Type::DIGITAL,
        DMPlatform::I2
    };
    c.ps.battery = {
        GenericSensor::Config::Type::DIGITAL,
        DMPlatform::I3
    };
    c.ps.i12vOk = {
        GenericSensor::Config::Type::DIGITAL,
        DMPlatform::I4
    };
    c.ps.mainPowerLow = 215.0f;
    c.ps.mainPowerHigh = 240.0f;

    // --- SECURITY ---
    c.security.enabled = true;
    c.security.intervalMs = 350;
    c.security.reportOnChange = false;
    c.security.eventArea = -1;
    c.security.panelCommandArea = AREA_SECURITY_CMD_AREA;
    c.security.panelBitCommandArea = AREA_SECURITY_BIT_AREA;
    c.security.statusArea = AREA_SECURITY_STATUS;
    c.security.statusArea2 = AREA_SECURITY_STATUS2;
    c.security.startupInhibitMs = 10000;

    c.security.sensors = WIRED_SENSOR_CONFIG;
    c.security.count =
        sizeof(WIRED_SENSOR_CONFIG) /
        sizeof(WIRED_SENSOR_CONFIG[0]);

    c.security.zones = SECURITY_ZONE_CONFIG;
    c.security.zoneCount =
        sizeof(SECURITY_ZONE_CONFIG) /
        sizeof(SECURITY_ZONE_CONFIG[0]);

    // --- POWER LIMITS ---
    c.power = POWER_PARAMS;

    // --- AVERAGES ---
    c.averages.enabled = true;
    c.averages.intervalMs = 15000;
    c.averages.gruppi = MEAN_GROUPS;
    c.averages.gruppiCount =
        sizeof(MEAN_GROUPS) /
        sizeof(MEAN_GROUPS[0]);

    // --- DIAGNOSTICS ---
    c.diagnostic = diagnosticParams;

    // --- WATCH ---
    c.watch.enabled = true;
    c.watch.aree = watchAreas;
    c.watch.count =
        sizeof(watchAreas) /
        sizeof(watchAreas[0]);

    // --- WEB API CLIENT ---
    c.webApi.client.enabled = false;
    c.webApi.client.devices = WEB_API_DEVICES;
    c.webApi.client.deviceCount =
        sizeof(WEB_API_DEVICES) /
        sizeof(WEB_API_DEVICES[0]);
    c.webApi.client.timeoutMs = 3000;

    // --- WEB API SERVER ---
    c.webApi.server.enabled = false;
    c.webApi.server.port = 8080;
    c.webApi.server.timeoutMs = 3000;

    // --- ACCESS CONTROL ---
    c.access.enabled = false;
    c.access.readers = ACCESS_READERS;
    c.access.readerCount =
        sizeof(ACCESS_READERS) /
        sizeof(ACCESS_READERS[0]);
    c.access.commonCardCode = "12345";
    c.access.defaultRelayIndex = 1;
    c.access.defaultRelayTime = 10;

    // --- JOBS ---
    c.jobs.enabled = false;
    c.jobs.intervalMs = 5500;

    // --- SOLAR INVERTER ---
    c.solarInverters.count = 1;

    auto& solar0 = c.solarInverters.inverter[0];
    solar0.enabled = false;
    solar0.reportOnChange = false;
    solar0.autoDiscovery = true;
    solar0.intervalMs = 5;
    solar0.transport =
        FrontendConfig::Solar::TransportType::TCP_CLIENT;

    solar0.serial = {
        &Serial1,
        2400
    };

    solar0.tcp = {
        IPAddress(192, 168, 12, 206),
        8899,
        3000
    };

    solar0.areas = {
        AREA_INVERTER_PV_VOLTAGE,
        AREA_INVERTER_PV_CURRENT,
        AREA_INVERTER_PV_POWER,
        AREA_INVERTER_BATTERY_VOLTAGE,
        AREA_INVERTER_BATTERY_SOC,
        -1,
        -1,
        -1,
        -1,
        AREA_INVERTER_TEMP,
        AREA_INVERTER_STATUS,
        AREA_INVERTER_FRONTEND_STATUS
    };
}

void setup()
{
    Serial.begin(19200);
    delay(3000);

    mainConfig = new FrontendConfig();

    if (!mainConfig) {
        Serial.println("[BOOT] allocazione FrontendConfig fallita");
        Serial.flush();
        while (true) {
            delay(1000);
        }
    }

    Serial.println("[BOOT] FrontendConfig costruito");
    Serial.flush();

    ConfigureFrontend(*mainConfig);

    Serial.println("[BOOT] configurazione completata");
    Serial.flush();

    DomoManagerFrontendEngine::Setup(*mainConfig);
}

void loop()
{
    DomoManagerFrontendEngine::loop();
}
