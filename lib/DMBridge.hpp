#ifndef DMBridge_HPP
#define DMBridge_HPP

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
#include <EthernetUdp.h>

#include "DMBaseClass.hpp"
#include "DMAEE.hpp"
#include "DMDeclares.h"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


class BridgePeerMonitor {
private:
    unsigned long lastActivity = 0;
    bool online = false;

public:
    void onPacket(unsigned long now) {
        lastActivity = now;
        if (!online) {
            online = true;
            LOG_IF("Bridge", "Peer ONLINE");
        }
    }

    void checkOffline(unsigned long now, unsigned long timeoutMs = 120000) {
        if (online && (now - lastActivity > timeoutMs)) {
            online = false;
            LOG_IF("Bridge", "Peer OFFLINE");
        }
    }

    bool isOnline() const { return online; }
};


class BridgeProtocol {
public:
    static bool handleIncoming(const String& msg, AEERegistry& reg, ICommandTransport& transport) {
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, msg)) {
            LOG_WF("Bridge", "Pacchetto non JSON ignorato");
            return false;
        }

        bool ok = AEEProtocol::parseJSON(msg, reg);
        if (!ok) return false;

        if (doc.containsKey("seq")) {
            uint32_t seq = doc["seq"];
            StaticJsonDocument<64> ack;
            ack["ack"] = seq;

            String out;
            serializeJson(ack, out);
            transport.send(out);
        }

        return true;
    }

};


class AEEProtocolEngine {
public:
    using SendCallback = std::function<void(const String&)>;

    struct AckState {
        uint32_t seqCounter = 0;
        uint32_t lastSentSeq = 0;
        uint32_t lastAckSeq = 0;
        unsigned long lastSendTime = 0;

        bool ackOk() const { return lastAckSeq == lastSentSeq; }
        bool ackTimeout(unsigned long now, unsigned long timeoutMs) const {
            return !ackOk() && (now - lastSendTime > timeoutMs);
        }
        uint32_t nextSeq() { return ++seqCounter; }
    };

    static void send(AEERegistry& reg,
                     AckState& state,
                     SendCallback sendFn)
    {
        String json = AEEProtocol::serializeChangedJSON(reg);
        if (json.length() == 0) return;

        StaticJsonDocument<1024> doc;
        deserializeJson(doc, json);

        uint32_t seq = state.nextSeq();
        doc["seq"] = seq;

        String out;
        serializeJson(doc, out);

        // 🔥 invio tramite callback
        sendFn(out);

        state.lastSentSeq = seq;
        state.lastSendTime = millis();
    }

    static void onPacket(const String& msg, AckState& state) {
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, msg)) return;

        if (doc.containsKey("ack")) {
            state.lastAckSeq = doc["ack"];
        }
    }

    static void checkResend(AEERegistry& reg,
                        AckState& state,
                        unsigned long now,
                        SendCallback sendFn)
    {
        const unsigned long RESEND_TIMEOUT = 120;   // più sicuro
        const unsigned long MIN_INTERVAL   = 40;    // evita burst

        if (state.ackTimeout(now, RESEND_TIMEOUT) &&
            (now - state.lastSendTime > MIN_INTERVAL))
        {
            send(reg, state, sendFn);
        }
    }

};

class BridgeEngine {
private:
    static inline ICommandTransport* transport = nullptr;
    static inline AEERegistry* regPtr = nullptr;
    static inline BridgePeerMonitor peer;
    static inline AEEProtocolEngine::AckState ack;

public:

    static void Init(ICommandTransport* t, AEERegistry& reg) {
        transport = t;
        regPtr = &reg;
        transport->begin();
    }

    static void Loop(unsigned long now) {
        if (!transport || !regPtr) {
            LOG_WF("BRIDGE", "Loop chiamato ma Bridge non inizializzato");
            return;
        }

        transport->loop(now);

        // Non abbiamo più available()/read() → usiamo request/response
        // quindi serve un adapter UDP che implementa available/read
        // oppure un metodo "pull" nel transport
        // soluzione: estendere ICommandTransport con onMessage()

        // ACK resend
        AEEProtocolEngine::checkResend(
            *regPtr, ack, now,
            [&](const String& out){ transport->send(out); }
        );
    }

    static bool isPeerOnline() {
        if (!transport)
            return false;   // Bridge disabilitato → peer offline
        return transport->isOnline();
    }


    static void SendRaw(const String& msg) {
        if (transport) transport->send(msg);
    }
};



#endif
