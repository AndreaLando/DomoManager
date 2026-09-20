
#ifndef DMFrontendOrchestrators_HPP
#define DMFrontendOrchestrators_HPP

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

#include "DMSetup.hpp"
#include "DMDeclares.h"
#include "DMIntrospection.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================
// DOMO MQTT CONFIG BUILDER
// ============================================================
//
// Costruisce la struttura runtime utilizzata esclusivamente
// dal frontend Home Assistant.
//
// NON costruisce:
//     FrontendConfig::MQTT
//     Client
//     Device
//     Mapping
//
// Il client MQTT resta quello definito staticamente in:
//     FrontendConfig::MQTT::Client
//
// Il Builder produce invece un registry di entity HA Discovery.
//
// Viene eseguito una sola volta durante il setup, dopo:
//     DomoIntrospection
//     DomoSemanticResolver
//
// Non usa:
//     std::vector
//     new
//     delete
//
// Tutto lo storage è statico.
//
// ============================================================

class DomoMQTTConfigBuilder
{
public:

    // ========================================================
    // LIMITI
    // ========================================================

    static constexpr size_t MAX_ENTITIES = 128;


    // ========================================================
    // DISCOVERY ENTITY
    // ========================================================

    struct HADiscovery
    {
        // ----------------------------------------------------
        // DOMO
        // ----------------------------------------------------

        int area = -1;

        DomoIntrospection::EntityKind kind =
            DomoIntrospection::EntityKind::UNKNOWN;


        // ----------------------------------------------------
        // DEVICE
        // ----------------------------------------------------

        const char* deviceId = nullptr;

        const char* deviceName = nullptr;


        // ----------------------------------------------------
        // ENTITY
        // ----------------------------------------------------

        const char* name = nullptr;

        const char* uniqueId = nullptr;

        const char* field = nullptr;


        // ----------------------------------------------------
        // HOME ASSISTANT
        // ----------------------------------------------------

        const char* component = nullptr;

        const char* unit = nullptr;

        const char* deviceClass = nullptr;

        const char* stateClass = nullptr;


        // ----------------------------------------------------
        // ACCESS
        // ----------------------------------------------------

        bool readable = false;

        bool writable = false;


        // ----------------------------------------------------
        // SCALE
        // ----------------------------------------------------

        float scale = 1.0f;


        // ----------------------------------------------------
        // VALID
        // ----------------------------------------------------

        bool valid = false;
    };


private:

    // ========================================================
    // STORAGE
    // ========================================================

    struct Storage
    {
        HADiscovery entities[
            MAX_ENTITIES
        ];

        char deviceIds[
            MAX_ENTITIES
        ][
            32
        ];

        char deviceNames[
            MAX_ENTITIES
        ][
            64
        ];

        char names[
            MAX_ENTITIES
        ][
            64
        ];

        char uniqueIds[
            MAX_ENTITIES
        ][
            96
        ];

        char fields[
            MAX_ENTITIES
        ][
            64
        ];

        size_t count = 0;

        bool initialized = false;
    };


    // ========================================================
    // STATIC STORAGE
    // ========================================================

    static Storage& getStorage()
    {
        static Storage storage{};
        return storage;
    }


public:

    // ========================================================
    // BUILD
    // ========================================================

    static bool Build(
        const DomoIntrospection& info
    )
    {
        Storage& s =
            getStorage();


        // ====================================================
        // RESET
        // ====================================================

        Reset();


        // ====================================================
        // ENTITY SCAN
        // ====================================================

        for (
            size_t i = 0;
            i < info.entityCount();
            ++i
        )
        {
            const DomoIntrospection::Entity* entity =
                info.getEntity(i);


            if (!entity)
                continue;


            // ------------------------------------------------
            // IMPLEMENTATION ONLY
            // ------------------------------------------------

            if (entity->implementationOnly)
                continue;


            // ------------------------------------------------
            // UNKNOWN
            // ------------------------------------------------

            if (
                entity->kind ==
                DomoIntrospection::EntityKind::UNKNOWN
            )
            {
                continue;
            }


            // ------------------------------------------------
            // CAPACITY
            // ------------------------------------------------

            if (
                s.count >= MAX_ENTITIES
            )
            {
                LOG_WF(
                    "MQTT",
                    "HA Builder: "
                    "MAX_ENTITIES=%u raggiunto",
                    (unsigned)MAX_ENTITIES
                );

                break;
            }


            // ------------------------------------------------
            // ADD
            // ------------------------------------------------

            AddEntity(
                *entity,
                info
            );
        }


        s.initialized =
            true;


        // ====================================================
        // DIAGNOSTICS
        // ====================================================

        LOG_IF(
            "MQTT",
            "HA Discovery Builder completato: "
            "entities=%u",
            (unsigned)s.count
        );


        Dump();


        return true;
    }


    // ========================================================
    // RESET
    // ========================================================

    static void Reset()
    {
        Storage& s =
            getStorage();


        s.count =
            0;

        s.initialized =
            false;


        for (
            size_t i = 0;
            i < MAX_ENTITIES;
            ++i
        )
        {
            s.entities[i] =
                HADiscovery{};


            s.deviceIds[i][0] =
                '\0';

            s.deviceNames[i][0] =
                '\0';

            s.names[i][0] =
                '\0';

            s.uniqueIds[i][0] =
                '\0';

            s.fields[i][0] =
                '\0';
        }
    }


    // ========================================================
    // STATUS
    // ========================================================

    static bool IsInitialized()
    {
        return getStorage().initialized;
    }


    // ========================================================
    // COUNT
    // ========================================================

    static size_t getCount()
    {
        return getStorage().count;
    }


    // ========================================================
    // ENTITY
    // ========================================================

    static const HADiscovery* get(
        size_t index
    )
    {
        const Storage& s =
            getStorage();


        if (index >= s.count)
            return nullptr;


        return &s.entities[index];
    }


private:

    // ========================================================
    // ADD ENTITY
    // ========================================================

    static void AddEntity(
        const DomoIntrospection::Entity& entity,
        const DomoIntrospection& info
    )
    {
        Storage& s =
            getStorage();


        const size_t index =
            s.count;


        HADiscovery& out =
            s.entities[index];


        // ====================================================
        // AREA
        // ====================================================

        out.area =
            entity.area;


        // ====================================================
        // KIND
        // ====================================================

        out.kind =
            entity.kind;


        // ====================================================
        // ACCESS
        // ====================================================

        out.readable =
            entity.readable;

        out.writable =
            entity.writable;


        // ====================================================
        // SCALE
        // ====================================================

        out.scale =
            entity.scale;


        // ====================================================
        // DEVICE
        // ====================================================

        BuildDevice(
            index,
            entity,
            info
        );


        out.deviceId =
            s.deviceIds[index];

        out.deviceName =
            s.deviceNames[index];


        // ====================================================
        // FIELD
        // ====================================================

        BuildField(
            index,
            entity,
            info
        );


        out.field =
            s.fields[index];


        // ====================================================
        // NAME
        // ====================================================

        BuildName(
            index,
            entity,
            info
        );


        out.name =
            s.names[index];


        // ====================================================
        // UNIQUE ID
        // ====================================================

        snprintf(
            s.uniqueIds[index],
            sizeof(
                s.uniqueIds[index]
            ),
            "domo_%d_%s",
            entity.area,
            s.fields[index]
        );


        out.uniqueId =
            s.uniqueIds[index];


        // ====================================================
        // HA COMPONENT
        // ====================================================

        out.component =
            GetComponent(
                entity.kind
            );


        // ====================================================
        // METADATA
        // ====================================================

        out.unit =
            entity.unit;

        out.deviceClass =
            entity.deviceClass;

        out.stateClass =
            entity.stateClass;


        // ====================================================
        // VALID
        // ====================================================

        out.valid =
            (
                out.component != nullptr &&
                out.field != nullptr &&
                out.field[0] != '\0'
            );


        if (!out.valid)
        {
            LOG_WF(
                "MQTT",
                "HA Builder: "
                "entity area=%d non valida",
                entity.area
            );

            return;
        }


        ++s.count;
    }


    // ========================================================
    // DEVICE
    // ========================================================

    static void BuildDevice(
        size_t index,
        const DomoIntrospection::Entity& entity,
        const DomoIntrospection& info
    )
    {
        Storage& s =
            getStorage();


        // ----------------------------------------------------
        // DEVICE ID
        // ----------------------------------------------------

        if (entity.deviceIndex >= 0)
        {
            snprintf(
                s.deviceIds[index],
                sizeof(
                    s.deviceIds[index]
                ),
                "device_%d",
                entity.deviceIndex
            );
        }
        else
        {
            snprintf(
                s.deviceIds[index],
                sizeof(
                    s.deviceIds[index]
                ),
                "domo"
            );
        }


        // ----------------------------------------------------
        // DEVICE NAME
        // ----------------------------------------------------

        const char* areaName =
            info.getAreaName(
                entity.area
            );


        if (areaName &&
            *areaName)
        {
            snprintf(
                s.deviceNames[index],
                sizeof(
                    s.deviceNames[index]
                ),
                "%s",
                areaName
            );
        }
        else if (
            entity.deviceIndex >= 0
        )
        {
            snprintf(
                s.deviceNames[index],
                sizeof(
                    s.deviceNames[index]
                ),
                "Device %d",
                entity.deviceIndex
            );
        }
        else
        {
            snprintf(
                s.deviceNames[index],
                sizeof(
                    s.deviceNames[index]
                ),
                "Domo"
            );
        }
    }


    // ========================================================
    // FIELD
    // ========================================================

    static void BuildField(
        size_t index,
        const DomoIntrospection::Entity& entity,
        const DomoIntrospection& info
    )
    {
        Storage& s =
            getStorage();


        const char* areaName =
            info.getAreaName(
                entity.area
            );


        if (
            areaName &&
            *areaName
        )
        {
            MakeSlug(
                areaName,
                s.fields[index],
                sizeof(
                    s.fields[index]
                )
            );
        }


        if (!s.fields[index][0])
        {
            snprintf(
                s.fields[index],
                sizeof(
                    s.fields[index]
                ),
                "area_%d",
                entity.area
            );
        }
    }


    // ========================================================
    // NAME
    // ========================================================

    static void BuildName(
        size_t index,
        const DomoIntrospection::Entity& entity,
        const DomoIntrospection& info
    )
    {
        Storage& s =
            getStorage();


        const char* areaName =
            info.getAreaName(
                entity.area
            );


        if (
            areaName &&
            *areaName
        )
        {
            snprintf(
                s.names[index],
                sizeof(
                    s.names[index]
                ),
                "%s",
                areaName
            );

            return;
        }


        snprintf(
            s.names[index],
            sizeof(
                s.names[index]
            ),
            "Area %d",
            entity.area
        );
    }


    // ========================================================
    // HA COMPONENT
    // ========================================================

    static const char* GetComponent(
        DomoIntrospection::EntityKind kind
    )
    {
        using EK =
            DomoIntrospection::EntityKind;


        switch (kind)
        {
            case EK::SENSOR:
                return "sensor";


            case EK::BINARY_SENSOR:
                return "binary_sensor";


            case EK::SWITCH:
                return "switch";


            case EK::LIGHT:
                return "light";


            case EK::COVER:
                return "cover";


            case EK::CLIMATE:
                return "climate";


            case EK::BUTTON:
                return "button";


            case EK::NUMBER:
                return "number";


            case EK::STATUS:
                return "sensor";


            default:
                return nullptr;
        }
    }


    // ========================================================
    // SLUG
    // ========================================================

    static void MakeSlug(
        const char* input,
        char* output,
        size_t outputSize
    )
    {
        if (!output ||
            outputSize == 0)
        {
            return;
        }


        output[0] =
            '\0';


        if (!input)
            return;


        size_t pos =
            0;


        bool lastUnderscore =
            false;


        while (
            *input &&
            pos + 1 < outputSize
        )
        {
            unsigned char c =
                (unsigned char)*input++;


            if (
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9')
            )
            {
                if (
                    c >= 'A' &&
                    c <= 'Z'
                )
                {
                    c =
                        (unsigned char)(
                            c -
                            'A' +
                            'a'
                        );
                }


                output[pos++] =
                    (char)c;


                lastUnderscore =
                    false;
            }
            else
            {
                if (
                    pos > 0 &&
                    !lastUnderscore
                )
                {
                    output[pos++] =
                        '_';

                    lastUnderscore =
                        true;
                }
            }
        }


        while (
            pos > 0 &&
            output[pos - 1] == '_'
        )
        {
            --pos;
        }


        output[pos] =
            '\0';
    }


    // ========================================================
    // DUMP
    // ========================================================

    static void Dump()
    {
        const Storage& s =
            getStorage();


        for (
            size_t i = 0;
            i < s.count;
            ++i
        )
        {
            const HADiscovery& e =
                s.entities[i];


            LOG_IF(
                "MQTT",
                "HA DISCOVERY[%u] "
                "area=%d "
                "kind=%u "
                "component=%s "
                "device=%s "
                "name=%s "
                "field=%s "
                "unique_id=%s "
                "readable=%u "
                "writable=%u "
                "unit=%s",

                (unsigned)i,

                e.area,

                (unsigned)e.kind,

                e.component
                    ? e.component
                    : "?",

                e.deviceId
                    ? e.deviceId
                    : "?",

                e.name
                    ? e.name
                    : "?",

                e.field
                    ? e.field
                    : "?",

                e.uniqueId
                    ? e.uniqueId
                    : "?",

                e.readable
                    ? 1
                    : 0,

                e.writable
                    ? 1
                    : 0,

                e.unit
                    ? e.unit
                    : ""
            );
        }
    }
};


class PowerSupervisorOrchestrator {
private:
    // ------------------------------------------------------------
    //  CONFIGURAZIONE DINAMICA (copiata dal frontend)
    // ------------------------------------------------------------
    static inline FrontendConfig::PowerSupervisor cfg;

    // ------------------------------------------------------------
    //  LETTURE SENSORI BASE (motore puro)
    // ------------------------------------------------------------
    static PowerSupervisor::PowerValue readMainPower() {
        auto& buf = DomoManager::instance->getBuffer();
        float v = GenericSensor::read(cfg.mainPower, buf);
        if (v <= 0)
            return {0.0f, false};
        return {v, true};
    }

    static bool readI24vOk() {
        auto& buf = DomoManager::instance->getBuffer();
        return GenericSensor::read(cfg.i24vOk, buf) != 0;
    }

    static bool readFault() {
        auto& buf = DomoManager::instance->getBuffer();
        return GenericSensor::read(cfg.fault, buf) != 0;
    }

    static bool readBattery() {
        auto& buf = DomoManager::instance->getBuffer();
        return GenericSensor::read(cfg.battery, buf) != 0;
    }

    // ------------------------------------------------------------
    //  ISTANZA SUPERVISORE
    // ------------------------------------------------------------
    static inline PowerSupervisor* instance = nullptr;

public:
    // Callback definita dal frontend
    using AlarmCallback = void (*)(const String&, long);

    // ------------------------------------------------------------
    //  SETUP (motore puro)
    // ------------------------------------------------------------
    static void Setup(const FrontendConfig::PowerSupervisor& cfgIn,
                      AlarmCallback cb)
    {
        cfg = cfgIn;

        if (instance)
            delete instance;

        // Costruzione configurazione base del supervisore
        PowerSupervisor::Config psCfg = {
            readMainPower,
            readI24vOk,
            readFault,
            readBattery
        };

        psCfg.mainPowerLow  = cfg.mainPowerLow;
        psCfg.mainPowerHigh = cfg.mainPowerHigh;

        instance = new PowerSupervisor(psCfg);

        // La callback viene dal frontend
        instance->onAlarm(cb);

        instance->setup();

        LOG_IF("PowerSupervisorEngine", "PowerSupervisor initialized");
    }

    // ------------------------------------------------------------
    //  LOOP
    // ------------------------------------------------------------
    static void Loop(unsigned long now) {
        if (instance)
            instance->loop(now);
    }

    // ------------------------------------------------------------
    //  ACCESSOR
    // ------------------------------------------------------------
    static PowerSupervisor& Get() {
        return *instance;
    }
};


class WeatherOrchestrator {
private:
    static inline WeatherStation* instance = nullptr;

public:
    using EventCallback = void (*)(WeatherEvent);
    using AlarmCallback = void (*)(const WeatherAlarm*, int);

    static void Setup(const FrontendConfig::Weather& cfg,
                      EventCallback evCb,
                      AlarmCallback alCb)
    {
        if (instance)
            delete instance;

        instance = new WeatherStation(cfg.config);

        instance->setEventCallback(evCb);
        instance->setAlarmCallback(alCb);

        LOG_IF("METEO", "Weather initialized (dynamic config)");
    }

    static void Loop(unsigned long now) {
        if (instance)
            instance->update();
    }

    static WeatherStation& Get() {
        return *instance;
    }
};


class PowerOrchestrator {
private:
    static inline PowerManager* instance = nullptr;

    // Costruisce o restituisce l’istanza del PowerManager
    static PowerManager& pm(int fallbackSoft = 3000,
                            const FrontendConfig::Power* cfg = nullptr)
    {
        if (!instance) {
            BatteryManager::Config batteryCfg{};   // nessuna batteria configurata dal frontend

            float gridLimit = cfg ? cfg->limitSoft
                                  : static_cast<float>(fallbackSoft);

            instance = new PowerManager(gridLimit, batteryCfg);
        }
        return *instance;
    }

    static inline unsigned long lastTune = 0;
public:
    // Tipi callback forniti dal frontend
    using LoadChangeCb = void (*)(const String&, bool);
    using WarningCb    = void (*)(float, float);
    using ExceededCb   = void (*)(float, float);
    using ErrorCb      = void (*)(int, const String&);
    using SuggestionCb = void (*)(const String&, int, const String&);

    // ============================================================
    // SETUP (motore puro, backend modulare)
    // ============================================================
static void Setup(const FrontendConfig::Power& cfg,
                  int fallbackSoft,
                  int /*fallbackHard*/,
                  LoadChangeCb onLoad,
                  WarningCb onWarn,
                  ExceededCb onExceeded,
                  ErrorCb onErr,
                  SuggestionCb onSug)
{
    // 1) Istanzia il PowerManager con il limite soft (o fallback)
    auto& p = pm(fallbackSoft);

    p.limitManager().setHardLimit(cfg.limitHard);
    p.limitManager().setHardLimitCallback(
        [&](float net, float limit){
            auto& lm = p.loadManager();

            for (auto& l : lm.getLoadsMutable()) {
                if (l.state && (millis() - l.lastChange) >= l.minOnMs) {
                    l.suggestedOff = true;
                    l.suggestedOn  = false;

                    p.fireSuggestion("stacca:" + l.name, 3, "hard limit");
                }
            }
        }
    );


    // 2) Configurazione interna del PowerManager (autoTune, intervalli, ecc.)
    PowerManager::Config pcfg;
    pcfg.autoTune       = cfg.autoTune;
    pcfg.tuneIntervalMs = cfg.tuneIntervalMs;
    p.setConfig(pcfg);

    // 3) CALLBACK DAL FRONTEND
    p.setOnLoadChange(onLoad);
    p.setOnLimitWarning(onWarn);
    p.setOnLimitExceeded(onExceeded);
    p.setOnError(onErr);
    p.setOnSuggestion(onSug);

    // 4) CARICHI DINAMICI
    auto& lm = p.loadManager();

    for (size_t i = 0; i < cfg.loadCount; ++i) {
        const auto& L = cfg.loads[i];

        lm.addLoad(
            L.name,
            L.watt,
            L.minOn,
            L.minOff
        );

        // PRIORITÀ (solo per carichi normali)
        lm.getLoadsMutable().back().priority = L.priority;
    }

    // 5) CARICO TERMICO
    const auto& T = cfg.thermal;

    if (T.enabled) {
        lm.addThermalLoad(
            T.name,
            true,        // heating mode
            T.setpoint,  // baseTarget
            T.min,       // comfortMin
            T.max,       // comfortMax
            T.minOn,
            T.minOff
        );
    }

    LOG_IF("POWER", "PowerOrchestrator initialized (modular PowerManager)");
}

    // ============================================================
    // LOOP (motore puro, backend modulare)
    // ============================================================

    static void Loop(unsigned long now,
                    int gridPower,
                    float lux,
                    float tempExt,
                    float actualProduction,
                    float meanTemperature,
                    int month,
                    int hour,
                    int minute)
    {
        auto& p = pm();
        const auto& cfg = p.getConfig();

        p.setGridPower(gridPower);

        auto& pv = p.pvManager();
        pv.updateLux(lux);
        pv.setTempExt(tempExt);
        pv.setStringPower(0, actualProduction);

        float forecast = pv.getForecast(month, hour, minute);
        pv.updateForecastError(forecast, actualProduction);

        auto& bat = p.batteryManager();
        float batteryPower = bat.update(gridPower, forecast, now);

        float netWithBattery = gridPower - batteryPower;

        auto& lm = p.loadManager();
        auto& at = p.autoTuneManager();

        if (cfg.autoTune) {
            if (now - lastTune >= cfg.tuneIntervalMs) {

                at.update(
                    pv.getLuxVolatility(),
                    pv.getForecastError(),
                    lm.getLoadsMutable(),
                    bat
                );

                lm.setHysteresis(at.getHystOn(), at.getHystOff());
                lastTune = now;
            }
        }

        lm.updateThermal(
            meanTemperature,
            forecast,
            now
        );

        float dynamicLimit = p.limitManager().getLimit();

        lm.updateLoads(
            netWithBattery,
            dynamicLimit,
            now
        );

        p.limitManager().check(
            netWithBattery,
            dynamicLimit
        );

        p.setSolarPower(pv.getTotalPower());
        p.setBatteryPower(batteryPower);
        p.setForecast(forecast);
    }


    // ============================================================
    // ACCESSOR
    // ============================================================
    static PowerManager& Get() {
        return pm();
    }
};


// ============================================================
//  TASK ENGINE (BACKEND / ORCHESTRATOR)
// ============================================================

class TaskEngineOrchestrator
{
public:

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================

    using TaskFn = void (*)(DomoManager&, unsigned long);

    static constexpr uint16_t STORAGE_MAX_TASKS = 32;

    // Limite dinamico:
    //
    //   1..2 task  -> 1 task/ciclo
    //   3..4       -> 2
    //   5..6       -> 3
    //   7..8       -> 4
    //   >8        -> massimo 4
    //
    // In questo modo la crescita dei task non porta ad una
    // crescita indiscriminata del lavoro eseguito nello stesso
    // passaggio.
    static constexpr uint16_t MIN_TASKS_PER_CYCLE = 1;
    static constexpr uint16_t MAX_TASKS_PER_CYCLE = 3;

    struct Task
    {
        TaskFn   fn       = nullptr;
        uint32_t interval = 0;
        uint32_t lastRun  = 0;
        bool     enabled  = false;
    };

private:

    static inline bool frontendCycleDone = false;

    /*
     * Mantengo il vector perché il tuo codice attuale lo usa già.
     *
     * IMPORTANTE:
     * - nessuna push_back durante Loop()
     * - AddTask() viene usato in Setup()
     * - il Loop() non alloca memoria
     */
    static inline std::vector<Task> tasks;

    static inline const FrontendConfig* cfg = nullptr;

    // Tie-break rotazionale tra task con la stessa urgenza.
    static inline uint16_t rrCursor = 0;

public:

    // ============================================================
    // SETUP
    // ============================================================

    static void Setup(const FrontendConfig& c)
    {
        cfg = &c;

        Clear();

        // Evita riallocazioni durante AddTask().
        if (tasks.capacity() < STORAGE_MAX_TASKS)
            tasks.reserve(STORAGE_MAX_TASKS);
    }

    // ============================================================
    // ADD TASK
    // ============================================================

    static void AddTask(
        TaskFn fn,
        uint32_t intervalMs,
        bool enabled)
    {
        if (!fn)
        {
            LOG_WF(
                "TASK-LOOP",
                "AddTask: callback nullo"
            );

            return;
        }

        if (tasks.size() >= STORAGE_MAX_TASKS)
        {
            LOG_WF(
                "TASK-LOOP",
                "Numero massimo task superato: %u/%u",
                static_cast<unsigned>(tasks.size()),
                static_cast<unsigned>(STORAGE_MAX_TASKS)
            );

            return;
        }

        tasks.push_back({
            fn,
            intervalMs,
            0,
            enabled
        });
    }

    // ============================================================
    // CLEAR
    // ============================================================

    static void Clear()
    {
        tasks.clear();

        frontendCycleDone = false;
        rrCursor = 0;
    }

    // ============================================================
    // LOOP
    // ============================================================

    static void Loop(
        DomoManager& manager,
        unsigned long now)
    {
        frontendCycleDone = false;

        const uint16_t count =
            static_cast<uint16_t>(tasks.size());

        if (count == 0)
        {
            frontendCycleDone = true;
            return;
        }

        /*
         * Numero massimo di task da eseguire in questo passaggio.
         *
         * Nessuna funzione costosa: semplice aritmetica.
         */
        uint16_t maxTasks =
            static_cast<uint16_t>((count + 1u) >> 1);

        if (maxTasks < MIN_TASKS_PER_CYCLE)
            maxTasks = MIN_TASKS_PER_CYCLE;

        if (maxTasks > MAX_TASKS_PER_CYCLE)
            maxTasks = MAX_TASKS_PER_CYCLE;

        if (maxTasks > count)
            maxTasks = count;

        uint16_t executed = 0;

        /*
         * Per evitare di rieseguire lo stesso task nel medesimo
         * Loop(), manteniamo una piccola bitmap locale.
         *
         * 32 task massimi -> uint32_t.
         *
         * Nessuna allocazione.
         */
        uint32_t selectedMask = 0;

        // ========================================================
        // SELEZIONE TASK PIÙ URGENTI
        // ========================================================

        while (executed < maxTasks)
        {
            int bestIndex = -1;
            uint32_t bestLateness = 0;
            uint16_t bestDistance = UINT16_MAX;

            /*
             * Scansione completa dei task.
             *
             * Il costo massimo è:
             *
             *   MAX_TASKS_PER_CYCLE * taskCount
             *
             * Con 4 * 8 = 32 confronti nel tuo caso.
             * È trascurabile rispetto al lavoro dei task reali.
             */
            for (uint16_t i = 0; i < count; ++i)
            {
                // Già selezionato in questo passaggio.
                if (selectedMask & (1UL << i))
                    continue;

                const Task& t = tasks[i];

                if (!t.enabled || !t.fn)
                    continue;

                /*
                 * unsigned subtraction:
                 * corretta anche in caso di rollover di millis().
                 */
                const uint32_t elapsed =
                    static_cast<uint32_t>(now - t.lastRun);

                /*
                 * Task non ancora scaduto.
                 */
                if (elapsed < t.interval)
                    continue;

                /*
                 * Urgenza = quanto siamo oltre la scadenza.
                 *
                 * interval 0:
                 * task sempre pronto.
                 */
                const uint32_t lateness =
                    elapsed - t.interval;

                /*
                 * A parità di lateness usiamo il cursore
                 * rotazionale.
                 */
                uint16_t distance;

                if (i >= rrCursor)
                    distance = i - rrCursor;
                else
                    distance =
                        static_cast<uint16_t>(
                            count - rrCursor + i
                        );

                if (
                    bestIndex < 0 ||
                    lateness > bestLateness ||
                    (
                        lateness == bestLateness &&
                        distance < bestDistance
                    )
                )
                {
                    bestIndex = i;
                    bestLateness = lateness;
                    bestDistance = distance;
                }
            }

            // Nessun task pronto.
            if (bestIndex < 0)
                break;

            Task& task = tasks[bestIndex];

            /*
             * IMPORTANTISSIMO:
             *
             * aggiorniamo lastRun PRIMA della callback.
             *
             * Così se il task genera un evento che porta
             * nuovamente dentro il sistema, non viene considerato
             * immediatamente nuovamente scaduto.
             */
            task.lastRun = static_cast<uint32_t>(now);

            selectedMask |=
                (1UL << bestIndex);

            ++executed;

            /*
             * Esecuzione vera.
             */
            task.fn(manager, now);

            /*
             * Il prossimo tie-break parte dal task successivo.
             *
             * Questo evita starvation in caso di pari priorità/
             * pari lateness.
             */
            rrCursor =
                static_cast<uint16_t>(
                    bestIndex + 1
                );

            if (rrCursor >= count)
                rrCursor = 0;
        }

        frontendCycleDone = true;
    }

    // ============================================================
    // DIAGNOSTICA / ACCESSO
    // ============================================================

    static bool hasFrontendCycleCompleted()
    {
        return frontendCycleDone;
    }

    static void resetFrontendCycleFlag()
    {
        frontendCycleDone = false;
    }

    static const FrontendConfig& getCfg()
    {
        return *cfg;
    }

    static uint16_t getTaskCount()
    {
        return static_cast<uint16_t>(tasks.size());
    }

    static uint16_t getMaxTasksPerCycle()
    {
        const uint16_t count =
            static_cast<uint16_t>(tasks.size());

        if (count == 0)
            return 0;

        uint16_t result =
            static_cast<uint16_t>((count + 1u) >> 1);

        if (result < MIN_TASKS_PER_CYCLE)
            result = MIN_TASKS_PER_CYCLE;

        if (result > MAX_TASKS_PER_CYCLE)
            result = MAX_TASKS_PER_CYCLE;

        if (result > count)
            result = count;

        return result;
    }
};

#endif
