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
#endif
