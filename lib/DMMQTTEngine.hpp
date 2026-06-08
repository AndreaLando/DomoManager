#ifndef DMMQTT_ENGINE_HPP
#define DMMQTT_ENGINE_HPP

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
#include <vector>
#include <ArduinoJson.h>
#include <Ethernet.h>


#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ======================================================
// 1) VAR STRUCT
// ======================================================

struct MQTTEngine_Var {
    const char* id;
    const char* name;
    const char* unit;

    enum class HAType {
        SENSOR,
        BINARY_SENSOR,
        SWITCH,
        LIGHT_RGBW,
        COVER,
        CLIMATE
    } type;

    float*    analogPtr    = nullptr;
    bool*     boolPtr      = nullptr;

    uint16_t* r = nullptr;
    uint16_t* g = nullptr;
    uint16_t* b = nullptr;
    uint16_t* w = nullptr;

    uint16_t*      coverPos     = nullptr;
    bool*     coverCmdOpen = nullptr;
    bool*     coverCmdClose= nullptr;

    float*    tempCurrent  = nullptr;
    float*    tempSetpoint = nullptr;
    bool*     hvacOn       = nullptr;
};

using Var = MQTTEngine_Var;

// ======================================================
// 2) BACKEND BASE (classe indipendente)
// ======================================================

class BackendBase {
public:
    virtual ~BackendBase() {}

    virtual String topicState(const char* nodeId, const Var& v) = 0;
    virtual String topicCommand(const char* nodeId, const Var& v) = 0;
    virtual String topicClimateSetpoint(const char* nodeId, const Var& v) = 0;

    virtual String buildPayload(const Var& v) = 0;

    virtual void parseCommand(Var& v,
                              const String& msg,
                              const String& fullTopic) = 0;

    virtual void publishDiscovery(PubSubClient& mqtt,
                                  const char* nodeId,
                                  Var* vars,
                                  size_t varCount) = 0;
};

// ======================================================
// 3) BACKEND HOME ASSISTANT
// ======================================================

class BackendHA : public BackendBase {
public:
    String topicState(const char* nodeId, const Var& v) override {
        return String("homeassistant/state/") + nodeId + "/" + v.id;
    }

    String topicCommand(const char* nodeId, const Var& v) override {
        return String("homeassistant/cmd/") + nodeId + "/" + v.id;
    }

    String topicClimateSetpoint(const char* nodeId, const Var& v) override {
        return String("homeassistant/cmd/") + nodeId + "/" + v.id + "/setpoint";
    }

    String buildPayload(const Var& v) override {
        switch (v.type) {
            case Var::HAType::SENSOR:
                return v.analogPtr ? String(*v.analogPtr, 2) : "";
            case Var::HAType::BINARY_SENSOR:
            case Var::HAType::SWITCH:
                return v.boolPtr ? (*v.boolPtr ? "ON" : "OFF") : "";
            case Var::HAType::LIGHT_RGBW: {
                String p = "{";
                p += "\"r\":" + String(v.r ? *v.r : 0) + ",";
                p += "\"g\":" + String(v.g ? *v.g : 0) + ",";
                p += "\"b\":" + String(v.b ? *v.b : 0) + ",";
                p += "\"w\":" + String(v.w ? *v.w : 0);
                p += "}";
                return p;
            }
            case Var::HAType::COVER:
                return v.coverPos ? String(*v.coverPos) : "";
            case Var::HAType::CLIMATE: {
                String p = "{";
                p += "\"temperature\":" + String(v.tempCurrent ? *v.tempCurrent : 0, 1) + ",";
                p += "\"setpoint\":" + String(v.tempSetpoint ? *v.tempSetpoint : 0, 1) + ",";
                p += "\"hvac_on\":" + String((v.hvacOn && *v.hvacOn) ? "true" : "false");
                p += "}";
                return p;
            }
        }
        return "";
    }

    void parseCommand(Var& v,
                      const String& msg,
                      const String& fullTopic) override
    {
        if (v.type == Var::HAType::CLIMATE &&
            fullTopic.endsWith("/setpoint"))
        {
            if (v.tempSetpoint) *v.tempSetpoint = msg.toFloat();
            return;
        }

        if (v.type == Var::HAType::SWITCH && v.boolPtr) {
            if (msg == "ON")  *v.boolPtr = true;
            if (msg == "OFF") *v.boolPtr = false;
        }
    }

    void publishDiscovery(PubSubClient& mqtt,
                          const char* nodeId,
                          Var* vars,
                          size_t varCount) override
    {
        for (size_t i = 0; i < varCount; i++) {
            const Var& v = vars[i];

            String type;
            switch (v.type) {
                case Var::HAType::SENSOR:        type = "sensor"; break;
                case Var::HAType::BINARY_SENSOR: type = "binary_sensor"; break;
                case Var::HAType::SWITCH:        type = "switch"; break;
                case Var::HAType::LIGHT_RGBW:    type = "light"; break;
                case Var::HAType::COVER:         type = "cover"; break;
                case Var::HAType::CLIMATE:       type = "climate"; break;
            }

            String uid = String(nodeId) + "_" + v.id;
            String topic = "homeassistant/" + type + "/" + uid + "/config";

            String payload = "{";
            payload += "\"name\":\"" + String(v.name) + "\",";
            payload += "\"unique_id\":\"" + uid + "\",";
            payload += "\"state_topic\":\"" + topicState(nodeId, v) + "\"";

            if (v.type == Var::HAType::CLIMATE)
                payload += ",\"temperature_command_topic\":\"" + topicClimateSetpoint(nodeId, v) + "\"";

            payload += "}";

            mqtt.publish(topic.c_str(), payload.c_str(), true);
        }
    }
};

// ======================================================
// 4) BACKEND ZIGBEE2MQTT
// ======================================================

class BackendZ2M : public BackendBase {
public:
    String topicState(const char* nodeId, const Var& v) override {
        return String("zigbee2mqtt/") + nodeId + "/" + v.id;
    }

    String topicCommand(const char* nodeId, const Var& v) override {
        return String("zigbee2mqtt/") + nodeId + "/" + v.id + "/set";
    }

    String topicClimateSetpoint(const char* nodeId, const Var& v) override {
        return String("zigbee2mqtt/") + nodeId + "/" + v.id + "/setpoint/set";
    }

    String buildPayload(const Var& v) override {
        // simile a HA ma formato Z2M
        String p = "{";
        if (v.type == Var::HAType::SENSOR && v.analogPtr)
            p += "\"" + String(v.id) + "\":" + String(*v.analogPtr, 2);
        p += "}";
        return p;
    }

    void parseCommand(Var& v,
                      const String& msg,
                      const String& fullTopic) override
    {
        // implementazione Z2M
    }

    void publishDiscovery(PubSubClient& mqtt,
                          const char* nodeId,
                          Var* vars,
                          size_t varCount) override
    {
        // Z2M non usa discovery
    }
};

// ======================================================
// 5) BACKEND SHELLY
// ======================================================

class BackendShelly : public BackendBase {
public:
    String topicState(const char* nodeId, const Var& v) override {
        return String("shellies/") + nodeId + "/" + v.id;
    }

    String topicCommand(const char* nodeId, const Var& v) override {
        return String("shellies/") + nodeId + "/" + v.id + "/command";
    }

    String topicClimateSetpoint(const char*, const Var&) override {
        return "";
    }

    String buildPayload(const Var& v) override {
        if (v.boolPtr)
            return *v.boolPtr ? "on" : "off";
        if (v.analogPtr)
            return String(*v.analogPtr, 2);
        return "";
    }

    void parseCommand(Var& v,
                      const String& msg,
                      const String&) override
    {
        if (v.boolPtr) {
            if (msg == "on")  *v.boolPtr = true;
            if (msg == "off") *v.boolPtr = false;
        }
    }

    void publishDiscovery(PubSubClient&, const char*, Var*, size_t) override {}
};

// ======================================================
// 6) MQTT (usa backend separati)
// ======================================================

class MQTT {
public:
    enum class Backend {
        HOME_ASSISTANT,
        ZIGBEE2MQTT,
        SHELLY
    };

    MQTT(EthernetClient& ethClient,
               PubSubClient&   mqttClient,
               Var*            variables,
               size_t          count,
               const char*     node);

    ~MQTT();

    void setBackend(Backend b);
    void begin(const IPAddress& broker, uint16_t port);
    void loop(unsigned long now);

private:
    BackendBase* backendImpl = nullptr;

    EthernetClient& eth;
    PubSubClient&   mqtt;

    const char* nodeId;
    Var*        vars;
    size_t      varCount;

    unsigned long heartbeatIntervalMs = 300000;

    void reconnect();
    void subscribeCommands();
    void publishStates(unsigned long now);
    void onMessage(char* topic, byte* payload, unsigned int len);

    static void mqttCallback(char* topic, uint8_t* payload, unsigned int len);
    static MQTT* self;
};

// ======================================================
// 7) IMPLEMENTAZIONE MQTT
// ======================================================

MQTT* MQTT::self = nullptr;

MQTT::MQTT(EthernetClient& ethClient,
                       PubSubClient&   mqttClient,
                       Var*            variables,
                       size_t          count,
                       const char*     node)
    : eth(ethClient),
      mqtt(mqttClient),
      vars(variables),
      varCount(count),
      nodeId(node)
{}

MQTT::~MQTT() {
    delete backendImpl;
}

void MQTT::setBackend(Backend b) {
    delete backendImpl;

    switch (b) {
        case Backend::HOME_ASSISTANT: backendImpl = new BackendHA(); break;
        case Backend::ZIGBEE2MQTT:    backendImpl = new BackendZ2M(); break;
        case Backend::SHELLY:         backendImpl = new BackendShelly(); break;
    }
}

void MQTT::begin(const IPAddress& broker, uint16_t port) {
    self = this;
    mqtt.setServer(broker, port);
    mqtt.setCallback(mqttCallback);
    reconnect();
}

void MQTT::loop(unsigned long now) {
    if (!mqtt.connected()) {
        reconnect();
        return;
    }
    mqtt.loop();
    publishStates(now);
}

void MQTT::reconnect() {
    if (mqtt.connect(nodeId)) {
        subscribeCommands();
        backendImpl->publishDiscovery(mqtt, nodeId, vars, varCount);
    }
}

void MQTT::subscribeCommands() {
    for (size_t i = 0; i < varCount; i++) {
        mqtt.subscribe(backendImpl->topicCommand(nodeId, vars[i]).c_str());
    }
}

void MQTT::publishStates(unsigned long now) {
    for (size_t i = 0; i < varCount; i++) {
        String topic = backendImpl->topicState(nodeId, vars[i]);
        String payload = backendImpl->buildPayload(vars[i]);
        mqtt.publish(topic.c_str(), payload.c_str());
    }
}

void MQTT::mqttCallback(char* topic, uint8_t* payload, unsigned int len) {
    if (self) self->onMessage(topic, payload, len);
}

void MQTT::onMessage(char* topic, byte* payload, unsigned int len) {
    String msg;
    for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];

    String t(topic);

    for (size_t i = 0; i < varCount; i++) {
        backendImpl->parseCommand(vars[i], msg, t);
    }
}

#endif

