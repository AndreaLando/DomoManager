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

class UdpAdapter : public TransportBase
{
private:

    // ========================================================
    // UDP
    // ========================================================

    EthernetUDP udp;

    IPAddress remoteIP;

    uint16_t remotePort;

    uint16_t localPort;


    // ========================================================
    // SOCKET MANAGER
    // ========================================================

    NetworkManager* networkManager = nullptr;

    SocketManager::OwnerId socketOwner = -1;

    bool socketAcquired = false;

    bool started = false;


public:

    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    UdpAdapter(
        const IPAddress& ip,
        uint16_t rPort,
        uint16_t lPort)
        : remoteIP(ip),
          remotePort(rPort),
          localPort(lPort),
          networkManager(nullptr),
          socketOwner(-1),
          socketAcquired(false),
          started(false)
    {
    }


    // ========================================================
    // SOCKET CONTEXT
    //
    // Deve essere chiamato DOPO registerProtocol("Bridge")
    // e PRIMA di BridgeEngine::init().
    // ========================================================

    void setSocketContext(
        NetworkManager& manager,
        SocketManager::OwnerId owner)
    {
        networkManager = &manager;

        socketOwner = owner;

        socketAcquired = false;

        started = false;
    }


    // ========================================================
    // BEGIN
    // ========================================================

    bool begin() override
    {
        // ----------------------------------------------------
        // Validazione SocketManager
        // ----------------------------------------------------

        if (!networkManager)
        {
            LOG_EF(
                "UdpAdapter",
                "NetworkManager unavailable"
            );

            return false;
        }

        if (socketOwner < 0)
        {
            LOG_EF(
                "UdpAdapter",
                "Bridge socket owner unavailable"
            );

            return false;
        }


        // ----------------------------------------------------
        // Già avviato
        // ----------------------------------------------------

        if (started)
        {
            return true;
        }


        // ----------------------------------------------------
        // Acquire SocketManager
        // ----------------------------------------------------

        if (!socketAcquired)
        {
            const int slot =
                networkManager->sockets().acquire(
                    socketOwner,
                    0,
                    SocketManager::SocketKind::UDP
                );

            if (slot < 0)
            {
                LOG_WF(
                    "UdpAdapter",
                    "UDP socket unavailable"
                );

                return false;
            }

            socketAcquired = true;

            LOG_IF(
                "UdpAdapter",
                "UDP socket reserved: slot=%d owner=%d",
                slot,
                socketOwner
            );
        }


        // ----------------------------------------------------
        // UDP begin
        // ----------------------------------------------------

        LOG_IF(
            "UdpAdapter",
            "Opening UDP socket localPort=%u remote=%s:%u",
            localPort,
            remoteIP.toString().c_str(),
            remotePort
        );


        const bool ok =
            udp.begin(localPort);


        if (!ok)
        {
            LOG_WF(
                "UdpAdapter",
                "UDP socket OPEN FAILED"
            );

            // ------------------------------------------------
            // Release reservation
            // ------------------------------------------------

            networkManager->sockets().release(
                socketOwner,
                0,
                SocketManager::SocketKind::UDP
            );

            socketAcquired = false;

            started = false;

            return false;
        }


        started = true;


        LOG_IF(
            "UdpAdapter",
            "UDP socket OPEN"
        );


        networkManager->sockets().dump();


        return true;
    }


    // ========================================================
    // STOP
    //
    // NON BLOCKING
    // ========================================================

    void stop()
    {
        if (started)
        {
            udp.stop();

            started = false;

            LOG_IF(
                "UdpAdapter",
                "UDP socket CLOSED"
            );
        }


        if (socketAcquired &&
            networkManager &&
            socketOwner >= 0)
        {
            networkManager->sockets().release(
                socketOwner,
                0,
                SocketManager::SocketKind::UDP
            );

            socketAcquired = false;
        }
    }


    // ========================================================
    // PACKET SIZE
    // ========================================================

    size_t maxPacketSize() const override
    {
        return 1500;
    }


    // ========================================================
    // SEND
    // ========================================================

    bool send(
        const String& payload) override
    {
        if (!started)
            return false;

        if (!payload.length())
            return false;


        LOG_DF(
            "SLAVE::UDP::TX",
            "TX → %s:%u : %s",
            remoteIP.toString().c_str(),
            remotePort,
            payload.c_str()
        );


        if (udp.beginPacket(
                remoteIP,
                remotePort) != 1)
        {
            return false;
        }


        udp.write(
            (const uint8_t*)
                payload.c_str(),
            payload.length()
        );


        return
            udp.endPacket() == 1;
    }


    // ========================================================
    // REQUEST
    // ========================================================

    bool request(
        const String& payload,
        ResponseCallback cb,
        unsigned long timeoutMs = 3000) override
    {
        (void)payload;
        (void)cb;
        (void)timeoutMs;

        // UDP non supporta request/response
        return false;
    }


protected:

    // ========================================================
    // RECEIVE
    // ========================================================

    bool rawReceive(
        String& out) override
    {
        out = "";


        if (!started)
            return false;


        const int packetSize =
            udp.parsePacket();


        if (packetSize <= 0)
            return false;


        // ----------------------------------------------------
        // Verifica IP/porta
        // ----------------------------------------------------

        IPAddress sender =
            udp.remoteIP();

        uint16_t port =
            udp.remotePort();


        if (sender != remoteIP ||
            port != remotePort)
        {
            LOG_WF(
                "UDP",
                "Packet from unauthorized sender %s:%u",
                sender.toString().c_str(),
                port
            );

            udp.flush();

            return false;
        }


        // ----------------------------------------------------
        // Protezione dimensione
        // ----------------------------------------------------

        const size_t limit =
            maxPacketSize();


        if (packetSize > limit)
        {
            char dump[64];

            udp.read(
                dump,
                sizeof(dump)
            );


            LOG_WF(
                "UDP",
                "Packet too large (%d > %u), discarded",
                packetSize,
                limit
            );


            return false;
        }


        // ----------------------------------------------------
        // Static buffer
        // ----------------------------------------------------

        static char buf[1600];


        const int len =
            udp.read(
                buf,
                limit
            );


        if (len <= 0)
            return false;


        buf[len] = '\0';


        out.reserve(len);

        out = buf;


        LOG_DF(
            "SLAVE::UDP::RX",
            "RX ← %s:%u : %s",
            sender.toString().c_str(),
            port,
            out.c_str()
        );


        return true;
    }


    // ========================================================
    // RAW LOOP
    // ========================================================

    void rawLoop(
        unsigned long now) override
    {
        (void)now;

        // UDP non richiede loop
    }
};



#endif
