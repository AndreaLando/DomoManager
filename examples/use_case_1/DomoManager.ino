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

// ============================================================================
// HARDWARE TARGET
// ============================================================================
//
// La piattaforma viene selezionata automaticamente da DMPlatform.hpp in base
// alla scheda scelta nel menu Tools -> Board dell'IDE Arduino.
//
//   Opta                -> ARDUINO_ARCH_MBED
//   ESP32-S3-POE-ETH    -> ARDUINO_ARCH_ESP32
//
// Assicurarsi che la scheda configurata nell'IDE corrisponda all'hardware
// fisicamente utilizzato.
// ============================================================================             

#include "DMFrontend.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ======================================================
// DIAGNOSTICS
// - diagnosticParams enables a set of diagnostic reports
// ======================================================
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

// Watch areas: indices of diagnostic bits to monitor, this stops debugging on SerialMonitor Arduino IDE
static const int watchAreas[] = {
    AREA_CUCINA_FLOOD_ALM,
    AREA_BAGNO_FLOOD_ALM    
};
// ------------------------------------------------------------
// FRONTEND CONFIG (mainConfig)
// - This lambda constructs the FrontendConfig instance used at startup.
// - Many network and subsystem flags are disabled by default (bridge, mqtt, weather, ps).
// ------------------------------------------------------------

static FrontendConfig mainConfig = [](){
    FrontendConfig c;

    c.pins.userButton = DMPlatform::UserButton;
    c.pins.leds = {
        DMPlatform::LedD0, // RS485 Read
        DMPlatform::LedD1, // RS485 Write
        DMPlatform::LedD2, // HMI, MQTT Read/Write
        DMPlatform::LedD3  // Devices in error
    };

    // --- NETWORK ---
    c.net.mac     = {0x90, 0xA2, 0xDA, 0x0E, 0x94, 0xB5};
    c.net.ip      = IPAddress(192, 168, 12, 202);
    c.net.gateway = IPAddress(192, 168, 12, 1);
    c.net.subnet  = IPAddress(255, 255, 255, 0);

    // --- BRIDGE ---
    c.bridge.enabled    = false;
    c.bridge.ip         = IPAddress(192,168,12,201); //Indirizzo del PEER
    c.bridge.localPort  = 8888;
    c.bridge.remotePort = 8888;
    c.bridge.aee.vars  = AEE_VARS;
    c.bridge.aee.count = sizeof(AEE_VARS) / sizeof(AEEVarDef);


    // --- MODBUS TCP to RTU ---
    c.modbus.enabled = true;
    c.modbus.timeoutMs = 200; //200ms non scendere mai sotto questo valore 15.05.2026
    
    
    // ============================================================================
    // CONFIG MQTT NEL FRONTEND
    // ============================================================================

    c.mqtt.enabled = true;
    c.mqtt.clients = MQTT_CLIENTS;
    c.mqtt.clientCount =
        sizeof(MQTT_CLIENTS) /
        sizeof(MQTT_CLIENTS[0]);

    // --- DomoManager ---
    c.domoManager=domoConfig;
    
    // --- HVAC ---
    c.hvac.enabled    = false;
    c.hvac.intervalMs = 10000;
    c.hvac.zones = HVAC_ZONES;
    c.hvac.zoneCount = sizeof(HVAC_ZONES) / sizeof(HVAC_ZONES[0]);;
    c.hvac.initialSetpoint = 20.0;
    c.hvac.initialMode = HeatPumpController::Mode::OFF;
    c.hvac.hpConfig=HEAT_PUMP_PARAMS;

    // External/internal temperature readers using DomoManager instance
    c.hvac.readOutdoorTemp  = [&](){
        return DomoManager::instance->getBuffer().getValueFast(5) / 10.0f;
    };
    c.hvac.readIndoorTemp  = [&](){
        return DomoManager::instance->getAverages().groupAverage("Temperature");
    };
    c.hvac.readWindowOpen = [&]() {
        auto& sys = SecurityOrchestrator::getSystem().info.f;
        using SM = SecurityOrchestrator::SystemManager;

        return sys[SM::WINDOWS_OPEN].get() ||
            sys[SM::DOORS_OPEN].get();
    };


    // --- WEATHER ---
    c.weather = WEATHER_PARAMS;

    // --- POWER SUPERVISOR ---
    c.ps.enabled    = false;
    c.ps.intervalMs = 5000;
    c.ps.mainPower = { GenericSensor::Config::Type::BUFFER, AREA_PMETER_VOLTAGE, 0.01f };
    c.ps.i24vOk    = { GenericSensor::Config::Type::DIGITAL, DMPlatform::I1 };
    c.ps.fault     = { GenericSensor::Config::Type::DIGITAL, DMPlatform::I2 };
    c.ps.battery   = { GenericSensor::Config::Type::DIGITAL, DMPlatform::I3 };
    c.ps.i12vOk    = { GenericSensor::Config::Type::DIGITAL, DMPlatform::I4 };
    c.ps.mainPowerLow  = 215.0f;
    c.ps.mainPowerHigh = 240.0f;

    // --- SECURITY ---
    c.security.enabled    = true;
    c.security.intervalMs = 350;
    c.security.reportOnChange  = false;                      //Abilito la visualizzazione del report ogni volta che cambia un sensore
    c.security.eventArea = -1;                              //Non mi interessa accedere al buffer eventi
    c.security.panelCommandArea = AREA_SECURITY_CMD_AREA;   //Area comandi centralina
    c.security.panelBitCommandArea=AREA_SECURITY_BIT_AREA;  //Area comandi a bit
    c.security.statusArea=  AREA_SECURITY_STATUS;            // WORD 1 - stato sicurezza    
    c.security.statusArea2= AREA_SECURITY_STATUS2;          // WORD 2 - stato centralina
    c.security.startupInhibitMs = 10000;
    
    c.security.sensors = WIRED_SENSOR_CONFIG;
    c.security.count   = sizeof(WIRED_SENSOR_CONFIG) / sizeof(WIRED_SENSOR_CONFIG[0]);

    c.security.zones = SECURITY_ZONE_CONFIG;
    c.security.zoneCount = sizeof(SECURITY_ZONE_CONFIG) / sizeof(SECURITY_ZONE_CONFIG[0]);

    // --- POWER LIMITS ---
    c.power = POWER_PARAMS;

    // --- Averages ---
    c.averages.enabled = true;
    c.averages.intervalMs = 15000;   // intervallo task
    c.averages.gruppi = MEAN_GROUPS;
    c.averages.gruppiCount = sizeof(MEAN_GROUPS) / sizeof(MEAN_GROUPS[0]);

    // --- DIAGNOSTICS  ---
    c.diagnostic=diagnosticParams;

    // --- WATCH ---
    c.watch.enabled = true;
    c.watch.aree = watchAreas;
    c.watch.count = sizeof(watchAreas) / sizeof(watchAreas[0]);

    // ============================================================
    // WEB API CLIENT / POLLING
    // ============================================================
    c.webApi.client.enabled = false;
    c.webApi.client.devices = WEB_API_DEVICES;
    c.webApi.client.deviceCount = sizeof(WEB_API_DEVICES) / sizeof(WEB_API_DEVICES[0]);
    c.webApi.client.timeoutMs = 3000;

    // ============================================================
    // WEB API SERVER / WEBHOOK
    // ============================================================
    c.webApi.server.enabled = false;
    c.webApi.server.port = 8080;
    c.webApi.server.timeoutMs = 3000;

    // ============================================================
    // ACCESS CONTROL
    // ============================================================
    c.access.enabled = false;
    c.access.readers = ACCESS_READERS;
    c.access.readerCount = sizeof(ACCESS_READERS) / sizeof(ACCESS_READERS[0]);;
    c.access.commonCardCode = "12345"; //Codice valido su tutti i lettori.
    c.access.defaultRelayIndex = 1;
    c.access.defaultRelayTime = 10;     // 10 decimi = 1 secondo.

    // --- JOBS ---
    c.jobs.enabled = false;
    c.jobs.intervalMs=5500;

    // ============================================================
    // SOLAR INVERTER (gestisce piu inverter)
    // ============================================================
    c.solarInverters.count = 1;

    auto& solar0 =
        c.solarInverters.inverter[0];

    solar0.enabled        = false;
    solar0.reportOnChange = false;
    solar0.autoDiscovery  = true;
    solar0.intervalMs     = 5; // Loop veloce: protocollo non bloccante

    solar0.transport =
        FrontendConfig::Solar::
            TransportType::TCP_CLIENT;

    solar0.serial =
    {
        &Serial1,
        2400
    };

    solar0.tcp =
    {
        IPAddress(192, 168, 12, 206),
        8899,
        3000
    };

    solar0.areas =
    {
        AREA_INVERTER_PV_VOLTAGE,
        AREA_INVERTER_PV_CURRENT,
        AREA_INVERTER_PV_POWER,

        AREA_INVERTER_BATTERY_VOLTAGE,
        AREA_INVERTER_BATTERY_SOC,

        -1, // gridVoltage
        -1, // gridPower

        -1, // outputPower
        -1, // loadPercent

        AREA_INVERTER_TEMP,
        AREA_INVERTER_STATUS,
        AREA_INVERTER_FRONTEND_STATUS
    };

    return c;
}();

void setup() {
    Serial.begin(19200);

    DMPlatform::SetupPlatformIO();
    DomoManagerFrontendEngine::Setup(mainConfig);
}

void loop() {
    DomoManagerFrontendEngine::loop();
}
