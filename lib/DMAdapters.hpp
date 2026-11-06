#ifndef DMAdapters_HPP
#define DMAdapters_HPP

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
#include <functional>
#include <String>

#include "DMTransport.hpp"
#include "DMWebAPI.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


// MqttAdapter.h (snippet)
class MqttAdapter : public TransportBase {
    MQTT* engine;
    String topic;

public:
    MqttAdapter(MQTT* e, const String& t)
        : engine(e), topic(t) {}

    bool begin() override {
        return true;
    }

    bool send(const String& payload) override {
        // il tuo codice originale non pubblica nulla
        // (probabilmente lo farai in futuro)
        return true;
    }

    bool request(const String& payload,
                 ResponseCallback cb,
                 unsigned long timeoutMs) override
    {
        // pattern reply-to non implementato
        return false;
    }

protected:
    bool rawReceive(String& out) override {
        return false; // come nel tuo codice originale
    }

    void rawLoop(unsigned long now) override {
        // nessuna logica MQTT nel tuo codice originale
    }
};


// HttpAdapter.h (snippet)
class HttpAdapter : public TransportBase
{
private:

    HttpClientTransport* transport;

    String pendingResp;

    ResponseCallback cb;

    unsigned long startTs = 0;

    unsigned long requestTimeoutMs = 0;

public:

    explicit HttpAdapter(
        HttpClientTransport* t)
        : transport(t)
    {
    }

    bool begin() override
    {
        return transport
            ? transport->begin()
            : false;
    }

    bool send(
        const String& payload) override
    {
        if (!transport)
            return false;

        HttpRequest req;

        req.method = "POST";
        req.url = payload;
        req.body = payload;

        return transport->request(
            req,
            [this](const HttpResponse& resp)
            {
                if (resp.success)
                {
                    pendingResp = resp.body;
                }
            }
        );
    }

    bool request(
        const String& payload,
        ResponseCallback callback,
        unsigned long timeoutMs) override
    {
        if (!transport)
            return false;

        cb = callback;

        requestTimeoutMs = timeoutMs;

        startTs = millis();

        HttpRequest req;

        req.method = "POST";
        req.url = payload;
        req.body = payload;
        req.timeoutMs = timeoutMs;

        return transport->request(
            req,
            [this](const HttpResponse& resp)
            {
                if (cb)
                {
                    cb(
                        resp.success
                            ? resp.body
                            : String()
                    );
                }

                if (resp.success)
                {
                    pendingResp = resp.body;
                }

                startTs = 0;
            }
        );
    }

protected:

    bool rawReceive(
        String& out) override
    {
        if (pendingResp.isEmpty())
            return false;

        out = pendingResp;

        pendingResp = "";

        return true;
    }

    void rawLoop(
        unsigned long now) override
    {
        if (!transport)
            return;

        transport->loop(now);

        if (
            startTs &&
            requestTimeoutMs &&
            (now - startTs > requestTimeoutMs)
        )
        {
            if (cb)
            {
                cb(String());
            }

            startTs = 0;
        }
    }
};


#endif
