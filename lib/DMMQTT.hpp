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
    struct HADiscoveryEntity
    {
        int area = -1;

        const char* deviceId = nullptr;
        const char* deviceName = nullptr;

        const char* name = nullptr;
        const char* uniqueId = nullptr;
        const char* field = nullptr;

        const char* component = nullptr;

        const char* unit = nullptr;
        const char* deviceClass = nullptr;
        const char* stateClass = nullptr;

        bool readable = false;
        bool writable = false;

        float scale = 1.0f;

        bool valid = false;
    };

    static size_t getMappingCount(
        const FrontendConfig::MQTT::Device* device
    )
    {
        if (!device)
            return 0;

        for (size_t i = 0;
            i < FrontendConfig::MQTT::Device::MAX_MAPPINGS;
            ++i)
        {
            if (!device->mappings[i].field)
                return i;
        }

        return FrontendConfig::MQTT::Device::MAX_MAPPINGS;
    }

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

                if (!device->mappings)
                    continue;

                for (size_t m = 0;
                    m < MQTT::getMappingCount(device);
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

                if (!dev->mappings)
                    continue;

                for (size_t m = 0;
                    m < MQTT::getMappingCount(dev);
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
                mqttInstance->getTopicPrefix(),
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
                mqttInstance->getTopicPrefix(),
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
            if (!mqttInstance)
                return;

            const size_t count =
                mqttInstance->getHADiscoveryCount();

            for (size_t i = 0;
                i < count;
                ++i)
            {
                const MQTT::HADiscoveryEntity* entity =
                    mqttInstance->getHADiscovery(i);

                if (!entity)
                    continue;

                if (!entity->valid)
                    continue;

                char topic[180];
                char payload[768];

                const char* component =
                    entity->component
                        ? entity->component
                        : "sensor";

                const char* field =
                    entity->field
                        ? entity->field
                        : "";

                const char* uniqueId =
                    entity->uniqueId
                        ? entity->uniqueId
                        : field;

                const char* name =
                    entity->name
                        ? entity->name
                        : field;

                const char* prefix =
                    mqttInstance->getTopicPrefix();

                /*
                * ====================================================
                * DISCOVERY TOPIC
                * ====================================================
                */

                snprintf(
                    topic,
                    sizeof(topic),
                    "homeassistant/%s/%s/config",
                    component,
                    uniqueId
                );

                /*
                * ====================================================
                * BASE JSON
                * ====================================================
                */

                snprintf(
                    payload,
                    sizeof(payload),
                    "{"
                    "\"name\":\"%s\","
                    "\"unique_id\":\"%s\","
                    "\"state_topic\":\"homeassistant/state/%s/%s\""
                    "}",
                    name,
                    uniqueId,
                    prefix,
                    field
                );

                /*
                * ====================================================
                * PUBLISH
                * ====================================================
                */

                const bool ok =
                    mqttInstance->publishRaw(
                        topic,
                        payload,
                        true
                    );

                LOG_IF(
                    "MQTT",
                    "HA DISCOVERY TX "
                    "ok=%d "
                    "topic=%s "
                    "payload=%s",
                    ok ? 1 : 0,
                    topic,
                    payload
                );
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
                mqttInstance->getTopicPrefix(),
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

                bool needState = false;
                bool needCommand = false;

                for (size_t m = 0;
                     m < MQTT::getMappingCount(device);
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
                        mqttInstance->getTopicPrefix(),
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

                char stateTopic[180];

                snprintf(
                    stateTopic,
                    sizeof(stateTopic),
                    "%s/%s",
                    mqttInstance->getTopicPrefix(),
                    dev->id
                );

                if (strcmp(topic, stateTopic) != 0)
                    continue;

                for (size_t m = 0;
                     m < MQTT::getMappingCount(dev);
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
            if (!payload || !field || !mapping)
                return false;

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


            /*
            * ------------------------------------------------------------
            * BOOL
            * ------------------------------------------------------------
            */

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


            /*
            * ------------------------------------------------------------
            * FLOAT
            * ------------------------------------------------------------
            */

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


            /*
            * ------------------------------------------------------------
            * ENUM
            * ------------------------------------------------------------
            *
            * Z2M invia una stringa:
            *
            *     {"action":"red"}
            *
            * Il Buffer riceve il valore numerico definito
            * nella tabella mapping->enums.
            */

            if (mapping->type ==
                FrontendConfig::MQTT::Mapping::DataType::ENUM)
            {
                if (mapping->enums.empty())
                    return false;

                /*
                * Il valore JSON è normalmente:
                *
                *     "red"
                *
                * Rimuoviamo le virgolette.
                */
                const char* text = buf;

                if (text[0] == '"')
                    ++text;

                char enumText[64];
                size_t textLen = strlen(text);

                if (
                    textLen > 0 &&
                    text[textLen - 1] == '"'
                )
                {
                    --textLen;
                }

                if (textLen >= sizeof(enumText))
                    textLen = sizeof(enumText) - 1;

                memcpy(
                    enumText,
                    text,
                    textLen
                );

                enumText[textLen] = '\0';


                /*
                * Cerca il testo nella tabella ENUM
                * associata a questa Mapping.
                */
                for (
                    size_t i = 0;
                    i < mapping->enums.count;
                    ++i
                )
                {
                    const FrontendConfig::MQTT::EnumValue& enumValue =
                        mapping->enums.data[i];

                    if (!enumValue.text)
                        continue;

                    if (
                        strcmp(
                            enumValue.text,
                            enumText
                        ) == 0
                    )
                    {
                        value = enumValue.value;

                        LOG_IF(
                            "MQTT",
                            "Z2M ENUM field=%s text=%s value=%ld",
                            mapping->field,
                            enumText,
                            value
                        );

                        return true;
                    }
                }

                LOG_WF(
                    "MQTT",
                    "Z2M ENUM non riconosciuto field=%s text=%s",
                    mapping->field,
                    enumText
                );

                return false;
            }


            /*
            * ------------------------------------------------------------
            * STRING / INT
            * ------------------------------------------------------------
            */

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
                mqttInstance->getTopicPrefix(),
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
                     m < MQTT::getMappingCount(device);
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
                        mqttInstance->getTopicPrefix(),
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
                     m < MQTT::getMappingCount(dev);
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
                        mqttInstance->getTopicPrefix(),
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

    uint8_t clientIndex;

    const FrontendConfig::MQTT::Client* clientCfg;
    const FrontendConfig::MQTT::Device* devices;
    size_t deviceCount;

    BackendBase* backendImpl;

    CommandCallback commandCallback;

    static MQTT* activeCallbackInstance;

    unsigned long lastReconnectAttempt;

    const HADiscoveryEntity* haDiscovery;
    size_t haDiscoveryCount;

public:

    MQTT(
        EthernetClient& ethClient,
        PubSubClient& mqttClient,
        uint8_t mqttClientIndex,
        const FrontendConfig::MQTT::Client* client,
        const FrontendConfig::MQTT::Device* deviceList,
        size_t deviceCount_
    )
        : eth(ethClient),
          mqtt(mqttClient),
          clientIndex(mqttClientIndex),
          clientCfg(client),
          devices(deviceList),
          deviceCount(deviceCount_),
          backendImpl(nullptr),
          commandCallback(nullptr),
          haDiscovery(nullptr),
        haDiscoveryCount(0)
    {
        
        lastReconnectAttempt = 0;
    }

    int state() const
    {
        return mqtt.state();
    }

    bool ethernetConnected() const
    {
        return eth.connected();
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
            (
                clientCfg &&
                clientCfg->name &&
                clientCfg->name[0]
            )
                ? clientCfg->name
                : "mqtt",
            (unsigned)clientIndex
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

        mqtt.loop();

        activeCallbackInstance = nullptr;
    }

    bool connected() const
    {
        const bool result =
            mqtt.connected();

        return result;
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

    const char* getTopicPrefix() const
    {
        if (!clientCfg)
            return "";

        return clientCfg->topicPrefix
            ? clientCfg->topicPrefix
            : "";
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

    const HADiscoveryEntity* getHADiscovery(
        size_t index
    ) const
    {
        if (index >= haDiscoveryCount)
            return nullptr;

        return &haDiscovery[index];
    }

    size_t getHADiscoveryCount() const
    {
        return haDiscoveryCount;
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

