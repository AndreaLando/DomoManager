#ifndef DMAdapterUDP_HPP
#define DMAdapterUDP_HPP

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

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


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
