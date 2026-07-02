
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

//Ha classe privata interna AEE
class BridgeEngine {
public:
    DMMasterBridge bridge;

    static BridgeEngine& instance() {
        static BridgeEngine inst;
        return inst;
    }

    static DMMasterBridge& get() {
        return instance().bridge;
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

#endif
