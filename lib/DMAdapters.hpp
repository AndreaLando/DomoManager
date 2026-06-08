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

#include "DMDeclares.h"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class BridgeEngine; //forward declaration


// MqttAdapter.h (snippet)
class MqttAdapter : public ICommandTransport {
    MQTTEngine* engine;
    String topic;
public:
    MqttAdapter(MQTTEngine* e, const String& t): engine(e), topic(t) {}
    bool begin() override { return true; }
    bool send(const String& payload) override {
        return true;
    }
    bool request(const String& payload, ResponseCallback cb, unsigned long timeoutMs) override {
        // implementare pattern reply-to con topic temporaneo o correlation id
        return false; // implementazione dipende dal supporto in MQTTEngine
    }
    void loop(unsigned long now) override { /* nulla */ }
    bool isOnline() const override { return true; }
};

// BridgeAdapter.h (snippet)
class BridgeAdapter : public ICommandTransport {
public:
    bool begin() override { return true; }
    bool send(const String& payload) override {
        BridgeEngine::SendRaw(payload);
        return true;
    }
    bool request(const String& payload, ResponseCallback cb, unsigned long timeoutMs) override {
        // BridgeEngine + DeviceMessageEngine supportano pending request:
        // DeviceMessageEngine::Send(profile, outKey, resolver) + Loop() gestisce pending
        // Qui si può invocare DeviceMessageEngine::Send e registrare callback tramite JobVarStore
        return false; // implementazione concreta dipende da DeviceMessageEngine API
    }
    void loop(unsigned long now) override { BridgeEngine::Loop(now); }
    bool isOnline() const override { return BridgeEngine::isPeerOnline(); }
};

// HttpAdapter.h (snippet)
class HttpAdapter : public ICommandTransport {
    SimpleHttpTransport* transport;
    String pendingResp;
    DeviceMessageEngine::PlaceholderResolver resolver;
    ICommandTransport::ResponseCallback cb;
    unsigned long startTs = 0;
    unsigned long timeoutMs = 0;
public:
    HttpAdapter(SimpleHttpTransport* t): transport(t) {}
    bool begin() override { return transport->begin(); }
    bool send(const String& payload) override {
        transport->startPOST(payload, payload); // adattare
        return true;
    }
    bool request(const String& payload, ResponseCallback callback, unsigned long tmo) override {
        cb = callback; timeoutMs = tmo;
        transport->startPOST(payload, payload);
        startTs = millis();
        return true;
    }
    void loop(unsigned long now) override {
        String out;
        if (transport->loop(now, out)) {
            if (cb) cb(out);
        } else if (startTs && (now - startTs > timeoutMs)) {
            if (cb) cb(String()); // timeout -> empty
            startTs = 0;
        }
    }
    bool isOnline() const override { return true; }
};

class RS485NodeAdapter : public ICommandTransport {
private:
    RS485Node node;
    ResponseCallback pendingCb = nullptr;
    unsigned long timeoutAt = 0;

public:
    RS485NodeAdapter(bool master=false)
        : node(master) {}

    bool begin() override {
        node.begin(9600);
        return true;
    }

    // FIRE-AND-FORGET
    bool send(const String& payload) override {
        node.sendPacket(
            0x10, 
            (uint8_t*)payload.c_str(), 
            payload.length(), 
            false
        );
        return true;
    }

    // REQUEST/RESPONSE
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

    // LOOP
    void loop(unsigned long now) override {
        node.poll();

        if (pendingCb && now > timeoutAt) {
            pendingCb(String()); // timeout
            pendingCb = nullptr;
        }
    }

    // ONLINE STATUS
    bool isOnline() const override {
        return !node.isAwaitingAck();
    }
};

class UdpAdapter : public ICommandTransport {
private:
    EthernetUDP udp;
    IPAddress ip;
    uint16_t localPort;
    uint16_t remotePort;
    ResponseCallback cb;

public:
    UdpAdapter(IPAddress ip, uint16_t lp, uint16_t rp)
        : ip(ip), localPort(lp), remotePort(rp) {}

    bool begin() override {
        return udp.begin(localPort) == 1;
    }

    bool send(const String& payload) override {
        udp.beginPacket(ip, remotePort);
        udp.print(payload);
        udp.endPacket();
        return true;
    }

    bool request(const String& payload, ResponseCallback callback, unsigned long timeoutMs) override {
        cb = callback;
        send(payload);
        return true;
    }

    void loop(unsigned long now) override {
        int size = udp.parsePacket();
        if (size > 0) {
            char buf[512];
            int len = udp.read(buf, sizeof(buf)-1);
            buf[len] = 0;
            if (cb) cb(String(buf));
        }
    }

    bool isOnline() const override { return true; }
};

#endif
