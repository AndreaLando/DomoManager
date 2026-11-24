#ifndef DMAEE_HPP
#define DMAEE_HPP

#pragma once

/* ============================================================================
 * DMAEE.hpp
 *
 * Adapter runtime tra AEEManagement e le sorgenti dati di DomoManager.
 *
 * Responsabilita:
 *   - polling delle sorgenti GenericSensor e Function;
 *   - conversione degli eventi BufferArea in aggiornamenti AEE;
 *   - acquisizione delle BufferArea modificate;
 *   - applicazione degli aggiornamenti alle variabili AEE tipizzate.
 *
 * Il modulo non possiede AEEManagement, Buffer o variabili AEE. Gli oggetti
 * vengono ricevuti per riferimento e restano di proprieta dei rispettivi
 * manager runtime.
 *
 * Autore:          Andrea Lando
 * Contatto:        mail@domo-manager.it
 * Versione modulo: 1.1.0
 * Ultima modifica: 2026-11-21
 * ============================================================================ */

#include <Arduino.h>
#include <vector>

#include "DMAEECore.hpp"
#include "DMBuffers.hpp"
#include "DMBaseClassCore.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================================
// DMAEE
// ============================================================================
class DMAEE
{
public:
    struct Update
    {
        AEEVariableBase* var = nullptr;
        float rawValue = 0.0f;
        bool valid = false;
    };

    // Polling di GenericSensor e Function. Le BufferArea sono escluse.
    static bool BuildUpdatesFromPolledSources(
        AEEManagement& mgr,
        Buffer& buf,
        std::vector<Update>& out)
    {
        bool updated = false;

        for (auto* var : mgr.vars)
        {
            if (!var ||
                var->def.direction == AEEDirection::ModuleToFrontend ||
                var->def.sourceType == AEEVarSourceType::BufferArea)
            {
                continue;
            }

            Update u{var, 0.0f, false};

            switch (var->def.sourceType)
            {
                case AEEVarSourceType::GenericSensor:
                    u.rawValue = GenericSensor::read(var->def.sensorCfg, buf);
                    u.valid = true;
                    break;

                case AEEVarSourceType::Function:
                    if (var->def.fnFloat)
                    {
                        u.rawValue = var->def.fnFloat();
                        u.valid = true;
                    }
                    else if (var->def.fnInt)
                    {
                        u.rawValue = static_cast<float>(var->def.fnInt());
                        u.valid = true;
                    }
                    else if (var->def.fnBool)
                    {
                        u.rawValue = var->def.fnBool() ? 1.0f : 0.0f;
                        u.valid = true;
                    }
                    break;

                default:
                    break;
            }

            if (!u.valid)
                continue;

            out.push_back(u);
            updated = true;
        }

        return updated;
    }

    // Converte un evento BufferArea senza rileggere il Buffer.
    static bool BuildUpdatesFromBufferArea(
        AEEManagement& mgr,
        int area,
        long value,
        std::vector<Update>& out)
    {
        bool updated = false;

        for (auto* var : mgr.vars)
        {
            if (!var ||
                var->def.direction == AEEDirection::ModuleToFrontend ||
                var->def.sourceType != AEEVarSourceType::BufferArea ||
                var->def.bufferArea != area)
            {
                continue;
            }

            long raw = value;
            if (var->def.bitIndex >= 0)
                raw = (raw >> var->def.bitIndex) & 1L;

            out.push_back({var, static_cast<float>(raw), true});
            updated = true;
        }

        return updated;
    }

    // Elabora le BufferArea modificate, quindi esegue il polling delle altre
    // sorgenti. ChangedView deve essere iterabile e contenere indici area.
    template<typename ChangedView>
    static bool BuildUpdatesFromBuffer(
        AEEManagement& mgr,
        Buffer& buf,
        const ChangedView& changed,
        std::vector<Update>& out)
    {
        bool updated = false;
        const size_t areaCount = buf.size();
        std::vector<bool> areaChanged(areaCount, false);

        for (const auto& item : changed)
        {
            const int area = static_cast<int>(item);
            if (area >= 0 && static_cast<size_t>(area) < areaCount)
                areaChanged[static_cast<size_t>(area)] = true;
        }

        for (auto* var : mgr.vars)
        {
            if (!var ||
                var->def.direction == AEEDirection::ModuleToFrontend ||
                var->def.sourceType != AEEVarSourceType::BufferArea)
            {
                continue;
            }

            const int area = var->def.bufferArea;
            if (area < 0 ||
                static_cast<size_t>(area) >= areaCount ||
                !areaChanged[static_cast<size_t>(area)])
            {
                continue;
            }

            long raw = buf.getValueFast(area);
            if (var->def.bitIndex >= 0)
                raw = (raw >> var->def.bitIndex) & 1L;

            out.push_back({var, static_cast<float>(raw), true});
            updated = true;
        }

        return BuildUpdatesFromPolledSources(mgr, buf, out) || updated;
    }

    // Applica gli aggiornamenti alle variabili AEE bool, int e float.
    static void ApplyUpdates(
        const std::vector<Update>& updates,
        unsigned long now)
    {
        for (const auto& u : updates)
        {
            if (!u.valid || !u.var)
                continue;

            auto* var = u.var;

            if (auto* target = as<bool>(var))
            {
                const bool oldValue = target->get();
                const bool newValue = u.rawValue != 0.0f;
                target->set(newValue, now);

                if (target->get() != oldValue)
                    LOG_DF("AEE", "Updated %s = %d", var->def.name, newValue);

                continue;
            }

            if (auto* target = as<int>(var))
            {
                const int oldValue = target->get();
                const int newValue =
                    static_cast<int>(u.rawValue * var->def.scale);
                target->set(newValue, now);

                if (target->get() != oldValue)
                    LOG_DF("AEE", "Updated %s = %d", var->def.name, newValue);

                continue;
            }

            if (auto* target = as<float>(var))
            {
                const float oldValue = target->get();
                const float newValue = u.rawValue * var->def.scale;
                target->set(newValue, now);

                if (target->get() != oldValue)
                    LOG_DF("AEE", "Updated %s = %.2f", var->def.name, newValue);
            }
        }
    }
};

#endif // DMAEE_HPP
