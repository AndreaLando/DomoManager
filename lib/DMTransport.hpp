#ifndef DMTransports_HPP
#define DMTransports_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑08‑24
   Note:
                    • Nessuna

   ============================================================================ */

#include <Arduino.h>
#include <functional>
#include <String>

#include "DMNetwork.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


// ============================================================
//  TRANSPORT INTERFACE
// ============================================================
class ICommandTransport {
public:
    using ResponseCallback = std::function<void(const String&)>;

    virtual ~ICommandTransport() {}
    virtual bool begin() = 0;
    virtual bool send(const String& payload) = 0;
    virtual bool request(const String& payload, ResponseCallback cb, unsigned long timeoutMs = 3000) = 0;
    virtual void loop(unsigned long now) = 0;
    virtual bool isOnline() const = 0;
    virtual bool receive(String& out) = 0;
    virtual size_t maxPacketSize() const { return 512; }

};

class TransportBase : public ICommandTransport {
protected:
    bool online = false;
    unsigned long lastRx = 0;
    unsigned long timeoutMs = 5000;

public:
    virtual ~TransportBase() {}

    void setTimeout(unsigned long ms) { timeoutMs = ms; }

    bool isOnline() const override {
        return online;
    }

    // ------------------------------------------------------------
    // RECEIVE BASE
    // ------------------------------------------------------------
    bool receive(String& out) override {
        if (!rawReceive(out))   // delega al transport concreto
            return false;

        online = true;
        lastRx = millis();
        return true;
    }

    // ------------------------------------------------------------
    // LOOP BASE
    // ------------------------------------------------------------
    void loop(unsigned long now) override {
        rawLoop(now);  // delega al transport concreto

        if (online && (now - lastRx > timeoutMs)) {
            online = false;
        }
    }

protected:
    // Questi due metodi li implementano gli adapter concreti
    virtual bool rawReceive(String& out) = 0;
    virtual void rawLoop(unsigned long now) = 0;
};

class IBridgePacer {
public:
    virtual ~IBridgePacer() {}

    virtual bool acquire(unsigned long now) = 0;
    virtual void release(unsigned long now) = 0;
};

// ============================================================================
// HTTP OBJECTS
// ============================================================================
struct HttpCredentials
{
    const char* username =
        nullptr;

    const char* password =
        nullptr;


    inline bool isConfigured() const
    {
        return
            username &&
            username[0] != '\0';
    }
};

struct HttpRequest
{
    String method;

    String url;

    String body;

    uint32_t timeoutMs =
        0;


    // ========================================================================
    // OPTIONAL HTTP AUTHENTICATION
    // ========================================================================

    HttpCredentials credentials;
};

struct HttpResponse
{
    int statusCode = 0;

    String headers;

    String body;

    bool success = false;
};

// ============================================================================
// HTTP SERVER REQUEST
// ============================================================================

struct HttpServerRequest
{
    String method;
    String path;
    String query;
    String body;

    String contentType;
    String authorization;

    size_t contentLength = 0;

    String getQueryValue(const char* key) const
    {
        if (!key || !query.length())
            return "";

        const String target =
            String(key) + "=";

        int start = 0;

        while (start < query.length())
        {
            int end =
                query.indexOf('&', start);

            if (end < 0)
                end = query.length();

            const String item =
                query.substring(start, end);

            if (item.startsWith(target))
                return item.substring(target.length());

            start = end + 1;
        }

        return "";
    }
};


// ============================================================================
// HTTP SERVER RESPONSE
// ============================================================================

struct HttpServerResponse
{
    int statusCode = 200;

    String contentType =
        "text/plain";

    String body = "OK";

    bool closeConnection = true;
};

// ============================================================================
// HTTP SERVER HANDLER
// ============================================================================

class IHttpServerHandler
{
public:

    virtual ~IHttpServerHandler() = default;

    virtual bool matches(
        const HttpServerRequest& request) const = 0;

    virtual void handle(
        const HttpServerRequest& request,
        HttpServerResponse& response) = 0;
};

// ============================================================================
// HTTP SERVER TRANSPORT INTERFACE
// ============================================================================

class IHttpServerTransport
{
public:

    virtual ~IHttpServerTransport() = default;

    virtual bool begin() = 0;

    virtual void loop(
        unsigned long now) = 0;

    virtual bool addHandler(
        IHttpServerHandler* handler) = 0;

    virtual bool isRunning() const = 0;

    virtual uint16_t getPort() const = 0;
};

// ============================================================================
// HTTP TRANSPORT
// ============================================================================

class IHttpTransport
{
public:

    using HttpCallback =
        std::function<void(
            const HttpResponse&
        )>;

    virtual ~IHttpTransport()
    {
    }

    virtual bool begin() = 0;

    virtual bool request(
        const HttpRequest& request,
        HttpCallback callback
    ) = 0;

    virtual bool isBusy() const = 0;

    virtual void cancel() = 0;

    virtual void loop(
        unsigned long now
    ) = 0;
};


// ============================================================================
// SIMPLE HTTP TRANSPORT
//
// Client HTTP/1.1 non bloccante.
//
// Supporta:
//   • HTTP
//   • GET
//   • POST
//   • host e porta estratti dalla URL
//
// Non supporta HTTPS/TLS.
// ============================================================================

class HttpClientTransport :
    public IHttpTransport
{
private:

    enum class State : uint8_t
    {
        Idle,
        Connecting,
        Sending,
        WaitingResponse,
        Reading,
        Completed,
        Error
    };

    DMEthernetClient client;

    HttpCallback callback;

    State state = State::Idle;

    bool online = false;

    String method;
    String url;
    String body;

    HttpCredentials credentials;

    String host;
    String path;

    String response;

    uint16_t port = 80;

    uint32_t timeoutMs = 2000;

    unsigned long stateStartedAt = 0;

public:

    explicit HttpClientTransport(
        uint32_t timeout = 2000)
        : timeoutMs(timeout)
    {
    }

    bool begin() override
    {
        cancel();
        return true;
    }

    bool request(
        const HttpRequest& req,
        HttpCallback cb) override
    {
        if (isBusy())
        {
            LOG_WF(
                "WEBAPI-HTTP",
                "Unable to start request: transport busy"
            );

            return false;
        }

        callback =
            cb;

        timeoutMs =
            req.timeoutMs > 0
                ? req.timeoutMs
                : 2000UL;

        return startRequest(
            req.method,
            req.url,
            req.body,
            req.credentials
        );
    }

    void loop(
        unsigned long now) override
    {
        String bodyResponse;

        /*
        * completeRequest() esegue reset(), che azzera
        * callback. Conserviamo quindi una copia valida
        * prima di avanzare la macchina a stati.
        */
        const HttpCallback completedCallback =
            callback;

        if (
            !loopRequest(
                now,
                bodyResponse
            )
        )
        {
            return;
        }

        if (!completedCallback)
        {
            LOG_WF(
                "WEBAPI-HTTP",
                "HTTP response completed without callback"
            );

            return;
        }

        HttpResponse httpResponse;

        httpResponse.success =
            true;

        /*
        * Per ora il transport non estrae ancora lo status
        * dalla status line HTTP.
        */
        httpResponse.statusCode =
            200;

        httpResponse.body =
            bodyResponse;

        completedCallback(
            httpResponse
        );
    }

    bool isBusy() const override
    {
        return state != State::Idle;
    }

    bool isOnline() const
    {
        return online;
    }

    void cancel() override
    {
        client.stop();

        state =
            State::Idle;

        online =
            false;

        method =
            "";

        url =
            "";

        body =
            "";

        credentials =
            HttpCredentials{};

        host =
            "";

        path =
            "";

        response =
            "";

        port =
            80;

        stateStartedAt =
            0;

        callback =
            nullptr;
    }

private:

    bool startRequest(
        const String& requestMethod,
        const String& requestUrl,
        const String& requestBody,
        const HttpCredentials& requestCredentials)
    {
        if (isBusy())
        {
            return false;
        }

        method =
            requestMethod;

        method.toUpperCase();

        url =
            requestUrl;

        body =
            requestBody;

        credentials =
            requestCredentials;

        response =
            "";

        response.reserve(
            512
        );

        if (!parseUrl(url))
        {
            LOG_EF(
                "WEBAPI-HTTP",
                "Invalid URL: %s",
                url.c_str()
            );

            reset();

            return false;
        }

        state =
            State::Connecting;

        stateStartedAt =
            millis();

        return true;
    }

    bool isResponseComplete() const
    {
        // ====================================================
        // HEADER TERMINATOR
        // ====================================================

        const int headerEnd =
            response.indexOf(
                "\r\n\r\n"
            );

        if (headerEnd < 0)
        {
            return false;
        }

        // ====================================================
        // CONTENT-LENGTH
        // ====================================================

        int position =
            0;

        while (
            position <
            headerEnd
        )
        {
            const int lineEnd =
                response.indexOf(
                    "\r\n",
                    position
                );

            if (
                lineEnd < 0 ||
                lineEnd > headerEnd
            )
            {
                break;
            }

            String line =
                response.substring(
                    position,
                    lineEnd
                );

            position =
                lineEnd + 2;

            const int separator =
                line.indexOf(
                    ':'
                );

            if (separator <= 0)
            {
                continue;
            }

            String key =
                line.substring(
                    0,
                    separator
                );

            String value =
                line.substring(
                    separator + 1
                );

            key.trim();
            key.toLowerCase();

            value.trim();

            if (
                key !=
                "content-length"
            )
            {
                continue;
            }

            const long parsedLength =
                value.toInt();

            if (parsedLength < 0)
            {
                return false;
            }

            const size_t contentLength =
                static_cast<size_t>(
                    parsedLength
                );

            const size_t bodyStart =
                static_cast<size_t>(
                    headerEnd + 4
                );

            if (
                response.length() <
                bodyStart
            )
            {
                return false;
            }

            const size_t receivedBodyLength =
                response.length() -
                bodyStart;

            return
                receivedBodyLength >=
                contentLength;
        }

        /*
        * In assenza di Content-Length non possiamo dichiarare
        * completa la risposta finché la connessione resta aperta.
        */
        return false;
    }

    bool completeRequest(
        String& outResponse)
    {
        online =
            true;

        outResponse =
            extractBody(
                response
            );

        LOG_DF(
            "WEBAPI-HTTP",
            "HTTP response completed: "
            "rawBytes=%u bodyBytes=%u",
            static_cast<unsigned>(
                response.length()
            ),
            static_cast<unsigned>(
                outResponse.length()
            )
        );

        /*
        * loop() ha già salvato una copia della callback.
        * reset() può quindi azzerare lo stato interno in
        * sicurezza.
        */
        reset();

        return true;
    }

    bool loopRequest(
        unsigned long now,
        String& outResponse)
    {
        switch (state)
        {
            // ====================================================
            // IDLE
            // ====================================================

            case State::Idle:
            {
                return false;
            }

            // ====================================================
            // CONNECTING
            // ====================================================

            case State::Connecting:
            {
                const unsigned long connectStartedAt =
                    micros();

                const bool connected =
                    client.connect(
                        host.c_str(),
                        port
                    );

                const unsigned long connectDurationUs =
                    micros() -
                    connectStartedAt;

                LOG_DF(
                    "WEBAPI-HTTP",
                    "Connect attempt: "
                    "host=%s port=%u result=%u "
                    "duration=%lu us",
                    host.c_str(),
                    static_cast<unsigned>(
                        port
                    ),
                    connected
                        ? 1U
                        : 0U,
                    connectDurationUs
                );

                if (connected)
                {
                    state =
                        State::Sending;

                    stateStartedAt =
                        now;
                }
                else if (
                    expired(
                        now
                    )
                )
                {
                    fail(
                        "Connection timeout"
                    );
                }

                break;
            }

            // ====================================================
            // SENDING
            // ====================================================

            case State::Sending:
            {
                LOG_DF(
                    "WEBAPI-HTTP",
                    "Sending request: "
                    "method=%s host=%s port=%u "
                    "path=%s auth=%u",
                    method.c_str(),
                    host.c_str(),
                    static_cast<unsigned>(
                        port
                    ),
                    path.c_str(),
                    credentials.isConfigured()
                        ? 1U
                        : 0U
                );

                sendHeaders();

                state =
                    State::WaitingResponse;

                stateStartedAt =
                    now;

                break;
            }

            // ====================================================
            // WAITING RESPONSE
            // ====================================================

            case State::WaitingResponse:
            {
                const int availableBytes =
                    client.available();

                if (availableBytes > 0)
                {
                    LOG_DF(
                        "WEBAPI-HTTP",
                        "Response data available: bytes=%d",
                        availableBytes
                    );

                    state =
                        State::Reading;

                    stateStartedAt =
                        now;

                    readAvailable();

                    /*
                    * Completa immediatamente quando header e body
                    * indicati da Content-Length sono arrivati.
                    */
                    if (
                        isResponseComplete()
                    )
                    {
                        return completeRequest(
                            outResponse
                        );
                    }

                    if (
                        !client.connected() &&
                        response.length() > 0
                    )
                    {
                        return completeRequest(
                            outResponse
                        );
                    }
                }
                else if (
                    !client.connected()
                )
                {
                    if (response.length() > 0)
                    {
                        return completeRequest(
                            outResponse
                        );
                    }

                    fail(
                        "Connection closed without response"
                    );
                }
                else if (
                    expired(
                        now
                    )
                )
                {
                    fail(
                        "Response timeout"
                    );
                }

                break;
            }

            // ====================================================
            // READING
            // ====================================================

            case State::Reading:
            {
                const size_t previousLength =
                    response.length();

                readAvailable();

                if (
                    response.length() >
                    previousLength
                )
                {
                    stateStartedAt =
                        now;
                }

                /*
                * Non attendiamo la chiusura TCP quando il
                * Content-Length dimostra che la risposta è
                * completa.
                */
                if (
                    isResponseComplete()
                )
                {
                    return completeRequest(
                        outResponse
                    );
                }

                if (
                    !client.connected()
                )
                {
                    if (response.length() > 0)
                    {
                        return completeRequest(
                            outResponse
                        );
                    }

                    fail(
                        "Connection closed without response"
                    );

                    break;
                }

                if (
                    expired(
                        now
                    )
                )
                {
                    if (response.length() > 0)
                    {
                        LOG_WF(
                            "WEBAPI-HTTP",
                            "Completing response after read timeout: "
                            "bytes=%u",
                            static_cast<unsigned>(
                                response.length()
                            )
                        );

                        return completeRequest(
                            outResponse
                        );
                    }

                    fail(
                        "Read timeout"
                    );
                }

                break;
            }

            // ====================================================
            // COMPLETED
            // ====================================================

            case State::Completed:
            {
                return completeRequest(
                    outResponse
                );
            }

            // ====================================================
            // ERROR
            // ====================================================

            case State::Error:
            {
                const HttpCallback errorCallback =
                    callback;

                reset();

                if (errorCallback)
                {
                    HttpResponse errorResponse;

                    errorResponse.success =
                        false;

                    errorResponse.statusCode =
                        -1;

                    errorCallback(
                        errorResponse
                    );
                }

                break;
            }
        }

        return false;
    }

    bool parseUrl(const String& value)
    {
        String source = value;

        source.trim();

        if (source.length() == 0)
            return false;

        if (source.startsWith("https://"))
            return false;

        int hostStart = 0;

        if (source.startsWith("http://"))
            hostStart = 7;

        const int pathStart =
            source.indexOf('/', hostStart);

        String authority;

        if (pathStart < 0)
        {
            authority =
                source.substring(hostStart);

            path = "/";
        }
        else
        {
            authority =
                source.substring(
                    hostStart,
                    pathStart);

            path =
                source.substring(pathStart);
        }

        const int colon =
            authority.lastIndexOf(':');

        if (colon > 0)
        {
            host =
                authority.substring(
                    0,
                    colon);

            port =
                authority
                    .substring(colon + 1)
                    .toInt();

            if (port == 0)
                port = 80;
        }
        else
        {
            host = authority;
            port = 80;
        }

        host.trim();

        return host.length() > 0;
    }

    static String EncodeBase64(
        const String& input)
    {
        static const char alphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        const size_t inputLength =
            input.length();

        String output;

        output.reserve(
            (
                (
                    inputLength + 2
                ) /
                3
            ) *
            4
        );

        size_t index =
            0;

        while (
            index + 2 <
            inputLength
        )
        {
            const uint32_t value =
                (
                    static_cast<uint32_t>(
                        static_cast<uint8_t>(
                            input[index]
                        )
                    ) << 16
                ) |
                (
                    static_cast<uint32_t>(
                        static_cast<uint8_t>(
                            input[index + 1]
                        )
                    ) << 8
                ) |
                static_cast<uint32_t>(
                    static_cast<uint8_t>(
                        input[index + 2]
                    )
                );

            output +=
                alphabet[
                    (
                        value >> 18
                    ) &
                    0x3FU
                ];

            output +=
                alphabet[
                    (
                        value >> 12
                    ) &
                    0x3FU
                ];

            output +=
                alphabet[
                    (
                        value >> 6
                    ) &
                    0x3FU
                ];

            output +=
                alphabet[
                    value &
                    0x3FU
                ];

            index +=
                3;
        }

        const size_t remaining =
            inputLength - index;

        if (remaining == 1)
        {
            const uint32_t value =
                static_cast<uint32_t>(
                    static_cast<uint8_t>(
                        input[index]
                    )
                ) << 16;

            output +=
                alphabet[
                    (
                        value >> 18
                    ) &
                    0x3FU
                ];

            output +=
                alphabet[
                    (
                        value >> 12
                    ) &
                    0x3FU
                ];

            output +=
                '=';

            output +=
                '=';
        }
        else if (remaining == 2)
        {
            const uint32_t value =
                (
                    static_cast<uint32_t>(
                        static_cast<uint8_t>(
                            input[index]
                        )
                    ) << 16
                ) |
                (
                    static_cast<uint32_t>(
                        static_cast<uint8_t>(
                            input[index + 1]
                        )
                    ) << 8
                );

            output +=
                alphabet[
                    (
                        value >> 18
                    ) &
                    0x3FU
                ];

            output +=
                alphabet[
                    (
                        value >> 12
                    ) &
                    0x3FU
                ];

            output +=
                alphabet[
                    (
                        value >> 6
                    ) &
                    0x3FU
                ];

            output +=
                '=';
        }

        return output;
    }

    void sendHeaders()
    {
        // ====================================================
        // REQUEST LINE
        // ====================================================

        client.print(
            method
        );

        client.print(
            ' '
        );

        client.print(
            path
        );

        client.println(
            " HTTP/1.1"
        );

        // ====================================================
        // HOST
        // ====================================================

        client.print(
            "Host: "
        );

        client.print(
            host
        );

        if (port != 80)
        {
            client.print(
                ':'
            );

            client.print(
                port
            );
        }

        client.println();

        // ====================================================
        // HTTP BASIC AUTHENTICATION
        // ====================================================

        if (
            credentials.isConfigured()
        )
        {
            String plainCredentials;

            plainCredentials.reserve(
                strlen(
                    credentials.username
                ) +
                1 +
                (
                    credentials.password
                        ? strlen(
                            credentials.password
                        )
                        : 0
                )
            );

            plainCredentials +=
                credentials.username;

            plainCredentials +=
                ':';

            if (credentials.password)
            {
                plainCredentials +=
                    credentials.password;
            }

            const String encodedCredentials =
                EncodeBase64(
                    plainCredentials
                );

            client.print(
                "Authorization: Basic "
            );

            client.println(
                encodedCredentials
            );

            /*
            * Non stampa username, password o token Base64.
            */
            plainCredentials =
                "";
        }

        // ====================================================
        // COMMON HEADERS
        // ====================================================

        client.println(
            "Accept: */*"
        );

        client.println(
            "Connection: close"
        );

        // ====================================================
        // BODY
        // ====================================================

        if (
            method == "POST" ||
            method == "PUT" ||
            method == "PATCH"
        )
        {
            client.println(
                "Content-Type: application/json"
            );

            client.print(
                "Content-Length: "
            );

            client.println(
                body.length()
            );

            client.println();

            if (body.length() > 0)
            {
                client.print(
                    body
                );
            }
        }
        else
        {
            client.println();
        }
    }

    void readAvailable()
    {
        while (client.available())
        {
            int c = client.read();

            if (c >= 0)
            {
                response +=
                    static_cast<char>(c);
            }
        }
    }

    static String extractBody(
        const String& rawResponse)
    {
        const int separator =
            rawResponse.indexOf(
                "\r\n\r\n");

        if (separator < 0)
            return rawResponse;

        return rawResponse.substring(
            separator + 4);
    }

    bool expired(
        unsigned long now) const
    {
        return
            static_cast<uint32_t>(
                now - stateStartedAt)
            >= timeoutMs;
    }

    static const char* StateToString(
        State value)
    {
        switch (value)
        {
            case State::Idle:
                return "Idle";

            case State::Connecting:
                return "Connecting";

            case State::Sending:
                return "Sending";

            case State::WaitingResponse:
                return "WaitingResponse";

            case State::Reading:
                return "Reading";

            case State::Completed:
                return "Completed";

            case State::Error:
                return "Error";

            default:
                return "Unknown";
        }
    }

    void fail(
        const char* reason)
    {
        const unsigned long now =
            millis();

        const unsigned long elapsedMs =
            static_cast<unsigned long>(
                now - stateStartedAt
            );

        const bool connected =
            client.connected();

        const int availableBytes =
            client.available();

        const char* currentState =
            StateToString(
                state
            );

        const char* failureReason =
            reason
                ? reason
                : "HTTP error";

        LOG_EF(
            "WEBAPI-HTTP",
            "%s: state=%s host=%s port=%u "
            "method=%s path=%s "
            "elapsed=%lu ms timeout=%lu ms "
            "connected=%u available=%d "
            "received=%u auth=%u",
            failureReason,
            currentState,
            host.length() > 0
                ? host.c_str()
                : "",
            static_cast<unsigned>(
                port
            ),
            method.length() > 0
                ? method.c_str()
                : "",
            path.length() > 0
                ? path.c_str()
                : "",
            elapsedMs,
            static_cast<unsigned long>(
                timeoutMs
            ),
            connected
                ? 1U
                : 0U,
            availableBytes,
            static_cast<unsigned>(
                response.length()
            ),
            credentials.isConfigured()
                ? 1U
                : 0U
        );

        client.stop();

        /*
        * Non chiamiamo reset() qui.
        *
        * Lo stato Error deve restare visibile a loopRequest(),
        * che genererà la HttpResponse di fallimento e invocherà
        * la callback del DeviceMessageEngine.
        */
        state =
            State::Error;
    }

    void reset()
    {
        client.stop();

        state =
            State::Idle;

        method =
            "";

        url =
            "";

        body =
            "";

        credentials =
            HttpCredentials{};

        host =
            "";

        path =
            "";

        response =
            "";

        port =
            80;

        stateStartedAt =
            0;

        callback =
            nullptr;
    }
};
   
   
// ============================================================================
// HTTP SERVER TRANSPORT
//
// Server HTTP/1.1 non bloccante.
//
// Responsabilità:
//   • ascolto su porta configurabile
//   • ricezione richiesta HTTP
//   • parsing request line e header
//   • lettura body
//   • routing verso handler registrati
//   • invio risposta HTTP
//
// Non contiene logica specifica Fanvil o di altri dispositivi.
// ============================================================================

class HttpServerTransport :
    public IHttpServerTransport
{
private:

    enum class State : uint8_t
    {
        Idle,
        ReadingHeaders,
        ReadingBody,
        Dispatching,
        Responding,
        Closing
    };

    static constexpr size_t MAX_HANDLERS = 8;
    static constexpr size_t MAX_HEADER_SIZE = 2048;
    static constexpr size_t MAX_BODY_SIZE = 4096;

    uint16_t port = 80;

    DMEthernetServer server;
    DMEthernetClient client;

    State state = State::Idle;

    IHttpServerHandler*
        handlers[MAX_HANDLERS] {};

    size_t handlerCount = 0;

    HttpServerRequest request;
    HttpServerResponse response;

    String headerBuffer;

    unsigned long stateStartedAt = 0;

    uint32_t timeoutMs = 3000;

    bool running = false;


public:

    explicit HttpServerTransport(
        uint16_t listenPort = 80,
        uint32_t timeout = 3000)
        : port(listenPort),
          server(listenPort),
          timeoutMs(timeout)
    {
    }


    // ========================================================================
    // INIT
    // ========================================================================

    bool begin() override
    {
        resetRequest();

        server.begin();

        running = true;

        LOG_DF(
            "WEBAPI-SERVER",
            "HTTP server started on port %u",
            port
        );

        return true;
    }


    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    void setTimeout(uint32_t timeout)
    {
        timeoutMs = timeout;
    }


    bool addHandler(
        IHttpServerHandler* handler) override
    {
        if (!handler)
            return false;

        if (handlerCount >= MAX_HANDLERS)
        {
            LOG_E(
                "WEBAPI-SERVER",
                "Maximum handler count reached"
            );

            return false;
        }

        handlers[handlerCount++] =
            handler;

        return true;
    }


    // ========================================================================
    // STATUS
    // ========================================================================

    bool isRunning() const override
    {
        return running;
    }


    uint16_t getPort() const override
    {
        return port;
    }


    // ========================================================================
    // LOOP
    // ========================================================================

    void loop(unsigned long now) override
    {
        if (!running)
            return;

        switch (state)
        {
            case State::Idle:
                acceptClient(now);
                break;

            case State::ReadingHeaders:
                readHeaders(now);
                break;

            case State::ReadingBody:
                readBody(now);
                break;

            case State::Dispatching:
                dispatch();
                state = State::Responding;
                break;

            case State::Responding:
                sendResponse();
                state = State::Closing;
                break;

            case State::Closing:
                closeClient();
                break;
        }
    }


private:

    // ========================================================================
    // ACCEPT CLIENT
    // ========================================================================

    void acceptClient(unsigned long now)
    {
        DMEthernetClient incoming =
            server.available();

        if (!incoming)
            return;

        client = incoming;

        resetRequest();

        state = State::ReadingHeaders;
        stateStartedAt = now;
    }


    // ========================================================================
    // READ HEADERS
    // ========================================================================

    void readHeaders(unsigned long now)
    {
        while (client.available())
        {
            const int value =
                client.read();

            if (value < 0)
                break;

            headerBuffer +=
                static_cast<char>(value);

            stateStartedAt = now;

            if (
                headerBuffer.length() >
                MAX_HEADER_SIZE
            )
            {
                buildErrorResponse(
                    413,
                    "Request headers too large"
                );

                state = State::Responding;
                return;
            }

            if (headerBuffer.endsWith("\r\n\r\n"))
            {
                if (!parseHeaders())
                {
                    buildErrorResponse(
                        400,
                        "Bad Request"
                    );

                    state = State::Responding;
                    return;
                }

                if (
                    request.contentLength >
                    MAX_BODY_SIZE
                )
                {
                    buildErrorResponse(
                        413,
                        "Request body too large"
                    );

                    state = State::Responding;
                    return;
                }

                state =
                    request.contentLength > 0
                        ? State::ReadingBody
                        : State::Dispatching;

                stateStartedAt = now;

                return;
            }
        }

        if (!client.connected())
        {
            closeClient();
            return;
        }

        if (expired(now))
        {
            buildErrorResponse(
                408,
                "Request Timeout"
            );

            state = State::Responding;
        }
    }


    // ========================================================================
    // READ BODY
    // ========================================================================

    void readBody(unsigned long now)
    {
        while (
            client.available() &&
            request.body.length() <
                request.contentLength
        )
        {
            const int value =
                client.read();

            if (value < 0)
                break;

            request.body +=
                static_cast<char>(value);

            stateStartedAt = now;
        }

        if (
            request.body.length() >=
            request.contentLength
        )
        {
            state = State::Dispatching;
            return;
        }

        if (!client.connected())
        {
            buildErrorResponse(
                400,
                "Incomplete request body"
            );

            state = State::Responding;
            return;
        }

        if (expired(now))
        {
            buildErrorResponse(
                408,
                "Request Timeout"
            );

            state = State::Responding;
        }
    }


    // ========================================================================
    // PARSE HEADERS
    // ========================================================================

    bool parseHeaders()
    {
        const int firstLineEnd =
            headerBuffer.indexOf("\r\n");

        if (firstLineEnd <= 0)
            return false;

        const String requestLine =
            headerBuffer.substring(
                0,
                firstLineEnd
            );

        if (!parseRequestLine(requestLine))
            return false;

        int position =
            firstLineEnd + 2;

        while (position < headerBuffer.length())
        {
            const int lineEnd =
                headerBuffer.indexOf(
                    "\r\n",
                    position
                );

            if (lineEnd < 0)
                break;

            if (lineEnd == position)
                break;

            String line =
                headerBuffer.substring(
                    position,
                    lineEnd
                );

            parseHeaderLine(line);

            position = lineEnd + 2;
        }

        return true;
    }


    // ========================================================================
    // PARSE REQUEST LINE
    // ========================================================================

    bool parseRequestLine(
        const String& line)
    {
        const int methodEnd =
            line.indexOf(' ');

        if (methodEnd <= 0)
            return false;

        const int targetEnd =
            line.indexOf(
                ' ',
                methodEnd + 1
            );

        if (targetEnd <= methodEnd)
            return false;

        request.method =
            line.substring(
                0,
                methodEnd
            );

        String target =
            line.substring(
                methodEnd + 1,
                targetEnd
            );

        request.method.toUpperCase();

        const int queryStart =
            target.indexOf('?');

        if (queryStart >= 0)
        {
            request.path =
                target.substring(
                    0,
                    queryStart
                );

            request.query =
                target.substring(
                    queryStart + 1
                );
        }
        else
        {
            request.path = target;
            request.query = "";
        }

        return request.path.length() > 0;
    }


    // ========================================================================
    // PARSE HEADER
    // ========================================================================

    void parseHeaderLine(String line)
    {
        const int separator =
            line.indexOf(':');

        if (separator <= 0)
            return;

        String key =
            line.substring(
                0,
                separator
            );

        String value =
            line.substring(
                separator + 1
            );

        key.trim();
        value.trim();

        key.toLowerCase();

        if (key == "content-length")
        {
            request.contentLength =
                static_cast<size_t>(
                    value.toInt()
                );
        }
        else if (key == "content-type")
        {
            request.contentType =
                value;
        }
        else if (key == "authorization")
        {
            request.authorization =
                value;
        }
    }


    // ========================================================================
    // DISPATCH
    // ========================================================================

    void dispatch()
    {
        LOG_DF(
            "WEBAPI-SERVER",
            "%s %s query='%s' body='%s'",
            request.method.c_str(),
            request.path.c_str(),
            request.query.c_str(),
            request.body.c_str()
        );

        for (
            size_t i = 0;
            i < handlerCount;
            ++i
        )
        {
            IHttpServerHandler* handler =
                handlers[i];

            if (
                !handler ||
                !handler->matches(request)
            )
            {
                continue;
            }

            handler->handle(
                request,
                response
            );

            return;
        }

        buildErrorResponse(
            404,
            "Not Found"
        );
    }


    // ========================================================================
    // SEND RESPONSE
    // ========================================================================

    void sendResponse()
    {
        if (!client)
            return;

        client.print("HTTP/1.1 ");
        client.print(response.statusCode);
        client.print(' ');
        client.println(
            statusText(response.statusCode)
        );

        client.print("Content-Type: ");
        client.println(response.contentType);

        client.print("Content-Length: ");
        client.println(response.body.length());

        client.println(
            response.closeConnection
                ? "Connection: close"
                : "Connection: keep-alive"
        );

        client.println();

        if (response.body.length() > 0)
            client.print(response.body);
    }


    // ========================================================================
    // ERROR RESPONSE
    // ========================================================================

    void buildErrorResponse(
        int statusCode,
        const char* message)
    {
        response.statusCode =
            statusCode;

        response.contentType =
            "text/plain";

        response.body =
            message ? message : "";

        response.closeConnection =
            true;
    }


    // ========================================================================
    // RESET / CLOSE
    // ========================================================================

    void closeClient()
    {
        client.stop();

        resetRequest();

        state = State::Idle;
    }


    void resetRequest()
    {
        headerBuffer = "";

        request =
            HttpServerRequest{};

        response =
            HttpServerResponse{};

        stateStartedAt = 0;
    }


    // ========================================================================
    // TIMEOUT
    // ========================================================================

    bool expired(unsigned long now) const
    {
        return
            static_cast<uint32_t>(
                now - stateStartedAt
            ) >= timeoutMs;
    }


    // ========================================================================
    // STATUS TEXT
    // ========================================================================

    static const char* statusText(
        int statusCode)
    {
        switch (statusCode)
        {
            case 200:
                return "OK";

            case 201:
                return "Created";

            case 204:
                return "No Content";

            case 400:
                return "Bad Request";

            case 401:
                return "Unauthorized";

            case 403:
                return "Forbidden";

            case 404:
                return "Not Found";

            case 405:
                return "Method Not Allowed";

            case 408:
                return "Request Timeout";

            case 413:
                return "Payload Too Large";

            case 500:
                return "Internal Server Error";

            default:
                return "HTTP Response";
        }
    }
};

#endif
