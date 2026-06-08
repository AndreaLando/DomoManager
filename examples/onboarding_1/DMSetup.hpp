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

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ------------------------------------------------------------
// CONFIGURAZIONE DOMOMANAGER
// ------------------------------------------------------------

// ************ IO AREA IDs*******************************
// 0..9 RESERVED
DEFINE_AREA(AREA_DI_BAGNO, 10) 
DEFINE_AREA(AREA_DI2, 11)
DEFINE_AREA(AREA_DI3, 12)
DEFINE_AREA(AREA_DI4, 13)

DEFINE_AREA(AREA_DO1, 14)
DEFINE_AREA(AREA_DO2, 15)
DEFINE_AREA(AREA_DO3, 16)
DEFINE_AREA(AREA_DO_ALIM_BAGNO, 17)

DEFINE_AREA(AREA_DO1A, 18)
DEFINE_AREA(AREA_DO2A, 19)
DEFINE_AREA(AREA_DO3A, 20)

DEFINE_AREA(AREA_DO_STRIP1_BAGNO, 23)
DEFINE_AREA(AREA_DO_STRIP2_BAGNO, 24)

// ************ PHISICAL DEVICES *******************************
arduino::IPAddress WaveShareP1_Addr=IPAddress(192, 168, 12, 203);
arduino::IPAddress WaveSharePT_Addr=IPAddress(192, 168, 12, 204);

static const DomoManagerConfig::Devices mainDevicesConfig = {
    {
        { "Scheda 4DI+4DO - Scala", WaveSharePT_Addr, 2, "MA01_XACX0440",
          { AREA_DI_BAGNO, AREA_DI2, AREA_DI3, AREA_DI4, AREA_DO1, AREA_DO2, AREA_DO3, AREA_DO_ALIM_BAGNO },
          3, High
        },

        { "Scheda 8DO - Fondo P1", WaveShareP1_Addr, 7, "MA01_AXCX0080",
          { AREA_DO1A, AREA_DO2A, AREA_DO3A, 21, 22, AREA_DO_STRIP1_BAGNO, AREA_DO_STRIP2_BAGNO, 25 }, 3, Low
        }
    }
};


// ************ IO AREAS *******************************
static const DomoManagerBufferEngine::AreasConfig mainAreasConfig = {
{
    // **** 4 DI EbYTE ID=2
    { AREA_DI_BAGNO, 0, "Pulsante bagno", {false,false,false}, {-1,-1} },
    { AREA_DI2, 0, "Pulsante Apri Vasistas", {true,true,false}, {-1,-1} },   // forwardArea = 57
    { AREA_DI3, 0,  "Pulsante Chiudi vasistas", {true,true,false}, {-1,-1} },   // -------A
    { AREA_DI4, 25, "Pulsante scala", {true,true,false}, {-1,-1} }, // forwardArea = 59
    // 4 DO EbYTE
    { AREA_DO1, 0, "", {false,false,false}, {-1,-1} },
    { AREA_DO2, 0, "", {false,false,false}, {-1,-1} },
    { AREA_DO3, 0, "", {true,true,false}, {-1,-1} },
    { AREA_DO_ALIM_BAGNO, 0, "Alimentatore LED bagno", {false,false,false}, {-1,-1} },
       
    // **** 8 DO EbYTE ID=7 - Vasistas e scuri
    { AREA_DO1A, 0, "Inv. vasistas sx.", {false,false,false}, {-1,-1} },
    { AREA_DO2A, 0, "Ali. vasistas sx.", {false,false,false}, {-1,-1} },
    { AREA_DO3A, 0, "", {false,false,false}, {-1,-1} },
    { 21, 0, "", {false,false,false}, {-1,-1} },
    { 22, 0, "", {false,false,false}, {-1,-1} },
    { AREA_DO_STRIP1_BAGNO, 0, "Led strip 1", {false,false,false}, {-1,-1} },
    { AREA_DO_STRIP2_BAGNO, 0, "Led strip 2", {false,false,false}, {-1,-1} },
    { 25, 0, "Luce scala", {false,false,false}, {-1,-1} },

     { 26, 0, "Virtual", {false,false,false}, {-1,-1} }
}};

// ************ ROUTES *******************************
static const DomoManagerRouteEngine::RoutesConfig mainRoutesConfig = {
    {
        {
            "Bagno - Interruttore luce",
           AREA_DI_BAGNO,
            {
                // Caso: value == 1
                {
                    1,
                    {
                        { AREA_DO_ALIM_BAGNO, 1 },
                        { AREA_DO_STRIP1_BAGNO, 1 },
                        { AREA_DO_STRIP2_BAGNO, 1 },
                        { 26, 150 }
                    }
                },

                // Caso: value == 0
                {
                    0,
                    {
                        { AREA_DO_ALIM_BAGNO, 0 },
                        { AREA_DO_STRIP1_BAGNO, 0 },
                        { AREA_DO_STRIP2_BAGNO, 0 },
                        { 26, 0 }
                    }
                }
            }
        }
    }
};

// ************ SPLITS  *******************************
static const DomoManagerSplitEngine::SplitsConfig mainSplitsConfig = {
    {
        { AREA_DI2, {AREA_DO1A,AREA_DO2A}, 10000 },
        { AREA_DI3, {AREA_DO1A},     100000 }
    }
};

// ************ TOGGLES *******************************
static const DomoManagerToggleEngine::TogglesConfig mainTogglesConfig = {
    {
        { AREA_DI_BAGNO, {  } }
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

    // --- Devices & Areas ---
    cfg.devices = mainDevicesConfig;
    cfg.areas   = mainAreasConfig;

    // --- Engines ---
    cfg.routes  = mainRoutesConfig;
    cfg.toggles = mainTogglesConfig;
    cfg.splits  = mainSplitsConfig;

    // --- Automation JSON ---
    //cfg.automation.json = AUTOMATION_JSON;

    return cfg;
}

static DomoManagerConfig domoConfig = makeDomoConfig();

#endif
