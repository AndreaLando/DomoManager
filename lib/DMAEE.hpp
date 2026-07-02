#ifndef DMAEE_HPP
#define DMAEE_HPP


/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑04‑24
   Note:
                    • Nessuna

   ============================================================================ */

#include <Arduino.h>
#include <ArduinoJson.h>

#include "DMAEECore.hpp"
#include "DMPLC.h"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


class DMAEE {
public:

    // ------------------------------------------------------------
    // 1) Struttura per gli update AEE
    // ------------------------------------------------------------
    struct Update {
        AEEVariableBase* var;
        float rawValue;
        bool valid;
    };

    // ------------------------------------------------------------
    // 2) Costruzione degli update dal buffer
    // ------------------------------------------------------------
    template<typename ChangedView>
    static bool BuildUpdatesFromBuffer(
        AEEManagement& mgr,
        Buffer& buf,
        const ChangedView& changed,
        std::vector<Update>& out)
    {
        bool hasUpdates = false;

        // Numero aree del buffer
        const int AREA_COUNT = buf.size();

        // Precalcolo aree cambiate → O(1)
        bool areaChanged[AREA_COUNT];
        memset(areaChanged, 0, sizeof(areaChanged));

        for (auto& kv : changed) {
            int area = kv.first / BufferFlagType_Count;
            if ((unsigned)area < AREA_COUNT)
                areaChanged[area] = true;
        }

        for (auto* v : mgr.vars)
        {
            if (v->def.direction == AEEDirection::ModuleToFrontend)
                continue;

            Update u{ v, 0.0f, false };

            switch (v->def.sourceType)
            {
                case AEEVarSourceType::BufferArea:
                {
                    int area = v->def.bufferArea;

                    if ((unsigned)area >= AREA_COUNT)
                        break;

                    if (!areaChanged[area])
                        break;

                    long raw = buf.getValueFast(area);

                    if (v->def.bitIndex >= 0)
                        raw = (raw >> v->def.bitIndex) & 1;

                    u.rawValue = (float)raw;
                    u.valid = true;
                }
                break;

                case AEEVarSourceType::GenericSensor:
                {
                    int raw = GenericSensor::read(v->def.sensorCfg, buf);
                    u.rawValue = (float)raw;
                    u.valid = true;
                }
                break;

                case AEEVarSourceType::Function:
                {
                    if (v->def.fnFloat) {
                        u.rawValue = v->def.fnFloat();
                        u.valid = true;
                    }
                    else if (v->def.fnInt) {
                        u.rawValue = (float)v->def.fnInt();
                        u.valid = true;
                    }
                    else if (v->def.fnBool) {
                        u.rawValue = v->def.fnBool() ? 1.0f : 0.0f;
                        u.valid = true;
                    }
                }
                break;

                default:
                    break;
            }

            if (u.valid) {
                out.push_back(u);
                hasUpdates = true;
            }
        }

        return hasUpdates;
    }


    // ------------------------------------------------------------
    // 3) Applicazione degli update alle variabili AEE
    // ------------------------------------------------------------
    static void ApplyUpdates(
    const std::vector<Update>& updates,
    unsigned long now)
    {
        for (auto& u : updates)
        {
            auto* v = u.var;

            // BOOL
            if (auto* b = as<bool>(v)) {
                bool old = b->get();
                bool nv = (u.rawValue != 0.0f);
                b->set(nv, now);
                if (b->get() != old)
                    LOG_DF("AEE", "Updated %s = %d", v->def.name, nv);
            }

            // INT
            else if (auto* i = as<int>(v)) {
                int old = i->get();
                int nv = (int)(u.rawValue * v->def.scale);
                i->set(nv, now);
                if (i->get() != old)
                    LOG_DF("AEE", "Updated %s = %d", v->def.name, nv);
            }

            // FLOAT
            else if (auto* f = as<float>(v)) {
                float old = f->get();
                float nv = u.rawValue * v->def.scale;
                f->set(nv, now);
                if (f->get() != old)
                    LOG_DF("AEE", "Updated %s = %.2f", v->def.name, nv);
            }
        }
    }

};
#endif
