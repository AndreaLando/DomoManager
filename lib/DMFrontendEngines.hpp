
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

#include "DMSetup.hpp"
#include "DMDeclares.h"
#include "DMWiredSensors.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


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

class AEEEngine {
public:
    AEERegistry registry;
    AEEManagement* manager = nullptr;

    static AEEEngine& instance() {
        static AEEEngine inst;   // 🔥 definizione interna, nessun undefined reference
        return inst;
    }

    void Setup(const FrontendConfig::Bridge::AEE& cfg) {
        manager = new AEEManagement(cfg.vars, cfg.count);
        manager->registerAll(registry);
    }

    static AEERegistry& getAEE() {
        return instance().registry;
    }

    static AEEManagement& getMgr() {
        return *instance().manager;
    }
};

class MQTTEngine {
public:
    class MQTTAEEBridge {
    public:
        struct AEEVar_MQTT {
            bool enabled = false;
            const char* mqttName = nullptr;
            const char* mqttUnit = nullptr;
            MQTTEngine_Var::HAType mqttType = MQTTEngine_Var::HAType::SENSOR;
        };

        struct AEEToMQTTMapEntry {
            const char* aeeName;   // nome variabile AEE
            AEEVar_MQTT mqtt;      // metadati MQTT
        };
    private:
        static inline std::vector<MQTTEngine_Var> vars;
        static inline MQTT* engine = nullptr;

    public:
        static void Setup(EthernetClient& eth,
                  PubSubClient& mqtt,
                  const AEEToMQTTMapEntry* map,
                  size_t mapCount,
                  const char* nodeId,
                  const IPAddress& broker,
                  uint16_t port)
    {
        vars.clear();

        // Scorri tutte le variabili AEE tramite AEEEngine
        AEEEngine::getAEE().forEach([&](AEEVariableBase* base){
            for (size_t i = 0; i < mapCount; i++) {
                if (strcmp(base->def.name, map[i].aeeName) != 0)
                    continue;

                if (!map[i].mqtt.enabled)
                    continue;

                MQTTEngine_Var v = {};
                v.id   = base->def.name;
                v.name = map[i].mqtt.mqttName ? map[i].mqtt.mqttName : base->def.name;
                v.unit = map[i].mqtt.mqttUnit;
                v.type = map[i].mqtt.mqttType;

                // Collega puntatore al valore AEE
                switch (base->def.varType) {
                    case AEEVarType::FLOAT:
                        v.analogPtr = const_cast<float*>(&as<float>(base)->get());
                        break;

                    case AEEVarType::BOOL:
                        v.boolPtr   = const_cast<bool*>(&as<bool>(base)->get());
                        break;

                    case AEEVarType::INT:
                        v.coverPos = const_cast<uint16_t*>(&as<uint16_t>(base)->get());
                        break;

                    default:
                        break;
                }

                vars.push_back(v);
            }
        });

        engine = new MQTT(eth, mqtt, vars.data(), vars.size(), nodeId);
        engine->setBackend(MQTT::Backend::HOME_ASSISTANT);
        engine->begin(broker, port);

        LOG_IF("MQTT", "MQTT AEE Bridge inizializzato con %u variabili", vars.size());
    }


        static void Loop(unsigned long now) {
            if (engine) engine->loop(now);
        }
    };
private:
    // --- Networking ---
    static inline PubSubClient mqttClient;

    // --- Istanza engine ---
    static inline MQTT* instance = nullptr;

    // --- Config mapping buffer → variabili MQTT ---
    struct RuntimeVar {
        FrontendConfig::MQTT::Var cfg;  // copia della config
        // storage per il valore esposto a MQTTEngine
        float    fVal  = 0.0f;
        bool     bVal  = false;
        uint16_t u16Val = 0;
    };

    static inline std::vector<RuntimeVar> runtimeVars;

public:

    static void Setup(DomoManager& dm,
                      EthernetClient& ethClient,
                      const FrontendConfig::MQTT& cfg)  {
        if (!cfg.enabled || !cfg.vars || cfg.varCount == 0) {
            LOG_IF("MQTT", "MQTT disabilitato o nessuna variabile configurata");
            return;
        }

        mqttClient.setClient(ethClient);
        mqttClient.setServer(cfg.broker, cfg.port);

        // Costruisci runtimeVars a partire dalla config
        runtimeVars.clear();
        runtimeVars.reserve(cfg.varCount);

        for (size_t i = 0; i < cfg.varCount; ++i) {
            RuntimeVar rv;
            rv.cfg = cfg.vars[i];
            runtimeVars.push_back(rv);
        }

        // Costruisci array MQTTEngine_Var dinamico
        static std::vector<MQTTEngine_Var> vars;
        vars.clear();
        vars.reserve(cfg.varCount);

        for (auto& rv : runtimeVars) {
            MQTTEngine_Var v = {};

            v.id   = rv.cfg.id;
            v.name = rv.cfg.name;
            v.unit = rv.cfg.unit;
            v.type = rv.cfg.type;

            // reset puntatori
            v.analogPtr = nullptr;
            v.boolPtr   = nullptr;
            v.coverPos  = nullptr;


            // collega in base al tipo
            switch (rv.cfg.type) {
                case MQTTEngine_Var::HAType::SENSOR:
                    v.analogPtr = &rv.fVal;
                    break;

                case MQTTEngine_Var::HAType::BINARY_SENSOR:
                    v.boolPtr   = &rv.bVal;
                    break;

                case MQTTEngine_Var::HAType::COVER:
                    v.coverPos  = &rv.u16Val;
                    break;

                default:
                    break;
            }

            vars.push_back(v);
        }

        instance = new MQTT(
            ethClient,
            mqttClient,
            vars.data(),
            vars.size(),
            cfg.nodeId
        );

        instance->begin(cfg.broker, cfg.port);

        LOG_IF("MQTT", "MQTT Engine inizializzato con %u variabili", vars.size());
    }

    static void Loop(DomoManager& dm, unsigned long now)
    {
        if (!instance) return;

        auto& buf = dm.getBuffer();

        // Aggiorna i valori runtime leggendo dal buffer
        for (auto& rv : runtimeVars) {
            int raw = buf.getValueFast(rv.cfg.area);
            float scaled = raw * rv.cfg.scale;

            switch (rv.cfg.type) {
                case MQTTEngine_Var::HAType::SENSOR:
                    rv.fVal = scaled;
                    break;

                case MQTTEngine_Var::HAType::BINARY_SENSOR:
                    rv.bVal = (raw != 0);
                    break;

                case MQTTEngine_Var::HAType::COVER:
                    rv.u16Val = static_cast<uint16_t>(raw);
                    break;

                default:
                    break;
            }
        }

        instance->loop(now);
    }
};

// ============================================================
//  WebAPIEngine — wrapper industriale per DeviceMessageEngine
// ============================================================
class WebAPIEngine {
private:
    static inline DeviceMessageEngine* instance = nullptr;
    static inline SimpleHttpTransport transport;

    struct PendingRequest {
        String url;
        String body;
        bool isPost;
    };

    static inline std::vector<PendingRequest> queue;
    static inline bool busy = false;
    static inline String lastResponse;

    static String EmptyResolver(const char*) {
        return "";
    }

public:
    static void Setup(const FrontendConfig::webApiDeviceMessaging& cfg)
    {
        if (!cfg.enabled) {
            LOG_IF("WEBAPI", "WebAPI disabilitato");
            return;
        }

        transport = SimpleHttpTransport(80, cfg.timeoutMs);
        transport.begin();

        if (instance) delete instance;
        instance = new DeviceMessageEngine();

        DeviceMessageEngine::Context ctx = {
            .groups            = cfg.groups,
            .groupCount        = cfg.groupCount,
            .transport         = &transport,
            .registry          = &AEEEngine::getAEE(),   // 🔥 usa AEEEngine
            .baseUrl           = cfg.baseUrl,
            .responseTimeoutMs = cfg.timeoutMs
        };

        instance->Init(ctx);

        LOG_IF("WEBAPI", "WebAPI inizializzato con %u gruppi", cfg.groupCount);
    }

    static void EnqueueGET(const String& url) {
        queue.push_back({url, "", false});
    }

    static void EnqueuePOST(const String& url, const String& body) {
        queue.push_back({url, body, true});
    }

    static void Loop(unsigned long now) {
        if (!instance) return;

        if (!busy && !queue.empty()) {
            auto req = queue.front();
            queue.erase(queue.begin());

            if (req.isPost)
                transport.startRequest("POST", req.url, req.body);
            else
                transport.startRequest("GET", req.url, "");

            busy = true;
        }

        if (busy) {
            String resp;
            if (transport.loop(now, resp)) {
                busy = false;
                lastResponse = resp;
                instance->OnResponse(resp, EmptyResolver);
            }
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


class SecurityOrchestrator {
public:

    // ============================================================
    //  SYSTEM MANAGER
    // ============================================================
    class SystemManager {
    public:
        enum Field {
            ALARM_INTRUSION,
            ALARM_INTRUSION_H24,
            ALARM_FLOOD,
            ALARM_SMOKE,
            WINDOWS_OPEN,
            DOORS_OPEN,
            ALARM_TAMPER,
            FIELD_COUNT
        };

        struct Info {
            Cell<bool> f[FIELD_COUNT];
        } info;

        SystemManager() {
            for (int i = 0; i < FIELD_COUNT; i++)
                info.f[i].set(false);
        }

        void set(Field f, bool v) {
            info.f[f].setIfDiff(v);
        }

        bool hasChanged() {
            for (int i = 0; i < FIELD_COUNT; i++)
                if (info.f[i].hasChanged()) {
                    info.f[i].resetChanged();
                    return true;
                }
            return false;
        }

        int getBitmask() {
            int m = 0;
            for (int i = 0; i < FIELD_COUNT; i++)
                if (info.f[i].get()) m |= (1 << i);
            return m;
        }

        void ComputeFrom(const WiredSensorsManager& ws) {
            auto st = ws.ComputeAggregate();

            set(ALARM_INTRUSION,     st.intrusion);
            set(ALARM_INTRUSION_H24, st.intrusionH24);
            set(ALARM_FLOOD,         st.flood);
            set(ALARM_SMOKE,         st.smoke);
            set(WINDOWS_OPEN,        st.windowsOpen);
            set(DOORS_OPEN,          st.doorsOpen);
            set(ALARM_TAMPER,        st.tamper);
        }

        void DiagnosticReport() {
            LOG_IF("SystemManager", "%-25s : %s", "Intrusion",        info.f[ALARM_INTRUSION].get()     ? "TRUE" : "false");
            LOG_IF("SystemManager", "%-25s : %s", "Intrusion H24",    info.f[ALARM_INTRUSION_H24].get() ? "TRUE" : "false");
            LOG_IF("SystemManager", "%-25s : %s", "Flood",            info.f[ALARM_FLOOD].get()         ? "TRUE" : "false");
            LOG_IF("SystemManager", "%-25s : %s", "Smoke",            info.f[ALARM_SMOKE].get()         ? "TRUE" : "false");
            LOG_IF("SystemManager", "%-25s : %s", "Windows Open",     info.f[WINDOWS_OPEN].get()        ? "TRUE" : "false");
            LOG_IF("SystemManager", "%-25s : %s", "Doors Open",       info.f[DOORS_OPEN].get()          ? "TRUE" : "false");
            LOG_IF("SystemManager", "%-25s : %s", "Tamper",           info.f[ALARM_TAMPER].get()        ? "TRUE" : "false");

            LOG_IF("SystemManager", "Bitmask: %d", getBitmask());
        }
    };

private:
    static inline SystemManager system;
    static inline WiredSensorsManager ws;
    static inline FrontendConfig::Security cfgCopy;
    static inline bool initialized = false;
    static inline AlarmBitmaskManager alarmMask;


    // ============================================================
    // VALIDATION
    // ============================================================
    static bool validate(const FrontendConfig::Security* cfg) {

        if (!cfg || cfg->count == 0) {
            LOG_IF("WiredSensors", "No wired sensors configured → skipping validation");
            return true;
        }

        for (size_t i = 0; i < cfg->count; i++) {

            const auto& s = cfg->sensors[i];

            if (!s.zone || strlen(s.zone) == 0) {
                LOG_EF("WiredSensors", "Sensor %u: invalid zone (empty or null)", (unsigned)i);
                return false;
            }

            if (s.channels.size() == 0) {
                LOG_EF("WiredSensors", "Sensor %u zone=%s: no channels defined", (unsigned)i, s.zone);
                return false;
            }

            if (s.readers.size() == 0) {
                LOG_EF("WiredSensors", "Sensor %u zone=%s: no readers defined", (unsigned)i, s.zone);
                return false;
            }

            if (s.readers.size() != s.channels.size()) {
                LOG_EF("WiredSensors",
                    "Sensor %u zone=%s: readers=%u but channels=%u",
                    (unsigned)i, s.zone,
                    (unsigned)s.readers.size(),
                    (unsigned)s.channels.size());
                return false;
            }

            std::vector<SensorChannelType> seenTypes;

            for (const auto& ch : s.channels) {

                if (ch.pin < -1) {
                    LOG_EF("WiredSensors",
                        "Sensor %u zone=%s: invalid pin=%d",
                        (unsigned)i, s.zone, ch.pin);
                    return false;
                }

                if (ch.type != SensorChannelType::RT &&
                    ch.type != SensorChannelType::H24 &&
                    ch.type != SensorChannelType::LEN &&
                    ch.type != SensorChannelType::MASK)
                {
                    LOG_EF("WiredSensors",
                        "Sensor %u zone=%s: invalid channel type",
                        (unsigned)i, s.zone);
                    return false;
                }

                for (auto t : seenTypes) {
                    if (t == ch.type) {
                        LOG_EF("WiredSensors",
                            "Sensor %u zone=%s: duplicate channel type",
                            (unsigned)i, s.zone);
                        return false;
                    }
                }
                seenTypes.push_back(ch.type);
            }

            if (s.cmdArea != -1) {
                auto& buf = DomoManager::instance->getBuffer();
                if (s.cmdArea < 0 || s.cmdArea >= buf.size()) {
                    LOG_EF("WiredSensors",
                        "Sensor %u zone=%s: cmdArea %d out of range",
                        (unsigned)i, s.zone, s.cmdArea);
                    return false;
                }
            }
        }

        return true;
    }

public:

    // ============================================================
    // SETUP
    // ============================================================
    static void Setup(const FrontendConfig::Security* cfg) {
        if (!validate(cfg)) {
            LOG_EF("SecurityOrchestrator", "Setup aborted: invalid wired sensors configuration");
            return;
        }

        cfgCopy = *cfg;

        ws.Init(cfg->sensors, cfg->count, cfg->startupInhibitMs);
        alarmMask.BuildMap(ws);

        ws.Zones().EnableAll(true);
        ws.Zones().EngageAll(true);

        initialized = true;

        alarmMask.SetCallback([](uint64_t newMask,
                         uint64_t currentMask,
                         uint64_t memMask,
                         size_t bitIndex,
                         size_t sensorIndex,
                         SensorChannelType type)
        {
            LOG_IF("SecurityOrchestrator",
                "NEW SIGNAL ALARM: bit=%u sensor=%u type=%u",
                (unsigned)bitIndex,
                (unsigned)sensorIndex,
                (unsigned)type);
        });

        LOG_IF("SecurityOrchestrator",
               "Setup: sensors=%u startupInhibit=%u ms",
               cfg->count,
               cfg->startupInhibitMs);
    }

    // ============================================================
    // LOOP
    // ============================================================
    static bool Loop(unsigned long now) {
        if (!initialized)
            return false;

        // 1) Process sensori
        ws.Process(now);

        // 2) Process eventi per tipo
        auto& zones = ws.Zones();
        zones.ProcessAllTypes();
        
        // 3) Stato aggregato
        system.ComputeFrom(ws);

        // 4) Aggiorna bitmask per-segnale
        bool engaged = true; // oppure leggi da Modbus / pannello
        alarmMask.SetEngage(engaged);
        alarmMask.ComputeCurrent(ws);

        // 5) Diagnostica
        bool changed = system.hasChanged();
        if (changed) {
            LOG_IF("SecurityOrchestrator", "===== SECURITY ZONES =====");

            for (const auto& entry : ws.GetZoneMap()) {
                const std::string& zoneName = entry.first;
                bool zAlm = zones.ZoneAlarm(zoneName);
                LOG_IF("SecurityOrchestrator", "%s → %s", zoneName.c_str(), zAlm ? "ALARM" : "OK");
            }

            LOG_IF("SecurityOrchestrator", "===== END SECURITY ZONES =====");

            system.DiagnosticReport();
        }

        return changed;
    }

    // ============================================================
    // COMMANDS
    // ============================================================
    static void ApplySecurityCommands(int area) {
        auto* dm = DomoManager::instance;
        if (!dm) return;

        auto& buffer = dm->getBuffer();
        const auto* cfg = ws.GetConfig();

        for (size_t i = 0; i < ws.Count(); i++) {
            const auto& c = cfg[i];
            Sensor* s = ws.GetSensor(i);
            if (!s) continue;

            if (c.cmdArea < 0 || c.cmdArea >= buffer.size()) continue;

            if (area == c.cmdArea) {
                long v = buffer.getValueFast(c.cmdArea);
                bool enable = bitRead(v, 0);
                bool engage = bitRead(v, 1);

                s->Enable(enable);
                s->Engage(engage);

                LOG_IF("SecurityOrchestrator",
                    "ApplySecurityCommands: sensor[%u] enable=%d engage=%d",
                    (unsigned)i, enable ? 1 : 0, engage ? 1 : 0);
            }
        }
    }

    static void ForceSecurityCommands(bool engage, unsigned long now) {
        auto* dm = DomoManager::instance;
        if (!dm) return;

        auto& buffer = dm->getBuffer();
        const auto* cfg = ws.GetConfig();

        for (size_t i = 0; i < ws.Count(); i++) {
            const auto& c = cfg[i];
            Sensor* s = ws.GetSensor(i);
            if (!s) continue;

            if (c.cmdArea < 0 || c.cmdArea >= buffer.size()) continue;

            long v = buffer.getValueFast(c.cmdArea);
            v = bitWrite(v, 1, engage);
            buffer.WriteElement(c.cmdArea, BufferFlagType::ToPanel, v, now);

            s->Engage(engage);

            LOG_IF("SecurityOrchestrator",
                "ForceSecurityCommands: sensor[%u] engage=%d written to area=%d",
                (unsigned)i, engage ? 1 : 0, c.cmdArea);
        }
    }

    // ============================================================
    // CALLBACK REGISTRATION API (Opzione B)
    // ============================================================
    static void RegisterCallbackAny(AlarmDispatcher::Callback cb) {
        ws.Zones().dispatcher.OnAnyAlarm(cb);
    }

    static void RegisterCallbackType(SensorChannelType type, AlarmDispatcher::Callback cb) {
        ws.Zones().dispatcher.OnAlarmType(type, cb);
    }

    static void RegisterCallbackZone(const std::string& zone, AlarmDispatcher::Callback cb) {
        ws.Zones().dispatcher.OnZoneAlarm(zone, cb);
    }

    static void RegisterCallbackZoneType(const std::string& zone,
                                         SensorChannelType type,
                                         AlarmDispatcher::Callback cb)
    {
        ws.Zones().dispatcher.OnZoneAlarmType(zone, type, cb);
    }

    // ============================================================
    // ACCESSORS
    // ============================================================
    static SystemManager& getSystem() { return system; }
    static WiredSensorsManager& getWiredSensors() { return ws; }

    class Diagnostic {
    public:
        static void ReportSensors();
        static void ReportZones();
        static void ReportCommands();
        static void ReportInconsistencies();
        static void FullReport();
    };


    static void ReportSensors() { /* empty or custom */ }
    static void ReportZones() { /* empty or custom */ }
    static void ReportCommands() { /* empty or custom */ }
    static void ReportInconsistencies() { /* empty or custom */ }
};

// ============================================================
//  TASK ENGINE (BACKEND / ORCHESTRATOR)
// ============================================================
class TaskEngineOrchestrator {
public:
    using TaskFn = void(*)(DomoManager&, unsigned long);

    struct Task {
        TaskFn fn;
        uint32_t interval;
        uint32_t lastRun;
        bool enabled;
    };

private:
    static inline bool frontendCycleDone = false;
    static inline std::vector<Task> tasks;   // 🔥 ORA Task è già dichiarato
    static inline const FrontendConfig* cfg = nullptr;

public:
    static void Setup(const FrontendConfig& c) {
        cfg = &c;   // 🔥
        Clear();
    }

    // Aggiunge un task dinamico
    static void AddTask(TaskFn fn, uint32_t intervalMs, bool enabled) {
        tasks.push_back({fn, intervalMs, 0, enabled});
    }

    // Cancella tutti i task (usato dal frontend in Setup)
    static void Clear() {
        tasks.clear();
    }

    // Loop principale del motore
    static void Loop(DomoManager& manager, unsigned long now) {
        for (auto& t : tasks) {
            if (!t.enabled)
                continue;

            if (now - t.lastRun >= t.interval) {
                t.lastRun = now;
                t.fn(manager, now);
            }
        }

        frontendCycleDone = true;
    }

    // ------------------------------------------------------------
    //  FRONTEND CYCLE STATUS
    // ------------------------------------------------------------
    static bool hasFrontendCycleCompleted() {
        return frontendCycleDone;
    }

    static void resetFrontendCycleFlag() {
        frontendCycleDone = false;
    }

    static const FrontendConfig& getCfg() {
        return *cfg;
    }
};


 void SecurityOrchestrator::Diagnostic::ReportSensors() {
    auto& ws = SecurityOrchestrator::ws;
    auto* cfg = ws.GetConfig();

    Serial.println("===== SENSORS REPORT =====");

    for (size_t i = 0; i < ws.Count(); i++) {
        const auto& c = cfg[i];
        Sensor* s = ws.GetSensor(i);

        Serial.print("Sensor ");
        Serial.print(i);
        Serial.print(" [zone=");
        Serial.print(c.zone);
        Serial.print("] category=");

        switch (c.category) {
            case SensorCategory::PIR:    Serial.print("PIR"); break;
            case SensorCategory::WINDOW: Serial.print("WINDOW"); break;
            case SensorCategory::DOOR:   Serial.print("DOOR"); break;
            case SensorCategory::FLOOD:  Serial.print("FLOOD"); break;
            case SensorCategory::SMOKE:  Serial.print("SMOKE"); break;
            case SensorCategory::TAMPER: Serial.print("TAMPER"); break;
            default:                     Serial.print("OTHER"); break;
        }

        Serial.print(" alarmOut=");
        Serial.println(s->alarmOut ? "YES" : "NO");
    }

    Serial.println("==========================");
}

void SecurityOrchestrator::Diagnostic::ReportZones() {
    auto& ws = SecurityOrchestrator::ws;
    auto& zones = ws.Zones();

    Serial.println("===== ZONES REPORT =====");

    for (const auto& entry : ws.GetZoneMap()) {
        const std::string& zoneName = entry.first;
        bool alm = zones.ZoneAlarm(zoneName);

        Serial.print(zoneName.c_str());
        Serial.print(" → ");
        Serial.println(alm ? "ALARM" : "OK");
    }

    Serial.println("=========================");
}

void SecurityOrchestrator::Diagnostic::ReportCommands() {
    auto& ws = SecurityOrchestrator::ws;
    auto* cfg = ws.GetConfig();

    Serial.println("===== COMMANDS REPORT =====");

    for (size_t i = 0; i < ws.Count(); i++) {
        const auto& c = cfg[i];

        Serial.print("Sensor ");
        Serial.print(i);
        Serial.print(" [zone=");
        Serial.print(c.zone);
        Serial.print("] cmdArea=");

        if (c.cmdArea >= 0)
            Serial.println(c.cmdArea);
        else
            Serial.println("NONE");
    }

    Serial.println("===========================");
}

void SecurityOrchestrator::Diagnostic::ReportInconsistencies() {
    auto& ws = SecurityOrchestrator::ws;
    auto* cfg = ws.GetConfig();

    Serial.println("===== INCONSISTENCIES REPORT =====");

    for (size_t i = 0; i < ws.Count(); i++) {
        const auto& c = cfg[i];
        Sensor* s = ws.GetSensor(i);

        // Esempio: finestra aperta ma nessun canale attivo
        if (c.category == SensorCategory::WINDOW && !s->alarmOut) {
            Serial.print("WARNING: Window sensor ");
            Serial.print(i);
            Serial.print(" in zone ");
            Serial.print(c.zone);
            Serial.println(" reports no alarm but category=WINDOW");
        }
    }

    Serial.println("===================================");
}

void SecurityOrchestrator::Diagnostic::FullReport() {
    ReportSensors();
    ReportZones();
    ReportCommands();
    ReportInconsistencies();

    Serial.println("===== SYSTEM STATE =====");
    SecurityOrchestrator::system.DiagnosticReport();
    Serial.println("========================");
}


#endif
