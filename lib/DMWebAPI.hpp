#ifndef DMWebAPI_HPP
#define DMWebAPI_HPP


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



/* ============================================================================
   Note:
                    • Separazione profilo / dispositivo
                    • Host e porta configurabili per dispositivo
                    • Eliminata dipendenza da AEE
                    • Eliminato DeviceMessageGroup
                    • Polling multi-device non bloccante
                    • Una sola richiesta HTTP in volo
                    • Supporto GET e POST
                    • Logging delle risposte ricevute

   ========================================================================= */

#include <Arduino.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <deque>
#include <functional>
#include <vector>

#include "DMTransport.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"



// ============================================================================
// PARSED RESPONSE
// ============================================================================

struct ParsedField
{
    String key;
    String value;
};

struct ParsedFields
{
    std::vector<ParsedField> fields;

    bool has(
        const char* key) const
    {
        if (!key)
            return false;

        for (const auto& field : fields)
        {
            if (
                field.key.equalsIgnoreCase(
                    key
                )
            )
            {
                return true;
            }
        }

        return false;
    }

    String get(
        const char* key) const
    {
        if (!key)
            return "";

        for (const auto& field : fields)
        {
            if (
                field.key.equalsIgnoreCase(
                    key
                )
            )
            {
                return field.value;
            }
        }

        return "";
    }

    void clear()
    {
        fields.clear();
    }

    size_t size() const
    {
        return fields.size();
    }
};

struct ParsedResponse
{
    ParsedFields fields;

    bool jsonValid = false;

    DynamicJsonDocument json
    {
        4096
    };
};

struct JsonResponse
{
    bool valid = false;

    DynamicJsonDocument doc
    {
        4096
    };
};

// ============================================================================
// DEVICE MESSAGE PROFILE
//
// Il profilo descrive il protocollo logico del dispositivo.
// Non contiene indirizzo IP, porta o stato runtime.
// ============================================================================

struct DeviceMessageProfile
{
    enum class ResponseFormat : uint8_t
    {
        KEY_VALUE,
        JSON,
        RAW
    };

    struct OutMessage
    {
        const char* key;
        const char* format;
        const char* method;
        const char* endpoint;

        ResponseFormat responseFormat =
            ResponseFormat::KEY_VALUE;
    };

    struct InField
    {
        const char* key;
        const char* pattern;
    };

    struct CorrelationRule
    {
        const char* outKey;
        const char* expectedIn;
        const char* condition;
    };

    struct StateRule
    {
        const char* state;
        const char* onEvent;
        const char* nextState;
        const char* action;
    };

    uint16_t profileId = 0;

    const char* name = nullptr;

    const OutMessage* outMessages = nullptr;
    size_t outCount = 0;

    const InField* inFields = nullptr;
    size_t inCount = 0;

    const CorrelationRule* rules = nullptr;
    size_t ruleCount = 0;

    const StateRule* stateRules = nullptr;
    size_t stateRuleCount = 0;
};


// ============================================================================
// WEB API DEVICE
//
// Rappresenta una singola istanza fisica.
//
// Più dispositivi possono condividere lo stesso profilo.
// ============================================================================

struct WebApiDevice
{
    const char* name = nullptr;

    const DeviceMessageProfile* profile = nullptr;

    const char* host = nullptr;

    uint16_t port = 80;

    /*
     * Intervallo del polling automatico.
     *
     * 0 = polling automatico disabilitato.
     */
    uint32_t intervalMs = 0;

    /*
     * Messaggio OUT utilizzato dal polling automatico.
     *
     * Esempio:
     *   "KEEPALIVE"
     *   "ONLINE"
     *
     * nullptr = polling automatico disabilitato.
     */
    const char* pollMessageKey = nullptr;

    bool enabled = true;

    // ========================================================================
    // OPTIONAL HTTP AUTHENTICATION
    // ========================================================================

    HttpCredentials credentials;
};


// ============================================================================
// DEVICE MESSAGE ENGINE
//
// Responsabilità:
//   • polling multi-device
//   • costruzione URL
//   • placeholder
//   • invio HTTP
//   • parsing key=value
//   • correlazione OUT -> IN
//   • state machine per singolo dispositivo
//   • logging
//
// Non dipende da:
//   • AEE
//   • EventManager
//   • BufferEngine
// ============================================================================

class DeviceMessageEngine
{
public:

    using PlaceholderResolver =
        std::function<String(
            const WebApiDevice& device,
            const char* variable)>;

    using ResponseCallback =
    std::function<void(
        const WebApiDevice& device,
        const DeviceMessageProfile::OutMessage& message,
        const String& rawResponse,
        const ParsedResponse& parsed)>;


    struct Context
    {
        const WebApiDevice* devices = nullptr;

        size_t deviceCount = 0;

        IHttpTransport* transport = nullptr;

        uint32_t responseTimeoutMs = 3000;

        PlaceholderResolver resolver;

        ResponseCallback onResponse;
    };


private:

    struct DeviceRuntime
    {
        const WebApiDevice* device = nullptr;

        String state = "ONLINE";

        unsigned long lastPoll = 0;

        bool pollInitialized = false;
    };


    struct QueuedRequest
    {
        const WebApiDevice* device = nullptr;

        const DeviceMessageProfile::OutMessage*
            message = nullptr;

        String body;
    };


    struct PendingRequest
    {
        const WebApiDevice* device = nullptr;

        const DeviceMessageProfile* profile = nullptr;

        const DeviceMessageProfile::OutMessage*
            message = nullptr;

        unsigned long timestamp = 0;

        bool active = false;
    };


    Context ctx;

    std::vector<DeviceRuntime> runtimes;

    std::deque<QueuedRequest> requestQueue;

    PendingRequest pending;

    // ========================================================================
    // RUNTIME LIMITS
    // ========================================================================

    static constexpr size_t
        MAX_QUEUED_REQUESTS =
            16;

    static constexpr size_t
        MAX_JSON_RESPONSE_SIZE =
            8192;
public:

    // ========================================================================
    // INIT
    // ========================================================================

    bool Init(const Context& context)
    {
        ctx = context;

        runtimes.clear();
        requestQueue.clear();
        pending = PendingRequest{};

        if (
            !ctx.transport ||
            !ctx.devices ||
            ctx.deviceCount == 0
        )
        {
            LOG_E(
                "WEBAPI",
                "Invalid WebAPI context"
            );

            return false;
        }

        if (!ctx.transport->begin())
        {
            LOG_E(
                "WEBAPI",
                "Transport initialization failed"
            );

            return false;
        }

        runtimes.reserve(ctx.deviceCount);

        for (
            size_t i = 0;
            i < ctx.deviceCount;
            ++i
        )
        {
            const auto& device =
                ctx.devices[i];

            if (
                !device.name ||
                !device.profile ||
                !device.host
            )
            {
                LOG_WF(
                    "WEBAPI",
                    "Invalid device at index %u",
                    static_cast<unsigned>(i)
                );

                continue;
            }

            DeviceRuntime runtime;

            runtime.device =
                &device;

            runtime.state =
                "ONLINE";

            runtime.lastPoll = 0;
            runtime.pollInitialized = false;

            runtimes.push_back(runtime);

            LOG_DF(
                "WEBAPI",
                "Device '%s' profile='%s' host=%s port=%u",
                device.name,
                device.profile->name
                    ? device.profile->name
                    : "",
                device.host,
                device.port
            );
        }

        LOG_IF(
            "WEBAPI",
            "Initialized with %u devices",
            static_cast<unsigned>(
                runtimes.size()
            )
        );

        return !runtimes.empty();
    }

    // ========================================================================
    // LOOP
    //
    // Esegue il ciclo completo del client WebAPI:
    //   1. avanza il transport HTTP
    //   2. controlla il timeout applicativo
    //   3. pianifica il polling dei dispositivi
    //   4. avvia la prossima richiesta accodata
    // ========================================================================

    void Loop(unsigned long now)
    {
        processTransport(now);
        processPendingTimeout(now);
        schedulePolling(now);
        startNextRequest(now);
    }

    // ========================================================================
    // SEND
    //
    // Accoda un messaggio associato a un dispositivo.
    // ========================================================================

    bool Send(
        const WebApiDevice* device,
        const char* outKey,
        const String& body = "")
    {
        // ====================================================
        // ARGUMENT VALIDATION
        // ====================================================

        if (
            !device ||
            !device->enabled ||
            !device->profile ||
            !outKey ||
            outKey[0] == '\0'
        )
        {
            return false;
        }

        // ====================================================
        // MESSAGE LOOKUP
        // ====================================================

        const DeviceMessageProfile::OutMessage*
            message =
                findOut(
                    device->profile,
                    outKey
                );

        if (!message)
        {
            LOG_WF(
                "WEBAPI",
                "Message '%s' not found for device '%s'",
                outKey,
                device->name
                    ? device->name
                    : ""
            );

            return false;
        }

        // ====================================================
        // DUPLICATE SUPPRESSION
        // ====================================================

        if (
            isQueued(
                device,
                message
            )
        )
        {
            return true;
        }

        // ====================================================
        // QUEUE LIMIT
        // ====================================================

        if (
            requestQueue.size() >=
            MAX_QUEUED_REQUESTS
        )
        {
            LOG_WF(
                "WEBAPI",
                "[%s] Request queue full: "
                "message=%s queued=%u limit=%u",
                device->name
                    ? device->name
                    : "",
                message->key
                    ? message->key
                    : "",
                static_cast<unsigned>(
                    requestQueue.size()
                ),
                static_cast<unsigned>(
                    MAX_QUEUED_REQUESTS
                )
            );

            return false;
        }

        // ====================================================
        // QUEUE REQUEST
        // ====================================================

        QueuedRequest request;

        request.device =
            device;

        request.message =
            message;

        request.body =
            body;

        requestQueue.push_back(
            request
        );

        return true;
    }


    // ========================================================================
    // SEND BY DEVICE NAME
    // ========================================================================

    bool Send(
        const char* deviceName,
        const char* outKey,
        const String& body = "")
    {
        return Send(
            findDevice(deviceName),
            outKey,
            body
        );
    }


    // ========================================================================
    // STATUS
    // ========================================================================

    bool isBusy() const
    {
        return
            pending.active ||
            (
                ctx.transport &&
                ctx.transport->isBusy()
            ) ||
            !requestQueue.empty();
    }


    size_t queuedRequests() const
    {
        return requestQueue.size();
    }


    const WebApiDevice*
    findDevice(const char* name) const
    {
        if (!name)
            return nullptr;

        for (const auto& runtime : runtimes)
        {
            if (
                runtime.device &&
                runtime.device->name &&
                strcmp(
                    runtime.device->name,
                    name
                ) == 0
            )
            {
                return runtime.device;
            }
        }

        return nullptr;
    }


    String getDeviceState(
        const char* deviceName) const
    {
        const auto* runtime =
            findRuntime(deviceName);

        return
            runtime
                ? runtime->state
                : "";
    }


private:

    // ========================================================================
    // TRANSPORT PROCESSING
    // ========================================================================

    void processTransport(unsigned long now)
    {
        if (!ctx.transport)
            return;

        ctx.transport->loop(now);
    }

    // ========================================================================
    // RESPONSE PROCESSING
    // ========================================================================

    void processResponse(
        const String& response)
    {
        if (
            !pending.active ||
            !pending.device ||
            !pending.profile ||
            !pending.message
        )
        {
            return;
        }

        const WebApiDevice* device =
            pending.device;

        const DeviceMessageProfile* profile =
            pending.profile;

        const auto* message =
            pending.message;

        LOG_DF(
            "WEBAPI",
            "[%s] RAW RESPONSE: %s",
            device->name
                ? device->name
                : "",
            response.c_str()
        );

        ParsedResponse parsed;

        bool correlated = true;

        switch (
            message->responseFormat
        )
        {
            case DeviceMessageProfile::
                ResponseFormat::
                    KEY_VALUE:
            {
                parsed.fields =
                    parseResponse(
                        response
                    );

                dumpFields(
                    *device,
                    parsed.fields
                );

                correlated =
                    checkCorrelation(
                        profile,
                        message,
                        parsed.fields
                    );

                break;
            }

            case DeviceMessageProfile::
                ResponseFormat::
                    JSON:
            {
                parsed.jsonValid =
                    parseJson(
                        response,
                        parsed.json
                    );

                correlated =
                    parsed.jsonValid;

                break;
            }

            case DeviceMessageProfile::
                ResponseFormat::
                    RAW:
            {
                correlated =
                    response.length() > 0;

                break;
            }
        }

        /*
        * Libera la richiesta prima della callback.
        * La callback può accodare nuove richieste.
        */
        pending =
            PendingRequest{};

        if (!correlated)
        {
            LOG_WF(
                "WEBAPI",
                "[%s] Response validation failed for '%s'",
                device->name
                    ? device->name
                    : "",
                message->key
                    ? message->key
                    : ""
            );

            return;
        }

        /*
        * Eventi stato solo per protocolli KEY_VALUE.
        */
        if (
            message->responseFormat ==
            DeviceMessageProfile::
                ResponseFormat::
                    KEY_VALUE
        )
        {
            if (
                parsed.fields.has("ack") &&
                parsed.fields.get("ack") == "1"
            )
            {
                applyStateEvent(
                    device,
                    "ack_ok"
                );
            }

            if (
                parsed.fields.has("screen") ||
                parsed.fields.has("cmd")
            )
            {
                applyStateEvent(
                    device,
                    "keepalive_ok"
                );
            }

            if (parsed.fields.has("key"))
            {
                applyStateEvent(
                    device,
                    "key"
                );
            }
        }

        if (ctx.onResponse)
        {
            ctx.onResponse(
                *device,
                *message,
                response,
                parsed
            );
        }
    }


    // ========================================================================
    // PENDING TIMEOUT
    // ========================================================================

    void processPendingTimeout(
        unsigned long now)
    {
        if (!pending.active)
        {
            return;
        }

        if (
            static_cast<uint32_t>(
                now - pending.timestamp
            ) < ctx.responseTimeoutMs
        )
        {
            return;
        }

        const WebApiDevice* device =
            pending.device;

        const DeviceMessageProfile::OutMessage* message =
            pending.message;

        const bool isPollingMessage =
            device &&
            message &&
            device->pollMessageKey &&
            device->pollMessageKey[0] != '\0' &&
            message->key &&
            strcmp(
                device->pollMessageKey,
                message->key
            ) == 0;

        LOG_WF(
            "WEBAPI",
            "[%s] Application response timeout: "
            "message=%s polling=%u",
            (
                device &&
                device->name
            )
                ? device->name
                : "",
            (
                message &&
                message->key
            )
                ? message->key
                : "",
            isPollingMessage
                ? 1U
                : 0U
        );

        if (ctx.transport)
        {
            ctx.transport->cancel();
        }

        pending =
            PendingRequest{};

        /*
        * Un timeout di polling può modificare lo stato
        * del dispositivo.
        *
        * Un comando esplicito fallito non deve dichiarare
        * automaticamente tutto il dispositivo OFFLINE.
        */
        if (
            device &&
            isPollingMessage
        )
        {
            applyStateEvent(
                device,
                "timeout"
            );
        }
    }


    // ========================================================================
    // POLLING SCHEDULER
    // ========================================================================

    void schedulePolling(unsigned long now)
    {
        for (auto& runtime : runtimes)
        {
            const WebApiDevice* device =
                runtime.device;

            if (
                !device ||
                !device->enabled ||
                !device->profile ||
                !device->pollMessageKey ||
                device->intervalMs == 0
            )
            {
                continue;
            }

            /*
            * Il primo polling viene pianificato dopo il primo
            * intervallo completo.
            *
            * Evita traffico HTTP durante la fase iniziale di boot.
            */
            if (!runtime.pollInitialized)
            {
                runtime.pollInitialized =
                    true;

                runtime.lastPoll =
                    now;

                continue;
            }

            if (
                static_cast<uint32_t>(
                    now - runtime.lastPoll
                ) < device->intervalMs
            )
            {
                continue;
            }

            if (
                Send(
                    device,
                    device->pollMessageKey
                )
            )
            {
                runtime.lastPoll =
                    now;
            }
        }
    }


    // ========================================================================
    // START NEXT REQUEST
    // ========================================================================

    void startNextRequest(
        unsigned long now)
    {
        if (
            !ctx.transport ||
            pending.active ||
            ctx.transport->isBusy() ||
            requestQueue.empty()
        )
        {
            return;
        }

        const QueuedRequest queued =
            requestQueue.front();

        requestQueue.pop_front();

        if (
            !queued.device ||
            !queued.device->enabled ||
            !queued.device->profile ||
            !queued.message
        )
        {
            LOG_WF(
                "WEBAPI",
                "Invalid queued request"
            );

            return;
        }

        // ====================================================
        // URL
        // ====================================================

        const String url =
            buildUrl(
                queued.device,
                queued.message
            );

        if (url.length() == 0)
        {
            LOG_WF(
                "WEBAPI",
                "[%s] Empty URL for request '%s'",
                queued.device->name
                    ? queued.device->name
                    : "",
                queued.message->key
                    ? queued.message->key
                    : ""
            );

            return;
        }

        // ====================================================
        // HTTP REQUEST
        // ====================================================

        HttpRequest httpRequest;

        httpRequest.method =
            queued.message->method
                ? queued.message->method
                : "GET";

        httpRequest.url =
            url;

        httpRequest.body =
            queued.body;

        httpRequest.timeoutMs =
            ctx.responseTimeoutMs;

        /*
        * Propaga al transport le credenziali opzionali
        * configurate nel WebApiDevice.
        */
        httpRequest.credentials =
            queued.device->credentials;

        // ====================================================
        // PENDING REQUEST
        //
        // Configurato prima dell'avvio del transport perché
        // il callback può essere eseguito immediatamente.
        // ====================================================

        pending.device =
            queued.device;

        pending.profile =
            queued.device->profile;

        pending.message =
            queued.message;

        pending.timestamp =
            now;

        pending.active =
            true;

        // ====================================================
        // TRANSPORT REQUEST
        // ====================================================

        const bool started =
            ctx.transport->request(
                httpRequest,
                [this](
                    const HttpResponse& response)
                {
                    if (!pending.active)
                    {
                        return;
                    }

                    const WebApiDevice* device =
                        pending.device;

                    const DeviceMessageProfile::OutMessage*
                        message =
                            pending.message;

                    const char* deviceName =
                        (
                                    device->name
                        )
                            ? device->name
                            : "";

                    const char* messageKey =
                        (
                            message &&
                            message->key
                        )
                            ? message->key
                            : "";

                    // ============================================
                    // HTTP FAILURE
                    // ============================================

                    if (!response.success)
                    {
                        LOG_WF(
                            "WEBAPI",
                            "[%s] HTTP request failed: "
                            "message=%s status=%d",
                            deviceName,
                            messageKey,
                            response.statusCode
                        );

                        /*
                        * Copia il puntatore prima di azzerare
                        * pending.
                        */
                        const WebApiDevice* failedDevice =
                            device;

                        pending =
                            PendingRequest{};

                        if (failedDevice)
                        {
                            applyStateEvent(
                                failedDevice,
                                "error"
                            );
                        }

                        return;
                    }

                    // ============================================
                    // HTTP SUCCESS
                    // ============================================

                    LOG_DF(
                        "WEBAPI",
                        "[%s] HTTP request completed: "
                        "message=%s status=%d bytes=%u",
                        deviceName,
                        messageKey,
                        response.statusCode,
                        static_cast<unsigned>(
                            response.body.length()
                        )
                    );

                    processResponse(
                        response.body
                    );
                }
            );

        // ====================================================
        // REQUEST STARTED
        // ====================================================

        if (started)
        {
            const String safeUrl =
                sanitizeUrlForLog(
                    httpRequest.url
                );

            LOG_DF(
                "WEBAPI",
                "[%s] Started %s %s "
                "message=%s auth=%u",
                queued.device->name
                    ? queued.device->name
                    : "",
                httpRequest.method.c_str(),
                safeUrl.c_str(),
                queued.message->key
                    ? queued.message->key
                    : "",
                httpRequest.credentials.isConfigured()
                    ? 1U
                    : 0U
            );

            return;
        }

        // ====================================================
        // REQUEST START FAILURE
        // ====================================================

        LOG_WF(
            "WEBAPI",
            "[%s] Unable to start request '%s'",
            queued.device->name
                ? queued.device->name
                : "",
            queued.message->key
                ? queued.message->key
                : ""
        );

        pending =
            PendingRequest{};
    }


    // ========================================================================
    // FIND OUT MESSAGE
    // ========================================================================

    static const DeviceMessageProfile::OutMessage*
    findOut(
        const DeviceMessageProfile* profile,
        const char* key)
    {
        if (
            !profile ||
            !profile->outMessages ||
            !key
        )
        {
            return nullptr;
        }

        for (
            size_t i = 0;
            i < profile->outCount;
            ++i
        )
        {
            const auto& message =
                profile->outMessages[i];

            if (
                message.key &&
                strcmp(
                    message.key,
                    key
                ) == 0
            )
            {
                return &message;
            }
        }

        return nullptr;
    }


    // ========================================================================
    // REQUEST ALREADY QUEUED
    // ========================================================================

    bool isQueued(
        const WebApiDevice* device,
        const DeviceMessageProfile::OutMessage*
            message) const
    {
        if (
            pending.active &&
            pending.device == device &&
            pending.message == message
        )
        {
            return true;
        }

        for (const auto& request : requestQueue)
        {
            if (
                request.device == device &&
                request.message == message
            )
            {
                return true;
            }
        }

        return false;
    }

    // ========================================================================
    // URL LOG SANITIZER
    //
    // Oscura i parametri sensibili senza modificare l'URL realmente inviato.
    //
    // Esempio:
    //
    //   password=admin123
    //
    // diventa:
    //
    //   password=***
    // ========================================================================

    static String sanitizeUrlForLog(
        const String& url)
    {
        String sanitized =
            url;

        MaskQueryParameter(
            sanitized,
            "password"
        );

        MaskQueryParameter(
            sanitized,
            "passwd"
        );

        MaskQueryParameter(
            sanitized,
            "pwd"
        );

        MaskQueryParameter(
            sanitized,
            "token"
        );

        return sanitized;
    }


    // ========================================================================
    // MASK QUERY PARAMETER
    // ========================================================================

    static void MaskQueryParameter(
        String& url,
        const char* parameterName)
    {
        if (
            !parameterName ||
            parameterName[0] == '\0' ||
            url.length() == 0
        )
        {
            return;
        }

        String token =
            parameterName;

        token +=
            '=';

        int searchPosition =
            0;

        while (
            searchPosition <
            url.length()
        )
        {
            const int parameterPosition =
                url.indexOf(
                    token,
                    searchPosition
                );

            if (parameterPosition < 0)
            {
                return;
            }

            /*
            * Verifica che il parametro inizi dopo:
            *
            *   ?
            *   &
            *
            * oppure all'inizio della stringa.
            */
            if (
                parameterPosition > 0 &&
                url[
                    parameterPosition - 1
                ] != '?' &&
                url[
                    parameterPosition - 1
                ] != '&'
            )
            {
                searchPosition =
                    parameterPosition +
                    token.length();

                continue;
            }

            const int valueStart =
                parameterPosition +
                token.length();

            int valueEnd =
                url.indexOf(
                    '&',
                    valueStart
                );

            if (valueEnd < 0)
            {
                valueEnd =
                    url.length();
            }

            const String prefix =
                url.substring(
                    0,
                    valueStart
                );

            const String suffix =
                url.substring(
                    valueEnd
                );

            url =
                prefix +
                "***" +
                suffix;

            searchPosition =
                valueStart +
                3;
        }
    }

    // ========================================================================
    // BUILD URL
    // ========================================================================

    String buildUrl(
        const WebApiDevice* device,
        const DeviceMessageProfile::OutMessage*
            message) const
    {
        if (
            !device ||
            !device->host ||
            !message
        )
        {
            return "";
        }

        String path =
            message->format
                ? message->format
                : "";

        resolvePlaceholders(
            path,
            *device
        );

        String url = "http://";

        url += device->host;

        if (
            device->port != 0 &&
            device->port != 80
        )
        {
            url += ':';
            url += String(device->port);
        }

        if (!path.startsWith("/"))
            url += '/';

        url += path;

        return url;
    }


    // ========================================================================
    // PLACEHOLDER RESOLUTION
    //
    // Supporta:
    //   ${variable}
    //   $variable
    // ========================================================================

    void resolvePlaceholders(
        String& value,
        const WebApiDevice& device) const
    {
        int start = -1;

        while (
            (
                start =
                    value.indexOf("${")
            ) >= 0
        )
        {
            const int end =
                value.indexOf(
                    '}',
                    start + 2
                );

            if (end < 0)
                break;

            const String variable =
                value.substring(
                    start + 2,
                    end
                );

            const String replacement =
                resolvePlaceholder(
                    device,
                    variable
                );

            value =
                value.substring(0, start) +
                replacement +
                value.substring(end + 1);
        }

        int position = 0;

        while (
            (
                position =
                    value.indexOf(
                        '$',
                        position
                    )
            ) >= 0
        )
        {
            /*
             * I placeholder ${...} sono già stati elaborati.
             */
            if (
                position + 1 <
                value.length() &&
                value[position + 1] == '{'
            )
            {
                ++position;
                continue;
            }

            int end = position + 1;

            while (
                end < value.length()
            )
            {
                const char c = value[end];

                if (
                    !isalnum(
                        static_cast<unsigned char>(c)
                    ) &&
                    c != '_'
                )
                {
                    break;
                }

                ++end;
            }

            if (end == position + 1)
            {
                ++position;
                continue;
            }

            const String variable =
                value.substring(
                    position + 1,
                    end
                );

            const String replacement =
                resolvePlaceholder(
                    device,
                    variable
                );

            value =
                value.substring(0, position) +
                replacement +
                value.substring(end);

            position += replacement.length();
        }
    }


    String resolvePlaceholder(
        const WebApiDevice& device,
        const String& variable) const
    {
        if (!ctx.resolver)
            return "";

        return ctx.resolver(
            device,
            variable.c_str()
        );
    }

    // ========================================================================
    // RESPONSE PARSER
    //
    // Formato:
    //
    //   key=value
    //   key2=value2
    //
    // Una coppia per linea.
    // ========================================================================

    static ParsedFields parseResponse(
        const String& response)
    {
        ParsedFields result;

        int start = 0;

        while (start < response.length())
        {
            int end =
                response.indexOf(
                    '\n',
                    start
                );

            if (end < 0)
                end = response.length();

            String line =
                response.substring(
                    start,
                    end
                );

            line.trim();

            start = end + 1;

            if (line.length() == 0)
                continue;

            const int separator =
                line.indexOf('=');

            if (separator <= 0)
                continue;

            ParsedField field;

            field.key =
                line.substring(
                    0,
                    separator
                );

            field.value =
                line.substring(
                    separator + 1
                );

            field.key.trim();
            field.value.trim();

            result.fields.push_back(
                field
            );
        }

        return result;
    }

    // ========================================================================
    // HEXADECIMAL DIGIT
    // ========================================================================

    static int HexadecimalValue(
        char value)
    {
        if (
            value >= '0' &&
            value <= '9'
        )
        {
            return
                value - '0';
        }

        if (
            value >= 'A' &&
            value <= 'F'
        )
        {
            return
                value - 'A' +
                10;
        }

        if (
            value >= 'a' &&
            value <= 'f'
        )
        {
            return
                value - 'a' +
                10;
        }

        return
            -1;
    }


    // ========================================================================
    // PARSE HTTP CHUNK SIZE
    //
    // Supporta:
    //
    //   1d9
    //   1D9
    //   1d9;extension=value
    // ========================================================================

    static bool ParseChunkSize(
        const String& sizeLine,
        size_t& chunkSize)
    {
        chunkSize =
            0;

        size_t processedDigits =
            0;

        for (
            size_t index = 0;
            index < sizeLine.length();
            ++index
        )
        {
            const char current =
                sizeLine[index];

            /*
            * Le chunk extension iniziano con ';'.
            */
            if (current == ';')
            {
                break;
            }

            /*
            * Tollera spazi dopo la dimensione.
            */
            if (
                current == ' ' ||
                current == '\t'
            )
            {
                break;
            }

            const int digit =
                HexadecimalValue(
                    current
                );

            if (digit < 0)
            {
                return false;
            }

            /*
            * Controllo overflow prima dello shift.
            */
            const size_t maximumValue =
                static_cast<size_t>(
                    -1
                );

            if (
                chunkSize >
                (
                    maximumValue -
                    static_cast<size_t>(
                        digit
                    )
                ) /
                16U
            )
            {
                return false;
            }

            chunkSize =
                chunkSize *
                16U +
                static_cast<size_t>(
                    digit
                );

            ++processedDigits;
        }

        return
            processedDigits > 0;
    }


    // ========================================================================
    // HTTP CHUNKED BODY DETECTION
    //
    // Riconosce un body con una prima riga esadecimale seguita da CRLF.
    //
    // Non considera automaticamente chunked qualsiasi contenuto che non inizi
    // con '[' oppure '{': la prima riga deve essere una dimensione esadecimale
    // valida.
    // ========================================================================

    static bool IsChunkedBody(
        const String& response)
    {
        if (response.length() == 0)
        {
            return false;
        }

        size_t firstContentPosition =
            0;

        while (
            firstContentPosition <
            response.length()
        )
        {
            const char current =
                response[
                    firstContentPosition
                ];

            if (
                current != ' ' &&
                current != '\t' &&
                current != '\r' &&
                current != '\n'
            )
            {
                break;
            }

            ++firstContentPosition;
        }

        if (
            firstContentPosition >=
            response.length()
        )
        {
            return false;
        }

        const char firstContentCharacter =
            response[
                firstContentPosition
            ];

        if (
            firstContentCharacter == '[' ||
            firstContentCharacter == '{'
        )
        {
            return false;
        }

        const int sizeLineEnd =
            response.indexOf(
                "\r\n",
                static_cast<unsigned int>(
                    firstContentPosition
                )
            );

        if (sizeLineEnd < 0)
        {
            return false;
        }

        const String sizeLine =
            response.substring(
                firstContentPosition,
                static_cast<size_t>(
                    sizeLineEnd
                )
            );

        size_t chunkSize =
            0;

        return
            ParseChunkSize(
                sizeLine,
                chunkSize
            );
    }


    // ========================================================================
    // HTTP CHUNKED BODY DECODER
    //
    // Formato:
    //
    //   <size-esadecimale>\r\n
    //   <dati>\r\n
    //   ...
    //   0\r\n
    //   <eventuali trailer>\r\n
    //
    // Restituisce false quando:
    //
    //   - manca la riga della dimensione;
    //   - la dimensione non è esadecimale;
    //   - i dati sono troncati;
    //   - manca il CRLF dopo un chunk;
    //   - il body supera il limite configurato.
    // ========================================================================

    static bool DecodeChunkedBody(
        const String& encodedBody,
        String& decodedBody)
    {
        decodedBody =
            "";

        if (encodedBody.length() == 0)
        {
            return false;
        }

        decodedBody.reserve(
            encodedBody.length()
        );

        size_t position =
            0;

        /*
        * Tollera eventuali CR/LF o spazi prima del primo chunk.
        */
        while (
            position <
            encodedBody.length() &&
            (
                encodedBody[position] == '\r' ||
                encodedBody[position] == '\n' ||
                encodedBody[position] == ' ' ||
                encodedBody[position] == '\t'
            )
        )
        {
            ++position;
        }

        while (
            position <
            encodedBody.length()
        )
        {
            // ====================================================
            // CHUNK SIZE LINE
            // ====================================================

            const int sizeLineEnd =
                encodedBody.indexOf(
                    "\r\n",
                    static_cast<unsigned int>(
                        position
                    )
                );

            if (sizeLineEnd < 0)
            {
                return false;
            }

            String sizeLine =
                encodedBody.substring(
                    position,
                    static_cast<size_t>(
                        sizeLineEnd
                    )
                );

            sizeLine.trim();

            size_t chunkSize =
                0;

            if (
                !ParseChunkSize(
                    sizeLine,
                    chunkSize
                )
            )
            {
                return false;
            }

            position =
                static_cast<size_t>(
                    sizeLineEnd
                ) +
                2U;

            // ====================================================
            // FINAL CHUNK
            // ====================================================

            if (chunkSize == 0)
            {
                /*
                * L'eventuale trailer HTTP non fa parte del body.
                */
                return true;
            }

            // ====================================================
            // RESPONSE SIZE LIMIT
            // ====================================================

            if (
                decodedBody.length() >
                    MAX_JSON_RESPONSE_SIZE ||
                chunkSize >
                    MAX_JSON_RESPONSE_SIZE -
                    decodedBody.length()
            )
            {
                return false;
            }

            // ====================================================
            // CHUNK BOUNDS
            // ====================================================

            const size_t chunkEnd =
                position +
                chunkSize;

            if (
                chunkEnd >
                encodedBody.length()
            )
            {
                return false;
            }

            // ====================================================
            // APPEND DATA
            // ====================================================

            decodedBody +=
                encodedBody.substring(
                    position,
                    chunkEnd
                );

            position =
                chunkEnd;

            // ====================================================
            // CHUNK TERMINATOR
            // ====================================================

            if (
                position + 2U >
                encodedBody.length() ||
                encodedBody[position] != '\r' ||
                encodedBody[position + 1U] != '\n'
            )
            {
                return false;
            }

            position +=
                2U;
        }

        /*
        * La risposta chunked deve terminare con il chunk zero.
        */
        return false;
    }

    // ========================================================================
    // JSON PARSER
    //
    // Supporta:
    //
    //   - JSON diretto;
    //   - JSON con spazi iniziali;
    //   - body HTTP chunked;
    //   - eventuale BOM UTF-8.
    //
    // La decodifica chunked è eseguita qui perché HttpResponse::body contiene
    // attualmente ancora la codifica dei chunk.
    //
    // In una successiva revisione la decodifica potrà essere centralizzata
    // direttamente in HttpClientTransport.
    // ========================================================================

    static bool parseJson(
        const String& response,
        DynamicJsonDocument& doc)
    {
        // ====================================================
        // RESET DOCUMENT
        // ====================================================

        doc.clear();

        // ====================================================
        // BASIC VALIDATION
        // ====================================================

        if (response.length() == 0)
        {
            LOG_WF(
                "WEBAPI",
                "JSON response is empty"
            );

            return false;
        }

        if (
            response.length() >
            MAX_JSON_RESPONSE_SIZE
        )
        {
            LOG_WF(
                "WEBAPI",
                "JSON response too large: "
                "bytes=%u limit=%u",
                static_cast<unsigned>(
                    response.length()
                ),
                static_cast<unsigned>(
                    MAX_JSON_RESPONSE_SIZE
                )
            );

            return false;
        }

        // ====================================================
        // BODY NORMALIZATION
        // ====================================================

        String jsonPayload;

        bool chunked =
            false;

        if (
            IsChunkedBody(
                response
            )
        )
        {
            chunked =
                true;

            if (
                !DecodeChunkedBody(
                    response,
                    jsonPayload
                )
            )
            {
                LOG_WF(
                    "WEBAPI",
                    "Unable to decode chunked JSON: "
                    "encodedBytes=%u",
                    static_cast<unsigned>(
                        response.length()
                    )
                );

                return false;
            }
        }
        else
        {
            jsonPayload =
                response;
        }

        // ====================================================
        // TRIM WHITESPACE
        // ====================================================

        jsonPayload.trim();

        // ====================================================
        // UTF-8 BOM
        // ====================================================

        if (
            jsonPayload.length() >= 3 &&
            static_cast<uint8_t>(
                jsonPayload[0]
            ) == 0xEF &&
            static_cast<uint8_t>(
                jsonPayload[1]
            ) == 0xBB &&
            static_cast<uint8_t>(
                jsonPayload[2]
            ) == 0xBF
        )
        {
            jsonPayload =
                jsonPayload.substring(
                    3
                );

            jsonPayload.trim();
        }

        // ====================================================
        // JSON PREFIX VALIDATION
        // ====================================================

        if (jsonPayload.length() == 0)
        {
            LOG_WF(
                "WEBAPI",
                "Decoded JSON payload is empty: "
                "chunked=%u encodedBytes=%u",
                chunked
                    ? 1U
                    : 0U,
                static_cast<unsigned>(
                    response.length()
                )
            );

            return false;
        }

        const char firstCharacter =
            jsonPayload[0];

        if (
            firstCharacter != '[' &&
            firstCharacter != '{'
        )
        {
            LOG_WF(
                "WEBAPI",
                "Invalid JSON prefix: "
                "first=0x%02X chunked=%u bytes=%u",
                static_cast<unsigned>(
                    static_cast<uint8_t>(
                        firstCharacter
                    )
                ),
                chunked
                    ? 1U
                    : 0U,
                static_cast<unsigned>(
                    jsonPayload.length()
                )
            );

            return false;
        }

        // ====================================================
        // JSON DESERIALIZATION
        // ====================================================

        const DeserializationError error =
            deserializeJson(
                doc,
                jsonPayload
            );

        if (error)
        {
            LOG_WF(
                "WEBAPI",
                "JSON parse error: "
                "%s chunked=%u "
                "encodedBytes=%u decodedBytes=%u",
                error.c_str(),
                chunked
                    ? 1U
                    : 0U,
                static_cast<unsigned>(
                    response.length()
                ),
                static_cast<unsigned>(
                    jsonPayload.length()
                )
            );

            return false;
        }

        // ====================================================
        // SUCCESS
        // ====================================================

        LOG_DF(
            "WEBAPI",
            "JSON parsed: "
            "chunked=%u encodedBytes=%u decodedBytes=%u",
            chunked
                ? 1U
                : 0U,
            static_cast<unsigned>(
                response.length()
            ),
            static_cast<unsigned>(
                jsonPayload.length()
            )
        );

        return true;
    }

    // ========================================================================
    // CORRELATION
    // ========================================================================

    static bool checkCorrelation(
        const DeviceMessageProfile* profile,
        const DeviceMessageProfile::OutMessage*
            message,
        const ParsedFields& fields)
    {
        if (
            !profile ||
            !message
        )
        {
            return false;
        }

        /*
         * Nessuna regola associata equivale a risposta accettata.
         */
        bool matchingRuleFound = false;

        for (
            size_t i = 0;
            i < profile->ruleCount;
            ++i
        )
        {
            const auto& rule =
                profile->rules[i];

            if (
                !rule.outKey ||
                strcmp(
                    rule.outKey,
                    message->key
                ) != 0
            )
            {
                continue;
            }

            matchingRuleFound = true;

            const bool present =
                fields.has(rule.expectedIn);

            const String value =
                fields.get(rule.expectedIn);

            if (
                !rule.condition ||
                strcmp(
                    rule.condition,
                    "optional"
                ) == 0
            )
            {
                continue;
            }

            if (
                strcmp(
                    rule.condition,
                    "always"
                ) == 0
            )
            {
                if (!present)
                    return false;

                continue;
            }

            if (
                strcmp(
                    rule.condition,
                    "value == 1"
                ) == 0
            )
            {
                if (
                    !present ||
                    value.toInt() != 1
                )
                {
                    return false;
                }
            }
        }

        /*
         * Se il messaggio non ha regole specifiche,
         * la risposta viene considerata valida.
         */
        return
            matchingRuleFound ||
            profile->ruleCount == 0 ||
            !hasRulesForMessage(
                profile,
                message
            );
    }


    static bool hasRulesForMessage(
        const DeviceMessageProfile* profile,
        const DeviceMessageProfile::OutMessage*
            message)
    {
        if (
            !profile ||
            !message
        )
        {
            return false;
        }

        for (
            size_t i = 0;
            i < profile->ruleCount;
            ++i
        )
        {
            const auto& rule =
                profile->rules[i];

            if (
                rule.outKey &&
                strcmp(
                    rule.outKey,
                    message->key
                ) == 0
            )
            {
                return true;
            }
        }

        return false;
    }


    // ========================================================================
    // STATE MACHINE
    // ========================================================================

    void applyStateEvent(
        const WebApiDevice* device,
        const char* event)
    {
        if (
            !device ||
            !device->profile ||
            !event
        )
        {
            return;
        }

        DeviceRuntime* runtime =
            findRuntimeMutable(device);

        if (!runtime)
            return;

        const DeviceMessageProfile* profile =
            device->profile;

        for (
            size_t i = 0;
            i < profile->stateRuleCount;
            ++i
        )
        {
            const auto& rule =
                profile->stateRules[i];

            if (
                !rule.state ||
                !rule.onEvent ||
                !rule.nextState
            )
            {
                continue;
            }

            if (
                runtime->state != rule.state ||
                strcmp(
                    event,
                    rule.onEvent
                ) != 0
            )
            {
                continue;
            }

            const String previousState =
                runtime->state;

            runtime->state =
                rule.nextState;

            LOG_DF(
                "WEBAPI",
                "[%s] State %s -> %s event=%s",
                device->name
                    ? device->name
                    : "",
                previousState.c_str(),
                runtime->state.c_str(),
                event
            );

            if (
                rule.action &&
                rule.action[0] != '\0'
            )
            {
                Send(
                    device,
                    rule.action
                );
            }

            return;
        }
    }


    // ========================================================================
    // RUNTIME LOOKUP
    // ========================================================================

    DeviceRuntime*
    findRuntimeMutable(
        const WebApiDevice* device)
    {
        for (auto& runtime : runtimes)
        {
            if (runtime.device == device)
                return &runtime;
        }

        return nullptr;
    }


    const DeviceRuntime*
    findRuntime(
        const char* deviceName) const
    {
        if (!deviceName)
            return nullptr;

        for (const auto& runtime : runtimes)
        {
            if (
                runtime.device &&
                runtime.device->name &&
                strcmp(
                    runtime.device->name,
                    deviceName
                ) == 0
            )
            {
                return &runtime;
            }
        }

        return nullptr;
    }


    // ========================================================================
    // DIAGNOSTIC LOG
    // ========================================================================

    static void dumpFields(
        const WebApiDevice& device,
        const ParsedFields& fields)
    {
        LOG_DF(
            "WEBAPI",
            "[%s] Received %u fields",
            device.name
                ? device.name
                : "",
            static_cast<unsigned>(
                fields.size()
            )
        );

        for (const auto& field : fields.fields)
        {
            LOG_DF(
                "WEBAPI",
                "[%s] %s = %s",
                device.name
                    ? device.name
                    : "",
                field.key.c_str(),
                field.value.c_str()
            );
        }
    }
};

// ============================================================================
// WEB API SERVER ENDPOINT
//
// Configurazione applicativa di un endpoint HTTP in ingresso.
// ============================================================================

struct WebApiServerEndpoint
{
    using RequestHandler =
        std::function<void(
            const HttpServerRequest& request,
            HttpServerResponse& response)>;

    const char* name = nullptr;

    const char* path = nullptr;

    const char* method = nullptr;

    bool enabled = true;

    RequestHandler handler;
};

// ============================================================================
// WEB API SERVER ENDPOINT ADAPTER
// ============================================================================

class WebApiEndpointHandler :
    public IHttpServerHandler
{
private:

    const WebApiServerEndpoint* endpoint =
        nullptr;


public:

    WebApiEndpointHandler() =
        default;


    explicit WebApiEndpointHandler(
        const WebApiServerEndpoint* config)
        : endpoint(config)
    {
    }


    void configure(
        const WebApiServerEndpoint* config)
    {
        endpoint =
            config;
    }


    bool matches(
        const HttpServerRequest& request) const override
    {
        if (
            !endpoint ||
            !endpoint->enabled ||
            !endpoint->path
        )
        {
            return false;
        }

        if (
            request.path !=
            endpoint->path
        )
        {
            return false;
        }

        if (
            endpoint->method &&
            endpoint->method[0] != '\0'
        )
        {
            return
                request.method.equalsIgnoreCase(
                    endpoint->method
                );
        }

        return true;
    }


    void handle(
        const HttpServerRequest& request,
        HttpServerResponse& response) override
    {
        if (
            !endpoint ||
            !endpoint->handler
        )
        {
            response.statusCode =
                500;

            response.contentType =
                "text/plain";

            response.body =
                "Endpoint handler not configured";

            response.closeConnection =
                true;

            return;
        }

        LOG_DF(
            "WEBAPI-SERVER",
            "[%s] %s %s",
            endpoint->name
                ? endpoint->name
                : "",
            request.method.c_str(),
            request.path.c_str()
        );

        endpoint->handler(
            request,
            response
        );
    }
};

// ============================================================================
// WEB API SERVER ENGINE
//
// Collega gli endpoint applicativi al transport HTTP server.
// ============================================================================

class WebApiServerEngine
{
public:

    struct Context
    {
        IHttpServerTransport* transport =
            nullptr;

        const WebApiServerEndpoint* endpoints =
            nullptr;

        size_t endpointCount =
            0;
    };


private:

    Context ctx;

    std::vector<WebApiEndpointHandler>
        handlers;

    bool initialized =
        false;


public:

    bool Init(
        const Context& context)
    {
        ctx =
            context;

        handlers.clear();

        initialized =
            false;

        if (
            !ctx.transport ||
            !ctx.endpoints ||
            ctx.endpointCount == 0
        )
        {
            LOG_E(
                "WEBAPI-SERVER",
                "Invalid server context"
            );

            return false;
        }

        handlers.reserve(
            ctx.endpointCount
        );

        for (
            size_t i = 0;
            i < ctx.endpointCount;
            ++i
        )
        {
            const WebApiServerEndpoint& endpoint =
                ctx.endpoints[i];

            if (
                !endpoint.enabled ||
                !endpoint.path ||
                !endpoint.handler
            )
            {
                continue;
            }

            handlers.emplace_back(
                &endpoint
            );
        }

        for (
            auto& handler : handlers
        )
        {
            if (
                !ctx.transport->addHandler(
                    &handler
                )
            )
            {
                LOG_E(
                    "WEBAPI-SERVER",
                    "Unable to register endpoint"
                );

                handlers.clear();

                return false;
            }
        }

        if (!ctx.transport->begin())
        {
            LOG_E(
                "WEBAPI-SERVER",
                "Unable to start HTTP server"
            );

            handlers.clear();

            return false;
        }

        initialized =
            true;

        LOG_IF(
            "WEBAPI-SERVER",
            "Initialized on port %u with %u endpoints",
            ctx.transport->getPort(),
            static_cast<unsigned>(
                handlers.size()
            )
        );

        return true;
    }


    void Loop(
        unsigned long now)
    {
        if (
            !initialized ||
            !ctx.transport
        )
        {
            return;
        }

        ctx.transport->loop(
            now
        );
    }


    bool isRunning() const
    {
        return
            initialized &&
            ctx.transport &&
            ctx.transport->isRunning();
    }


    size_t endpointCount() const
    {
        return
            handlers.size();
    }
};

#endif
