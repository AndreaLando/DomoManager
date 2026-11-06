

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "DMIntrospection.hpp"

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑10‑23
   Note:
                    • Nessuna

   ============================================================================ */

/* ============================================================================
   
   Descrizione:
                    Integrazione Home Assistant per DomoManager.

                    Il modulo gestisce:

                      • Generazione automatica discovery MQTT
                      • Pubblicazione availability
                      • Pubblicazione stati entity
                      • Mapping introspection → Home Assistant
                      • Device registry Home Assistant
                      • Generazione topic MQTT
                      • Configurazione automatica da client MQTT

   Refactoring 1.1.0:
                    • Rinominato DMHADiscovery → DMHomeAssistant
                    • Rimossa allocazione dinamica
                    • Aggiunto costruttore default
                    • Eliminata dipendenza da DomoMQTTConfigBuilder
                    • Centralizzata la generazione dei topic MQTT
                    • Aggiunto commandSubscriptionTopic()
                    • Consolidata la configurazione Home Assistant
                    • Migliorata integrazione con DMHAOrchestrator

   Dipendenze:
                    • ArduinoJson
                    • DMIntrospection
                    • FrontendConfig::MQTT

   Note:
                    • Classe stateless lato protocollo
                    • Nessuna dipendenza diretta da MQTTEngine
                    • Topic runtime derivati automaticamente
                    • Compatibile con publish discovery incrementale

   ============================================================================
 */

class DMHomeAssistant
{
public:
    // ========================================================================
    // MQTT PUBLISH CALLBACK
    // ========================================================================

    using PublishCallback =
        bool (*)(
            void* context,
            const char* topic,
            const char* payload,
            bool retain
        );


    // ========================================================================
    // CONFIG
    // ========================================================================
    //
    // nodeId, deviceId e deviceName possono essere ricavati direttamente
    // dalla configurazione del client MQTT HOME_ASSISTANT.
    //
    // topicPrefix e availabilityTopic sono opzionali.
    //
    // Se non valorizzati:
    //
    //   topicPrefix       = deviceId normalizzato
    //   availabilityTopic = <topicPrefix>/status
    //
    // ========================================================================

    struct Config
    {
        const char* discoveryPrefix =
            "homeassistant";

        const char* nodeId =
            "domomanager";

        const char* deviceId =
            "domomanager";

        const char* deviceName =
            "DomoManager";

        const char* manufacturer =
            "DomoManager";

        const char* model =
            "DomoManager Controller";

        const char* softwareVersion =
            nullptr;

        /*
         * Se nullptr o vuoti vengono generati automaticamente
         * a partire da deviceId.
         */
        const char* topicPrefix =
            nullptr;

        const char* availabilityTopic =
            nullptr;

        const char* payloadAvailable =
            "online";

        const char* payloadNotAvailable =
            "offline";

        bool retainDiscovery =
            true;

        bool retainAvailability =
            true;

        bool retainState =
            true;

        bool publishSensors =
            true;

        bool publishBinarySensors =
            true;

        bool publishSwitches =
            true;

        bool publishLights =
            true;

        bool publishCovers =
            true;

        bool publishButtons =
            true;

        bool publishClimate =
            false;

        bool publishNumbers =
            false;

        bool publishTexts = true;
    };


    // ========================================================================
    // RESULT
    // ========================================================================

    struct Result
    {
        size_t examined = 0;

        size_t published = 0;

        size_t skippedHidden = 0;

        size_t skippedUnknown = 0;

        size_t skippedStatus = 0;

        size_t skippedDisabled = 0;

        size_t failed = 0;

        bool ok() const
        {
            return failed == 0;
        }
    };


    // ========================================================================
    // MAKE CONFIG FROM MQTT CLIENT
    // ========================================================================
    //
    // La configurazione Home Assistant viene derivata dal client MQTT.
    //
    // FrontendConfig::MQTT::Client non espone un campo clientId.
    // Il campo name viene quindi utilizzato come identità sorgente.
    //
    // Gli identificatori MQTT vengono successivamente normalizzati da:
    //
    //     sanitizeTopicPart()
    //
    // topicPrefix e availabilityTopic restano nullptr e vengono generati
    // automaticamente a partire da deviceId.
    //
    // ========================================================================

    static Config MakeConfig(
        const FrontendConfig::MQTT::Client& client)
    {
        Config config;

        const char* identity =
            hasText(client.name)
                ? client.name
                : "DomoManager";

        config.discoveryPrefix =
            "homeassistant";

        config.nodeId =
            identity;

        config.deviceId =
            identity;

        config.deviceName =
            identity;

        config.manufacturer =
            "DomoManager";

        config.model =
            "DomoManager Controller";

        config.softwareVersion =
            nullptr;

        config.topicPrefix =
            nullptr;

        config.availabilityTopic =
            nullptr;

        // ----------------------------------------------------
        // ENABLED DOMAINS
        // ----------------------------------------------------

        config.publishSensors       = true;
        config.publishBinarySensors = true;
        config.publishSwitches      = true;
        config.publishLights        = true;
        config.publishCovers        = true;
        config.publishButtons       = true;
        config.publishNumbers       = true;
        config.publishClimate       = true;

        return config;
    }


    // ========================================================================
    // CONSTRUCTORS
    // ========================================================================
    //
    // Due overload distinti evitano problemi con vecchie versioni GCC
    // e argomenti predefiniti contenenti Config{}.
    //
    // ========================================================================

    DMHomeAssistant()
        : _publishCallback(nullptr),
        _publishContext(nullptr),
        _config()
    {
    }

    DMHomeAssistant(
        PublishCallback publishCallback,
        void* publishContext)
        : _publishCallback(
              publishCallback
          ),
          _publishContext(
              publishContext
          ),
          _config()
    {
    }


    DMHomeAssistant(
        PublishCallback publishCallback,
        void* publishContext,
        const Config& config)
        : _publishCallback(
              publishCallback
          ),
          _publishContext(
              publishContext
          ),
          _config(
              config
          )
    {
    }


    // ========================================================================
    // SET PUBLISHER
    // ========================================================================

    void setPublisher(
        PublishCallback publishCallback,
        void* publishContext)
    {
        _publishCallback =
            publishCallback;

        _publishContext =
            publishContext;
    }


    // ========================================================================
    // SET CONFIG
    // ========================================================================

    void setConfig(
        const Config& config)
    {
        _config =
            config;
    }


    // ========================================================================
    // GET CONFIG
    // ========================================================================

    const Config& config() const
    {
        return _config;
    }


    // ========================================================================
    // PUBLISH ALL
    // ========================================================================

    Result publishAll(
        const DomoIntrospection& introspection)
    {
        Result result;

        for (size_t i = 0;
             i < introspection.entityCount();
             ++i)
        {
            const DomoIntrospection::Entity* entity =
                introspection.getEntity(i);

            if (!entity)
                continue;

            result.examined++;


            // ------------------------------------------------------------
            // HIDDEN
            // ------------------------------------------------------------

            if (entity->implementationOnly)
            {
                result.skippedHidden++;
                continue;
            }


            // ------------------------------------------------------------
            // UNKNOWN
            // ------------------------------------------------------------

            if (entity->kind ==
                DomoIntrospection::EntityKind::UNKNOWN)
            {
                result.skippedUnknown++;
                continue;
            }


            // ------------------------------------------------------------
            // STATUS
            // ------------------------------------------------------------
            //
            // Le entity STATUS richiedono una proiezione semantica
            // specifica e non vengono pubblicate come sensori generici.
            //
            // ------------------------------------------------------------

            if (entity->kind ==
                DomoIntrospection::EntityKind::STATUS)
            {
                result.skippedStatus++;
                continue;
            }


            // ------------------------------------------------------------
            // DOMAIN FILTER
            // ------------------------------------------------------------

            if (!isDomainEnabled(
                    entity->kind))
            {
                result.skippedDisabled++;
                continue;
            }


            // ------------------------------------------------------------
            // DISCOVERY ENTITY
            // ------------------------------------------------------------

            if (publishEntity(
                    introspection,
                    *entity))
            {
                result.published++;
            }
            else
            {
                result.failed++;
            }
        }

        return result;
    }


    // ========================================================================
    // PUBLISH ENTITY
    // ========================================================================

    bool publishEntity(
        const DomoIntrospection& introspection,
        const DomoIntrospection::Entity& entity)
    {
        if (!_publishCallback)
            return false;

        if (entity.area < 0 &&
            entity.origin != DomoIntrospection::EntityOrigin::HVAC &&
            entity.origin != DomoIntrospection::EntityOrigin::RUNTIME)
        {
            return false;
        }

        const char* component =
            componentFor(
                entity.kind
            );

        if (!component)
            return false;


        const String objectId =
            makeObjectId(
                entity
            );

        const String uniqueId =
            makeUniqueId(
                entity
            );

        const String configTopic =
            makeConfigTopic(
                component,
                objectId
            );

        const String stateTopic =
            makeStateTopic(
                entity
            );

        const String commandTopic =
            makeCommandTopic(
                entity
            );

        const String availabilityTopic =
            makeAvailabilityTopic();

        const String name =
            makeEntityName(
                introspection,
                entity
            );


        // ------------------------------------------------------------
        // JSON
        // ------------------------------------------------------------

        StaticJsonDocument<1536> document;


        // ------------------------------------------------------------
        // IDENTITY
        // ------------------------------------------------------------

        document["name"] =
            name;

        document["unique_id"] =
            uniqueId;


        // ------------------------------------------------------------
        // STATE TOPIC
        // ------------------------------------------------------------

        if (entity.kind !=
            DomoIntrospection::EntityKind::BUTTON)
        {
            document["state_topic"] =
                stateTopic;
        }


        // ------------------------------------------------------------
        // COMMAND TOPIC
        // ------------------------------------------------------------

        if (requiresCommandTopic(
                entity.kind) &&
            entity.writable)
        {
            document["command_topic"] =
                commandTopic;
        }


        // ------------------------------------------------------------
        // AVAILABILITY
        // ------------------------------------------------------------

        if (availabilityTopic.length() > 0)
        {
            document["availability_topic"] =
                availabilityTopic;

            document["payload_available"] =
                safeText(
                    _config.payloadAvailable,
                    "online"
                );

            document["payload_not_available"] =
                safeText(
                    _config.payloadNotAvailable,
                    "offline"
                );
        }


        // ------------------------------------------------------------
        // SEMANTIC METADATA
        // ------------------------------------------------------------

        appendSemanticMetadata(
            document,
            entity
        );


        // ------------------------------------------------------------
        // COMPONENT CONFIGURATION
        // ------------------------------------------------------------

        appendComponentConfiguration(
            document,
            entity
        );


        // ------------------------------------------------------------
        // DEVICE REGISTRY
        // ------------------------------------------------------------

        appendDeviceInformation(
            document, entity
        );


        // ------------------------------------------------------------
        // SERIALIZE
        // ------------------------------------------------------------

        String payload;

        payload.reserve(
            measureJson(document) + 1
        );

        const size_t serialized =
            serializeJson(
                document,
                payload
            );

        if (serialized == 0)
            return false;


        // ------------------------------------------------------------
        // PUBLISH
        // ------------------------------------------------------------

        return _publishCallback(
            _publishContext,
            configTopic.c_str(),
            payload.c_str(),
            _config.retainDiscovery
        );
    }


    // ========================================================================
    // REMOVE ENTITY
    // ========================================================================

    bool removeEntity(
        const DomoIntrospection::Entity& entity)
    {
        if (!_publishCallback)
            return false;

        const char* component =
            componentFor(
                entity.kind
            );

        if (!component)
            return false;

        const String topic =
            makeConfigTopic(
                component,
                makeObjectId(entity)
            );

        return _publishCallback(
            _publishContext,
            topic.c_str(),
            "",
            true
        );
    }


    // ========================================================================
    // PUBLISH AVAILABILITY
    // ========================================================================

    bool publishAvailability(
        bool available)
    {
        if (!_publishCallback)
            return false;

        const String topic =
            makeAvailabilityTopic();

        if (topic.length() == 0)
            return false;

        const char* payload =
            available
                ? safeText(
                    _config.payloadAvailable,
                    "online"
                )
                : safeText(
                    _config.payloadNotAvailable,
                    "offline"
                );

        return _publishCallback(
            _publishContext,
            topic.c_str(),
            payload,
            _config.retainAvailability
        );
    }


    // ========================================================================
    // PUBLISH RAW STATE
    // ========================================================================

    bool publishState(
        const DomoIntrospection::Entity& entity,
        const char* payload)
    {
        if (!_publishCallback)
            return false;

        if (!payload)
            return false;

        if (entity.kind ==
            DomoIntrospection::EntityKind::BUTTON)
        {
            return false;
        }

        const String topic =
            makeStateTopic(
                entity
            );

        return _publishCallback(
            _publishContext,
            topic.c_str(),
            payload,
            _config.retainState
        );
    }


    // ========================================================================
    // PUBLISH INTEGER STATE
    // ========================================================================

    bool publishState(
        const DomoIntrospection::Entity& entity,
        long value)
    {
        const String payload(
            value
        );

        return publishState(
            entity,
            payload.c_str()
        );
    }


    // ========================================================================
    // PUBLISH FLOAT STATE
    // ========================================================================

    bool publishState(
        const DomoIntrospection::Entity& entity,
        float value,
        uint8_t decimals = 2)
    {
        const String payload(
            value,
            static_cast<unsigned int>(
                decimals
            )
        );

        return publishState(
            entity,
            payload.c_str()
        );
    }


    // ========================================================================
    // PUBLISH BOOLEAN STATE
    // ========================================================================

    bool publishBooleanState(
        const DomoIntrospection::Entity& entity,
        bool value)
    {
        return publishState(
            entity,
            value
                ? "ON"
                : "OFF"
        );
    }


    // ========================================================================
    // TOPIC ACCESSORS
    // ========================================================================

    String discoveryTopic(
        const DomoIntrospection::Entity& entity) const
    {
        const char* component =
            componentFor(
                entity.kind
            );

        if (!component)
            return String();

        return makeConfigTopic(
            component,
            makeObjectId(entity)
        );
    }


    String stateTopic(
        const DomoIntrospection::Entity& entity) const
    {
        return makeStateTopic(
            entity
        );
    }


    String commandTopic(
        const DomoIntrospection::Entity& entity) const
    {
        return makeCommandTopic(
            entity
        );
    }


    String availabilityTopic() const
    {
        return makeAvailabilityTopic();
    }

    String commandSubscriptionTopic() const
    {
        String topic =
            makeRuntimeTopicPrefix();

        topic += "/area/+/set";

        return topic;
    }

    // ========================================================================
    // RUNTIME COMMAND SUBSCRIPTION TOPIC
    //
    // Riceve i comandi destinati alle variabili DMRuntimeStore.
    //
    // Esempio:
    //
    //   domomanager/runtime/access.room01.card_code/set
    //
    // Il carattere '+' intercetta l'ID RuntimeStore.
    // Gli ID RuntimeStore non possono contenere '/', quindi occupano
    // sempre un solo livello MQTT.
    // ========================================================================

    String runtimeCommandSubscriptionTopic() const
    {
        String topic =
            makeRuntimeTopicPrefix();

        topic += "/runtime/+/set";

        return topic;
    }
private:

    // ========================================================================
    // STORAGE
    // ========================================================================

    PublishCallback _publishCallback =
        nullptr;

    void* _publishContext =
        nullptr;

    Config _config;

    // ========================================================================
    // DOMAIN ENABLED
    // ========================================================================

    bool isDomainEnabled(
        DomoIntrospection::EntityKind kind) const
    {
        using Kind =
            DomoIntrospection::EntityKind;

        switch (kind)
        {
            case Kind::SENSOR:
                return _config.publishSensors;

            case Kind::BINARY_SENSOR:
                return _config.publishBinarySensors;

            case Kind::SWITCH:
                return _config.publishSwitches;

            case Kind::LIGHT:
                return _config.publishLights;

            case Kind::COVER:
                return _config.publishCovers;

            case Kind::BUTTON:
                return _config.publishButtons;

            case Kind::CLIMATE:
                return _config.publishClimate;

            case Kind::NUMBER:
                return _config.publishNumbers;

            case Kind::TEXT:
                return _config.publishTexts;

            case Kind::STATUS:
            case Kind::UNKNOWN:
                return false;
        }

        return false;
    }


    // ========================================================================
    // COMPONENT
    // ========================================================================

    static const char* componentFor(
        DomoIntrospection::EntityKind kind)
    {
        using Kind =
            DomoIntrospection::EntityKind;

        switch (kind)
        {
            case Kind::SENSOR:
                return "sensor";

            case Kind::BINARY_SENSOR:
                return "binary_sensor";

            case Kind::SWITCH:
                return "switch";

            case Kind::LIGHT:
                return "light";

            case Kind::COVER:
                return "cover";

            case Kind::CLIMATE:
                return "climate";

            case Kind::BUTTON:
                return "button";

            case Kind::NUMBER:
                return "number";

            case Kind::TEXT:
                return "text";

            case Kind::STATUS:
            case Kind::UNKNOWN:
                return nullptr;
        }

        return nullptr;
    }


    // ========================================================================
    // REQUIRES COMMAND TOPIC
    // ========================================================================

    static bool requiresCommandTopic(
        DomoIntrospection::EntityKind kind)
    {
        using Kind =
            DomoIntrospection::EntityKind;

        switch (kind)
        {
            case Kind::TEXT:
                return true;

            case Kind::SWITCH:
            case Kind::LIGHT:
            case Kind::COVER:
            case Kind::CLIMATE:
            case Kind::BUTTON:
            case Kind::NUMBER:
                return true;

            case Kind::SENSOR:
            case Kind::BINARY_SENSOR:
            case Kind::STATUS:
            case Kind::UNKNOWN:
                return false;
        }

        return false;
    }


    // ========================================================================
    // SEMANTIC METADATA
    // ========================================================================

    static void appendSemanticMetadata(
        JsonDocument& document,
        const DomoIntrospection::Entity& entity)
    {
        if (hasText(
                entity.unit))
        {
            document["unit_of_measurement"] =
                entity.unit;
        }

        if (hasText(
                entity.deviceClass))
        {
            document["device_class"] =
                entity.deviceClass;
        }

        if (hasText(
                entity.stateClass))
        {
            document["state_class"] =
                entity.stateClass;
        }
    }


    // ========================================================================
    // COMPONENT CONFIGURATION
    // ========================================================================

    static void appendComponentConfiguration(
        JsonDocument& document,
        const DomoIntrospection::Entity& entity)
    {
        using Kind =
            DomoIntrospection::EntityKind;

        switch (entity.kind)
        {
            case Kind::BINARY_SENSOR:
            {
                document["payload_on"] =
                    "ON";

                document["payload_off"] =
                    "OFF";

                break;
            }

            case Kind::SWITCH:
            case Kind::LIGHT:
            {
                document["payload_on"] =
                    "ON";

                document["payload_off"] =
                    "OFF";

                document["state_on"] =
                    "ON";

                document["state_off"] =
                    "OFF";

                break;
            }

            case Kind::COVER:
            {
                document["payload_open"] =
                    "OPEN";

                document["payload_close"] =
                    "CLOSE";

                document["payload_stop"] =
                    "STOP";

                document["state_open"] =
                    "open";

                document["state_closed"] =
                    "closed";

                document["state_opening"] =
                    "opening";

                document["state_closing"] =
                    "closing";

                document["state_stopped"] =
                    "stopped";

                break;
            }

            case Kind::BUTTON:
            {
                document["payload_press"] =
                    "PRESS";

                break;
            }

            case Kind::NUMBER:
            {
                document["min"] =
                    entity.number.minimumValue;

                document["max"] =
                    entity.number.maximumValue;

                document["step"] =
                    entity.number.stepValue;

                document["suggested_display_precision"] =
                    entity.number.displayDecimals;

                switch (entity.number.mode)
                {
                    case DomoIntrospection::NumberConfig::Mode::SLIDER:
                        document["mode"] = "slider";
                        break;

                    case DomoIntrospection::NumberConfig::Mode::BOX:
                        document["mode"] = "box";
                        break;

                    case DomoIntrospection::NumberConfig::Mode::AUTO:
                    default:
                        document["mode"] = "auto";
                        break;
                }

                break;
            }

            case Kind::TEXT:
            {
                document["mode"] = "text";
                break;
            }

            case Kind::SENSOR:
            case Kind::CLIMATE:
            case Kind::STATUS:
            case Kind::UNKNOWN:
            {
                break;
            }
        }
    }
    

    // ========================================================================
    // DEVICE INFORMATION
    // ========================================================================
    //
    // Genera un dispositivo Home Assistant distinto per ogni device fisico
    // DomoManager.
    //
    // Entity associate allo stesso:
    //
    //     entity.deviceIndex
    //
    // vengono aggregate sotto lo stesso dispositivo Home Assistant.
    //
    // Le entity prive di device fisico restano associate al dispositivo
    // principale DomoManager.
    //
    // ========================================================================

    void appendDeviceInformation(
        JsonDocument& document,
        const DomoIntrospection::Entity& entity) const
    {
        JsonObject device =
            document.createNestedObject(
                "device"
            );

        String rootIdentifier =
            sanitizeTopicPart(
                safeText(
                    _config.deviceId,
                    "domomanager"
                )
            );

        if (rootIdentifier.length() == 0)
        {
            rootIdentifier =
                "domomanager";
        }

        String identifier;
        String deviceName;
        const char* manufacturer =
            safeText(
                _config.manufacturer,
                "DomoManager"
            );

        const char* model =
            safeText(
                _config.model,
                "DomoManager Controller"
            );

        // ============================================================
        // DEVICE GROUP
        // ============================================================

        switch (entity.group)
        {
            // ========================================================
            // SECURITY SYSTEM
            // ========================================================

            case DomoIntrospection::EntityGroup::SECURITY:
            {
                identifier =
                    rootIdentifier;

                identifier +=
                    "_security";

                deviceName =
                    "Sistema antifurto";

                model =
                    "DomoManager Security";

                break;
            }

            // ========================================================
            // SOLAR INVERTER
            // ========================================================

            case DomoIntrospection::EntityGroup::SOLAR:
            {
                identifier =
                    rootIdentifier;

                identifier +=
                    "_solar";

                deviceName =
                    "Inverter solare";

                model =
                    "Solar Inverter";

                break;
            }

            // ========================================================
            // PHYSICAL DEVICE
            // ========================================================

            case DomoIntrospection::EntityGroup::PHYSICAL_DEVICE:
            {
                identifier =
                    rootIdentifier;

                identifier +=
                    "_device_";

                identifier +=
                    String(
                        entity.deviceIndex
                    );

                if (entity.device &&
                    entity.device->name.length() > 0)
                {
                    deviceName =
                        entity.device->name;
                }
                else
                {
                    deviceName =
                        "DomoManager Device ";

                    deviceName +=
                        String(
                            entity.deviceIndex
                        );
                }

                if (entity.device &&
                    entity.device->profile.length() > 0)
                {
                    model =
                        entity.device->profile.c_str();
                }
                else
                {
                    model =
                        "DomoManager Device";
                }

                break;
            }

            // ========================================================
            // ROOT DOMOMANAGER
            // ========================================================

            case DomoIntrospection::EntityGroup::ROOT:
            default:
            {
                identifier =
                    rootIdentifier;

                deviceName =
                    safeText(
                        _config.deviceName,
                        "DomoManager"
                    );

                break;
            }
        }

        // ============================================================
        // IDENTIFIERS
        // ============================================================

        JsonArray identifiers =
            device.createNestedArray(
                "identifiers"
            );

        identifiers.add(
            identifier
        );

        // ============================================================
        // DEVICE METADATA
        // ============================================================

        device["name"] =
            deviceName;

        device["manufacturer"] =
            manufacturer;

        device["model"] =
            model;

        if (hasText(
                _config.softwareVersion))
        {
            device["sw_version"] =
                _config.softwareVersion;
        }

        // ============================================================
        // SUGGESTED AREA
        //
        // Valida solamente per i device fisici.
        // ============================================================

        if (entity.group ==
                DomoIntrospection::EntityGroup::PHYSICAL_DEVICE &&
            entity.device &&
            hasText(
                entity.device->suggestedArea))
        {
            device["suggested_area"] =
                entity.device->suggestedArea;
        }

        // ============================================================
        // PARENT DEVICE
        //
        // Security, Solar e device fisici vengono esposti come device
        // gestiti dal controller principale DomoManager.
        // ============================================================

        if (entity.group !=
            DomoIntrospection::EntityGroup::ROOT)
        {
            device["via_device"] =
                rootIdentifier;
        }
    }


    // ========================================================================
    // ENTITY NAME
    // ========================================================================

    static String makeEntityName(
        const DomoIntrospection& introspection,
        const DomoIntrospection::Entity& entity)
    {
        // ============================================================
        // RUNTIME STORE
        // ============================================================

        if (entity.origin ==
                DomoIntrospection::EntityOrigin::RUNTIME &&
            entity.runtimeEntry)
        {
            const char* name =
                entity.runtimeEntry->name;

            if (hasText(name))
            {
                return String(name);
            }

            if (hasText(entity.runtimeEntry->id))
            {
                return String(
                    entity.runtimeEntry->id
                );
            }

            return "Runtime";
        }

        // ============================================================
        // AREA ENTITY
        // ============================================================

        const char* sourceName =
            introspection.getAreaName(
                entity.area
            );

        if (!hasText(sourceName))
        {
            String fallback(
                "Area "
            );

            fallback +=
                String(entity.area);

            return fallback;
        }

        String name(
            sourceName
        );

        cleanupDisplayName(
            name
        );

        if (name.length() == 0)
        {
            name =
                "Area ";

            name +=
                String(entity.area);
        }

        return name;
    }


    // ========================================================================
    // CLEAN DISPLAY NAME
    // ========================================================================

    static void cleanupDisplayName(
        String& name)
    {
        name.trim();

        removePrefix(
            name,
            "DI:"
        );

        removePrefix(
            name,
            "DO:"
        );

        name.replace(
            ", NEGATO",
            ""
        );

        name.replace(
            ", negato",
            ""
        );

        name.trim();
    }


    // ========================================================================
    // REMOVE PREFIX
    // ========================================================================

    static void removePrefix(
        String& text,
        const char* prefix)
    {
        if (!prefix)
            return;

        const size_t prefixLength =
            strlen(prefix);

        if (text.length() <
            prefixLength)
        {
            return;
        }

        const String start =
            text.substring(
                0,
                prefixLength
            );

        if (!start.equalsIgnoreCase(
                prefix))
        {
            return;
        }

        text.remove(
            0,
            prefixLength
        );

        text.trim();
    }


    // ========================================================================
    // OBJECT ID
    // ========================================================================

    String makeObjectId(
    const DomoIntrospection::Entity& entity) const
    {
        if (entity.kind ==
            DomoIntrospection::EntityKind::CLIMATE)
        {
            String id("climate_");
            id += String(-entity.area);

            return id;
        }

        if (entity.origin ==
                DomoIntrospection::EntityOrigin::RUNTIME &&
            entity.runtimeEntry &&
            entity.runtimeEntry->id)
        {
            String id("runtime_");

            id += sanitizeTopicPart(
                entity.runtimeEntry->id
            );

            return id;
        }

        String objectId("area_");
        objectId += String(entity.area);

        return objectId;
    }


    // ========================================================================
    // UNIQUE ID
    // ========================================================================

    String makeUniqueId(
        const DomoIntrospection::Entity& entity) const
    {
        String uniqueId =
            sanitizeTopicPart(
                safeText(
                    _config.deviceId,
                    "domomanager"
                )
            );

        if (entity.kind ==
            DomoIntrospection::EntityKind::CLIMATE)
        {
            uniqueId += "_climate_";
            uniqueId += String(-entity.area);

            return uniqueId;
        }

        // ============================================================
        // RUNTIME STORE
        // ============================================================

        if (entity.origin ==
                DomoIntrospection::EntityOrigin::RUNTIME &&
            entity.runtimeEntry &&
            entity.runtimeEntry->id)
        {
            uniqueId += "_runtime_";

            uniqueId += sanitizeTopicPart(
                entity.runtimeEntry->id
            );

            return uniqueId;
        }

        // ============================================================
        // AREA
        // ============================================================

        uniqueId += "_area_";
        uniqueId += String(entity.area);

        return uniqueId;
    }


    // ========================================================================
    // RUNTIME TOPIC PREFIX
    // ========================================================================
    //
    // Se topicPrefix è configurato esplicitamente viene utilizzato.
    //
    // In caso contrario viene derivato automaticamente da deviceId.
    //
    // ========================================================================

    String makeRuntimeTopicPrefix() const
    {
        const char* source =
            hasText(
                _config.topicPrefix
            )
                ? _config.topicPrefix
                : safeText(
                    _config.deviceId,
                    "domomanager"
                );

        String prefix =
            sanitizeTopicPart(
                source
            );

        if (prefix.length() == 0)
        {
            prefix =
                "domomanager";
        }

        return prefix;
    }


    // ========================================================================
    // CONFIG TOPIC
    // ========================================================================

    String makeConfigTopic(
        const char* component,
        const String& objectId) const
    {
        String topic;

        topic.reserve(128);

        topic +=
            safeText(
                _config.discoveryPrefix,
                "homeassistant"
            );

        topic += "/";
        topic += component;
        topic += "/";

        topic +=
            sanitizeTopicPart(
                safeText(
                    _config.nodeId,
                    "domomanager"
                )
            );

        topic += "/";
        topic += objectId;
        topic += "/config";

        return topic;
    }


    // ========================================================================
    // STATE TOPIC
    // ========================================================================

    String makeStateTopic(
        const DomoIntrospection::Entity& entity) const
    {
        String topic =
            makeRuntimeTopicPrefix();

        if (entity.origin ==
                DomoIntrospection::EntityOrigin::RUNTIME &&
            entity.runtimeEntry &&
            hasText(
                entity.runtimeEntry->id))
        {
            topic += "/runtime/";
            topic += entity.runtimeEntry->id;
            topic += "/state";

            return topic;
        }

        topic += "/area/";
        topic += String(
            entity.area
        );
        topic += "/state";

        return topic;
    }


    // ========================================================================
    // COMMAND TOPIC
    // ========================================================================

    String makeCommandTopic(
        const DomoIntrospection::Entity& entity) const
    {
        String topic =
            makeRuntimeTopicPrefix();

        if (entity.origin ==
                DomoIntrospection::EntityOrigin::RUNTIME &&
            entity.runtimeEntry &&
            hasText(
                entity.runtimeEntry->id))
        {
            topic += "/runtime/";
            topic += entity.runtimeEntry->id;
            topic += "/set";

            return topic;
        }

        topic += "/area/";
        topic += String(
            entity.area
        );
        topic += "/set";

        return topic;
    }


    // ========================================================================
    // AVAILABILITY TOPIC
    // ========================================================================

    String makeAvailabilityTopic() const
    {
        if (hasText(
                _config.availabilityTopic))
        {
            return String(
                _config.availabilityTopic
            );
        }

        String topic =
            makeRuntimeTopicPrefix();

        topic +=
            "/status";

        return topic;
    }


    // ========================================================================
    // SANITIZE TOPIC PART
    // ========================================================================

    static String sanitizeTopicPart(
        const char* source)
    {
        String result =
            source
                ? source
                : "";

        result.toLowerCase();

        for (size_t i = 0;
             i < result.length();
             ++i)
        {
            const char c =
                result[i];

            const bool valid =
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '_' ||
                c == '-';

            if (!valid)
            {
                result.setCharAt(
                    i,
                    '_'
                );
            }
        }

        while (result.indexOf(
                   "__") >= 0)
        {
            result.replace(
                "__",
                "_"
            );
        }

        while (result.length() > 0 &&
               result[0] == '_')
        {
            result.remove(
                0,
                1
            );
        }

        while (result.length() > 0 &&
               result[result.length() - 1] == '_')
        {
            result.remove(
                result.length() - 1,
                1
            );
        }

        return result;
    }


    // ========================================================================
    // TEXT HELPERS
    // ========================================================================

    static bool hasText(
        const char* text)
    {
        return
            text &&
            text[0] != '\0';
    }


    static const char* safeText(
        const char* text,
        const char* fallback)
    {
        return
            hasText(text)
                ? text
                : fallback;
    }
};