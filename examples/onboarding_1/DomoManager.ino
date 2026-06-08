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
        .reportUnusedBufferAreas    = false,
        .reportNeverInitialized     = false,
        .reportMultipleInitialized  = true,
        .reportAutomationConfig     = false,
        .reportLogBuffer            = true
    };

// Watch areas: indices of diagnostic bits to monitor, this stops debugging on SerialMonitor Arduino IDE
static const int watchAreas[] = { 26 };
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

    // --- MODBUS TCP to RTU ---
    c.modbus.timeoutMs = 250; //200ms non scendere mai sotto questo valore 15.05.2026, valore ok=250 cautelativo
    
    // --- DomoManager ---
    c.domoManager=domoConfig;

    // --- DIAGNOSTICS  ---
    c.diagnostic=diagnosticParams;

    // --- WATCH ---
    c.watch.enabled = false;
    c.watch.aree = watchAreas;
    c.watch.count = sizeof(watchAreas) / sizeof(watchAreas[0]);

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
