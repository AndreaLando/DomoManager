
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

    struct ReceiveContext {
        Buffer* buffer = nullptr;
        EventManager* events = nullptr;
        int aeeSource = -1;
    };

private:
    ReceiveContext receiveContext;

public:

    static AEEEngine& instance() {
        static AEEEngine inst;
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

    // ============================================================
    // CONTEXT RICEZIONE
    // ============================================================

    void attachReceiveContext(
        Buffer& buffer,
        EventManager& events,
        int aeeSource)
    {
        receiveContext.buffer = &buffer;
        receiveContext.events = &events;
        receiveContext.aeeSource = aeeSource;
    }

    // ============================================================
    // RICEZIONE AEE DAL PEER
    // ============================================================

    void applyReceived(
        AEEVariableBase* v,
        unsigned long now)
    {
        if (!v)
            return;

        if (!receiveContext.buffer ||
            !receiveContext.events)
            return;

        // --------------------------------------------------------
        // La variabile deve essere ricevibile dal Peer.
        // --------------------------------------------------------

        if (v->def.direction != AEEDirection::ModuleToFrontend &&
            v->def.direction != AEEDirection::Bidirectional)
        {
            return;
        }

        // ========================================================
        // 1. BUFFER AREA
        // ========================================================

        if (v->def.sourceType == AEEVarSourceType::BufferArea)
        {
            applyBufferArea(v, now);
            return;
        }

        // ========================================================
        // 2. FUNZIONE / SISTEMA
        // ========================================================

        onAEEVarChanged(v, now);
    }

private:

    // ============================================================
    // AEE BUFFER AREA → BUFFER + EVENT MANAGER
    // ============================================================

    void applyBufferArea(
        AEEVariableBase* v,
        unsigned long now)
    {
        Buffer& buffer = *receiveContext.buffer;

        const int area = v->def.bufferArea;

        if (area < 0 || area >= buffer.size())
        {
            LOG_WF(
                "AEE::RX",
                "Area non valida per '%s': %d",
                v->def.name,
                area
            );
            return;
        }

        long value = 0;

        // --------------------------------------------------------
        // BOOL
        // --------------------------------------------------------

        if (auto* b = as<bool>(v))
        {
            if (v->def.bitIndex >= 0)
            {
                // Mantieni tutti gli altri bit.
                value = buffer.getValueFast(area);

                bitWrite(
                    value,
                    v->def.bitIndex,
                    b->get()
                );
            }
            else
            {
                value = b->get() ? 1L : 0L;
            }
        }

        // --------------------------------------------------------
        // INT
        // --------------------------------------------------------

        else if (auto* i = as<int>(v))
        {
            value = static_cast<long>(
                i->get() / v->def.scale
            );
        }

        // --------------------------------------------------------
        // FLOAT
        // --------------------------------------------------------

        else if (auto* f = as<float>(v))
        {
            value = static_cast<long>(
                f->get() / v->def.scale
            );
        }

        else
        {
            LOG_WF(
                "AEE::RX",
                "Tipo non supportato per '%s'",
                v->def.name
            );
            return;
        }

        // --------------------------------------------------------
        // 1) Scrittura nel Buffer
        // --------------------------------------------------------

        buffer.WriteElement(
            area,
            value,
            now
        );

        // --------------------------------------------------------
        // 2) Pubblicazione evento
        //
        // IMPORTANTE:
        // usiamo lo stesso aeeSource del consumer AEE.
        // Quindi AEEEventCallback viene escluso dal dispatch.
        // --------------------------------------------------------

        receiveContext.events->push(
            area,
            value,
            static_cast<uint8_t>(
                receiveContext.aeeSource
            )
        );

        LOG_DF(
            "AEE::RX",
            "AEE '%s' -> Buffer area=%d value=%ld",
            v->def.name,
            area,
            value
        );
    }


    // ============================================================
    // POST PROCESSOR AEE
    //
    // Qui confluisce la vecchia OnAEEVarChanged().
    // ============================================================

    void onAEEVarChanged(
        AEEVariableBase* v,
        unsigned long now)
    {
        if (!v)
            return;

        // ========================================================
        // NONE / MESSAGGI DI SISTEMA
        // ========================================================

        if (v->def.sourceType == AEEVarSourceType::None)
        {
            // ----------------------------------------------------
            // EPOCH
            // ----------------------------------------------------

            if (strcmp(
                    v->def.name,
                    "epoch") == 0)
            {
                auto* e = as<int>(v);

                if (!e)
                {
                    LOG_WF(
                        "AEE",
                        "epoch ricevuto con tipo non valido"
                    );
                    return;
                }

                int epoch = e->get();

                LOG_IF(
                    "AEE",
                    "Epoch ricevuto dal PEER: %d → applico al RTC",
                    epoch
                );

                receiveContext.buffer; // nessun uso, solo contesto AEE

                // Qui non usiamo il Buffer.
                // Serve il TimeManager del DOMO.
                if (DomoManager::instance)
                {
                    DomoManager::instance
                        ->getTimeManager()
                        .getRTC()
                        .applyEpoch(epoch);
                }

                v->clearChanged();

                return;
            }

            return;
        }

        // ========================================================
        // FUNCTION
        // ========================================================

        if (v->def.sourceType ==
            AEEVarSourceType::Function)
        {
            // Post processor applicativo.
            //
            // Qui aggiungeremo le eventuali AEE Function
            // ricevibili dal Peer.
            return;
        }
    }
};

class BridgeEngine
{
private:

    class DomoBridgePacer : public IBridgePacer
    {
    private:
        NetworkManager& network;
        NetworkManager::ProtocolId protocolId;

    public:
        DomoBridgePacer(
            NetworkManager& n,
            NetworkManager::ProtocolId id)
            : network(n),
              protocolId(id)
        {}

        bool acquire(unsigned long now) override
        {
            return network.tryAcquire(
                protocolId,
                now
            );
        }

        void release(unsigned long now) override
        {
            network.release(
                protocolId,
                now
            );
        }
    };


    DMMasterBridge bridge;
    DomoBridgePacer* pacer = nullptr;


public:

    static BridgeEngine& instance()
    {
        static BridgeEngine inst;
        return inst;
    }


    static DMMasterBridge& get()
    {
        return instance().bridge;
    }


    void setupPacer(
        NetworkManager& network,
        NetworkManager::ProtocolId protocolId)
    {
        delete pacer;

        pacer = new DomoBridgePacer(
            network,
            protocolId
        );

        bridge.setPacer(*pacer);
    }
};

// ============================================================
// MQTT FRONTEND ENGINE
// Integrazione MQTT <-> DomoManager
// ============================================================

class MQTTEngine
{
public:

    static const uint8_t MAX_CLIENTS  = 4;
    static const uint8_t MAX_MAPPINGS = 64;

    // ========================================================
    // CALLBACK DAL LOW LEVEL MQTT
    // ========================================================

    typedef void (*LowLevelCommandCallback)(
        uint8_t clientIndex,
        const FrontendConfig::MQTT::Device* device,
        const FrontendConfig::MQTT::Mapping* mapping,
        long value
    );

    // ========================================================
    // RUNTIME MAPPING
    // ========================================================

    struct RuntimeMapping
    {
        uint8_t clientIndex;

        const FrontendConfig::MQTT::Client* client;
        const FrontendConfig::MQTT::Device* device;
        const FrontendConfig::MQTT::Mapping* mapping;

        long lastRaw;

        bool dirty;
        bool initialized;
    };

    // ========================================================
    // RUNTIME CLIENT
    // ========================================================

    struct RuntimeClient
    {
        uint8_t index;

        const FrontendConfig::MQTT::Client* cfg;

        EthernetClient* eth;
        PubSubClient* pubSub;
        MQTT* mqtt;

        unsigned long lastHeartbeat;

        bool initialized;

        SocketHandle socket;
    };

private:
    // ========================================================
    // STORAGE STATICO COMPATIBILE CON VECCHIO GCC
    // ========================================================

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

    /*
     * PubSubClient deve essere associato al relativo
     * EthernetClient prima dell'uso.
     */
    static MQTT** getMQTTInstances()
    {
        static MQTT* instances[MAX_CLIENTS];
        return instances;
    }

    static DomoManager* manager;

    static const FrontendConfig::MQTT* configuration;

    static size_t clientCount;
    static size_t mappingCount;

    static unsigned long lastLoop;
    
    static NetworkManager* networkManager;
    static NetworkManager::ProtocolId mqttProtocolId;
    static uint8_t mqttSource;
public:

    // ========================================================
    // SETUP
    // ========================================================

    static void Setup(
        DomoManager& dm,
        NetworkManager& netManager,
        EthernetClient* mqttClients,
        PubSubClient* pubSubClients,
        size_t mqttClientCount,
        const FrontendConfig::MQTT& cfg,
        NetworkManager::ProtocolId protocol,
        uint8_t eventSource
    )
    {
        manager = &dm;
        mqttSource = eventSource;
        networkManager = &netManager;
        configuration = &cfg;
        mqttProtocolId = protocol;

        RuntimeClient* clients = getClients();
        RuntimeMapping* mappings = getMappings();

        memset(
            mappings,
            0,
            sizeof(RuntimeMapping) * MAX_MAPPINGS
        );

        clientCount = 0;
        mappingCount = 0;

        BuildRuntimeMappings(cfg);

        if (!cfg.enabled)
        {
            LOG_IF(
                "MQTT",
                "MQTT disabilitato"
            );
            return;
        }

        if (!networkManager)
        {
            LOG_IF(
                "MQTT",
                "NetworkManager nullo"
            );
            return;
        }

        if (!mqttClients)
        {
            LOG_IF(
                "MQTT",
                "Array EthernetClient MQTT nullo"
            );
            return;
        }

        if (!pubSubClients)
        {
            LOG_IF(
                "MQTT",
                "Array PubSubClient MQTT nullo"
            );
            return;
        }

        size_t maxClients = MAX_CLIENTS;

        if (mqttClientCount < maxClients)
            maxClients = mqttClientCount;

        if (cfg.clientCount < maxClients)
            maxClients = cfg.clientCount;

        for (size_t i = 0; i < maxClients; ++i)
        {
            const FrontendConfig::MQTT::Client* clientCfg =
                &cfg.clients[i];

            if (!clientCfg->enabled)
                continue;

            if (clientCount >= MAX_CLIENTS)
                break;

            RuntimeClient& rc =
                clients[clientCount];

            rc.index = (uint8_t)i;
            rc.cfg = clientCfg;

            rc.eth =
                &mqttClients[i];

            rc.pubSub =
                &pubSubClients[i];

            rc.mqtt = nullptr;
            rc.lastHeartbeat = 0;
            rc.initialized = false;

            rc.mqtt = new MQTT(
                *rc.eth,
                *rc.pubSub,
                cfg.nodeId,
                (uint8_t)i,
                clientCfg,
                cfg.devices,
                cfg.deviceCount
            );

            rc.socket.setup(
                networkManager,
                "MQTT",
                (int)rc.index
            );

            if (!rc.mqtt)
            {
                LOG_IF(
                    "MQTT",
                    "Client[%u] allocazione MQTT fallita",
                    (unsigned)i
                );
                continue;
            }

            rc.mqtt->setCommandCallback(
                MQTTCommandCallback
            );

            rc.mqtt->setBackend(
                clientCfg->backend
            );

            if (!rc.mqtt->begin())
            {
                LOG_IF(
                    "MQTT",
                    "Client[%u] begin fallito: %s",
                    (unsigned)i,
                    clientCfg->name
                        ? clientCfg->name
                        : "unnamed"
                );

                delete rc.mqtt;
                rc.mqtt = nullptr;

                continue;
            }

            rc.initialized = true;

            LOG_IF(
                "MQTT",
                "Client[%u] inizializzato: %s backend=%u broker=%s:%u",
                (unsigned)i,
                clientCfg->name
                    ? clientCfg->name
                    : "unnamed",
                (unsigned)clientCfg->backend,
                clientCfg->broker.toString().c_str(),
                (unsigned)clientCfg->port
            );

            ++clientCount;
        }

        InitializeRuntimeValues();

        LOG_IF(
            "MQTT",
            "MQTT Setup completato: clients=%u mappings=%u protocol=%d",
            (unsigned)clientCount,
            (unsigned)mappingCount,
            (int)mqttProtocolId
        );
    }

    // ========================================================
    // LOOP
    // ========================================================

    static void Loop(
        unsigned long now
    )
    {
        if (!configuration)
            return;

        if (!configuration->enabled)
            return;

        if (!networkManager)
            return;

        /*
        * Lasciamo partire HMI e Modbus prima di MQTT.
        */
        static bool mqttStartupReady = false;

        if (!mqttStartupReady)
        {
            if (now < 10000UL)
                return;

            mqttStartupReady = true;
        }

        RuntimeClient* clients =
            getClients();

        for (size_t i = 0;
            i < clientCount;
            ++i)
        {
            RuntimeClient& rc =
                clients[i];

            if (!rc.initialized)
                continue;

            if (!rc.mqtt)
                continue;

            /*
            * MQTT accede alla rete solo se
            * NetworkManager concede il canale.
            */
            if (!networkManager->tryAcquire(
                    mqttProtocolId,
                    now))
            {
                LOG_EF(
                    "MQTT",
                    "Client[%u] NetworkManager NEGATO",
                    (unsigned)i
                );

                continue;
            }

            /*
            * ========================================================
            * CONNECT / RECONNECT
            * ========================================================
            *
            * Il timer di retry è gestito interamente da MQTT.
            * Non acquisiamo il socket se il tentativo non è dovuto.
            */
            const bool mqttConnectedBefore =
                rc.mqtt->connected();

            if (!mqttConnectedBefore)
            {
                if (rc.mqtt->reconnectDue())
                {
                    if (rc.socket.acquire())
                    {
                        /*
                        * Se la connessione fallisce,
                        * il socket viene immediatamente liberato.
                        */
                        if (!rc.mqtt->reconnect())
                        {
                            rc.socket.release();
                        }
                    }
                    else
                    {
                        LOG_WF(
                            "MQTT",
                            "Client[%u] socket non disponibile",
                            (unsigned)i
                        );
                    }
                }
            }


            /*
            * ========================================================
            * MQTT LOOP
            * ========================================================
            */
            if (rc.mqtt->connected())
            {
                rc.mqtt->loop();
            }
            else
            {
                LOG_EF(
                    "MQTT",
                    "Client[%u] NOT CONNECTED BEFORE mqtt.loop state=%d eth=%d socket=%d",
                    (unsigned)i,
                    rc.pubSub->state(),
                    rc.eth->connected() ? 1 : 0,
                    rc.socket.valid() ? 1 : 0
                );

                rc.socket.release();
            }
            
            /*
            * ========================================================
            * RELEASE DEL TURNO PROTOCOLLO
            * ========================================================
            *
            * Non è il rilascio del socket fisico.
            */
            networkManager->release(
                mqttProtocolId,
                now
            );
        }

        /*
        * Publish dei mapping dirty.
        */
        PublishDirty();

        lastLoop = now;
    }

    // ========================================================
    // EVENTO DAL SISTEMA DOMOTICO
    // ========================================================

    static void PublishEvent(
        const EventManager::Event& event
    )
    {
        PublishEvent(
            event.area,
            event.value
        );
    }

    static void PublishEvent(
        int area,
        long value
    )
    {
        RuntimeMapping* mappings =
            getMappings();

        for (size_t i = 0;
             i < mappingCount;
             ++i)
        {
            RuntimeMapping& rm =
                mappings[i];

            if (!rm.mapping)
                continue;

            if (rm.mapping->area != area)
                continue;

            LOG_EF(
                "MQTT",
                "PublishEvent MATCH client=%u device=%s field=%s area=%d value=%ld",
                (unsigned)rm.clientIndex,
                rm.device && rm.device->id ? rm.device->id : "?",
                rm.mapping && rm.mapping->field ? rm.mapping->field : "?",
                rm.mapping->area,
                value
            );

            if (!CanWrite(rm.mapping))
                continue;

            rm.lastRaw = value;
            rm.dirty = true;
        }
    }

    // ========================================================
    // CALLBACK MQTT -> DOMO
    // ========================================================

    static void MQTTCommandCallback(
        uint8_t clientIndex,
        const FrontendConfig::MQTT::Device* device,
        const FrontendConfig::MQTT::Mapping* mapping,
        long value
    )
    {
        if (!manager)
            return;

        if (!mapping)
            return;

        if (!CanRead(mapping))
            return;

        /*
         * MQTT -> sistema domotico.
         *
         * IMPORTANTE:
         * qui DMMQTTEngine finisce.
         *
         * DomoManager viene usato esclusivamente
         * dal FrontendEngine.
         */
        manager->forceEvent(
            mapping->area,
            value,
            mqttSource
        );

        LOG_IF(
            "MQTT",
            "CMD client=%u device=%s field=%s area=%d value=%ld",
            (unsigned)clientIndex,
            device && device->id
                ? device->id
                : "?",
            mapping->field
                ? mapping->field
                : "?",
            mapping->area,
            value
        );
    }

    // ========================================================
    // DIAGNOSTICA
    // ========================================================

    static size_t getClientCount()
    {
        return clientCount;
    }

    static size_t getMappingCount()
    {
        return mappingCount;
    }

    static const RuntimeClient* getClient(
        size_t index
    )
    {
        if (index >= clientCount)
            return nullptr;

        return &getClients()[index];
    }

    static const RuntimeMapping* getMapping(
        size_t index
    )
    {
        if (index >= mappingCount)
            return nullptr;

        return &getMappings()[index];
    }

private:

    // ========================================================
    // BUILD RUNTIME MAPPINGS
    // ========================================================

    static void BuildRuntimeMappings(
        const FrontendConfig::MQTT& cfg
    )
    {
        RuntimeMapping* mappings =
            getMappings();

        mappingCount = 0;

        if (!cfg.devices)
            return;

        for (size_t d = 0;
             d < cfg.deviceCount;
             ++d)
        {
            const FrontendConfig::MQTT::Device& device =
                cfg.devices[d];

            if (!cfg.clients)
                continue;

            if (device.client >= cfg.clientCount)
                continue;

            const FrontendConfig::MQTT::Client& client =
                cfg.clients[device.client];

            if (!client.enabled)
                continue;

            if (!device.mappings)
                continue;

            for (size_t m = 0;
                 m < device.mappingCount;
                 ++m)
            {
                if (mappingCount >= MAX_MAPPINGS)
                    return;

                RuntimeMapping& rm =
                    mappings[mappingCount];

                rm.clientIndex =
                    device.client;

                rm.client =
                    &client;

                rm.device =
                    &device;

                rm.mapping =
                    &device.mappings[m];

                rm.lastRaw = 0;
                rm.dirty = false;
                rm.initialized = false;

                ++mappingCount;
            }
        }
    }

    // ========================================================
    // INIZIALIZZAZIONE VALORI
    // ========================================================

    static void InitializeRuntimeValues()
    {
        RuntimeMapping* mappings =
            getMappings();

        /*
         * Qui leggiamo il valore attuale dal sistema.
         *
         * Questa parte dipende dall'API reale del tuo Buffer.
         *
         * Per il momento non tocchiamo il Buffer:
         * initialized resta false e il primo evento
         * proveniente dal sistema farà partire il publish.
         */
        for (size_t i = 0;
             i < mappingCount;
             ++i)
        {
            mappings[i].lastRaw = 0;
            mappings[i].dirty = false;
            mappings[i].initialized = false;
        }
    }

    // ========================================================
    // PUBLISH DIRTY
    // ========================================================

    static void PublishDirty()
    {
        RuntimeClient* clients =
            getClients();

        RuntimeMapping* mappings =
            getMappings();

        for (size_t i = 0;
             i < mappingCount;
             ++i)
        {
            RuntimeMapping& rm =
                mappings[i];

            if (!rm.dirty)
                continue;

            for (size_t c = 0;
                 c < clientCount;
                 ++c)
            {
                RuntimeClient& rc =
                    clients[c];

                if (rc.index != rm.clientIndex)
                    continue;

                if (!rc.initialized)
                    break;

                if (!rc.mqtt)
                    break;

                if (!rc.mqtt->connected())
                    break;

                if (
                    rc.mqtt->publishMapping(
                        rm.device,
                        rm.mapping,
                        rm.lastRaw
                    )
                )
                {
                    rm.dirty = false;
                    rm.initialized = true;
                }

                break;
            }
        }
    }

    // ========================================================
    // DIRECTION
    // ========================================================

    static bool CanRead(
        const FrontendConfig::MQTT::Mapping* mapping
    )
    {
        if (!mapping)
            return false;

        return
            mapping->direction ==
                FrontendConfig::MQTT::Mapping::Direction::READ
            ||
            mapping->direction ==
                FrontendConfig::MQTT::Mapping::Direction::READ_WRITE;
    }

    static bool CanWrite(
        const FrontendConfig::MQTT::Mapping* mapping
    )
    {
        if (!mapping)
            return false;

        return
            mapping->direction ==
                FrontendConfig::MQTT::Mapping::Direction::WRITE
            ||
            mapping->direction ==
                FrontendConfig::MQTT::Mapping::Direction::READ_WRITE;
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

NetworkManager* MQTTEngine::networkManager = nullptr;
NetworkManager::ProtocolId MQTTEngine::mqttProtocolId = -1;
uint8_t MQTTEngine::mqttSource = 255;

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

// ============================================================
// HMI ENGINE
//
// Responsabilità:
//   1. Gestione connessioni Modbus TCP HMI
//   2. Poll dei client
//   3. Sincronizzazione Buffer <-> HMI
//
// NON esegue direttamente Modbus RTU.
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

    void PushEvent(const EventManager::Event& event)
    {
        for (uint8_t client = 0;
            client < _maxClients;
            ++client)
        {
            if (!_context.clients[client] ||
                !_context.clients[client].connected())
                continue;

            if (event.area < 0 ||
                event.area >= _registerCount)
            {
                continue;
            }

            const uint16_t value =
                static_cast<uint16_t>(event.value);

            bool ok =
                _context.modbusServers[client]
                    .holdingRegisterWrite(
                        event.area,
                        value
                    );

            if (!ok)
            {
                LOG_WF(
                    "HMIEngine",
                    "PushEvent FAIL: client=%u area=%d value=%u",
                    client,
                    event.area,
                    value
                );

                continue;
            }

            // Allinea la shadow al valore scritto internamente.
            _registerShadow[client][event.area] = value;
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

    struct PendingHMIUpdate
    {
        int area;
        long value;
        uint8_t client;
    };
    std::vector<PendingHMIUpdate> pendingUpdates;

    // Stato ultimo valore noto dei Holding Register per ogni client.
    // Serve per distinguere una WRITE reale del client da una
    // scrittura effettuata internamente da PushEvent().
    uint16_t _registerCount = 0;
    std::vector<std::vector<uint16_t>> _registerShadow;

    uint8_t _eventSource = 0;

    // ========================================================
    // NETWORK CONTEXT
    //
    // Questo è il contesto PRIVATO dell'HMI.
    //
    // Non appartiene a NetworkContext.
    // ========================================================

    struct Context
    {
        EthernetServer server;

        std::vector<EthernetClient> clients;

        std::vector<ModbusTCPServer> modbusServers;
    };


    Context _context;


    // ========================================================
    // SETUP SERVER
    // ========================================================

    void SetupServer()
    {
        _activeClients = 0;


        _context.clients.clear();

        _context.modbusServers.clear();


        // ----------------------------------------------------
        // Dynamic client slots
        // ----------------------------------------------------

        _context.clients.resize(
            _maxClients
        );

        _context.modbusServers.resize(
            _maxClients
        );


        // ----------------------------------------------------
        // TCP SERVER
        // ----------------------------------------------------

        _context.server.begin(
            _config.port
        );


        // ----------------------------------------------------
        // MODBUS TCP SERVER
        // ----------------------------------------------------

        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            if (!_context.modbusServers[i].begin())
            {
                LOG_EF(
                    "HMIEngine",
                    "Modbus TCP server %d initialization failed",
                    i
                );

                while (true)
                    delay(100);
            }


            _context.modbusServers[i]
                .configureHoldingRegisters(
                    0x00,
                    _registerCount
                );
        }


        LOG_IF(
            "HMIEngine",
            "HMI Modbus TCP started: port=%d maxClients=%d",
            _config.port,
            _maxClients
        );
    }


    // ========================================================
    // CONNECTION MANAGEMENT
    // ========================================================

    void ProcessConnections()
    {
        // ====================================================
        // 1. CLEANUP
        // ====================================================

        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            if (_context.clients[i] &&
                !_context.clients[i].connected())
            {
                LOG_IF(
                    "HMIEngine",
                    "HMI client %d disconnected",
                    i
                );


                _context.clients[i].stop();


                if (_activeClients > 0)
                    --_activeClients;
            }
        }


        // ====================================================
        // 2. ACCEPT
        // ====================================================

        EthernetClient newClient =
            _context.server.accept();


        if (!newClient)
            return;


        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            if (!_context.clients[i] ||
                !_context.clients[i].connected())
            {
                _context.clients[i] = newClient;


                _context.modbusServers[i].accept(
                    _context.clients[i]
                );


                ++_activeClients;


                LOG_IF(
                    "HMIEngine",
                    "HMI client %d connected - active=%d",
                    i,
                    _activeClients
                );


                return;
            }
        }


        // ====================================================
        // 3. NO SLOT
        // ====================================================

        LOG_WF(
            "HMIEngine",
            "HMI connection rejected: slots full"
        );


        newClient.stop();
    }


    // ========================================================
    // POLL
    // ========================================================

    void Poll()
    {
        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            if (_context.clients[i] &&
                _context.clients[i].connected())
            {
                _context.modbusServers[i].poll();
            }
        }
    }

    // ========================================================
    // HMI -> BUFFER
    //
    // Holding Register
    //       ↓
    // FromPanel
    //       ↓
    // Field
    //       ↓
    // ToPanel
    // ========================================================
    bool consumePendingUpdate(
        uint8_t client,
        int area,
        long value)
    {
        for (auto it = pendingUpdates.begin();
            it != pendingUpdates.end();
            ++it)
        {
            if (it->client == client &&
                it->area == area &&
                it->value == value)
            {
                pendingUpdates.erase(it);
                return true;
            }
        }

        return false;
    }

    void SyncHMIToBuffer(
        DomoManager& manager,
        unsigned long now)
    {
        (void)now;

        // ============================================================
        // HMI -> EVENT MANAGER
        //
        // Holding Register
        //      ↓
        // confronto con shadow
        //      ↓
        // modifica reale HMI
        //      ↓
        // EventManager.push()
        // ============================================================

        if (!manager.getEventManager().size())
            return;

        for (uint8_t client = 0;
            client < _maxClients;
            ++client)
        {
            if (!_context.clients[client] ||
                !_context.clients[client].connected())
                continue;

            auto& server =
                _context.modbusServers[client];

            for (int area = 0;
                area < _registerCount;
                ++area)
            {
                const uint16_t value =
                    server.holdingRegisterRead(area);

                // ----------------------------------------------------
                // Nessuna variazione rispetto all'ultimo stato noto
                // ----------------------------------------------------

                if (_registerShadow[client][area] == value)
                    continue;

                // ----------------------------------------------------
                // Il client HMI ha realmente modificato il registro
                // ----------------------------------------------------

                _registerShadow[client][area] = value;

                // ----------------------------------------------------
                // HMI -> EventManager
                // ----------------------------------------------------

                manager.getEventManager().push(
                    area,
                    value,
                    _eventSource 
                );
            }
        }
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
          _context()
    {
    }


    // ========================================================
    // SETUP
    //
    // cfg:
    //     cfg.domoManager.hmi
    //
    // protocol:
    //     identificativo protocollo.
    //
    // maxClients:
    //     numero dinamico di client HMI.
    // ========================================================

    void Setup(
        const DomoManagerConfig::HMI& cfg,
        NetworkManager::ProtocolId protocol,
        uint8_t maxClients,
        uint16_t registerCount,
        uint8_t eventSource)
    {
        (void)protocol;

        // ----------------------------------------------------
        // Configuration
        // ----------------------------------------------------

        _config.enabled =
            cfg.enabled;


        _config.port =
            cfg.port;


        _maxClients = maxClients;
        _registerCount = registerCount;
        _eventSource   = eventSource;

        _registerShadow.clear();
        _registerShadow.resize(_maxClients);

        for (uint8_t client = 0; client < _maxClients; ++client)
        {
            _registerShadow[client].assign(
                _registerCount,
                0
            );
        }

        _activeClients =
            0;


        _loopEnabled =
            false;


        // ----------------------------------------------------
        // Disabled
        // ----------------------------------------------------

        if (!_config.enabled)
        {
            LOG_IF(
                "HMIEngine",
                "HMI disabled"
            );

            return;
        }


        // ----------------------------------------------------
        // Validate client count
        // ----------------------------------------------------

        if (_maxClients == 0)
        {
            return;
        }


        // ----------------------------------------------------
        // Start server
        // ----------------------------------------------------

        SetupServer();
    }


    // ========================================================
    // LOOP ENABLE
    //
    // Il networking HMI rimane fermo fino a quando
    // il frontend decide di abilitarlo.
    //
    // Esempio:
    //
    // HMIEngine::SetLoopEnabled(
    //     Manager->getPowerOnCycleCompleted()
    // );
    // ========================================================

    void SetLoopEnabled(
        bool enabled)
    {
        if (!_config.enabled)
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
    //
    // Solo:
    //   cleanup
    //   accept
    //   poll
    //
    // Nessuna logica Buffer.
    // ========================================================

    void ProcessNetwork(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;
        (void)now;


        if (!_config.enabled)
            return;


        if (!_loopEnabled)
            return;


        ProcessConnections();

        Poll();
    }


    // ========================================================
    // SYNC
    //
    // HMI -> Buffer
    // Buffer -> HMI
    // ========================================================

    void Sync(DomoManager& manager, unsigned long now)
    {
        if (!_config.enabled)
            return;

        if (!_loopEnabled)
            return;

        SyncHMIToBuffer(manager, now);
    }


    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    bool enabled() const
    {
        return _config.enabled;
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
};

class SecuritySensorEngine {
private:

    // ------------------------------------------------------------
    // CALLBACKS
    // ------------------------------------------------------------
    static void onAnyAlarm(const std::string& zone,
                           SensorChannelType type,
                           const std::vector<Sensor*>& sensors)
    {
        LOG_IF("SecurityFrontend",
               "[GLOBAL] Alarm zone=%s type=%d sensors=%u",
               zone.c_str(), (int)type, (unsigned)sensors.size());
    }

    static void onZoneAlarm(const std::string& zone,
                            SensorChannelType type,
                            const std::vector<Sensor*>& sensors)
    {
        LOG_IF("SecurityFrontend",
               "[ZONE] Alarm in zone=%s type=%d sensors=%u",
               zone.c_str(), (int)type, (unsigned)sensors.size());
    }

    static void onTypeAlarm(const std::string& zone,
                            SensorChannelType type,
                            const std::vector<Sensor*>& sensors)
    {
        LOG_IF("SecurityFrontend",
               "[TYPE] Alarm type=%d in zone=%s sensors=%u",
               (int)type, zone.c_str(), (unsigned)sensors.size());
    }

    static void onZoneTypeAlarm(const std::string& zone,
                                SensorChannelType type,
                                const std::vector<Sensor*>& sensors)
    {
        LOG_IF("SecurityFrontend",
               "[ZONE+TYPE] Alarm zone=%s type=%d sensors=%u",
               zone.c_str(), (int)type, (unsigned)sensors.size());
    }

    // ------------------------------------------------------------
    // DYNAMIC CALLBACK REGISTRATION
    // ------------------------------------------------------------
    static void registerSecurityCallbacks() {

        auto& wired = SecurityOrchestrator::getWiredSensors();

        // Global
        SecurityOrchestrator::RegisterCallbackAny(onAnyAlarm);

        // Types
        static const SensorChannelType allTypes[] = {
            SensorChannelType::RT,
            SensorChannelType::H24,
            SensorChannelType::MASK,
            SensorChannelType::LEN
        };

        for (auto t : allTypes)
            SecurityOrchestrator::RegisterCallbackType(t, onTypeAlarm);

        // Dynamic zones
        for (const auto& entry : wired.GetZoneMap()) {
            const std::string& zoneName = entry.first;

            SecurityOrchestrator::RegisterCallbackZone(zoneName, onZoneAlarm);

            for (auto t : allTypes)
                SecurityOrchestrator::RegisterCallbackZoneType(zoneName, t, onZoneTypeAlarm);
        }

        LOG_DF("SecurityFrontend", "Dynamic security callbacks registered");
    }

public:

    // ------------------------------------------------------------
    // SETUP
    // ------------------------------------------------------------
    static void Setup(const FrontendConfig::Security& cfg) {
        SecurityOrchestrator::Setup(&cfg);

        registerSecurityCallbacks();

        LOG_DF("SecurityFrontend", "Security frontend setup completed");
    }

    // ------------------------------------------------------------
    // LOOP
    // ------------------------------------------------------------
    static bool Loop(unsigned long now) {
        return SecurityOrchestrator::Loop(now);
    }

    // ============================================================
    // ACCESSORS
    // ============================================================
    static SecurityOrchestrator::SystemManager& getSystem() { return SecurityOrchestrator::getSystem(); }
    static WiredSensorsManager& getWiredSensors() { return SecurityOrchestrator::getWiredSensors(); }

    // ============================================================
    // COMMANDS
    // ============================================================
    static void ApplySecurityCommands(int area) {
        SecurityOrchestrator::ApplySecurityCommands(area);
    }

    // ============================================================
    // DIAGNOSTICA
    // ============================================================
    class Diagnostic {
    public:
        static void FullReport() {
            SecurityOrchestrator::Diagnostic::FullReport();
        }
    };
};

class FrontendNetwork
{
public:

    struct ModbusTCP
    {
        EthernetClient client;
        ModbusTCPClient modbus{client};
    };

    struct MQTT
    {
        EthernetClient client;
        PubSubClient pubSub{client};
    };


    explicit FrontendNetwork(size_t mqttClientCount)
        : _mqttClientCount(mqttClientCount),
          _mqtt(new MQTT[mqttClientCount])
    {
    }


    ~FrontendNetwork()
    {
        delete[] _mqtt;
    }


    FrontendNetwork(const FrontendNetwork&) = delete;
    FrontendNetwork& operator=(const FrontendNetwork&) = delete;


    ModbusTCP& modbusTCP()
    {
        return _modbusTCP;
    }


    MQTT& mqtt(size_t index)
    {
        return _mqtt[index];
    }


    size_t mqttClientCount() const
    {
        return _mqttClientCount;
    }


private:

    ModbusTCP _modbusTCP;

    size_t _mqttClientCount;
    MQTT* _mqtt;
};

class TaskEngineBase
{
private:

    // ============================================================
    // AEE
    // ============================================================

    static AEERegistry* aee()
    {
        return &AEEEngine::getAEE();
    }


    // ============================================================
    // COMMUNICATION SCHEDULER
    // ============================================================

    static inline CommunicationScheduler communicationScheduler{3};


    // ============================================================
    // COMMUNICATION SERVICES
    // ============================================================

    static void Communication_Bridge(unsigned long now)
    {
        BridgeEngine::get().loop(now);
    }


    static void Communication_MQTT(unsigned long now)
    {
        MQTTEngine::Loop(now);
    }


    static void Communication_WebAPI(unsigned long now)
    {
        WebAPIEngine::Loop(now);
    }

    // ============================================================
    // TASK: HVAC
    // ============================================================

    static void Task_HVAC(
        DomoManager& manager,
        unsigned long now)
    {
        const auto& cfg =
            TaskEngineOrchestrator::getCfg().hvac;


        // --------------------------------------------------------
        // TIME
        // --------------------------------------------------------

        struct tm t;

        manager
            .getTimeManager()
            .getDateTime(t);


        HeatPumpController::HVACTime ht;

        ht.dayOfWeek = t.tm_wday;
        ht.hour      = t.tm_hour;
        ht.minute    = t.tm_min;


        // --------------------------------------------------------
        // ZONE TEMPERATURES
        // --------------------------------------------------------

        auto& buffer =
            manager.getBuffer();

        constexpr size_t MAX_ZONE_TEMPERATURES = 16;

        static float zoneTemps[MAX_ZONE_TEMPERATURES];


        const size_t zoneCount =
            (cfg.zoneCount < MAX_ZONE_TEMPERATURES)
                ? cfg.zoneCount
                : MAX_ZONE_TEMPERATURES;


        for (size_t i = 0; i < zoneCount; ++i)
        {
            const int area =
                cfg.zones[i].temperatureArea;

            zoneTemps[i] =
                buffer.getValueFast(area) / 10.0f;
        }


        // --------------------------------------------------------
        // GLOBAL TEMPERATURES / WINDOW
        // --------------------------------------------------------

        const float tInterna =
            cfg.readIndoorTemp
                ? cfg.readIndoorTemp()
                : 0.0f;


        const float tEsterna =
            cfg.readOutdoorTemp
                ? cfg.readOutdoorTemp()
                : 0.0f;


        const bool finestraAperta =
            cfg.readWindowOpen
                ? cfg.readWindowOpen()
                : false;


        // --------------------------------------------------------
        // HVAC ENGINE
        // --------------------------------------------------------

        HVACEngine::Loop(
            now,
            zoneTemps,
            tInterna,
            tEsterna,
            finestraAperta,
            ht
        );
    }


    // ============================================================
    // TASK: AVERAGES
    // ============================================================

    static void Task_Averages(
        DomoManager& manager,
        unsigned long now)
    {
        (void)now;

        const auto& cfg =
            TaskEngineOrchestrator::getCfg();


        auto& buffer =
            manager.getBuffer();

        auto& averages =
            manager.getAverages();

        // --------------------------------------------------------
        // GROUPS
        // --------------------------------------------------------

        for (size_t g = 0;
             g < cfg.averages.gruppiCount;
             ++g)
        {
            const auto& gruppo =
                cfg.averages.gruppi[g];


            // ----------------------------------------------------
            // MEASUREMENTS
            // ----------------------------------------------------

            for (size_t i = 0;
                 i < gruppo.count;
                 ++i)
            {
                const auto& s =
                    gruppo.sensori[i];


                const float raw =
                    buffer.getValueFast(s.area);


                const float value =
                    raw * s.factor;


                averages.addMeasurement(
                    gruppo.nome,
                    value
                );
            }


            // ----------------------------------------------------
            // AVERAGE
            // ----------------------------------------------------

            const float average =
                averages.groupAverage(
                    gruppo.nome
                );


            // ----------------------------------------------------
            // OUTPUT
            // ----------------------------------------------------

            if (gruppo.areaOut >= 0)
            {
                manager.forceInternalEvent(
                    gruppo.areaOut,
                    static_cast<long>(
                        average *
                        gruppo.outScale
                    )
                );
            }
        }
    } 

    // ============================================================
    // TASK: SENSORS / SECURITY
    // ============================================================

    static void Task_Sensors(
        DomoManager& manager,
        unsigned long now)
    {
        // --------------------------------------------------------
        // WAIT POWER-ON CYCLE
        // --------------------------------------------------------

        if (!manager.getPowerOnCycleCompleted())
            return;


        // --------------------------------------------------------
        // SECURITY ENGINE
        // --------------------------------------------------------

        const bool changed =
            SecuritySensorEngine::Loop(now);


        if (!changed)
            return;


        // --------------------------------------------------------
        // WRITE SECURITY STATUS
        // --------------------------------------------------------

        auto& sys =
            SecurityOrchestrator::getSystem();


        manager.forceInternalEvent(
            AREA_SECURITY_STATUS,
            sys.getBitmask()
        );
    }

    // ============================================================
    // TASK: AEE DUMP
    // ============================================================

    static void Task_AEE_Dump(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;
        (void)now;


        AEERegistry* registry =
            aee();


        if (!registry)
            return;


        StaticJsonDocument<1024> doc;


        registry->forEach(
            [&](AEEVariableBase* v)
            {
                if (v)
                    v->toJson(doc);
            }
        );


        String out;

        serializeJson(
            doc,
            out
        );


        LOG_IF(
            "AEE-DUMP",
            "%s",
            out.c_str()
        );
    }


    // ============================================================
    // TASK: COMMUNICATION
    // ============================================================

    static void Task_Communication(
        DomoManager& manager,
        unsigned long now)
    {
        // --------------------------------------------------------
        // HOT STANDBY
        // --------------------------------------------------------

        #if HOTSTANDBY_ENABLED

        if (!manager.isClusterMaster())
            return;

        #endif


        // --------------------------------------------------------
        // POWER-ON CYCLE
        // --------------------------------------------------------

        if (!manager.getPowerOnCycleCompleted())
            return;


        // --------------------------------------------------------
        // COMMUNICATION ROUND ROBIN
        // --------------------------------------------------------

        communicationScheduler.update(now);
    }


public:

    // ============================================================
    // SETUP
    //
    // Registra tutti i task standard del frontend.
    // ============================================================

    static void Setup(
        const FrontendConfig& cfg)
    {
        // --------------------------------------------------------
        // ORCHESTRATOR
        // --------------------------------------------------------

        TaskEngineOrchestrator::Setup(cfg);


        // --------------------------------------------------------
        // STANDARD TASKS
        // --------------------------------------------------------

        TaskEngineOrchestrator::AddTask(
            Task_Sensors,
            cfg.security.intervalMs,
            cfg.security.enabled
        );


        TaskEngineOrchestrator::AddTask(
            Task_HVAC,
            cfg.hvac.intervalMs,
            cfg.hvac.enabled
        );

        TaskEngineOrchestrator::AddTask(
            Task_Averages,
            cfg.averages.intervalMs,
            cfg.averages.enabled
        );


        /*
         * Questo task è sempre presente perché gestisce
         * il round-robin dei servizi di comunicazione.
         */

        TaskEngineOrchestrator::AddTask(
            Task_Communication,
            15,
            true
        );

        // --------------------------------------------------------
        // COMMUNICATION SERVICES
        // --------------------------------------------------------

        if (cfg.bridge.enabled)
        {
            communicationScheduler.add(
                Communication_Bridge,
                20,
                100,
                true
            );
        }


        if (cfg.mqtt.enabled)
        {
            communicationScheduler.add(
                Communication_MQTT,
                50,
                50,
                true
            );
        }


        if (cfg.webApi.enabled)
        {
            communicationScheduler.add(
                Communication_WebAPI,
                100,
                20,
                true
            );
        }
    }


    // ============================================================
    // ADD TASK
    //
    // Entry point per i task custom definiti dal frontend.
    // ============================================================

    static void AddTask(
        void (*fn)(DomoManager&, unsigned long),
        uint32_t intervalMs,
        bool enabled = true)
    {
        TaskEngineOrchestrator::AddTask(
            fn,
            intervalMs,
            enabled
        );
    }


    // ============================================================
    // LOOP
    // ============================================================

    static void Loop(
        DomoManager& manager,
        unsigned long now)
    {
        TaskEngineOrchestrator::Loop(
            manager,
            now
        );
    }


    // ============================================================
    // FRONTEND CYCLE
    // ============================================================

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


// ============================================================
//  Runtime comune dei frontend DomoManager.
//
//  Responsabilità:
//    - ButtonManager
//    - acquisizione del timestamp corrente
//    - gestione Developer Mode
//    - gestione transizione HotStandby MASTER / SLAVE
//
//  Il comportamento specifico del frontend viene delegato
//  ai virtual hook:
//    - onStartupButton()
//    - onButtonPressed()
//    - onDeveloperMode()
//    - onBecomeMaster()
//    - onBecomeSlave()
//
//  La classe NON gestisce:
//    - Ethernet
//    - HMI
//    - MQTT
//    - Bridge
//    - TaskEngine
//    - diagnostica applicativa
//    - DomoManager::loop()
// ============================================================

class FrontendRuntime
{
protected:

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

    unsigned long now = 0;

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

    void processButton()
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
                    SecurityOrchestrator::Diagnostic::FullReport();

                    SecuritySensorEngine::getSystem()
                        .DiagnosticReport();
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


        processButton();


        processDeveloperMode();


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
#endif
