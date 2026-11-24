
#ifndef DMFrontendEngines_HPP
#define DMFrontendEngines_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑05‑24
   Note:
                    • Nessuna

   ============================================================================ */


#include <Arduino.h>

#include "DMDeclares.h"
#include "DMMQTT.hpp"
#include "DMFrontendOrchestrators.hpp"
#include "DMIntrospection.hpp"
#include "DMSemanticResolver.hpp"
#include "DMIntrusion.hpp"
#include "DMPlatformNetwork.hpp"
#include "DMPlatformModbus.hpp"
#include "DMOnline.hpp"
#include "DMAdapterUDP.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


// ============================================================================
// FRONTEND STATUS
//
// Rappresenta lo stato tecnico comune di un frontend.
//
// La classe:
//   - non dipende da DomoManager;
//   - non conosce aree buffer;
//   - non conosce il protocollo specifico;
//   - non esegue pubblicazioni;
//   - non usa allocazione dinamica;
//   - non usa funzioni virtuali;
//   - occupa pochi byte;
//   - segnala quando la status word cambia.
//
// Ogni Engine mantiene una o più istanze FrontendStatus e traduce
// autonomamente il proprio stato runtime nei flag comuni.
//
// Layout status word:
//
//   bit 0 = Enabled
//   bit 1 = Running
//   bit 2 = Initialized
//   bit 3 = Connected
//   bit 4 = Active
//   bit 5 = Recovering
//   bit 6 = Degraded
//   bit 7 = Fault
//
//   bit 8..15 = Reserved
//
// I bit riservati non devono essere utilizzati dai singoli frontend
// senza estendere formalmente il contratto comune.
// ============================================================================

class FrontendStatus
{
public:

    // ========================================================================
    // TYPES
    // ========================================================================

    using Word =
        uint16_t;

    // ========================================================================
    // COMMON FLAGS
    //
    // bit 0..7  = stato comune dei frontend
    // bit 8..15 = riservati
    // ========================================================================

    enum class Flag : uint8_t
    {
        Enabled     = 0,
        Running     = 1,
        Initialized = 2,
        Connected   = 3,
        Active      = 4,
        Recovering  = 5,
        Degraded    = 6,
        Fault       = 7
    };

    // ========================================================================
    // MASKS
    //
    // Utilizzabili anche senza costruire un'istanza FrontendStatus.
    // ========================================================================

    static constexpr Word
        ENABLED_MASK =
            static_cast<Word>(
                1U << 0
            );

    static constexpr Word
        RUNNING_MASK =
            static_cast<Word>(
                1U << 1
            );

    static constexpr Word
        INITIALIZED_MASK =
            static_cast<Word>(
                1U << 2
            );

    static constexpr Word
        CONNECTED_MASK =
            static_cast<Word>(
                1U << 3
            );

    static constexpr Word
        ACTIVE_MASK =
            static_cast<Word>(
                1U << 4
            );

    static constexpr Word
        RECOVERING_MASK =
            static_cast<Word>(
                1U << 5
            );

    static constexpr Word
        DEGRADED_MASK =
            static_cast<Word>(
                1U << 6
            );

    static constexpr Word
        FAULT_MASK =
            static_cast<Word>(
                1U << 7
            );

    static constexpr Word
        COMMON_MASK =
            0x00FFU;

    static constexpr Word
        RESERVED_MASK =
            0xFF00U;

private:

    // ========================================================================
    // CURRENT STATUS WORD
    //
    // Non utilizzare il nome "word":
    //
    // Arduino.h definisce:
    //
    //   #define word(...) makeWord(__VA_ARGS__)
    //
    // Il nome statusWord evita il conflitto con la macro Arduino.
    // ========================================================================

    Word
        statusWord;

    // ========================================================================
    // LAST CONSUMED STATUS WORD
    //
    // Contiene l'ultima word consegnata al consumer tramite
    // ConsumeChanged().
    // ========================================================================

    Word
        consumedStatusWord;

    // ========================================================================
    // CHANGE STATE
    //
    // Indica se almeno una versione della word è già stata consumata.
    // ========================================================================

    bool
        consumed;

public:

    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    constexpr FrontendStatus()
        : statusWord(0),
          consumedStatusWord(0),
          consumed(false)
    {
    }

    // ========================================================================
    // MASK
    // ========================================================================

    static constexpr Word Mask(
        Flag flag)
    {
        return static_cast<Word>(
            1U <<
            static_cast<uint8_t>(
                flag
            )
        );
    }

    // ========================================================================
    // SANITIZE
    //
    // Conserva esclusivamente i bit comuni 0..7.
    //
    // I bit riservati 8..15 vengono sempre azzerati dalle operazioni
    // SetWord(), Reset(initialWord) e SetMasked().
    // ========================================================================

    static constexpr Word Sanitize(
        Word value)
    {
        return static_cast<Word>(
            value &
            COMMON_MASK
        );
    }

    // ========================================================================
    // RESET
    //
    // Azzera tutti i flag e forza la successiva ConsumeChanged() a
    // restituire la word corrente.
    // ========================================================================

    void Reset()
    {
        statusWord =
            0;

        consumedStatusWord =
            0;

        consumed =
            false;
    }

    // ========================================================================
    // RESET WITH INITIAL WORD
    //
    // Imposta una word iniziale, eliminando eventuali bit riservati.
    //
    // La successiva ConsumeChanged() restituisce sempre la word.
    // ========================================================================

    void Reset(
        Word initialWord)
    {
        statusWord =
            Sanitize(
                initialWord
            );

        consumedStatusWord =
            0;

        consumed =
            false;
    }

    // ========================================================================
    // SET FLAG
    //
    // Imposta o azzera un singolo flag.
    //
    // Restituisce:
    //   true  = status word modificata;
    //   false = nessuna modifica.
    // ========================================================================

    bool Set(
        Flag flag,
        bool value)
    {
        const Word mask =
            Mask(
                flag
            );

        const Word previousStatusWord =
            statusWord;

        if (value)
        {
            statusWord =
                static_cast<Word>(
                    statusWord |
                    mask
                );
        }
        else
        {
            statusWord =
                static_cast<Word>(
                    statusWord &
                    static_cast<Word>(
                        ~mask
                    )
                );
        }

        return
            statusWord !=
            previousStatusWord;
    }

    // ========================================================================
    // SET MULTIPLE FLAGS
    //
    // Aggiorna atomisticamente più flag.
    //
    // mask:
    //   identifica i bit da modificare.
    //
    // values:
    //   contiene i nuovi valori dei bit selezionati.
    //
    // Esempio:
    //
    //   SetMasked(
    //       RUNNING_MASK |
    //       INITIALIZED_MASK,
    //       RUNNING_MASK
    //   );
    //
    // Risultato:
    //   Running     = true
    //   Initialized = false
    //
    // I bit riservati vengono ignorati.
    // ========================================================================

    bool SetMasked(
        Word mask,
        Word values)
    {
        mask =
            Sanitize(
                mask
            );

        values =
            Sanitize(
                values
            );

        const Word previousStatusWord =
            statusWord;

        statusWord =
            static_cast<Word>(
                (
                    statusWord &
                    static_cast<Word>(
                        ~mask
                    )
                ) |
                (
                    values &
                    mask
                )
            );

        return
            statusWord !=
            previousStatusWord;
    }

    // ========================================================================
    // SET ENTIRE WORD
    //
    // Sostituisce integralmente lo stato comune.
    //
    // I bit riservati vengono sempre azzerati.
    // ========================================================================

    bool SetWord(
        Word value)
    {
        const Word sanitizedValue =
            Sanitize(
                value
            );

        if (
            statusWord ==
            sanitizedValue)
        {
            return false;
        }

        statusWord =
            sanitizedValue;

        return true;
    }

    // ========================================================================
    // CLEAR FLAG
    // ========================================================================

    bool Clear(
        Flag flag)
    {
        return Set(
            flag,
            false
        );
    }

    // ========================================================================
    // CLEAR ALL FLAGS
    //
    // Diversamente da Reset(), conserva lo stato di consumo.
    //
    // Se la word precedente era diversa da zero, HasChanged() rileverà
    // normalmente la variazione.
    // ========================================================================

    bool ClearAll()
    {
        return SetWord(
            0
        );
    }

    // ========================================================================
    // FLAG ACCESS
    // ========================================================================

    bool IsSet(
        Flag flag) const
    {
        return
            (
                statusWord &
                Mask(flag)
            ) != 0;
    }

    // ========================================================================
    // MASK ACCESS
    //
    // Restituisce true quando tutti i bit specificati nella mask
    // risultano impostati.
    // ========================================================================

    bool HasAll(
        Word mask) const
    {
        mask =
            Sanitize(
                mask
            );

        return
            (
                statusWord &
                mask
            ) ==
            mask;
    }

    // ========================================================================
    // ANY MASK ACCESS
    //
    // Restituisce true quando almeno uno dei bit specificati nella mask
    // risulta impostato.
    // ========================================================================

    bool HasAny(
        Word mask) const
    {
        mask =
            Sanitize(
                mask
            );

        return
            (
                statusWord &
                mask
            ) != 0;
    }

    // ========================================================================
    // WORD ACCESS
    // ========================================================================

    Word GetWord() const
    {
        return
            statusWord;
    }

    // ========================================================================
    // COMMON ACCESSORS
    // ========================================================================

    bool IsEnabled() const
    {
        return IsSet(
            Flag::Enabled
        );
    }

    bool IsRunning() const
    {
        return IsSet(
            Flag::Running
        );
    }

    bool IsInitialized() const
    {
        return IsSet(
            Flag::Initialized
        );
    }

    bool IsConnected() const
    {
        return IsSet(
            Flag::Connected
        );
    }

    bool IsActive() const
    {
        return IsSet(
            Flag::Active
        );
    }

    bool IsRecovering() const
    {
        return IsSet(
            Flag::Recovering
        );
    }

    bool IsDegraded() const
    {
        return IsSet(
            Flag::Degraded
        );
    }

    bool HasFault() const
    {
        return IsSet(
            Flag::Fault
        );
    }

    // ========================================================================
    // OPERATIONAL STATE
    //
    // Definizione comune minima:
    //
    //   Enabled
    //   Running
    //   Initialized
    //   Active
    //   not Fault
    //
    // Connected non è obbligatorio perché alcuni frontend, per esempio
    // un server HTTP, possono essere operativi senza client connessi.
    // ========================================================================

    bool IsOperational() const
    {
        return
            IsEnabled() &&
            IsRunning() &&
            IsInitialized() &&
            IsActive() &&
            !HasFault();
    }

    // ========================================================================
    // HEALTH STATE
    //
    // Indica che il frontend non è degradato e non è in fault.
    //
    // Non richiede che il frontend sia abilitato o in esecuzione.
    // ========================================================================

    bool IsHealthy() const
    {
        return
            !IsDegraded() &&
            !HasFault();
    }

    // ========================================================================
    // CHANGE DETECTION
    //
    // Restituisce true quando:
    //
    //   - la word non è mai stata consumata;
    //   - la word corrente è diversa dall'ultima word consumata.
    // ========================================================================

    bool HasChanged() const
    {
        return
            !consumed ||
            statusWord !=
                consumedStatusWord;
    }

    // ========================================================================
    // CONSUME CHANGED WORD
    //
    // Se lo stato è cambiato:
    //
    //   - copia la word corrente in output;
    //   - aggiorna consumedStatusWord;
    //   - marca la word come consumata;
    //   - restituisce true.
    //
    // Se lo stato non è cambiato:
    //
    //   - non modifica output;
    //   - restituisce false.
    //
    // Consente agli Engine di pubblicare la status word nel Buffer solo
    // quando cambia realmente.
    // ========================================================================

    bool ConsumeChanged(
        Word& output)
    {
        if (!HasChanged())
            return false;

        output =
            statusWord;

        consumedStatusWord =
            statusWord;

        consumed =
            true;

        return true;
    }

    // ========================================================================
    // PEEK LAST CONSUMED WORD
    // ========================================================================

    Word GetConsumedWord() const
    {
        return
            consumedStatusWord;
    }

    // ========================================================================
    // MARK CONSUMED
    //
    // Marca la word corrente come già consumata senza restituirla.
    //
    // Utile per inizializzare slot non utilizzati senza produrre eventi.
    // ========================================================================

    void MarkConsumed()
    {
        consumedStatusWord =
            statusWord;

        consumed =
            true;
    }

    // ========================================================================
    // FORCE CHANGED
    //
    // Forza la successiva ConsumeChanged() a restituire la word corrente,
    // anche quando non è stata modificata.
    // ========================================================================

    void ForceChanged()
    {
        consumed =
            false;
    }
};


class WatchEngine {
public:

    // ============================================================
    // 1. REGISTRAZIONE AREE DA MONITORARE
    // ============================================================
    static void attach(DomoManager& manager, const FrontendConfig::Watch& cfg) {
        if (!cfg.enabled)
            return;

        auto& diag = manager.getWatchDiag();

        for (size_t i = 0; i < cfg.count; i++) {
            diag.addArea(cfg.aree[i]);
        }

        LOG_IF("WatchEngine", "Diagnostic areas registered (%u)", cfg.count);
    }
};

class HVACEngine {
private:
    static inline DMHVAC hvac;   // wrapper ufficiale

public:

    // ========================================================================
    //  SETUP
    // ========================================================================
    static void Setup(const FrontendConfig::HVAC& cfg)
    {
        // FrontendConfig::HVAC eredita DMHVAC::HVACConfig
        hvac.setup(cfg);

        LOG_IF("HVAC", "HVAC initialized with %u zones", cfg.zoneCount);
    }

    // ========================================================================
    //  LOOP
    // ========================================================================
    static void Loop(unsigned long now,
                 const float* temperaturePerZona,
                 float tInterna,
                 float tEsterna,
                 bool finestraAperta,
                 const HeatPumpController::HVACTime& ht)
    {
        // Passiamo tutto direttamente al wrapper DMHVAC
        hvac.loop(
            now,
            ht,
            const_cast<float*>(temperaturePerZona),
            tInterna,
            tEsterna,
            finestraAperta
        );
    }


    // ========================================================================
    //  ACCESSORS
    // ========================================================================
    static HeatPumpController& GetHP() {
        return hvac.getHP();
    }

    static HeatPumpController::ZoneHVAC& GetZone(size_t i) {
        return *(hvac.getHP().getZoneList()[i]);
    }

    static size_t GetZoneCount() {
        return hvac.getHP().getZoneList().size();
    }

    static void DiagnosticReport() {
        HeatPumpController& hp = hvac.getHP();
        HeatPumpController::Diagnostic::Report(hp);
    }
};

class AEEEngine
{
public:
    // ============================================================
    // COMPOSABLE INTROSPECTION FILTER
    //
    // Tutte le condizioni configurate sono cumulative (AND).
    // Le maschere descrivono gli elementi da escludere.
    // Nessuna std::function e nessuna allocazione dinamica.
    // ============================================================

    struct EntityFilter
    {
        struct AreaRange
        {
            int first = -1;
            int last = -1;

            bool contains(int area) const
            {
                return
                    first >= 0 &&
                    last >= first &&
                    area >= first &&
                    area <= last;
            }
        };

        static constexpr size_t MAX_EXCLUDED_AREA_RANGES = 4U;

        uint32_t excludedKinds = 0;
        uint32_t excludedIOTypes = 0;
        AreaRange excludedAreaRanges[MAX_EXCLUDED_AREA_RANGES] {};
        size_t excludedAreaRangeCount = 0U;

        bool excludeRouteTriggers = false;
        bool excludeToggleSources = false;
        bool excludeSplitMains = false;

        static constexpr uint32_t kindMask(
            DomoIntrospection::EntityKind kind)
        {
            return
                1UL << static_cast<uint8_t>(kind);
        }

        static constexpr uint32_t ioTypeMask(
            DomoIntrospection::IOType ioType)
        {
            return
                1UL << static_cast<uint8_t>(ioType);
        }

        bool matches(
            const DomoIntrospection::Entity& entity) const
        {
            const size_t rangeCount =
                excludedAreaRangeCount < MAX_EXCLUDED_AREA_RANGES
                    ? excludedAreaRangeCount
                    : MAX_EXCLUDED_AREA_RANGES;

            for (size_t index = 0U;
                 index < rangeCount;
                 ++index)
            {
                if (excludedAreaRanges[index].contains(entity.area))
                {
                    return false;
                }
            }

            if (
                excludedKinds &
                kindMask(entity.kind)
            )
            {
                return false;
            }

            if (
                excludedIOTypes &
                ioTypeMask(entity.ioType)
            )
            {
                return false;
            }

            if (excludeRouteTriggers &&
                entity.isRouteTrigger)
            {
                return false;
            }

            if (excludeToggleSources &&
                entity.isToggleSource)
            {
                return false;
            }

            if (excludeSplitMains &&
                entity.isSplitMain)
            {
                return false;
            }

            return true;
        }
    };

    // ============================================================
    // RECEIVE CONTEXT
    // ============================================================

    struct ReceiveContext
    {
        Buffer* buffer =
            nullptr;

        EventManager* events =
            nullptr;

        int aeeSource =
            -1;

        bool isValid() const
        {
            return
                buffer != nullptr &&
                events != nullptr &&
                aeeSource >= 0 &&
                aeeSource <= 254;
        }
    };

private:

    // ============================================================
    // STATE
    // ============================================================

    AEERegistry registry;

    AEEManagement* manager =
        nullptr;

    ReceiveContext receiveContext;

    // ============================================================
    // DYNAMIC INTROSPECTION MODEL
    //
    // Utilizzato esclusivamente dal channel Knob.
    //
    // AEEVarDef possiede internamente il nome mediante String.
    // Non è quindi necessario mantenere uno storage separato
    // dei nomi.
    // ============================================================

    std::vector<AEEVarDef>
        dynamicDefinitions;

    // ============================================================
    // DIRECTION HELPERS
    // ============================================================

    static bool canReceiveFromPeer(
        AEEDirection direction)
    {
        return
            direction ==
                AEEDirection::ModuleToFrontend ||
            direction ==
                AEEDirection::Bidirectional;
    }

    // ============================================================
    // INTROSPECTION ENTITY TYPE
    // ============================================================

    static bool resolveVariableType(
        const DomoIntrospection::Entity& entity,
        AEEVarType& variableType)
    {
        switch (entity.kind)
        {
            case DomoIntrospection::EntityKind::BINARY_SENSOR:
            case DomoIntrospection::EntityKind::SWITCH:
            case DomoIntrospection::EntityKind::LIGHT:
            case DomoIntrospection::EntityKind::COVER:
            case DomoIntrospection::EntityKind::BUTTON:
            {
                variableType =
                    AEEVarType::BOOL;

                return true;
            }

            case DomoIntrospection::EntityKind::SENSOR:
            case DomoIntrospection::EntityKind::NUMBER:
            {
                variableType =
                    AEEVarType::FLOAT;

                return true;
            }

            case DomoIntrospection::EntityKind::TEXT:
            case DomoIntrospection::EntityKind::CLIMATE:
            case DomoIntrospection::EntityKind::STATUS:
            case DomoIntrospection::EntityKind::UNKNOWN:
            default:
            {
                return false;
            }
        }
    }

    // ============================================================
    // INTROSPECTION ENTITY DIRECTION
    // ============================================================

    static AEEDirection resolveDirection(
        const DomoIntrospection::Entity& entity)
    {
        if (
            entity.readable &&
            entity.writable
        )
        {
            return
                AEEDirection::Bidirectional;
        }

        if (entity.readable)
        {
            /*
             * DomoManager master -> peer slave.
             */
            return
                AEEDirection::FrontendToModule;
        }

        /*
         * Peer slave -> DomoManager master.
         */
        return
            AEEDirection::ModuleToFrontend;
    }

    // ============================================================
    // INTROSPECTION VARIABLE NAME
    // ============================================================

    static String buildVariableName(
        int area)
    {
        String variableName;

        variableName.reserve(
            20
        );

        variableName =
            "area_";

        variableName +=
            String(
                area
            );

        return
            variableName;
    }

    // ============================================================
    // INTROSPECTION SCALE
    // ============================================================

    static float resolveScale(
        float configuredScale)
    {
        if (
            !isfinite(
                configuredScale
            ) ||
            configuredScale == 0.0f
        )
        {
            return
                1.0f;
        }

        return
            configuredScale;
    }

    static const char* resolveSemanticKind(
        DomoIntrospection::EntityKind kind)
    {
        using Kind = DomoIntrospection::EntityKind;

        switch (kind)
        {
            case Kind::SENSOR:        return "sensor";
            case Kind::BINARY_SENSOR: return "binary_sensor";
            case Kind::SWITCH:        return "switch";
            case Kind::LIGHT:         return "light";
            case Kind::COVER:         return "cover";
            case Kind::CLIMATE:       return "climate";
            case Kind::BUTTON:        return "button";
            case Kind::NUMBER:        return "number";
            case Kind::TEXT:          return "text";
            case Kind::STATUS:        return "status";
            case Kind::UNKNOWN:
            default:                  return "";
        }
    }

    static const char* resolveSemanticGroup(
        DomoIntrospection::EntityGroup group)
    {
        using Group = DomoIntrospection::EntityGroup;

        switch (group)
        {
            case Group::PHYSICAL_DEVICE: return "device";
            case Group::SECURITY:        return "security";
            case Group::SOLAR:           return "solar";
            case Group::ROOT:
            default:                     return "";
        }
    }

    static const char* resolveSchemaSection(
        DomoIntrospection::EntityGroup group)
    {
        using Group = DomoIntrospection::EntityGroup;

        switch (group)
        {
            case Group::PHYSICAL_DEVICE: return "zones";
            case Group::SECURITY:        return "security";
            case Group::SOLAR:           return "solar";
            case Group::ROOT:
            default:                     return "variables";
        }
    }

    static const char* resolveSchemaZone(
        const DomoIntrospection::Entity& entity)
    {
        using Group = DomoIntrospection::EntityGroup;

        if (entity.group != Group::PHYSICAL_DEVICE &&
            entity.group != Group::SECURITY)
        {
            return "";
        }

        // La zona deve provenire dall'introspection gia risolta.
        // Per Security non esiste necessariamente un device fisico, quindi
        // suggestedArea non puo essere usata come unica sorgente.
        if (entity.zoneResolved &&
            entity.zone &&
            entity.zone[0] != '\0')
        {
            return entity.zone;
        }

        // Fallback legacy per entita fisiche non ancora risolte.
        if (entity.device &&
            entity.device->suggestedArea &&
            entity.device->suggestedArea[0] != '\0')
        {
            return entity.device->suggestedArea;
        }

        return "Generali";
    }

    void attachSchemaMetadata(
        const DomoIntrospection& introspection)
    {
        registry.reserveSchemaMetadata(
            dynamicDefinitions.size()
        );

        introspection.forEachEntity(
            [this, &introspection](
                const DomoIntrospection::Entity& entity)
            {
                if (!entity.isPublicBufferEntity())
                    return;

                if (!entity.readable && !entity.writable)
                    return;

                AEEVarType variableType;

                if (!resolveVariableType(entity, variableType))
                    return;

                const String variableName =
                    buildVariableName(entity.area);

                if (!registry.find(variableName))
                    return;

                const char* label =
                    introspection.getAreaName(entity.area);

                const bool hasNumericRange =
                    entity.kind ==
                        DomoIntrospection::EntityKind::NUMBER &&
                    entity.number.valid();

                registry.setSchemaMetadata(
                    variableName.c_str(),
                    label,
                    resolveSemanticKind(entity.kind),
                    resolveSemanticGroup(entity.group),
                    resolveSchemaSection(entity.group),
                    resolveSchemaZone(entity),
                    entity.unit,
                    entity.deviceClass,
                    hasNumericRange,
                    entity.number.minimumValue,
                    entity.number.maximumValue,
                    entity.number.stepValue,
                    entity.number.displayDecimals
                );
            }
        );

        LOG_IF(
            "AEE",
            "Schema metadata ready: entries=%u",
            static_cast<unsigned>(
                registry.schemaMetadataSize()
            )
        );
    }

    // ============================================================
    // MANAGEMENT CREATION
    // ============================================================

    bool createManagement(
        const AEEVarDef* definitions,
        size_t definitionCount,
        const char* context)
    {
        if (
            !definitions ||
            definitionCount == 0
        )
        {
            LOG_EF(
                "AEE",
                "Creazione management rifiutata: "
                "context=%s definitions=%p count=%u",
                context
                    ? context
                    : "UNKNOWN",
                static_cast<const void*>(
                    definitions
                ),
                static_cast<unsigned>(
                    definitionCount
                )
            );

            return false;
        }

        // ========================================================
        // VALIDATE DEFINITIONS
        // ========================================================

        if (
            !AEEManagement::ValidateAndLog(
                definitions,
                definitionCount,
                context
            )
        )
        {
            LOG_EF(
                "AEE",
                "Definizioni AEE non valide: context=%s",
                context
                    ? context
                    : "UNKNOWN"
            );

            return false;
        }

        // ========================================================
        // CREATE MANAGEMENT
        // ========================================================

        manager =
            new AEEManagement(
                definitions,
                definitionCount
            );

        if (!manager)
        {
            LOG_EF(
                "AEE",
                "Allocazione AEEManagement fallita: "
                "context=%s count=%u",
                context
                    ? context
                    : "UNKNOWN",
                static_cast<unsigned>(
                    definitionCount
                )
            );

            return false;
        }

        // ========================================================
        // MANAGEMENT STATE
        // ========================================================

        if (!manager->isValid())
        {
            LOG_EF(
                "AEE",
                "AEEManagement costruito ma non valido: "
                "context=%s",
                context
                    ? context
                    : "UNKNOWN"
            );

            delete manager;

            manager =
                nullptr;

            return false;
        }

        if (
            manager->size() !=
            definitionCount
        )
        {
            LOG_EF(
                "AEE",
                "Conteggio management non coerente: "
                "context=%s definitions=%u variables=%u",
                context
                    ? context
                    : "UNKNOWN",
                static_cast<unsigned>(
                    definitionCount
                ),
                static_cast<unsigned>(
                    manager->size()
                )
            );

            delete manager;

            manager =
                nullptr;

            return false;
        }

        // ========================================================
        // REGISTER VARIABLES
        // ========================================================

        if (
            !manager->registerAll(
                registry
            )
        )
        {
            LOG_EF(
                "AEE",
                "Registrazione variabili fallita: "
                "context=%s",
                context
                    ? context
                    : "UNKNOWN"
            );

            registry.clear();

            delete manager;

            manager =
                nullptr;

            return false;
        }

        // ========================================================
        // REGISTRY CONSISTENCY
        // ========================================================

        if (
            registry.size() !=
            definitionCount
        )
        {
            LOG_EF(
                "AEE",
                "Conteggio registry non coerente: "
                "context=%s definitions=%u registry=%u",
                context
                    ? context
                    : "UNKNOWN",
                static_cast<unsigned>(
                    definitionCount
                ),
                static_cast<unsigned>(
                    registry.size()
                )
            );

            registry.clear();

            delete manager;

            manager =
                nullptr;

            return false;
        }

        LOG_IF(
            "AEE",
            "Management inizializzato: "
            "context=%s definitions=%u variables=%u",
            context
                ? context
                : "UNKNOWN",
            static_cast<unsigned>(
                definitionCount
            ),
            static_cast<unsigned>(
                registry.size()
            )
        );

        return true;
    }

    // ============================================================
    // BUILD INTROSPECTION DEFINITIONS
    // ============================================================

    bool buildIntrospectionDefinitions(
        const DomoIntrospection& introspection,
        const EntityFilter* entityFilter)
    {
        const size_t maximumDefinitions =
            introspection.entityCount();

        if (maximumDefinitions == 0)
        {
            LOG_EF(
                "AEE",
                "Introspection priva di entity"
            );

            return false;
        }

        dynamicDefinitions.reserve(
            maximumDefinitions
        );

        introspection.forEachEntity(
            [this, entityFilter](
                const DomoIntrospection::Entity& entity)
            {
                // =================================================
                // COMPOSABLE CHANNEL FILTER
                // =================================================

                if (entityFilter &&
                    !entityFilter->matches(entity))
                {
                    return;
                }

                // =================================================
                // PUBLIC BUFFER ENTITY
                // =================================================

                if (!entity.isPublicBufferEntity())
                {
                    return;
                }

                if (
                    !entity.readable &&
                    !entity.writable
                )
                {
                    return;
                }

                if (entity.area < 0)
                {
                    LOG_WF(
                        "AEE",
                        "Entity Introspection ignorata: "
                        "area non valida=%d",
                        entity.area
                    );

                    return;
                }

                // =================================================
                // VARIABLE TYPE
                // =================================================

                AEEVarType variableType;

                if (
                    !resolveVariableType(
                        entity,
                        variableType
                    )
                )
                {
                    return;
                }

                // =================================================
                // DIRECTION
                // =================================================

                const AEEDirection direction =
                    resolveDirection(
                        entity
                    );

                // =================================================
                // NAME
                // =================================================

                const String variableName =
                    buildVariableName(
                        entity.area
                    );

                // =================================================
                // SCALE
                // =================================================

                const float scale =
                    resolveScale(
                        entity.scale
                    );

                // =================================================
                // SENSOR CONFIGURATION
                // =================================================

                const GenericSensorConfig sensorConfig =
                {
                    GenericSensorConfig::Type::BUFFER,
                    entity.area,
                    1.0f
                };

                // =================================================
                // DEFINITION
                //
                // AEEVarDef copia variableName nella propria String.
                // Il puntatore c_str() viene usato soltanto durante
                // il costruttore.
                // =================================================

                dynamicDefinitions.emplace_back(
                    variableName.c_str(),
                    direction,
                    AEEVarSourceType::BufferArea,
                    entity.area,
                    -1,
                    sensorConfig,
                    nullptr,
                    nullptr,
                    nullptr,
                    variableType,
                    0.0f,
                    scale
                );
            }
        );

        if (dynamicDefinitions.empty())
        {
            LOG_EF(
                "AEE",
                "Introspection non contiene "
                "entity Buffer esportabili"
            );

            return false;
        }

        return true;
    }

public:

    // ============================================================
    // CONSTRUCTOR
    // ============================================================

    AEEEngine() =
        default;

    // ============================================================
    // DESTRUCTOR
    // ============================================================

    ~AEEEngine()
    {
        clear();
    }

    // ============================================================
    // COPY / MOVE POLICY
    // ============================================================

    AEEEngine(
        const AEEEngine&) =
            delete;

    AEEEngine& operator=(
        const AEEEngine&) =
            delete;

    AEEEngine(
        AEEEngine&&) =
            delete;

    AEEEngine& operator=(
        AEEEngine&&) =
            delete;

    // ============================================================
    // SETUP STATIC CONFIGURATION
    // ============================================================

    bool setup(
        const FrontendConfig::Bridge::AEE& cfg)
    {
        clear();

        if (
            !cfg.vars ||
            cfg.count == 0
        )
        {
            LOG_EF(
                "AEE",
                "Configurazione AEE statica non valida: "
                "vars=%p count=%u",
                static_cast<const void*>(
                    cfg.vars
                ),
                static_cast<unsigned>(
                    cfg.count
                )
            );

            return false;
        }

        if (
            !createManagement(
                cfg.vars,
                cfg.count,
                "STATIC"
            )
        )
        {
            clear();

            return false;
        }

        LOG_IF(
            "AEE",
            "AEE statica inizializzata: "
            "definitions=%u variables=%u",
            static_cast<unsigned>(
                cfg.count
            ),
            static_cast<unsigned>(
                registry.size()
            )
        );

        return true;
    }

    // ============================================================
    // SETUP FROM INTROSPECTION
    // ============================================================

    bool setup(
        const DomoIntrospection& introspection,
        const EntityFilter* entityFilter = nullptr)
    {
        clear();

        if (
            !buildIntrospectionDefinitions(
                introspection,
                entityFilter
            )
        )
        {
            clear();

            return false;
        }

        if (
            !createManagement(
                dynamicDefinitions.data(),
                dynamicDefinitions.size(),
                "INTROSPECTION"
            )
        )
        {
            clear();

            return false;
        }

        attachSchemaMetadata(
            introspection
        );

        LOG_IF(
            "AEE",
            "AEE Introspection inizializzata: "
            "entities=%u definitions=%u variables=%u",
            static_cast<unsigned>(
                introspection.entityCount()
            ),
            static_cast<unsigned>(
                dynamicDefinitions.size()
            ),
            static_cast<unsigned>(
                registry.size()
            )
        );

        return true;
    }

    // ============================================================
    // CLEAR
    // ============================================================

    void clear()
    {
        /*
         * Il registry conserva riferimenti non proprietari.
         * Deve essere svuotato prima di distruggere manager.
         */
        registry.clear();

        /*
         * AEEManagement possiede le AEEVariableBase.
         */
        delete manager;

        manager =
            nullptr;

        /*
         * Le definizioni dinamiche possiedono autonomamente
         * i nomi tramite String.
         */
        dynamicDefinitions.clear();

        /*
         * Elimina eventuali puntatori runtime precedenti.
         */
        receiveContext =
            ReceiveContext{};
    }

    // ============================================================
    // REGISTRY ACCESSORS
    // ============================================================

    AEERegistry& getRegistry()
    {
        return
            registry;
    }

    const AEERegistry& getRegistry() const
    {
        return
            registry;
    }

    // ============================================================
    // MANAGEMENT ACCESSORS
    // ============================================================

    AEEManagement* getManager()
    {
        return
            manager;
    }

    const AEEManagement* getManager() const
    {
        return
            manager;
    }

    // ============================================================
    // DYNAMIC DEFINITIONS ACCESSORS
    // ============================================================

    const std::vector<AEEVarDef>&
        getDynamicDefinitions() const
    {
        return
            dynamicDefinitions;
    }

    size_t getDynamicDefinitionCount() const
    {
        return
            dynamicDefinitions.size();
    }

    // ============================================================
    // READY STATE
    // ============================================================

    bool isReady() const
    {
        return
            manager != nullptr &&
            manager->isValid() &&
            manager->size() > 0 &&
            registry.size() ==
                manager->size();
    }

    // ============================================================
    // RECEIVE CONTEXT
    // ============================================================

    bool attachReceiveContext(
        Buffer& buffer,
        EventManager& events,
        int aeeSource)
    {
        receiveContext =
            ReceiveContext{};

        if (
            aeeSource < 0 ||
            aeeSource > 254
        )
        {
            LOG_EF(
                "AEE::RX",
                "Sorgente EventManager AEE non valida: %d",
                aeeSource
            );

            return false;
        }

        receiveContext.buffer =
            &buffer;

        receiveContext.events =
            &events;

        receiveContext.aeeSource =
            aeeSource;

        LOG_IF(
            "AEE::RX",
            "Contesto ricezione inizializzato: source=%d",
            aeeSource
        );

        return true;
    }

    bool hasReceiveContext() const
    {
        return
            receiveContext.isValid();
    }

    const ReceiveContext& getReceiveContext() const
    {
        return
            receiveContext;
    }

    // ============================================================
    // RECEIVE FROM PEER
    // ============================================================

    void applyReceived(
        AEEVariableBase* variable,
        unsigned long now)
    {
        if (!variable)
        {
            LOG_WF(
                "AEE::RX",
                "Variabile ricevuta nulla"
            );

            return;
        }

        if (!isReady())
        {
            LOG_WF(
                "AEE::RX",
                "AEEEngine non pronto"
            );

            return;
        }

        if (!receiveContext.isValid())
        {
            LOG_WF(
                "AEE::RX",
                "Contesto ricezione AEE non inizializzato"
            );

            return;
        }

        const AEEDirection direction =
            variable->def.direction;

        if (
            !canReceiveFromPeer(
                direction
            )
        )
        {
            return;
        }

        switch (variable->def.sourceType)
        {
            case AEEVarSourceType::BufferArea:
            {
                applyBufferArea(
                    variable,
                    now
                );

                return;
            }

            case AEEVarSourceType::None:
            case AEEVarSourceType::Function:
            {
                onAEEVarChanged(
                    variable,
                    now
                );

                return;
            }

            case AEEVarSourceType::GenericSensor:
            default:
            {
                LOG_WF(
                    "AEE::RX",
                    "Sorgente ricevuta non supportata: "
                    "name=%s source=%u",
                    variable
                        ->def
                        .name
                        .c_str(),
                    static_cast<unsigned>(
                        variable
                            ->def
                            .sourceType
                    )
                );

                return;
            }
        }
    }

private:

    // ============================================================
    // APPLY BUFFER AREA
    // ============================================================

    void applyBufferArea(
        AEEVariableBase* variable,
        unsigned long now)
    {
        (void)now;

        if (
            !variable ||
            !receiveContext.isValid()
        )
        {
            return;
        }

        Buffer& buffer =
            *receiveContext.buffer;

        const int area =
            variable->def.bufferArea;

        if (
            area < 0 ||
            area >= buffer.size()
        )
        {
            LOG_WF(
                "AEE::RX",
                "Area non valida per '%s': "
                "area=%d bufferSize=%d",
                variable
                    ->def
                    .name
                    .c_str(),
                area,
                buffer.size()
            );

            return;
        }

        long value =
            0;

        bool supported =
            false;

        // ========================================================
        // BOOL
        // ========================================================

        if (
            auto* booleanVariable =
                as<bool>(
                    variable
                )
        )
        {
            if (
                variable->def.bitIndex >= 0
            )
            {
                if (
                    variable->def.bitIndex >
                    31
                )
                {
                    LOG_WF(
                        "AEE::RX",
                        "Bit index non valido per '%s': %d",
                        variable
                            ->def
                            .name
                            .c_str(),
                        variable
                            ->def
                            .bitIndex
                    );

                    return;
                }

                value =
                    buffer.getValueFast(
                        area
                    );

                bitWrite(
                    value,
                    variable->def.bitIndex,
                    booleanVariable->get()
                );
            }
            else
            {
                value =
                    booleanVariable->get()
                        ? 1L
                        : 0L;
            }

            supported =
                true;
        }

        // ========================================================
        // INT
        // ========================================================

        else if (
            auto* integerVariable =
                as<int>(
                    variable
                )
        )
        {
            if (
                !isfinite(
                    variable->def.scale
                ) ||
                variable->def.scale == 0.0f
            )
            {
                LOG_WF(
                    "AEE::RX",
                    "Scala non valida per '%s': %.6f",
                    variable
                        ->def
                        .name
                        .c_str(),
                    variable
                        ->def
                        .scale
                );

                return;
            }

            value =
                static_cast<long>(
                    static_cast<float>(
                        integerVariable->get()
                    ) /
                    variable->def.scale
                );

            supported =
                true;
        }

        // ========================================================
        // FLOAT
        // ========================================================

        else if (
            auto* floatVariable =
                as<float>(
                    variable
                )
        )
        {
            if (
                !isfinite(
                    variable->def.scale
                ) ||
                variable->def.scale == 0.0f
            )
            {
                LOG_WF(
                    "AEE::RX",
                    "Scala non valida per '%s': %.6f",
                    variable
                        ->def
                        .name
                        .c_str(),
                    variable
                        ->def
                        .scale
                );

                return;
            }

            const float scaledValue =
                floatVariable->get() /
                variable->def.scale;

            if (!isfinite(scaledValue))
            {
                LOG_WF(
                    "AEE::RX",
                    "Valore scalato non finito per '%s'",
                    variable
                        ->def
                        .name
                        .c_str()
                );

                return;
            }

            value =
                static_cast<long>(
                    scaledValue
                );

            supported =
                true;
        }

        // ========================================================
        // UNSUPPORTED
        // ========================================================

        if (!supported)
        {
            LOG_WF(
                "AEE::RX",
                "Tipo non supportato per '%s': type=%u",
                variable
                    ->def
                    .name
                    .c_str(),
                static_cast<unsigned>(
                    variable
                        ->def
                        .varType
                )
            );

            return;
        }

        // ========================================================
        // BUFFER EVENT
        // ========================================================

        if (!DomoManager::instance)
        {
            LOG_WF(
                "AEE::RX",
                "DomoManager non disponibile"
            );

            return;
        }

        DomoManager::instance->forceEvent(
            area,
            value,
            static_cast<uint8_t>(
                receiveContext.aeeSource
            )
        );

        LOG_DF(
            "AEE::RX",
            "AEE '%s' -> Buffer "
            "area=%d value=%ld source=%u",
            variable
                ->def
                .name
                .c_str(),
            area,
            value,
            static_cast<unsigned>(
                receiveContext.aeeSource
            )
        );
    }

    // ============================================================
    // POST PROCESSOR
    // ============================================================

    void onAEEVarChanged(
        AEEVariableBase* variable,
        unsigned long now)
    {
        (void)now;

        if (!variable)
        {
            return;
        }

        switch (variable->def.sourceType)
        {
            // ====================================================
            // NONE / SYSTEM MESSAGES
            // ====================================================

            case AEEVarSourceType::None:
            {
                processSystemVariable(
                    variable
                );

                return;
            }

            // ====================================================
            // FUNCTION
            // ====================================================

            case AEEVarSourceType::Function:
            {
                /*
                 * Punto di estensione per future funzioni AEE
                 * ricevibili dal peer.
                 */
                return;
            }

            default:
            {
                return;
            }
        }
    }

    // ============================================================
    // SYSTEM VARIABLE PROCESSOR
    // ============================================================

    void processSystemVariable(
        AEEVariableBase* variable)
    {
        if (!variable)
        {
            return;
        }

        // ========================================================
        // EPOCH
        // ========================================================

        if (
            variable->def.name ==
            "epoch"
        )
        {
            processEpoch(
                variable
            );

            return;
        }

        LOG_DF(
            "AEE::RX",
            "Variabile di sistema senza handler: %s",
            variable
                ->def
                .name
                .c_str()
        );
    }

    // ============================================================
    // EPOCH PROCESSOR
    // ============================================================

    void processEpoch(
        AEEVariableBase* variable)
    {
        auto* epochVariable =
            as<int>(
                variable
            );

        if (!epochVariable)
        {
            LOG_WF(
                "AEE",
                "epoch ricevuto con tipo non valido"
            );

            return;
        }

        const int epoch =
            epochVariable->get();

        if (epoch <= 0)
        {
            LOG_WF(
                "AEE",
                "Epoch ricevuto non valido: %d",
                epoch
            );

            variable->clearChanged();

            return;
        }

        if (!DomoManager::instance)
        {
            LOG_WF(
                "AEE",
                "Epoch non applicato: "
                "DomoManager non disponibile"
            );

            return;
        }

        LOG_IF(
            "AEE",
            "Epoch ricevuto dal PEER: "
            "%d -> applicazione RTC",
            epoch
        );

        DomoManager::instance
            ->getTimeManager()
            .getRTC()
            .applyEpoch(
                epoch
            );

        variable->clearChanged();
    }
};

// ============================================================
// BRIDGE ENGINE
//
// Wrapper runtime non statico di DMMasterBridge.
//
// Ogni istanza BridgeEngine possiede:
//
// - una propria identità;
// - un proprio DMMasterBridge;
// - un proprio pacer;
// - una propria callback di ricezione;
// - un proprio stato di inizializzazione.
//
// La classe non possiede:
//
// - il transport ICommandTransport;
// - il registry AEE;
// - il DMNetworkManager.
//
// Queste dipendenze devono rimanere valide per tutta la durata
// operativa del BridgeEngine.
//
// La classe è progettata per consentire più bridge paralleli.
// Ogni BridgeEngine può quindi essere associato a:
//
// - un transport differente;
// - un registry AEE differente;
// - un protocollo di rete differente;
// - una callback di ricezione differente.
//
// Nessuno stato runtime è statico.
// ============================================================

class BridgeEngine
{
public:

    // ========================================================
    // RECEIVE CALLBACK TYPE
    //
    // std::function consente al composition root di registrare:
    //
    // - funzioni statiche;
    // - lambda senza cattura;
    // - lambda che catturano l'istanza AEEBridgeChannel;
    // - callable object.
    //
    // Questo elimina la necessità di utilizzare manualmente un
    // puntatore void* come contesto.
    // ========================================================

    using ReceiveCallback =
        std::function<
            void(
                AEEVariableBase* variable,
                unsigned long now
            )
        >;

    using NamedValueCallback =
        std::function<void(const BridgeNamedValue&, unsigned long)>;

private:

    // ========================================================
    // PACER IMPLEMENTATION
    //
    // Adapter tra IBridgePacer e DMNetworkManager.
    //
    // Ogni BridgeEngine crea un pacer associato al proprio
    // protocolId.
    //
    // In presenza di due bridge:
    //
    // - Bridge 0 possiede Pacer 0 e ProtocolId 0;
    // - Bridge 1 possiede Pacer 1 e ProtocolId 1.
    //
    // Non viene condiviso alcuno stato di pacing.
    // ========================================================

    class DomoBridgePacer
        : public IBridgePacer
    {
    private:

        // ----------------------------------------------------
        // NETWORK REFERENCE
        //
        // DMNetworkManager è posseduto dal DomoManager.
        // Il pacer conserva solamente un riferimento.
        // ----------------------------------------------------

        DMNetworkManager& network;

        // ----------------------------------------------------
        // PROTOCOL ID
        //
        // Identifica il protocollo registrato specificamente
        // per questa istanza BridgeEngine.
        // ----------------------------------------------------

        DMNetworkManager::ProtocolId
            protocolId =
                -1;

    public:

        // ====================================================
        // CONSTRUCTOR
        // ====================================================

        DomoBridgePacer(
            DMNetworkManager& networkRef,
            DMNetworkManager::ProtocolId id)
            : network(
                networkRef
              ),
              protocolId(
                id
              )
        {
        }

        // ====================================================
        // ACQUIRE
        //
        // Richiede al DMNetworkManager il diritto di eseguire
        // una transazione per il protocollo associato.
        // ====================================================

        bool acquire(
            unsigned long now) override
        {
            return
                network.tryAcquire(
                    protocolId,
                    now
                );
        }

        // ====================================================
        // RELEASE
        //
        // Rilascia il protocollo dopo il completamento della
        // transazione Bridge.
        // ====================================================

        void release(
            unsigned long now) override
        {
            network.release(
                protocolId,
                now
            );
        }

        // ====================================================
        // ACCESSOR
        // ====================================================

        DMNetworkManager::ProtocolId
        getProtocolId() const
        {
            return
                protocolId;
        }
    };

private:

    // ========================================================
    // IDENTITY
    //
    // L'identità serve esclusivamente per:
    //
    // - diagnostica;
    // - log;
    // - distinzione tra più istanze.
    //
    // Non viene utilizzata per individuare il registry AEE.
    // Il registry viene passato direttamente a init().
    // ========================================================

    uint8_t id =
        0;

    String name =
        "BRIDGE";

    // ========================================================
    // BRIDGE CORE
    //
    // Ogni BridgeEngine possiede un proprio DMMasterBridge.
    //
    // Di conseguenza ogni istanza possiede separatamente:
    //
    // - stato del protocollo;
    // - callback;
    // - transport associato;
    // - registry associato;
    // - stato master/slave;
    // - pacer associato.
    // ========================================================

    DMMasterBridge bridge;

    // ========================================================
    // PACER OWNERSHIP
    //
    // Il pacer viene allocato durante setupPacer().
    //
    // BridgeEngine ne possiede completamente la durata.
    // ========================================================

    DomoBridgePacer* pacer =
        nullptr;

    // ========================================================
    // RUNTIME STATE
    // ========================================================

    bool initialized =
        false;

    bool pacerConfigured =
        false;

    // ========================================================
    // RESET PACER
    //
    // Rimuove il pacer corrente.
    //
    // Deve essere utilizzato prima di configurare un nuovo
    // protocollo sulla stessa istanza BridgeEngine.
    // ========================================================

    void releasePacer()
    {
        delete pacer;

        pacer =
            nullptr;

        pacerConfigured =
            false;
    }

public:

    // ========================================================
    // DEFAULT CONSTRUCTOR
    // ========================================================

    BridgeEngine() = default;

    // ========================================================
    // IDENTITY CONSTRUCTOR
    // ========================================================

    BridgeEngine(
        uint8_t bridgeId,
        const char* bridgeName)
        : id(
            bridgeId
          ),
          name(
            bridgeName &&
            bridgeName[0] != '\0'
                ? bridgeName
                : "BRIDGE"
          )
    {
    }

    // ========================================================
    // DESTRUCTOR
    //
    // Il pacer deve essere eliminato prima della distruzione
    // automatica del DMMasterBridge.
    //
    // La callback viene azzerata preventivamente per evitare
    // che il bridge conservi lambda riferite a oggetti ormai in
    // distruzione.
    // ========================================================

    ~BridgeEngine()
    {
        clearReceiveCallback();

        releasePacer();

        initialized =
            false;
    }

    // ========================================================
    // NON-COPYABLE / NON-MOVABLE
    //
    // BridgeEngine contiene:
    //
    // - un DMMasterBridge con stato runtime;
    // - un pacer posseduto dinamicamente;
    // - riferimenti indiretti a transport e registry.
    //
    // Copia e movimento vengono disabilitati per evitare:
    //
    // - doppia distruzione del pacer;
    // - invalidazione dei riferimenti;
    // - duplicazione dello stato del protocollo;
    // - callback associate all'istanza errata.
    // ========================================================

    BridgeEngine(
        const BridgeEngine&) =
            delete;

    BridgeEngine& operator=(
        const BridgeEngine&) =
            delete;

    BridgeEngine(
        BridgeEngine&&) =
            delete;

    BridgeEngine& operator=(
        BridgeEngine&&) =
            delete;

    // ========================================================
    // IDENTITY
    // ========================================================

    uint8_t getId() const
    {
        return
            id;
    }

    const String& getName() const
    {
        return
            name;
    }

    void setIdentity(
        uint8_t bridgeId,
        const char* bridgeName)
    {
        id =
            bridgeId;

        name =
            bridgeName &&
            bridgeName[0] != '\0'
                ? bridgeName
                : "BRIDGE";
    }

    // ========================================================
    // BRIDGE ACCESS
    //
    // Accesso diretto al DMMasterBridge.
    //
    // Deve essere utilizzato solamente per configurazioni o
    // diagnostica non esposte direttamente da BridgeEngine.
    // ========================================================

    DMMasterBridge& getBridge()
    {
        return
            bridge;
    }

    const DMMasterBridge& getBridge() const
    {
        return
            bridge;
    }

    // ========================================================
    // RECEIVE CALLBACK
    //
    // La callback appartiene alla singola istanza.
    //
    // Per associare il BridgeEngine a un AEEBridgeChannel si
    // utilizza una lambda che cattura il puntatore this del
    // channel:
    //
    // bridgeEngine.setReceiveCallback(
    //     //         AEEVariableBase* variable,
    //         unsigned long now
    //     {
    //         onVariableReceived(
    //             variable,
    //             now
    //         );
    //     }
    // );
    //
    // Ogni istanza conserva una lambda diversa e quindi inoltra
    // la variabile al proprio AEEEngine.
    // ========================================================

    void setReceiveCallback(
        ReceiveCallback callback)
    {
        bridge.onFrontendAEEChange =
            std::move(
                callback
            );
    }

    // ========================================================
    // CLEAR RECEIVE CALLBACK
    //
    // Disconnette esplicitamente il destinatario degli eventi
    // AEE ricevuti.
    //
    // È utile durante distruzione, reset o riconfigurazione.
    // ========================================================

    void clearReceiveCallback()
    {
        bridge.onFrontendAEEChange =
            nullptr;
        bridge.onFrontendNamedValue =
            nullptr;
    }

    void setNamedValueCallback(
        NamedValueCallback callback)
    {
        bridge.onFrontendNamedValue =
            std::move(callback);
    }

    // ========================================================
    // INIT
    //
    // Associa il bridge a:
    //
    // - uno specifico transport;
    // - uno specifico registry AEE;
    // - uno specifico ruolo master/slave.
    //
    // Il transport e il registry non vengono copiati e devono
    // rimanere validi per tutta la durata operativa del bridge.
    //
    // Chiamare init() una sola volta per configurazione oppure
    // solamente dopo aver arrestato completamente l'utilizzo
    // precedente del bridge.
    // ========================================================

    void init(
        ICommandTransport& transport,
        AEERegistry& registry,
        bool master = true)
    {
        initialized =
            false;

        bridge.init(
            &transport,
            registry,
            master
        );

        initialized =
            true;

        LOG_IF(
            "BRIDGE",
            "BridgeEngine '%s' inizializzato: "
            "id=%u master=%u registry=%u",
            name.c_str(),
            static_cast<unsigned>(
                id
            ),
            master
                ? 1U
                : 0U,
            static_cast<unsigned>(
                registry.size()
            )
        );
    }

    // ========================================================
    // PACER SETUP
    //
    // Crea un pacer associato al protocollo registrato per
    // questa istanza BridgeEngine.
    //
    // Se esiste già un pacer, viene eliminato prima della nuova
    // configurazione.
    // ========================================================

    bool setupPacer(
        DMNetworkManager& network,
        DMNetworkManager::ProtocolId protocolId)
    {
        if (protocolId < 0)
        {
            LOG_EF(
                "BRIDGE",
                "Protocol ID non valido per '%s': %d",
                name.c_str(),
                static_cast<int>(
                    protocolId
                )
            );

            return false;
        }

        releasePacer();

        pacer =
            new DomoBridgePacer(
                network,
                protocolId
            );

        if (!pacer)
        {
            LOG_EF(
                "BRIDGE",
                "Allocazione pacer fallita per '%s'",
                name.c_str()
            );

            return false;
        }

        bridge.setPacer(
            *pacer
        );

        pacerConfigured =
            true;

        LOG_IF(
            "BRIDGE",
            "Pacer configurato per '%s': "
            "protocol=%d",
            name.c_str(),
            static_cast<int>(
                protocolId
            )
        );

        return true;
    }

    // ========================================================
    // LOOP
    //
    // Esegue il ciclo del singolo DMMasterBridge.
    //
    // BridgeEngine non applica scheduling locale. Il pacing è
    // delegato al DomoBridgePacer e al DMNetworkManager.
    //
    // Il composition root può richiamare più BridgeEngine nello
    // stesso task oppure utilizzare un task per ogni channel.
    // ========================================================

    void loop(
        unsigned long now)
    {
        if (!initialized)
        {
            return;
        }

        bridge.loop(
            now
        );
    }

    // ========================================================
    // RUNTIME ACCESSORS
    // ========================================================

    bool isInitialized() const
    {
        return
            initialized;
    }

    bool isPacerConfigured() const
    {
        return
            pacerConfigured;
    }

    DMNetworkManager::ProtocolId
    getProtocolId() const
    {
        if (!pacer)
        {
            return
                -1;
        }

        return
            pacer->getProtocolId();
    }
};

// ============================================================
// AEE BRIDGE CHANNEL
//
// Rappresenta un canale di comunicazione AEE completamente
// autonomo.
//
// Ogni istanza possiede:
//
// - un AEEEngine;
// - un BridgeEngine;
// - un registry AEE indipendente;
// - un AEEManagement indipendente;
// - un transport UDP indipendente;
// - un protocollo DMNetworkManager indipendente;
// - un socket owner indipendente;
// - una sorgente EventManager indipendente;
// - una configurazione AEE indipendente.
//
// La classe permette quindi di eseguire più bridge in parallelo
// senza condividere registry, stato, callback o transport.
//
// Esempio:
//
//   AEEBridgeChannel primaryBridgeChannel;
//   AEEBridgeChannel knobBridgeChannel;
//
// Il transport viene allocato solamente durante setup() e viene
// rilasciato dal distruttore. Non vengono effettuate allocazioni
// dinamiche nel loop.
//
// La classe non possiede:
//
// - DomoManager;
// - DMNetworkManager;
// - Buffer;
// - EventManager.
//
// Queste dipendenze vengono ricevute durante setup() e conservate
// esclusivamente dove necessario.
// ============================================================

class AEEBridgeChannel
{
public:

    // ========================================================
    // TYPE ALIASES
    // ========================================================

    using ProtocolId =
        DMNetworkManager::ProtocolId;

    using SocketOwnerId =
        SocketManager::OwnerId;

    // ========================================================
    // CONFIGURATION
    //
    // La configurazione appartiene al chiamante.
    //
    // AEEBridgeChannel non conserva il puntatore alla struttura
    // Config dopo il completamento di setup().
    //
    // Il blocco AEE viene copiato internamente da AEEEngine
    // secondo il comportamento della relativa setup().
    // ========================================================

    struct Config
    {
        // ----------------------------------------------------
        // ENABLE
        // ----------------------------------------------------

        bool enabled =
            false;

        // ----------------------------------------------------
        // CHANNEL IDENTITY
        // ----------------------------------------------------

        uint8_t id =
            0;

        const char* name =
            nullptr;

        // ----------------------------------------------------
        // UDP ENDPOINT
        // ----------------------------------------------------

        IPAddress remoteIp;

        uint16_t localPort =
            0;

        uint16_t remotePort =
            0;

        // ----------------------------------------------------
        // NETWORK PACING
        // ----------------------------------------------------

        uint32_t protocolIntervalMs =
            50UL;

        uint32_t protocolTimeoutMs =
            300UL;

        uint32_t taskIntervalMs =
            20UL;

        // ----------------------------------------------------
        // STATIC AEE CONFIGURATION
        //
        // Utilizzata solamente quando:
        //
        //     modelSource == AEE_REGISTRY
        // ----------------------------------------------------

        FrontendConfig::Bridge::AEE aee;

        // ----------------------------------------------------
        // COMPOSABLE INTROSPECTION FILTER
        //
        // Disabilitato per default: gli altri channel continuano
        // a ricevere il modello Introspection completo.
        // ----------------------------------------------------

        bool filterIntrospection =
            false;

        AEEEngine::EntityFilter
            introspectionFilter;

        BridgeEngine::NamedValueCallback
            namedValueCallback = nullptr;
    };

private:

    // ========================================================
    // IDENTITY
    // ========================================================

    uint8_t channelId =
        0;

    const char* channelName =
        nullptr;

    // ========================================================
    // ENABLE / RUNTIME STATE
    // ========================================================

    bool enabled =
        false;

    bool initialized =
        false;

    // ========================================================
    // EVENT SOURCE
    //
    // La sorgente EventManager è specifica del canale.
    //
    // Il valore 255 rappresenta una sorgente non valida o non
    // ancora inizializzata.
    // ========================================================

    uint8_t eventSource =
        255;

    // ========================================================
    // NETWORK REGISTRATION
    // ========================================================

    ProtocolId protocolId =
        -1;

    SocketOwnerId socketOwner =
        -1;

    // ========================================================
    // TASK CONFIGURATION
    //
    // Il valore viene conservato per consentire al composition
    // root di registrare il task con l'intervallo configurato.
    // ========================================================

    uint32_t taskIntervalMs =
        20UL;

    // ========================================================
    // SHARED BUFFER
    //
    // Puntatore non posseduto.
    //
    // Viene utilizzato per filtrare lo snapshot iniziale del
    // channel Introspection tramite HasEverChanged().
    // ========================================================

    Buffer* buffer =
        nullptr;

    BridgeEngine::NamedValueCallback
        namedValueCallback = nullptr;

    // ========================================================
    // CHANNEL ENGINES
    //
    // L'ordine di dichiarazione è intenzionale.
    //
    // Costruzione:
    //   1. AEEEngine
    //   2. BridgeEngine
    //
    // Distruzione:
    //   1. BridgeEngine
    //   2. AEEEngine
    //
    // Il Bridge utilizza il registry posseduto da AEEEngine.
    // ========================================================

    AEEEngine aeeEngine;

    BridgeEngine bridgeEngine;

    // ========================================================
    // UDP TRANSPORT
    //
    // UdpAdapter non viene costruito finché non sono disponibili:
    //
    // - configurazione IP;
    // - porte;
    // - DMNetworkManager;
    // - socket owner.
    //
    // L'allocazione avviene una sola volta durante setup().
    // Nessuna allocazione viene eseguita nel loop.
    // ========================================================

    UdpAdapter* transport =
        nullptr;

    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    unsigned long lastAliveLog =
        0;

    unsigned long maximumLoopTimeUs =
        0;

    // ========================================================
    // VALID CHANNEL NAME
    // ========================================================

    const char* validName() const
    {
        return
            channelName &&
            channelName[0] != '\0'
                ? channelName
                : "AEE-BRIDGE";
    }

    // ========================================================
    // RESET RUNTIME STATE
    //
    // Azzera lo stato runtime, disconnette le callback,
    // pulisce AEEEngine e rilascia il transport posseduto.
    //
    // Non distrugge BridgeEngine, ma rimuove le callback che
    // potrebbero riferirsi alla configurazione precedente.
    // ========================================================

    void resetRuntimeState()
    {
        initialized =
            false;

        enabled =
            false;

        eventSource =
            EventManager::
                INVALID_CONSUMER_ID;

        protocolId =
            -1;

        socketOwner =
            -1;

        taskIntervalMs =
            20UL;

        introspection =
            nullptr;

        buffer =
            nullptr;

        lastAliveLog =
            0;

        maximumLoopTimeUs =
            0;

        /*
        * Disconnette preventivamente la callback che cattura this.
        */
        bridgeEngine
            .clearReceiveCallback();

        /*
        * Rimuove registry, management e definizioni dinamiche
        * appartenenti alla configurazione precedente.
        */
        aeeEngine.clear();

        if (transport)
        {
            delete transport;

            transport =
                nullptr;
        }
    }

    // ========================================================
    // RECEIVED VARIABLE HANDLER
    //
    // Metodo associato esclusivamente all'istanza corrente.
    //
    // La lambda registrata nel BridgeEngine cattura this e
    // inoltra qui ogni variabile ricevuta.
    //
    // Di conseguenza:
    //
    // - primaryBridgeChannel aggiorna primary AEE;
    // - knobBridgeChannel aggiorna knob AEE;
    // - i registry non vengono incrociati.
    // ========================================================

    void onVariableReceived(
        AEEVariableBase* variable,
        unsigned long now)
    {
        if (!initialized ||
            !variable)
        {
            return;
        }

        aeeEngine.applyReceived(
            variable,
            now
        );
    }

    const DomoIntrospection*
        introspection =
            nullptr;
public:
    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    AEEBridgeChannel() = default;

    // ========================================================
    // DESTRUCTOR
    //
    // Il transport deve essere distrutto prima degli engine.
    //
    // Poiché transport è un puntatore posseduto, viene eliminato
    // esplicitamente qui. Gli engine vengono poi distrutti
    // automaticamente in ordine inverso di dichiarazione.
    // ========================================================

    ~AEEBridgeChannel()
    {
        if (transport)
        {
            delete transport;

            transport =
                nullptr;
        }
    }

    // ========================================================
    // NON-COPYABLE
    //
    // La classe possiede un transport e contiene engine con
    // stato runtime. La copia produrrebbe:
    //
    // - doppia ownership del transport;
    // - registry duplicati in modo ambiguo;
    // - protocol ID duplicati;
    // - socket owner duplicati.
    //
    // La copia viene quindi esplicitamente disabilitata.
    // ========================================================

    AEEBridgeChannel(
        const AEEBridgeChannel&) =
            delete;

    AEEBridgeChannel& operator=(
        const AEEBridgeChannel&) =
            delete;

    // ========================================================
    // SETUP
    //
    // Inizializza completamente il canale.
    //
    // Sequenza:
    //
    //  1. reset dello stato runtime;
    //  2. acquisizione identità;
    //  3. setup AEEEngine;
    //  4. registrazione sorgente EventManager;
    //  5. collegamento AEE a Buffer ed EventManager;
    //  6. registrazione protocollo DMNetworkManager;
    //  7. acquisizione socket owner;
    //  8. costruzione UdpAdapter;
    //  9. impostazione contesto socket;
    // 10. registrazione callback Bridge contestualizzata;
    // 11. inizializzazione BridgeEngine;
    // 12. configurazione pacer;
    // 13. attivazione runtime.
    //
    // La funzione non registra il task nel TaskEngine. Questa
    // responsabilità rimane al composition root.
    // ========================================================

    bool setup(
        const Config& config,
        DomoManager& manager,
        DMNetworkManager& networkManager,
        const DomoIntrospection* runtimeIntrospection =
            nullptr)
    {
        // ----------------------------------------------------
        // RESET PREVIOUS STATE
        // ----------------------------------------------------

        resetRuntimeState();

        // ----------------------------------------------------
        // IDENTITY
        // ----------------------------------------------------

        channelId =
            config.id;

        channelName =
            config.name;

        namedValueCallback =
            config.namedValueCallback;

        taskIntervalMs =
            config.taskIntervalMs > 0
                ? config.taskIntervalMs
                : 1UL;

        buffer =
            &manager.getBuffer();

        /*
        * Se esiste una configurazione AEE statica, il channel
        * utilizza quella.
        *
        * Se vars/count sono assenti, utilizza Introspection
        * fornita dal composition root.
        */
        const bool hasStaticAEE =
            config.aee.vars != nullptr &&
            config.aee.count > 0;

        introspection =
            hasStaticAEE
                ? nullptr
                : runtimeIntrospection;

        // ----------------------------------------------------
        // DISABLED CHANNEL
        // ----------------------------------------------------

        if (!config.enabled)
        {
            LOG_IF(
                "AEE-BRIDGE",
                "[%u] %s disabled",
                static_cast<unsigned>(
                    channelId
                ),
                validName()
            );

            return true;
        }

        // ----------------------------------------------------
        // NAME VALIDATION
        // ----------------------------------------------------

        if (!config.name ||
            config.name[0] == '\0')
        {
            LOG_EF(
                "AEE-BRIDGE",
                "Setup failed: invalid channel name"
            );

            return false;
        }

        // ----------------------------------------------------
        // UDP VALIDATION
        // ----------------------------------------------------

        const bool dynamicRemoteEndpoint =
            config.remoteIp[0] == 0 &&
            config.remoteIp[1] == 0 &&
            config.remoteIp[2] == 0 &&
            config.remoteIp[3] == 0;

        if (config.localPort == 0)
        {
            LOG_EF(
                "AEE-BRIDGE",
                "[%u] %s invalid local UDP port",
                static_cast<unsigned>(
                    channelId
                ),
                validName()
            );

            return false;
        }

        if (
            !dynamicRemoteEndpoint &&
            config.remotePort == 0
        )
        {
            LOG_EF(
                "AEE-BRIDGE",
                "[%u] %s static endpoint without remote port",
                static_cast<unsigned>(
                    channelId
                ),
                validName()
            );

            return false;
        }

        // ----------------------------------------------------
        // MODEL VALIDATION
        // ----------------------------------------------------

        if (!hasStaticAEE)
        {
            if (!introspection)
            {
                LOG_EF(
                    "AEE-BRIDGE",
                    "[%u] %s has no static AEE and "
                    "no runtime Introspection",
                    static_cast<unsigned>(
                        channelId
                    ),
                    validName()
                );

                return false;
            }

            if (introspection->entityCount() == 0)
            {
                LOG_EF(
                    "AEE-BRIDGE",
                    "[%u] %s runtime Introspection empty",
                    static_cast<unsigned>(
                        channelId
                    ),
                    validName()
                );

                return false;
            }
        }

        // ----------------------------------------------------
        // BRIDGE IDENTITY
        // ----------------------------------------------------

        bridgeEngine.setIdentity(
            channelId,
            validName()
        );

        // ----------------------------------------------------
        // AEE ENGINE
        // ----------------------------------------------------

        const AEEEngine::EntityFilter*
            entityFilter =
                config.filterIntrospection
                    ? &config.introspectionFilter
                    : nullptr;

        const bool aeeSetupCompleted =
            hasStaticAEE
                ? aeeEngine.setup(
                    config.aee
                )
                : aeeEngine.setup(
                    *introspection,
                    entityFilter
                );

        if (!aeeSetupCompleted)
        {
            LOG_EF(
                "AEE-BRIDGE",
                "[%u] %s AEE setup failed: model=%s",
                static_cast<unsigned>(
                    channelId
                ),
                validName(),
                hasStaticAEE
                    ? "STATIC"
                    : "INTROSPECTION"
            );

            return false;
        }

        // ----------------------------------------------------
        // EVENT SOURCE
        // ----------------------------------------------------

        const int registeredSource =
            manager
                .getEventManager()
                .add(
                    validName()
                );

        if (
            registeredSource < 0 ||
            registeredSource > 254
        )
        {
            LOG_EF(
                "AEE-BRIDGE",
                "[%u] %s invalid event source: %d",
                static_cast<unsigned>(
                    channelId
                ),
                validName(),
                registeredSource
            );

            return false;
        }

        eventSource =
            static_cast<uint8_t>(
                registeredSource
            );

        // ----------------------------------------------------
        // RECEIVE CONTEXT
        // ----------------------------------------------------

        aeeEngine.attachReceiveContext(
            manager.getBuffer(),
            manager.getEventManager(),
            eventSource
        );

        // ----------------------------------------------------
        // NETWORK PROTOCOL
        // ----------------------------------------------------

        const uint32_t protocolIntervalMs =
            config.protocolIntervalMs > 0
                ? config.protocolIntervalMs
                : 1UL;

        const uint32_t protocolTimeoutMs =
            config.protocolTimeoutMs > 0
                ? config.protocolTimeoutMs
                : 1UL;

        protocolId =
            networkManager.registerProtocol(
                validName(),
                protocolIntervalMs,
                protocolTimeoutMs,
                1,
                true
            );

        if (protocolId < 0)
        {
            LOG_EF(
                "AEE-BRIDGE",
                "[%u] %s protocol registration failed",
                static_cast<unsigned>(
                    channelId
                ),
                validName()
            );

            return false;
        }

        // ----------------------------------------------------
        // SOCKET OWNER
        // ----------------------------------------------------

        socketOwner =
            networkManager
                .getProtocol(
                    protocolId
                )
                .socketOwner;

        if (socketOwner < 0)
        {
            LOG_EF(
                "AEE-BRIDGE",
                "[%u] %s invalid socket owner",
                static_cast<unsigned>(
                    channelId
                ),
                validName()
            );

            return false;
        }

        // ----------------------------------------------------
        // UDP TRANSPORT
        // ----------------------------------------------------

        transport =
            new UdpAdapter(
                config.remoteIp,
                config.localPort,
                config.remotePort
            );

        if (!transport)
        {
            LOG_EF(
                "AEE-BRIDGE",
                "[%u] %s transport allocation failed",
                static_cast<unsigned>(
                    channelId
                ),
                validName()
            );

            return false;
        }

        transport->setSocketContext(
            networkManager,
            socketOwner
        );

        // ----------------------------------------------------
        // RECEIVE CALLBACK
        // ----------------------------------------------------

        bridgeEngine.setReceiveCallback(
            [this](
                AEEVariableBase* variable,
                unsigned long now)
            {
                onVariableReceived(
                    variable,
                    now
                );
            }
        );

        bridgeEngine.setNamedValueCallback(
            [this](const BridgeNamedValue& value, unsigned long now)
            {
                if (initialized && namedValueCallback)
                {
                    namedValueCallback(value, now);
                }
            }
        );

        // ----------------------------------------------------
        // BRIDGE INITIALIZATION
        // ----------------------------------------------------

        bridgeEngine.init(
            *transport,
            aeeEngine.getRegistry(),
            true
        );

        // ----------------------------------------------------
        // INTROSPECTION SNAPSHOT
        // ----------------------------------------------------

        if (introspection)
        {
            bridgeEngine
                .getBridge()
                .setInitialSnapshotCallback(
                    [this](
                        AEERegistry& registry,
                        unsigned long now) -> size_t
                    {
                        if (!buffer ||
                            !introspection)
                        {
                            return 0;
                        }

                        size_t selectedCount =
                            0;

                        registry.forEach(
                            [this,
                            now,
                            &selectedCount](
                                AEEVariableBase* variable)
                            {
                                if (!variable)
                                {
                                    return;
                                }

                                if (
                                    variable->def.sourceType !=
                                    AEEVarSourceType::BufferArea
                                )
                                {
                                    return;
                                }

                                const int area =
                                    variable
                                        ->def
                                        .bufferArea;

                                if (
                                    area < 0 ||
                                    area >= buffer->size()
                                )
                                {
                                    return;
                                }

                                const DomoIntrospection::Entity*
                                    entity =
                                        introspection->findEntity(
                                            area
                                        );

                                if (
                                    !entity ||
                                    !entity->isReadableBufferEntity()
                                )
                                {
                                    return;
                                }

                                if (
                                    !buffer->HasEverChanged(
                                        area
                                    )
                                )
                                {
                                    return;
                                }

                                const long bufferValue =
                                    buffer->getValueFast(
                                        area
                                    );

                                bool valueApplied =
                                    false;

                                // ====================================
                                // BOOL
                                // ====================================

                                if (
                                    auto* boolVariable =
                                        as<bool>(
                                            variable
                                        )
                                )
                                {
                                    const bool value =
                                        variable->def.bitIndex >= 0
                                            ? bitRead(
                                                bufferValue,
                                                variable->def.bitIndex
                                            )
                                            : bufferValue != 0;

                                    boolVariable->set(
                                        value,
                                        now
                                    );

                                    valueApplied =
                                        true;
                                }

                                // ====================================
                                // INT
                                // ====================================

                                else if (
                                    auto* intVariable =
                                        as<int>(
                                            variable
                                        )
                                )
                                {
                                    const float scaledValue =
                                        static_cast<float>(
                                            bufferValue
                                        ) *
                                        variable->def.scale;

                                    intVariable->set(
                                        static_cast<int>(
                                            scaledValue
                                        ),
                                        now
                                    );

                                    valueApplied =
                                        true;
                                }

                                // ====================================
                                // FLOAT
                                // ====================================

                                else if (
                                    auto* floatVariable =
                                        as<float>(
                                            variable
                                        )
                                )
                                {
                                    const float scaledValue =
                                        static_cast<float>(
                                            bufferValue
                                        ) *
                                        variable->def.scale;

                                    floatVariable->set(
                                        scaledValue,
                                        now
                                    );

                                    valueApplied =
                                        true;
                                }

                                if (!valueApplied)
                                {
                                    LOG_WF(
                                        "AEE-BRIDGE",
                                        "[%u] %s snapshot type "
                                        "non supportato: area=%d name=%s",
                                        static_cast<unsigned>(
                                            channelId
                                        ),
                                        validName(),
                                        area,
                                        variable
                                            ->def
                                            .name
                                            .c_str()
                                    );

                                    return;
                                }

                                variable->forceChanged(
                                    now
                                );

                                ++selectedCount;
                            }
                        );

                        return
                            selectedCount;
                    }
                );
        }
        else
        {
            bridgeEngine
                .getBridge()
                .clearInitialSnapshotCallback();
        }

        // ----------------------------------------------------
        // PACER
        // ----------------------------------------------------

        if (
            !bridgeEngine.setupPacer(
                networkManager,
                protocolId
            )
        )
        {
            LOG_EF(
                "AEE-BRIDGE",
                "[%u] %s pacer setup failed",
                static_cast<unsigned>(
                    channelId
                ),
                validName()
            );

            return false;
        }

        enabled =
            true;

        initialized =
            true;

        LOG_IF(
            "AEE-BRIDGE",
            "[%u] %s ready: model=%s endpoint=%s "
            "protocol=%d owner=%d source=%u "
            "localPort=%u remotePort=%u vars=%u",
            static_cast<unsigned>(
                channelId
            ),
            validName(),
            hasStaticAEE
                ? "STATIC"
                : "INTROSPECTION",
            dynamicRemoteEndpoint
                ? "DYNAMIC"
                : "STATIC",
            static_cast<int>(
                protocolId
            ),
            static_cast<int>(
                socketOwner
            ),
            static_cast<unsigned>(
                eventSource
            ),
            static_cast<unsigned>(
                config.localPort
            ),
            static_cast<unsigned>(
                config.remotePort
            ),
            static_cast<unsigned>(
                aeeEngine
                    .getRegistry()
                    .size()
            )
        );

        return true;
    }

    // ========================================================
    // LOOP
    //
    // Esegue esclusivamente il BridgeEngine appartenente al
    // canale.
    //
    // Non vengono eseguite allocazioni dinamiche.
    // ========================================================

    void loop(
        unsigned long now)
    {
        if (!enabled ||
            !initialized)
        {
            return;
        }

        const unsigned long startUs =
            micros();

        bridgeEngine.loop(
            now
        );

        const unsigned long elapsedUs =
            micros() - startUs;

        if (
            elapsedUs >
            maximumLoopTimeUs
        )
        {
            maximumLoopTimeUs =
                elapsedUs;
        }

        if (elapsedUs > 100000UL)
        {
            LOG_WF(
                "AEE-BRIDGE",
                "[%u] %s slow loop: %lu us",
                static_cast<unsigned>(
                    channelId
                ),
                validName(),
                elapsedUs
            );
        }

        if (
            lastAliveLog == 0 ||
            now - lastAliveLog >=
                5000UL
        )
        {
            LOG_DF(
                "AEE-BRIDGE",
                "[%u] %s active: "
                "now=%lu elapsed=%lu us max=%lu us state=%d",
                static_cast<unsigned>(
                    channelId
                ),
                validName(),
                now,
                elapsedUs,
                maximumLoopTimeUs,
                static_cast<int>(
                    bridgeEngine
                        .getBridge()
                        .getState()
                )
            );

            lastAliveLog =
                now;
        }
    }

    // ========================================================
    // UPDATE FROM POLLED SOURCES
    //
    // Costruisce gli aggiornamenti AEE derivati dalle sorgenti
    // acquisite dal DomoManager.
    //
    // La funzione utilizza esclusivamente l'AEEManagement e il
    // registry appartenenti al canale corrente.
    //
    // Deve essere richiamata dopo il completamento del ciclo
    // backend/frontend.
    // ========================================================

    void updateFromPolledSources(
        DomoManager& manager,
        unsigned long now)
    {
        if (!enabled ||
            !initialized)
        {
            return;
        }

        AEEManagement* aeeManager =
            aeeEngine.getManager();

        if (!aeeManager)
        {
            return;
        }

        std::vector<DMAEE::Update>
            updates;

        updates.reserve(
            16
        );

        if (
            !DMAEE::
                BuildUpdatesFromPolledSources(
                    *aeeManager,
                    manager.getBuffer(),
                    updates
                )
        )
        {
            return;
        }

        if (updates.empty())
        {
            return;
        }

        DMAEE::ApplyUpdates(
            updates,
            now
        );
    }

    // ========================================================
    // APPLY BUFFER EVENT
    //
    // Converte una variazione di area Buffer negli aggiornamenti
    // appartenenti esclusivamente al registry di questo canale.
    //
    // Può essere richiamata da una callback EventManager comune
    // che distribuisce l'evento a tutti i canali attivi.
    //
    // Restituisce true quando è stato generato almeno un update.
    // ========================================================

    bool applyBufferEvent(
        int area,
        long value,
        unsigned long now)
    {
        if (!enabled ||
            !initialized ||
            area < 0)
        {
            return false;
        }

        // ----------------------------------------------------
        // INTROSPECTION FILTER
        //
        // Se Introspection è presente, l'area deve essere una
        // entity Buffer pubblica e leggibile.
        //
        // Per il modello AEE statico il filtro viene applicato
        // direttamente dalle AEEVarDef configurate.
        // ----------------------------------------------------

        if (introspection)
        {
            const DomoIntrospection::Entity*
                entity =
                    introspection->findEntity(
                        area
                    );

            if (
                !entity ||
                !entity
                    ->isReadableBufferEntity()
            )
            {
                return false;
            }
        }

        AEEManagement* aeeManager =
            aeeEngine.getManager();

        if (!aeeManager)
        {
            return false;
        }

        std::vector<DMAEE::Update>
            updates;

        updates.reserve(
            8
        );

        if (
            !DMAEE::
                BuildUpdatesFromBufferArea(
                    *aeeManager,
                    area,
                    value,
                    updates
                )
        )
        {
            return false;
        }

        if (updates.empty())
        {
            return false;
        }

        DMAEE::ApplyUpdates(
            updates,
            now
        );

        return true;
    }

    // ========================================================
    // ACCESSORS
    // ========================================================

    bool isEnabled() const
    {
        return
            enabled;
    }

    bool isInitialized() const
    {
        return
            initialized;
    }

    uint8_t getChannelId() const
    {
        return
            channelId;
    }

    const char* getChannelName() const
    {
        return
            validName();
    }

    uint8_t getEventSource() const
    {
        return
            eventSource;
    }

    ProtocolId getProtocolId() const
    {
        return
            protocolId;
    }

    SocketOwnerId getSocketOwner() const
    {
        return
            socketOwner;
    }

    uint32_t getTaskIntervalMs() const
    {
        return
            taskIntervalMs;
    }

    unsigned long getMaximumLoopTimeUs() const
    {
        return
            maximumLoopTimeUs;
    }

    AEEEngine& getAEEEngine()
    {
        return
            aeeEngine;
    }

    const AEEEngine& getAEEEngine() const
    {
        return
            aeeEngine;
    }

    AEERegistry& getRegistry()
    {
        return
            aeeEngine.getRegistry();
    }

    const AEERegistry& getRegistry() const
    {
        return
            aeeEngine.getRegistry();
    }

    AEEManagement* getAEEManager()
    {
        return
            aeeEngine.getManager();
    }

    BridgeEngine& getBridgeEngine()
    {
        return
            bridgeEngine;
    }

    const BridgeEngine& getBridgeEngine() const
    {
        return
            bridgeEngine;
    }

    UdpAdapter* getTransport()
    {
        return
            transport;
    }

    const UdpAdapter* getTransport() const
    {
        return
            transport;
    }
};

// ============================================================
// AEE BRIDGE ORCHESTRATOR
//
// Coordina più istanze AEEBridgeChannel indipendenti.
//
// Ogni AEEBridgeChannel possiede autonomamente:
//
// - AEEEngine;
// - BridgeEngine;
// - AEERegistry;
// - AEEManagement;
// - UdpAdapter;
// - protocollo DMNetworkManager;
// - socket owner;
// - sorgente EventManager;
// - stato runtime e diagnostica.
//
// L'orchestrator non implementa direttamente:
//
// - protocolli AEE;
// - trasporto UDP;
// - pacing di rete;
// - conversione delle variabili AEE;
// - registrazione delle variabili;
// - scrittura nel Buffer;
// - ricezione dei pacchetti.
//
// Queste responsabilità rimangono incapsulate nel singolo
// AEEBridgeChannel.
//
// Responsabilità dell'orchestrator:
//
// - possedere la collezione dei canali;
// - inizializzare i canali configurati;
// - eseguire il loop di tutti i canali;
// - distribuire gli eventi Buffer;
// - distribuire gli aggiornamenti provenienti dal polling;
// - fornire accesso controllato ai singoli canali;
// - fornire accesso ai registry AEE per Introspection;
// - produrre diagnostica aggregata.
//
// La quantità massima è intenzionalmente statica per evitare:
//
// - allocazioni durante il runtime;
// - riallocazioni;
// - spostamenti di oggetti non copiabili;
// - invalidazione dei riferimenti ai registry;
// - invalidazione delle lambda che catturano this.
//
// AEEBridgeChannel non è copiabile né spostabile, quindi non
// deve essere conservato in std::vector.
// ============================================================

class AEEBridgeOrchestrator
{
public:

    // ========================================================
    // CONSTANTS
    //
    // Il valore corrente consente:
    //
    //   channel 0 = bridge AEE principale;
    //   channel 1 = bridge AEE Knob.
    //
    // Può essere aumentato successivamente senza modificare
    // l'interfaccia pubblica dell'orchestrator.
    // ========================================================

    static constexpr size_t
        MAX_CHANNELS =
            2;

private:

    // ========================================================
    // CHANNEL STORAGE
    //
    // Storage statico e contiguo.
    //
    // Non utilizziamo std::vector perché AEEBridgeChannel:
    //
    // - possiede risorse runtime;
    // - non è copiabile;
    // - non è spostabile;
    // - registra callback che catturano this.
    // ========================================================

    AEEBridgeChannel channels[
        MAX_CHANNELS
    ];

    // ========================================================
    // CONFIGURED COUNT
    //
    // Numero di slot definiti dalla configurazione.
    //
    // Comprende anche eventuali canali configurati ma
    // esplicitamente disabilitati.
    // ========================================================

    size_t configuredCount =
        0;

    // ========================================================
    // INITIALIZED COUNT
    //
    // Numero di canali che hanno completato effettivamente:
    //
    // - setup AEE;
    // - registrazione EventManager;
    // - registrazione protocollo;
    // - acquisizione socket owner;
    // - creazione transport;
    // - inizializzazione BridgeEngine;
    // - setup pacer.
    // ========================================================

    size_t initializedCount =
        0;

    // ========================================================
    // ENABLED COUNT
    //
    // Numero di configurazioni con enabled=true.
    //
    // Può essere maggiore di initializedCount se uno dei setup
    // fallisce.
    // ========================================================

    size_t enabledCount =
        0;

    // ========================================================
    // RUNTIME STATE
    //
    // configured:
    //   setup() è stato invocato con parametri formalmente
    //   validi.
    //
    // initialized:
    //   tutti i canali abilitati hanno completato il setup.
    // ========================================================

    bool configured =
        false;

    bool initialized =
        false;

    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    unsigned long loopCount =
        0;

    unsigned long eventCount =
        0;

    unsigned long handledEventCount =
        0;

    unsigned long polledUpdateCount =
        0;

    // ========================================================
    // NORMALIZED COUNT
    //
    // Limita il numero richiesto alla capacità reale dello
    // storage.
    // ========================================================

    static size_t normalizeCount(
        size_t count)
    {
        return
            count < MAX_CHANNELS
                ? count
                : MAX_CHANNELS;
    }

    // ========================================================
    // VALIDATE INDEX
    // ========================================================

    bool isValidIndex(
        size_t index) const
    {
        return
            index <
                configuredCount &&
            index <
                MAX_CHANNELS;
    }

public:

    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    AEEBridgeOrchestrator() =
        default;

    // ========================================================
    // DESTRUCTOR
    //
    // I canali vengono distrutti automaticamente in ordine
    // inverso.
    //
    // Ogni AEEBridgeChannel rilascia autonomamente:
    //
    // - UdpAdapter;
    // - BridgeEngine;
    // - pacer;
    // - AEEManagement;
    // - registry AEE.
    // ========================================================

    ~AEEBridgeOrchestrator() =
        default;

    // ========================================================
    // NON-COPYABLE
    //
    // Copiare l'orchestrator duplicherebbe ownership e stato
    // di protocolli, transport, pacer e registry.
    // ========================================================

    AEEBridgeOrchestrator(
        const AEEBridgeOrchestrator&) =
            delete;

    AEEBridgeOrchestrator& operator=(
        const AEEBridgeOrchestrator&) =
            delete;

    // ========================================================
    // NON-MOVABLE
    //
    // Le callback dei BridgeEngine catturano l'indirizzo del
    // rispettivo AEEBridgeChannel.
    //
    // Spostare l'orchestrator invaliderebbe tali indirizzi.
    // ========================================================

    AEEBridgeOrchestrator(
        AEEBridgeOrchestrator&&) =
            delete;

    AEEBridgeOrchestrator& operator=(
        AEEBridgeOrchestrator&&) =
            delete;

    // ========================================================
    // SETUP STATIC CHANNELS
    //
    // Prima fase di inizializzazione.
    //
    // Configura esclusivamente i channel che possiedono:
    //
    //     aee.vars != nullptr
    //     aee.count > 0
    //
    // I channel privi di configurazione AEE statica vengono
    // rimandati alla fase setupIntrospectionChannels().
    // ========================================================

    bool setupStaticChannels(
        AEEBridgeChannel::Config* configurations,
        size_t count,
        DomoManager& manager,
        DMNetworkManager& networkManager)
    {
        configured =
            true;

        initialized =
            false;

        configuredCount =
            normalizeCount(
                count
            );

        initializedCount =
            0;

        enabledCount =
            0;

        loopCount =
            0;

        eventCount =
            0;

        handledEventCount =
            0;

        polledUpdateCount =
            0;

        if (!configurations ||
            configuredCount == 0)
        {
            initialized =
                true;

            return true;
        }

        bool setupCompleted =
            true;

        for (
            size_t index = 0;
            index < configuredCount;
            ++index
        )
        {
            AEEBridgeChannel::Config& config =
                configurations[index];

            if (!config.enabled)
            {
                continue;
            }

            ++enabledCount;

            const bool hasStaticAEE =
                config.aee.vars != nullptr &&
                config.aee.count > 0;

            if (!hasStaticAEE)
            {
                /*
                * Channel Introspection:
                * inizializzato nella seconda fase.
                */
                continue;
            }

            const bool channelResult =
                channels[index].setup(
                    config,
                    manager,
                    networkManager,
                    nullptr
                );

            if (!channelResult)
            {
                setupCompleted =
                    false;

                LOG_EF(
                    "AEE-BRIDGE",
                    "Static channel setup failed: "
                    "index=%u id=%u name=%s",
                    static_cast<unsigned>(
                        index
                    ),
                    static_cast<unsigned>(
                        config.id
                    ),
                    config.name
                        ? config.name
                        : "AEE-BRIDGE"
                );

                continue;
            }

            if (
                channels[index]
                    .isInitialized()
            )
            {
                ++initializedCount;
            }
        }

        /*
        * Non impostiamo ancora initialized=true se esistono
        * channel Introspection non inizializzati.
        */
        initialized =
            setupCompleted &&
            initializedCount ==
                enabledCount;

        LOG_IF(
            "AEE-BRIDGE",
            "Static phase completed: "
            "configured=%u enabled=%u initialized=%u",
            static_cast<unsigned>(
                configuredCount
            ),
            static_cast<unsigned>(
                enabledCount
            ),
            static_cast<unsigned>(
                initializedCount
            )
        );

        return
            setupCompleted;
    }

    // ========================================================
    // SETUP INTROSPECTION CHANNELS
    //
    // Seconda fase di inizializzazione.
    //
    // Configura esclusivamente i channel abilitati privi di
    // configurazione AEE statica.
    //
    // Tali channel utilizzano il modello appena costruito da
    // DomoIntrospection.
    // ========================================================

    bool setupIntrospectionChannels(
        AEEBridgeChannel::Config* configurations,
        size_t count,
        DomoManager& manager,
        DMNetworkManager& networkManager,
        const DomoIntrospection& runtimeIntrospection)
    {
        if (!configurations)
        {
            LOG_EF(
                "AEE-BRIDGE",
                "Introspection phase failed: "
                "configurations NULL"
            );

            return false;
        }

        if (
            runtimeIntrospection
                .entityCount() == 0
        )
        {
            LOG_EF(
                "AEE-BRIDGE",
                "Introspection phase failed: "
                "model empty"
            );

            return false;
        }

        const size_t normalizedCount =
            normalizeCount(
                count
            );

        /*
        * Mantiene coerente il numero configurato anche se questa
        * funzione viene richiamata dopo la fase statica.
        */
        configuredCount =
            normalizedCount;

        bool setupCompleted =
            true;

        for (
            size_t index = 0;
            index < normalizedCount;
            ++index
        )
        {
            AEEBridgeChannel::Config& config =
                configurations[index];

            if (!config.enabled)
            {
                continue;
            }

            const bool hasStaticAEE =
                config.aee.vars != nullptr &&
                config.aee.count > 0;

            if (hasStaticAEE)
            {
                /*
                * Già gestito durante setupStaticChannels().
                */
                continue;
            }

            LOG_IF(
                "AEE-BRIDGE",
                "Introspection channel setup: "
                "index=%u id=%u name=%s entities=%u",
                static_cast<unsigned>(
                    index
                ),
                static_cast<unsigned>(
                    config.id
                ),
                config.name
                    ? config.name
                    : "AEE-BRIDGE",
                static_cast<unsigned>(
                    runtimeIntrospection
                        .entityCount()
                )
            );

            const bool channelResult =
                channels[index].setup(
                    config,
                    manager,
                    networkManager,
                    &runtimeIntrospection
                );

            if (!channelResult)
            {
                setupCompleted =
                    false;

                LOG_EF(
                    "AEE-BRIDGE",
                    "Introspection channel setup failed: "
                    "index=%u id=%u name=%s",
                    static_cast<unsigned>(
                        index
                    ),
                    static_cast<unsigned>(
                        config.id
                    ),
                    config.name
                        ? config.name
                        : "AEE-BRIDGE"
                );

                continue;
            }

            if (
                channels[index]
                    .isInitialized()
            )
            {
                ++initializedCount;
            }
        }

        initialized =
            setupCompleted &&
            initializedCount ==
                enabledCount;

        LOG_IF(
            "AEE-BRIDGE",
            "Introspection phase completed: "
            "configured=%u enabled=%u initialized=%u "
            "running=%u",
            static_cast<unsigned>(
                configuredCount
            ),
            static_cast<unsigned>(
                enabledCount
            ),
            static_cast<unsigned>(
                initializedCount
            ),
            initialized
                ? 1U
                : 0U
        );

        return
            initialized;
    }

    // ========================================================
    // SETUP
    //
    // Inizializza tutti i canali dichiarati nella configurazione.
    //
    // Parametri:
    //
    // configurations:
    //   array di configurazioni AEEBridgeChannel.
    //
    // count:
    //   numero di elementi presenti nell'array.
    //
    // manager:
    //   DomoManager condiviso.
    //
    // networkManager:
    //   DMNetworkManager condiviso.
    //
    // Comportamento:
    //
    // - count viene limitato a MAX_CHANNELS;
    // - ogni canale mantiene risorse completamente separate;
    // - un canale disabilitato non viene considerato errore;
    // - il fallimento di un canale abilitato rende fallito il
    //   setup complessivo;
    // - i canali già inizializzati rimangono disponibili per
    //   diagnostica, ma il loop aggregato non viene attivato.
    //
    // La registrazione del TaskEngine rimane responsabilità
    // del composition root.
    // ========================================================

    bool setup(
        const AEEBridgeChannel::Config* configurations,
        size_t count,
        DomoManager& manager,
        DMNetworkManager& networkManager,
        const DomoIntrospection* introspection =
            nullptr)
    {
        configured =
            false;

        initialized =
            false;

        configuredCount =
            0;

        initializedCount =
            0;

        enabledCount =
            0;

        loopCount =
            0;

        eventCount =
            0;

        handledEventCount =
            0;

        polledUpdateCount =
            0;

        if (!configurations ||
            count == 0)
        {
            configured =
                true;

            initialized =
                true;

            LOG_IF(
                "AEE-BRIDGE",
                "No AEE bridge channels configured"
            );

            return true;
        }

        configuredCount =
            normalizeCount(
                count
            );

        configured =
            true;

        if (count > MAX_CHANNELS)
        {
            LOG_WF(
                "AEE-BRIDGE",
                "Configured channel count truncated: "
                "requested=%u maximum=%u",
                static_cast<unsigned>(
                    count
                ),
                static_cast<unsigned>(
                    MAX_CHANNELS
                )
            );
        }

        // ----------------------------------------------------
        // UNIQUE IDS
        // ----------------------------------------------------

        for (
            size_t index = 0;
            index < configuredCount;
            ++index
        )
        {
            for (
                size_t previous = 0;
                previous < index;
                ++previous
            )
            {
                if (
                    configurations[index].id ==
                    configurations[previous].id
                )
                {
                    LOG_EF(
                        "AEE-BRIDGE",
                        "Duplicate channel id: "
                        "index=%u previous=%u id=%u",
                        static_cast<unsigned>(
                            index
                        ),
                        static_cast<unsigned>(
                            previous
                        ),
                        static_cast<unsigned>(
                            configurations[index].id
                        )
                    );

                    return false;
                }
            }
        }

        bool setupCompleted =
            true;

        for (
            size_t index = 0;
            index < configuredCount;
            ++index
        )
        {
            const AEEBridgeChannel::Config&
                channelConfig =
                    configurations[index];

            if (channelConfig.enabled)
            {
                ++enabledCount;
            }

            /*
            * Se il channel non possiede configurazione AEE
            * statica, riceve Introspection.
            */
            const bool hasStaticAEE =
                channelConfig.aee.vars != nullptr &&
                channelConfig.aee.count > 0;

            const DomoIntrospection*
                channelIntrospection =
                    hasStaticAEE
                        ? nullptr
                        : introspection;

            const bool channelResult =
                channels[index].setup(
                    channelConfig,
                    manager,
                    networkManager,
                    channelIntrospection
                );

            if (!channelResult)
            {
                setupCompleted =
                    false;

                LOG_EF(
                    "AEE-BRIDGE",
                    "Channel setup failed: "
                    "index=%u id=%u name=%s",
                    static_cast<unsigned>(
                        index
                    ),
                    static_cast<unsigned>(
                        channelConfig.id
                    ),
                    channelConfig.name &&
                    channelConfig.name[0] != '\0'
                        ? channelConfig.name
                        : "AEE-BRIDGE"
                );

                continue;
            }

            if (
                channels[index]
                    .isInitialized()
            )
            {
                ++initializedCount;
            }
        }

        initialized =
            setupCompleted &&
            initializedCount ==
                enabledCount;

        LOG_IF(
            "AEE-BRIDGE",
            "Orchestrator setup completed: "
            "configured=%u enabled=%u initialized=%u "
            "running=%u",
            static_cast<unsigned>(
                configuredCount
            ),
            static_cast<unsigned>(
                enabledCount
            ),
            static_cast<unsigned>(
                initializedCount
            ),
            initialized
                ? 1U
                : 0U
        );

        return
            initialized;
    }

    // ========================================================
    // LOOP
    //
    // Esegue sequenzialmente tutti i canali inizializzati.
    //
    // Ogni channel applica internamente:
    //
    // - controllo enabled;
    // - controllo initialized;
    // - BridgeEngine::loop();
    // - diagnostica dei tempi;
    // - pacing tramite protocolId dedicato.
    //
    // L'esecuzione sequenziale evita concorrenza sui servizi
    // condivisi del DMNetworkManager.
    // ========================================================

    void loop(
        unsigned long now)
    {
        if (!initialized)
        {
            return;
        }

        ++loopCount;

        for (
            size_t index = 0;
            index < configuredCount;
            ++index
        )
        {
            if (
                !channels[index]
                    .isInitialized()
            )
            {
                continue;
            }

            channels[index].loop(
                now
            );
        }
    }

    // ========================================================
    // APPLY BUFFER EVENT
    //
    // Distribuisce un evento Buffer a tutti i canali attivi.
    //
    // Ogni canale consulta esclusivamente il proprio:
    //
    // - AEEManagement;
    // - AEERegistry;
    // - insieme di AEEVarDef.
    //
    // Di conseguenza uno stesso evento può:
    //
    // - essere ignorato da tutti i canali;
    // - appartenere a un solo canale;
    // - appartenere a più canali.
    //
    // Restituisce true se almeno un canale ha generato un
    // aggiornamento AEE.
    // ========================================================

    bool applyBufferEvent(
        int area,
        long value,
        unsigned long now)
    {
        if (!initialized ||
            area < 0)
        {
            return false;
        }

        ++eventCount;

        bool handled =
            false;

        for (
            size_t index = 0;
            index < configuredCount;
            ++index
        )
        {
            if (
                !channels[index]
                    .isInitialized()
            )
            {
                continue;
            }

            if (
                channels[index]
                    .applyBufferEvent(
                        area,
                        value,
                        now
                    )
            )
            {
                handled =
                    true;
            }
        }

        if (handled)
        {
            ++handledEventCount;
        }

        return
            handled;
    }

    // ========================================================
    // UPDATE FROM POLLED SOURCES
    //
    // Propaga a ogni canale i valori acquisiti dal ciclo di
    // polling DomoManager.
    //
    // Ogni canale costruisce aggiornamenti esclusivamente per
    // le proprie definizioni AEE.
    //
    // Questa funzione deve essere richiamata al completamento
    // del ciclo backend/frontend.
    // ========================================================

    void updateFromPolledSources(
        DomoManager& manager,
        unsigned long now)
    {
        if (!initialized)
        {
            return;
        }

        ++polledUpdateCount;

        for (
            size_t index = 0;
            index < configuredCount;
            ++index
        )
        {
            if (
                !channels[index]
                    .isInitialized()
            )
            {
                continue;
            }

            channels[index]
                .updateFromPolledSources(
                    manager,
                    now
                );
        }
    }

    // ========================================================
    // CONFIGURED STATE
    // ========================================================

    bool isConfigured() const
    {
        return
            configured;
    }

    // ========================================================
    // INITIALIZED STATE
    // ========================================================

    bool isInitialized() const
    {
        return
            initialized;
    }

    // ========================================================
    // OPERATIONAL STATE
    //
    // Un orchestrator è operativo quando:
    //
    // - setup è stato completato;
    // - almeno un canale è abilitato;
    // - tutti i canali abilitati sono inizializzati.
    // ========================================================

    bool isOperational() const
    {
        return
            initialized &&
            enabledCount > 0 &&
            initializedCount ==
                enabledCount;
    }

    // ========================================================
    // COUNTS
    // ========================================================

    size_t getConfiguredCount() const
    {
        return
            configuredCount;
    }

    size_t getEnabledCount() const
    {
        return
            enabledCount;
    }

    size_t getInitializedCount() const
    {
        return
            initializedCount;
    }

    // ========================================================
    // CHANNEL BY INDEX
    // ========================================================

    AEEBridgeChannel* getChannel(
        size_t index)
    {
        if (!isValidIndex(
                index))
        {
            return nullptr;
        }

        return
            &channels[index];
    }

    const AEEBridgeChannel* getChannel(
        size_t index) const
    {
        if (!isValidIndex(
                index))
        {
            return nullptr;
        }

        return
            &channels[index];
    }

    // ========================================================
    // CHANNEL BY ID
    // ========================================================

    AEEBridgeChannel* findChannelById(
        uint8_t channelId)
    {
        for (
            size_t index = 0;
            index < configuredCount;
            ++index
        )
        {
            if (
                channels[index]
                    .getChannelId() ==
                channelId
            )
            {
                return
                    &channels[index];
            }
        }

        return nullptr;
    }

    const AEEBridgeChannel*
    findChannelById(
        uint8_t channelId) const
    {
        for (
            size_t index = 0;
            index < configuredCount;
            ++index
        )
        {
            if (
                channels[index]
                    .getChannelId() ==
                channelId
            )
            {
                return
                    &channels[index];
            }
        }

        return nullptr;
    }

    // ========================================================
    // PRIMARY CHANNEL
    //
    // Per compatibilità con la configurazione corrente e con
    // Introspection, il primo canale viene considerato il
    // canale AEE principale.
    // ========================================================

    AEEBridgeChannel* getPrimaryChannel()
    {
        return
            getChannel(
                0
            );
    }

    const AEEBridgeChannel*
    getPrimaryChannel() const
    {
        return
            getChannel(
                0
            );
    }

    // ========================================================
    // PRIMARY REGISTRY
    //
    // Restituisce nullptr quando:
    //
    // - non esiste alcun channel;
    // - il primary channel non è inizializzato.
    //
    // Questo accessor può essere utilizzato temporaneamente da
    // Introspection finché non verrà aggiunto il supporto per
    // registry multipli.
    // ========================================================

    AEERegistry* getPrimaryRegistry()
    {
        AEEBridgeChannel* channel =
            getPrimaryChannel();

        if (!channel ||
            !channel->isInitialized())
        {
            return nullptr;
        }

        return
            &channel->getRegistry();
    }

    const AEERegistry*
    getPrimaryRegistry() const
    {
        const AEEBridgeChannel* channel =
            getPrimaryChannel();

        if (!channel ||
            !channel->isInitialized())
        {
            return nullptr;
        }

        return
            &channel->getRegistry();
    }

    // ========================================================
    // REGISTRY BY CHANNEL ID
    // ========================================================

    AEERegistry* getRegistryById(
        uint8_t channelId)
    {
        AEEBridgeChannel* channel =
            findChannelById(
                channelId
            );

        if (!channel ||
            !channel->isInitialized())
        {
            return nullptr;
        }

        return
            &channel->getRegistry();
    }

    const AEERegistry*
    getRegistryById(
        uint8_t channelId) const
    {
        const AEEBridgeChannel* channel =
            findChannelById(
                channelId
            );

        if (!channel ||
            !channel->isInitialized())
        {
            return nullptr;
        }

        return
            &channel->getRegistry();
    }

    // ========================================================
    // DIAGNOSTIC COUNTERS
    // ========================================================

    unsigned long getLoopCount() const
    {
        return
            loopCount;
    }

    unsigned long getEventCount() const
    {
        return
            eventCount;
    }

    unsigned long
    getHandledEventCount() const
    {
        return
            handledEventCount;
    }

    unsigned long
    getPolledUpdateCount() const
    {
        return
            polledUpdateCount;
    }

    // ========================================================
    // DIAGNOSTIC REPORT
    //
    // Produce un riepilogo sintetico dell'orchestrator e di
    // ogni canale configurato.
    // ========================================================

    void diagnosticReport() const
    {
        Serial.println();

        Serial.println(
            "===== AEE BRIDGE ORCHESTRATOR ====="
        );

        Serial.print(
            "Configured: "
        );

        Serial.println(
            configured
                ? "YES"
                : "NO"
        );

        Serial.print(
            "Initialized: "
        );

        Serial.println(
            initialized
                ? "YES"
                : "NO"
        );

        Serial.print(
            "Configured channels: "
        );

        Serial.println(
            static_cast<unsigned>(
                configuredCount
            )
        );

        Serial.print(
            "Enabled channels: "
        );

        Serial.println(
            static_cast<unsigned>(
                enabledCount
            )
        );

        Serial.print(
            "Initialized channels: "
        );

        Serial.println(
            static_cast<unsigned>(
                initializedCount
            )
        );

        Serial.print(
            "Loop count: "
        );

        Serial.println(
            loopCount
        );

        Serial.print(
            "Buffer events: "
        );

        Serial.println(
            eventCount
        );

        Serial.print(
            "Handled events: "
        );

        Serial.println(
            handledEventCount
        );

        Serial.print(
            "Polled updates: "
        );

        Serial.println(
            polledUpdateCount
        );

        for (
            size_t index = 0;
            index < configuredCount;
            ++index
        )
        {
            const AEEBridgeChannel&
                channel =
                    channels[index];

            Serial.print(
                "Channel["
            );

            Serial.print(
                static_cast<unsigned>(
                    index
                )
            );

            Serial.print(
                "] id="
            );

            Serial.print(
                static_cast<unsigned>(
                    channel.getChannelId()
                )
            );

            Serial.print(
                " name="
            );

            Serial.print(
                channel.getChannelName()
            );

            Serial.print(
                " initialized="
            );

            Serial.print(
                channel.isInitialized()
                    ? "YES"
                    : "NO"
            );

            Serial.print(
                " protocol="
            );

            Serial.print(
                static_cast<int>(
                    channel.getProtocolId()
                )
            );

            Serial.print(
                " owner="
            );

            Serial.print(
                static_cast<int>(
                    channel.getSocketOwner()
                )
            );

            Serial.print(
                " source="
            );

            Serial.print(
                static_cast<unsigned>(
                    channel.getEventSource()
                )
            );

            Serial.print(
                " vars="
            );

            Serial.print(
                static_cast<unsigned>(
                    channel
                        .getRegistry()
                        .size()
                )
            );

            Serial.print(
                " maxLoopUs="
            );

            Serial.println(
                channel
                    .getMaximumLoopTimeUs()
            );
        }

        Serial.println(
            "===== END AEE BRIDGE ORCHESTRATOR ====="
        );

        Serial.println();
    }
};

class FrontendNetwork
{
public:

    static constexpr size_t MAX_MQTT_CLIENTS = 4;

    struct SolarTCP
    {
        DMPlatform::Network::TCPClient client;
    };

    struct ModbusTCP
    {
        DMPlatform::Network::TCPClient client;

        DMPlatform::Modbus::TCPClient modbus{
            client
        };
    };


    struct MQTT
    {
        DMPlatform::Network::TCPClient client;

        PubSubClient pubSub;


        MQTT()
            : client(),
              pubSub()
        {
            pubSub.setClient(
                client
            );
        }
    };


    explicit FrontendNetwork(
        size_t mqttClientCount)
    {
        _mqttClientCount =
            mqttClientCount;


        if (_mqttClientCount >
            MAX_MQTT_CLIENTS)
        {
            _mqttClientCount =
                MAX_MQTT_CLIENTS;
        }
    }


    ModbusTCP& modbusTCP()
    {
        return _modbusTCP;
    }


    MQTT& mqtt(
        size_t index)
    {
        return _mqtt[index];
    }


    size_t mqttClientCount() const
    {
        return _mqttClientCount;
    }

    SolarTCP& solar(
        size_t index)
    {
        return
            _solar[index];
    }

    const SolarTCP& solar(
        size_t index) const
    {
        return
            _solar[index];
    }
    
private:
    SolarTCP _solar [
                        FrontendConfig::
                            MAX_SOLAR_INVERTERS
                    ];

    ModbusTCP _modbusTCP;

    size_t _mqttClientCount = 0;

    MQTT _mqtt[
        MAX_MQTT_CLIENTS
    ];
};

// ============================================================
// SOLAR ENGINE
//
// Coordina il sottosistema Solar multi-inverter.
//
// Responsabilità:
//   - gestione della collezione di SolarOrchestrator;
//   - associazione tra configurazione e singolo orchestrator;
//   - assegnazione di un client TCP dedicato a ogni inverter;
//   - propagazione delle callback verso il frontend;
//   - esecuzione del loop di tutti gli inverter configurati;
//   - accesso centralizzato a stato, snapshot e diagnostica;
//   - inoltro dei comandi raw al corretto orchestrator;
//   - arresto coordinato dell'intero sottosistema.
//
// Ogni SolarOrchestrator gestisce autonomamente:
//   - creazione e distruzione del transport Serial/TCP;
//   - creazione dell'implementazione ISolarInverter;
//   - inizializzazione e capability discovery;
//   - polling del protocollo;
//   - stato runtime e connessione;
//   - retry e recovery della comunicazione;
//   - routing Snapshot verso DomoManager;
//   - diagnostica del singolo inverter.
//
// Deve essere dichiarato DOPO FrontendNetwork perché assegna
// a ogni SolarOrchestrator il relativo client TCP tramite:
//
//   network.solar(index).client
//
// SolarEngine non possiede direttamente transport o inverter:
// la loro ownership appartiene al singolo SolarOrchestrator.
// ============================================================

class SolarEngine
{
public:

    // ========================================================
    // PUBLIC TYPES
    //
    // SolarEngine espone gli stessi tipi del singolo
    // SolarOrchestrator, mantenendo stabile l'API usata
    // dal frontend.
    // ========================================================

    using Snapshot =
        SolarOrchestrator::Snapshot;

    using Diagnostics =
        SolarOrchestrator::Diagnostics;

    using RuntimeState =
        SolarOrchestrator::RuntimeState;

    using SnapshotChangedCallback =
        SolarOrchestrator::
            SnapshotChangedCallback;

    using FrameReceivedCallback =
        SolarOrchestrator::
            FrameReceivedCallback;

    using CommandStateCallback =
        SolarOrchestrator::
            CommandStateCallback;

private:

    // ========================================================
    // CONSTANTS
    // ========================================================

    static constexpr uint8_t
        MAX_ORCHESTRATORS =
            FrontendConfig::
                MAX_SOLAR_INVERTERS;

    static constexpr unsigned long
        COMMUNICATION_STALE_MS =
            30000UL;

    // ========================================================
    // SHARED REFERENCES
    // ========================================================

    static inline DomoManager*
        frontendManager =
            nullptr;

    // ========================================================
    // ORCHESTRATORS
    //
    // Ogni SolarOrchestrator gestisce un singolo inverter:
    //
    //   - transport;
    //   - protocollo;
    //   - polling;
    //   - runtime;
    //   - recovery;
    //   - routing eventi;
    //   - diagnostica.
    //
    // SolarEngine coordina esclusivamente la collezione.
    // ========================================================

    static inline SolarOrchestrator
        orchestrators[MAX_ORCHESTRATORS];

    static inline uint8_t
        orchestratorCount =
            0;

    // ========================================================
    // SLOT METADATA
    //
    // Conservati direttamente da SolarEngine per evitare
    // dipendenze da getter specifici di SolarOrchestrator.
    // ========================================================

    static inline const FrontendConfig::Solar*
        configurations[MAX_ORCHESTRATORS] {};

    static inline uint8_t
        eventSources[MAX_ORCHESTRATORS] {};

    // ========================================================
    // GENERIC FRONTEND STATUS
    //
    // Ogni orchestrator possiede una word tecnica separata
    // dallo status funzionale dell'inverter.
    // ========================================================

    static inline FrontendStatus
        frontendStatus[MAX_ORCHESTRATORS];

    // ========================================================
    // FRONTEND CALLBACKS
    //
    // Le callback vengono conservate anche prima del Setup.
    // ========================================================

    static inline SnapshotChangedCallback
        snapshotChangedCallback =
            nullptr;

    static inline FrameReceivedCallback
        frameReceivedCallback =
            nullptr;

    static inline CommandStateCallback
        commandStateCallback =
            nullptr;

private:

    // ========================================================
    // INDEX VALIDATION
    // ========================================================

    static bool IsValidId(
        uint8_t orchestratorId)
    {
        return
            orchestratorId <
                orchestratorCount &&
            orchestratorId <
                MAX_ORCHESTRATORS;
    }

    // ========================================================
    // INTERNAL ACCESSORS
    // ========================================================

    static SolarOrchestrator*
    Find(
        uint8_t orchestratorId)
    {
        if (!IsValidId(
                orchestratorId))
        {
            return nullptr;
        }

        return
            &orchestrators[
                orchestratorId
            ];
    }

    static const SolarOrchestrator*
    FindConst(
        uint8_t orchestratorId)
    {
        if (!IsValidId(
                orchestratorId))
        {
            return nullptr;
        }

        return
            &orchestrators[
                orchestratorId
            ];
    }

    // ========================================================
    // UPDATE FRONTEND STATUS
    //
    // Traduce lo stato specifico di SolarOrchestrator nella
    // word tecnica comune FrontendStatus.
    //
    // La funzione non pubblica eventi.
    // ========================================================

    static void UpdateFrontendStatus(
        uint8_t orchestratorId,
        unsigned long now)
    {
        SolarOrchestrator* orchestrator =
            Find(
                orchestratorId
            );

        if (!orchestrator)
            return;

        FrontendStatus& status =
            frontendStatus[
                orchestratorId
            ];

        const RuntimeState& runtime =
            orchestrator
                ->GetRuntime();

        const bool enabled =
            orchestrator
                ->IsEnabled();

        const bool running =
            orchestrator
                ->IsRunning();

        const bool initialized =
            orchestrator
                ->IsInitialized();

        const unsigned long lastSuccess =
            runtime.recovery
                .lastSuccessfulCommunication;

        const unsigned long nextRetry =
            runtime.recovery
                .nextInitializationAttempt;

        const bool hasSuccessfulCommunication =
            lastSuccess != 0;

        const bool communicationStale =
            hasSuccessfulCommunication &&
            static_cast<unsigned long>(
                now - lastSuccess
            ) >
            30000UL;

        /*
        * Connected significa:
        * almeno una risposta valida ricevuta
        * e non scaduta.
        */
        const bool connected =
            hasSuccessfulCommunication &&
            !communicationStale;

        /*
        * Active significa:
        * frontend realmente operativo.
        */
        const bool active =
            initialized &&
            connected;

        const bool recovering =
            enabled &&
            running &&
            !initialized &&
            nextRetry != 0;

        const bool degraded =
            enabled &&
            running &&
            initialized &&
            !active;

        const bool fault =
            enabled &&
            (
                !running ||
                (
                    !active &&
                    !recovering &&
                    !degraded
                )
            );

        status.Set(
            FrontendStatus::Flag::Enabled,
            enabled
        );

        status.Set(
            FrontendStatus::Flag::Running,
            running
        );

        status.Set(
            FrontendStatus::Flag::Initialized,
            initialized
        );

        status.Set(
            FrontendStatus::Flag::Connected,
            connected
        );

        status.Set(
            FrontendStatus::Flag::Active,
            active
        );

        status.Set(
            FrontendStatus::Flag::Recovering,
            recovering
        );

        status.Set(
            FrontendStatus::Flag::Degraded,
            degraded
        );

        status.Set(
            FrontendStatus::Flag::Fault,
            fault
        );

        LOG_DF(
            "SOLAR",
            "FrontendStatus id=%u init=%u conn=%u active=%u lastSuccess=%lu",
            static_cast<unsigned>(
                orchestratorId
            ),
            initialized,
            connected,
            active,
            lastSuccess
        );
    }

    // ========================================================
    // PUBLISH FRONTEND STATUS
    //
    // Pubblica la word solo quando cambia.
    //
    // La classe FrontendStatus mantiene internamente:
    //
    //   - word corrente;
    //   - ultima word consumata;
    //   - stato di cambiamento.
    // ========================================================

    static void PublishFrontendStatus(
        uint8_t orchestratorId,
        unsigned long now)
    {
        if (!IsValidId(
                orchestratorId))
        {
            return;
        }

        const FrontendConfig::Solar*
            configuration =
                configurations[
                    orchestratorId
                ];

        if (!configuration)
            return;

        const int area =
            configuration
                ->areas
                .frontendStatus;

        if (area < 0)
            return;

        if (!configuration->publishEvents)
            return;

        if (!frontendManager)
            return;

        UpdateFrontendStatus(
            orchestratorId,
            now
        );

        FrontendStatus::Word
            statusValue =
                0;

        if (
            !frontendStatus[
                orchestratorId
            ].ConsumeChanged(
                statusValue
            ))
        {
            return;
        }

        frontendManager
            ->forceEvent(
                area,
                static_cast<long>(
                    statusValue
                ),
                eventSources[
                    orchestratorId
                ]
            );

        LOG_DF(
            "SOLAR",
            "Orchestrator %u frontend status=0x%04X",
            static_cast<unsigned>(
                orchestratorId
            ),
            static_cast<unsigned>(
                statusValue
            )
        );
    }

public:

    // ========================================================
    // SHUTDOWN
    //
    // Arresta tutti gli orchestrator configurati.
    //
    // Le callback registrate su SolarEngine vengono conservate
    // per consentire un successivo Setup.
    // ========================================================

    static void Shutdown()
    {
        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .Shutdown();

            frontendStatus[index]
                .Reset();

            frontendStatus[index]
                .MarkConsumed();

            configurations[index] =
                nullptr;

            eventSources[index] =
                255;
        }

        orchestratorCount =
            0;

        frontendManager =
            nullptr;
    }

    // ========================================================
    // RESET
    //
    // Esegue uno shutdown completo e azzera anche le callback
    // frontend.
    // ========================================================

    static void Reset()
    {
        Shutdown();

        snapshotChangedCallback =
            nullptr;

        frameReceivedCallback =
            nullptr;

        commandStateCallback =
            nullptr;
    }

    // ========================================================
    // SETUP
    //
    // Costruisce la collezione di SolarOrchestrator.
    //
    // Ogni inverter TCP riceve il proprio client:
    //
    //   network.solar(0).client
    //   network.solar(1).client
    //   network.solar(2).client
    //   network.solar(3).client
    //
    // Gli inverter seriali non richiedono un client TCP.
    // ========================================================

    static void Setup(
        DomoManager& manager,
        DMNetworkManager& networkManager,
        FrontendNetwork& network,
        const FrontendConfig::SolarInverters&
            solarConfigurations,
        DMNetworkManager::ProtocolId protocolId,
        uint8_t firstEventSource)
    {
        Shutdown();

        frontendManager =
            &manager;

        orchestratorCount =
            solarConfigurations.count <
                MAX_ORCHESTRATORS
            ? solarConfigurations.count
            : MAX_ORCHESTRATORS;

        LOG_IF(
            "SOLAR",
            "SolarEngine setup: configured=%u activeSlots=%u",
            static_cast<unsigned>(
                solarConfigurations.count
            ),
            static_cast<unsigned>(
                orchestratorCount
            )
        );

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            const FrontendConfig::Solar&
                configuration =
                    solarConfigurations
                        .inverter[index];

            SolarOrchestrator& orchestrator =
                orchestrators[index];

            FrontendStatus& status =
                frontendStatus[index];

            // ------------------------------------------------
            // SLOT METADATA
            // ------------------------------------------------

            configurations[index] =
                &configuration;

            eventSources[index] =
                static_cast<uint8_t>(
                    firstEventSource +
                    index
                );

            // ------------------------------------------------
            // RESET FRONTEND STATUS
            //
            // Reset forza la prima pubblicazione.
            // ------------------------------------------------

            status.Reset();

            // ------------------------------------------------
            // TCP CLIENT ASSIGNMENT
            // ------------------------------------------------

            DMPlatform::Network::TCPClient*
                assignedTcpClient =
                    nullptr;

            if (
                configuration.transport ==
                FrontendConfig::Solar::
                    TransportType::TCP_CLIENT)
            {
                assignedTcpClient =
                    &network
                        .solar(index)
                        .client;
            }

            // ------------------------------------------------
            // FRONTEND CALLBACKS
            // ------------------------------------------------

            orchestrator
                .SetSnapshotChangedCallback(
                    snapshotChangedCallback
                );

            orchestrator
                .SetFrameReceivedCallback(
                    frameReceivedCallback
                );

            orchestrator
                .SetCommandStateCallback(
                    commandStateCallback
                );

            // ------------------------------------------------
            // SINGLE ORCHESTRATOR SETUP
            // ------------------------------------------------

            const bool configured =
                orchestrator.Setup(
                    index,
                    manager,
                    networkManager,
                    assignedTcpClient,
                    configuration,
                    protocolId,
                    eventSources[index]
                );

            const unsigned long now =
                millis();

            UpdateFrontendStatus(
                index,
                now
            );

            if (!configured)
            {
                status.Set(
                    FrontendStatus::Flag::Fault,
                    true
                );

                status.Set(
                    FrontendStatus::Flag::Recovering,
                    false
                );

                LOG_EF(
                    "SOLAR",
                    "Orchestrator %u setup failed",
                    static_cast<unsigned>(
                        index
                    )
                );
            }
            else
            {
                LOG_IF(
                    "SOLAR",
                    "Orchestrator %u configured: "
                    "enabled=%u transport=%u status=0x%04X",
                    static_cast<unsigned>(
                        index
                    ),
                    configuration.enabled
                        ? 1U
                        : 0U,
                    static_cast<unsigned>(
                        configuration.transport
                    ),
                    static_cast<unsigned>(
                        status.GetWord()
                    )
                );
            }

            PublishFrontendStatus(
                index,
                now
            );
        }

        // ----------------------------------------------------
        // RESET UNUSED SLOTS
        // ----------------------------------------------------

        for (
            uint8_t index = orchestratorCount;
            index < MAX_ORCHESTRATORS;
            ++index)
        {
            configurations[index] =
                nullptr;

            eventSources[index] =
                255;

            frontendStatus[index]
                .Reset();

            frontendStatus[index]
                .MarkConsumed();
        }
    }

    // ========================================================
    // LOOP
    //
    // Esegue il loop di tutti gli orchestrator configurati.
    //
    // Dopo ogni ciclo aggiorna e pubblica la word tecnica
    // soltanto quando cambia.
    // ========================================================

    static void Loop(
        unsigned long now)
    {
        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .Loop(now);

            PublishFrontendStatus(
                index,
                now
            );
        }
    }

    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    static void DiagnosticReport()
    {
        Serial.println();
        Serial.println(
            "===== SOLAR ENGINE ====="
        );

        Serial.print(
            "Orchestrator count: "
        );

        Serial.println(
            orchestratorCount
        );

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            Serial.print(
                "Frontend status ["
            );

            Serial.print(
                index
            );

            Serial.print(
                "]: 0x"
            );

            Serial.println(
                frontendStatus[index]
                    .GetWord(),
                HEX
            );

            orchestrators[index]
                .DiagnosticReport();
        }

        Serial.println(
            "===== END SOLAR ENGINE ====="
        );
    }

    static void DiagnosticReport(
        uint8_t orchestratorId)
    {
        SolarOrchestrator* orchestrator =
            Find(
                orchestratorId
            );

        if (!orchestrator)
        {
            Serial.print(
                "Solar orchestrator not found: "
            );

            Serial.println(
                orchestratorId
            );

            return;
        }

        Serial.print(
            "Frontend status: 0x"
        );

        Serial.println(
            frontendStatus[
                orchestratorId
            ].GetWord(),
            HEX
        );

        orchestrator
            ->DiagnosticReport();
    }

    // ========================================================
    // COUNT
    // ========================================================

    static uint8_t GetEngineCount()
    {
        return
            orchestratorCount;
    }

    static uint8_t GetOrchestratorCount()
    {
        return
            orchestratorCount;
    }

    // ========================================================
    // ORCHESTRATOR ACCESS
    // ========================================================

    static SolarOrchestrator*
    Get(
        uint8_t orchestratorId)
    {
        return
            Find(
                orchestratorId
            );
    }

    static const SolarOrchestrator*
    GetConst(
        uint8_t orchestratorId)
    {
        return
            FindConst(
                orchestratorId
            );
    }

    // ========================================================
    // FRONTEND STATUS ACCESS
    // ========================================================

    static const FrontendStatus*
    GetFrontendStatus(
        uint8_t orchestratorId = 0)
    {
        if (!IsValidId(
                orchestratorId))
        {
            return nullptr;
        }

        return
            &frontendStatus[
                orchestratorId
            ];
    }

    static FrontendStatus::Word
    GetFrontendStatusWord(
        uint8_t orchestratorId = 0)
    {
        const FrontendStatus* status =
            GetFrontendStatus(
                orchestratorId
            );

        return status
            ? status->GetWord()
            : 0;
    }

    static bool IsFrontendOperational(
        uint8_t orchestratorId = 0)
    {
        const FrontendStatus* status =
            GetFrontendStatus(
                orchestratorId
            );

        return
            status &&
            status->IsOperational();
    }

    // ========================================================
    // ORCHESTRATOR STATUS
    // ========================================================

    static bool IsInitialized(
        uint8_t orchestratorId = 0)
    {
        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return
            orchestrator &&
            orchestrator
                ->IsInitialized();
    }

    static bool IsRunning(
        uint8_t orchestratorId = 0)
    {
        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return
            orchestrator &&
            orchestrator
                ->IsRunning();
    }

    static bool IsConnected(
        uint8_t orchestratorId = 0)
    {
        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return
            orchestrator &&
            orchestrator
                ->IsConnected();
    }

    // ========================================================
    // RUNTIME ACCESSOR
    // ========================================================

    static const RuntimeState&
    GetRuntime(
        uint8_t orchestratorId = 0)
    {
        static const RuntimeState
            emptyRuntime {};

        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetRuntime()
            : emptyRuntime;
    }

    // ========================================================
    // INVERTER ACCESSORS
    // ========================================================

    static ISolarInverter*
    GetInstance(
        uint8_t orchestratorId = 0)
    {
        SolarOrchestrator* orchestrator =
            Find(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetInstance()
            : nullptr;
    }

    static const ISolarInverter*
    GetInstanceConst(
        uint8_t orchestratorId = 0)
    {
        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetInstance()
            : nullptr;
    }

    // ========================================================
    // SNAPSHOT ACCESSOR
    // ========================================================

    static const Snapshot&
    GetSnapshot(
        uint8_t orchestratorId = 0)
    {
        static const Snapshot
            emptySnapshot {};

        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetSnapshot()
            : emptySnapshot;
    }

    // ========================================================
    // DIAGNOSTICS ACCESSOR
    // ========================================================

    static const Diagnostics&
    GetDiagnostics(
        uint8_t orchestratorId = 0)
    {
        static const Diagnostics
            emptyDiagnostics {};

        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetDiagnostics()
            : emptyDiagnostics;
    }

    // ========================================================
    // RAW COMMAND
    // ========================================================

    static bool SendRawCommand(
        uint8_t orchestratorId,
        const String& command,
        String& response)
    {
        SolarOrchestrator* orchestrator =
            Find(
                orchestratorId
            );

        if (!orchestrator)
            return false;

        return orchestrator
            ->SendRawCommand(
                command,
                response
            );
    }

    // ========================================================
    // LEGACY RAW COMMAND
    // ========================================================

    static bool SendRawCommand(
        const String& command,
        String& response)
    {
        return SendRawCommand(
            0,
            command,
            response
        );
    }

    // ========================================================
    // SNAPSHOT CALLBACK
    // ========================================================

    static void SetSnapshotChangedCallback(
        SnapshotChangedCallback callback)
    {
        snapshotChangedCallback =
            callback;

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .SetSnapshotChangedCallback(
                    callback
                );
        }
    }

    // ========================================================
    // FRAME CALLBACK
    // ========================================================

    static void SetFrameReceivedCallback(
        FrameReceivedCallback callback)
    {
        frameReceivedCallback =
            callback;

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .SetFrameReceivedCallback(
                    callback
                );
        }
    }

    // ========================================================
    // COMMAND STATE CALLBACK
    // ========================================================

    static void SetCommandStateCallback(
        CommandStateCallback callback)
    {
        commandStateCallback =
            callback;

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .SetCommandStateCallback(
                    callback
                );
        }
    }
};

// ============================================================
// MQTT FRONTEND ENGINE
// Integrazione MQTT <-> DomoManager
//
// Versione rifattorizzata:
//   - nessuna dipendenza da DomoIntrospection
//   - nessuna dipendenza da DMHADiscovery
//   - nessun runtime Home Assistant
//   - trasporto raw esposto tramite API generiche
//   - callback raw e routed generiche
//   - mapping statici, dirty queue e anti-eco preservati
// ============================================================

class MQTTEngine
{
public:
    static constexpr uint8_t MAX_CLIENTS = 4;
    static constexpr uint8_t MAX_MAPPINGS = 64;

    using RawMessageReceivedCallback = void (*)(
        uint8_t clientIndex,
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        size_t length
    );

    using ServiceLoopCallback = void (*)(unsigned long now);

    using RoutedEventCallback = void (*)(
        uint8_t originClientIndex,
        int area,
        long value
    );

    // ========================================================================
    // INBOUND MESSAGE CALLBACK
    //
    // Callback applicativa unica per tutti i messaggi MQTT normalizzati.
    //
    // Riceve:
    //
    //   - mapping numerici;
    //   - mapping testuali;
    //   - messaggi raw non risolti;
    //   - comandi Home Assistant area;
    //   - comandi Home Assistant RuntimeStore.
    //
    // I puntatori contenuti in MQTTInboundMessage sono validi solamente
    // durante l'esecuzione sincrona della callback.
    // ========================================================================

    using InboundMessageReceivedCallback =
        void (*)(
            const MQTTInboundMessage& message
        );


    struct RuntimeClient
    {
        uint8_t index = 0;
        const FrontendConfig::MQTT::Client* cfg = nullptr;
        EthernetClient* eth = nullptr;
        PubSubClient* pubSub = nullptr;
        MQTT* mqtt = nullptr;
        unsigned long lastHeartbeat = 0;
        bool initialized = false;
        SocketHandle socket;
    };

    struct RuntimeCommandState
    {
        long value = 0;
        unsigned long timestamp = 0;
        bool initialized = false;
    };

    struct RuntimeMapping
    {
        uint8_t clientIndex = 0;
        const FrontendConfig::MQTT::Client* client = nullptr;
        const FrontendConfig::MQTT::Device* device = nullptr;
        const FrontendConfig::MQTT::Mapping* mapping = nullptr;
        RuntimeClient* runtimeClient = nullptr;
        RuntimeCommandState command;
        long lastRaw = 0;
        bool dirty = false;
        bool initialized = false;
    };

private:
    static DomoManager* manager;
    static const FrontendConfig::MQTT* configuration;
    static size_t clientCount;
    static size_t mappingCount;
    static unsigned long lastLoop;
    static DMNetworkManager* networkManager;
    static DMNetworkManager::ProtocolId mqttProtocolId;
    static uint8_t mqttSource;
    static uint16_t dirtyMappings[MAX_MAPPINGS];
    static uint16_t dirtyCount;

    static inline InboundMessageReceivedCallback
        inboundMessageReceivedCallback = nullptr;
    static inline ServiceLoopCallback serviceLoopCallback = nullptr;
    static inline RoutedEventCallback routedEventCallback = nullptr;
    static inline RawMessageReceivedCallback rawMessageReceivedCallback = nullptr;

   

    static RuntimeClient* getClients()
    {
        static RuntimeClient clients[MAX_CLIENTS];
        return clients;
    }

    static RuntimeMapping* getMappings()
    {
        static RuntimeMapping mappings[MAX_MAPPINGS];
        return mappings;
    }

    static void EnqueueDirty(uint16_t mappingIndex)
    {
        if (mappingIndex >= mappingCount)
            return;

        RuntimeMapping& rm = getMappings()[mappingIndex];

        if (rm.dirty)
            return;

        if (dirtyCount >= MAX_MAPPINGS)
        {
            LOG_WF("MQTT", "Dirty queue piena");
            return;
        }

        rm.dirty = true;
        dirtyMappings[dirtyCount++] = mappingIndex;
    }

    static bool CanRead(const FrontendConfig::MQTT::Mapping* mapping)
    {
        if (!mapping)
            return false;

        return
            mapping->direction == FrontendConfig::MQTT::Mapping::Direction::READ ||
            mapping->direction == FrontendConfig::MQTT::Mapping::Direction::READ_WRITE;
    }

    static bool CanWrite(const FrontendConfig::MQTT::Mapping* mapping)
    {
        if (!mapping)
            return false;

        return
            mapping->direction == FrontendConfig::MQTT::Mapping::Direction::WRITE ||
            mapping->direction == FrontendConfig::MQTT::Mapping::Direction::READ_WRITE;
    }

    static void BuildRuntimeMappings(const FrontendConfig::MQTT& cfg)
    {
        RuntimeMapping* mappings = getMappings();
        mappingCount = 0;

        if (!cfg.clients)
            return;

        for (size_t c = 0; c < cfg.clientCount; ++c)
        {
            const FrontendConfig::MQTT::Client& client = cfg.clients[c];

            if (!client.enabled || !client.devices)
                continue;

            for (size_t d = 0; d < client.deviceCount; ++d)
            {
                const FrontendConfig::MQTT::Device& device = client.devices[d];

                for (size_t m = 0;
                     m < FrontendConfig::MQTT::Device::MAX_MAPPINGS;
                     ++m)
                {
                    const FrontendConfig::MQTT::Mapping& mapping = device.mappings[m];

                    if (!mapping.field)
                        continue;

                    if (mappingCount >= MAX_MAPPINGS)
                    {
                        LOG_WF(
                            "MQTT",
                            "MAX_MAPPINGS raggiunto: %u",
                            static_cast<unsigned>(MAX_MAPPINGS)
                        );
                        return;
                    }

                    RuntimeMapping& rm = mappings[mappingCount++];
                    rm = RuntimeMapping{};
                    rm.clientIndex = static_cast<uint8_t>(c);
                    rm.client = &client;
                    rm.device = &device;
                    rm.mapping = &mapping;
                }
            }
        }
    }

    static void BindRuntimeMappings()
    {
        RuntimeClient* clients = getClients();
        RuntimeMapping* mappings = getMappings();

        for (size_t m = 0; m < mappingCount; ++m)
        {
            RuntimeMapping& rm = mappings[m];
            rm.runtimeClient = nullptr;

            for (size_t c = 0; c < clientCount; ++c)
            {
                if (clients[c].index == rm.clientIndex)
                {
                    rm.runtimeClient = &clients[c];
                    break;
                }
            }
        }
    }

    static void InitializeRuntimeValues()
    {
        RuntimeMapping* mappings = getMappings();

        for (size_t i = 0; i < mappingCount; ++i)
        {
            mappings[i].lastRaw = 0;
            mappings[i].dirty = false;
            mappings[i].initialized = false;
            mappings[i].command = RuntimeCommandState{};
        }
    }

    static void PublishDirty()
    {
        RuntimeMapping* mappings = getMappings();
        uint16_t writePosition = 0;

        for (uint16_t i = 0; i < dirtyCount; ++i)
        {
            const uint16_t mappingIndex = dirtyMappings[i];

            if (mappingIndex >= mappingCount)
                continue;

            RuntimeMapping& rm = mappings[mappingIndex];
            RuntimeClient* rc = rm.runtimeClient;

            if (!rc ||
                !rc->initialized ||
                !rc->mqtt ||
                rc->mqtt->state() != 0)
            {
                dirtyMappings[writePosition++] = mappingIndex;
                continue;
            }

            if (rc->mqtt->publishMapping(rm.device, rm.mapping, rm.lastRaw))
            {
                rm.dirty = false;
                rm.initialized = true;
            }
            else
            {
                dirtyMappings[writePosition++] = mappingIndex;
            }
        }

        dirtyCount = writePosition;
    }

    static void PublishMappedEventExcept(
        int excludedClientIndex,
        int area,
        long value)
    {
        if (area < 0)
            return;

        RuntimeMapping* mappings = getMappings();

        for (size_t i = 0; i < mappingCount; ++i)
        {
            RuntimeMapping& rm = mappings[i];

            if (!rm.mapping || rm.mapping->area != area)
                continue;

            if (!CanWrite(rm.mapping))
                continue;

            if (excludedClientIndex >= 0 &&
                rm.clientIndex == static_cast<uint8_t>(excludedClientIndex))
            {
                continue;
            }

            rm.lastRaw = value;
            EnqueueDirty(static_cast<uint16_t>(i));
        }
    }

    // ========================================================================
    // NOTIFY INBOUND MESSAGE
    //
    // Punto unico di uscita applicativo di MQTTEngine.
    // ========================================================================

    static void NotifyInboundMessage(
        const MQTTInboundMessage& message)
    {
        if (!inboundMessageReceivedCallback)
            return;

        if (!message.valid())
        {
            LOG_WF(
                "MQTT",
                "Inbound message non valido: "
                "client=%u origin=%u",
                static_cast<unsigned>(
                    message.clientIndex
                ),
                static_cast<unsigned>(
                    message.origin
                )
            );

            return;
        }

        inboundMessageReceivedCallback(
            message
        );
    }

    // ========================================================================
    // MQTT INBOUND CALLBACK
    //
    // Riceve i messaggi già normalizzati dal layer MQTT low-level.
    //
    // MAPPING_NUMERIC:
    //
    //   - aggiorna RuntimeMapping;
    //   - aggiorna Buffer;
    //   - inoltra il valore agli altri client MQTT;
    //   - notifica Home Assistant;
    //   - inoltra il messaggio all'applicazione.
    //
    // MAPPING_TEXT / RAW:
    //
    //   - inoltra direttamente il messaggio all'applicazione.
    // ========================================================================

    static void MQTTInboundCallback(
        const MQTTInboundMessage& message)
    {
        using Origin =
            MQTTInboundMessage::Origin;

        // ========================================================
        // NON-NUMERIC MESSAGE
        // ========================================================

        if (message.origin !=
            Origin::MAPPING_NUMERIC)
        {
            NotifyInboundMessage(
                message
            );

            return;
        }

        // ========================================================
        // NUMERIC MAPPING VALIDATION
        // ========================================================

        const FrontendConfig::MQTT::Mapping* mapping =
            message.mapping;

        if (!mapping)
            return;

        const long value =
            message.integerValue;

        // ========================================================
        // COMMAND STATE
        // ========================================================

        RuntimeMapping* mappings =
            getMappings();

        bool unchanged =
            false;

        for (size_t i = 0;
            i < mappingCount;
            ++i)
        {
            RuntimeMapping& runtimeMapping =
                mappings[i];

            if (runtimeMapping.mapping !=
                mapping)
            {
                continue;
            }

            if (runtimeMapping.command.initialized &&
                runtimeMapping.command.value ==
                    value)
            {
                unchanged =
                    true;
            }
            else
            {
                runtimeMapping.command.initialized =
                    true;

                runtimeMapping.command.value =
                    value;

                runtimeMapping.command.timestamp =
                    millis();
            }

            break;
        }

        /*
        * La callback applicativa viene comunque invocata perché rappresenta
        * un messaggio MQTT realmente ricevuto.
        *
        * Il controllo unchanged impedisce solamente di applicare nuovamente
        * lo stesso comando al Buffer.
        */

        NotifyInboundMessage(
            message
        );

        if (unchanged)
            return;

        // ========================================================
        // BUFFER ROUTING
        // ========================================================

        if (mapping->area < 0 ||
            !manager ||
            !CanRead(mapping))
        {
            return;
        }

        manager->forceEvent(
            mapping->area,
            value,
            mqttSource
        );

        // ========================================================
        // CROSS-CLIENT ROUTING
        // ========================================================

        PublishMappedEventExcept(
            static_cast<int>(
                message.clientIndex
            ),
            mapping->area,
            value
        );

        // ========================================================
        // HOME ASSISTANT ROUTING
        // ========================================================

        if (routedEventCallback)
        {
            routedEventCallback(
                message.clientIndex,
                mapping->area,
                value
            );
        }

        LOG_IF(
            "MQTT_ROUTE",
            "Comando mapping applicato: "
            "originClient=%u device=%s field=%s "
            "area=%d value=%ld",
            static_cast<unsigned>(
                message.clientIndex
            ),
            message.device &&
            message.device->id
                ? message.device->id
                : "?",
            mapping->field
                ? mapping->field
                : "?",
            mapping->area,
            value
        );
    }

    static void MQTTRawMessageCallback(
        uint8_t clientIndex,
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        size_t length)
    {
        if (!rawMessageReceivedCallback)
            return;

        rawMessageReceivedCallback(
            clientIndex,
            backend,
            topic,
            payload,
            length
        );
    }


public:
    static void Setup(
        DomoManager& dm,
        DMNetworkManager& netManager,
        FrontendNetwork& network,
        size_t mqttClientCount,
        const FrontendConfig::MQTT& cfg,
        DMNetworkManager::ProtocolId protocol,
        uint8_t eventSource)
    {
        // ========================================================
        // SHARED REFERENCES
        // ========================================================

        manager =
            &dm;

        networkManager =
            &netManager;

        configuration =
            &cfg;

        mqttProtocolId =
            protocol;

        mqttSource =
            eventSource;

        // ========================================================
        // CALLBACK RESET
        //
        // La callback applicativa unificata verrà registrata dal
        // frontend dopo Setup().
        // ========================================================

        inboundMessageReceivedCallback =
            nullptr;

        serviceLoopCallback =
            nullptr;

        routedEventCallback =
            nullptr;

        rawMessageReceivedCallback =
            nullptr;

        // ========================================================
        // RUNTIME STORAGE
        // ========================================================

        RuntimeClient* clients =
            getClients();

        RuntimeMapping* mappings =
            getMappings();

        memset(
            mappings,
            0,
            sizeof(RuntimeMapping) *
                MAX_MAPPINGS
        );

        // ========================================================
        // DESTROY PREVIOUS CLIENTS
        // ========================================================

        for (size_t i = 0;
            i < MAX_CLIENTS;
            ++i)
        {
            if (clients[i].mqtt)
            {
                delete clients[i].mqtt;

                clients[i].mqtt =
                    nullptr;
            }

            clients[i] =
                RuntimeClient{};
        }

        clientCount =
            0;

        mappingCount =
            0;

        dirtyCount =
            0;

        // ========================================================
        // CONFIGURATION VALIDATION
        // ========================================================

        if (!cfg.enabled)
        {
            LOG_IF(
                "MQTT",
                "MQTT disabilitato"
            );

            return;
        }

        if (!cfg.clients ||
            cfg.clientCount == 0)
        {
            LOG_IF(
                "MQTT",
                "Nessun client MQTT configurato"
            );

            return;
        }

        if (mqttClientCount == 0)
        {
            LOG_IF(
                "MQTT",
                "Nessun slot MQTT disponibile"
            );

            return;
        }

        // ========================================================
        // BUILD STATIC DEVICE/MAPPING INDEX
        // ========================================================

        BuildRuntimeMappings(
            cfg
        );

        // ========================================================
        // SOCKET OWNER
        // ========================================================

        const SocketManager::OwnerId mqttSocketOwner =
            networkManager
                ->getProtocol(
                    mqttProtocolId
                )
                .socketOwner;

        if (mqttSocketOwner < 0)
        {
            LOG_EF(
                "MQTT",
                "Socket owner MQTT non valido: protocol=%d",
                static_cast<int>(
                    mqttProtocolId
                )
            );

            return;
        }

        // ========================================================
        // CLIENT LIMIT
        // ========================================================

        size_t maxClients =
            cfg.clientCount;

        if (maxClients >
            MAX_CLIENTS)
        {
            maxClients =
                MAX_CLIENTS;
        }

        if (maxClients >
            mqttClientCount)
        {
            maxClients =
                mqttClientCount;
        }

        // ========================================================
        // CLIENT CREATION
        // ========================================================

        for (size_t configIndex = 0;
            configIndex < maxClients;
            ++configIndex)
        {
            const FrontendConfig::MQTT::Client& clientCfg =
                cfg.clients[
                    configIndex
                ];

            if (!clientCfg.enabled)
                continue;

            RuntimeClient& rc =
                clients[
                    clientCount
                ];

            rc =
                RuntimeClient{};

            rc.index =
                static_cast<uint8_t>(
                    configIndex
                );

            rc.cfg =
                &clientCfg;

            rc.eth =
                &network
                    .mqtt(
                        configIndex
                    )
                    .client;

            rc.pubSub =
                &network
                    .mqtt(
                        configIndex
                    )
                    .pubSub;

            // ====================================================
            // LOW-LEVEL MQTT INSTANCE
            // ====================================================

            rc.mqtt =
                new MQTT(
                    *rc.eth,
                    *rc.pubSub,
                    static_cast<uint8_t>(
                        configIndex
                    ),
                    &clientCfg,
                    clientCfg.devices,
                    clientCfg.deviceCount
                );

            if (!rc.mqtt)
            {
                LOG_EF(
                    "MQTT",
                    "Client[%u] allocazione MQTT fallita",
                    static_cast<unsigned>(
                        configIndex
                    )
                );

                continue;
            }

            // ====================================================
            // SOCKET REGISTRATION
            // ====================================================

            rc.socket.setup(
                networkManager,
                mqttSocketOwner,
                static_cast<int>(
                    rc.index
                ),
                SocketManager::SocketKind::
                    TCP_CLIENT
            );

            // ====================================================
            // MQTT CALLBACKS
            //
            // inbound:
            //   mapping numerici, testuali e raw applicativi.
            //
            // raw:
            //   ponte infrastrutturale verso DMHAOrchestrator.
            // ====================================================

            rc.mqtt
                ->setInboundMessageCallback(
                    MQTTInboundCallback
                );

            rc.mqtt
                ->setRawMessageCallback(
                    MQTTRawMessageCallback
                );

            rc.mqtt
                ->setBackend(
                    clientCfg.backend
                );

            // ====================================================
            // LOW-LEVEL BEGIN
            // ====================================================

            if (!rc.mqtt->begin())
            {
                LOG_EF(
                    "MQTT",
                    "Client[%u] begin fallito: %s",
                    static_cast<unsigned>(
                        configIndex
                    ),
                    clientCfg.name
                        ? clientCfg.name
                        : "unnamed"
                );

                delete rc.mqtt;

                rc.mqtt =
                    nullptr;

                continue;
            }

            // ====================================================
            // MQTT BUFFER
            // ====================================================

            const uint16_t mqttBufferSize =
                clientCfg.backend ==
                    FrontendConfig::MQTT::Client::
                        Backend::HOME_ASSISTANT
                    ? 2048U
                    : 512U;

            if (!rc.pubSub
                    ->setBufferSize(
                        mqttBufferSize
                    ) ||
                rc.pubSub
                    ->getBufferSize() <
                        mqttBufferSize)
            {
                LOG_EF(
                    "MQTT",
                    "Client[%u] buffer non valido: "
                    "requested=%u actual=%u",
                    static_cast<unsigned>(
                        configIndex
                    ),
                    static_cast<unsigned>(
                        mqttBufferSize
                    ),
                    static_cast<unsigned>(
                        rc.pubSub
                            ->getBufferSize()
                    )
                );

                delete rc.mqtt;

                rc.mqtt =
                    nullptr;

                continue;
            }

            rc.initialized =
                true;

            LOG_IF(
                "MQTT",
                "Client[%u]: %s backend=%u "
                "broker=%s:%u devices=%u buffer=%u",
                static_cast<unsigned>(
                    configIndex
                ),
                clientCfg.name
                    ? clientCfg.name
                    : "unnamed",
                static_cast<unsigned>(
                    clientCfg.backend
                ),
                clientCfg.broker
                    .toString()
                    .c_str(),
                static_cast<unsigned>(
                    clientCfg.port
                ),
                static_cast<unsigned>(
                    clientCfg.deviceCount
                ),
                static_cast<unsigned>(
                    rc.pubSub
                        ->getBufferSize()
                )
            );

            ++clientCount;
        }

        // ========================================================
        // RUNTIME MAPPING INITIALIZATION
        // ========================================================

        BindRuntimeMappings();

        InitializeRuntimeValues();

        LOG_IF(
            "MQTT",
            "MQTT Setup completato: "
            "clients=%u mappings=%u protocol=%d",
            static_cast<unsigned>(
                clientCount
            ),
            static_cast<unsigned>(
                mappingCount
            ),
            static_cast<int>(
                mqttProtocolId
            )
        );
    }

    static void Loop(unsigned long now)
    {
        if (!configuration ||
            !configuration->enabled ||
            !networkManager)
        {
            return;
        }

        static bool mqttStartupReady = false;

        if (!mqttStartupReady)
        {
            if (now < 10000UL)
                return;

            mqttStartupReady = true;
        }

        if (!networkManager->tryAcquire(mqttProtocolId, now))
            return;

        RuntimeClient* clients = getClients();

        for (size_t i = 0; i < clientCount; ++i)
        {
            RuntimeClient& rc = clients[i];

            if (!rc.initialized || !rc.mqtt)
                continue;

            if (!rc.mqtt->connected())
            {
                if (rc.mqtt->reconnectDue())
                {
                    if (rc.socket.acquire())
                    {
                        if (!rc.mqtt->reconnect())
                            rc.socket.release();
                    }
                }
            }

            if (rc.mqtt->connected())
                rc.mqtt->loop();
            else
                rc.socket.release();
        }

        PublishDirty();

        if (serviceLoopCallback)
            serviceLoopCallback(now);

        networkManager->release(mqttProtocolId, now);
        lastLoop = now;
    }

    static void PublishEvent(const EventManager::Event& event)
    {
        PublishEvent(event.area, event.value);
    }

    static void PublishEvent(int area, long value)
    {
        PublishMappedEventExcept(-1, area, value);
    }

    static void PublishEventFromMQTT(
        uint8_t originClientIndex,
        int area,
        long value)
    {
        PublishMappedEventExcept(
            static_cast<int>(originClientIndex),
            area,
            value
        );

        if (routedEventCallback)
        {
            routedEventCallback(
                originClientIndex,
                area,
                value
            );
        }
    }

    // ========================================================================
    // SET INBOUND MESSAGE CALLBACK
    //
    // La registrazione può avvenire anche dopo Setup().
    //
    // In tal caso la callback viene propagata a tutte le istanze MQTT già
    // costruite.
    // ========================================================================

    static void setInboundMessageReceivedCallback(
        InboundMessageReceivedCallback callback)
    {
        inboundMessageReceivedCallback =
            callback;

        RuntimeClient* clients =
            getClients();

        for (size_t i = 0;
            i < clientCount;
            ++i)
        {
            RuntimeClient& client =
                clients[i];

            if (!client.initialized ||
                !client.mqtt)
            {
                continue;
            }

            client.mqtt->setInboundMessageCallback(
                MQTTInboundCallback
            );
        }
    }

    static void setServiceLoopCallback(
        ServiceLoopCallback callback)
    {
        serviceLoopCallback = callback;
    }

    static void setRoutedEventCallback(
        RoutedEventCallback callback)
    {
        routedEventCallback = callback;
    }

    static void setRawMessageReceivedCallback(
        RawMessageReceivedCallback callback)
    {
        rawMessageReceivedCallback = callback;
    }

    static size_t getClientCount()
    {
        return clientCount;
    }

    static size_t getMappingCount()
    {
        return mappingCount;
    }

    static const RuntimeClient* getClient(size_t index)
    {
        if (index >= clientCount)
            return nullptr;

        return &getClients()[index];
    }

    static const RuntimeMapping* getMapping(size_t index)
    {
        if (index >= mappingCount)
            return nullptr;

        return &getMappings()[index];
    }

    static RuntimeClient* FindClientByBackend(
        FrontendConfig::MQTT::Client::Backend backend)
    {
        RuntimeClient* clients = getClients();

        for (size_t i = 0; i < clientCount; ++i)
        {
            RuntimeClient& client = clients[i];

            if (!client.initialized ||
                !client.cfg ||
                !client.mqtt)
            {
                continue;
            }

            if (client.cfg->backend == backend)
                return &client;
        }

        return nullptr;
    }

    static bool IsConnected(
        FrontendConfig::MQTT::Client::Backend backend)
    {
        RuntimeClient* client = FindClientByBackend(backend);

        return
            client &&
            client->initialized &&
            client->mqtt &&
            client->mqtt->connected();
    }

    static bool SubscribeRaw(
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic)
    {
        if (!topic || topic[0] == '\0')
            return false;

        RuntimeClient* client = FindClientByBackend(backend);

        if (!client ||
            !client->initialized ||
            !client->mqtt ||
            !client->mqtt->connected())
        {
            return false;
        }

        return client->mqtt->subscribeRaw(topic);
    }

    static bool PublishRaw(
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        bool retain = false)
    {
        if (!topic || topic[0] == '\0' || !payload)
            return false;

        RuntimeClient* client = FindClientByBackend(backend);

        if (!client ||
            !client->initialized ||
            !client->mqtt ||
            !client->mqtt->connected())
        {
            return false;
        }

        return client->mqtt->publishRaw(
            topic,
            payload,
            retain
        );
    }
};

// ============================================================
// STATIC MEMBERS
// ============================================================

DomoManager* MQTTEngine::manager = nullptr;

const FrontendConfig::MQTT*
    MQTTEngine::configuration = nullptr;

size_t MQTTEngine::clientCount = 0;
size_t MQTTEngine::mappingCount = 0;

unsigned long MQTTEngine::lastLoop = 0;

DMNetworkManager*
    MQTTEngine::networkManager = nullptr;

DMNetworkManager::ProtocolId
    MQTTEngine::mqttProtocolId = -1;

uint8_t MQTTEngine::mqttSource = 255;

uint16_t MQTTEngine::dirtyMappings[
    MQTTEngine::MAX_MAPPINGS
];

uint16_t MQTTEngine::dirtyCount = 0;

// ============================================================
// WEB API ENGINE
//
// Gestisce:
//
//   CLIENT:
//     DomoManager -> dispositivi HTTP remoti
//
//   SERVER:
//     dispositivi HTTP remoti -> DomoManager
//
//   ACCESS:
//     AccessControlService
//     FanvilWebAPI
//     AxessMCcWebAPI
//
// Gli endpoint HTTP vengono creati internamente a runtime.
// ============================================================

class WebAPIEngine
{
public:

    using WebRequestCallback =
        std::function<void(
            const char* endpointName,
            const HttpServerRequest& request,
            const HttpServerResponse& response
        )>;

    using AccessEventCallback =
        AccessControlService::EventCallback;

    using AccessAuthorizedCallback =
        AccessControlService::AuthorizedCallback;

    using DeviceResponseCallback =
    std::function<void(
        const WebApiDevice& device,
        const DeviceMessageProfile::OutMessage& message,
        const String& rawResponse,
        const ParsedResponse& parsed
    )>;
private:
    // ========================================================================
    // DOMOMANAGER ONLINE CLIENT
    // ========================================================================

    static inline DomoManagerOnlineClient
        onlineClient;

    // ========================================================
    // CONFIGURATION
    // ========================================================

    static inline const FrontendConfig::
        webApiDeviceMessaging*
        configuration =
            nullptr;


    // ========================================================
    // CLIENT / POLLING
    // ========================================================

    static inline HttpClientTransport*
        clientTransport =
            nullptr;

    static inline DeviceMessageEngine*
        clientEngine =
            nullptr;

    static inline bool
        clientRunning =
            false;


    // ========================================================
    // SERVER / WEBHOOK
    // ========================================================

    static inline HttpServerTransport*
        serverTransport =
            nullptr;

    static inline WebApiServerEngine*
        serverEngine =
            nullptr;

    static inline bool
        serverRunning =
            false;


    // ========================================================
    // ACCESS CONTROL
    // ========================================================

    static inline AccessControlService*
        accessControl =
            nullptr;

    static inline std::vector<IAccessWebAPI*> accessProtocols;

    static inline bool
        accessRunning =
            false;


    // ========================================================
    // RUNTIME SERVER ENDPOINTS
    //
    // La quantità di endpoint dipende dai protocolli
    // registrati a runtime.
    //
    // Lo storage rimane valido per tutta la vita del server.
    // SetupAccessControl() completa la registrazione prima che
    // SetupServer() passi data() al WebApiServerEngine.
    // ========================================================

    static inline std::vector<WebApiServerEndpoint>
        serverEndpoints;


    // ========================================================
    // CLIENT CLEANUP
    // ========================================================

    static void CleanupClient()
    {
        // ====================================================
        // DOMOMANAGER ONLINE
        //
        // Il client online mantiene riferimenti a:
        //
        //   - DeviceMessageEngine;
        //   - WebApiDevice interno al motore;
        //
        // Deve quindi essere resettato prima della distruzione
        // di clientEngine.
        // ====================================================

        onlineClient.Reset();

        // ====================================================
        // STOP RUNTIME
        // ====================================================

        clientRunning =
            false;

        // ====================================================
        // CANCEL TRANSPORT
        //
        // La cancellazione viene eseguita prima di distruggere
        // il DeviceMessageEngine, così eventuali callback HTTP
        // pendenti non possono utilizzare il motore eliminato.
        // ====================================================

        if (clientTransport)
        {
            clientTransport->cancel();
        }

        // ====================================================
        // DEVICE MESSAGE ENGINE
        // ====================================================

        if (clientEngine)
        {
            delete clientEngine;

            clientEngine =
                nullptr;
        }

        // ====================================================
        // HTTP CLIENT TRANSPORT
        // ====================================================

        if (clientTransport)
        {
            delete clientTransport;

            clientTransport =
                nullptr;
        }

        LOG_DF(
            "WEBAPI-CLIENT",
            "HTTP client resources released"
        );
    }


    // ========================================================
    // SERVER CLEANUP
    //
    // WebApiServerEngine deve essere eliminato prima del
    // transport perché contiene gli endpoint adapter.
    // ========================================================

    static void CleanupServer()
    {
        if (serverEngine)
        {
            delete serverEngine;

            serverEngine =
                nullptr;
        }

        if (serverTransport)
        {
            delete serverTransport;

            serverTransport =
                nullptr;
        }

        serverRunning =
            false;
    }


    // ========================================================
    // ACCESS CONTROL CLEANUP
    //
    // Deve essere chiamata dopo CleanupServer().
    // ========================================================

    static void CleanupAccessControl()
    {
        /*
        * Gli endpoint contengono std::function collegate
        * alle istanze dei protocolli.
        *
        * Gli handler devono essere distrutti prima delle
        * istanze FanvilWebAPI e AxessTMCWebAPI.
        */

        serverEndpoints.clear();

        /*
        * Opzionale:
        *
        * Non usiamo shrink_to_fit() perché questa funzione
        * può essere chiamata anche durante una reinizializzazione.
        * Mantenere la capacità evita riallocazioni successive.
        */

        for (
            IAccessWebAPI* protocol :
            accessProtocols
        )
        {
            delete protocol;
        }

        accessProtocols.clear();

        if (accessControl)
        {
            delete accessControl;

            accessControl =
                nullptr;
        }

        accessRunning =
            false;
    }


    // ========================================================
    // COMPLETE CLEANUP
    // ========================================================

    static void Cleanup()
    {
        /*
         * Ordine obbligatorio:
         *
         *   1. client
         *   2. server e adapter
         *   3. endpoint e protocolli
         */

        CleanupClient();
        CleanupServer();
        CleanupAccessControl();

        configuration =
            nullptr;
    }


    // ========================================================
    // REGISTER ACCESS PROTOCOL ENDPOINTS
    //
    // Registra tutti gli endpoint esposti dal protocollo.
    // ========================================================

    static bool RegisterAccessEndpoints(
        IAccessWebAPI& protocol)
    {
        const size_t protocolEndpointCount =
            protocol.endpointCount();

        if (protocolEndpointCount == 0)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Protocol '%s' exposes no endpoints",
                protocol.protocolId()
                    ? protocol.protocolId()
                    : ""
            );

            return false;
        }

        /*
        * Salviamo la dimensione iniziale per poter effettuare
        * il rollback se una definizione del protocollo non è
        * valida o genera un duplicato.
        */
        const size_t initialEndpointCount =
            serverEndpoints.size();

        /*
        * Una sola eventuale riallocazione per tutti gli endpoint
        * esposti dal protocollo corrente.
        *
        * In questa fase il WebApiServerEngine non è ancora stato
        * inizializzato, quindi data() non è ancora utilizzato da
        * alcun componente.
        */
        serverEndpoints.reserve(
            initialEndpointCount +
            protocolEndpointCount
        );

        for (
            size_t protocolEndpointIndex = 0;
            protocolEndpointIndex <
                protocolEndpointCount;
            ++protocolEndpointIndex
        )
        {
            const AccessWebAPIEndpointDef* definition =
                protocol.endpointDefinition(
                    protocolEndpointIndex
                );

            // ====================================================
            // DEFINITION VALIDATION
            // ====================================================

            if (
                !definition ||
                !definition->name ||
                definition->name[0] == '\0' ||
                !definition->path ||
                definition->path[0] == '\0'
            )
            {
                LOG_EF(
                    "WEBAPI-SERVER",
                    "Invalid endpoint definition: "
                    "protocol=%s index=%u",
                    protocol.protocolId()
                        ? protocol.protocolId()
                        : "",
                    static_cast<unsigned>(
                        protocolEndpointIndex
                    )
                );

                serverEndpoints.resize(
                    initialEndpointCount
                );

                return false;
            }

            // ====================================================
            // DUPLICATE VALIDATION
            //
            // Impedisce la registrazione dello stesso metodo
            // sullo stesso path.
            // ====================================================

            for (
                const WebApiServerEndpoint& registered :
                serverEndpoints
            )
            {
                const bool samePath =
                    registered.path &&
                    strcmp(
                        registered.path,
                        definition->path
                    ) == 0;

                const bool sameMethod =
                    (
                        !registered.method &&
                        !definition->method
                    ) ||
                    (
                        registered.method &&
                        definition->method &&
                        strcmp(
                            registered.method,
                            definition->method
                        ) == 0
                    );

                if (
                    samePath &&
                    sameMethod
                )
                {
                    LOG_EF(
                        "WEBAPI-SERVER",
                        "Duplicate endpoint: "
                        "method=%s path=%s "
                        "protocol=%s",
                        definition->method
                            ? definition->method
                            : "*",
                        definition->path,
                        protocol.protocolId()
                            ? protocol.protocolId()
                            : ""
                    );

                    serverEndpoints.resize(
                        initialEndpointCount
                    );

                    return false;
                }
            }

            // ====================================================
            // ENDPOINT CONSTRUCTION
            // ====================================================

            WebApiServerEndpoint endpoint{};

            endpoint.name =
                definition->name;

            endpoint.path =
                definition->path;

            endpoint.method =
                definition->method;

            endpoint.enabled =
                true;

            IAccessWebAPI* protocolPtr =
                &protocol;

            const size_t capturedEndpointIndex =
                protocolEndpointIndex;

            const char* capturedEndpointName =
                definition->name;

            endpoint.handler =
                [
                    protocolPtr,
                    capturedEndpointIndex,
                    capturedEndpointName
                ](
                    const HttpServerRequest& request,
                    HttpServerResponse& response)
                {
                    if (!protocolPtr)
                    {
                        response.statusCode =
                            503;

                        response.contentType =
                            "text/plain";

                        response.body =
                            "SERVICE UNAVAILABLE";

                        response.closeConnection =
                            true;

                        if (webRequestCallback)
                        {
                            webRequestCallback(
                                capturedEndpointName,
                                request,
                                response
                            );
                        }

                        return;
                    }

                    protocolPtr->handleRequest(
                        capturedEndpointIndex,
                        request,
                        response
                    );

                    if (webRequestCallback)
                    {
                        webRequestCallback(
                            capturedEndpointName,
                            request,
                            response
                        );
                    }
                };

            // ====================================================
            // REGISTRATION
            // ====================================================

            serverEndpoints.push_back(
                std::move(
                    endpoint
                )
            );

            LOG_IF(
                "WEBAPI-SERVER",
                "Endpoint registered: "
                "protocol=%s index=%u "
                "name=%s method=%s path=%s "
                "total=%u",
                protocol.protocolId()
                    ? protocol.protocolId()
                    : "",
                static_cast<unsigned>(
                    protocolEndpointIndex
                ),
                definition->name,
                definition->method
                    ? definition->method
                    : "*",
                definition->path,
                static_cast<unsigned>(
                    serverEndpoints.size()
                )
            );
        }

        return true;
    }

     static bool AddAccessProtocol(
        IAccessWebAPI* protocol)
    {
        if (!protocol)
            return false;

        if (
            !RegisterAccessEndpoints(
                *protocol
            )
        )
        {
            delete protocol;

            return false;
        }

        accessProtocols.push_back(
            protocol
        );

        return true;
    }

    static bool BuildAccessProtocols()
    {
        if (!accessControl)
            return false;

        if (
            !AddAccessProtocol(
                new FanvilWebAPI(
                    *accessControl
                )
            )
        )
        {
            LOG_EF(
                "ACCESS",
                "Unable to register Fanvil protocol"
            );

            return false;
        }

        if (
            !AddAccessProtocol(
                new AxessTMCWebAPI(
                    *accessControl
                )
            )
        )
        {
            LOG_EF(
                "ACCESS",
                "Unable to register AxessTMC protocol"
            );

            return false;
        }

        return true;
    }

    // ========================================================
    // ACCESS CONTROL SETUP
    // ========================================================

    static bool SetupAccessControl(
        const FrontendConfig::Access& cfg)
    {
        CleanupAccessControl();

        // ====================================================
        // DISABLED
        // ====================================================

        if (!cfg.enabled)
        {
            LOG_IF(
                "ACCESS",
                "Access control disabled"
            );

            return true;
        }

        // ====================================================
        // CONFIGURATION VALIDATION
        // ====================================================

        if (
            !cfg.readers ||
            cfg.readerCount == 0
        )
        {
            LOG_EF(
                "ACCESS",
                "Access control enabled without readers"
            );

            return false;
        }

        if (cfg.defaultRelayIndex < 0)
        {
            LOG_EF(
                "ACCESS",
                "Invalid default relay index: %d",
                cfg.defaultRelayIndex
            );

            return false;
        }

        if (cfg.defaultRelayTime < 0)
        {
            LOG_EF(
                "ACCESS",
                "Invalid default relay time: %d",
                cfg.defaultRelayTime
            );

            return false;
        }

        // ====================================================
        // ACCESS SERVICE
        // ====================================================

        accessControl =
            new AccessControlService();

        if (!accessControl)
        {
            LOG_EF(
                "ACCESS",
                "Unable to allocate AccessControlService"
            );

            return false;
        }

        // ====================================================
        // ACCESS SERVICE CONFIGURATION
        // ====================================================

        AccessControlService::Config
            accessConfig;

        accessConfig.readers =
            cfg.readers;

        accessConfig.readerCount =
            cfg.readerCount;

        accessConfig.commonCardCode =
            cfg.commonCardCode;

        accessConfig.defaultRelayIndex =
            cfg.defaultRelayIndex;

        accessConfig.defaultRelayTime =
            cfg.defaultRelayTime;

        // ====================================================
        // ALL ACCESS EVENTS
        // ====================================================

        accessConfig.onEvent =
            [](
                const AccessControlService::Request& request,
                const AccessControlService::Decision& decision)
            {
                const char* readerName =
                    (
                        decision.reader &&
                        decision.reader->name
                    )
                        ? decision.reader->name
                        : request.readerName.c_str();

                const char* definitionId =
                    (
                        decision.readerDef &&
                        decision.readerDef->id
                    )
                        ? decision.readerDef->id
                        : "";

                const unsigned authorized =
                    decision.authorized
                        ? 1U
                        : 0U;

                const unsigned commonCode =
                    decision.commonCode
                        ? 1U
                        : 0U;

                const char* result =
                    AccessControlService::ResultToString(
                        decision.result
                    );

                LOG_IF(
                    "ACCESS",
                    "reader=%s def=%s card=%s "
                    "authorized=%u common=%u result=%s",
                    readerName,
                    definitionId,
                    decision.cardCode.c_str(),
                    authorized,
                    commonCode,
                    result
                );

                if (accessEventCallback)
                {
                    accessEventCallback(
                        request,
                        decision
                    );
                }
            };

        // ====================================================
        // AUTHORIZED ACCESS EVENTS
        // ====================================================

        accessConfig.onAuthorized =
        [](
            const AccessControlService::Request& request,
            const AccessControlService::Decision& decision)
        {
            const char* readerName =
                (
                    decision.reader &&
                    decision.reader->name
                )
                    ? decision.reader->name
                    : request.readerName.c_str();

            const int area =
                decision.reader
                    ? decision.reader->area
                    : -1;

            LOG_IF(
                "ACCESS",
                "Access granted: "
                "reader=%s card=%s relay=%d "
                "time=%d area=%d",
                readerName,
                decision.cardCode.c_str(),
                decision.relayIndex,
                decision.relayTime,
                area
            );

            if (accessAuthorizedCallback)
            {
                accessAuthorizedCallback(
                    request,
                    decision
                );
            }
        };

        // ====================================================
        // SERVICE INITIALIZATION
        // ====================================================

        if (
            !accessControl->setup(
                accessConfig
            )
        )
        {
            LOG_EF(
                "ACCESS",
                "AccessControlService setup failed"
            );

            CleanupAccessControl();

            return false;
        }

        // ====================================================
        // ACCESS PROTOCOLS AND ENDPOINTS
        //
        // BuildAccessProtocols() crea e registra i protocolli
        // concreti usando la collezione accessProtocols.
        // ====================================================

        if (!BuildAccessProtocols())
        {
            LOG_EF(
                "ACCESS",
                "Unable to build access protocols"
            );

            CleanupAccessControl();

            return false;
        }

        // ====================================================
        // FINAL VALIDATION
        // ====================================================

        if (accessProtocols.empty())
        {
            LOG_EF(
                "ACCESS",
                "No access protocols registered"
            );

            CleanupAccessControl();

            return false;
        }

        if (serverEndpoints.empty())
        {
            LOG_EF(
                "ACCESS",
                "No access endpoints registered"
            );

            CleanupAccessControl();

            return false;
        }

        // ====================================================
        // SUCCESS
        // ====================================================

        accessRunning =
            true;

        const unsigned configuredReaders =
            static_cast<unsigned>(
                accessControl->readerCount()
            );

        const unsigned configuredProtocols =
            static_cast<unsigned>(
                accessProtocols.size()
            );

        const unsigned configuredEndpoints =
            static_cast<unsigned>(
                serverEndpoints.size()
            );

        const bool commonCardConfigured =
            cfg.commonCardCode &&
            cfg.commonCardCode[0] != '\0';

        const char* commonCardStatus =
            commonCardConfigured
                ? "CONFIGURED"
                : "NOT_CONFIGURED";

        LOG_IF(
            "ACCESS",
            "Access control initialized: "
            "readers=%u protocols=%u "
            "endpoints=%u commonCard=%s",
            configuredReaders,
            configuredProtocols,
            configuredEndpoints,
            commonCardStatus
        );

        return true;
    }


    // ========================================================
    // CLIENT SETUP
    // ========================================================

    static bool SetupClient(
        const FrontendConfig::
            webApiDeviceMessaging::Client& cfg)
    {
        CleanupClient();

        // ====================================================
        // DISABLED
        // ====================================================

        if (!cfg.enabled)
        {
            LOG_IF(
                "WEBAPI-CLIENT",
                "HTTP client disabled"
            );

            return false;
        }

        // ====================================================
        // VALIDATION
        // ====================================================

        if (
            !cfg.devices ||
            cfg.deviceCount == 0
        )
        {
            LOG_WF(
                "WEBAPI-CLIENT",
                "HTTP client enabled without devices"
            );

            return false;
        }

        if (cfg.timeoutMs == 0)
        {
            LOG_EF(
                "WEBAPI-CLIENT",
                "Invalid client timeout"
            );

            return false;
        }

        // ====================================================
        // TRANSPORT
        // ====================================================

        clientTransport =
            new HttpClientTransport(
                cfg.timeoutMs
            );

        if (!clientTransport)
        {
            LOG_EF(
                "WEBAPI-CLIENT",
                "Unable to allocate HTTP client transport"
            );

            return false;
        }

        // ====================================================
        // ENGINE
        // ====================================================

        clientEngine =
            new DeviceMessageEngine();

        if (!clientEngine)
        {
            LOG_EF(
                "WEBAPI-CLIENT",
                "Unable to allocate client engine"
            );

            CleanupClient();

            return false;
        }

        // ====================================================
        // CONTEXT
        // ====================================================

        DeviceMessageEngine::Context context;

        context.devices =
            cfg.devices;

        context.deviceCount =
            cfg.deviceCount;

        context.transport =
            clientTransport;

        context.responseTimeoutMs =
            cfg.timeoutMs;

        // ====================================================
        // PLACEHOLDER RESOLVER
        //
        // DomoManagerOnlineClient gestisce:
        //
        //   ${controllerId}
        //   ${section}
        //
        // Gli altri profili attualmente non utilizzano
        // placeholder applicativi gestiti dal frontend.
        // ====================================================

        context.resolver =
        [](
            const WebApiDevice& device,
            const char* variable) -> String
        {
            if (
                !variable ||
                variable[0] == '\0'
            )
            {
                return "";
            }

            // ====================================================
            // DEVICE USERNAME
            // ====================================================

            if (
                strcmp(
                    variable,
                    "username"
                ) == 0
            )
            {
                return
                    device.credentials.username
                        ? String(
                            device.credentials.username
                        )
                        : String("");
            }

            // ====================================================
            // DEVICE PASSWORD
            // ====================================================

            if (
                strcmp(
                    variable,
                    "password"
                ) == 0
            )
            {
                return
                    device.credentials.password
                        ? String(
                            device.credentials.password
                        )
                        : String("");
            }

            // ====================================================
            // DOMOMANAGER ONLINE
            // ====================================================

            String resolvedValue;

            if (
                onlineClient.ResolvePlaceholder(
                    device,
                    variable,
                    resolvedValue
                )
            )
            {
                return resolvedValue;
            }

            // ====================================================
            // UNRESOLVED
            // ====================================================

            LOG_WF(
                "WEBAPI-CLIENT",
                "[%s] Unresolved placeholder: %s",
                device.name
                    ? device.name
                    : "",
                variable
            );

            return "";
        };

        // ====================================================
        // RESPONSE CALLBACK
        //
        // DomoManagerOnlineClient riceve per primo la risposta.
        //
        // Se HandleResponse() restituisce true, la risposta
        // appartiene al profilo DomoManager Online ed è già
        // stata elaborata.
        //
        // Negli altri casi viene eseguito il log generico per:
        //
        //   - Fanvil;
        //   - AxessTMC;
        //   - futuri profili WebAPI.
        // ====================================================

        context.onResponse =
        [](
            const WebApiDevice& device,
            const DeviceMessageProfile::OutMessage& message,
            const String& rawResponse,
            const ParsedResponse& parsed)
        {
            // ====================================================
            // DOMOMANAGER ONLINE
            // ====================================================

            if (
                onlineClient.HandleResponse(
                    device,
                    message,
                    rawResponse,
                    parsed
                )
            )
            {
                return;
            }

            // ====================================================
            // FRONTEND DEVICE RESPONSE
            //
            // Reolink e futuri device client vengono elaborati
            // dal frontend senza aggiungere dipendenze specifiche
            // dentro WebAPIEngine.
            // ====================================================

            if (deviceResponseCallback)
            {
                deviceResponseCallback(
                    device,
                    message,
                    rawResponse,
                    parsed
                );
            }

            // ==========================================
            // GENERIC RESPONSE LOG
            // ====================================================

            const char* deviceName =
                device.name
                    ? device.name
                    : "";

            const char* messageKey =
                message.key
                    ? message.key
                    : "";

            const unsigned responseLength =
                static_cast<unsigned>(
                    rawResponse.length()
                );

            switch (message.responseFormat)
            {
                // ====================================================
                // KEY VALUE
                // ====================================================

                case DeviceMessageProfile::
                    ResponseFormat::KEY_VALUE:
                {
                    LOG_DF(
                        "WEBAPI-CLIENT",
                        "[%s] message=%s "
                        "format=KEY_VALUE fields=%u bytes=%u",
                        deviceName,
                        messageKey,
                        static_cast<unsigned>(
                            parsed.fields.size()
                        ),
                        responseLength
                    );

                    break;
                }

                // ====================================================
                // JSON
                // ====================================================

                case DeviceMessageProfile::
                    ResponseFormat::JSON:
                {
                    LOG_DF(
                        "WEBAPI-CLIENT",
                        "[%s] message=%s "
                        "format=JSON valid=%u bytes=%u",
                        deviceName,
                        messageKey,
                        parsed.jsonValid
                            ? 1U
                            : 0U,
                        responseLength
                    );

                    break;
                }

                // ====================================================
                // RAW
                // ====================================================

                case DeviceMessageProfile::
                    ResponseFormat::RAW:
                {
                    LOG_DF(
                        "WEBAPI-CLIENT",
                        "[%s] message=%s "
                        "format=RAW bytes=%u",
                        deviceName,
                        messageKey,
                        responseLength
                    );

                    break;
                }

                // ====================================================
                // UNKNOWN FORMAT
                // ====================================================

                default:
                {
                    LOG_WF(
                        "WEBAPI-CLIENT",
                        "[%s] message=%s "
                        "unknown response format bytes=%u",
                        deviceName,
                        messageKey,
                        responseLength
                    );

                    break;
                }
            }
        };

        // ====================================================
        // DEVICE MESSAGE ENGINE INITIALIZATION
        // ====================================================

        if (
            !clientEngine->Init(
                context
            )
        )
        {
            LOG_EF(
                "WEBAPI-CLIENT",
                "Client engine initialization failed"
            );

            CleanupClient();

            return false;
        }

        // ====================================================
        // DOMOMANAGER ONLINE INITIALIZATION
        //
        // Per ora i parametri sono cablati qui.
        //
        // Nel passaggio successivo potranno essere spostati in:
        //
        //   FrontendConfig::webApiDeviceMessaging::Online
        //
        // ====================================================

        DomoManagerOnlineClient::Config onlineConfig;

        onlineConfig.enabled =
            true;

        onlineConfig.deviceName =
            "DomoManagerOnline";

        onlineConfig.controllerId =
            "opta-domotica";

        onlineConfig.allowEmptyWriteResponse =
            false;

        const bool onlineInitialized =
            onlineClient.Begin(
                *clientEngine,
                onlineConfig
            );

        if (!onlineInitialized)
        {
            /*
            * Il client WebAPI generico resta operativo anche
            * quando DomoManager Online non è configurato.
            */
            LOG_WF(
                "WEBAPI-CLIENT",
                "DomoManager Online not initialized"
            );
        }
        else
        {
            // ====================================================
            // VERSION CALLBACK
            // ====================================================

            onlineClient.SetVersionCallback(
                [](
                    const DomoManagerOnlineClient::
                        VersionInfo& version)
                {
                    LOG_IF(
                        "DOMO-ONLINE",
                        "Remote version: "
                        "schema=%lu revision=%lu checksum=%s",
                        static_cast<unsigned long>(
                            version.schemaVersion
                        ),
                        static_cast<unsigned long>(
                            version.revision
                        ),
                        version.checksum.c_str()
                    );
                }
            );

            // ====================================================
            // MANIFEST CALLBACK
            // ====================================================

            onlineClient.SetManifestCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Manifest received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );
                }
            );

            // ====================================================
            // FULL CONFIG CALLBACK
            // ====================================================

            onlineClient.SetFullConfigCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Full configuration received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );

                    /*
                    * Futura integrazione:
                    *
                    * DMConfigInterpreter::Load(
                    *     rawResponse
                    * );
                    */
                }
            );

            // ====================================================
            // SECTION CALLBACK
            // ====================================================

            onlineClient.SetSectionCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Configuration section received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );
                }
            );

            // ====================================================
            // JOBS CALLBACK
            // ====================================================

            onlineClient.SetJobsCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Jobs received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );

                    /*
                    * Futura integrazione:
                    *
                    * DmInterpreter::loadFromString(
                    *     rawResponse.c_str(),
                    *     scheduler,
                    *     manager,
                    *     registry
                    * );
                    */
                }
            );

            
            // ====================================================
            // ACK CALLBACK
            // ====================================================

            onlineClient.SetAckCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Configuration ACK received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );
                }
            );

            // ====================================================
            // UPLOAD CALLBACK
            // ====================================================

            onlineClient.SetUploadCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Configuration upload completed: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );
                }
            );

            // ====================================================
            // ERROR CALLBACK
            // ====================================================

            onlineClient.SetErrorCallback(
                [](
                    const char* messageKey,
                    const String& error)
                {
                    LOG_EF(
                        "DOMO-ONLINE",
                        "Request failed: "
                        "message=%s error=%s",
                        messageKey
                            ? messageKey
                            : "",
                        error.c_str()
                    );
                }
            );

            // ====================================================
            // TEMPORARY CONFIG_VERSION TEST
            // ====================================================

            if (!onlineClient.RequestVersion())
            {
                LOG_EF(
                    "DOMO-ONLINE",
                    "Unable to queue CONFIG_VERSION test"
                );
            }
            else
            {
                LOG_IF(
                    "DOMO-ONLINE",
                    "CONFIG_VERSION test queued"
                );
            }
            
        }

        // ====================================================
        // COMPLETE
        // ====================================================

        clientRunning =
            true;

        LOG_IF(
            "WEBAPI-CLIENT",
            "HTTP client initialized: "
            "devices=%u timeout=%lu online=%u",
            static_cast<unsigned>(
                cfg.deviceCount
            ),
            static_cast<unsigned long>(
                cfg.timeoutMs
            ),
            onlineClient.isInitialized()
                ? 1U
                : 0U
        );

        return true;
    }


    // ========================================================
    // SERVER SETUP
    //
    // Gli endpoint sono creati da SetupAccessControl().
    // Non vengono letti da FrontendConfig.
    // ========================================================

    static bool SetupServer(
        const FrontendConfig::
            webApiDeviceMessaging::Server& cfg)
    {
        CleanupServer();

        // ====================================================
        // DISABLED
        // ====================================================

        if (!cfg.enabled)
        {
            LOG_IF(
                "WEBAPI-SERVER",
                "HTTP server disabled"
            );

            return false;
        }

        // ====================================================
        // VALIDATION
        // ====================================================

        if (cfg.port == 0)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Invalid HTTP server port"
            );

            return false;
        }

        if (cfg.timeoutMs == 0)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Invalid HTTP server timeout"
            );

            return false;
        }

        if (serverEndpoints.empty())
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "No runtime HTTP endpoints registered"
            );

            return false;
        }

        // ====================================================
        // TRANSPORT
        // ====================================================

        serverTransport =
            new HttpServerTransport(
                cfg.port,
                cfg.timeoutMs
            );

        if (!serverTransport)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Unable to allocate HTTP server transport"
            );

            return false;
        }

        // ====================================================
        // SERVER ENGINE
        // ====================================================

        serverEngine =
            new WebApiServerEngine();

        if (!serverEngine)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Unable to allocate server engine"
            );

            CleanupServer();

            return false;
        }

        // ====================================================
        // SERVER CONTEXT
        // ====================================================

        WebApiServerEngine::Context
            context;

        context.transport =
            serverTransport;

        /*
        * La registrazione degli endpoint deve essere conclusa
        * prima di questa assegnazione.
        *
        * Dopo Init() non si devono aggiungere o rimuovere endpoint
        * finché il server è attivo, perché una riallocazione del
        * vector invaliderebbe il puntatore restituito da data().
        */
        context.endpoints =
            serverEndpoints.data();

        context.endpointCount =
            serverEndpoints.size();

        // ====================================================
        // INITIALIZATION
        // ====================================================

        if (
            !serverEngine->Init(
                context
            )
        )
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Server engine initialization failed"
            );

            CleanupServer();

            return false;
        }

        serverRunning =
            true;

        const unsigned configuredPort =
            static_cast<unsigned>(
                cfg.port
            );

        const unsigned configuredEndpoints =
            static_cast<unsigned>(
                serverEndpoints.size()
            );

        const unsigned long configuredTimeout =
            static_cast<unsigned long>(
                cfg.timeoutMs
            );

        LOG_IF(
            "WEBAPI-SERVER",
            "HTTP server initialized: "
            "port=%u endpoints=%u timeout=%lu",
            configuredPort,
            configuredEndpoints,
            configuredTimeout
        );

        return true;
    }


    // ========================================================
    // FRONTEND CALLBACKS
    // ========================================================

    static inline WebRequestCallback
        webRequestCallback =
            nullptr;

    static inline AccessEventCallback
        accessEventCallback =
            nullptr;

    static inline AccessAuthorizedCallback
        accessAuthorizedCallback =
            nullptr;

    static inline DeviceResponseCallback
        deviceResponseCallback =
            nullptr;
public:
   
    // ========================================================================
    // DOMOMANAGER ONLINE STATUS
    // ========================================================================

    static bool IsOnlineConfigReady()
    {
        return
            onlineClient.isInitialized();
    }


    // ========================================================================
    // REQUEST ONLINE CONFIG VERSION
    // ========================================================================

    static bool RequestOnlineConfigVersion()
    {
        return
            onlineClient.RequestVersion();
    }


    // ========================================================================
    // REQUEST ONLINE CONFIG MANIFEST
    // ========================================================================

    static bool RequestOnlineConfigManifest()
    {
        return
            onlineClient.RequestManifest();
    }


    // ========================================================================
    // REQUEST FULL ONLINE CONFIG
    // ========================================================================

    static bool RequestFullOnlineConfig()
    {
        return
            onlineClient.RequestFullConfig();
    }


    // ========================================================================
    // REQUEST ONLINE CONFIG SECTION
    // ========================================================================

    static bool RequestOnlineConfigSection(
        const char* section)
    {
        return
            onlineClient.RequestSection(
                section
            );
    }


    // ========================================================================
    // REQUEST ONLINE JOBS
    // ========================================================================

    static bool RequestOnlineJobs()
    {
        return
            onlineClient.RequestJobs();
    }


    // ========================================================================
    // SEND ONLINE CONFIG ACK
    // ========================================================================

    static bool SendOnlineConfigAck(
        uint32_t revision,
        const char* status,
        bool rebootRequired,
        const char* detail = nullptr)
    {
        return
            onlineClient.SendAck(
                revision,
                status,
                rebootRequired,
                detail
            );
    }


    // ========================================================================
    // UPLOAD ONLINE CONFIG
    // ========================================================================

    static bool UploadOnlineConfig(
        const JsonDocument& document)
    {
        return
            onlineClient.UploadConfig(
                document
            );
    }

    // ========================================================
    // FRONTEND CALLBACK SETUP
    // ========================================================

    static void SetWebRequestCallback(
        const WebRequestCallback& callback)
    {
        webRequestCallback =
            callback;
    }


    static void SetAccessEventCallback(
        const AccessEventCallback& callback)
    {
        accessEventCallback =
            callback;
    }


    static void SetAccessAuthorizedCallback(
        const AccessAuthorizedCallback& callback)
    {
        accessAuthorizedCallback =
            callback;
    }

    static void SetDeviceResponseCallback(
        const DeviceResponseCallback& callback)
    {
        deviceResponseCallback =
            callback;
    }

    // ========================================================
    // SETUP
    //
    // WebAPI e Access sono configurazioni separate.
    // ========================================================

    static void Setup(
        const FrontendConfig::
            webApiDeviceMessaging& webApiCfg,
        const FrontendConfig::Access& accessCfg)
    {
        // ====================================================
        // CLEANUP PREVIOUS RUNTIME
        // ====================================================

        CleanupClient();
        CleanupServer();
        CleanupAccessControl();

        configuration =
            &webApiCfg;

        // ====================================================
        // COMPLETELY DISABLED
        // ====================================================

        if (
            !webApiCfg.client.enabled &&
            !webApiCfg.server.enabled &&
            !accessCfg.enabled
        )
        {
            LOG_IF(
                "WEBAPI",
                "WebAPI and access control disabled"
            );

            return;
        }

        // ====================================================
        // CONFIGURATION CONSISTENCY
        // ====================================================

        if (
            accessCfg.enabled &&
            !webApiCfg.server.enabled
        )
        {
            LOG_EF(
                "ACCESS",
                "Access control requires WebAPI server"
            );

            /*
             * Il client HTTP può continuare a funzionare.
             */
            if (webApiCfg.client.enabled)
            {
                SetupClient(
                    webApiCfg.client
                );
            }

            LOG_IF(
                "WEBAPI",
                "Setup completed: "
                "client=%s server=DISABLED "
                "access=DISABLED endpoints=0",
                clientRunning
                    ? "RUNNING"
                    : "DISABLED"
            );

            return;
        }

        // ====================================================
        // ACCESS CONTROL
        //
        // Deve precedere SetupServer perché registra
        // gli endpoint runtime.
        // ====================================================

        bool accessSetupResult =
            true;

        if (accessCfg.enabled)
        {
            accessSetupResult =
                SetupAccessControl(
                    accessCfg
                );

            if (!accessSetupResult)
            {
                LOG_EF(
                    "ACCESS",
                    "Access control initialization failed"
                );
            }
        }
        else
        {
            LOG_IF(
                "ACCESS",
                "Access control disabled"
            );
        }

        // ====================================================
        // HTTP CLIENT
        // ====================================================

        if (webApiCfg.client.enabled)
        {
            SetupClient(
                webApiCfg.client
            );
        }
        else
        {
            LOG_IF(
                "WEBAPI-CLIENT",
                "HTTP client disabled"
            );
        }

        // ====================================================
        // HTTP SERVER
        // ====================================================

        if (webApiCfg.server.enabled)
        {
            if (
                accessCfg.enabled &&
                !accessSetupResult
            )
            {
                LOG_EF(
                    "WEBAPI-SERVER",
                    "HTTP server not started because "
                    "access setup failed"
                );
            }
            else if (
                !SetupServer(
                    webApiCfg.server
                )
            )
            {
                CleanupAccessControl();
            }
        }
        else
        {
            LOG_IF(
                "WEBAPI-SERVER",
                "HTTP server disabled"
            );
        }

        // ====================================================
        // RESULT
        // ====================================================

        const char* clientStatus =
            clientRunning
                ? "RUNNING"
                : "DISABLED";

        const char* serverStatus =
            serverRunning
                ? "RUNNING"
                : "DISABLED";

        const char* accessStatus =
            accessRunning
                ? "RUNNING"
                : "DISABLED";

        const unsigned endpointCount =
            static_cast<unsigned>(
                serverEndpoints.size()
            );

        LOG_IF(
            "WEBAPI",
            "Setup completed: "
            "client=%s server=%s access=%s endpoints=%u",
            clientStatus,
            serverStatus,
            accessStatus,
            endpointCount
        );
    }


    // ========================================================
    // LOOP
    // ========================================================

    static void Loop(
        unsigned long now)
    {
        if (!configuration)
            return;

        if (
            clientRunning &&
            clientEngine
        )
        {
            clientEngine->Loop(
                now
            );
        }

        if (
            serverRunning &&
            serverEngine
        )
        {
            serverEngine->Loop(
                now
            );
        }
    }


    // ========================================================
    // CLIENT COMMAND
    // ========================================================

    static bool Send(
        const char* deviceName,
        const char* messageKey,
        const String& body = "")
    {
        if (
            !clientRunning ||
            !clientEngine
        )
        {
            return false;
        }

        return clientEngine->Send(
            deviceName,
            messageKey,
            body
        );
    }


    // ========================================================
    // CLIENT ACCESSORS
    // ========================================================

    static DeviceMessageEngine*
    GetClientEngine()
    {
        return clientEngine;
    }


    static HttpClientTransport*
    GetClientTransport()
    {
        return clientTransport;
    }


    // ========================================================
    // SERVER ACCESSORS
    // ========================================================

    static WebApiServerEngine*
    GetServerEngine()
    {
        return serverEngine;
    }


    static HttpServerTransport*
    GetServerTransport()
    {
        return serverTransport;
    }


    static size_t GetServerEndpointCount()
    {
        return
            serverEndpoints.size();
    }


    // ========================================================
    // ACCESS CONTROL ACCESSORS
    // ========================================================

    static AccessControlService*
    GetAccessControl()
    {
        return accessControl;
    }


    static size_t GetAccessProtocolCount()
    {
        return accessProtocols.size();
    }


    static IAccessWebAPI*
        GetAccessProtocol(
            size_t index)
        {
            if (
                index >=
                accessProtocols.size()
            )
            {
                return nullptr;
            }

            return accessProtocols[index];
        }


    static IAccessWebAPI*
        FindAccessProtocol(
            const char* protocolId)
        {
            if (!protocolId)
                return nullptr;

            for (
                IAccessWebAPI* protocol :
                accessProtocols
            )
            {
                if (
                    protocol &&
                    protocol->protocolId() &&
                    strcmp(
                        protocol->protocolId(),
                        protocolId
                    ) == 0
                )
                {
                    return protocol;
                }
            }

            return nullptr;
        }


    // ========================================================
    // COMPATIBILITY
    // ========================================================

    static DeviceMessageEngine*
    GetInstance()
    {
        return clientEngine;
    }


    // ========================================================
    // STATUS
    // ========================================================

    static bool IsClientRunning()
    {
        return clientRunning;
    }


    static bool IsServerRunning()
    {
        return serverRunning;
    }


    static bool IsAccessRunning()
    {
        return accessRunning;
    }


    static bool IsRunning()
    {
        /*
         * AccessControlService è event-driven e viene
         * eseguito tramite il server HTTP.
         */

        return
            clientRunning ||
            serverRunning;
    }


    // ========================================================
    // SHUTDOWN
    // ========================================================

    static void Shutdown()
    {
        Cleanup();

        webRequestCallback =
            nullptr;

        accessEventCallback =
            nullptr;

        accessAuthorizedCallback =
            nullptr;

        deviceResponseCallback =
            nullptr;
    }
};

// ============================================================
// HMI ENGINE
//
// Responsabilità:
//   1. Gestione connessioni Modbus TCP HMI
//   2. Poll dei client
//   3. Sincronizzazione Buffer <-> HMI
//
// Le modifiche HMI finiscono nel Buffer e seguono
// il normale percorso DomoManager -> NetworkManager -> RTU.
// ============================================================


class HMIEngine
{
public:

    // ========================================================
    // CONFIG
    // ========================================================

    struct Config
    {
        bool enabled;
        uint16_t port;

        Config()
            : enabled(false),
              port(502)
        {
        }
    };


    // ========================================================
    // EVENT -> HMI
    // ========================================================

    void PushEvent(
        const EventManager::Event& event)
    {
        if (!_config.enabled)
            return;

        if (!_running)
            return;


        const int area =
            event.area;


        // ----------------------------------------------------
        // VALIDATE AREA
        // ----------------------------------------------------

        if (area < 0 ||
            area >= static_cast<int>(_registerCount))
        {
            LOG_WF(
                "HMIEngine",
                "PushEvent ignored: invalid area=%d "
                "registerCount=%u",
                area,
                (unsigned)_registerCount
            );

            return;
        }


        const uint16_t value =
            static_cast<uint16_t>(
                event.value
            );


        // ====================================================
        // PROPAGATE EVENT TO ALL CONNECTED HMI CLIENTS
        // ====================================================

        for (uint8_t client = 0;
             client < _maxClients;
             ++client)
        {
            DMPlatform::Network::TCPClient& tcpClient =
                _context.clients[client];


            if (!tcpClient)
                continue;


            if (!tcpClient.connected())
                continue;


            DMPlatform::Modbus::TCPServer& server =
                _context.modbusServers[client];


            // ------------------------------------------------
            // ArduinoModbus:
            //
            // 1 = SUCCESS
            // 0 = FAILURE
            // ------------------------------------------------

            const int result =
                DMPlatform::Modbus::serverHoldingRegisterWrite(
                    server,
                    area,
                    value
                );


            if (result != 1)
            {
                LOG_WF(
                    "HMIEngine",
                    "PushEvent FAIL: "
                    "client=%u area=%d value=%u result=%d",
                    (unsigned)client,
                    area,
                    (unsigned)value,
                    result
                );

                continue;
            }


            // ------------------------------------------------
            // Allinea shadow alla scrittura interna.
            //
            // Evita che SyncHMIToBuffer() interpreti questa
            // scrittura come modifica proveniente dall'HMI.
            // ------------------------------------------------

            _registerShadow[client][area] =
                value;
        }
    }


private:

    // ========================================================
    // STATE
    // ========================================================

    Config _config;

    uint8_t _maxClients = 0;

    uint8_t _activeClients = 0;

    bool _loopEnabled = false;

    bool _running = false;


    // ========================================================
    // PROTOCOL
    // ========================================================

    DMNetworkManager::ProtocolId _protocolId = -1;


    // ========================================================
    // PENDING HMI UPDATE
    // ========================================================
    //
    // Conservato dal modello originale.
    //
    // Attualmente non viene prodotto da SyncHMIToBuffer(),
    // ma rimane disponibile per eventuali percorsi futuri.
    //
    // ========================================================

    struct PendingHMIUpdate
    {
        int area;
        long value;
        uint8_t client;
    };


    std::vector<
        PendingHMIUpdate
    > pendingUpdates;


    // ========================================================
    // HOLDING REGISTER SHADOW
    // ========================================================

    uint16_t _registerCount = 0;

    std::vector<
        std::vector<uint16_t>
    > _registerShadow;


    // ========================================================
    // EVENT SOURCE
    // ========================================================

    uint8_t _eventSource = 0;


    // ========================================================
    // NETWORK MANAGER
    // ========================================================

    DMNetworkManager* _networkManager = nullptr;


    // ========================================================
    // SOCKET OWNER
    // ========================================================

    SocketManager::OwnerId _socketOwner = -1;


    // ========================================================
    // SOCKET RESOURCE IDS
    // ========================================================
    //
    // Sono resource ID LOGICI del SocketManager.
    //
    // ========================================================

    static constexpr int SOCKET_RESOURCE_SERVER =
        0;


    static constexpr int SOCKET_RESOURCE_CLIENT_BASE =
        2000;


    // ========================================================
    // RESOURCE STATE
    // ========================================================

    bool _serverResourceAcquired = false;


    // ========================================================
    // NETWORK CONTEXT
    // ========================================================
    //
    // Nessun riferimento diretto a EthernetServer /
    // EthernetClient.
    //
    // Tutto passa da DMPlatform::Network.
    //
    // ========================================================

    struct Context
    {
        DMPlatform::Network::TCPServer* server = nullptr;

        std::vector<
            DMPlatform::Network::TCPClient
        > clients;

        std::vector<
            DMPlatform::Modbus::TCPServer
        > modbusServers;
    };


    Context _context;


    // ========================================================
    // RESOURCE ID HELPERS
    // ========================================================

    
    static int clientResourceId(
        uint8_t index)
    {
        return
            SOCKET_RESOURCE_CLIENT_BASE +
            static_cast<int>(index);
    }


    // ========================================================
    // SOCKET ACQUIRE
    // ========================================================

    bool acquireServerSocket()
    {
        if (!_networkManager)
        {
            LOG_EF(
                "HMIEngine",
                "NetworkManager unavailable"
            );

            return false;
        }


        if (_socketOwner < 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI socket owner unavailable"
            );

            return false;
        }


        const int slot =
            _networkManager
                ->sockets()
                .acquire(
                    _socketOwner,
                    SOCKET_RESOURCE_SERVER,
                    SocketManager::SocketKind::TCP_SERVER
                );


        if (slot < 0)
        {
            LOG_WF(
                "HMIEngine",
                "HMI TCP listener resource unavailable"
            );

            return false;
        }


        _serverResourceAcquired =
            true;


        return true;
    }


  

    // ========================================================
    // CLIENT RESOURCE ACQUIRE
    // ========================================================

    bool acquireClientSocket(
        uint8_t index)
    {
        if (!_networkManager)
        {
            LOG_EF(
                "HMIEngine",
                "NetworkManager unavailable"
            );

            return false;
        }


        if (_socketOwner < 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI socket owner unavailable"
            );

            return false;
        }


        const int resourceId =
            clientResourceId(index);


        const int slot =
            _networkManager
                ->sockets()
                .acquire(
                    _socketOwner,
                    resourceId,
                    SocketManager::SocketKind::TCP_CLIENT
                );


        if (slot < 0)
        {
            LOG_WF(
                "HMIEngine",
                "HMI client resource unavailable: "
                "client=%u resource=%d",
                (unsigned)index,
                resourceId
            );

            return false;
        }


        return true;
    }


    // ========================================================
    // SERVER RESOURCE RELEASE
    // ========================================================

    void releaseServerSocket()
    {
        if (!_networkManager)
            return;


        if (_socketOwner < 0)
            return;


        if (!_serverResourceAcquired)
            return;


        _networkManager
            ->sockets()
            .release(
                _socketOwner,
                SOCKET_RESOURCE_SERVER,
                SocketManager::SocketKind::TCP_SERVER
            );


        _serverResourceAcquired =
            false;
    }

    // ========================================================
    // CLIENT RESOURCE RELEASE
    // ========================================================

    void releaseClientSocket(
        uint8_t index)
    {
        if (!_networkManager)
            return;


        if (_socketOwner < 0)
            return;


        _networkManager
            ->sockets()
            .release(
                _socketOwner,
                clientResourceId(index),
                SocketManager::SocketKind::TCP_CLIENT
            );
    }


    // ========================================================
    // RELEASE ALL HMI RESOURCES
    // ========================================================

    void releaseAllSockets()
    {
        if (!_networkManager)
            return;


        if (_socketOwner < 0)
            return;


        _networkManager
            ->sockets()
            .releaseOwner(
                _socketOwner
            );


        _serverResourceAcquired =
            false;

    }


    // ========================================================
    // RESET REGISTER SHADOW
    // ========================================================

    void resetRegisterShadow()
    {
        _registerShadow.clear();


        _registerShadow.resize(
            _maxClients
        );


        for (uint8_t client = 0;
             client < _maxClients;
             ++client)
        {
            _registerShadow[client].assign(
                _registerCount,
                0
            );
        }
    }


    // ========================================================
    // DESTROY TCP SERVER OBJECT
    // ========================================================
    //
    // Il lifecycle del transport è incapsulato in:
    //
    //     DMPlatform::Network
    //
    // ========================================================

    void destroyServerObject()
    {
        if (_context.server == nullptr)
            return;

        DMPlatform::Network::TCPServer* server =
            _context.server;

        _context.server = nullptr;

        DMPlatform::Network::DestroyTCPServer(server);
    }


    // ========================================================
    // CREATE TCP SERVER
    //
    // Ricrea sempre il listener TCP.
    //
    // Questo rende Setup() idempotente e impedisce che un
    // precedente oggetto server blocchi la reinizializzazione.
    // ========================================================

    bool createServer()
    {
        // ====================================================
        // CLEANUP PREVIOUS SERVER
        // ====================================================

        if (_context.server != nullptr)
        {
            LOG_WF(
                "HMIEngine",
                "Existing HMI TCP server found: recreating"
            );

            destroyServerObject();
        }

        // ====================================================
        // CREATE SERVER
        // ====================================================

        _context.server =
            DMPlatform::Network::CreateTCPServer(
                _config.port
            );

        if (_context.server == nullptr)
        {
            LOG_EF(
                "HMIEngine",
                "Unable to create TCP server: port=%u",
                static_cast<unsigned>(
                    _config.port
                )
            );

            return false;
        }

        // ====================================================
        // START LISTENER
        // ====================================================

        if (
            !DMPlatform::Network::StartTCPServer(
                *_context.server
            )
        )
        {
            LOG_EF(
                "HMIEngine",
                "Unable to start HMI TCP listener: port=%u",
                static_cast<unsigned>(
                    _config.port
                )
            );

            destroyServerObject();

            return false;
        }

        LOG_IF(
            "HMIEngine",
            "HMI TCP listener started: port=%u",
            static_cast<unsigned>(
                _config.port
            )
        );

        return true;
    }


    // ========================================================
    // CLEANUP MODBUS SERVERS
    // ========================================================

    void cleanupModbusServers(
        uint8_t count)
    {
        const uint8_t available =
            static_cast<uint8_t>(
                _context.modbusServers.size()
            );


        if (count > available)
            count = available;


        for (uint8_t i = 0;
             i < count;
             ++i)
        {
            _context.modbusServers[i].end();
        }
    }


    // ========================================================
    // INITIALIZE MODBUS SERVERS
    // ========================================================

    bool initializeModbusServers()
    {
        uint8_t initializedServers = 0;


        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            DMPlatform::Modbus::TCPServer& server =
                _context.modbusServers[i];


            // =================================================
            // MODBUS BEGIN
            // =================================================
            //
            // ArduinoModbus:
            //
            //     1 = success
            //     0 = failure
            //
            // =================================================

            const int beginResult =
                server.begin();


            if (beginResult != 1)
            {
                LOG_EF(
                    "HMIEngine",
                    "Modbus TCP server initialization failed: "
                    "index=%u result=%d",
                    (unsigned)i,
                    beginResult
                );


                cleanupModbusServers(
                    initializedServers
                );


                return false;
            }


            // =================================================
            // HOLDING REGISTERS
            // =================================================
            //
            // ArduinoModbus implementation used on OPTA:
            //
            //     1 = success
            //     0 = failure
            //
            // =================================================

            const int result =
                DMPlatform::Modbus::serverConfigureHoldingRegisters(
                    server,
                    0,
                    static_cast<int>(_registerCount)
                );


            if (result != 1)
            {
                LOG_EF(
                    "HMIEngine",
                    "Holding register configuration failed: "
                    "index=%u count=%u result=%d",
                    (unsigned)i,
                    (unsigned)_registerCount,
                    result
                );


                server.end();


                cleanupModbusServers(
                    initializedServers
                );


                return false;
            }


            ++initializedServers;
        }


        LOG_IF(
            "HMIEngine",
            "HMI Modbus contexts initialized: count=%u",
            (unsigned)initializedServers
        );


        return true;
    }


    // ========================================================
    // SETUP SERVER
    //
    // Sequenza:
    //   1. cleanup eventuale runtime precedente
    //   2. inizializzazione slot client
    //   3. prenotazione listener
    //   4. creazione server TCP
    //   5. inizializzazione contesti Modbus
    // ========================================================

    bool SetupServer()
    {
        // ====================================================
        // CLEANUP PREVIOUS RUNTIME
        // ====================================================

        cleanupModbusServers(
            static_cast<uint8_t>(
                _context.modbusServers.size()
            )
        );

        for (
            size_t i = 0;
            i < _context.clients.size();
            ++i
        )
        {
            DMPlatform::Network::TCPClient& client =
                _context.clients[i];

            if (client)
            {
                DMPlatform::Network::Stop(
                    client
                );
            }
        }

        destroyServerObject();
        releaseAllSockets();

        // ====================================================
        // RESET STATE
        // ====================================================

        _activeClients = 0;
        _serverResourceAcquired = false;

        _context.clients.clear();
        _context.modbusServers.clear();

        // ====================================================
        // LOGICAL CLIENT SLOTS
        // ====================================================

        _context.clients.resize(
            _maxClients
        );

        _context.modbusServers.resize(
            _maxClients
        );

        // ====================================================
        // SERVER RESOURCE
        // ====================================================

        if (!acquireServerSocket())
        {
            LOG_WF(
                "HMIEngine",
                "Unable to reserve HMI TCP listener resource"
            );

            return false;
        }

        // ====================================================
        // PHYSICAL TCP SERVER
        // ====================================================

        if (!createServer())
        {
            LOG_WF(
                "HMIEngine",
                "Unable to create HMI TCP listener"
            );

            releaseServerSocket();

            return false;
        }

        // ====================================================
        // MODBUS SERVER CONTEXTS
        // ====================================================

        if (!initializeModbusServers())
        {
            LOG_EF(
                "HMIEngine",
                "Unable to initialize HMI Modbus contexts"
            );

            cleanupModbusServers(
                _maxClients
            );

            destroyServerObject();
            releaseServerSocket();

            return false;
        }

        // ====================================================
        // SUCCESS
        // ====================================================

        const unsigned socketDemand =
            1U +
            static_cast<unsigned>(
                _maxClients
            );

        LOG_IF(
            "HMIEngine",
            "HMI Modbus TCP started: "
            "port=%u maxClients=%u registers=%u "
            "socketDemand=%u",
            static_cast<unsigned>(
                _config.port
            ),
            static_cast<unsigned>(
                _maxClients
            ),
            static_cast<unsigned>(
                _registerCount
            ),
            socketDemand
        );

        if (_networkManager)
        {
            _networkManager
                ->sockets()
                .dump();
        }

        return true;
    }


    // ========================================================
    // CLEANUP DISCONNECTED CLIENTS
    // ========================================================

    void cleanupDisconnectedClients()
    {
        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            DMPlatform::Network::TCPClient& client =
                _context.clients[i];


            if (!client)
                continue;


            if (client.connected())
                continue;


            LOG_IF(
                "HMIEngine",
                "HMI client %u disconnected",
                (unsigned)i
            );


            DMPlatform::Network::Stop(
                client
            );


            releaseClientSocket(
                i
            );


            if (_activeClients > 0)
            {
                --_activeClients;
            }
        }
    }


    // ========================================================
    // ACCEPT NEW CLIENT
    //
    // Accetta il client, lo associa al server Modbus e copia
    // immediatamente nei registri holding l'intero stato del
    // buffer DomoManager.
    // ========================================================

    void acceptNewClient(
        DomoManager& manager)
    {
        if (_context.server == nullptr)
            return;

        DMPlatform::Network::TCPClient newClient =
            DMPlatform::Network::AcceptTCPClient(
                *_context.server
            );

        if (!newClient)
            return;

        for (uint8_t i = 0;
            i < _maxClients;
            ++i)
        {
            DMPlatform::Network::TCPClient& slot =
                _context.clients[i];

            if (slot &&
                slot.connected())
            {
                continue;
            }

            if (!acquireClientSocket(i))
            {
                LOG_WF(
                    "HMIEngine",
                    "HMI client rejected: "
                    "no socket resource, client=%u",
                    static_cast<unsigned>(i)
                );

                DMPlatform::Network::Stop(
                    newClient
                );

                return;
            }

            slot =
                newClient;

            _context.modbusServers[i].accept(
                slot
            );

            ++_activeClients;

            LOG_IF(
                "HMIEngine",
                "HMI client %u connected - active=%u",
                static_cast<unsigned>(i),
                static_cast<unsigned>(_activeClients)
            );

            /*
            * Sincronizzazione iniziale obbligatoria.
            *
            * Gli eventi generati prima della connessione del
            * pannello non sono più presenti nella coda, ma i loro
            * valori correnti sono conservati nel Buffer.
            */
            if (!syncBufferToClient(
                    manager,
                    i))
            {
                LOG_WF(
                    "HMIEngine",
                    "Initial buffer sync incomplete: client=%u",
                    static_cast<unsigned>(i)
                );
            }

            return;
        }

        LOG_WF(
            "HMIEngine",
            "HMI connection rejected: logical slots full"
        );

        DMPlatform::Network::Stop(
            newClient
        );
    }


    // ========================================================
    // CONNECTION MANAGEMENT
    // ========================================================

    void ProcessConnections(
        DomoManager& manager)
    {
        cleanupDisconnectedClients();

        acceptNewClient(
            manager
        );
    }


    // ========================================================
    // MODBUS POLL
    // ========================================================

    void Poll()
    {
        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            DMPlatform::Network::TCPClient& client =
                _context.clients[i];


            if (!client)
                continue;


            if (!client.connected())
                continue;


            _context.modbusServers[i].poll();
        }
    }


    // ========================================================
    // PENDING UPDATE CONSUMER
    // ========================================================

    bool consumePendingUpdate(
        uint8_t client,
        int area,
        long value)
    {
        for (auto it =
                 pendingUpdates.begin();
             it != pendingUpdates.end();
             ++it)
        {
            if (it->client == client &&
                it->area == area &&
                it->value == value)
            {
                pendingUpdates.erase(
                    it
                );

                return true;
            }
        }


        return false;
    }


    // ========================================================
    // HMI -> EVENT MANAGER
    // ========================================================

    void SyncHMIToBuffer(
        DomoManager& manager,
        unsigned long now)
    {
        (void)now;


        // ----------------------------------------------------
        // La rilevazione della modifica HMI è indipendente
        // dalla coda corrente dell'EventManager.
        // ----------------------------------------------------

        for (uint8_t client = 0;
             client < _maxClients;
             ++client)
        {
            DMPlatform::Network::TCPClient& tcpClient =
                _context.clients[client];


            if (!tcpClient)
                continue;


            if (!tcpClient.connected())
                continue;


            DMPlatform::Modbus::TCPServer& server =
                _context.modbusServers[client];


            for (uint16_t area = 0;
                 area < _registerCount;
                 ++area)
            {
                const long rawValue =
                    DMPlatform::Modbus::serverHoldingRegisterRead(
                        server,
                        static_cast<int>(area)
                    );


                // ------------------------------------------------
                // -1 = failure
                // >=0 = valid value
                // ------------------------------------------------

                if (rawValue < 0)
                    continue;


                const uint16_t value =
                    static_cast<uint16_t>(
                        rawValue
                    );


                // ------------------------------------------------
                // Nessuna modifica.
                // ------------------------------------------------

                if (_registerShadow[client][area] ==
                    value)
                {
                    continue;
                }


                // ------------------------------------------------
                // Aggiorna prima la shadow.
                // ------------------------------------------------

                _registerShadow[client][area] =
                    value;


                // ------------------------------------------------
                // HMI -> EventManager.
                // ------------------------------------------------
                
                manager.forceEvent(
                    static_cast<int>(area),
                    value,
                    static_cast<uint8_t>(_eventSource)
                );
            }
        }
    }

    // ========================================================
    // SYNC BUFFER TO CLIENT
    //
    // Copia lo stato corrente del Buffer nei registri holding
    // del client HMI appena connesso.
    //
    // Serve perché gli eventi generati prima della connessione
    // del client non possono essere ricevuti retroattivamente.
    // ========================================================

    bool syncBufferToClient(
        DomoManager& manager,
        uint8_t client)
    {
        if (client >= _maxClients)
        {
            LOG_EF(
                "HMIEngine",
                "Initial sync failed: invalid client=%u",
                static_cast<unsigned>(client)
            );

            return false;
        }

        if (client >= _context.clients.size() ||
            client >= _context.modbusServers.size() ||
            client >= _registerShadow.size())
        {
            LOG_EF(
                "HMIEngine",
                "Initial sync failed: "
                "client structures unavailable, client=%u",
                static_cast<unsigned>(client)
            );

            return false;
        }

        DMPlatform::Network::TCPClient& tcpClient =
            _context.clients[client];

        if (!tcpClient ||
            !tcpClient.connected())
        {
            LOG_WF(
                "HMIEngine",
                "Initial sync skipped: "
                "client=%u not connected",
                static_cast<unsigned>(client)
            );

            return false;
        }

        DMPlatform::Modbus::TCPServer& server =
            _context.modbusServers[client];

        Buffer& buffer =
            manager.getBuffer();

        uint16_t synchronized = 0;
        uint16_t failed = 0;

        for (uint16_t area = 0;
            area < _registerCount;
            ++area)
        {
            const long rawValue =
                buffer.getValueFast(
                    static_cast<int>(area)
                );

            /*
            * I registri HMI sono uint16_t.
            *
            * Il cast conserva la rappresentazione binaria anche
            * per bitmask con bit 15 attivo.
            */
            const uint16_t value =
                static_cast<uint16_t>(
                    rawValue
                );

            const int result =
                DMPlatform::Modbus::
                    serverHoldingRegisterWrite(
                        server,
                        static_cast<int>(area),
                        value
                    );

            if (result != 1)
            {
                ++failed;

                LOG_WF(
                    "HMIEngine",
                    "Initial sync write failed: "
                    "client=%u area=%u value=%u result=%d",
                    static_cast<unsigned>(client),
                    static_cast<unsigned>(area),
                    static_cast<unsigned>(value),
                    result
                );

                continue;
            }

            /*
            * Lo shadow deve essere identico al registro appena
            * scritto, altrimenti SyncHMIToBuffer() interpreterebbe
            * la sincronizzazione iniziale come comando proveniente
            * dal pannello.
            */
            _registerShadow[client][area] =
                value;

            ++synchronized;
        }

        LOG_IF(
            "HMIEngine",
            "Initial buffer sync completed: "
            "client=%u synchronized=%u failed=%u",
            static_cast<unsigned>(client),
            static_cast<unsigned>(synchronized),
            static_cast<unsigned>(failed)
        );

        return failed == 0;
    }
public:

    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    HMIEngine()
        : _config(),
          _maxClients(0),
          _activeClients(0),
          _loopEnabled(false),
          _running(false),
          _protocolId(-1),
          _registerCount(0),
          _eventSource(0),
          _networkManager(nullptr),
          _socketOwner(-1),
          _serverResourceAcquired(false),
          _context()
    {
    }


    // ========================================================
    // SETUP
    // ========================================================

    void Setup(
        const DomoManagerConfig::HMI& cfg,
        DMNetworkManager& networkManager,
        SocketManager::OwnerId socketOwner,
        DMNetworkManager::ProtocolId protocol,
        uint8_t maxClients,
        uint16_t registerCount,
        uint8_t eventSource)
    {
        // ----------------------------------------------------
        // Network references
        // ----------------------------------------------------

        _networkManager =
            &networkManager;


        _socketOwner =
            socketOwner;


        _protocolId =
            protocol;


        // ----------------------------------------------------
        // Configuration
        // ----------------------------------------------------

        _config.enabled =
            cfg.enabled;


        _config.port =
            cfg.port;


        _maxClients =
            maxClients;


        _registerCount =
            registerCount;


        _eventSource =
            eventSource;


        // ----------------------------------------------------
        // Runtime state
        // ----------------------------------------------------

        _activeClients =
            0;


        _loopEnabled =
            false;


        _running =
            false;

        _serverResourceAcquired =
            false;


        // ----------------------------------------------------
        // Reset shadow
        // ----------------------------------------------------

        resetRegisterShadow();


        // ====================================================
        // DISABLED
        // ====================================================

        if (!_config.enabled)
        {
            LOG_IF(
                "HMIEngine",
                "HMI disabled"
            );

            return;
        }


        // ====================================================
        // VALIDATE MAX CLIENTS
        // ====================================================

        if (_maxClients == 0)
        {
            LOG_WF(
                "HMIEngine",
                "HMI enabled but maxClients=0"
            );

            return;
        }


        // ====================================================
        // VALIDATE REGISTER COUNT
        // ====================================================

        if (_registerCount == 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI enabled but registerCount=0"
            );

            return;
        }


        // ====================================================
        // VALIDATE PORT
        // ====================================================

        if (_config.port == 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI enabled but TCP port=0"
            );

            return;
        }


        // ====================================================
        // VALIDATE SOCKET OWNER
        // ====================================================

        if (_socketOwner < 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI socket owner is invalid"
            );

            return;
        }


        // ====================================================
        // SETUP
        // ====================================================

        _running =
            SetupServer();


        if (!_running)
        {
            LOG_WF(
                "HMIEngine",
                "HMI network resources unavailable - "
                "engine remains disabled"
            );

            return;
        }
    }


    // ========================================================
    // LOOP ENABLE
    // ========================================================

    void SetLoopEnabled(
        bool enabled)
    {
        if (!_config.enabled)
            return;


        if (!_running)
            return;


        if (_loopEnabled == enabled)
            return;


        _loopEnabled =
            enabled;


        LOG_IF(
            "HMIEngine",
            "HMI loop %s",
            _loopEnabled
                ? "ENABLED"
                : "DISABLED"
        );
    }


    // ========================================================
    // NETWORK LOOP
    // ========================================================

    void ProcessNetwork(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;
        (void)now;

        if (!_config.enabled)
            return;

        if (!_running)
            return;

        if (!_loopEnabled)
            return;

        ProcessConnections(
            manager
        );

        Poll();
    }


    // ========================================================
    // SYNC LOOP
    // ========================================================

    void Sync(
        DomoManager& manager,
        unsigned long now)
    {
        if (!_config.enabled)
            return;


        if (!_running)
            return;


        if (!_loopEnabled)
            return;


        SyncHMIToBuffer(
            manager,
            now
        );
    }


    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    bool enabled() const
    {
        return _config.enabled;
    }


    bool running() const
    {
        return _running;
    }


    bool loopEnabled() const
    {
        return _loopEnabled;
    }


    uint16_t port() const
    {
        return _config.port;
    }


    uint8_t activeClients() const
    {
        return _activeClients;
    }


    uint8_t maxClients() const
    {
        return _maxClients;
    }


    uint16_t registerCount() const
    {
        return _registerCount;
    }


    DMNetworkManager::ProtocolId protocolId() const
    {
        return _protocolId;
    }


    SocketManager::OwnerId socketOwner() const
    {
        return _socketOwner;
    }
};

class SecurityEngine
{
private:
    // ============================================================
    // SECURITY COMMAND AREA LOOKUP
    //
    // Verifica se l'area ricevuta appartiene alle aree comando
    // configurate per il sottosistema Security.
    //
    // Non modifica alcuno stato.
    // Non genera log.
    // Non applica alcun comando.
    // ============================================================

    static bool IsCommandArea(
        int area)
    {
        if (area < 0 ||
            !context.config)
        {
            return false;
        }

        const FrontendConfig::Security& config =
            *context.config;

        // ========================================================
        // PANEL COMMAND AREAS
        // ========================================================

        if (config.panelBitCommandArea >= 0 &&
            area == config.panelBitCommandArea)
        {
            return true;
        }

        if (config.panelCommandArea >= 0 &&
            area == config.panelCommandArea)
        {
            return true;
        }

        // ========================================================
        // ZONE COMMAND AREAS
        // ========================================================

        if (config.zones &&
            config.zoneCount > 0)
        {
            for (size_t index = 0;
                index < config.zoneCount;
                ++index)
            {
                const int commandArea =
                    config.zones[index].cmdArea;

                if (commandArea >= 0 &&
                    area == commandArea)
                {
                    return true;
                }
            }
        }

        // ========================================================
        // SENSOR COMMAND AREAS
        // ========================================================

        if (config.sensors &&
            config.count > 0)
        {
            for (size_t index = 0;
                index < config.count;
                ++index)
            {
                const int commandArea =
                    config.sensors[index].cmdArea;

                if (commandArea >= 0 &&
                    area == commandArea)
                {
                    return true;
                }
            }
        }

        return false;
    }

    // ============================================================
    // ALARM CALLBACK
    // ============================================================

    using AlarmCallback =
        void (*)(bool engaged,
                 bool active,
                 uint64_t currentAlarmMask,
                 uint64_t effectiveAlarmMask,
                 uint64_t systemBitmask);

    // ============================================================
    // ALARM CALLBACK STATE
    // ============================================================

    struct AlarmCallbackState
    {
        AlarmCallback callback;
        bool valid;
        bool engaged;
        bool active;
        uint64_t currentAlarmMask;
        uint64_t effectiveAlarmMask;
        uint64_t systemBitmask;
    };

    // ============================================================
    // ALARM SILENCE STATE
    // ============================================================

    struct AlarmSilenceState
    {
        bool valid;
        uint64_t lastMask;
    };

    // ============================================================
    // CONTEXT
    // ============================================================

    struct Context
    {
        DomoManager* manager;
        FrontendConfig::Security* config;
        uint8_t securitySource;
    };

    // ============================================================
    // RUNTIME
    // ============================================================

    struct RuntimeState
    {
        bool initialized;
        bool hmiInitialSyncPending;
        bool reportOnChange;
        uint8_t changeFlags;
    };

    // ============================================================
    // STATIC STATE
    // ============================================================

    static inline AlarmCallbackState alarmState;
    static inline AlarmSilenceState silenceState;
    static inline Context context;
    static inline RuntimeState runtime;

    // ============================================================
    // EVENT ENCODER
    // ============================================================

    static long EncodeEvent(const AlarmPanelInterface::Event& event)
    {
        const uint32_t type =
            static_cast<uint32_t>(event.type) & 0xFFu;

        const uint32_t zone =
            static_cast<uint32_t>(
                event.zone < 0 ? 0xFF : event.zone
            ) & 0xFFu;

        const uint32_t partition =
            static_cast<uint32_t>(
                event.partition < 0 ? 0xFF : event.partition
            ) & 0xFFu;

        return static_cast<long>(
            type |
            (zone << 8) |
            (partition << 16)
        );
    }

    // ============================================================
    // PUBLISH PANEL EVENT
    // ============================================================

    static void PublishPanelEvent(
        const AlarmPanelInterface::Event& event,
        bool alarmEvent)
    {
        (void)alarmEvent;

        if(!runtime.initialized ||
        !context.manager ||
        !context.config ||
        context.config->eventArea < 0 ||
        context.securitySource == 255)
        {
            return;
        }

        const long value =
            EncodeEvent(event);

        context.manager->getEventManager().push(
            context.config->eventArea,
            value,
            context.securitySource
        );

        runtime.changeFlags |=
            SecurityOrchestrator::CHANGE_PANEL;
    }

    // ============================================================
    // ALARM PANEL EVENT CALLBACKS
    // ============================================================

    static void AlarmPanelAlarmCallback(
        const AlarmPanelInterface::Event& event)
    {
        PublishPanelEvent(event, true);
    }

    static void AlarmPanelEventCallback(
        const AlarmPanelInterface::Event& event)
    {
        PublishPanelEvent(event, false);
    }

    // ============================================================
    // UPDATE SECURITY HMI
    // ============================================================


    static void UpdateSecurityHmi(bool force = false)
    {
        if(!runtime.initialized ||
        !context.manager ||
        !context.config)
        {
            return;
        }

        const uint8_t changes =
            runtime.changeFlags;

        if(!force &&
        changes == SecurityOrchestrator::CHANGE_NONE)
        {
            return;
        }

        DomoManager* dm =
            context.manager;

        FrontendConfig::Security* cfg =
            context.config;

        auto& buffer =
            dm->getBuffer();

        auto& hmi =
            SecurityHmiInterface::instance();

        auto& ws =
            SecurityOrchestrator::getWiredSensors();

        // ========================================================
        // SENSORI
        // ========================================================

        if(force ||
        (changes &
            (SecurityOrchestrator::CHANGE_SENSOR |
            SecurityOrchestrator::CHANGE_COMMAND)))
        {
            const auto* sensorCfg =
                ws.GetConfig();

            if(sensorCfg)
            {
                const size_t count =
                    ws.Count();

                for(size_t i = 0; i < count; ++i)
                {
                    const int area =
                        sensorCfg[i].statusArea;

                    if(area < 0)
                        continue;

                    SecurityHmiInterface::SensorState state;

                    if(!hmi.getSensorState(i, state))
                        continue;

                    uint16_t value = 0;

                    value |= state.active[0]  ? 0x0001 : 0;
                    value |= state.active[1]  ? 0x0002 : 0;
                    value |= state.active[2]  ? 0x0004 : 0;
                    value |= state.active[3]  ? 0x0008 : 0;

                    value |= state.alarm[0]   ? 0x0010 : 0;
                    value |= state.alarm[1]   ? 0x0020 : 0;
                    value |= state.alarm[2]   ? 0x0040 : 0;
                    value |= state.alarm[3]   ? 0x0080 : 0;

                    value |= state.inhibit[0] ? 0x0100 : 0;
                    value |= state.inhibit[1] ? 0x0200 : 0;
                    value |= state.inhibit[2] ? 0x0400 : 0;
                    value |= state.inhibit[3] ? 0x0800 : 0;

                    value |= state.alarmOut ? 0x1000 : 0;
                    value |= !state.enabled  ? 0x2000 : 0;
                    value |= state.rtMem     ? 0x4000 : 0;
                    value |= state.h24Mem    ? 0x8000 : 0;

                    if(force ||
                    buffer.getValueFast(area) !=
                    static_cast<long>(value))
                    {
                        dm->forceInternalEvent(
                            area,
                            static_cast<long>(value)
                        );
                    }
                }
            }
        }

        // ========================================================
        // ZONE
        // ========================================================

        if(force ||
        (changes &
            SecurityOrchestrator::CHANGE_ZONE))
        {
            auto& zones =
                ws.Zones();

            const size_t zoneCount =
                ws.GetZoneCount();

            for(size_t i = 0;
                i < zoneCount;
                ++i)
            {
                const char* zoneName =
                    ws.GetZoneName(i);

                if(!zoneName)
                    continue;

                const int area =
                    zones.GetZoneStatusArea(zoneName);

                if(area < 0)
                    continue;

                SecurityHmiInterface::ZoneState state;

                if(!hmi.getZoneState(i, state))
                    continue;

                uint16_t value = 0;

                value |= state.alarm    ? 0x0001 : 0;
                value |= state.alarmH24 ? 0x0002 : 0;
                value |= state.trouble  ? 0x0004 : 0;
                value |= state.bypassed ? 0x0008 : 0;
                value |= state.rtMem    ? 0x0010 : 0;
                value |= state.h24Mem   ? 0x0020 : 0;

                if(force ||
                buffer.getValueFast(area) !=
                static_cast<long>(value))
                {
                    dm->forceInternalEvent(
                        area,
                        static_cast<long>(value)
                    );
                }
            }
        }

        // ========================================================
        // SISTEMA / CENTRALE
        // ========================================================

        if(force ||
        (changes &
            (SecurityOrchestrator::CHANGE_SYSTEM |
            SecurityOrchestrator::CHANGE_PANEL  |
            SecurityOrchestrator::CHANGE_COMM   |
            SecurityOrchestrator::CHANGE_COMMAND |
            SecurityOrchestrator::CHANGE_ZONE   |
            SecurityOrchestrator::CHANGE_SENSOR)))
        {
            SecurityHmiInterface::PanelState state;

            if(!hmi.getPanelState(state, 0))
                return;

            // ----------------------------------------------------
            // SYSTEM STATUS
            // ----------------------------------------------------

            const int statusArea =
                cfg->statusArea;

            if(statusArea >= 0)
            {
                const uint16_t value =
                    static_cast<uint16_t>(
                        state.systemBitmask
                    );

                if(force ||
                buffer.getValueFast(statusArea) !=
                static_cast<long>(value))
                {
                    dm->forceInternalEvent(
                        statusArea,
                        static_cast<long>(value)
                    );
                }
            }

            // ----------------------------------------------------
            // SYSTEM STATUS 2
            // ----------------------------------------------------

            const int statusArea2 =
                cfg->statusArea2;

            if(statusArea2 >= 0)
            {
                uint16_t value = 0;

                const bool engaged =
                    state.armState !=
                    AlarmPanelInterface::ArmState::DISARMED;

                // ------------------------------------------------
                // Presenza di una memoria di allarme:
                // bit 6..11 = memorie centrale
                // ------------------------------------------------

                const bool alarmMemorized =
                    (static_cast<uint16_t>(
                        state.systemBitmask
                    ) & 0x0FC0u) != 0;

                const bool alarmSilenceAvailable =
                    engaged &&
                    alarmMemorized;

                value |= state.connected
                    ? 0x0001 : 0;

                value |= state.communicationFault
                    ? 0x0002 : 0;

                value |= state.channelSupervised
                    ? 0x0004 : 0;

                // bit 3 = PRONTO
                value |= state.ready
                    ? 0x0008 : 0;

                // bit 4 = AVARIA
                value |= state.globalTrouble
                    ? 0x0010 : 0;

                // bit 5 = PRONTO PER ATTIVAZIONE
                //
                // Attivo solo se:
                // - centrale pronta
                // - nessuna avaria
                // - nessun allarme memorizzato
                value |= (
                    state.readyForArm &&
                    state.ready &&
                    !state.globalTrouble &&
                    !alarmMemorized
                )
                    ? 0x0020 : 0;

                // bit 6 = disponibilità tacitazione allarme
                value |= alarmSilenceAvailable
                    ? 0x0040 : 0;

                if(force ||
                buffer.getValueFast(statusArea2) !=
                static_cast<long>(value))
                {
                    dm->forceInternalEvent(
                        statusArea2,
                        static_cast<long>(value)
                    );
                }
            }

            // ----------------------------------------------------
            // ARM STATE
            // ----------------------------------------------------

            const int armStateArea =
                cfg->armStateArea;

            if(armStateArea >= 0)
            {
                const long value =
                    static_cast<long>(
                        static_cast<uint8_t>(
                            state.armState
                        )
                    );

                if(force ||
                buffer.getValueFast(armStateArea) !=
                value)
                {
                    dm->forceInternalEvent(
                        armStateArea,
                        value
                    );
                }
            }
        }
    }


    // ============================================================
    // UPDATE ALARM CALLBACK
    // ============================================================

    static void UpdateAlarmCallback()
    {
        auto& hmi =
            SecurityHmiInterface::instance();

        SecurityHmiInterface::PanelState panelState;

        if(!hmi.getPanelState(panelState, 0))
            return;

        const bool engaged =
            panelState.armState !=
            AlarmPanelInterface::ArmState::DISARMED;

        const uint64_t systemBitmask =
            static_cast<uint64_t>(
                panelState.systemBitmask
            );

        const uint64_t currentAlarmMask =
            SecurityOrchestrator::getCurrentAlarmMask();

        auto& alarmPanel =
            DomoManagerAlarmPanel::instance();

        const uint64_t effectiveAlarmMask =
            alarmPanel.getEffectiveAlarmMask();

        const bool active =
            effectiveAlarmMask != 0;

        if(alarmState.valid &&
        engaged == alarmState.engaged &&
        active == alarmState.active &&
        currentAlarmMask ==
            alarmState.currentAlarmMask &&
        effectiveAlarmMask ==
            alarmState.effectiveAlarmMask &&
        systemBitmask ==
            alarmState.systemBitmask)
        {
            return;
        }

        alarmState.engaged =
            engaged;

        alarmState.active =
            active;

        alarmState.currentAlarmMask =
            currentAlarmMask;

        alarmState.effectiveAlarmMask =
            effectiveAlarmMask;

        alarmState.systemBitmask =
            systemBitmask;

        alarmState.valid =
            true;

        if(alarmState.callback)
        {
            alarmState.callback(
                engaged,
                active,
                currentAlarmMask,
                effectiveAlarmMask,
                systemBitmask
            );
        }
    }

public:

    // ============================================================
    // SETUP
    // ============================================================

    static bool Setup(
        DomoManager& dm,
        FrontendConfig::Security& cfg,
        uint8_t source)
    {
        context.manager =
            &dm;

        context.config =
            &cfg;

        context.securitySource =
            source;

        runtime.initialized =
            false;

        runtime.hmiInitialSyncPending =
            true;

        runtime.reportOnChange =
            cfg.reportOnChange;

        runtime.changeFlags =
            SecurityOrchestrator::CHANGE_NONE;

        alarmState.valid =
            false;

        alarmState.engaged =
            false;

        alarmState.active =
            false;

        alarmState.currentAlarmMask =
            0;

        alarmState.effectiveAlarmMask =
            0;

        alarmState.systemBitmask =
            0;

        silenceState.valid =
            false;

        silenceState.lastMask =
            0;

        if(!cfg.enabled)
            return false;

        if(source == 255)
        {
            LOG_EF(
                "SECURITY",
                "Invalid EventManager source"
            );
            return false;
        }

        SecurityOrchestrator::Setup(&cfg);

        if(!SecurityOrchestrator::isInitialized())
        {
            LOG_EF(
                "SECURITY",
                "Security orchestrator initialization failed"
            );
            return false;
        }

        DMRuntime* runtimeInstance =
            DMRuntime::GetInstance();

        if(runtimeInstance)
        {
            runtimeInstance->SetSecurityAreaErrorCallback(
                [](int area)
                {
                    SecurityEngine::NotifyAreaErrorChanged(area);
                }
            );
        }
        else
        {
            LOG_EF(
                "SECURITY",
                "DMRuntime instance not available: "
                "device error callback NOT registered"
            );
        }

        auto& panel =
            DomoManagerAlarmPanel::instance();

        panel.setAlarmCallback(
            AlarmPanelAlarmCallback
        );

        panel.setEventCallback(
            AlarmPanelEventCallback
        );

        runtime.initialized =
            true;

        return true;
    }

    // ============================================================
    // SET ALARM CALLBACK
    // ============================================================

    static void setAlarmCallback(
        AlarmCallback callback)
    {
        alarmState.callback =
            callback;

        alarmState.valid =
            false;

        alarmState.engaged =
            false;

        alarmState.active =
            false;

        alarmState.currentAlarmMask =
            0;

        alarmState.effectiveAlarmMask =
            0;

        alarmState.systemBitmask =
            0;
    }

    // ============================================================
    // AREA ERROR EVENT
    // ============================================================

    static bool NotifyAreaErrorChanged(int area)
    {
        if(!runtime.initialized || area<0)
            return false;

        if(!SecurityOrchestrator::NotifyAreaErrorChanged(area))
            return false;

        runtime.changeFlags |=
            SecurityOrchestrator::CHANGE_SENSOR |
            SecurityOrchestrator::CHANGE_ZONE;

        return true;
    }


    // ============================================================
    // LOOP
    // ============================================================

    static bool Loop(
        DomoManager& dm,
        unsigned long now)
    {
        if(!runtime.initialized ||
        !context.manager ||
        !context.config ||
        !context.config->enabled)
        {
            return false;
        }

        if(!dm.getPowerOnCycleCompleted())
            return false;

        const bool alarmEngaged =
            DomoManagerAlarmPanel::instance()
                .getArmState(0) !=
            AlarmPanelInterface::ArmState::DISARMED;

        runtime.changeFlags |=
            SecurityOrchestrator::Loop(
                now,
                alarmEngaged
            );

        const uint64_t currentAlarmMask =
            SecurityOrchestrator::getCurrentAlarmMask();

        if(!silenceState.valid ||
        currentAlarmMask !=
            silenceState.lastMask)
        {
            DomoManagerAlarmPanel::instance()
                .synchronizeAlarmSilence(
                    currentAlarmMask
                );

            silenceState.lastMask =
                currentAlarmMask;

            silenceState.valid =
                true;
        }

        const bool forceHmi =
            runtime.hmiInitialSyncPending;

        const uint8_t changesBeforeHmi =
            runtime.changeFlags;

        UpdateSecurityHmi(forceHmi);

        if(forceHmi)
            runtime.hmiInitialSyncPending =
                false;

        if(forceHmi ||
        !alarmState.valid ||
        (changesBeforeHmi &
            (SecurityOrchestrator::CHANGE_SYSTEM |
            SecurityOrchestrator::CHANGE_PANEL  |
            SecurityOrchestrator::CHANGE_COMM   |
            SecurityOrchestrator::CHANGE_COMMAND |
            SecurityOrchestrator::CHANGE_ZONE)))
        {
            UpdateAlarmCallback();
        }

        if(runtime.changeFlags ==
        SecurityOrchestrator::CHANGE_NONE)
        {
            return false;
        }

        if(runtime.reportOnChange &&
        (runtime.changeFlags &
            (SecurityOrchestrator::CHANGE_ZONE |
            SecurityOrchestrator::CHANGE_SYSTEM |
            SecurityOrchestrator::CHANGE_PANEL |
            SecurityOrchestrator::CHANGE_COMM |
            SecurityOrchestrator::CHANGE_COMMAND)))
        {
            SecurityOrchestrator::Diagnostic::CoreReport();
        }

        runtime.changeFlags =
            SecurityOrchestrator::CHANGE_NONE;

        return true;
    }

    // ============================================================
    // REMOTE PANIC
    // ============================================================

    static bool TriggerPanic(unsigned long now)
    {
        if (!runtime.initialized ||
            !context.config ||
            !context.config->enabled ||
            context.config->panelBitCommandArea < 0)
        {
            return false;
        }

        return ApplyCommand(
            context.config->panelBitCommandArea,
            static_cast<long>(
                SecurityOrchestrator::SystemManager::ALARM_PANIC
            ),
            now
        );
    }

    // ============================================================
    // EVENT MANAGER CALLBACK
    //
    // Riceve gli eventi globali da EventManager, ma inoltra ad
    // ApplyCommand() esclusivamente le aree comando appartenenti
    // al sottosistema Security.
    // ============================================================

    static void EventCallback(
        const EventManager::Event& event)
    {
        // ========================================================
        // RUNTIME VALIDATION
        // ========================================================

        if (!runtime.initialized ||
            !context.manager ||
            !context.config ||
            !context.config->enabled)
        {
            return;
        }

        // ========================================================
        // SECURITY AREA FILTER
        // ========================================================

        if (!IsCommandArea(
                event.area))
        {
            return;
        }

        // ========================================================
        // COMMAND DISPATCH
        // ========================================================

        const unsigned long now =
            context.manager
                ->getTimeManager()
                .nowMs();

        ApplyCommand(
            event.area,
            event.value,
            now
        );
    }

    // ============================================================
    // APPLY COMMAND
    // ============================================================

    static bool ApplyCommand(
        int area,
        long value,
        unsigned long now)
    {
        // ========================================================
        // RUNTIME VALIDATION
        // ========================================================

        if (!runtime.initialized ||
            !context.config ||
            !context.manager ||
            area < 0)
        {
            return false;
        }

        using Orchestrator =
            SecurityOrchestrator;

        FrontendConfig::Security& securityConfig =
            *context.config;

        DomoManager& manager =
            *context.manager;

        LOG_IF(
            "SECURITY",
            "ApplyCommand ENTRY: area=%d value=%ld now=%lu",
            area,
            value,
            now
        );

        // ========================================================
        // PANEL BIT COMMAND
        // ========================================================

        if (securityConfig.panelBitCommandArea >= 0 &&
            area == securityConfig.panelBitCommandArea)
        {
            LOG_IF(
                "SECURITY",
                "PANEL BIT AREA MATCH: "
                "area=%d value=0x%08lX",
                area,
                static_cast<unsigned long>(
                    value
                )
            );

            const bool handled =
                Orchestrator::ApplyPanelBitCommand(
                    area,
                    value
                );

            if (!handled)
            {
                LOG_WF(
                    "SECURITY",
                    "Panel bit command rejected: "
                    "area=%d value=0x%08lX",
                    area,
                    static_cast<unsigned long>(
                        value
                    )
                );

                /*
                * L'area appartiene comunque al sottosistema Security.
                * Il comando è stato riconosciuto, ma il valore è stato
                * rifiutato dall'orchestrator.
                */
                return true;
            }

            runtime.changeFlags |=
                Orchestrator::CHANGE_COMMAND;

            /*
            * I comandi a bit sono impulsivi.
            * Dopo l'elaborazione la relativa area viene azzerata.
            */
            manager
                .getBuffer()
                .WriteElement(
                    area,
                    0,
                    false,
                    now
                );

            LOG_IF(
                "SECURITY",
                "PANEL BIT CMD applied and reset: "
                "area=%d value=0",
                area
            );

            return true;
        }

        // ========================================================
        // ALARM PANEL COMMAND
        // ========================================================

        if (securityConfig.panelCommandArea >= 0 &&
            area == securityConfig.panelCommandArea)
        {
            const bool handled =
                Orchestrator::ApplyPanelCommand(
                    area,
                    value
                );

            if (!handled)
            {
                LOG_WF(
                    "SECURITY",
                    "Panel command rejected: "
                    "area=%d value=%ld",
                    area,
                    value
                );

                /*
                * Anche in questo caso l'area è stata riconosciuta
                * come area Security.
                */
                return true;
            }

            runtime.changeFlags |=
                Orchestrator::CHANGE_COMMAND;

            LOG_IF(
                "SECURITY",
                "PANEL CMD applied: "
                "area=%d value=%ld",
                area,
                value
            );

            return true;
        }

        // ========================================================
        // SENSOR / ZONE COMMAND
        //
        // ApplySecurityCommand esegue internamente il routing sulle
        // cmdArea dei sensori e delle zone.
        // ========================================================

        if (!Orchestrator::ApplySecurityCommand(
                area,
                value))
        {
            return false;
        }

        runtime.changeFlags |=
            Orchestrator::CHANGE_COMMAND |
            Orchestrator::CHANGE_SENSOR  |
            Orchestrator::CHANGE_ZONE    |
            Orchestrator::CHANGE_SYSTEM;

        LOG_IF(
            "SECURITY",
            "SENSOR/ZONE CMD applied: "
            "area=%d value=%ld",
            area,
            value
        );

        return true;
    }

    // ============================================================
    // DIRECT STATUS ACCESS
    // ============================================================

    static long GetStatus()
    {
        if (!runtime.initialized)
            return 0;

        return SecurityOrchestrator::
            getSystem()
            .getBitmask();
    }

    // ============================================================
    // ACCESSORS
    // ============================================================

    static bool enabled()
    {
        return runtime.initialized &&
               context.config &&
               context.config->enabled;
    }

    static bool isInitialized()
    {
        return runtime.initialized;
    }

    static uint8_t getChangeFlags()
    {
        return runtime.changeFlags;
    }

    static uint8_t getSource()
    {
        return context.securitySource;
    }

    static DomoManager* getManager()
    {
        return context.manager;
    }

    static FrontendConfig::Security* getConfig()
    {
        return context.config;
    }
};

// ============================================================
// TASK ENGINE
//
// Registra esclusivamente i task standard del frontend.
//
// Non conosce:
// - AEEEngine;
// - BridgeEngine;
// - MQTTEngine;
// - DomoManagerFrontendEngine;
// - CommunicationScheduler.
//
// I servizi applicativi e di comunicazione vengono registrati
// dal composition root tramite AddTask().
// ============================================================

class TaskEngine
{
public:
    using TaskFn =
        void (*)(
            DomoManager& manager,
            unsigned long now
        );

private:
    // ========================================================
    // TASK: HVAC
    // ========================================================

    static void Task_HVAC(
        DomoManager& manager,
        unsigned long now)
    {
        const FrontendConfig& frontendConfig =
            TaskEngineOrchestrator::getCfg();

        const auto& config =
            frontendConfig.hvac;

        // ----------------------------------------------------
        // TIME
        // ----------------------------------------------------

        struct tm currentTime {};

        manager
            .getTimeManager()
            .getDateTime(
                currentTime
            );

        HeatPumpController::HVACTime hvacTime;

        hvacTime.dayOfWeek =
            currentTime.tm_wday;

        hvacTime.hour =
            currentTime.tm_hour;

        hvacTime.minute =
            currentTime.tm_min;

        // ----------------------------------------------------
        // ZONE TEMPERATURES
        // ----------------------------------------------------

        static constexpr size_t
            MAX_ZONE_TEMPERATURES = 16;

        static float zoneTemperatures[
            MAX_ZONE_TEMPERATURES
        ] {};

        const size_t zoneCount =
            config.zoneCount <
                MAX_ZONE_TEMPERATURES
                    ? config.zoneCount
                    : MAX_ZONE_TEMPERATURES;

        Buffer& buffer =
            manager.getBuffer();

        for (
            size_t index = 0;
            index < zoneCount;
            ++index
        )
        {
            const int area =
                config
                    .zones[index]
                    .temperatureArea;

            zoneTemperatures[index] =
                area >= 0
                    ? static_cast<float>(
                        buffer.getValueFast(
                            area
                        )
                    ) / 10.0f
                    : 0.0f;
        }

        // ----------------------------------------------------
        // GLOBAL VALUES
        // ----------------------------------------------------

        const float indoorTemperature =
            config.readIndoorTemp
                ? config.readIndoorTemp()
                : 0.0f;

        const float outdoorTemperature =
            config.readOutdoorTemp
                ? config.readOutdoorTemp()
                : 0.0f;

        const bool windowOpen =
            config.readWindowOpen
                ? config.readWindowOpen()
                : false;

        // ----------------------------------------------------
        // ENGINE
        // ----------------------------------------------------

        HVACEngine::Loop(
            now,
            zoneTemperatures,
            indoorTemperature,
            outdoorTemperature,
            windowOpen,
            hvacTime
        );
    }

    // ========================================================
    // TASK: AVERAGES
    // ========================================================

    static void Task_Averages(
        DomoManager& manager,
        unsigned long now)
    {
        (void)now;

        const FrontendConfig& config =
            TaskEngineOrchestrator::getCfg();

        Buffer& buffer =
            manager.getBuffer();

        auto& averages =
            manager.getAverages();

        for (
            size_t groupIndex = 0;
            groupIndex <
                config.averages.gruppiCount;
            ++groupIndex
        )
        {
            const auto& group =
                config
                    .averages
                    .gruppi[groupIndex];

            for (
                size_t sensorIndex = 0;
                sensorIndex < group.count;
                ++sensorIndex
            )
            {
                const auto& sensor =
                    group.sensori[
                        sensorIndex
                    ];

                if (sensor.area < 0)
                    continue;

                const float rawValue =
                    static_cast<float>(
                        buffer.getValueFast(
                            sensor.area
                        )
                    );

                const float scaledValue =
                    rawValue *
                    sensor.factor;

                averages.addMeasurement(
                    group.nome,
                    scaledValue
                );
            }

            const float average =
                averages.groupAverage(
                    group.nome
                );

            if (group.areaOut < 0)
                continue;

            manager.forceInternalEvent(
                group.areaOut,
                static_cast<long>(
                    average *
                    group.outScale
                )
            );
        }
    }

    // ========================================================
    // TASK: SECURITY SENSORS
    // ========================================================

    static void Task_Sensors(
        DomoManager& manager,
        unsigned long now)
    {
        SecurityEngine::Loop(
            manager,
            now
        );
    }

public:
    // ========================================================
    // SETUP
    // ========================================================

    static void Setup(
        const FrontendConfig& config)
    {
        TaskEngineOrchestrator::Setup(
            config
        );

        // ----------------------------------------------------
        // SECURITY
        // ----------------------------------------------------

        TaskEngineOrchestrator::AddTask(
            Task_Sensors,
            normalizedInterval(
                config.security.intervalMs
            ),
            config.security.enabled
        );

        // ----------------------------------------------------
        // HVAC
        // ----------------------------------------------------

        TaskEngineOrchestrator::AddTask(
            Task_HVAC,
            normalizedInterval(
                config.hvac.intervalMs
            ),
            config.hvac.enabled
        );

        // ----------------------------------------------------
        // AVERAGES
        // ----------------------------------------------------

        TaskEngineOrchestrator::AddTask(
            Task_Averages,
            normalizedInterval(
                config.averages.intervalMs
            ),
            config.averages.enabled
        );

        LOG_IF(
            "TASK-ENGINE",
            "Task standard registrati: count=%u",
            static_cast<unsigned>(
                TaskEngineOrchestrator::
                    getTaskCount()
            )
        );
    }

    // ========================================================
    // ADD TASK
    //
    // Entry point per:
    // - task tecnici registrati dal composition root;
    // - task applicativi registrati da DMFrontend.
    // ========================================================

    static bool AddTask(
        TaskFn function,
        uint32_t intervalMs,
        bool enabled = true)
    {
        if (!function)
        {
            LOG_WF(
                "TASK-ENGINE",
                "AddTask ignorato: callback NULL"
            );

            return false;
        }

        return TaskEngineOrchestrator::AddTask(
            function,
            normalizedInterval(intervalMs),
            enabled
        );
    }

    // ========================================================
    // LOOP
    // ========================================================

    static void Loop(
        DomoManager& manager,
        unsigned long now)
    {
        TaskEngineOrchestrator::Loop(
            manager,
            now
        );
    }

    // ========================================================
    // FRONTEND CYCLE
    // ========================================================

    static bool hasFrontendCycleCompleted()
    {
        return
            TaskEngineOrchestrator::
                hasFrontendCycleCompleted();
    }

    static void resetFrontendCycleFlag()
    {
        TaskEngineOrchestrator::
            resetFrontendCycleFlag();
    }

    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    static uint16_t getTaskCount()
    {
        return
            TaskEngineOrchestrator::
                getTaskCount();
    }

    static uint16_t getMaxTasksPerCycle()
    {
        return
            TaskEngineOrchestrator::
                getMaxTasksPerCycle();
    }

private:
    // ========================================================
    // INTERVAL NORMALIZATION
    //
    // Un intervallo nullo renderebbe il task sempre pronto.
    // In assenza di una richiesta esplicita, normalizziamo a 1 ms.
    // ========================================================

    static uint32_t normalizedInterval(
        uint32_t intervalMs)
    {
        return
            intervalMs > 0
                ? intervalMs
                : 1UL;
    }
};

class FrontendOwnerMode
{
private:

    static const char* modeToString(
        OwnerManager::Mode mode)
    {
        switch (mode)
        {
            case OwnerManager::OWNER:
                return "OWNER";

            case OwnerManager::GUEST:
                return "GUEST";

            case OwnerManager::DEVELOPER:
                return "DEVELOPER";
        }

        return "UNKNOWN";
    }

public:

    static void OnChanged(
        OwnerManager::Mode oldMode,
        OwnerManager::Mode newMode,
        OwnerManager::Reason reason)
    {
        (void)reason;

        if (newMode == OwnerManager::DEVELOPER)
            LogManager::enable();
        else
            LogManager::disable();

        LOG_IF(
            "FrontendOwnerMode",
            "Callback OwnerModeChanged: %s -> %s",
            modeToString(oldMode),
            modeToString(newMode)
        );
    }
};

class FrontendRuntime
{
protected:
    unsigned long now = 0;
    bool fastServicesCycle = false;

    // ========================================================
    //  CORE REFERENCES
    // ========================================================

    DomoManager& manager;

    FrontendConfig config;


    // ========================================================
    //  USER BUTTON
    // ========================================================

    ButtonManager btnUSER;


    // ========================================================
    //  RUNTIME STATE
    // ========================================================
   
    bool devModeApplied = false;


    #if HOTSTANDBY_ENABLED

        // ========================================================
        //  HOTSTANDBY STATE
        // ========================================================

        bool lastMaster = false;

        bool isMaster = false;

    #endif


    // ========================================================
    //  CUSTOM FRONTEND HOOKS
    //
    //  Il frontend derivato può sovrascrivere questi metodi
    //  per eseguire operazioni specifiche.
    // ========================================================

    virtual void onStartupButton(
        unsigned long now)
    {
        (void)now;
    }


    virtual void onButtonPressed(
        unsigned long now)
    {
        (void)now;
    }


    virtual void onDeveloperMode()
    {
    }


    #if HOTSTANDBY_ENABLED

        virtual void onBecomeMaster()
        {
        }


        virtual void onBecomeSlave()
        {
        }

    #endif


    // ========================================================
    //  BUTTON PROCESSING
    // ========================================================

    void processButton(unsigned long now)
    {
        auto st =
            btnUSER.update(now);


        // ----------------------------------------------------
        // Pulsante premuto durante lo startup
        // ----------------------------------------------------

        if (st.pressedAtStartup)
        {
            LOG_IF(
                "BUTTON",
                "Pulsante tenuto premuto allo startup"
            );


            manager.getWatchDiag().onButtonPressed(
                manager,
                0,
                config.diagnostic
            );


            onStartupButton(now);

            return;
        }


        // ----------------------------------------------------
        // Pulsante premuto durante il normale loop
        // ----------------------------------------------------

        if (st.pressedNow)
        {
            LOG_IF(
                "BUTTON",
                "Pulsante premuto durante il loop"
            );


            auto& wd =
                manager.getWatchDiag();


            wd.onButtonPressed(
                manager,
                1,
                config.diagnostic
            );


            onButtonPressed(now);
        }
    }


    // ========================================================
    //  DEVELOPER MODE
    // ========================================================

    void processDeveloperMode()
    {
        if (devModeApplied)
            return;


        manager.getOwner().setDeveloper(
            OwnerManager::SYSTEM_REBOOT
        );


        devModeApplied = true;


        onDeveloperMode();
    }


    #if HOTSTANDBY_ENABLED

        // ========================================================
        //  HOTSTANDBY PROCESSING
        // ========================================================

        void processHotStandby()
        {
            const bool master =
                manager.isClusterMaster();


            isMaster = master;


            // ----------------------------------------------------
            // Nessuna transizione
            // ----------------------------------------------------

            if (master == lastMaster)
                return;


            // ----------------------------------------------------
            // SLAVE -> MASTER
            // ----------------------------------------------------

            if (master)
            {
                LOG_I(
                    "Main",
                    "Passo a MASTER"
                );


                onBecomeMaster();
            }


            // ----------------------------------------------------
            // MASTER -> SLAVE
            // ----------------------------------------------------

            else
            {
                LOG_I(
                    "Main",
                    "Passo a SLAVE"
                );


                onBecomeSlave();
            }


            lastMaster = master;
        }

    #endif

    void RegisterFrontendDiagnostics()
    {
        Diagnostic::frontendDiagnosticCallback() =
            [this]()
            {
                Serial.println(
                    "\n===== FRONTEND DIAGNOSTIC ====="
                );

                // ===== DIAGNOSTICA BASE =====

                if (config.ps.enabled)
                {
                    PowerSupervisor::Diagnostic::Report(
                        PowerSupervisorOrchestrator::Get()
                    );
                }

                if (config.hvac.enabled)
                {
                    HeatPumpController::Diagnostic::Report(
                        HVACEngine::GetHP()
                    );
                }

                if (config.security.enabled)
                {
                    // Diagnostica completa della centrale virtuale
                    DomoManagerAlarmPanel::instance().diagnostic();

                    // Diagnostica del motore sensori
                    SecurityOrchestrator::Diagnostic::FullReport();

                    // Stato aggregato
                    SecurityOrchestrator::getSystem().DiagnosticReport();
                }

                // ===== ESTENSIONE FRONTEND =====

                onFrontendDiagnostics();

                Serial.println(
                    "===== END FRONTEND DIAGNOSTIC =====\n"
                );
            };
    }

    virtual void onFrontendDiagnostics()
    {
    }

    void updateFastServices(unsigned long now)
    {
        // ====================================================
        // PULSE OUTPUTS
        // ====================================================

        PulseManager::Loop(
            now
        );

        // ====================================================
        // WEB API
        //
        // Il server HTTP deve essere servito con la massima
        // frequenza possibile, senza attendere il round-robin
        // Bridge/MQTT.
        // ====================================================

         /*
        * Client polling e server webhook sono indipendenti.
        *
        * Il servizio viene registrato se almeno uno dei due
        * è abilitato.
        */
        const bool webApiEnabled =
            config.webApi.client.enabled ||
            config.webApi.server.enabled;

        if (webApiEnabled)
        {

            if (WebAPIEngine::IsRunning())
            {
                WebAPIEngine::Loop(
                    now
                );
            }
        }
    }
public:

    // ========================================================
    //  CONSTRUCTOR
    // ========================================================

    FrontendRuntime(
        DomoManager& dm,
        const FrontendConfig& cfg)
        : manager(dm),
          config(cfg),
          btnUSER(cfg.pins.userButton)
    {
        btnUSER.begin();
    }


    // ========================================================
    //  DESTRUCTOR
    // ========================================================

    virtual ~FrontendRuntime() = default;


    // ========================================================
    //  UPDATE RUNTIME
    //
    //  Viene chiamato una volta per ogni ciclo del frontend.
    //
    //  Esegue:
    //    1. acquisizione now
    //    2. gestione pulsante
    //    3. Developer Mode
    //    4. HotStandby
    //
    //  Restituisce il timestamp acquisito.
    // ========================================================

    unsigned long updateRuntime()
    {
        now =
            manager.getTimeManager().nowMs();

        // ====================================================
        // FAST SERVICES
        //
        // Eseguiti un ciclo frontend sì e uno no.
        // ====================================================

        fastServicesCycle =
            !fastServicesCycle;

        if (fastServicesCycle)
        {
            updateFastServices(
                now
            );
        }
        else
        {
            processButton(now);
            processDeveloperMode();
        }

        #if HOTSTANDBY_ENABLED

                processHotStandby();

        #endif

        return now;
    }


    // ========================================================
    //  GET CURRENT TIME
    // ========================================================

    unsigned long getNow() const
    {
        return now;
    }


    // ========================================================
    //  GET MANAGER
    // ========================================================

    DomoManager& getManager()
    {
        return manager;
    }


    const DomoManager& getManager() const
    {
        return manager;
    }


    // ========================================================
    //  GET CONFIG
    // ========================================================

    FrontendConfig& getConfig()
    {
        return config;
    }


    const FrontendConfig& getConfig() const
    {
        return config;
    }


    #if HOTSTANDBY_ENABLED

        // ========================================================
        //  GET MASTER STATE
        // ========================================================

        bool getIsMaster() const
        {
            return isMaster;
        }

    #endif
};

// ============================================================
// INTROSPECTION ENGINE
//
// Costruisce e risolve semanticamente il modello Introspection.
//
// Non possiede:
// - DomoManager;
// - DomoIntrospection;
// - AEERegistry;
// - configurazione frontend.
//
// Tutte le dipendenze vengono ricevute dal composition root.
// ============================================================

class IntrospectionEngine
{
public:

    // ========================================================
    // BUILD RESULT
    // ========================================================

    struct Result
    {
        bool success =
            false;

        size_t deviceCount =
            0;

        size_t profilesFound =
            0;

        size_t profilesMissing =
            0;

        size_t profileAreaMismatches =
            0;

        size_t entityCount =
            0;
    };

private:

    // ========================================================
    // DEVICE PROFILE VALIDATION
    // ========================================================

    static void ValidateDeviceProfiles(
        const DomoManagerConfig::Devices& devices,
        const DeviceProfiles& deviceProfiles,
        Result& result)
    {
        result.deviceCount =
            devices.list.size();

        for (size_t deviceIndex = 0;
             deviceIndex < devices.list.size();
             ++deviceIndex)
        {
            const auto& device =
                devices.list[
                    deviceIndex
                ];

            const auto* profile =
                deviceProfiles.get(
                    device.profile.c_str()
                );

            if (!profile)
            {
                ++result.profilesMissing;

                LOG_WF(
                    "INTROSPECTION",
                    "Profile missing: "
                    "deviceIndex=%u device='%s' "
                    "profile='%s' areas=%u",
                    static_cast<unsigned>(
                        deviceIndex
                    ),
                    device.name.c_str(),
                    device.profile.c_str(),
                    static_cast<unsigned>(
                        device.areas.size()
                    )
                );

                continue;
            }

            ++result.profilesFound;

            size_t expandedItems =
                0;

            for (size_t channelIndex = 0;
                 channelIndex < profile->size();
                 ++channelIndex)
            {
                const int items =
                    (*profile)[
                        channelIndex
                    ].items;

                if (items > 0)
                {
                    expandedItems +=
                        static_cast<size_t>(
                            items
                        );
                }
            }

            if (expandedItems !=
                device.areas.size())
            {
                ++result.profileAreaMismatches;

                LOG_WF(
                    "INTROSPECTION",
                    "Profile/areas mismatch: "
                    "deviceIndex=%u device='%s' "
                    "profile='%s' expandedItems=%u areas=%u",
                    static_cast<unsigned>(
                        deviceIndex
                    ),
                    device.name.c_str(),
                    device.profile.c_str(),
                    static_cast<unsigned>(
                        expandedItems
                    ),
                    static_cast<unsigned>(
                        device.areas.size()
                    )
                );
            }
        }
    }

public:

    // ========================================================
    // BUILD
    // ========================================================

    static Result Build(
        DomoIntrospection& introspection,
        DomoManager& manager,
        const FrontendConfig& config,
        AEERegistry* aeeRegistry)
    {
        Result result;

        // ====================================================
        // DEVICE PROFILES
        //
        // Per ora conserva il comportamento attuale.
        // Tutti i profili configurati sono stati verificati
        // come disponibili in questa istanza.
        // ====================================================

        static DeviceProfiles
            deviceProfiles;

        ValidateDeviceProfiles(
            config
                .domoManager
                .devices,
            deviceProfiles,
            result
        );

        LOG_IF(
            "INTROSPECTION",
            "Profile check: "
            "devices=%u found=%u missing=%u mismatches=%u",
            static_cast<unsigned>(
                result.deviceCount
            ),
            static_cast<unsigned>(
                result.profilesFound
            ),
            static_cast<unsigned>(
                result.profilesMissing
            ),
            static_cast<unsigned>(
                result.profileAreaMismatches
            )
        );

        // ====================================================
        // CONTEXT
        // ====================================================

        DomoIntrospection::Context ctx;

        ctx.domo =
            &config.domoManager;

        ctx.deviceProfiles =
            &deviceProfiles;

        ctx.buffer =
            &manager.getBuffer();

        ctx.runtimeStore =
            &manager.getRuntimeStore();

        ctx.averages =
            &manager.getAverages();

        ctx.averagesConfig =
            &config.averages;

        ctx.time =
            &manager.getTimeManager();

        ctx.automationEngine =
            &manager.getAutomation();

        ctx.aeeRegistry =
            aeeRegistry;

        ctx.mqtt =
            &config.mqtt;

        ctx.webApi =
            &config.webApi;

        ctx.hvac =
            &config.hvac;

        ctx.security =
            &config.security;

        ctx.solar =
            config.solarInverters.count > 0
                ? &config
                    .solarInverters
                    .inverter[0]
                : nullptr;

        // ====================================================
        // REQUIRED CONTEXT VALIDATION
        // ====================================================

        if (!ctx.domo ||
            !ctx.deviceProfiles ||
            !ctx.buffer ||
            !ctx.runtimeStore)
        {
            LOG_E(
                "INTROSPECTION",
                "Build failed: invalid context"
            );

            return result;
        }

        if (result.profilesMissing > 0 ||
            result.profileAreaMismatches > 0)
        {
            LOG_WF(
                "INTROSPECTION",
                "Build continuing with profile errors: "
                "missing=%u mismatches=%u",
                static_cast<unsigned>(
                    result.profilesMissing
                ),
                static_cast<unsigned>(
                    result.profileAreaMismatches
                )
            );
        }

        // ====================================================
        // STRUCTURAL BUILD
        // ====================================================

        introspection.build(
            ctx
        );

        result.entityCount =
            introspection.entityCount();

        LOG_IF(
            "INTROSPECTION",
            "Structural build completed: entities=%u",
            static_cast<unsigned>(
                result.entityCount
            )
        );

        // ====================================================
        // SEMANTIC RESOLUTION
        //
        // Deve avvenire prima che Home Assistant utilizzi
        // l'istanza Introspection.
        // ====================================================

        DomoSemanticResolver
            semanticResolver;

        const auto semanticResult =
            semanticResolver.resolve(
                introspection
            );

        (void)semanticResult;

        LOG_IF(
            "INTROSPECTION",
            "Semantic resolution completed: entities=%u",
            static_cast<unsigned>(
                introspection.entityCount()
            )
        );

        result.success =
            introspection.entityCount() > 0;

        return result;
    }
};

#endif
