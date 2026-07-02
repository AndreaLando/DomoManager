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
#include "DMMQTTEngine.hpp"
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
//DEFINE_AREA(AREA_CAMERA_WIN1_ALM, 13) 
//DEFINE_AREA(AREA_CAMERA_WIN2_ALM, 14)
//DEFINE_AREA(AREA_CAMERA_WIN3_ALM, 15)
//DEFINE_AREA(AREA_CUCINA_SMOKE_ALM, 16)

//DEFINE_AREA(AREA_PMETER_VOLTAGE, 10)
//DEFINE_AREA(AREA_PMETER_CURRENT, 11)
//DEFINE_AREA(AREA_PMETER_POWER, 12)



// ************ PHISICAL DEVICES *******************************
arduino::IPAddress WaveShareP1_Addr=IPAddress(192, 168, 12, 203);

static const DomoManagerConfig::Devices mainDevicesConfig = {
    {
        /*
        
        { "Lettore consumi - Quadro P1", WaveShareP1_Addr, 1, "LE_01MQ",
          { 10, 11, 12, 13, 14 }, 3, Low
        },

        { "Scheda 4DI+2DO - Fondo P1", WaveShareP1_Addr, 8, "MA01_AXCX4020",
          { 15, 16, 17, 18, 19, 20 }, 3, High
        }
        */
  
    }
};


// ************ IO AREAS *******************************
static const DomoManagerBufferEngine::AreasConfig mainAreasConfig = {
{
    // --- PARTE 1 ---
    // Lettura consumi ID=1
    //{ AREA_PMETER_VOLTAGE, 0, "Lettura tensione", {true,false,false}, {50,200} },
    //{ AREA_PMETER_CURRENT, 0, "Lettura corrente", {true,false,false}, {50,100} },
    //{ AREA_PMETER_POWER,   0, "Lettura consumo W.", {true,false,false}, {400,700} },

    //{ 13, 0, "Lettura consumo 4", {true,false,false}, {-1,-1} },
    //{ 14, 0, "Lettura frequenza Hz.", {true,false,false}, {10,20} },

    
}};

// ************ ROUTES, SPLITS and TOGGLES *******************************
static const DomoManagerRouteEngine::RoutesConfig mainRoutesConfig = {
    {
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
                        { BAGNO_OUT_LED02, 4095 }
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
                        { BAGNO_OUT_LED02, 0 }
                    }
                }
            }
        }*/
    }
};

static const DomoManagerSplitEngine::SplitsConfig mainSplitsConfig = {
    {
       /* { 78, {114,115}, 30000 },
        { 79, {114},     30000 },
        { 80, {113},     30000 },
        { 81, {113,112}, 30000 },

        { 86, {116,117}, 30000 },
        { 87, {116},     30000 },
        { 88, {111},     30000 },
        { 89, {110,111}, 30000 } */
    }
};

static const DomoManagerToggleEngine::TogglesConfig mainTogglesConfig = {
    {
      /*  { 54, { BAGNO_IN_P2 } },
        { 90, { 82, 55 } },

        { 63, {} },
        { 64, {} },
        { 91, {} },
        { 56, {} },
        { 123, {} },
        { 124, {} },

        { BAGNO_IN_P1, {} },
        { BAGNO_IN_P3, {} } */
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


// ************ LOADER *******************************
DomoManagerConfig makeDomoConfig() {
    DomoManagerConfig cfg;   // usa tutti i default della struct

    cfg.hmi.enabled = false;   // opzionale, è già default
    cfg.hmi.port = 502;        // default
    cfg.hmi.pollingMs = 500;   // default

    // --- Modbus ---
    cfg.modbusRTU.port = 502;

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
    //cfg.automation.json = AUTOMATION_JSON;

    return cfg;
}

static DomoManagerConfig domoConfig = makeDomoConfig();



#endif
