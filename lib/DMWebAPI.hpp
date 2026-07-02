#ifndef DMWebAPI_HPP
#define DMWebAPI_HPP

#pragma once

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

#include "DMAEECore.hpp"        


#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================================
// SUPPORT STRUCTS
// ============================================================================

class IWebApiTransport {
public:
    virtual ~IWebApiTransport() {}
    virtual bool begin() = 0;
    virtual String get(const String& url) = 0;
    virtual String post(const String& url, const String& body) = 0;
};

class SimpleHttpTransport : public IWebApiTransport {
private:
    EthernetClient client;

    enum class State {
        Idle,
        Connecting,
        Sending,
        WaitingResponse,
        Reading,
        Completed,
        Error
    };

    State state = State::Idle;

    String url;
    String method;
    String body;

    String host;
    String path;

    String response;

    unsigned long startTime = 0;
    uint16_t port;
    uint32_t timeoutMs;

public:
    SimpleHttpTransport(uint16_t port = 80, uint32_t timeoutMs = 2000)
        : port(port), timeoutMs(timeoutMs) {}

    bool begin() override {
        return true;
    }

    // ------------------------------------------------------------
    //  START REQUEST (NON BLOCCANTE)
    // ------------------------------------------------------------
    void startGET(const String& url) {
        startRequest("GET", url, "");
    }

    void startPOST(const String& url, const String& body) {
        startRequest("POST", url, body);
    }

    void startRequest(const String& m, const String& u, const String& b) {
        method = m;
        url = u;
        body = b;
        response = "";

        // parse URL
        int idx = url.indexOf('/', 8);
        host = url.substring(7, idx);
        path = url.substring(idx);

        state = State::Connecting;
        startTime = millis();
    }

    // ------------------------------------------------------------
    //  LOOP NON BLOCCANTE
    // ------------------------------------------------------------
    bool loop(unsigned long now, String& outResponse) {
        switch (state) {

        case State::Idle:
            return false;

        case State::Connecting:
            if (client.connect(host.c_str(), port)) {
                state = State::Sending;
            } else if (now - startTime > timeoutMs) {
                state = State::Error;
            }
            break;

        case State::Sending:
            client.print(method);
            client.print(" ");
            client.print(path);
            client.println(" HTTP/1.1");

            client.print("Host: ");
            client.println(host);

            client.println("Connection: close");

            if (method == "POST") {
                client.println("Content-Type: application/json");
                client.print("Content-Length: ");
                client.println(body.length());
                client.println();
                client.print(body);
            } else {
                client.println();
            }

            state = State::WaitingResponse;
            startTime = now;
            break;

        case State::WaitingResponse:
            if (client.available()) {
                state = State::Reading;
            } else if (now - startTime > timeoutMs) {
                state = State::Error;
            }
            break;

        case State::Reading:
            while (client.available()) {
                char c = client.read();
                response += c;
            }

            if (!client.connected()) {
                client.stop();
                state = State::Completed;
            }
            break;

        case State::Completed: {
            int idx = response.indexOf("\r\n\r\n");
            if (idx >= 0)
                response = response.substring(idx + 4);

            outResponse = response;
            state = State::Idle;
            return true;
        }

        case State::Error:
            client.stop();
            state = State::Idle;
            return false;
        }

        return false;
    }

    // ------------------------------------------------------------
    //  COMPATIBILITÀ CON IWebApiTransport
    // ------------------------------------------------------------
    String get(const String& url) override {
        // NON BLOCCANTE → ritorna subito
        startGET(url);
        return "";
    }

    String post(const String& url, const String& body) override {
        startPOST(url, body);
        return "";
    }
};

// ************ DEFINIZIONE STRUTTURA WebAPI *******************************

// ============================================================================
// DEVICE MESSAGE PROFILE
// ============================================================================
struct DeviceMessageProfile {

    uint16_t profileId;
    const char* name;

    struct OutMessage {
        const char* key;
        const char* format;
        const char* method;     // GET / POST
        const char* endpoint;   // opzionale
    };

    struct InField {
        const char* key;
        const char* pattern;    // es: "ack=([0-9]+)"
    };

    struct CorrelationRule {
        const char* outKey;
        const char* expectedIn;
        const char* condition;  // "always", "optional", "value == 1"
    };

    struct InToRegistryMap {
        const char* inKey;
        const char* regKey;
        const char* transform;  // "int", "bool", "string", "raw"
    };

    struct StateRule {
        const char* state;
        const char* onEvent;
        const char* nextState;
        const char* action;     // messaggio OUT da inviare
    };

    const OutMessage* outMessages;
    size_t outCount;

    const InField* inFields;
    size_t inCount;

    const CorrelationRule* rules;
    size_t ruleCount;

    const InToRegistryMap* regMap;
    size_t regMapCount;

    const StateRule* stateRules;
    size_t stateRuleCount;
};

struct DeviceMessageGroup {
    const char* groupName;
    const DeviceMessageProfile* profiles;
    size_t profileCount;
};

struct ParsedField {
    String key;
    String value;
};

struct ParsedFields {
    std::vector<ParsedField> fields;

    bool has(const char* k) const {
        for (auto& f : fields) if (f.key == k) return true;
        return false;
    }
    String get(const char* k) const {
        for (auto& f : fields) if (f.key == k) return f.value;
        return "";
    }
};


// ============================================================================
// DEVICE MESSAGE ENGINE
// ============================================================================

class DeviceMessageEngine {
public:

    using PlaceholderResolver = std::function<String(const char* var)>;

    struct Context {
        const DeviceMessageGroup* groups = nullptr;
        size_t groupCount = 0;

        IWebApiTransport* transport = nullptr;
        AEERegistry* registry = nullptr;

        const char* baseUrl = nullptr;
        uint32_t responseTimeoutMs = 3000;
    };

private:

    Context ctx;

    struct PendingRequest {
        const DeviceMessageProfile* profile = nullptr;
        const DeviceMessageProfile::OutMessage* outMsg = nullptr;
        unsigned long timestamp = 0;
        bool active = false;
    };

    PendingRequest pending;

    struct ProfileState {
        const DeviceMessageProfile* profile = nullptr;
        String state = "ONLINE";
    };

    ProfileState profileState;

public:

    // =========================================================================
    // INIT
    // =========================================================================
    void Init(const Context& c) {
        ctx = c;
        if (ctx.transport) ctx.transport->begin();

        if (ctx.groups && ctx.groupCount > 0) {
            profileState.profile = &ctx.groups[0].profiles[0];
            profileState.state = "ONLINE";
        }
    }

    // =========================================================================
    // SEND OUT MESSAGE
    // =========================================================================
    bool Send(const char* profileName,
              const char* outKey,
              PlaceholderResolver resolver)
    {
        if (!ctx.transport) return false;

        const auto* profile = findProfile(profileName);
        if (!profile) return false;

        const auto* out = findOut(profile, outKey);
        if (!out) return false;

        String url = buildUrl(profile, out, resolver);
        String resp;

        if (strcmp(out->method, "GET") == 0)
            resp = ctx.transport->get(url);
        else
            resp = ctx.transport->post(url, "");

        pending.profile = profile;
        pending.outMsg = out;
        pending.timestamp = millis();
        pending.active = true;

        if (resp.length() > 0)
            OnResponse(resp, resolver);

        return true;
    }

    // =========================================================================
    // ON RESPONSE
    // =========================================================================
    void OnResponse(const String& response, PlaceholderResolver resolver) {
        if (!pending.active || !pending.profile) return;

        ParsedFields fields = parseResponse(pending.profile, response);

        // EVENTI DI STATO
        if (fields.has("ack") && fields.get("ack") == "1")
            applyStateEvent("ack_ok", resolver);

        if (fields.has("screen") || fields.has("cmd"))
            applyStateEvent("keepalive_ok", resolver);

        if (fields.has("key"))
            applyStateEvent("key", resolver);

        // CORRELAZIONE OUT→IN
        if (checkCorrelation(pending.profile, pending.outMsg, fields)) {
            pending.active = false;
            applyFieldsToRegistry(fields);
        }
    }

    // =========================================================================
    // LOOP (TIMEOUT)
    // =========================================================================
    void Loop(unsigned long now, PlaceholderResolver resolver) {
        if (pending.active &&
            now - pending.timestamp > ctx.responseTimeoutMs)
        {
            pending.active = false;
            applyStateEvent("timeout", resolver);
        }
    }

private:

    // =========================================================================
    // FIND PROFILE
    // =========================================================================
    const DeviceMessageProfile* findProfile(const char* name) const {
        for (size_t g = 0; g < ctx.groupCount; ++g) {
            const auto& grp = ctx.groups[g];
            for (size_t i = 0; i < grp.profileCount; ++i) {
                if (strcmp(grp.profiles[i].name, name) == 0)
                    return &grp.profiles[i];
            }
        }
        return nullptr;
    }

    // =========================================================================
    // FIND OUT MESSAGE
    // =========================================================================
    const DeviceMessageProfile::OutMessage*
    findOut(const DeviceMessageProfile* profile, const char* key) const {
        for (size_t i = 0; i < profile->outCount; ++i)
            if (strcmp(profile->outMessages[i].key, key) == 0)
                return &profile->outMessages[i];
        return nullptr;
    }

    // =========================================================================
    // BUILD URL WITH PLACEHOLDERS
    // =========================================================================
    String buildUrl(const DeviceMessageProfile* profile,
                    const DeviceMessageProfile::OutMessage* out,
                    PlaceholderResolver resolver) const
    {
        String base = ctx.baseUrl ? ctx.baseUrl : "";
        String fmt = out->format ? out->format : "";

        // ${var}
        int start;
        while ((start = fmt.indexOf("${")) >= 0) {
            int end = fmt.indexOf("}", start);
            if (end < 0) break;
            String var = fmt.substring(start + 2, end);
            String val = resolver(var.c_str());
            fmt = fmt.substring(0, start) + val + fmt.substring(end + 1);
        }

        // $var
        for (;;) {
            int pos = fmt.indexOf('$');
            if (pos < 0) break;
            int end = pos + 1;
            while (end < fmt.length() &&
                   (isalnum(fmt[end]) || fmt[end] == '_'))
                end++;
            String var = fmt.substring(pos + 1, end);
            String val = resolver(var.c_str());
            fmt = fmt.substring(0, pos) + val + fmt.substring(end);
        }

        if (fmt.startsWith("/"))
            return base + fmt;
        return base + "/" + fmt;
    }

    // =========================================================================
    // PARSE RESPONSE (key=value per linea)
    // =========================================================================
    ParsedFields parseResponse(const DeviceMessageProfile* profile,
                               const String& resp) const
    {
        ParsedFields result;
        int start = 0;

        while (start < resp.length()) {
            int end = resp.indexOf('\n', start);
            if (end < 0) end = resp.length();

            String line = resp.substring(start, end);
            line.trim();
            start = end + 1;

            if (line.length() == 0) continue;

            int eq = line.indexOf('=');
            if (eq <= 0) continue;

            ParsedField f;
            f.key = line.substring(0, eq);
            f.value = line.substring(eq + 1);
            f.key.trim();
            f.value.trim();

            result.fields.push_back(f);
        }

        return result;
    }

    // =========================================================================
    // CORRELATION OUT→IN
    // =========================================================================
    bool checkCorrelation(const DeviceMessageProfile* profile,
                          const DeviceMessageProfile::OutMessage* out,
                          const ParsedFields& fields) const
    {
        for (size_t i = 0; i < profile->ruleCount; ++i) {
            const auto& r = profile->rules[i];
            if (strcmp(r.outKey, out->key) != 0) continue;

            bool present = fields.has(r.expectedIn);
            String val = fields.get(r.expectedIn);

            if (strcmp(r.condition, "always") == 0 && !present)
                return false;

            if (strcmp(r.condition, "value == 1") == 0 &&
                (!present || val.toInt() != 1))
                return false;
        }
        return true;
    }

    // =========================================================================
    // APPLY FIELDS TO REGISTRY
    // =========================================================================
    void applyFieldsToRegistry(const ParsedFields& fields) {
        if (!ctx.registry || !pending.profile) return;

        const auto* profile = pending.profile;
        
        unsigned long now=millis();
        for (size_t i = 0; i < profile->regMapCount; ++i) {
            const auto& m = profile->regMap[i];

            // Se il campo IN non è presente nella risposta, salta
            if (!fields.has(m.inKey))
                continue;

            String val = fields.get(m.inKey);

            // Trova la variabile AEE corrispondente
            AEEVariableBase* v = ctx.registry->find(m.regKey);
            if (!v) {
                LOG_WF("DevMsg", "Registry key '%s' non trovata", m.regKey);
                continue;
            }

            // ------------------------------
            // INT
            // ------------------------------
            if (strcmp(m.transform, "int") == 0) {
                if (auto* vi = as<int>(v)) {
                    vi->set(val.toInt(), now);
                    LOG_IF("DevMsg", "AEE[%s] = %d", m.regKey, val.toInt());
                } else {
                    LOG_WF("DevMsg", "Tipo AEE non compatibile (int) per '%s'", m.regKey);
                }
                continue;
            }

            // ------------------------------
            // BOOL
            // ------------------------------
            if (strcmp(m.transform, "bool") == 0) {
                bool b = (val == "1" || val == "true" || val == "TRUE");
                if (auto* vb = as<bool>(v)) {
                    vb->set(b, now);
                    LOG_IF("DevMsg", "AEE[%s] = %d", m.regKey, b);
                } else {
                    LOG_WF("DevMsg", "Tipo AEE non compatibile (bool) per '%s'", m.regKey);
                }
                continue;
            }

            // ------------------------------
            // FLOAT
            // ------------------------------
            if (strcmp(m.transform, "float") == 0) {
                float f = val.toFloat();
                if (auto* vf = as<float>(v)) {
                    vf->set(f, now);
                    LOG_IF("DevMsg", "AEE[%s] = %.3f", m.regKey, f);
                } else {
                    LOG_WF("DevMsg", "Tipo AEE non compatibile (float) per '%s'", m.regKey);
                }
                continue;
            }

            // ------------------------------
            // STRING (default)
            // ------------------------------
            if (auto* vs = as<String>(v)) {
                vs->set(val, now);
                LOG_IF("DevMsg", "AEE[%s] = '%s'", m.regKey, val.c_str());
            } else {
                LOG_WF("DevMsg", "Tipo AEE non compatibile (string/raw) per '%s'", m.regKey);
            }
        }
    }


    // =========================================================================
    // STATE MACHINE
    // =========================================================================
    void applyStateEvent(const char* event, PlaceholderResolver resolver) {
        const auto* profile = profileState.profile;
        if (!profile || !profile->stateRules) return;

        for (size_t i = 0; i < profile->stateRuleCount; ++i) {
            const auto& r = profile->stateRules[i];

            if (profileState.state == r.state &&
                strcmp(event, r.onEvent) == 0)
            {
                profileState.state = r.nextState;

                if (r.action)
                    Send(profile->name, r.action, resolver);

                return;
            }
        }
    }
};

#endif
