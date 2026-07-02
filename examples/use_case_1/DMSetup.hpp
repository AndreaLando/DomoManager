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
DEFINE_AREA(AREA_CAMERA_WIN1_ALM, 13) /////////// Per comodita
DEFINE_AREA(AREA_CAMERA_WIN2_ALM, 14)
DEFINE_AREA(AREA_CAMERA_WIN3_ALM, 15)
DEFINE_AREA(AREA_CUCINA_SMOKE_ALM, 16)

DEFINE_AREA(AREA_PMETER_VOLTAGE, 10)
DEFINE_AREA(AREA_PMETER_CURRENT, 11)
DEFINE_AREA(AREA_PMETER_POWER, 12)

DEFINE_AREA(AREA_CAMERA_PIR_ALM, 23)
DEFINE_AREA(AREA_CAMERA_PIR_TAMPER, 24)
DEFINE_AREA(AREA_CAMERA_SMOKE_ALM, 29)

DEFINE_AREA(AREA_INGRESSO_DOOR_ALM, 53)

DEFINE_AREA(AREA_PLAFONIERA_EXT, 59)
DEFINE_AREA(AREA_CUCINA_PIR_ALM, 98)
DEFINE_AREA(AREA_CUCINA_PIR_TAMPER, 99)
DEFINE_AREA(AREA_CUCINA_DOOR_ALM, 100)

DEFINE_AREA(SENSORE_CAMERA_HUM, 118)
DEFINE_AREA(SENSORE_CAMERA_TEMP, 119)
DEFINE_AREA(SENSORE_CAMERA_CO, 120)

DEFINE_AREA(SENSORE_CUCINA_CORRENTE, 122)
DEFINE_AREA(AREA_CUCINA_FLOOD_ALM, 125)

DEFINE_AREA(AREA_ESTRATTORE_CUCINA, 135)
DEFINE_AREA(AREA_INGRESSO_EV_OPEN_DI, 147)

DEFINE_AREA(RELAY_LED_BAGNO, 151)
DEFINE_AREA(AREA_INGRESSO_EV_CLOSE, 152)
DEFINE_AREA(AREA_INGRESSO_EV_OPEN, 153)
DEFINE_AREA(AREA_ESTRATTORE_BAGNO, 154)
DEFINE_AREA(AREA_BAGNO_PIR_ALM, 155)
DEFINE_AREA(AREA_BAGNO_PIR_TAMPER, 156)
DEFINE_AREA(AREA_INGRESSO_PIR_ALM, 157)
DEFINE_AREA(AREA_INGRESSO_PIR_TAMPER, 158)

DEFINE_AREA(AREA_BAGNO_FLOOD_ALM, 167)
DEFINE_AREA(BAGNO_IN_P1, 168)
DEFINE_AREA(BAGNO_IN_P2, 169)
DEFINE_AREA(BAGNO_IN_P3, 170)

DEFINE_AREA(SENSORE_CUCINA_HUM, 188)
DEFINE_AREA(SENSORE_CUCINA_TEMP, 189)
DEFINE_AREA(SENSORE_CUCINA_LUX, 190)

DEFINE_AREA(BAGNO_OUT_LED01B, 191)
DEFINE_AREA(BAGNO_OUT_LED01W, 192)
DEFINE_AREA(BAGNO_OUT_LED01G, 193)
DEFINE_AREA(BAGNO_OUT_LED01R, 194)

DEFINE_AREA(BAGNO_OUT_LED02, 197)

DEFINE_AREA(SENSORE_BAGNO_HUM, 215)
DEFINE_AREA(SENSORE_BAGNO_TEMP, 216)
DEFINE_AREA(SENSORE_BAGNO_LUX, 217)

// VIRTUAL, end physical IO
DEFINE_AREA(AREA_CUCINA_PIR_CMD, 222)
DEFINE_AREA(AREA_CUCINA_DOOR_CMD, 223)
DEFINE_AREA(AREA_CUCINA_SMOKE_CMD, 224)
DEFINE_AREA(AREA_CUCINA_FLOOD_CMD, 225)
DEFINE_AREA(AREA_INGRESSO_PIR_CMD, 226)
DEFINE_AREA(AREA_INGRESSO_DOOR_CMD, 227)
DEFINE_AREA(AREA_CAMERA_SMOKE_CMD, 228)
DEFINE_AREA(AREA_CAMERA_PIR_CMD, 229)
DEFINE_AREA(AREA_CAMERA_WIN1_CMD, 230)
DEFINE_AREA(AREA_CAMERA_WIN2_CMD, 231)
DEFINE_AREA(AREA_CAMERA_WIN3_CMD, 232)
DEFINE_AREA(AREA_BAGNO_FLOOD_CMD, 233)
DEFINE_AREA(AREA_BAGNO_PIR_CMD, 234)

DEFINE_AREA(AREA_BAGNO_ESTRATTORE_BIT, 235)
DEFINE_AREA(AREA_CUCINA_ESTRATTORE_BIT, 236)
DEFINE_AREA(AREA_MEAN_TEMPS, 237)
DEFINE_AREA(AREA_MEAN_HUMS, 238)
DEFINE_AREA(AREA_SECURITY_STATUS, 239)

// ************ PHISICAL DEVICES *******************************
arduino::IPAddress WaveShareP1_Addr=IPAddress(192, 168, 12, 203);
arduino::IPAddress WaveSharePT_Addr=IPAddress(192, 168, 12, 204);
arduino::IPAddress WaveShareCantina_Addr=IPAddress(192, 168, 12, 205);

static const DomoManagerConfig::Devices mainDevicesConfig = {
    {
        // --- P1 ---
        
        { "Lettore consumi - Quadro P1", WaveShareP1_Addr, 1, "LE_01MQ",
          { 10, 11, 12, 13, 14 }, 3, Low
        },

        { "Scheda 4DI+2DO - Fondo P1", WaveShareP1_Addr, 8, "MA01_AXCX4020",
          { 15, 16, 17, 18, 19, 20 }, 3, High
        },

        { "Scheda 32DI - Parete P1", WaveShareP1_Addr, 4, "N4DIH32",
          {
              21, 22, AREA_CAMERA_PIR_ALM, AREA_CAMERA_PIR_TAMPER, 25, 26, 27, 28,
              AREA_CAMERA_SMOKE_ALM, 30, 31, 32, 33, 34, 35, 36,
              37, 38, 39, 40, 41, 42, 43, 44,
              45, 46, 47, 48, 49, 50, 51, 52
          },
          3, High
        },

        { "Scheda 4DI+4DO - Scala", WaveShareP1_Addr, 2, "MA01_XACX0440",
          { AREA_INGRESSO_DOOR_ALM, 54, 55, 56, 57, 58, AREA_PLAFONIERA_EXT, 60 },
          3, High
        },

        { "Scheda 4DI+4DO - Quadro P1", WaveShareP1_Addr, 3, "MA01_XACX0440",
          { 62, 63, 64, 65, 66, 67, 68, 69 }, 3, High
        },

        { "Scheda 4DI+4DO - Parete P1", WaveShareP1_Addr, 5, "MA01_XACX0440",
          { 70, 71, 72, 73, 74, 75, 76, 77 }, 3, Medium
        },

        { "Scheda 32DI - Fondo P1", WaveShareP1_Addr, 6, "N4DIH32",
          {
              78, 79, 80, 81, 82, 83, 84, 85,
              86, 87, 88, 89, 90, 91, 92, 93,
              94, 95, 96, 97, AREA_CUCINA_PIR_ALM, AREA_CUCINA_PIR_TAMPER, AREA_CUCINA_DOOR_ALM, 101, 
              102, 103, 104, 105, 106, 107, 108, 109
          },
          3, High
        },

        { "Scheda 8DO - Fondo P1", WaveShareP1_Addr, 7, "MA01_AXCX0080",
          { 110, 111, 112, 113, 114, 115, 116, 117 }, 3, Low
        },

        { "Sensore CO/Temp/Hum - P1", WaveShareP1_Addr, 10, "CWT_THCO_2K",
          { SENSORE_CAMERA_HUM, SENSORE_CAMERA_TEMP, SENSORE_CAMERA_CO }, 3, Low
        },

        // --- PT ---
    
        { "Sensore Corrente - Quadro cucina PT", WaveSharePT_Addr, 1, "CTR4A01",
          { SENSORE_CUCINA_CORRENTE }, 3, Low
        },

        { "Scheda 4DI+4DO - Quadro cucina PT", WaveSharePT_Addr, 3, "MA01_XACX0440",
          { 123, 124, AREA_CUCINA_FLOOD_ALM, 126, 127, 128, 129, 130 }, 3, High
        },

        { "Scheda 4DI+4DO - Parete cucina PT", WaveSharePT_Addr, 4, "MA01_XACX0440",
          { 131, 132, 133, 134, AREA_ESTRATTORE_CUCINA, 136, 137, 138 }, 3, Low
        },

        { "Scheda 4DI+4DO - Corridoio PT", WaveSharePT_Addr, 5, "MA01_XACX0440",
          { 139, 140, 141, 142, 143, 144, 145, 146 }, 3, Low
        },

        { "Scheda 4AI+4DO - Corridoio PT", WaveSharePT_Addr, 6, "MA01_XACX0440",
          {
              AREA_INGRESSO_EV_OPEN_DI, 148, 149, 150,
              RELAY_LED_BAGNO,
              AREA_INGRESSO_EV_CLOSE, AREA_INGRESSO_EV_OPEN,
              AREA_ESTRATTORE_BAGNO
          },
          3, Low
        },

        { "Scheda 32DI - Corridoio PT", WaveSharePT_Addr, 7, "N4DIH32",
          {
              AREA_BAGNO_PIR_ALM, AREA_BAGNO_PIR_TAMPER,
              AREA_INGRESSO_PIR_ALM, AREA_INGRESSO_PIR_TAMPER,
              159, 160, 161, 162, 163, 164, 165, 166,
              AREA_BAGNO_FLOOD_ALM,
              BAGNO_IN_P1, BAGNO_IN_P2, BAGNO_IN_P3,
              171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
              181, 182, 183, 184, 185, 186
          },
          3, High
        }, 

        { "Sensore Light/Temp/Hum - cucina PT", WaveSharePT_Addr, 10, "CWT_THCO_2K",
          { SENSORE_CUCINA_HUM, SENSORE_CUCINA_TEMP, SENSORE_CUCINA_LUX }, 3, Low
        },

        { "Gestore LED - Bagno", WaveSharePT_Addr, 8, "RESI_LED",
          { 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202 }, 3, Low
        },

        { "Sensore Light/Temp/Hum - Bagno", WaveSharePT_Addr, 11, "CWT_SLTH_6W_S_C",
          { 215, 216, 217 }, 3, Low
        }
    }
};


// ************ IO AREAS *******************************
static const DomoManagerBufferEngine::AreasConfig mainAreasConfig = {
{
    // --- PARTE 1 ---
    // Lettura consumi ID=1
    { AREA_PMETER_VOLTAGE, 0, "Lettura tensione", {true,false,false}, {50,200} },
    { AREA_PMETER_CURRENT, 0, "Lettura corrente", {true,false,false}, {50,100} },
    { AREA_PMETER_POWER,   0, "Lettura consumo W.", {true,false,false}, {400,700} },

    { 13, 0, "Lettura consumo 4", {true,false,false}, {-1,-1} },
    { 14, 0, "Lettura frequenza Hz.", {true,false,false}, {10,20} },

    // **** 4 DI EbYTE ID=8 
    { 15, 0, "", {false,false,false}, {-1,-1} },
    { 16, 0, "Vasistas dx. chiuso", {true,false,false}, {-1,-1} },
    { 17, 0, "", {false,false,false}, {-1,-1} },
    { 18, 0, "Vasistas sx. chiuso", {true,false,false}, {-1,-1} },

    // 2 DO EbYTE ID=8
    { 19, 0, "Luce letto 1", {false,false,false}, {-1,-1} },
    { 20, 0, "Luce letto 2", {false,false,false}, {-1,-1} },

    // **** 32 DI EbYTE ID=4 (da 21 a 52)
    { AREA_CAMERA_PIR_ALM,    0, "PIR Camera Allarme, NEGATO", {true,false,true}, {-1,-1} },
    { AREA_CAMERA_PIR_TAMPER, 0, "Tamper PIR Camera, NEGATO",  {true,false,true}, {-1,-1} },
    { AREA_CAMERA_SMOKE_ALM,  0, "Sensore fumo Camera, NEGATO",{true,false,true}, {-1,-1} },

    { 30, 0, "", {false,false,false}, {-1,-1} },
    // ============================================================
    // ========================   SCALE   ==========================
    // ============================================================

    // **** 4 DI EbYTE ID=2
    { AREA_INGRESSO_DOOR_ALM, 0, "Sensore porta ingresso, NEGATO", {false,false,true}, {-1,-1} },

    // Pulsanti scala
    { 54, 57, "Pulsante scala 3", {true,true,false}, {-1,-1} },   // forwardArea = 57
    { 55, 0,  "Pulsante scala 1", {true,true,false}, {-1,-1} },   // -------A
    { 56, AREA_PLAFONIERA_EXT, "Pulsante scala 2 (Luce PT)", {true,true,false}, {-1,-1} }, // forwardArea = 59

    // 4 DO EbYTE
    { 57, 0, "Luce scale P1", {false,false,false}, {-1,-1} },
    { 58, 0, "Luce scale PT", {false,false,false}, {-1,-1} },
    { AREA_PLAFONIERA_EXT, 0, "Luce Ext.", {true,true,false}, {-1,-1} },
    { 60, 0, "", {false,false,false}, {-1,-1} },
    // ============================================================
    // =======================   QUADRO P1   =======================
    // ============================================================

    // **** 4 DI EbYTE ID=3
    { 62, 0,  "Pulsante chiusura scuri", {true,true,false}, {-1,-1} },
    { 63, 75, "Pulsante trave 2",        {true,true,false}, {-1,-1} },  // forwardArea = 75
    { 64, 76, "Pulsante trave 1",        {true,true,false}, {-1,-1} },  // forwardArea = 76
    { 65, 0,  "Pulsante apertura scuri", {true,true,false}, {-1,-1} },

    // 4 DO EbYTE
    { 66, 0, "Buzzer",          {false,false,false}, {-1,-1} },
    { 67, 0, "Open/close scuro",{false,false,false}, {-1,-1} },
    { 68, 0, "On/off scuro",    {false,false,false}, {-1,-1} },
    { 69, 0, "",                {false,false,false}, {-1,-1} },
    // ============================================================
    // =======================   PARETE P1   =======================
    // ============================================================

    // **** 4 DI EbYTE ID=5
    { 70, 0, "", {false,false,false}, {-1,-1} },
    { 71, 0, "", {false,false,false}, {-1,-1} },
    { 72, 0, "", {false,false,false}, {-1,-1} },
    { 73, 0, "", {false,false,false}, {-1,-1} },

    // 4 DO EbYTE ID=5
    { 74, 0, "DO: Led binario", {false,false,false}, {-1,-1} },
    { 75, 0, "DO: Led trave 2", {false,false,false}, {-1,-1} },
    { 76, 0, "DO: Led trave 1", {false,false,false}, {-1,-1} },
    { 77, 0, "", {false,false,false}, {-1,-1} },
    // ============================================================
    // ===================   PULSANTI LETTO / VASISTAS   ==========
    // ============================================================

    // **** 32 DI EbYTE ID=6 - Pulsanti Letto e sensori
    { 78, 0, "Pulsante Apertura serranda vasistas sx.", {true,true,false}, {-1,-1} },
    { 79, 0, "Pulsante Chiusura serranda vasistas sx.", {true,true,false}, {-1,-1} },
    { 80, 0, "Pulsante Apertura vasistas sx.", {true,true,false}, {-1,-1} },
    { 81, 0, "Pulsante Chiusura vasistas sx.", {true,true,false}, {-1,-1} },

    { 82, 0,  "Pulsante Letto sx. 1", {false,false,false}, {-1,-1} },   // -------A
    { 83, 19, "Pulsante Letto sx. 2", {true,true,false}, {-1,-1} },     // forwardArea = 19

    { 86, 0, "Pulsante Apertura serranda vasistas dx.", {true,true,false}, {-1,-1} },
    { 87, 0, "Pulsante Chiusura serranda vasistas dx.", {true,true,false}, {-1,-1} },
    { 88, 0, "Pulsante Apertura vasistas dx.", {true,true,false}, {-1,-1} },
    { 89, 0, "Pulsante Chiusura vasistas dx.", {true,true,false}, {-1,-1} },

    { 90, 74, "Pulsante Letto dx. 1", {true,true,false}, {-1,-1} },     // forwardArea = 74
    { 91, 20, "Pulsante Letto dx. 2", {true,true,false}, {-1,-1} },     // forwardArea = 20

    { 96, 0, "", {false,false,false}, {-1,-1} },
    { 97, 0, "", {false,false,false}, {-1,-1} },
    // ============================================================
    // ==========================   CUCINA   =======================
    // ============================================================

    { AREA_CUCINA_PIR_ALM,    0, "PIR Cucina Allarme, NEGATO", {true,false,true}, {-1,-1} },
    { AREA_CUCINA_PIR_TAMPER, 0, "Tamper PIR Cucina, NEGATO",  {true,false,true}, {-1,-1} },
    { AREA_CUCINA_DOOR_ALM,   0, "Sensore porta ingresso Cucina, NEGATO", {false,false,true}, {-1,-1} },

    { 101, 0, "", {false,false,false}, {-1,-1} },

    { 106, 0, "Pulsante cucina fondo 1", {true,true,false}, {-1,-1} },
    { 107, 0, "Pulsante cucina fondo 2", {true,true,false}, {-1,-1} },
    { 109, 0, "Pulsante cucina fondo 3", {true,true,false}, {-1,-1} },

    // **** 8 DO EbYTE ID=7 - Vasistas e scuri
    { 110, 0, "Inv. vasistas sx.", {false,false,false}, {-1,-1} },
    { 111, 0, "Ali. vasistas sx.", {false,false,false}, {-1,-1} },
    { 112, 0, "", {false,false,false}, {-1,-1} },
    { 113, 0, "", {false,false,false}, {-1,-1} },
    { 114, 0, "", {false,false,false}, {-1,-1} },
    { 115, 0, "", {false,false,false}, {-1,-1} },
    { 116, 0, "", {false,false,false}, {-1,-1} },
    { 117, 0, "X", {false,false,false}, {-1,-1} },
    // ============================================================
    // =====================   SENSORI CAMERA   ====================
    // ============================================================

    { SENSORE_CAMERA_CO,   0, "Co",          {true,false,false}, {-1,-1} },
    { SENSORE_CAMERA_TEMP, 0, "Temperatura", {true,false,false}, {-1,-1} },
    { SENSORE_CAMERA_HUM,  0, "Umidita",     {true,false,false}, {-1,-1} },
    // ============================================================
    // =======================   CUCINA PT   =======================
    // ============================================================

    { SENSORE_CUCINA_CORRENTE, 0, "Sensore corrente Induzione", {false,false,false}, {-1,-1} },

    // **** 4 DI EbYTE
    { 123, 127, "Pulsante Cucina 1", {true,true,false}, {-1,-1} },   // forwardArea = 127
    { 124, 129, "Pulsante Cucina 2", {true,true,false}, {-1,-1} },   // forwardArea = 129

    { AREA_CUCINA_FLOOD_ALM, 0, "Allagamento Cucina", {false,false,false}, {-1,-1} },
    { 126, 0, "", {false,false,false}, {-1,-1} },

    // **** 4 DO EbYTE
    { 127, 0, "Luce cucina 1", {false,false,false}, {-1,-1} },
    { 128, 0, "Bluetooth",     {true,true,false}, {-1,-1} },
    { 129, 0, "Luce cucina 2", {false,false,false}, {-1,-1} },
    { 130, 0, "", {false,false,false}, {-1,-1} },

    // **** 4 DI EbYTE ID=4 Parete Cucina
    { 131, 0, "Valvola calorifero bagno NC", {true,false,false}, {-1,-1} },
    { 132, 0, "Valvola calorifero bagno NO", {true,false,false}, {-1,-1} },
    { 133, 0, "", {false,false,false}, {-1,-1} },
    { 134, 0, "", {false,false,false}, {-1,-1} },

    // **** 4 DO EbYTE ID=4
    { AREA_ESTRATTORE_CUCINA, 0, "DO: Estrattore", {false,false,false}, {-1,-1} },
    { 136, 0, "DO: Valvola calorifero bagno ON",  {true,false,false}, {-1,-1} },
    { 137, 0, "DO: Valvola calorifero bagno OFF", {true,false,false}, {-1,-1} },
    { 138, 0, "DO: Led Fondo", {true,true,false}, {-1,-1} },

    // **** 4 AI EbYTE ID=5 Corridoio PT
    { 139, 0, "Pressione rete H2O", {false,false,false}, {-1,-1} },
    { 140, 0, "", {false,false,false}, {-1,-1} },
    { 141, 0, "", {false,false,false}, {-1,-1} },
    { 142, 0, "", {false,false,false}, {-1,-1} },

    // **** 4 DO
    { 143, 0, "Luci??", {false,false,false}, {-1,-1} },
    { 144, 0, "", {false,false,false}, {-1,-1} },
    { 145, 0, "", {false,false,false}, {-1,-1} },
    { 146, 0, "", {false,false,false}, {-1,-1} },

    // **** 4 DI EbYTE ID=6
    { AREA_INGRESSO_EV_OPEN_DI, 0, "Valvola acqua casa APERTA", {true,false,false}, {-1,-1} },
    { 148, 0, "", {false,false,false}, {-1,-1} },
    { 149, 0, "", {false,false,false}, {-1,-1} },
    { 150, 0, "", {false,false,false}, {-1,-1} },

    // **** 4 DO
    { RELAY_LED_BAGNO,      0, "Alimentatore LED bagno", {false,false,false}, {-1,-1} },
    { AREA_INGRESSO_EV_CLOSE, 0, "Valvola acqua casa Alim (ON-Chiusa)", {true,true,false}, {-1,-1} },
    { AREA_INGRESSO_EV_OPEN,  0, "Valvola acqua casa Aperta",          {true,true,false}, {-1,-1} },
    { AREA_ESTRATTORE_BAGNO,  0, "Estrattore bagno",                   {true,true,false}, {-1,-1} },
    // ============================================================
    // ==========================   BAGNO   ========================
    // ============================================================

    // **** 32 DI EbYTE ID=7
    { AREA_BAGNO_PIR_ALM,    0, "PIR Bagno Allarme, NEGATO", {true,false,true}, {-1,-1} },
    { AREA_BAGNO_PIR_TAMPER, 0, "Tamper PIR Bagno, NEGATO",  {true,false,true}, {-1,-1} },
    { AREA_INGRESSO_PIR_ALM, 0, "PIR Ingresso Allarme, NEGATO", {true,false,true}, {-1,-1} },
    { AREA_INGRESSO_PIR_TAMPER, 0, "Tamper PIR Ingresso, NEGATO", {true,false,true}, {-1,-1} },

    { 166, 0, "Pulsante bagno", {true,true,false}, {-1,-1} },

    { AREA_BAGNO_FLOOD_ALM, 0, "DI: Sensore allagamento bagno", {true,false,false}, {-1,-1} },

    { BAGNO_IN_P1, 0, "DI: Int. Luce bagno",     {true,true,false}, {-1,-1} },
    { BAGNO_IN_P2, 0, "DI: Int. Luce corridoio", {true,true,false}, {-1,-1} },
    { BAGNO_IN_P3, 0, "Int. Luce ingresso",      {true,true,false}, {-1,-1} },

    // Sensori bagno
    { SENSORE_CUCINA_HUM,  0, "Umidita",     {true,false,false}, {-1,-1} },
    { SENSORE_CUCINA_TEMP, 0, "Temperatura", {true,false,false}, {-1,-1} },
    { SENSORE_CUCINA_LUX,  0, "Luminosita",  {true,false,false}, {-1,-1} },

    // RESI LED ID=8
    { BAGNO_OUT_LED01B, 0, "Bagno LED B",    {true,true,false}, {-1,-1} },
    { BAGNO_OUT_LED01W, 0, "Bagno LED W",    {true,true,false}, {-1,-1} },
    { BAGNO_OUT_LED01G, 0, "Bagno LED G",    {true,true,false}, {-1,-1} },
    { BAGNO_OUT_LED01R, 0, "Bagno LED R",    {true,true,false}, {-1,-1} },
    { BAGNO_OUT_LED02,  0, "Bagno LED MAIN", {true,true,false}, {-1,-1} },

    // Sensori bagno ID=11
    { SENSORE_BAGNO_LUX, 0, "Lux",         {true,false,false}, {-1,-1} },
    { SENSORE_BAGNO_TEMP,0, "Temperatura", {true,false,false}, {-1,-1} },
    { SENSORE_BAGNO_HUM, 0, "Umidita",     {true,false,false}, {-1,-1} },
    // ============================================================
    // ======================   AREE VIRTUALI   ====================
    // ============================================================

    { AREA_CUCINA_ESTRATTORE_BIT, 0, "", {true,true,false}, {-1,-1} },
    { AREA_BAGNO_ESTRATTORE_BIT,  0, "", {true,true,false}, {-1,-1} },
    { AREA_SECURITY_STATUS,       0, "", {true,false,false}, {-1,-1} }
}};

// ************ ROUTES, SPLITS and TOGGLES *******************************
static const DomoManagerRouteEngine::RoutesConfig mainRoutesConfig = {
    {
        {
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
        }
    }
};

static const DomoManagerSplitEngine::SplitsConfig mainSplitsConfig = {
    {
        { 78, {114,115}, 30000 },
        { 79, {114},     30000 },
        { 80, {113},     30000 },
        { 81, {113,112}, 30000 },

        { 86, {116,117}, 30000 },
        { 87, {116},     30000 },
        { 88, {111},     30000 },
        { 89, {110,111}, 30000 }
    }
};

static const DomoManagerToggleEngine::TogglesConfig mainTogglesConfig = {
    {
        { 54, { BAGNO_IN_P2 } },
        { 90, { 82, 55 } },

        { 63, {} },
        { 64, {} },
        { 91, {} },
        { 56, {} },
        { 123, {} },
        { 124, {} },

        { BAGNO_IN_P1, {} },
        { BAGNO_IN_P3, {} }
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
    {
      "name": "BagnoUmido",
      "actions": [
        { "area": "AREA_ESTRATTORE_BAGNO", "value": 1 }
      ]
    },
    {
      "name": "BagnoNormale",
      "actions": [
        { "area": "AREA_ESTRATTORE_BAGNO", "value": 0 }
      ]
    },
    {
        "name": "LightsOff",
        "actions": [
            { "area": "AREA_PLAFONIERA_EXT", "value": 0 },
            { "area": "RELAY_LED_BAGNO", "value": 0 }
        ]
    }

  ],

  "rules": [
    {
      "name": "RegolaBagnoUmido",
      "type": "trend",
      "intervalMs": 20000,
      "trend": {
        "area": "SENSORE_BAGNO_HUM",
        "scale": 10,
        "threshold": 65,
        "trend": "rising",
        "tofMinutes": 5
      },
      "sceneTrue": "BagnoUmido",
      "sceneFalse": "BagnoNormale"
    },

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
    },

    {
        "name": "AutoOff",
        "type": "composite",
        "intervalMs": 300000,

        "composite": {
            "logic": "AND",

            "inputs": [
            { "type": "simple", "area": "AREA_CUCINA_PIR_ALM" },
            { "type": "simple", "area": "AREA_CAMERA_PIR_ALM" },
            { "type": "simple", "area": "AREA_CAMERA_WIN1_ALM" },
            { "type": "simple", "area": "AREA_CAMERA_WIN2_ALM" },
            { "type": "simple", "area": "AREA_CAMERA_WIN3_ALM" },
            { "type": "simple", "area": "AREA_INGRESSO_DOOR_ALM" },
            { "type": "simple", "area": "AREA_CUCINA_DOOR_ALM" }
            ],

            "output": {
            "area": "AREA_PLAFONIERA_EXT",
            "bitIndex": 0
            }
        },

        "sceneTrue": "LightsOff",
        "sceneFalse": "NoAction"
    }

  ],

  "sequences": []
}
)json";

// ************ LOADER *******************************
DomoManagerConfig makeDomoConfig() {
    DomoManagerConfig cfg;   // usa tutti i default della struct

    cfg.hmi.enabled = true;   // opzionale, è già default
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

    // ============================
    // CUCINA
    // ============================

    // PIR CUCINA
    { "Cucina",
        {
            SensorChannel(-1, RT_DELAY, SensorChannelType::RT),
            SensorChannel(-1, INITIAL_DELAY,      SensorChannelType::H24)
        },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CUCINA_PIR_ALM) != 0; },
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CUCINA_PIR_TAMPER) != 0; }
        },
        SensorCategory::PIR
    },

    // Porta Cucina
    { "Cucina",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CUCINA_DOOR_ALM) != 0; }
        },
        SensorCategory::DOOR
    },

    // Flood Cucina
    { "Cucina",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CUCINA_FLOOD_ALM) != 0; }
        },
        SensorCategory::FLOOD
    },

    // Smoke Cucina
    { "Cucina",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CUCINA_SMOKE_ALM) != 0; }
        },
        SensorCategory::SMOKE
    },

    // ============================
    // CAMERA
    // ============================
    { "Camera",
        {
            SensorChannel(-1, RT_DELAY, SensorChannelType::RT),
            SensorChannel(-1, INITIAL_DELAY,      SensorChannelType::H24)
        },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CAMERA_PIR_ALM) != 0; },
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CAMERA_PIR_TAMPER) != 0; }
        },
        SensorCategory::PIR
    },

    // Finestre Camera
    { "Camera",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CAMERA_WIN1_ALM) != 0; }
        },
        SensorCategory::WINDOW
    },

    { "Camera",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CAMERA_WIN2_ALM) != 0; }
        },
        SensorCategory::WINDOW
    },

    { "Camera",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CAMERA_WIN3_ALM) != 0; }
        },
        SensorCategory::WINDOW
    },

    // Smoke Camera
    { "Camera",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_CAMERA_SMOKE_ALM) != 0; }
        },
        SensorCategory::SMOKE
    },

    // ============================
    // BAGNO
    // ============================
    { "Bagno",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_BAGNO_FLOOD_ALM) != 0; }
        },
        SensorCategory::FLOOD
    },

    { "Bagno",
        {
            SensorChannel(-1, RT_DELAY, SensorChannelType::RT),
            SensorChannel(-1, INITIAL_DELAY,      SensorChannelType::H24)
        },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_BAGNO_PIR_ALM) != 0; },
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_BAGNO_PIR_TAMPER) != 0; }
        },
        SensorCategory::PIR
    },

    // ============================
    // INGRESSO
    // ============================
    { "Ingresso",
        { SensorChannel(-1, RT_DELAY, SensorChannelType::RT) },
        {
            [](){ return DomoManager::instance->getBuffer().getValueFast(AREA_INGRESSO_DOOR_ALM) != 0; }
        },
        SensorCategory::DOOR
    }
};

// ---------------------------------------------------------------------------
// MQTT variables exported by the frontend configuration
// Each entry maps a buffer area or sensor to an MQTT-friendly variable.
// Fields: id, friendly name, unit, HA type, source area, scale factor.
// ---------------------------------------------------------------------------
static const FrontendConfig::MQTT::Var MQTT_VARS[] = {
    {
        "temp_cucina",
        "Temperatura Cucina",
        "°C",
        MQTTEngine_Var::HAType::SENSOR,
        SENSORE_CUCINA_TEMP,   // area buffer
        0.1f                   // /10
    },
    {
        "porta_ingresso",
        "Porta Ingresso",
        nullptr,
        MQTTEngine_Var::HAType::BINARY_SENSOR,
        AREA_INGRESSO_DOOR_ALM,
        1.0f
    },
    {
        "tapparella_sala",
        "Tapparella Sala",
        nullptr,
        MQTTEngine_Var::HAType::COVER,
        16,                    // area buffer posizione tapparella
        1.0f
    }
};

// ------------------------------------------------------------
// GENERIC SENSOR CONFIGURATION
// These configs tell GenericSensor where to read values from.
// Type::BUFFER means read from Buffer area.
// ------------------------------------------------------------
static GenericSensor::Config tempSensorCfg  = { GenericSensor::Config::Type::BUFFER, AREA_PMETER_CURRENT };
static GenericSensor::Config windSensorCfg  = { GenericSensor::Config::Type::BUFFER, AREA_PMETER_CURRENT };
static GenericSensor::Config rainSensorCfg  = { GenericSensor::Config::Type::BUFFER, AREA_PMETER_CURRENT };
static GenericSensor::Config lightSensorCfg = { GenericSensor::Config::Type::BUFFER, AREA_PMETER_CURRENT };

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
    { "Giorno", 22.0, SENSORE_CUCINA_TEMP, 1  },
    { "Notte", 20.0, SENSORE_CAMERA_TEMP, 1  },
    { "Bagno", 23.0, SENSORE_BAGNO_TEMP, 1  }
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
    { SENSORE_CAMERA_TEMP,  0.1f },
    { SENSORE_BAGNO_TEMP,   0.1f },
    { SENSORE_CUCINA_TEMP,  0.1f }
};

// Humidity sensors group
static const FrontendConfig::Averages::Sensore sensoriHum[] = {
    { SENSORE_CAMERA_HUM,  0.1f },
    { SENSORE_BAGNO_HUM,   0.1f },
    { SENSORE_CUCINA_HUM,  0.1f }
};

// Groups definition: name, outScale, output area, sensors array, sensor count
static const FrontendConfig::Averages::Gruppo MEAN_GROUPS[] = {
    {
        "Temperature",        // nome gruppo
        10.0f,                // outScale (scrittura *10)
        AREA_MEAN_TEMPS,      // area di output
        sensoriTemp,          // array sensori
        sizeof(sensoriTemp) / sizeof(sensoriTemp[0])
    },
    {
        "Umidita",
        10.0f,
        AREA_MEAN_HUMS,
        sensoriHum,
        sizeof(sensoriHum) / sizeof(sensoriHum[0])
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
        AREA_SECURITY_STATUS, 0,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::BOOL },

    { "allarmeAllagamento", AEEDirection::FrontendToModule,
        AEEVarSourceType::BufferArea,
        AREA_SECURITY_STATUS, 2,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::BOOL },

    { "allarmeFumo", AEEDirection::FrontendToModule,
        AEEVarSourceType::BufferArea,
        AREA_SECURITY_STATUS, 3,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::BOOL },

    { "porteAperte", AEEDirection::FrontendToModule,
        AEEVarSourceType::BufferArea,
        AREA_SECURITY_STATUS, 5,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::BOOL },

    { "finestreAperte", AEEDirection::FrontendToModule,
        AEEVarSourceType::BufferArea,
        AREA_SECURITY_STATUS, 4,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::BOOL },

    { "systemStatus", AEEDirection::FrontendToModule,
        AEEVarSourceType::BufferArea,
        DeviceManager::AREA_SYSTEM_ERRORS, -1,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::BOOL },

    // ============================================================
    // PRESENZA (modulo → frontend)
    // ============================================================
    { "arrivoCasa", AEEDirection::ModuleToFrontend,
        AEEVarSourceType::None,
        -1, -1,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::INT },

    { "proximity", AEEDirection::ModuleToFrontend,
        AEEVarSourceType::None,
        -1, -1,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::BOOL },

    // ============================================================
    // HVAC (funzioni)
    // ============================================================
    { "temperaturaMediaInterna", AEEDirection::FrontendToModule,
        AEEVarSourceType::Function,
        -1, -1,
        {},
        [](){ return DomoManager::instance->getAverages().groupAverage("Temperature"); },
        nullptr,
        nullptr,
        AEEVarType::FLOAT,
        0.3f   // 🔥 minDelta = 0.3°C
    },

    { "umiditaMedia", AEEDirection::FrontendToModule,
        AEEVarSourceType::Function,
        -1, -1,
        {},
        [](){ return DomoManager::instance->getAverages().groupAverage("Umidita"); },
        nullptr,
        nullptr,
        AEEVarType::FLOAT, 
        0.5f   // 🔥 minDelta = 0.3°C
    },

    // ============================================================
    // POWER (buffer)
    // ============================================================
    { "gridPower", AEEDirection::FrontendToModule,
        AEEVarSourceType::BufferArea,
        AREA_PMETER_POWER, -1,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::FLOAT,
        50.0f,   // 🔥 minDelta = 50 W
        0.01f    //Scala la variabile 2 decimale
    },

    // ============================================================
    // TIME (modulo → frontend)
    // ============================================================
    { "epoch", AEEDirection::ModuleToFrontend,
        AEEVarSourceType::None,
        -1, -1,
        {},
        nullptr, nullptr, nullptr,
        AEEVarType::INT, 0.0f },
        
};


#endif
