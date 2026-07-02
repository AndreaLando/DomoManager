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

#include "DMUsb.hpp"
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
        .reportLogBuffer            = true
    };

// Watch areas: indices of diagnostic bits to monitor, this stops debugging on SerialMonitor Arduino IDE
static const int watchAreas[] = {
    //AREA_CUCINA_FLOOD_ALM,
    //AREA_BAGNO_FLOOD_ALM    
};
// ------------------------------------------------------------
// FRONTEND CONFIG (mainConfig)
// - This lambda constructs the FrontendConfig instance used at startup.
// - Many network and subsystem flags are disabled by default (bridge, mqtt, weather, ps).
// ------------------------------------------------------------
static FrontendConfig mainConfig = [](){
    FrontendConfig c;

    c.pins.userButton = BTN_USER;
    c.pins.leds = LedController::LedPins(
        LED_D0, //RS485 Read
        LED_D1, //RS485 Write
        LED_D2, // HMI Read/Write   
        LED_D3  // Devices in error
    );

    // --- NETWORK ---
    c.net.mac     = {0x90, 0xA2, 0xDA, 0x0E, 0x94, 0xB5};
    c.net.ip      = IPAddress(192, 168, 12, 202);
    c.net.gateway = IPAddress(192, 168, 12, 1);
    c.net.subnet  = IPAddress(255, 255, 255, 0);

    // --- IOT ---
    c.bridge.enabled    = false;
    c.bridge.ip         = IPAddress(192,168,12,201);
    c.bridge.localPort  = 8888;
    c.bridge.remotePort = 8888;
    //c.bridge.aee.vars  = AEE_VARS;
    //c.bridge.aee.count = sizeof(AEE_VARS) / sizeof(AEEVarDef);


    // --- MODBUS TCP to RTU ---
    c.modbus.timeoutMs = 250; //200ms non scendere mai sotto questo valore 15.05.2026, valore ok=250 cautelativo
    
    // --- MQTT ---
    c.mqtt.enabled = false;
    c.mqtt.broker = IPAddress(192,168,1,10);
    c.mqtt.port = 1883;
    c.mqtt.nodeId = "opta_domotica";
    //c.mqtt.vars      = MQTT_VARS;
    //c.mqtt.varCount  = sizeof(MQTT_VARS) / sizeof(MQTT_VARS[0]);

    // --- DomoManager ---
    c.domoManager=domoConfig;
    
    // --- HVAC ---
    c.hvac.enabled    = false;
    c.hvac.intervalMs = 10000;
    //c.hvac.zones = HVAC_ZONES;
    //c.hvac.zoneCount = sizeof(HVAC_ZONES) / sizeof(HVAC_ZONES[0]);;
    c.hvac.initialSetpoint = 20.0;
    c.hvac.initialMode = HeatPumpController::Mode::OFF;
    //c.hvac.hpConfig=HEAT_PUMP_PARAMS;

    // External/internal temperature readers using DomoManager instance
    c.hvac.readOutdoorTemp  = [&](){
        return DomoManager::instance->getBuffer().getValueFast(5) / 10.0f;
    };
    c.hvac.readIndoorTemp  = [&](){
        return DomoManager::instance->getAverages().groupAverage("Temperature");
    };
    c.hvac.readWindowOpen = [&]() {
        auto& sys = SecuritySensorEngine::getSystem().info.f;
        using SM = SecurityOrchestrator::SystemManager;

        return sys[SM::WINDOWS_OPEN].get() ||
            sys[SM::DOORS_OPEN].get();
    };


    // --- WEATHER ---
    c.weather.enabled    = false;
    c.weather.intervalMs=5000;
    //c.weather = WEATHER_PARAMS;

    // --- POWER SUPERVISOR ---
    c.ps.enabled    = false;
    c.ps.intervalMs = 5000;
    //c.ps.mainPower = { GenericSensor::Config::Type::BUFFER, AREA_PMETER_VOLTAGE, 0.01f };
    c.ps.i24vOk    = { GenericSensor::Config::Type::DIGITAL, I1 };
    c.ps.fault     = { GenericSensor::Config::Type::DIGITAL, I2 };
    c.ps.battery   = { GenericSensor::Config::Type::DIGITAL, I3 };
    c.ps.i12vOk    = { GenericSensor::Config::Type::DIGITAL, I4 };
    c.ps.mainPowerLow  = 215.0f;
    c.ps.mainPowerHigh = 240.0f;

    // --- SECURITY ---
    c.security.enabled    = false;
    c.security.intervalMs = 2000;
    c.security.startupInhibitMs = 10000;
    //c.security.sensors = WIRED_SENSOR_CONFIG;
    //c.security.count   = sizeof(WIRED_SENSOR_CONFIG) / sizeof(WIRED_SENSOR_CONFIG[0]);

    // --- POWER LIMITS ---
    //c.power = POWER_PARAMS;

    // --- Averages ---
    c.averages.enabled = false;
    c.averages.intervalMs = 15000;   // intervallo task
    //c.averages.gruppi = MEAN_GROUPS;
    //c.averages.gruppiCount = sizeof(MEAN_GROUPS) / sizeof(MEAN_GROUPS[0]);

    // --- DIAGNOSTICS  ---
    c.diagnostic=diagnosticParams;

    // --- WATCH ---
    c.watch.enabled = true;
    c.watch.aree = watchAreas;
    c.watch.count = sizeof(watchAreas) / sizeof(watchAreas[0]);

    // --- WEB API (Fanvil example) ---
    c.webApi.enabled = false;
    c.webApi.intervalMs=1500;
    c.webApi.groups = API_DEVICE_GROUPS;
    c.webApi.groupCount = sizeof(API_DEVICE_GROUPS) / sizeof(API_DEVICE_GROUPS[0]);
    c.webApi.baseUrl = "http://192.168.1.100";   // Fanvil
    c.webApi.timeoutMs = 3000;

    // --- JOBS ---
    c.jobs.enabled = true;
    c.jobs.intervalMs=5500;

    return c;
}();

void setup() {
    Serial.begin(19200);

    //Setup OPTA IO
    pinMode(I1, INPUT);
    pinMode(I2, INPUT);
    pinMode(I3, INPUT);
    pinMode(I4, INPUT);
    
    /* USBConfigLoader::Init([](bool ok){
        if (ok) {
            LOG_I("USB", "USB inizializzato");
        } else {
            LOG_E("USB", "Errore inizializzazione USB");
        }
    });

      🔥 NUOVO: tentiamo di caricare config.json dalla USB
     FrontendConfig cfg;
    if (USBConfigLoader::LoadFromUSB(cfg)) {
        LOG_I("CFG", "Caricata configurazione da USB");
        DomoManagerFrontendEngine::Setup(cfg);

        LOG_I("USB", "Config valida, salvata in flash");
        LOG_I("USB", "Riavvio...");
        NVIC_SystemReset();

    } else {
        LOG_I("CFG", "Uso configurazione interna");
        DomoManagerFrontendEngine::Setup(mainConfig);
    }*/

     DomoManagerFrontendEngine::Setup(mainConfig);
}

void loop() {
    DomoManagerFrontendEngine::loop();
}
