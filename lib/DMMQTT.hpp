#ifndef DMMQTT_HPP
#define DMMQTT_HPP

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
   
#include <Arduino.h>
#include <PubSubClient.h>
#include <Ethernet.h>


#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"



// ============================================================
// MQTT LOW LEVEL / BACKENDS
// ============================================================

class MQTT {
public:

    typedef void (*CommandCallback)(
        uint8_t clientIndex,
        const FrontendConfig::MQTT::Device* device,
        const FrontendConfig::MQTT::Mapping* mapping,
        long value
    );

    class BackendBase
    {
    public:

        virtual ~BackendBase() {}

        virtual void begin(
            MQTT* mqttInstance
        ) = 0;

        virtual void loop(
            MQTT* mqttInstance
        ) = 0;

        virtual bool publish(
            MQTT* mqttInstance,
            const FrontendConfig::MQTT::Device* device,
            const FrontendConfig::MQTT::Mapping* mapping,
            long value
        ) = 0;

        virtual void subscribe(
            MQTT* mqttInstance
        ) = 0;

        virtual bool decodeMessage(
            MQTT* mqttInstance,
            const char* topic,
            const char* payload,
            size_t len,
            const FrontendConfig::MQTT::Device*& device,
            const FrontendConfig::MQTT::Mapping*& mapping,
            long& value
        ) = 0;

        virtual const char* name() const = 0;
    };

    // ========================================================
    // HOME ASSISTANT BACKEND
    // ========================================================

    class BackendHA : public BackendBase
    {
    public:

        virtual void begin(
            MQTT* mqttInstance
        )
        {
            publishDiscovery(mqttInstance);
        }

        virtual void loop(
            MQTT* mqttInstance
        )
        {
            (void)mqttInstance;
        }

        virtual bool publish(
            MQTT* mqttInstance,
            const FrontendConfig::MQTT::Device* device,
            const FrontendConfig::MQTT::Mapping* mapping,
            long value
        )
        {
            (void)device;

            char topic[160];
            char payload[64];

            buildStateTopic(
                mqttInstance,
                mapping,
                topic,
                sizeof(topic)
            );

            buildValuePayload(
                mapping,
                value,
                payload,
                sizeof(payload)
            );

            return mqttInstance->publishRaw(
                topic,
                payload
            );
        }

        virtual void subscribe(
            MQTT* mqttInstance
        )
        {
            for (size_t d = 0;
                d < mqttInstance->getDeviceCount();
                ++d)
            {
                const FrontendConfig::MQTT::Device* device =
                    mqttInstance->getDevice(d);

                if (!device)
                    continue;

                /*
                * Questo client gestisce solo i device
                * assegnati a lui.
                */
                if (device->client != mqttInstance->getClientIndex())
                    continue;

                if (!device->mappings)
                    continue;

                for (size_t m = 0;
                    m < device->mappingCount;
                    ++m)
                {
                    const FrontendConfig::MQTT::Mapping* mapping =
                        &device->mappings[m];

                    if (!mqttInstance->canRead(mapping))
                        continue;

                    char topic[160];

                    buildCommandTopic(
                        mqttInstance,
                        mapping,
                        topic,
                        sizeof(topic)
                    );

                    mqttInstance->subscribeRaw(topic);
                }
            }
        }

        virtual bool decodeMessage(
            MQTT* mqttInstance,
            const char* topic,
            const char* payload,
            size_t len,
            const FrontendConfig::MQTT::Device*& device,
            const FrontendConfig::MQTT::Mapping*& mapping,
            long& value
        )
        {
            device = nullptr;
            mapping = nullptr;
            value = 0;

            for (size_t d = 0;
                d < mqttInstance->getDeviceCount();
                ++d)
            {
                const FrontendConfig::MQTT::Device* dev =
                    mqttInstance->getDevice(d);

                if (!dev)
                    continue;

                /*
                * Ignora i device appartenenti ad altri
                * client MQTT.
                */
                if (dev->client != mqttInstance->getClientIndex())
                    continue;

                if (!dev->mappings)
                    continue;

                for (size_t m = 0;
                    m < dev->mappingCount;
                    ++m)
                {
                    const FrontendConfig::MQTT::Mapping* map =
                        &dev->mappings[m];

                    if (!mqttInstance->canRead(map))
                        continue;

                    char expected[160];

                    buildCommandTopic(
                        mqttInstance,
                        map,
                        expected,
                        sizeof(expected)
                    );

                    if (strcmp(topic, expected) != 0)
                        continue;

                    if (!parseValue(
                            map,
                            payload,
                            len,
                            value))
                    {
                        return false;
                    }

                    device = dev;
                    mapping = map;

                    return true;
                }
            }

            return false;
        }

        virtual const char* name() const
        {
            return "HOME_ASSISTANT";
        }

    private:

        static void buildStateTopic(
            MQTT* mqttInstance,
            const FrontendConfig::MQTT::Mapping* mapping,
            char* out,
            size_t outSize
        )
        {
            snprintf(
                out,
                outSize,
                "homeassistant/state/%s/%s",
                mqttInstance->getNodeId(),
                mapping->field
            );
        }

        static void buildCommandTopic(
            MQTT* mqttInstance,
            const FrontendConfig::MQTT::Mapping* mapping,
            char* out,
            size_t outSize
        )
        {
            snprintf(
                out,
                outSize,
                "homeassistant/cmd/%s/%s",
                mqttInstance->getNodeId(),
                mapping->field
            );
        }

        static void buildValuePayload(
            const FrontendConfig::MQTT::Mapping* mapping,
            long value,
            char* out,
            size_t outSize
        )
        {
            switch (mapping->type)
            {
                case FrontendConfig::MQTT::Mapping::DataType::BOOL:
                    snprintf(
                        out,
                        outSize,
                        "%s",
                        value ? "ON" : "OFF"
                    );
                    break;

                case FrontendConfig::MQTT::Mapping::DataType::FLOAT:
                    snprintf(
                        out,
                        outSize,
                        "%.3f",
                        ((float)value) * mapping->scale
                    );
                    break;

                case FrontendConfig::MQTT::Mapping::DataType::STRING:
                    snprintf(
                        out,
                        outSize,
                        "%ld",
                        value
                    );
                    break;

                case FrontendConfig::MQTT::Mapping::DataType::INT:
                default:
                    snprintf(
                        out,
                        outSize,
                        "%ld",
                        value
                    );
                    break;
            }
        }

        static bool parseValue(
            const FrontendConfig::MQTT::Mapping* mapping,
            const char* payload,
            size_t len,
            long& value
        )
        {
            char buf[64];

            if (len >= sizeof(buf))
                len = sizeof(buf) - 1;

            memcpy(
                buf,
                payload,
                len
            );

            buf[len] = '\0';

            switch (mapping->type)
            {
                case FrontendConfig::MQTT::Mapping::DataType::BOOL:
                {
                    if (
                        strcmp(buf, "ON") == 0 ||
                        strcmp(buf, "on") == 0 ||
                        strcmp(buf, "true") == 0 ||
                        strcmp(buf, "TRUE") == 0 ||
                        strcmp(buf, "1") == 0
                    )
                    {
                        value = 1;
                        return true;
                    }

                    if (
                        strcmp(buf, "OFF") == 0 ||
                        strcmp(buf, "off") == 0 ||
                        strcmp(buf, "false") == 0 ||
                        strcmp(buf, "FALSE") == 0 ||
                        strcmp(buf, "0") == 0
                    )
                    {
                        value = 0;
                        return true;
                    }

                    return false;
                }

                case FrontendConfig::MQTT::Mapping::DataType::FLOAT:
                {
                    float f = atof(buf);

                    if (mapping->scale == 0.0f)
                        value = (long)f;
                    else
                        value = (long)(f / mapping->scale);

                    return true;
                }

                case FrontendConfig::MQTT::Mapping::DataType::STRING:
                    /*
                    * Il modello RuntimeMapping attuale usa long.
                    * La conversione a stringa non è quindi realmente
                    * supportata a livello Buffer in questa versione.
                    */
                    value = atol(buf);
                    return true;

                case FrontendConfig::MQTT::Mapping::DataType::INT:
                default:
                    value = atol(buf);
                    return true;
            }
        }

        static void publishDiscovery(
            MQTT* mqttInstance
        )
        {
            for (size_t d = 0;
                d < mqttInstance->getDeviceCount();
                ++d)
            {
                const FrontendConfig::MQTT::Device* device =
                    mqttInstance->getDevice(d);

                if (!device)
                    continue;

                /*
                * Discovery solo per i device assegnati
                * a questo client MQTT.
                */
                if (device->client != mqttInstance->getClientIndex())
                    continue;

                if (!device->mappings)
                    continue;

                for (size_t m = 0;
                    m < device->mappingCount;
                    ++m)
                {
                    const FrontendConfig::MQTT::Mapping* mapping =
                        &device->mappings[m];

                    if (!mqttInstance->canWrite(mapping))
                        continue;

                    char topic[180];
                    char payload[512];

                    const char* component = "sensor";

                    if (
                        mapping->type ==
                        FrontendConfig::MQTT::Mapping::DataType::BOOL
                    )
                    {
                        component = "binary_sensor";
                    }

                    snprintf(
                        topic,
                        sizeof(topic),
                        "homeassistant/%s/%s_%s/config",
                        component,
                        mqttInstance->getNodeId(),
                        mapping->field
                    );

                    snprintf(
                        payload,
                        sizeof(payload),
                        "{\"name\":\"%s\",\"unique_id\":\"%s_%s\",\"state_topic\":\"homeassistant/state/%s/%s\",\"command_topic\":\"homeassistant/cmd/%s/%s\",\"value_template\":\"{{ value_json.%s }}\"}",
                        mapping->field,
                        mqttInstance->getNodeId(),
                        mapping->field,
                        mqttInstance->getNodeId(),
                        mapping->field,
                        mqttInstance->getNodeId(),
                        mapping->field,
                        mapping->field
                    );

                    mqttInstance->publishRaw(
                        topic,
                        payload,
                        true
                    );
                }
            }
        }
    };

    // ========================================================
    // ZIGBEE2MQTT BACKEND
    // ========================================================

    class BackendZ2M : public BackendBase
    {
    public:

        virtual void begin(
            MQTT* mqttInstance
        )
        {
            (void)mqttInstance;
        }

        virtual void loop(
            MQTT* mqttInstance
        )
        {
            (void)mqttInstance;
        }

        virtual bool publish(
            MQTT* mqttInstance,
            const FrontendConfig::MQTT::Device* device,
            const FrontendConfig::MQTT::Mapping* mapping,
            long value
        )
        {
            char topic[180];
            char payload[160];

            snprintf(
                topic,
                sizeof(topic),
                "%s/%s/set",
                mqttInstance->getNodeId(),
                device->id
            );

            buildJsonPayload(
                mapping,
                value,
                payload,
                sizeof(payload)
            );

            LOG_EF(
                "MQTT",
                "Z2M TX topic=%s payload=%s",
                topic,
                payload
            );

            return mqttInstance->publishRaw(
                topic,
                payload
            );
        }

        virtual void subscribe(
            MQTT* mqttInstance
        )
        {
            for (size_t d = 0;
                 d < mqttInstance->getDeviceCount();
                 ++d)
            {
                const FrontendConfig::MQTT::Device* device =
                    mqttInstance->getDevice(d);

                if (!device)
                    continue;

                if (device->client != mqttInstance->getClientIndex())
                        continue;

                bool needState = false;
                bool needCommand = false;

                for (size_t m = 0;
                     m < device->mappingCount;
                     ++m)
                {
                    const FrontendConfig::MQTT::Mapping* mapping =
                        &device->mappings[m];

                    if (mqttInstance->canRead(mapping))
                        needState = true;

                    if (mqttInstance->canWrite(mapping))
                        needCommand = true;
                }

                if (needState)
                {
                    char topic[180];

                    snprintf(
                        topic,
                        sizeof(topic),
                        "%s/%s",
                        mqttInstance->getNodeId(),
                        device->id
                    );

                    mqttInstance->subscribeRaw(topic);
                }

                /*
                 * Z2M normalmente usa /set in uscita.
                 * Non serve sottoscriversi a /set per la gestione
                 * dello stato proveniente da Z2M.
                 */
                (void)needCommand;
            }
        }

        virtual bool decodeMessage(
            MQTT* mqttInstance,
            const char* topic,
            const char* payload,
            size_t len,
            const FrontendConfig::MQTT::Device*& device,
            const FrontendConfig::MQTT::Mapping*& mapping,
            long& value
        )
        {
            device = nullptr;
            mapping = nullptr;
            value = 0;

            for (size_t d = 0;
                 d < mqttInstance->getDeviceCount();
                 ++d)
            {
                const FrontendConfig::MQTT::Device* dev =
                    mqttInstance->getDevice(d);

                if (!dev)
                    continue;

                /*
                * Questo client MQTT deve gestire
                * solamente i device assegnati a lui.
                */
                if (dev->client != mqttInstance->getClientIndex())
                    continue;

                char stateTopic[180];

                snprintf(
                    stateTopic,
                    sizeof(stateTopic),
                    "%s/%s",
                    mqttInstance->getNodeId(),
                    dev->id
                );

                if (strcmp(topic, stateTopic) != 0)
                    continue;

                for (size_t m = 0;
                     m < dev->mappingCount;
                     ++m)
                {
                    const FrontendConfig::MQTT::Mapping* map =
                        &dev->mappings[m];

                    if (!mqttInstance->canRead(map))
                        continue;

                    if (!extractJsonValue(
                            payload,
                            len,
                            map->field,
                            map,
                            value))
                    {
                        continue;
                    }

                    device = dev;
                    mapping = map;
                    return true;
                }

                return false;
            }

            return false;
        }

        virtual const char* name() const
        {
            return "ZIGBEE2MQTT";
        }

    private:

        static void buildJsonPayload(
            const FrontendConfig::MQTT::Mapping* mapping,
            long value,
            char* out,
            size_t outSize
        )
        {
            switch (mapping->type)
            {
                case FrontendConfig::MQTT::Mapping::DataType::BOOL:
                    snprintf(
                        out,
                        outSize,
                        "{\"%s\":\"%s\"}",
                        mapping->field,
                        value ? "ON" : "OFF"
                    );
                    break;

                case FrontendConfig::MQTT::Mapping::DataType::FLOAT:
                    snprintf(
                        out,
                        outSize,
                        "{\"%s\":%.3f}",
                        mapping->field,
                        ((float)value) * mapping->scale
                    );
                    break;

                case FrontendConfig::MQTT::Mapping::DataType::INT:
                default:
                    snprintf(
                        out,
                        outSize,
                        "{\"%s\":%ld}",
                        mapping->field,
                        value
                    );
                    break;
            }
        }

        static bool extractJsonValue(
            const char* payload,
            size_t len,
            const char* field,
            const FrontendConfig::MQTT::Mapping* mapping,
            long& value
        )
        {
            char key[96];

            snprintf(
                key,
                sizeof(key),
                "\"%s\"",
                field
            );

            const char* p = strstr(payload, key);

            if (!p)
                return false;

            p += strlen(key);

            while (
                (size_t)(p - payload) < len &&
                (*p == ' ' || *p == '\t' || *p == ':')
            )
            {
                ++p;
            }

            char buf[64];
            size_t n = 0;

            while (
                (size_t)(p - payload) < len &&
                *p != ',' &&
                *p != '}' &&
                *p != ' ' &&
                *p != '\r' &&
                *p != '\n' &&
                n < sizeof(buf) - 1
            )
            {
                buf[n++] = *p++;
            }

            buf[n] = '\0';

            if (mapping->type ==
                FrontendConfig::MQTT::Mapping::DataType::BOOL)
            {
                if (
                    strcmp(buf, "\"ON\"") == 0 ||
                    strcmp(buf, "true") == 0 ||
                    strcmp(buf, "1") == 0
                )
                {
                    value = 1;
                    return true;
                }

                if (
                    strcmp(buf, "\"OFF\"") == 0 ||
                    strcmp(buf, "false") == 0 ||
                    strcmp(buf, "0") == 0
                )
                {
                    value = 0;
                    return true;
                }

                return false;
            }

            if (mapping->type ==
                FrontendConfig::MQTT::Mapping::DataType::FLOAT)
            {
                float f = atof(buf);

                if (mapping->scale == 0.0f)
                    value = (long)f;
                else
                    value = (long)(f / mapping->scale);

                return true;
            }

            value = atol(buf);
            return true;
        }
    };

    // ========================================================
    // SHELLY BACKEND
    // ========================================================

    class BackendShelly : public BackendBase
    {
    public:

        virtual void begin(
            MQTT* mqttInstance
        )
        {
            (void)mqttInstance;
        }

        virtual void loop(
            MQTT* mqttInstance
        )
        {
            (void)mqttInstance;
        }

        virtual bool publish(
            MQTT* mqttInstance,
            const FrontendConfig::MQTT::Device* device,
            const FrontendConfig::MQTT::Mapping* mapping,
            long value
        )
        {
            char topic[180];
            char payload[64];

            snprintf(
                topic,
                sizeof(topic),
                "%s/%s/set",
                mqttInstance->getNodeId(),
                device->id
            );

            snprintf(
                payload,
                sizeof(payload),
                "%ld",
                value
            );

            bool ok = mqttInstance->publishRaw(
                topic,
                payload
            );

            LOG_IF(
                "MQTT",
                "SHELLY TX topic=%s payload=%s result=%d",
                topic,
                payload,
                ok ? 1 : 0
            );

            return ok;
        }

        virtual void subscribe(
            MQTT* mqttInstance
        )
        {
            for (size_t d = 0;
                 d < mqttInstance->getDeviceCount();
                 ++d)
            {
                const FrontendConfig::MQTT::Device* device =
                    mqttInstance->getDevice(d);

                if (!device)
                    continue;

                for (size_t m = 0;
                     m < device->mappingCount;
                     ++m)
                {
                    const FrontendConfig::MQTT::Mapping* mapping =
                        &device->mappings[m];

                    if (!mqttInstance->canRead(mapping))
                        continue;

                    char topic[180];

                    snprintf(
                        topic,
                        sizeof(topic),
                        "%s/%s",
                        mqttInstance->getNodeId(),
                        device->id
                    );

                    mqttInstance->subscribeRaw(topic);
                }
            }
        }

        virtual bool decodeMessage(
            MQTT* mqttInstance,
            const char* topic,
            const char* payload,
            size_t len,
            const FrontendConfig::MQTT::Device*& device,
            const FrontendConfig::MQTT::Mapping*& mapping,
            long& value
        )
        {
            (void)len;

            device = nullptr;
            mapping = nullptr;
            value = 0;

            for (size_t d = 0;
                 d < mqttInstance->getDeviceCount();
                 ++d)
            {
                const FrontendConfig::MQTT::Device* dev =
                    mqttInstance->getDevice(d);

                if (!dev)
                    continue;

                for (size_t m = 0;
                     m < dev->mappingCount;
                     ++m)
                {
                    const FrontendConfig::MQTT::Mapping* map =
                        &dev->mappings[m];

                    if (!mqttInstance->canRead(map))
                        continue;

                    char expected[180];

                    snprintf(
                        expected,
                        sizeof(expected),
                        "%s/%s",
                        mqttInstance->getNodeId(),
                        dev->id
                    );

                    if (strcmp(topic, expected) != 0)
                        continue;

                    value = atol(payload);

                    device = dev;
                    mapping = map;

                    return true;
                }
            }

            return false;
        }

        virtual const char* name() const
        {
            return "SHELLY";
        }
    };

private:

    EthernetClient& eth;
    PubSubClient& mqtt;

    const char* nodeId;
    uint8_t clientIndex;

    const FrontendConfig::MQTT::Client* clientCfg;
    const FrontendConfig::MQTT::Device* devices;
    size_t deviceCount;

    BackendBase* backendImpl;

    CommandCallback commandCallback;

    static MQTT* activeCallbackInstance;

    unsigned long lastReconnectAttempt;

public:

    MQTT(
        EthernetClient& ethClient,
        PubSubClient& mqttClient,
        const char* node,
        uint8_t mqttClientIndex,
        const FrontendConfig::MQTT::Client* client,
        const FrontendConfig::MQTT::Device* deviceList,
        size_t deviceCount_
    )
        : eth(ethClient),
          mqtt(mqttClient),
          nodeId(node),
          clientIndex(mqttClientIndex),
          clientCfg(client),
          devices(deviceList),
          deviceCount(deviceCount_),
          backendImpl(nullptr),
          commandCallback(nullptr)
    {
        lastReconnectAttempt = 0;
    }

    void setCommandCallback(
        CommandCallback cb
    )
    {
        commandCallback = cb;
    }

    void setBackend(
        FrontendConfig::MQTT::Client::Backend backend
    )
    {
        switch (backend)
        {
            case FrontendConfig::MQTT::Client::Backend::HOME_ASSISTANT:
                backendImpl = &haBackend;
                break;

            case FrontendConfig::MQTT::Client::Backend::ZIGBEE2MQTT:
                backendImpl = &z2mBackend;
                break;

            case FrontendConfig::MQTT::Client::Backend::SHELLY:
                backendImpl = &shellyBackend;
                break;

            case FrontendConfig::MQTT::Client::Backend::GENERIC:
            default:
                backendImpl = &haBackend;
                break;
        }
    }

    bool begin(int bufferSize=512)
    {
        if (!clientCfg)
            return false;

        mqtt.setServer(
            clientCfg->broker,
            clientCfg->port
        );

        mqtt.setCallback(mqttCallback);
        mqtt.setBufferSize(bufferSize);

        if (!backendImpl)
        {
            setBackend(clientCfg->backend);
        }

        if (!backendImpl)
            return false;

        backendImpl->begin(this);

        return true;
    }

    bool reconnect()
    {
        if (!clientCfg)
        {
            LOG_EF(
                "MQTT",
                "reconnect: clientCfg NULL"
            );

            return false;
        }

        if (mqtt.connected())
        {
            LOG_IF(
                "MQTT",
                "reconnect: gia' connected"
            );

            return true;
        }

        if (!reconnectDue())
        {
            return false;
        }

        const unsigned long now = millis();

        lastReconnectAttempt = now;

        char clientName[64];

        snprintf(
            clientName,
            sizeof(clientName),
            "%s_mqtt_%u",
            nodeId ? nodeId : "node",
            (unsigned)clientIndex
        );

        LOG_IF(
            "MQTT",
            "reconnect: CONNECT -> %s:%u clientId=%s",
            clientCfg->broker.toString().c_str(),
            (unsigned)clientCfg->port,
            clientName
        );

        const bool ok = mqtt.connect(clientName);
        if (ok)
        {
            if (backendImpl)
                backendImpl->subscribe(this);

            return true;
        }

        LOG_EF(
            "MQTT",
            "Client[%u] Connessione MQTT FALLITA state=%d",
            (unsigned)clientIndex,
            mqtt.state()
        );

        return false;
    }

    bool reconnectDue() const
    {
        static const unsigned long RECONNECT_INTERVAL = 60000UL;

        if (lastReconnectAttempt == 0)
            return true;

        return
            (millis() - lastReconnectAttempt) >=
            RECONNECT_INTERVAL;
    }

    void loop()
    {
        if (!mqtt.connected())
        {
            LOG_EF(
                "MQTT",
                "loop: NON CONNECTED state=%d eth.connected=%d",
                mqtt.state(),
                eth.connected() ? 1 : 0
            );

            return;
        }

        activeCallbackInstance = this;

        const int available =
            eth.available();

        const bool ok = mqtt.loop();

        activeCallbackInstance = nullptr;
    }

    bool connected() const
    {
        return mqtt.connected();
    }

    bool publishMapping(
        const FrontendConfig::MQTT::Device* device,
        const FrontendConfig::MQTT::Mapping* mapping,
        long value
    )
    {
        if (!device || !mapping)
            return false;

        if (!canWrite(mapping))
            return false;

        if (!backendImpl)
            return false;

        if (!mqtt.connected())
            return false;

        return backendImpl->publish(
            this,
            device,
            mapping,
            value
        );
    }

    bool publishRaw(
        const char* topic,
        const char* payload,
        bool retained = false
    )
    {
        if (!mqtt.connected())
            return false;

        return mqtt.publish(
            topic,
            payload,
            retained
        );
    }

    bool subscribeRaw(
        const char* topic
    )
    {
        if (!mqtt.connected())
            return false;

        bool ok = mqtt.subscribe(topic);

        LOG_IF(
            "MQTT",
            "SUBSCRIBE -> %s result=%d",
            topic,
            ok ? 1 : 0
        );

        return ok;
    }

    bool canRead(
        const FrontendConfig::MQTT::Mapping* mapping
    ) const
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

    bool canWrite(
        const FrontendConfig::MQTT::Mapping* mapping
    ) const
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

    const char* getNodeId() const
    {
        return nodeId ? nodeId : "";
    }

    uint8_t getClientIndex() const
    {
        return clientIndex;
    }

    const FrontendConfig::MQTT::Client* getClientConfig() const
    {
        return clientCfg;
    }

    size_t getDeviceCount() const
    {
        return deviceCount;
    }

    const FrontendConfig::MQTT::Device* getDevice(
        size_t index
    ) const
    {
        if (index >= deviceCount)
            return nullptr;

        return &devices[index];
    }

private:

    BackendHA haBackend;
    BackendZ2M z2mBackend;
    BackendShelly shellyBackend;

    static void mqttCallback(
        char* topic,
        uint8_t* payload,
        unsigned int len
    )
    {
        LOG_DF(
            "MQTT",
            "RX topic=%s payload=%s",
            topic,
            payload
        );

        if (!activeCallbackInstance)
            return;

        activeCallbackInstance->onMessage(
            topic,
            payload,
            len
        );
    }

    void onMessage(
        const char* topic,
        const uint8_t* payload,
        size_t len
    )
    {
        if (!backendImpl)
            return;

        const FrontendConfig::MQTT::Device* device = nullptr;
        const FrontendConfig::MQTT::Mapping* mapping = nullptr;

        long value = 0;

        char payloadBuffer[256];

        if (len >= sizeof(payloadBuffer))
            len = sizeof(payloadBuffer) - 1;

        memcpy(
            payloadBuffer,
            payload,
            len
        );

        payloadBuffer[len] = '\0';
        
        if (!backendImpl->decodeMessage(
                this,
                topic,
                payloadBuffer,
                len,
                device,
                mapping,
                value))
        {
            LOG_WF(
                "MQTT",
                "RX decode FALLITO topic=%s",
                topic
            );

            return;
        }

        if (!device || !mapping)
            return;

        if (!commandCallback)
            return;

        commandCallback(
            clientIndex,
            device,
            mapping,
            value
        );
    }
};


// ============================================================
// STATIC MEMBER
// ============================================================

MQTT* MQTT::activeCallbackInstance = nullptr;


#endif

