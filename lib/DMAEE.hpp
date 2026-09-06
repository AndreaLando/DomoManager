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

    struct Update {
        AEEVariableBase* var;
        float rawValue;
        bool valid;
    };

    // ------------------------------------------------------------
    // POLLING DELLE SORGENTI NON-BUFFER
    //
    // Gestisce esclusivamente:
    //   - GenericSensor
    //   - Function
    //
    // NON gestisce BufferArea.
    // ------------------------------------------------------------
    static bool BuildUpdatesFromPolledSources(
        AEEManagement& mgr,
        Buffer& buf,
        std::vector<Update>& out)
    {
        bool hasUpdates = false;

        for (auto* v : mgr.vars)
        {
            if (!v)
                continue;

            // Solo DOMO → PEER
            if (v->def.direction == AEEDirection::ModuleToFrontend)
                continue;

            // BufferArea gestite da BuildUpdatesFromBuffer()
            if (v->def.sourceType == AEEVarSourceType::BufferArea)
                continue;

            Update u{ v, 0.0f, false };

            switch (v->def.sourceType)
            {
                case AEEVarSourceType::GenericSensor:
                {
                    int raw =
                        GenericSensor::read(
                            v->def.sensorCfg,
                            buf
                        );

                    u.rawValue = static_cast<float>(raw);
                    u.valid = true;
                }
                break;

                case AEEVarSourceType::Function:
                {
                    if (v->def.fnFloat)
                    {
                        u.rawValue = v->def.fnFloat();
                        u.valid = true;
                    }
                    else if (v->def.fnInt)
                    {
                        u.rawValue =
                            static_cast<float>(
                                v->def.fnInt()
                            );
                        u.valid = true;
                    }
                    else if (v->def.fnBool)
                    {
                        u.rawValue =
                            v->def.fnBool()
                                ? 1.0f
                                : 0.0f;
                        u.valid = true;
                    }
                }
                break;

                default:
                    break;
            }

            if (u.valid)
            {
                out.push_back(u);
                hasUpdates = true;
            }
        }

        return hasUpdates;
    }


    // ------------------------------------------------------------
    // EVENT MANAGER → AEE
    //
    // Gestisce esclusivamente BufferArea.
    // Il valore è già disponibile nell'evento.
    //
    // NON legge il Buffer.
    // ------------------------------------------------------------
    static bool BuildUpdatesFromBufferArea(
        AEEManagement& mgr,
        int area,
        long value,
        std::vector<Update>& out)
    {
        bool hasUpdates = false;

        for (auto* v : mgr.vars)
        {
            if (!v)
                continue;

            if (v->def.direction == AEEDirection::ModuleToFrontend)
                continue;

            if (v->def.sourceType != AEEVarSourceType::BufferArea)
                continue;

            if (v->def.bufferArea != area)
                continue;

            Update u{ v, 0.0f, false };

            long raw = value;

            if (v->def.bitIndex >= 0)
                raw = (raw >> v->def.bitIndex) & 1L;

            u.rawValue = static_cast<float>(raw);
            u.valid = true;

            out.push_back(u);
            hasUpdates = true;
        }

        return hasUpdates;
    }


    // ------------------------------------------------------------
    // BUFFER POLLING COMPLETO
    //
    // Mantiene il comportamento precedente:
    //
    //   BufferArea      → lettura dal Buffer
    //   GenericSensor   → polling
    //   Function        → polling
    //
    // La parte GenericSensor/Function è delegata a
    // BuildUpdatesFromPolledSources().
    // ------------------------------------------------------------
    template<typename ChangedView>
    static bool BuildUpdatesFromBuffer(
        AEEManagement& mgr,
        Buffer& buf,
        const ChangedView& changed,
        std::vector<Update>& out)
    {
        bool hasUpdates = false;

        const int AREA_COUNT = buf.size();

        // --------------------------------------------------------
        // BUFFER AREA
        // --------------------------------------------------------

        bool areaChanged[AREA_COUNT];
        memset(areaChanged, 0, sizeof(areaChanged));

        for (const auto& area : changed)
        {
            if ((unsigned)area < AREA_COUNT)
                areaChanged[area] = true;
        }

        for (auto* v : mgr.vars)
        {
            if (!v)
                continue;

            if (v->def.direction == AEEDirection::ModuleToFrontend)
                continue;

            if (v->def.sourceType != AEEVarSourceType::BufferArea)
                continue;

            const int area = v->def.bufferArea;

            if ((unsigned)area >= AREA_COUNT)
                continue;

            if (!areaChanged[area])
                continue;

            long raw = buf.getValueFast(area);

            if (v->def.bitIndex >= 0)
                raw = (raw >> v->def.bitIndex) & 1L;

            Update u{ v, static_cast<float>(raw), true };

            out.push_back(u);
            hasUpdates = true;
        }

        // --------------------------------------------------------
        // GENERIC SENSOR + FUNCTION
        // --------------------------------------------------------

        if (BuildUpdatesFromPolledSources(
                mgr,
                buf,
                out))
        {
            hasUpdates = true;
        }

        return hasUpdates;
    }


    // ------------------------------------------------------------
    // APPLY
    // ------------------------------------------------------------

    static void ApplyUpdates(
        const std::vector<Update>& updates,
        unsigned long now)
    {
        for (auto& u : updates)
        {
            auto* v = u.var;

            if (auto* b = as<bool>(v))
            {
                bool old = b->get();
                bool nv = (u.rawValue != 0.0f);

                b->set(nv, now);

                if (b->get() != old)
                    LOG_DF(
                        "AEE",
                        "Updated %s = %d",
                        v->def.name,
                        nv
                    );
            }

            else if (auto* i = as<int>(v))
            {
                int old = i->get();
                int nv =
                    static_cast<int>(
                        u.rawValue * v->def.scale
                    );

                i->set(nv, now);

                if (i->get() != old)
                    LOG_DF(
                        "AEE",
                        "Updated %s = %d",
                        v->def.name,
                        nv
                    );
            }

            else if (auto* f = as<float>(v))
            {
                float old = f->get();
                float nv =
                    u.rawValue * v->def.scale;

                f->set(nv, now);

                if (f->get() != old)
                    LOG_DF(
                        "AEE",
                        "Updated %s = %.2f",
                        v->def.name,
                        nv
                    );
            }
        }
    }
};

#endif
