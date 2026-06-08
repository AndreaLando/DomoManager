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
// Sample DEFINE_AREA(AREA_DI1, 10) 



// ************ PHISICAL DEVICES *******************************
//Sample arduino::IPAddress WaveShareP1_Addr=IPAddress(192, 168, 12, 203);


static const DomoManagerConfig::Devices mainDevicesConfig = {
    {
        
    }
};


// ************ IO AREAS *******************************
static const DomoManagerBufferEngine::AreasConfig mainAreasConfig = {
{
    
}};

// ************ ROUTES, SPLITS and TOGGLES *******************************
static const DomoManagerRouteEngine::RoutesConfig mainRoutesConfig = {
    {
        
    }
};

static const DomoManagerSplitEngine::SplitsConfig mainSplitsConfig = {
    {
        
    }
};

static const DomoManagerToggleEngine::TogglesConfig mainTogglesConfig = {
    {
        
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
