#ifndef DMWebAPIDefs_HPP
#define DMWebAPIDefs_HPP

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

#include "DMWebAPI.hpp"       

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// Profilo Fanvil (Action URL + Active URI)
// ---------------- FANVIL OUT ----------------
static const DeviceMessageProfile::OutMessage fanvil_out[] = {
    { "Incomingcall", "/newcall.xml?num=${call_id}", "GET", nullptr },
    { "DoorSensor1",  "/door1.xml?state=${state}",   "GET", nullptr },
    { "Tamper",       "/tamper.xml?mac=${mac}",      "GET", nullptr },
    { "KEEPALIVE",    "/keepalive.xml?mac=${mac}",   "GET", nullptr }
};

// ---------------- FANVIL IN ----------------
static const DeviceMessageProfile::InField fanvil_in[] = {
    { "key",  "key=(.*)" },
    { "code", "code=(.*)" }
};

// ---------------- FANVIL CORRELATION ----------------
static const DeviceMessageProfile::CorrelationRule fanvil_rules[] = {
    { "Incomingcall", "key", "optional" },
    { "DoorSensor1",  "key", "optional" }
};

// ---------------- FANVIL REGISTRY MAP ----------------
static const DeviceMessageProfile::InToRegistryMap fanvil_regmap[] = {
    { "key",  "fanvil_last_key",  "string" },
    { "code", "fanvil_last_code", "string" }
};

// ---------------- FANVIL STATE MACHINE ----------------
static const DeviceMessageProfile::StateRule fanvil_stateRules[] = {
    { "ONLINE", "timeout", "ONLINE", "KEEPALIVE" },
    { "ONLINE", "key",     "ONLINE", nullptr }
};

// ---------------- FANVIL PROFILE ----------------
static const DeviceMessageProfile fanvil_profile = {
    .profileId = 2001,
    .name = "Fanvil_Doorphone",
    .outMessages = fanvil_out,
    .outCount = 4,
    .inFields = fanvil_in,
    .inCount = 2,
    .rules = fanvil_rules,
    .ruleCount = 2,
    .regMap = fanvil_regmap,
    .regMapCount = 2,
    .stateRules = fanvil_stateRules,
    .stateRuleCount = 2
};

//Profilo 930 X1/X2 (Online, KeepAlive, Batch, BioUpdate)
// ---------------- 930 OUT ----------------
static const DeviceMessageProfile::OutMessage x1x2_out[] = {
    { "ONLINE",    "/online?badge=${transaction}&id=${termid}", "GET", nullptr },
    { "KEEPALIVE", "/keepalive?term=${termid}",                 "GET", nullptr },
    { "BATCH",     "/batch?trsn=${transaction}&id=${termid}",   "GET", nullptr },
    { "BIOUPDATE", "/batch",                                    "POST", nullptr }
};

// ---------------- 930 IN ----------------
static const DeviceMessageProfile::InField x1x2_in[] = {
    { "ack",    "ack=([0-9]+)" },
    { "screen", "screen=(.*)" },
    { "beep",   "beep=([0-9]+)" },
    { "relay",  "relay=(.*)" },
    { "time",   "time=([0-9]{6})" },
    { "date",   "date=([0-9]{8})" }
};

// ---------------- 930 CORRELATION ----------------
static const DeviceMessageProfile::CorrelationRule x1x2_rules[] = {
    { "BATCH",     "ack", "value == 1" },
    { "BIOUPDATE", "ack", "value == 1" }
};

// ---------------- 930 REGISTRY MAP ----------------
static const DeviceMessageProfile::InToRegistryMap x1x2_regmap[] = {
    { "ack",    "x1x2_ack",    "int" },
    { "screen", "x1x2_screen", "string" },
    { "beep",   "x1x2_beep",   "int" },
    { "relay",  "x1x2_relay",  "string" },
    { "time",   "x1x2_time",   "string" },
    { "date",   "x1x2_date",   "string" }
};

// ---------------- 930 STATE MACHINE ----------------
static const DeviceMessageProfile::StateRule x1x2_stateRules[] = {
    { "ONLINE",  "timeout",      "OFFLINE", "KEEPALIVE" },
    { "OFFLINE", "keepalive_ok", "BATCH",   "BATCH"     },
    { "BATCH",   "ack_ok",       "ONLINE",  nullptr     }
};

// ---------------- 930 PROFILE ----------------
static const DeviceMessageProfile x1x2_profile = {
    .profileId = 930,
    .name = "X1X2_HTTP",
    .outMessages = x1x2_out,
    .outCount = 4,
    .inFields = x1x2_in,
    .inCount = 6,
    .rules = x1x2_rules,
    .ruleCount = 2,
    .regMap = x1x2_regmap,
    .regMapCount = 6,
    .stateRules = x1x2_stateRules,
    .stateRuleCount = 3
};

//**************** GRUPPI
static const DeviceMessageProfile fanvil_profiles[] = { fanvil_profile };
static const DeviceMessageProfile x1x2_profiles[] = { x1x2_profile };

static const DeviceMessageGroup API_DEVICE_GROUPS[] = {
    { "FanvilGroup", fanvil_profiles, 1 },
    { "X1X2Group",   x1x2_profiles,   1 }
};

#endif
