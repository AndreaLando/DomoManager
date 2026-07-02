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

#include "DMMQTTEngine.hpp"
#include "DMWebAPI.hpp"
#include "DMRS485Node.hpp"

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


class UdpAdapter : public TransportBase {
private:
    EthernetUDP udp;
    IPAddress remoteIP;
    uint16_t remotePort;
    uint16_t localPort;

public:
    UdpAdapter(const IPAddress& ip, uint16_t rPort, uint16_t lPort)
        : remoteIP(ip), remotePort(rPort), localPort(lPort)
    {}

    bool begin() override {
        return udp.begin(localPort);
    }

    size_t maxPacketSize() const override {
        return 1500;   // MTU Ethernet
    }

    bool send(const String& payload) override {
        if (!payload.length()) return false;

        // LOG opzionale
        LOG_DF("SLAVE::UDP::TX", "TX → %s:%u : %s",
            remoteIP.toString().c_str(),
            remotePort,
            payload.c_str());

        if (udp.beginPacket(remoteIP, remotePort) != 1)
            return false;

        udp.write((const uint8_t*)payload.c_str(), payload.length());
        return udp.endPacket() == 1;
    }

    bool request(const String& payload,
                 ResponseCallback cb,
                 unsigned long timeoutMs = 3000) override
    {
        // UDP non supporta request/response
        return false;
    }

protected:

    // 🔥 VERSIONE NON BLOCCANTE
    bool rawReceive(String& out) override {
        out = "";

        int packetSize = udp.parsePacket();
        if (packetSize <= 0)
            return false;

        // Verifica IP/porta
        IPAddress sender = udp.remoteIP();
        uint16_t port = udp.remotePort();

        if (sender != remoteIP || port != remotePort) {
            LOG_WF("UDP", "Packet from unauthorized sender %s:%u",
                sender.toString().c_str(), port);
            udp.flush();
            return false;
        }

        // Protezione dimensione
        size_t limit = maxPacketSize();
        if (packetSize > limit) {
            char dump[64];
            udp.read(dump, sizeof(dump));
            LOG_WF("UDP", "Packet too large (%d > %u), discarded", packetSize, limit);
            return false;
        }

        // Buffer statico (no alloca)
        static char buf[1600];

        int len = udp.read(buf, limit);
        if (len <= 0)
            return false;

        buf[len] = '\0';
        out.reserve(len);
        out = buf;

        LOG_DF("SLAVE::UDP::RX", "RX ← %s:%u : %s",
            sender.toString().c_str(),
            port,
            out.c_str());

        return true;
    }

    void rawLoop(unsigned long now) override {
        // UDP non richiede loop
    }
};



#endif
