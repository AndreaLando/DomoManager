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
#include "DMRS485Node.hpp"

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
class HttpAdapter : public TransportBase {
    SimpleHttpTransport* transport;
    String pendingResp;
    DeviceMessageEngine::PlaceholderResolver resolver;
    ICommandTransport::ResponseCallback cb;
    unsigned long startTs = 0;
    unsigned long timeoutMs = 0;

public:
    HttpAdapter(SimpleHttpTransport* t) : transport(t) {}

    bool begin() override {
        return transport->begin();
    }

    bool send(const String& payload) override {
        transport->startPOST(payload, payload);
        return true;
    }

    bool request(const String& payload,
                 ResponseCallback callback,
                 unsigned long tmo) override
    {
        cb = callback;
        timeoutMs = tmo;
        transport->startPOST(payload, payload);
        startTs = millis();
        return true;
    }

protected:
    bool rawReceive(String& out) override {
        if (pendingResp.length() == 0)
            return false;

        out = pendingResp;
        pendingResp = "";
        return true;
    }

    void rawLoop(unsigned long now) override {
        String out;
        if (transport->loop(now, out)) {
            if (cb) cb(out);
        } else if (startTs && (now - startTs > timeoutMs)) {
            if (cb) cb(String());
            startTs = 0;
        }
    }
};


class RS485NodeAdapter : public TransportBase {
private:
    RS485Node node;

    struct Packet {
        String data;
    };
    std::vector<Packet> rxQueue;

    ResponseCallback pendingCb = nullptr;
    unsigned long timeoutAt = 0;

public:
    RS485NodeAdapter(bool master=false)
        : node(master)
    {
        node.onPacket([](uint8_t type, uint8_t* payload, uint8_t len){
            if (instance) instance->onPacketInternal(type, payload, len);
        });
    }

    static inline RS485NodeAdapter* instance = nullptr;

    void onPacketInternal(uint8_t type, uint8_t* payload, uint8_t len) {
        String s;
        for (int i = 0; i < len; i++)
            s += (char)payload[i];

        rxQueue.push_back({ s });
    }

    bool begin() override {
        instance = this;
        node.begin(9600);
        return true;
    }

    bool send(const String& payload) override {
        node.sendPacket(
            0x10,
            (uint8_t*)payload.c_str(),
            payload.length(),
            false
        );
        return true;
    }

    bool request(const String& payload, ResponseCallback cb, unsigned long timeoutMs) override {
        pendingCb = cb;
        timeoutAt = millis() + timeoutMs;

        node.sendPacket(
            0x90,
            (uint8_t*)payload.c_str(),
            payload.length(),
            true
        );
        return true;
    }

protected:
    bool rawReceive(String& out) override {
        if (rxQueue.empty())
            return false;

        out = rxQueue.front().data;
        rxQueue.erase(rxQueue.begin());
        return true;
    }

    void rawLoop(unsigned long now) override {
        node.poll();

        if (pendingCb && now > timeoutAt) {
            pendingCb(String());
            pendingCb = nullptr;
        }
    }
};



#endif
