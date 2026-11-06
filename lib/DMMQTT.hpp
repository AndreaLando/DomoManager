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
#include "DMEthernet.hpp"


#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================================
// MQTT INBOUND MESSAGE
//
// Rappresentazione normalizzata di un messaggio MQTT ricevuto.
//
// La struttura viene utilizzata sia dal layer MQTT Device/Mapping sia dal
// DMHAOrchestrator.
//
// Il payload e tutti i puntatori presenti nella struttura sono validi
// solamente durante l'esecuzione sincrona della callback.
//
// Se un consumer deve conservare il contenuto deve copiarlo.
// ============================================================================

struct MQTTInboundMessage
{
    // ========================================================================
    // ORIGIN
    // ========================================================================

    enum class Origin : uint8_t
    {
        UNKNOWN = 0,

        // Messaggio MQTT ricevuto ma non ancora interpretato.
        RAW,

        // Mapping MQTT configurato.
        MAPPING_NUMERIC,
        MAPPING_TEXT,

        // Comando Home Assistant.
        HOME_ASSISTANT_AREA,
        HOME_ASSISTANT_RUNTIME
    };

    // ========================================================================
    // VALUE TYPE
    // ========================================================================

    enum class ValueType : uint8_t
    {
        NONE = 0,

        BOOLEAN,
        INTEGER,
        FLOAT,
        TEXT,
        JSON
    };

    // ========================================================================
    // SOURCE
    // ========================================================================

    Origin origin =
        Origin::UNKNOWN;

    ValueType valueType =
        ValueType::NONE;

    uint8_t clientIndex =
        0;

    FrontendConfig::MQTT::Client::Backend backend =
        FrontendConfig::MQTT::Client::Backend::GENERIC;

    // ========================================================================
    // RAW MQTT
    // ========================================================================

    const char* topic =
        nullptr;

    const char* payload =
        nullptr;

    size_t payloadLength =
        0;

    // ========================================================================
    // DEVICE / MAPPING
    // ========================================================================

    const FrontendConfig::MQTT::Device*
        device =
            nullptr;

    const FrontendConfig::MQTT::Mapping*
        mapping =
            nullptr;

    // ========================================================================
    // NUMERIC VALUE
    // ========================================================================

    long integerValue =
        0;

    float floatValue =
        0.0f;

    bool booleanValue =
        false;

    // ========================================================================
    // BUFFER AREA
    // ========================================================================

    int area =
        -1;

    // ========================================================================
    // RUNTIME STORE
    //
    // Non vengono utilizzati tipi annidati di DMRuntimeStore.
    //
    // Questo mantiene DMMQTT.hpp indipendente da DMRuntimeStore.hpp ed evita
    // dipendenze circolari tra il layer MQTT e il runtime database.
    // ========================================================================

    static constexpr uint8_t INVALID_RUNTIME_HANDLE =
        0xFF;

    uint8_t runtimeHandle =
        INVALID_RUNTIME_HANDLE;

    const char* runtimeId =
        nullptr;

    uint8_t runtimeType =
        0;

    bool runtimeSensitive =
        false;

    // ========================================================================
    // HELPERS
    // ========================================================================

    bool hasPayload() const
    {
        return
            payload != nullptr;
    }

    bool isRaw() const
    {
        return
            origin ==
                Origin::RAW;
    }

    bool isMapping() const
    {
        return
            origin ==
                Origin::MAPPING_NUMERIC ||
            origin ==
                Origin::MAPPING_TEXT;
    }

    bool isHomeAssistant() const
    {
        return
            origin ==
                Origin::HOME_ASSISTANT_AREA ||
            origin ==
                Origin::HOME_ASSISTANT_RUNTIME;
    }

    bool isArea() const
    {
        return
            area >= 0;
    }

    bool isRuntime() const
    {
        return
            origin ==
                Origin::HOME_ASSISTANT_RUNTIME &&
            runtimeHandle !=
                INVALID_RUNTIME_HANDLE &&
            runtimeId !=
                nullptr;
    }

    bool valid() const
    {
        switch (origin)
        {
            case Origin::RAW:
            {
                return
                    topic != nullptr &&
                    payload != nullptr;
            }

            case Origin::MAPPING_NUMERIC:
            {
                return
                    device != nullptr &&
                    mapping != nullptr;
            }

            case Origin::MAPPING_TEXT:
            {
                return
                    device != nullptr &&
                    mapping != nullptr &&
                    payload != nullptr;
            }

            case Origin::HOME_ASSISTANT_AREA:
            {
                return
                    area >= 0;
            }

            case Origin::HOME_ASSISTANT_RUNTIME:
            {
                return
                    runtimeHandle !=
                        INVALID_RUNTIME_HANDLE &&
                    runtimeId !=
                        nullptr;
            }

            case Origin::UNKNOWN:
            default:
            {
                return false;
            }
        }
    }
};


// ============================================================
// MQTT LOW LEVEL / BACKENDS
// ============================================================

class MQTT {
public:
    
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

    // ========================================================================
    // INBOUND MESSAGE CALLBACK
    //
    // Callback applicativa unica per:
    //
    //   - mapping numerici;
    //   - mapping testuali;
    //   - messaggi raw, se richiesto;
    //   - eventi Home Assistant normalizzati dal relativo orchestratore.
    //
    // La callback è sincrona.
    // ========================================================================

    using InboundMessageCallback =
        void (*)(
            const MQTTInboundMessage& message
        );

    // ========================================================
    // RAW MESSAGE CALLBACK
    // ========================================================
    //
    // Utilizzato dai backend privi di mapping statici.
    //
    // Caso principale:
    //
    //   Home Assistant Auto Discovery
    //
    // dove i comandi arrivano su:
    //
    //   domomanager/area/<area>/set
    //
    // ========================================================

    typedef void (*RawMessageCallback)(
        uint8_t clientIndex,
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        size_t length
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

        // ========================================================
        // HOME ASSISTANT SUBSCRIBE
        // ========================================================
        //
        // Il client Home Assistant non utilizza Device/Mapping
        // statici.
        //
        // Tutti i comandi Auto Discovery vengono ricevuti tramite:
        //
        //     <topicPrefix>/area/+/set
        //
        // Con topicPrefix:
        //
        //     domomanager
        //
        // la subscription risultante è:
        //
        //     domomanager/area/+/set
        //
        // ========================================================

        virtual void subscribe(
            MQTT* mqttInstance)
        {
            if (!mqttInstance)
            {
                LOG_EF(
                    "MQTT",
                    "BackendHA::subscribe: mqttInstance NULL"
                );

                return;
            }


            // ====================================================
            // TOPIC PREFIX
            // ====================================================

            const char* prefix =
                mqttInstance->getTopicPrefix();


            if (!prefix ||
                prefix[0] == '\0')
            {
                prefix =
                    "domomanager";
            }


            // ====================================================
            // COMMAND SUBSCRIPTION
            // ====================================================

            char commandTopic[160];

            const int length =
                snprintf(
                    commandTopic,
                    sizeof(commandTopic),
                    "%s/area/+/set",
                    prefix
                );


            if (length <= 0 ||
                static_cast<size_t>(length) >=
                    sizeof(commandTopic))
            {
                LOG_EF(
                    "MQTT",
                    "BackendHA::subscribe: "
                    "topic non valido prefix=%s",
                    prefix
                );

                return;
            }


            LOG_IF(
                "MQTT",
                "HA command subscription: topic=%s",
                commandTopic
            );


            const bool subscribed =
                mqttInstance->subscribeRaw(
                    commandTopic
                );


            if (!subscribed)
            {
                LOG_EF(
                    "MQTT",
                    "HA command subscription FALLITA: topic=%s",
                    commandTopic
                );

                return;
            }


            LOG_IF(
                "MQTT",
                "HA command subscription completata: topic=%s",
                commandTopic
            );
        }

        // ========================================================
        // HOME ASSISTANT DECODE MESSAGE
        // ========================================================
        //
        // I comandi Auto Discovery vengono gestiti mediante il
        // RawMessageCallback.
        //
        // Questo backend non produce Device/Mapping statici.
        //
        // ========================================================

        virtual bool decodeMessage(
            MQTT* mqttInstance,
            const char* topic,
            const char* payload,
            size_t len,
            const FrontendConfig::MQTT::Device*& device,
            const FrontendConfig::MQTT::Mapping*& mapping,
            long& value)
        {
            (void)mqttInstance;
            (void)topic;
            (void)payload;
            (void)len;

            device =
                nullptr;

            mapping =
                nullptr;

            value =
                0;

            return false;
        }

        virtual bool decodeTextMessage(
            MQTT* mqttInstance,
            const char* topic,
            const char* payload,
            size_t len,
            const FrontendConfig::MQTT::Device*& device,
            const FrontendConfig::MQTT::Mapping*& mapping,
            char* text,
            size_t textSize,
            size_t& textLength)
        {
            (void)mqttInstance;
            (void)topic;
            (void)payload;
            (void)len;

            device = nullptr;
            mapping = nullptr;

            if (text && textSize > 0)
            {
                text[0] = '\0';
            }

            textLength = 0;

            return false;
        }

        virtual const char* name() const = 0;
    };

    // ========================================================
    // HOME ASSISTANT BACKEND
    // ========================================================

    class BackendHA : public BackendBase
    {
    public:

        virtual void begin(
            MQTT* mqttInstance)
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

            LOG_IF(
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

        virtual bool decodeTextMessage(
            MQTT* mqttInstance,
            const char* topic,
            const char* payload,
            size_t len,
            const FrontendConfig::MQTT::Device*& device,
            const FrontendConfig::MQTT::Mapping*& mapping,
            char* text,
            size_t textSize,
            size_t& textLength)
        {
            device = nullptr;
            mapping = nullptr;
            textLength = 0;

            if (!mqttInstance ||
                !topic ||
                !payload ||
                !text ||
                textSize < 2)
            {
                return false;
            }

            text[0] = '\0';

            for (size_t d = 0;
                d < mqttInstance->getDeviceCount();
                ++d)
            {
                const FrontendConfig::MQTT::Device* dev =
                    mqttInstance->getDevice(d);

                if (!dev)
                    continue;

                char stateTopic[180];

                const int topicLength =
                    snprintf(
                        stateTopic,
                        sizeof(stateTopic),
                        "%s/%s",
                        mqttInstance->getTopicPrefix(),
                        dev->id
                    );

                if (topicLength <= 0 ||
                    static_cast<size_t>(topicLength) >=
                        sizeof(stateTopic))
                {
                    continue;
                }

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

                    if (map->type !=
                            FrontendConfig::MQTT::Mapping::DataType::STRING &&
                        map->type !=
                            FrontendConfig::MQTT::Mapping::DataType::JSON)
                    {
                        continue;
                    }

                    bool decoded = false;

                    if (map->type ==
                        FrontendConfig::MQTT::Mapping::DataType::STRING)
                    {
                        decoded =
                            extractJsonTextValue(
                                payload,
                                len,
                                map->field,
                                text,
                                textSize,
                                textLength
                            );
                    }
                    else
                    {
                        /*
                        * JSON trasporta il payload completo.
                        *
                        * Il consumer frontend potrà successivamente
                        * deserializzarlo con ArduinoJson.
                        */
                        if (len < textSize)
                        {
                            memcpy(
                                text,
                                payload,
                                len
                            );

                            text[len] = '\0';
                            textLength = len;
                            decoded = true;
                        }
                    }

                    if (!decoded)
                        continue;

                    device = dev;
                    mapping = map;

                    LOG_DF(
                        "MQTT",
                        "Z2M TEXT field=%s bytes=%u value='%s'",
                        map->field
                            ? map->field
                            : "?",
                        static_cast<unsigned>(
                            textLength
                        ),
                        text
                    );

                    return true;
                }

                /*
                * Il topic appartiene al device trovato, ma nessuna
                * mapping testuale corrisponde al payload.
                */
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

        static bool extractJsonTextValue(
            const char* payload,
            size_t len,
            const char* field,
            char* output,
            size_t outputSize,
            size_t& outputLength)
        {
            outputLength = 0;

            if (!payload ||
                len == 0 ||
                !field ||
                field[0] == '\0' ||
                !output ||
                outputSize < 2)
            {
                return false;
            }

            output[0] = '\0';

            char key[96];

            const int keyLength =
                snprintf(
                    key,
                    sizeof(key),
                    "\"%s\"",
                    field
                );

            if (keyLength <= 0 ||
                static_cast<size_t>(keyLength) >=
                    sizeof(key))
            {
                return false;
            }

            const char* payloadEnd =
                payload + len;

            const char* position =
                strstr(
                    payload,
                    key
                );

            if (!position ||
                position >= payloadEnd)
            {
                return false;
            }

            position +=
                static_cast<size_t>(
                    keyLength
                );

            /*
            * Cerca il separatore ':' senza oltrepassare
            * il payload ricevuto.
            */
            while (position < payloadEnd &&
                (*position == ' ' ||
                    *position == '\t' ||
                    *position == '\r' ||
                    *position == '\n'))
            {
                ++position;
            }

            if (position >= payloadEnd ||
                *position != ':')
            {
                return false;
            }

            ++position;

            /*
            * Salta gli spazi dopo ':'.
            */
            while (position < payloadEnd &&
                (*position == ' ' ||
                    *position == '\t' ||
                    *position == '\r' ||
                    *position == '\n'))
            {
                ++position;
            }

            if (position >= payloadEnd)
                return false;

            /*
            * Per DataType::STRING richiediamo una stringa JSON.
            */
            if (*position != '"')
                return false;

            ++position;

            bool escaped = false;

            while (position < payloadEnd)
            {
                const char current =
                    *position++;

                if (escaped)
                {
                    char decoded =
                        current;

                    switch (current)
                    {
                        case '"':
                            decoded = '"';
                            break;

                        case '\\':
                            decoded = '\\';
                            break;

                        case '/':
                            decoded = '/';
                            break;

                        case 'n':
                            decoded = '\n';
                            break;

                        case 'r':
                            decoded = '\r';
                            break;

                        case 't':
                            decoded = '\t';
                            break;

                        case 'b':
                            decoded = '\b';
                            break;

                        case 'f':
                            decoded = '\f';
                            break;

                        default:
                            /*
                            * Le sequenze Unicode \uXXXX non vengono
                            * decodificate in questa prima versione.
                            */
                            decoded = current;
                            break;
                    }

                    if (outputLength >=
                        outputSize - 1)
                    {
                        output[0] = '\0';
                        outputLength = 0;
                        return false;
                    }

                    output[outputLength++] =
                        decoded;

                    escaped = false;
                    continue;
                }

                if (current == '\\')
                {
                    escaped = true;
                    continue;
                }

                if (current == '"')
                {
                    output[outputLength] =
                        '\0';

                    return true;
                }

                if (outputLength >=
                    outputSize - 1)
                {
                    output[0] = '\0';
                    outputLength = 0;
                    return false;
                }

                output[outputLength++] =
                    current;
            }

            /*
            * Stringa JSON non chiusa.
            */
            output[0] = '\0';
            outputLength = 0;

            return false;
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

                        LOG_DF(
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

    InboundMessageCallback inboundMessageCallback;
    RawMessageCallback rawMessageCallback;

    static MQTT* activeCallbackInstance;

    unsigned long lastReconnectAttempt;

public:

    MQTT(
        EthernetClient& ethClient,
        PubSubClient& mqttClient,
        uint8_t mqttClientIndex,
        const FrontendConfig::MQTT::Client* client,
        const FrontendConfig::MQTT::Device* deviceList,
        size_t deviceCount_)
        : eth(ethClient),
        mqtt(mqttClient),
        clientIndex(mqttClientIndex),
        clientCfg(client),
        devices(deviceList),
        deviceCount(deviceCount_),
        backendImpl(nullptr),
        inboundMessageCallback(nullptr),
        rawMessageCallback(nullptr),
        lastReconnectAttempt(0)
    {
    }

    int state() const
    {
        return mqtt.state();
    }

    bool ethernetConnected() const
    {
        return eth.connected();
    }

    // ========================================================================
    // SET INBOUND MESSAGE CALLBACK
    //
    // Registra la callback applicativa unica.
    //
    // La callback riceve solamente messaggi già normalizzati dal layer che li
    // ha riconosciuti.
    // ========================================================================

    void setInboundMessageCallback(
        InboundMessageCallback callback)
    {
        inboundMessageCallback =
            callback;
    }

    // ========================================================================
    // SET RAW MESSAGE CALLBACK
    //
    // Registra il consumer infrastrutturale dei messaggi raw.
    //
    // Il caso principale è DMHAOrchestrator.
    // ========================================================================

    void setRawMessageCallback(
        RawMessageCallback callback)
    {
        rawMessageCallback =
            callback;
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

private:

    BackendHA haBackend;
    BackendZ2M z2mBackend;
    BackendShelly shellyBackend;

    // ========================================================================
    // RESOLVE MAPPING VALUE TYPE
    // ========================================================================

    static MQTTInboundMessage::ValueType mappingValueType(
        const FrontendConfig::MQTT::Mapping* mapping)
    {
        using DataType =
            FrontendConfig::MQTT::Mapping::DataType;

        using ValueType =
            MQTTInboundMessage::ValueType;

        if (!mapping)
            return ValueType::NONE;

        switch (mapping->type)
        {
            case DataType::BOOL:
                return ValueType::BOOLEAN;

            case DataType::FLOAT:
                return ValueType::FLOAT;

            case DataType::STRING:
                return ValueType::TEXT;

            case DataType::JSON:
                return ValueType::JSON;

            case DataType::ENUM:
            case DataType::INT:
            default:
                return ValueType::INTEGER;
        }
    }

    // ========================================================================
    // NOTIFY INBOUND
    // ========================================================================

    void notifyInbound(
        const MQTTInboundMessage& message)
    {
        if (!inboundMessageCallback)
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

        inboundMessageCallback(
            message
        );
    }

    // ========================================================================
    // NOTIFY RAW
    //
    // Il messaggio raw viene notificato alla callback applicativa solamente
    // quando non è destinato al backend Home Assistant.
    //
    // I messaggi Home Assistant vengono prima interpretati da DMHAOrchestrator
    // e poi notificati come HOME_ASSISTANT_AREA o HOME_ASSISTANT_RUNTIME.
    //
    // Questo impedisce una doppia notifica dello stesso messaggio.
    // ========================================================================

    void notifyRawApplication(
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        size_t length)
    {
        if (backend ==
            FrontendConfig::MQTT::Client::Backend::HOME_ASSISTANT)
        {
            return;
        }

        MQTTInboundMessage message;

        message.origin =
            MQTTInboundMessage::Origin::RAW;

        message.valueType =
            MQTTInboundMessage::ValueType::NONE;

        message.clientIndex =
            clientIndex;

        message.backend =
            backend;

        message.topic =
            topic;

        message.payload =
            payload;

        message.payloadLength =
            length;

        notifyInbound(
            message
        );
    }

    // ========================================================================
    // NOTIFY NUMERIC MAPPING
    // ========================================================================

    void notifyNumericMapping(
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        size_t length,
        const FrontendConfig::MQTT::Device* device,
        const FrontendConfig::MQTT::Mapping* mapping,
        long value)
    {
        if (!device ||
            !mapping)
        {
            return;
        }

        MQTTInboundMessage message;

        message.origin =
            MQTTInboundMessage::Origin::
                MAPPING_NUMERIC;

        message.valueType =
            mappingValueType(
                mapping
            );

        message.clientIndex =
            clientIndex;

        message.backend =
            backend;

        message.topic =
            topic;

        message.payload =
            payload;

        message.payloadLength =
            length;

        message.device =
            device;

        message.mapping =
            mapping;

        message.area =
            mapping->area;

        message.integerValue =
            value;

        switch (mapping->type)
        {
            case FrontendConfig::MQTT::Mapping::
                DataType::BOOL:
            {
                message.booleanValue =
                    value != 0;

                break;
            }

            case FrontendConfig::MQTT::Mapping::
                DataType::FLOAT:
            {
                message.floatValue =
                    static_cast<float>(
                        value
                    ) *
                    mapping->scale;

                break;
            }

            default:
                break;
        }

        notifyInbound(
            message
        );
    }

    // ========================================================================
    // NOTIFY TEXT MAPPING
    // ========================================================================

    void notifyTextMapping(
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* rawPayload,
        size_t rawPayloadLength,
        const FrontendConfig::MQTT::Device* device,
        const FrontendConfig::MQTT::Mapping* mapping,
        const char* decodedPayload,
        size_t decodedLength)
    {
        if (!device ||
            !mapping ||
            !decodedPayload)
        {
            return;
        }

        MQTTInboundMessage message;

        message.origin =
            MQTTInboundMessage::Origin::
                MAPPING_TEXT;

        message.valueType =
            mappingValueType(
                mapping
            );

        message.clientIndex =
            clientIndex;

        message.backend =
            backend;

        message.topic =
            topic;

        /*
        * Per il mapping testuale il payload applicativo è quello già
        * decodificato dal backend, non il payload MQTT grezzo.
        */
        message.payload =
            decodedPayload;

        message.payloadLength =
            decodedLength;

        message.device =
            device;

        message.mapping =
            mapping;

        message.area =
            mapping->area;

        (void)rawPayload;
        (void)rawPayloadLength;

        notifyInbound(
            message
        );
    }

    // ========================================================
    // MQTT CALLBACK
    // ========================================================

    static void mqttCallback(
        char* topic,
        uint8_t* payload,
        unsigned int len)
    {
        if (!activeCallbackInstance)
            return;

        activeCallbackInstance->onMessage(
            topic,
            payload,
            static_cast<size_t>(
                len
            )
        );
    }

    // ========================================================================
    // MQTT MESSAGE DISPATCH
    //
    // Flusso:
    //
    //   1. copia e termina il payload;
    //   2. invia sempre il raw al consumer infrastrutturale;
    //   3. HOME_ASSISTANT termina qui perché verrà interpretato da
    //      DMHAOrchestrator;
    //   4. prova il decoder testuale;
    //   5. prova il decoder numerico;
    //   6. se nessun decoder riconosce il messaggio, notifica RAW
    //      all'applicazione.
    //
    // Ogni messaggio produce una sola notifica applicativa normalizzata.
    // ========================================================================

    void onMessage(
        const char* topic,
        const uint8_t* payload,
        size_t len)
    {
        if (!topic ||
            !payload)
        {
            return;
        }

        // ========================================================
        // PAYLOAD COPY
        // ========================================================

        char payloadBuffer[256];

        if (len >=
            sizeof(payloadBuffer))
        {
            LOG_WF(
                "MQTT",
                "RX payload troppo grande: "
                "client=%u topic=%s bytes=%u max=%u",
                static_cast<unsigned>(
                    clientIndex
                ),
                topic,
                static_cast<unsigned>(
                    len
                ),
                static_cast<unsigned>(
                    sizeof(payloadBuffer) - 1
                )
            );

            return;
        }

        memcpy(
            payloadBuffer,
            payload,
            len
        );

        payloadBuffer[len] =
            '\0';

        const size_t payloadLength =
            len;

        LOG_DF(
            "MQTT",
            "RX client=%u topic=%s payload=%s len=%u",
            static_cast<unsigned>(
                clientIndex
            ),
            topic,
            payloadBuffer,
            static_cast<unsigned>(
                payloadLength
            )
        );

        // ========================================================
        // BACKEND
        // ========================================================

        const FrontendConfig::MQTT::Client::Backend backend =
            clientCfg
                ? clientCfg->backend
                : FrontendConfig::MQTT::Client::
                    Backend::GENERIC;

        // ========================================================
        // INFRASTRUCTURE RAW DISPATCH
        //
        // Utilizzato da DMHAOrchestrator.
        // ========================================================

        if (rawMessageCallback)
        {
            rawMessageCallback(
                clientIndex,
                backend,
                topic,
                payloadBuffer,
                payloadLength
            );
        }

        // ========================================================
        // HOME ASSISTANT
        //
        // Il messaggio verrà notificato all'applicazione solamente
        // dopo essere stato interpretato da DMHAOrchestrator.
        // ========================================================

        if (backend ==
            FrontendConfig::MQTT::Client::
                Backend::HOME_ASSISTANT)
        {
            return;
        }

        // ========================================================
        // BACKEND VALIDATION
        // ========================================================

        if (!backendImpl)
        {
            notifyRawApplication(
                backend,
                topic,
                payloadBuffer,
                payloadLength
            );

            return;
        }

        // ========================================================
        // TEXT / JSON DECODER
        // ========================================================

        const FrontendConfig::MQTT::Device* textDevice =
            nullptr;

        const FrontendConfig::MQTT::Mapping* textMapping =
            nullptr;

        char textValue[192];

        size_t textLength =
            0;

        const bool textDecoded =
            backendImpl->decodeTextMessage(
                this,
                topic,
                payloadBuffer,
                payloadLength,
                textDevice,
                textMapping,
                textValue,
                sizeof(textValue),
                textLength
            );

        if (textDecoded)
        {
            if (!textDevice ||
                !textMapping)
            {
                LOG_WF(
                    "MQTT",
                    "Text decode senza device/mapping: "
                    "client=%u topic=%s",
                    static_cast<unsigned>(
                        clientIndex
                    ),
                    topic
                );

                return;
            }

            notifyTextMapping(
                backend,
                topic,
                payloadBuffer,
                payloadLength,
                textDevice,
                textMapping,
                textValue,
                textLength
            );

            return;
        }

        // ========================================================
        // NUMERIC DECODER
        // ========================================================

        const FrontendConfig::MQTT::Device* device =
            nullptr;

        const FrontendConfig::MQTT::Mapping* mapping =
            nullptr;

        long value =
            0;

        const bool numericDecoded =
            backendImpl->decodeMessage(
                this,
                topic,
                payloadBuffer,
                payloadLength,
                device,
                mapping,
                value
            );

        if (numericDecoded)
        {
            if (!device ||
                !mapping)
            {
                LOG_WF(
                    "MQTT",
                    "Numeric decode senza device/mapping: "
                    "client=%u topic=%s",
                    static_cast<unsigned>(
                        clientIndex
                    ),
                    topic
                );

                return;
            }

            notifyNumericMapping(
                backend,
                topic,
                payloadBuffer,
                payloadLength,
                device,
                mapping,
                value
            );

            return;
        }

        // ========================================================
        // UNRESOLVED RAW MESSAGE
        // ========================================================

        notifyRawApplication(
            backend,
            topic,
            payloadBuffer,
            payloadLength
        );

        LOG_DF(
            "MQTT",
            "RX non risolto: "
            "client=%u backend=%u topic=%s",
            static_cast<unsigned>(
                clientIndex
            ),
            static_cast<unsigned>(
                backend
            ),
            topic
        );
    }
};


// ============================================================
// STATIC MEMBER
// ============================================================

MQTT* MQTT::activeCallbackInstance = nullptr;


#endif

