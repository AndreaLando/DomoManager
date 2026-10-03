
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
#include "DMIntrusion.hpp"
#include "DMPlatformNetwork.hpp"

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

        if (DomoManager::instance)
        {
            DomoManager::instance->forceEvent(
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
        DMNetworkManager& network;
        DMNetworkManager::ProtocolId protocolId;

    public:
        DomoBridgePacer(
            DMNetworkManager& n,
            DMNetworkManager::ProtocolId id)
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
        DMNetworkManager& network,
        DMNetworkManager::ProtocolId protocolId)
    {
        delete pacer;

        pacer = new DomoBridgePacer(
            network,
            protocolId
        );

        bridge.setPacer(*pacer);
    }
};

class FrontendNetwork
{
public:

    static constexpr size_t MAX_MQTT_CLIENTS = 4;


    struct ModbusTCP
    {
        DMPlatform::Network::TCPClient client;

        ModbusTCPClient modbus{
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


private:

    ModbusTCP _modbusTCP;

    size_t _mqttClientCount = 0;

    MQTT _mqtt[
        MAX_MQTT_CLIENTS
    ];
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

    typedef void (*CommandReceivedCallback)(
        uint8_t clientIndex,
        const FrontendConfig::MQTT::Device* device,
        const FrontendConfig::MQTT::Mapping* mapping,
        long value
    );

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

    // ========================================================
    // RUNTIME MAPPING
    // ========================================================
    struct RuntimeCommandState
    {
        long value;
        unsigned long timestamp;
        bool initialized;

        RuntimeCommandState()
            : value(0)
            , timestamp(0)
            , initialized(false)
        {
        }
    };


    struct RuntimeMapping
    {
        uint8_t clientIndex;

        const FrontendConfig::MQTT::Client* client;
        const FrontendConfig::MQTT::Device* device;
        const FrontendConfig::MQTT::Mapping* mapping;

        RuntimeClient* runtimeClient;

        RuntimeCommandState command;

        long lastRaw;

        bool dirty;
        bool initialized;
    };

private:
    static uint16_t dirtyMappings[MAX_MAPPINGS];
    static uint16_t dirtyCount;

    static void EnqueueDirty(
        uint16_t mappingIndex
    )
    {
        RuntimeMapping* mappings =
            getMappings();

        RuntimeMapping& rm =
            mappings[mappingIndex];

        /*
        * Già schedulato.
        */
        if (rm.dirty)
            return;

        if (dirtyCount >= MAX_MAPPINGS)
        {
            LOG_WF(
                "MQTT",
                "Dirty queue piena"
            );

            return;
        }

        rm.dirty = true;

        dirtyMappings[
            dirtyCount++
        ] = mappingIndex;
    }

    static bool IsDuplicateCommand(
        const FrontendConfig::MQTT::Mapping* mapping,
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

            if (rm.mapping != mapping)
                continue;

            if (!rm.command.initialized)
            {
                rm.command.initialized = true;
                rm.command.value = value;
                rm.command.timestamp = millis();

                return false;
            }

            if (rm.command.value == value)
            {
                return true;
            }

            rm.command.value = value;
            rm.command.timestamp = millis();

            return false;
        }

        return false;
    }

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

    static DMNetworkManager* networkManager;
    static DMNetworkManager::ProtocolId mqttProtocolId;
    static uint8_t mqttSource;

    static CommandReceivedCallback commandReceivedCallback;
public:

    // ========================================================
    // SETUP
    // ========================================================

    static void Setup(
        DomoManager& dm,
        DMNetworkManager& netManager,
        FrontendNetwork& network,
        size_t mqttClientCount,
        const FrontendConfig::MQTT& cfg,
        DMNetworkManager::ProtocolId protocol,
        uint8_t eventSource
    )
    {
        /*
        * ------------------------------------------------------------
        * Global runtime state
        * ------------------------------------------------------------
        */

        manager = &dm;
        networkManager = &netManager;
        configuration = &cfg;

        commandReceivedCallback = nullptr;

        mqttProtocolId = protocol;
        mqttSource = eventSource;

        RuntimeClient* clients = getClients();
        RuntimeMapping* mappings = getMappings();

        /*
        * Reset runtime mappings.
        */
        memset(
            mappings,
            0,
            sizeof(RuntimeMapping) * MAX_MAPPINGS
        );

        /*
        * Reset runtime clients.
        *
        * Gli elementi sono statici e possono provenire
        * da una precedente inizializzazione.
        */
        for (size_t i = 0; i < MAX_CLIENTS; ++i)
        {
            clients[i] = RuntimeClient{};
        }

        clientCount = 0;
        mappingCount = 0;
        dirtyCount = 0;

        /*
        * ------------------------------------------------------------
        * Configuration validation
        * ------------------------------------------------------------
        */

        if (!cfg.enabled)
        {
            LOG_IF(
                "MQTT",
                "MQTT disabilitato"
            );
            return;
        }

        if (!cfg.clients || cfg.clientCount == 0)
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

        /*
        * ------------------------------------------------------------
        * Build runtime mappings
        * ------------------------------------------------------------
        *
        * La configurazione ora è:
        *
        * MQTT
        *  └── clients[]
        *       └── devices[]
        *            └── mappings[]
        *
        * Non esiste più un MQTT::devices[] globale.
        */
        BuildRuntimeMappings(cfg);

        /*
        * ------------------------------------------------------------
        * SocketManager
        * ------------------------------------------------------------
        *
        * Tutti i client MQTT utilizzano lo stesso SocketManager owner,
        * ma una resourceId diversa:
        *
        *     resourceId = indice del client MQTT
        */

        const SocketManager::OwnerId mqttSocketOwner =
            networkManager
                ->getProtocol(mqttProtocolId)
                .socketOwner;

        if (mqttSocketOwner < 0)
        {
            LOG_IF(
                "MQTT",
                "Socket owner MQTT non valido: protocol=%d",
                (int)mqttProtocolId
            );
            return;
        }

        /*
        * ------------------------------------------------------------
        * Number of clients to initialize
        * ------------------------------------------------------------
        */

        size_t maxClients = cfg.clientCount;

        if (maxClients > MAX_CLIENTS)
            maxClients = MAX_CLIENTS;

        if (maxClients > mqttClientCount)
            maxClients = mqttClientCount;

        /*
        * ------------------------------------------------------------
        * Initialize MQTT clients
        * ------------------------------------------------------------
        */

        for (size_t i = 0; i < maxClients; ++i)
        {
            const FrontendConfig::MQTT::Client& clientCfg =
                cfg.clients[i];

            /*
            * Client disabilitato:
            * semplicemente ignorato.
            */
            if (!clientCfg.enabled)
                continue;

            RuntimeClient& rc = clients[clientCount];

            /*
            * Runtime identity.
            */
            rc.index = (uint8_t)i;
            rc.cfg = &clientCfg;

            /*
            * Hardware MQTT resources.
            */
            rc.eth = &network.mqtt(i).client;
            rc.pubSub = &network.mqtt(i).pubSub;

            /*
            * Runtime state.
            */
            rc.mqtt = nullptr;
            rc.lastHeartbeat = 0;
            rc.initialized = false;

            /*
            * --------------------------------------------------------
            * MQTT engine
            * --------------------------------------------------------
            *
            * IMPORTANTE:
            * ogni MQTT instance riceve solamente i device
            * appartenenti al proprio Client.
            */
            rc.mqtt = new MQTT(
                *rc.eth,
                *rc.pubSub,
                (uint8_t)i,
                &clientCfg,
                clientCfg.devices,
                clientCfg.deviceCount
            );

            if (!rc.mqtt)
            {
                LOG_EF(
                    "MQTT",
                    "Client[%u] allocazione MQTT fallita",
                    (unsigned)i
                );

                continue;
            }

            /*
            * --------------------------------------------------------
            * Socket
            * --------------------------------------------------------
            */

            rc.socket.setup(
                networkManager,
                mqttSocketOwner,
                (int)rc.index,
                SocketManager::SocketKind::TCP_CLIENT
            );

            /*
            * --------------------------------------------------------
            * MQTT callbacks / backend
            * --------------------------------------------------------
            */

            rc.mqtt->setCommandCallback(
                MQTTCommandCallback
            );

            rc.mqtt->setBackend(
                clientCfg.backend
            );

            /*
            * --------------------------------------------------------
            * MQTT begin
            * --------------------------------------------------------
            */

            if (!rc.mqtt->begin())
            {
                LOG_IF(
                    "MQTT",
                    "Client[%u] begin fallito: %s",
                    (unsigned)i,
                    clientCfg.name
                        ? clientCfg.name
                        : "unnamed"
                );

                delete rc.mqtt;
                rc.mqtt = nullptr;

                continue;
            }

            /*
            * Client completamente inizializzato.
            */
            rc.initialized = true;

            LOG_IF(
                "MQTT",
                "Client[%u] inizializzato: %s "
                "backend=%u broker=%s:%u devices=%u",
                (unsigned)i,
                clientCfg.name
                    ? clientCfg.name
                    : "unnamed",
                (unsigned)clientCfg.backend,
                clientCfg.broker.toString().c_str(),
                (unsigned)clientCfg.port,
                (unsigned)clientCfg.deviceCount
            );

            ++clientCount;
        }

        BindRuntimeMappings();

        /*
        * ------------------------------------------------------------
        * Runtime initial values
        * ------------------------------------------------------------
        */

        InitializeRuntimeValues();

        /*
        * ------------------------------------------------------------
        * Setup completed
        * ------------------------------------------------------------
        */

        LOG_IF(
            "MQTT",
            "MQTT Setup completato: "
            "clients=%u mappings=%u protocol=%d",
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

        static bool mqttStartupReady = false;

        if (!mqttStartupReady)
        {
            if (now < 10000UL)
                return;

            mqttStartupReady = true;
        }

        RuntimeClient* clients =
            getClients();

        if (!networkManager->tryAcquire(
                mqttProtocolId,
                now))
        {
            LOG_WF(
                "MQTT",
                "tryAcquire NEGATO"
            );

            return;
        }

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

            // ----------------------------------------------------
            // SOCKET / RECONNECT
            // ----------------------------------------------------
            const bool isConnected = rc.mqtt->connected();
            
            if (!isConnected)
            {
                if (rc.mqtt->reconnectDue())
                {
                    if (rc.socket.acquire())
                    {
                        const bool ok =
                            rc.mqtt->reconnect();

                        if (!ok)
                        {
                            LOG_WF(
                                "MQTT",
                                "CLIENT[%u] release socket",
                                (unsigned)i
                            );

                            rc.socket.release();
                        }
                    }
                    else
                    {
                        LOG_WF(
                            "MQTT",
                            "CLIENT[%u] socket acquire FAILED",
                            (unsigned)i
                        );
                    }
                }
            }

            // ----------------------------------------------------
            // MQTT LOOP
            // ----------------------------------------------------

            if (rc.mqtt->connected())
            {
                MQTT* testMqtt = rc.mqtt;
                testMqtt->loop();
            }
            else
            {
                rc.socket.release();
            }
        }

        PublishDirty();

        networkManager->release(
            mqttProtocolId,
            now
        );

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

            if (!CanWrite(rm.mapping))
                continue;

            rm.lastRaw = value;

            EnqueueDirty(
                (uint16_t)i
            );
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
        if (!mapping)
            return;

        /*
        * Elimina eventi consecutivi identici.
        *
        * Molti telecomandi Zigbee e Zigbee2MQTT
        * possono pubblicare ripetutamente lo stesso
        * comando a breve distanza.
        */
        RuntimeMapping* mappings =
            getMappings();

        for (size_t i = 0;
            i < mappingCount;
            ++i)
        {
            RuntimeMapping& rm =
                mappings[i];

            if (rm.mapping != mapping)
                continue;

            if (
                rm.command.initialized
                &&
                rm.command.value == value
            )
            {
                return;
            }

            rm.command.initialized =
                true;

            rm.command.value =
                value;

            rm.command.timestamp =
                millis();

            break;
        }

        /*
        * Callback utente.
        */
        if (commandReceivedCallback)
        {
            commandReceivedCallback(
                clientIndex,
                device,
                mapping,
                value
            );
        }

        if (!manager)
            return;

        if (!CanRead(mapping))
            return;

        /*
        * MQTT -> sistema domotico.
        */
        manager->forceEvent(
            mapping->area,
            value,
            mqttSource
        );

        LOG_DF(
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


    // ========================================================
    // SETTER
    // ========================================================

    static void setCommandReceivedCallback(
        CommandReceivedCallback callback
    )
    {
        commandReceivedCallback = callback;
    }
private:

    // ========================================================
    // BUILD RUNTIME MAPPINGS
    // ========================================================
    static void BindRuntimeMappings()
    {
        RuntimeClient* clients =
            getClients();

        RuntimeMapping* mappings =
            getMappings();

        for (size_t m = 0;
            m < mappingCount;
            ++m)
        {
            RuntimeMapping& rm =
                mappings[m];

            rm.runtimeClient =
                nullptr;

            for (size_t c = 0;
                c < clientCount;
                ++c)
            {
                if (clients[c].index ==
                    rm.clientIndex)
                {
                    rm.runtimeClient =
                        &clients[c];

                    break;
                }
            }
        }
    }

    static void BuildRuntimeMappings(
        const FrontendConfig::MQTT& cfg
    )
    {
        RuntimeMapping* mappings =
            getMappings();

        mappingCount = 0;

        if (!cfg.clients)
            return;

        for (size_t c = 0;
            c < cfg.clientCount;
            ++c)
        {
            const FrontendConfig::MQTT::Client& client =
                cfg.clients[c];

            if (!client.enabled)
                continue;

            if (!client.devices)
                continue;

            for (size_t d = 0;
                d < client.deviceCount;
                ++d)
            {
                const FrontendConfig::MQTT::Device& device =
                    client.devices[d];

                for (size_t m = 0;
                    m < FrontendConfig::MQTT::Device::MAX_MAPPINGS;
                    ++m)
                {
                    const FrontendConfig::MQTT::Mapping& mapping =
                        device.mappings[m];

                    if (!mapping.field)
                        continue;

                    if (mappingCount >= MAX_MAPPINGS)
                    {
                        LOG_IF(
                            "MQTT",
                            "MAX_MAPPINGS raggiunto: %u",
                            (unsigned)MAX_MAPPINGS
                        );

                        return;
                    }

                    RuntimeMapping& rm =
                        mappings[mappingCount];

                    rm.clientIndex =
                        (uint8_t)c;

                    rm.client =
                        &client;

                    rm.device =
                        &device;

                    rm.mapping =
                        &mapping;

                    rm.runtimeClient =
                        nullptr;

                    /*
                    * Runtime stato comandi MQTT.
                    */
                    rm.command =
                        RuntimeCommandState();

                    /*
                    * Runtime publish.
                    */
                    rm.lastRaw =
                        0;

                    rm.dirty =
                        false;

                    rm.initialized =
                        false;

                    ++mappingCount;
                }
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
        RuntimeMapping* mappings =
            getMappings();

        uint16_t writePos = 0;

        for (
            uint16_t i = 0;
            i < dirtyCount;
            ++i
        )
        {
            const uint16_t mappingIndex =
                dirtyMappings[i];

            RuntimeMapping& rm =
                mappings[mappingIndex];

            RuntimeClient* rc =
                rm.runtimeClient;

            if (!rc)
            {
                dirtyMappings[
                    writePos++
                ] = mappingIndex;

                continue;
            }

            if (!rc->initialized)
            {
                dirtyMappings[
                    writePos++
                ] = mappingIndex;

                continue;
            }

            if (!rc->mqtt)
            {
                dirtyMappings[
                    writePos++
                ] = mappingIndex;

                continue;
            }

            if (rc->mqtt->state() != 0)
            {
                dirtyMappings[
                    writePos++
                ] = mappingIndex;

                continue;
            }

            if (
                rc->mqtt->publishMapping(
                    rm.device,
                    rm.mapping,
                    rm.lastRaw
                )
            )
            {
                rm.dirty = false;
                rm.initialized = true;
            }
            else
            {
                dirtyMappings[
                    writePos++
                ] = mappingIndex;
            }
        }

        dirtyCount = writePos;
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

DMNetworkManager* MQTTEngine::networkManager = nullptr;
DMNetworkManager::ProtocolId MQTTEngine::mqttProtocolId = -1;
uint8_t MQTTEngine::mqttSource = 255;

MQTTEngine::CommandReceivedCallback
    MQTTEngine::commandReceivedCallback = nullptr;

uint16_t MQTTEngine::dirtyMappings[
    MQTTEngine::MAX_MAPPINGS
];

uint16_t MQTTEngine::dirtyCount = 0;

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


            ModbusTCPServer& server =
                _context.modbusServers[client];


            // ------------------------------------------------
            // ArduinoModbus:
            //
            // 1 = SUCCESS
            // 0 = FAILURE
            // ------------------------------------------------

            const int result =
                server.holdingRegisterWrite(
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


    static constexpr int SOCKET_RESOURCE_MODBUS_BASE =
        1000;


    static constexpr int SOCKET_RESOURCE_CLIENT_BASE =
        2000;


    // ========================================================
    // RESOURCE STATE
    // ========================================================

    uint8_t _reservedModbusResources = 0;

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
            ModbusTCPServer
        > modbusServers;
    };


    Context _context;


    // ========================================================
    // RESOURCE ID HELPERS
    // ========================================================

    static int modbusResourceId(
        uint8_t index)
    {
        return
            SOCKET_RESOURCE_MODBUS_BASE +
            static_cast<int>(index);
    }


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
    // MODBUS RESOURCE ACQUIRE
    // ========================================================

    bool acquireModbusSocket(
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
            modbusResourceId(index);


        const int slot =
            _networkManager
                ->sockets()
                .acquire(
                    _socketOwner,
                    resourceId,
                    SocketManager::SocketKind::TCP_SERVER
                );


        if (slot < 0)
        {
            LOG_WF(
                "HMIEngine",
                "HMI Modbus resource unavailable: "
                "index=%u resource=%d",
                (unsigned)index,
                resourceId
            );

            return false;
        }


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
    // MODBUS RESOURCE RELEASE
    // ========================================================

    void releaseModbusSocket(
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
                modbusResourceId(index),
                SocketManager::SocketKind::TCP_SERVER
            );
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
    // RELEASE MODBUS RESOURCES
    // ========================================================

    void releaseModbusResources(
        uint8_t count)
    {
        if (count > _maxClients)
            count = _maxClients;


        for (uint8_t i = 0;
             i < count;
             ++i)
        {
            releaseModbusSocket(i);
        }


        _reservedModbusResources =
            0;
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


        _reservedModbusResources =
            0;
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


        DMPlatform::Network::DestroyTCPServer(
            _context.server
        );
    }


    // ========================================================
    // CREATE TCP SERVER
    // ========================================================

    bool createServer()
    {
        if (_context.server != nullptr)
        {
            LOG_EF(
                "HMIEngine",
                "HMI TCP server object already exists"
            );

            return false;
        }


        _context.server =
            DMPlatform::Network::CreateTCPServer(
                _config.port
            );


        if (_context.server == nullptr)
        {
            LOG_EF(
                "HMIEngine",
                "Unable to create TCP server: port=%u",
                (unsigned)_config.port
            );

            return false;
        }


        if (!DMPlatform::Network::StartTCPServer(
                *_context.server
            ))
        {
            LOG_EF(
                "HMIEngine",
                "Unable to start HMI TCP listener: port=%u",
                (unsigned)_config.port
            );


            destroyServerObject();

            return false;
        }


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
            ModbusTCPServer& server =
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
                server.configureHoldingRegisters(
                    0,
                    static_cast<int>(
                        _registerCount
                    )
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
    // ========================================================

    bool SetupServer()
    {
        _activeClients = 0;

        _reservedModbusResources = 0;

        _serverResourceAcquired = false;


        // ====================================================
        // LOGICAL CLIENT SLOTS
        // ====================================================

        _context.clients.clear();

        _context.modbusServers.clear();


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
        // MODBUS LOGICAL RESOURCES
        // ====================================================

        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            if (!acquireModbusSocket(i))
            {
                LOG_WF(
                    "HMIEngine",
                    "Unable to reserve HMI Modbus resource: "
                    "index=%u",
                    (unsigned)i
                );


                releaseModbusResources(
                    _reservedModbusResources
                );


                destroyServerObject();

                releaseServerSocket();

                return false;
            }


            ++_reservedModbusResources;
        }


        // ====================================================
        // MODBUS CONTEXTS
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


            releaseModbusResources(
                _reservedModbusResources
            );


            destroyServerObject();

            releaseServerSocket();

            return false;
        }


        // ====================================================
        // SUCCESS
        // ====================================================

        const unsigned socketDemand =
            1u +
            (
                2u *
                static_cast<unsigned>(
                    _maxClients
                )
            );


        LOG_IF(
            "HMIEngine",
            "HMI Modbus TCP started: "
            "port=%u maxClients=%u registers=%u "
            "socketDemand=%u",
            (unsigned)_config.port,
            (unsigned)_maxClients,
            (unsigned)_registerCount,
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
    // ========================================================

    void acceptNewClient()
    {
        if (_context.server == nullptr)
            return;


        DMPlatform::Network::TCPClient newClient =
            DMPlatform::Network::AcceptTCPClient(
                *_context.server
            );


        if (!newClient)
            return;


        // ====================================================
        // FIND FREE LOGICAL SLOT
        // ====================================================

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


            // ------------------------------------------------
            // SocketManager = tracking/resource accounting.
            //
            // Preserviamo la semantica originale:
            // un failure di acquire non forza il disconnect
            // del TCP client.
            // ------------------------------------------------

            const bool tracked =
                acquireClientSocket(i);


            if (!tracked)
            {
                LOG_WF(
                    "HMIEngine",
                    "HMI client %u accepted without "
                    "SocketManager resource tracking",
                    (unsigned)i
                );
            }


            // ------------------------------------------------
            // Salva il client nello slot stabile.
            // ------------------------------------------------

            slot =
                newClient;


            // ------------------------------------------------
            // Associa il client al Modbus context.
            // ------------------------------------------------

            _context.modbusServers[i].accept(
                slot
            );


            ++_activeClients;


            LOG_IF(
                "HMIEngine",
                "HMI client %u connected - active=%u",
                (unsigned)i,
                (unsigned)_activeClients
            );


            return;
        }


        // ====================================================
        // NO LOGICAL SLOT
        // ====================================================

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

    void ProcessConnections()
    {
        cleanupDisconnectedClients();

        acceptNewClient();
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


            ModbusTCPServer& server =
                _context.modbusServers[client];


            for (uint16_t area = 0;
                 area < _registerCount;
                 ++area)
            {
                const long rawValue =
                    server.holdingRegisterRead(
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

                manager.getEventManager().push(
                    static_cast<int>(area),
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
          _running(false),
          _protocolId(-1),
          _registerCount(0),
          _eventSource(0),
          _networkManager(nullptr),
          _socketOwner(-1),
          _reservedModbusResources(0),
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


        _reservedModbusResources =
            0;


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


        ProcessConnections();

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
    // LAST ALARM CALLBACK STATE
    //
    // La callback viene notificata solo quando cambia lo stato
    // security osservabile dal frontend.
    //
    // ============================================================

    static inline bool alarmCallbackStateValid = false;

    static inline bool lastAlarmEngaged = false;
    static inline bool lastAlarmActive = false;

    static inline uint64_t lastCurrentAlarmMask = 0;
    static inline uint64_t lastEffectiveAlarmMask = 0;
    static inline uint64_t lastSystemBitmask = 0;


    // ============================================================
    // SECURITY ALARM CALLBACK
    //
    // Callback verso il frontend.
    //
    // SecurityEngine NON conosce relay, sirene o altre uscite.
    //
    // Il frontend riceve:
    //
    // engaged           = centrale armata
    // active            = esiste un allarme effettivo
    // currentAlarmMask  = allarmi attualmente presenti
    // effectiveAlarmMask= allarmi non silenziati
    //
    // ============================================================

    using AlarmCallback =
        void (*)(
            bool engaged,
            bool active,
            uint64_t currentAlarmMask,
            uint64_t effectiveAlarmMask,
            uint64_t systemBitmask
        );

    static inline AlarmCallback alarmCallback = nullptr;

    static inline bool hmiInitialSyncPending = true;
    static inline bool reportOnChange = false;

    // ============================================================
    // INSTANCE / CONTEXT
    // ============================================================

    // DomoManager utilizzato dal frontend security per:
    // - pubblicare gli eventi sull'EventManager
    // - pubblicare lo stato sulla statusArea
    static inline DomoManager* manager = nullptr;


    // Configurazione security del Frontend.
    // Contiene:
    // - configurazione sensori
    // - statusArea
    // - panelCommandArea
    // - eventArea
    static inline FrontendConfig::Security* config = nullptr;


    // Source già registrato dal DomoManagerFrontendEngine
    // nell'EventManager.
    //
    // SECURITY non possiede il source: lo utilizza.
    static inline uint8_t securitySource = 255;


    // Engine inizializzato correttamente.
    static inline bool initialized = false;

    // ============================================================
    // CHANGE FLAGS
    //
    // Accumula le variazioni rilevate durante il ciclo security.
    //
    // Più flag possono essere attivi contemporaneamente.
    // ============================================================

    static inline uint8_t changeFlags =
        SecurityOrchestrator::CHANGE_NONE;

    // ============================================================
    // SECURITY EVENT ENCODER
    //
    // Conversione dell'evento semantico:
    //
    // AlarmPanelInterface::Event
    //
    // nel formato:
    //
    // EventManager::Event
    //
    // value:
    //
    // bits  0..7   = EventType
    // bits  8..15  = zone
    // bits 16..23  = partition
    //
    // 0xFF = valore non disponibile (-1)
    //
    // description e timestamp restano disponibili nell'eventLog
    // del DomoManagerAlarmPanel, mentre EventManager trasporta
    // il codice compatto area/value/source.
    // ============================================================

    static long EncodeEvent(
        const AlarmPanelInterface::Event& event)
    {
        const uint32_t type =
            static_cast<uint32_t>(
                event.type
            ) & 0xFFu;


        const uint32_t zone =
            static_cast<uint32_t>(
                event.zone < 0
                    ? 0xFF
                    : event.zone
            ) & 0xFFu;


        const uint32_t partition =
            static_cast<uint32_t>(
                event.partition < 0
                    ? 0xFF
                    : event.partition
            ) & 0xFFu;


        const uint32_t value =
            type |
            (zone << 8) |
            (partition << 16);


        return static_cast<long>(
            value
        );
    }


    // ============================================================
    // ALARM PANEL EVENT CALLBACK
    //
    // Percorso:
    //
    // AlarmPanel
    //      ↓
    // SecurityEngine
    //      ↓
    // EventManager
    //
    // SecurityEngine genera l'evento nel sistema frontend.
    // HMI / AEE / MQTT rimangono consumer separati dell'EventManager.
    // ============================================================

    static void AlarmPanelAlarmCallback(
        const AlarmPanelInterface::Event& event)
    {
        if (!initialized)
            return;

        if (!manager)
            return;

        if (!config)
            return;

        if (config->eventArea < 0)
            return;

        if (securitySource == 255)
            return;


        const long value =
            EncodeEvent(event);


        manager->getEventManager().push(
            config->eventArea,
            value,
            securitySource
        );


        changeFlags |=
            SecurityOrchestrator::CHANGE_PANEL;


        LOG_IF(
            "SECURITY",
            "ALARM type=%d alarmType=%d zone=%d partition=%d area=%d value=%ld",
            static_cast<int>(event.type),
            static_cast<int>(event.alarmType),
            event.zone,
            event.partition,
            config->eventArea,
            value
        );
    }

    static void AlarmPanelEventCallback(
        const AlarmPanelInterface::Event& event)
    {
        if (!initialized)
            return;

        if (!manager)
            return;

        if (!config)
            return;

        if (config->eventArea < 0)
            return;

        if (securitySource == 255)
            return;


        const long value =
            EncodeEvent(event);


        manager->getEventManager().push(
            config->eventArea,
            value,
            securitySource
        );


        changeFlags |=
            SecurityOrchestrator::CHANGE_PANEL;


        LOG_IF(
            "SECURITY",
            "EVENT type=%d zone=%d partition=%d area=%d value=%ld",
            static_cast<int>(event.type),
            event.zone,
            event.partition,
            config->eventArea,
            value
        );
    }


    static void UpdateSecurityHmi(bool force = false)
    {
        if (!initialized || !config || !manager)
            return;


        const uint8_t changes =
            changeFlags;


        if (changes == SecurityOrchestrator::CHANGE_NONE &&
            !force)
        {
            return;
        }


        auto& buffer =
            manager->getBuffer();

        auto& hmi =
            SecurityHmiInterface::instance();


        // ============================================================
        // SENSORI
        // ============================================================
        //
        // SENSOR STATUS AREA
        //
        // bit  0 = RT active
        // bit  1 = RT alarm
        //
        // bit  2 = H24 active
        // bit  3 = H24 alarm
        //
        // bit  4 = LEN active
        // bit  5 = LEN alarm
        //
        // bit  6 = MASK active
        // bit  7 = MASK alarm
        //
        // bit  8 = RT inhibit
        // bit  9 = H24 inhibit
        // bit 10 = LEN inhibit
        // bit 11 = MASK inhibit
        //
        // bit 12 = alarmOut
        //
        // bit 13 = ESCLUSO
        //          true  = sensor disabled
        //          false = sensor enabled
        //
        // bit 14 = RT_MEM
        // bit 15 = H24_MEM
        //
        // ============================================================

        if (force ||
            (changes &
            (SecurityOrchestrator::CHANGE_SENSOR |
            SecurityOrchestrator::CHANGE_COMMAND)))
        {
            auto& ws =
                SecurityOrchestrator::getWiredSensors();

            const auto* cfg =
                ws.GetConfig();


            if (cfg)
            {
                for (size_t i = 0;
                    i < ws.Count();
                    ++i)
                {
                    const int area =
                        cfg[i].statusArea;


                    if (area < 0)
                        continue;


                    SecurityHmiInterface::SensorState state;


                    if (!hmi.getSensorState(
                            i,
                            state))
                    {
                        continue;
                    }


                    long value = 0;


                    // ------------------------------------------------
                    // CHANNELS
                    // ------------------------------------------------

                    for (size_t b = 0;
                        b < 4;
                        ++b)
                    {
                        bitWrite(
                            value,
                            b,
                            state.active[b]
                        );


                        bitWrite(
                            value,
                            4 + b,
                            state.alarm[b]
                        );


                        bitWrite(
                            value,
                            8 + b,
                            state.inhibit[b]
                        );
                    }


                    // ------------------------------------------------
                    // SENSOR ALARM OUTPUT
                    // ------------------------------------------------

                    bitWrite(
                        value,
                        12,
                        state.alarmOut
                    );


                    // ------------------------------------------------
                    // SENSOR EXCLUDED
                    // ------------------------------------------------

                    bitWrite(
                        value,
                        13,
                        !state.enabled
                    );


                    // ------------------------------------------------
                    // SENSOR MEMORY
                    // ------------------------------------------------

                    bitWrite(
                        value,
                        14,
                        state.rtMem
                    );


                    bitWrite(
                        value,
                        15,
                        state.h24Mem
                    );


                    // ------------------------------------------------
                    // WRITE
                    // ------------------------------------------------

                    if (force ||
                        buffer.getValueFast(area) != value)
                    {
                        manager->forceInternalEvent(
                            area,
                            value
                        );
                    }
                }
            }
        }


        // ============================================================
        // ZONE
        // ============================================================
        //
        // bit 0 = ALLARME RT
        // bit 1 = ALLARME H24
        // bit 2 = ANOMALIA
        // bit 3 = ESCLUSA
        // bit 4 = MEM RT
        // bit 5 = MEM H24
        //
        // ============================================================

        if (force ||
            (changes &
            SecurityOrchestrator::CHANGE_ZONE))
        {
            auto& ws =
                SecurityOrchestrator::getWiredSensors();

            auto& zones =
                ws.Zones();


            for (size_t i = 0;
                i < ws.GetZoneCount();
                ++i)
            {
                const char* zoneName =
                    ws.GetZoneName(i);


                if (!zoneName)
                    continue;


                const int area =
                    zones.GetZoneStatusArea(
                        zoneName
                    );


                if (area < 0)
                    continue;


                SecurityHmiInterface::ZoneState state;


                if (!hmi.getZoneState(
                        i,
                        state))
                {
                    continue;
                }


                long value = 0;


                bitWrite(
                    value,
                    0,
                    state.alarm
                );


                bitWrite(
                    value,
                    1,
                    state.alarmH24
                );


                bitWrite(
                    value,
                    2,
                    state.trouble
                );


                bitWrite(
                    value,
                    3,
                    state.bypassed
                );


                bitWrite(
                    value,
                    4,
                    state.rtMem
                );


                bitWrite(
                    value,
                    5,
                    state.h24Mem
                );


                if (force ||
                    buffer.getValueFast(area) != value)
                {
                    manager->forceInternalEvent(
                        area,
                        value
                    );
                }
            }
        }


        // ============================================================
        // SISTEMA / CENTRALE
        // ============================================================
        //
        // WORD 1
        //
        // bit  0 = intrusion REALTIME
        // bit  1 = intrusion H24 REALTIME
        // bit  2 = flood REALTIME
        // bit  3 = smoke REALTIME
        // bit  4 = windows open REALTIME
        // bit  5 = doors open REALTIME
        //
        // bit  6 = intrusion MEM
        // bit  7 = intrusion H24 MEM
        // bit  8 = flood MEM
        // bit  9 = smoke MEM
        // bit 10 = windows open MEM
        // bit 11 = doors open MEM
        //
        // bit 12 = global tamper
        //
        //
        //
        // WORD 2
        //
        // bit 0 = connected
        // bit 1 = communication fault
        // bit 2 = channel supervised
        // bit 3 = ready
        // bit 4 = global trouble
        // bit 5 = ready for arm
        //
        //
        // ARM STATE
        //
        // armState viene scritto su una area dedicata.
        //
        // ============================================================

        if (force ||
            (changes &
            (SecurityOrchestrator::CHANGE_SYSTEM |
            SecurityOrchestrator::CHANGE_PANEL  |
            SecurityOrchestrator::CHANGE_COMM   |
            SecurityOrchestrator::CHANGE_COMMAND)))
        {
            SecurityHmiInterface::PanelState state;


            if (hmi.getPanelState(
                    state,
                    0))
            {
                // ========================================================
                // WORD 1 - SYSTEM
                // ========================================================

                const int statusArea =
                    config->statusArea;


                if (statusArea >= 0)
                {
                    const uint16_t word1 =
                        static_cast<uint16_t>(
                            state.systemBitmask
                        );


                    if (force ||
                        buffer.getValueFast(statusArea) !=
                            static_cast<long>(word1))
                    {
                        manager->forceInternalEvent(
                            statusArea,
                            static_cast<long>(word1)
                        );
                    }
                }


                // ========================================================
                // WORD 2 - PANEL STATUS
                // ========================================================

                const int statusArea2 =
                    config->statusArea2;


                if (statusArea2 >= 0)
                {
                    uint16_t word2 = 0;


                    bitWrite(
                        word2,
                        0,
                        state.connected
                    );


                    bitWrite(
                        word2,
                        1,
                        state.communicationFault
                    );


                    bitWrite(
                        word2,
                        2,
                        state.channelSupervised
                    );


                    bitWrite(
                        word2,
                        3,
                        state.ready
                    );


                    bitWrite(
                        word2,
                        4,
                        state.globalTrouble
                    );


                    bitWrite(
                        word2,
                        5,
                        state.readyForArm
                    );


                    if (force ||
                        buffer.getValueFast(statusArea2) !=
                            static_cast<long>(word2))
                    {
                        manager->forceInternalEvent(
                            statusArea2,
                            static_cast<long>(word2)
                        );
                    }
                }


                // ========================================================
                // ARM STATE AREA
                // ========================================================

                const int armStateArea =
                    config->armStateArea;


                if (armStateArea >= 0)
                {
                    const long armState =
                        static_cast<long>(
                            static_cast<uint8_t>(
                                state.armState
                            )
                        );


                    if (force ||
                        buffer.getValueFast(armStateArea) != armState)
                    {
                        manager->forceInternalEvent(
                            armStateArea,
                            armState
                        );
                    }
                }
            }
        }
    }

    // ============================================================
    // UPDATE ALARM CALLBACK
    //
    // Legge lo stato corrente della centrale e degli allarmi.
    //
    // La callback viene chiamata solo se registrata.
    //
    // ============================================================

    static void UpdateAlarmCallback()
    {
        SecurityHmiInterface::PanelState panelState;

        auto& hmi = SecurityHmiInterface::instance();

        if (!hmi.getPanelState(panelState, 0))
            return;

        const bool engaged =
            panelState.armState !=
            AlarmPanelInterface::ArmState::DISARMED;

        const uint64_t systemBitmask =
            static_cast<uint64_t>(panelState.systemBitmask);

        const uint64_t currentAlarmMask =
            SecurityOrchestrator::getCurrentAlarmMask();

        auto& alarmPanel =
            DomoManagerAlarmPanel::instance();

        // Non sincronizzare qui silencedAlarmMask.
        //
        // La tacitazione viene gestita da silenceAlarm().
        // La memoria di tacitazione viene rimossa da
        // synchronizeAlarmSilence() quando l'allarme non è più presente.

        const uint64_t effectiveAlarmMask =
            alarmPanel.getEffectiveAlarmMask();

        const bool active =
            effectiveAlarmMask != 0;

        const bool changed =
            !alarmCallbackStateValid ||
            engaged != lastAlarmEngaged ||
            active != lastAlarmActive ||
            currentAlarmMask != lastCurrentAlarmMask ||
            effectiveAlarmMask != lastEffectiveAlarmMask ||
            systemBitmask != lastSystemBitmask;

        if (!changed)
            return;

        lastAlarmEngaged =
            engaged;

        lastAlarmActive =
            active;

        lastCurrentAlarmMask =
            currentAlarmMask;

        lastEffectiveAlarmMask =
            effectiveAlarmMask;

        lastSystemBitmask =
            systemBitmask;

        alarmCallbackStateValid =
            true;

        if (alarmCallback)
        {
            alarmCallback(
                engaged,
                active,
                currentAlarmMask,
                effectiveAlarmMask,
                systemBitmask
            );
        }

        LOG_IF(
            "SECURITY",
            "CALLBACK CALC: "
            "engaged=%d "
            "active=%d "
            "current=0x%016llX "
            "effective=0x%016llX "
            "silenced=0x%016llX",
            engaged ? 1 : 0,
            active ? 1 : 0,
            (unsigned long long)currentAlarmMask,
            (unsigned long long)effectiveAlarmMask,
            (unsigned long long)alarmPanel.getSilencedAlarmMask()
        );
    }
    

public:
    // ============================================================
    // SET ALARM CALLBACK
    //
    // Il frontend registra qui la propria callback.
    //
    // Esempio:
    //
    // SecurityEngine::setAlarmCallback(
    //     SecurityAlarmCallback
    // );
    //
    // ============================================================

    static void setAlarmCallback(
        AlarmCallback callback)
    {
        alarmCallback =
            callback;


        // ------------------------------------------------------------
        // Una nuova callback deve ricevere il primo stato disponibile.
        // ------------------------------------------------------------

        alarmCallbackStateValid =
            false;


        lastAlarmEngaged =
            false;

        lastAlarmActive =
            false;

        lastCurrentAlarmMask =
            0;

        lastEffectiveAlarmMask =
            0;

        lastSystemBitmask =
            0;


        LOG_IF(
            "SECURITY",
            "Alarm callback %s",
            alarmCallback
                ? "REGISTERED"
                : "CLEARED"
        );
    }


    // ============================================================
    // SETUP
    //
    // Il source SECURITY viene creato dal
    // DomoManagerFrontendEngine, insieme agli altri source
    // dell'EventManager.
    //
    // SecurityEngine riceve semplicemente il source già creato.
    // ============================================================

    static bool Setup(
        DomoManager& dm,
        FrontendConfig::Security& cfg,
        uint8_t source)
    {
        manager = &dm;

        config = &cfg;

        securitySource = source;

        initialized = false;

        hmiInitialSyncPending = true;

        reportOnChange =
            cfg.reportOnChange;

        changeFlags =
            SecurityOrchestrator::CHANGE_NONE;


        // ============================================================
        // SECURITY DISABLED
        // ============================================================

        if (!cfg.enabled)
        {
            LOG_IF(
                "SECURITY",
                "Security engine disabled"
            );

            return false;
        }


        // ============================================================
        // VALID SOURCE
        // ============================================================

        if (securitySource == 255)
        {
            LOG_EF(
                "SECURITY",
                "Invalid EventManager source"
            );

            return false;
        }


        // ============================================================
        // ORCHESTRATOR
        // ============================================================

        SecurityOrchestrator::Setup(
            &cfg
        );


        // ============================================================
        // ALARM PANEL CALLBACKS
        // ============================================================

        DomoManagerAlarmPanel::instance()
            .setAlarmCallback(
                AlarmPanelAlarmCallback
            );


        DomoManagerAlarmPanel::instance()
            .setEventCallback(
                AlarmPanelEventCallback
            );


        // ============================================================
        // INITIALIZED
        // ============================================================

        initialized = true;


        LOG_IF(
            "SECURITY",
            "Security engine initialized "
            "panelCommandArea=%d "
            "statusArea=%d "
            "armStateArea=%d "
            "eventArea=%d "
            "source=%d "
            "reportOnChange=%d",
            cfg.panelCommandArea,
            cfg.statusArea,
            cfg.armStateArea,
            cfg.eventArea,
            securitySource,
            reportOnChange ? 1 : 0
        );


        return true;
    }


    // ============================================================
    // LOOP
    //
    // Raccoglie tutti i cambiamenti:
    //
    // - reader / sensori
    // - zone
    // - stato sistema
    // - eventi pannello
    // - comunicazione
    // - comandi
    //
    // Il report viene eseguito una sola volta quando esiste
    // almeno una variazione.
    // ============================================================

    static bool Loop(
        DomoManager& dm,
        unsigned long now)
    {
        if (!initialized)
            return false;

        if (!config || !config->enabled)
            return false;

        if (!dm.getPowerOnCycleCompleted())
            return false;


        // =========================================================
        // SECURITY ORCHESTRATOR
        //
        // Questo è l'UNICO punto in cui viene eseguito Loop().
        // =========================================================

        const uint8_t changes =
            SecurityOrchestrator::Loop(now);

        changeFlags |=
            changes;


        DomoManagerAlarmPanel::instance()
        .synchronizeAlarmSilence(
            SecurityOrchestrator::getCurrentAlarmMask()
        );


        // =========================================================
        // HMI
        //
        // Prima aggiorniamo la HMI, poi leggiamo PanelState
        // nel callback.
        // =========================================================

        const bool forceHmi =
            hmiInitialSyncPending;

        UpdateSecurityHmi(
            forceHmi
        );

        if (forceHmi)
            hmiInitialSyncPending = false;


        // =========================================================
        // ALARM CALLBACK
        //
        // Ora PanelState è aggiornato.
        // =========================================================

        UpdateAlarmCallback();


        // =========================================================
        // NOTHING CHANGED
        // =========================================================

        if (changeFlags ==
            SecurityOrchestrator::CHANGE_NONE)
        {
            return false;
        }


        // =========================================================
        // REPORT
        // =========================================================

        if (reportOnChange &&
            (changeFlags &
            (SecurityOrchestrator::CHANGE_ZONE |
            SecurityOrchestrator::CHANGE_SYSTEM |
            SecurityOrchestrator::CHANGE_PANEL |
            SecurityOrchestrator::CHANGE_COMM |
            SecurityOrchestrator::CHANGE_COMMAND)))
        {
            SecurityOrchestrator::
                Diagnostic::FullReport();
        }


        changeFlags =
            SecurityOrchestrator::CHANGE_NONE;


        return true;
    }


    // ============================================================
    // FRONTEND COMMAND
    //
    // Punto unico di ingresso dei comandi security provenienti
    // dal frontend.
    //
    // Non distingue il chiamante:
    // HMI / AEE / MQTT devono semplicemente passare:
    //
    //     area + value + now
    //
    // SecurityEngine stabilisce se l'area appartiene:
    //
    // 1) al pannello
    // 2) a un singolo sensore
    // 3) a nessun comando security
    //
    // ------------------------------------------------------------
    //
    // PANEL
    //     panelCommandArea
    //          ↓
    //     ApplyPanelCommand()
    //
    // SENSOR
    //     WiredSensorConfig::cmdArea
    //          ↓
    //     ApplySecurityCommand()
    //
    // ============================================================

    static bool ApplyCommand(
        int area,
        long value,
        unsigned long now)
    {
        // ============================================================
        // ENTRY
        // ============================================================

        LOG_IF(
            "SECURITY",
            "ApplyCommand ENTRY: "
            "area=%d value=%ld now=%lu",
            area,
            value,
            now
        );


        if (!initialized)
            return false;


        if (!config)
            return false;


        // ============================================================
        // DEBUG CONFIGURATION
        // ============================================================

        LOG_IF(
            "SECURITY",
            "ApplyCommand CHECK: "
            "area=%d "
            "value=0x%08lX "
            "panelCommandArea=%d "
            "panelBitCommandArea=%d",
            area,
            (unsigned long)value,
            config->panelCommandArea,
            config->panelBitCommandArea
        );


        // ============================================================
        // PANEL BIT COMMAND
        //
        // Area generica a bit.
        //
        // Il comando viene elaborato come impulso.
        // Dopo l'elaborazione il valore dell'area viene riportato
        // a 0 senza generare un nuovo evento.
        // ============================================================

        if (config->panelBitCommandArea >= 0 &&
            area == config->panelBitCommandArea)
        {
            LOG_IF(
                "SECURITY",
                "PANEL BIT AREA MATCH: "
                "area=%d value=0x%08lX",
                area,
                (unsigned long)value
            );


            const bool handled =
                SecurityOrchestrator::
                    ApplyPanelBitCommand(
                        area,
                        value
                    );


            if (handled)
            {
                changeFlags |=
                    SecurityOrchestrator::CHANGE_COMMAND;


                LOG_IF(
                    "SECURITY",
                    "PANEL BIT CMD: "
                    "area=%d value=0x%08lX",
                    area,
                    (unsigned long)value
                );


                // ----------------------------------------------------
                // RESET IMPULSO
                //
                // Il reset è SILENT:
                // 1 -> 0 non deve generare un nuovo comando.
                // ----------------------------------------------------

                if (manager)
                {
                    manager->getBuffer().WriteElement(
                        area,
                        0,
                        false,
                        now
                    );


                    LOG_IF(
                        "SECURITY",
                        "PANEL BIT CMD RESET: "
                        "area=%d value=0",
                        area
                    );
                }
                else
                {
                    LOG_EF(
                        "SECURITY",
                        "PANEL BIT CMD RESET FAILED: manager=null"
                    );
                }
            }
            else
            {
                LOG_WF(
                    "SECURITY",
                    "Panel bit command rejected "
                    "area=%d value=0x%08lX",
                    area,
                    (unsigned long)value
                );
            }


            // L'area è riservata ai panel bit commands.
            return true;
        }


        // ============================================================
        // ALARM PANEL COMMAND
        //
        // Comando a valore:
        //
        // 0 = DISARM
        // 1 = ARM_AWAY
        // 2 = ARM_STAY
        // 3 = ARM_NIGHT
        // ============================================================

        if (config->panelCommandArea >= 0 &&
            area == config->panelCommandArea)
        {
            const bool handled =
                SecurityOrchestrator::
                    ApplyPanelCommand(
                        area,
                        value
                    );


            if (handled)
            {
                changeFlags |=
                    SecurityOrchestrator::CHANGE_COMMAND;


                LOG_IF(
                    "SECURITY",
                    "PANEL CMD area=%d value=%ld",
                    area,
                    value
                );
            }
            else
            {
                LOG_WF(
                    "SECURITY",
                    "Panel command rejected "
                    "area=%d value=%ld",
                    area,
                    value
                );
            }


            // L'area è riservata al pannello.
            return true;
        }


        // ============================================================
        // SINGLE SENSOR
        // ============================================================

        if (SecurityOrchestrator::
                ApplySecurityCommand(
                    area,
                    value
                ))
        {
            changeFlags |=
                SecurityOrchestrator::CHANGE_COMMAND;


            LOG_IF(
                "SECURITY",
                "SENSOR CMD area=%d value=%ld",
                area,
                value
            );


            return true;
        }


        // ============================================================
        // NOT A SECURITY COMMAND
        // ============================================================

        return false;
    }


    // ============================================================
    // DIRECT STATUS ACCESS
    // ============================================================

    static long GetStatus()
    {
        if (!initialized)
            return 0;


        return SecurityOrchestrator::
            getSystem()
            .getBitmask();
    }


    // ============================================================
    // EVENT SOURCE
    // ============================================================

    static uint8_t getSource()
    {
        return securitySource;
    }


    // ============================================================
    // STATE
    // ============================================================

    static bool enabled()
    {
        return initialized &&
               config &&
               config->enabled;
    }
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
        SecurityEngine::Loop(
            manager,
            now
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
