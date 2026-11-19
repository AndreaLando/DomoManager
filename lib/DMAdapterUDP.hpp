#ifndef DMADAPTERUDP_HPP
#define DMADAPTERUDP_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it

   Versione modulo: 1.1.2
   Ultima modifica: 2026-11-17

   Note:
                    - Backend UDP astratto tramite DMEthernetUDP.
                    - Compatibile con EthernetUDP e WiFiUDP.
                    - Mantiene DMNetworkManager e SocketManager.
                    - Mantiene endpoint statico e dinamico.

   ============================================================================ */

#include <Arduino.h>
#include <IPAddress.h>
#include <String.h>
#include <cstdint>
#include <functional>

#include "DMEthernet.hpp"
#include "DMNetwork.hpp"
#include "DMTransport.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class UdpAdapter final :
    public TransportBase
{
private:
    // ========================================================
    // CONSTANTS
    // ========================================================

    static constexpr size_t MAX_PACKET_SIZE =
        1500U;

    static constexpr int UDP_RESOURCE_ID =
        0;

    // ========================================================
    // UDP BACKEND
    //
    // DMEthernetUDP viene selezionato da DMEthernet.hpp:
    //
    // Ethernet -> EthernetUDP
    // Wi-Fi    -> WiFiUDP
    // ========================================================

    DMEthernetUDP udp;

    // ========================================================
    // CONFIGURED ENDPOINT
    //
    // configuredRemoteIP == 0.0.0.0:
    //     endpoint remoto dinamico.
    //
    // configuredRemoteIP != 0.0.0.0:
    //     endpoint remoto statico.
    // ========================================================

    IPAddress configuredRemoteIP;

    uint16_t configuredRemotePort =
        0;

    // ========================================================
    // ACTIVE ENDPOINT
    // ========================================================

    IPAddress remoteIP;

    uint16_t remotePort =
        0;

    uint16_t localPort =
        0;

    bool dynamicRemote =
        false;

    bool remoteLearned =
        false;

    // ========================================================
    // SOCKET CONTEXT
    // ========================================================

    DMNetworkManager* networkManager =
        nullptr;

    SocketManager::OwnerId socketOwner =
        -1;

    int socketSlot =
        -1;

    bool socketAcquired =
        false;

    bool started =
        false;

private:
    // ========================================================
    // ADDRESS UTILITIES
    // ========================================================

    static bool isNullAddress(
        const IPAddress& address)
    {
        return
            address[0] == 0 &&
            address[1] == 0 &&
            address[2] == 0 &&
            address[3] == 0;
    }

    static bool sameAddress(
        const IPAddress& first,
        const IPAddress& second)
    {
        return
            first[0] == second[0] &&
            first[1] == second[1] &&
            first[2] == second[2] &&
            first[3] == second[3];
    }

    static bool sameEndpoint(
        const IPAddress& firstIP,
        uint16_t firstPort,
        const IPAddress& secondIP,
        uint16_t secondPort)
    {
        return
            sameAddress(
                firstIP,
                secondIP
            ) &&
            firstPort == secondPort;
    }

    // ========================================================
    // ACTIVE ENDPOINT MANAGEMENT
    // ========================================================

    void clearRemoteEndpoint()
    {
        if (!dynamicRemote)
        {
            remoteIP =
                configuredRemoteIP;

            remotePort =
                configuredRemotePort;

            remoteLearned =
                !isNullAddress(remoteIP) &&
                remotePort != 0;

            return;
        }

        remoteIP =
            IPAddress(
                0,
                0,
                0,
                0
            );

        remotePort =
            0;

        remoteLearned =
            false;
    }

    bool acceptRemoteEndpoint(
        const IPAddress& senderIP,
        uint16_t senderPort)
    {
        if (
            isNullAddress(senderIP) ||
            senderPort == 0
        )
        {
            return false;
        }

        // ----------------------------------------------------
        // STATIC ENDPOINT
        // ----------------------------------------------------

        if (!dynamicRemote)
        {
            return
                sameEndpoint(
                    senderIP,
                    senderPort,
                    remoteIP,
                    remotePort
                );
        }

        // ----------------------------------------------------
        // DYNAMIC ENDPOINT: FIRST PACKET
        // ----------------------------------------------------

        if (!remoteLearned)
        {
            remoteIP =
                senderIP;

            remotePort =
                senderPort;

            remoteLearned =
                true;

            LOG_IF(
                "UDP",
                "Dynamic endpoint learned: %s:%u",
                remoteIP
                    .toString()
                    .c_str(),
                static_cast<unsigned>(
                    remotePort
                )
            );

            return true;
        }

        // ----------------------------------------------------
        // DYNAMIC ENDPOINT: LOCKED PEER
        // ----------------------------------------------------

        return
            sameEndpoint(
                senderIP,
                senderPort,
                remoteIP,
                remotePort
            );
    }

    // ========================================================
    // PACKET CLEANUP
    // ========================================================

    void discardCurrentPacket()
    {
        uint8_t dump[64] {};

        while (
            udp.available() >
            0
        )
        {
            const int availableBytes =
                udp.available();

            const size_t readLength =
                static_cast<size_t>(
                    availableBytes
                ) <
                sizeof(dump)
                    ? static_cast<size_t>(
                          availableBytes
                      )
                    : sizeof(dump);

            const int readBytes =
                udp.read(
                    dump,
                    readLength
                );

            if (readBytes <= 0)
            {
                break;
            }
        }

        udp.flush();
    }

    // ========================================================
    // SOCKET ACQUISITION
    // ========================================================

    bool acquireSocket()
    {
        if (socketAcquired)
        {
            return true;
        }

        if (
            !networkManager ||
            socketOwner < 0
        )
        {
            return false;
        }

        socketSlot =
            networkManager
                ->sockets()
                .acquire(
                    socketOwner,
                    UDP_RESOURCE_ID,
                    SocketManager::
                        SocketKind::UDP
                );

        if (socketSlot < 0)
        {
            LOG_WF(
                "UdpAdapter",
                "UDP socket unavailable: owner=%d",
                socketOwner
            );

            return false;
        }

        socketAcquired =
            true;

        LOG_DF(
            "UdpAdapter",
            "UDP socket reserved: slot=%d owner=%d",
            socketSlot,
            socketOwner
        );

        return true;
    }

    void releaseSocket()
    {
        if (!socketAcquired)
        {
            socketSlot =
                -1;

            return;
        }

        if (
            networkManager &&
            socketOwner >= 0
        )
        {
            networkManager
                ->sockets()
                .release(
                    socketOwner,
                    UDP_RESOURCE_ID,
                    SocketManager::
                        SocketKind::UDP
                );
        }

        socketAcquired =
            false;

        socketSlot =
            -1;
    }

public:
    // ========================================================
    // CONSTRUCTION
    // ========================================================

    UdpAdapter(
        const IPAddress& ip,
        uint16_t listenPort,
        uint16_t targetPort)
        : configuredRemoteIP(
              ip
          ),
          configuredRemotePort(
              targetPort
          ),
          remoteIP(
              ip
          ),
          remotePort(
              targetPort
          ),
          localPort(
              listenPort
          ),
          dynamicRemote(
              isNullAddress(
                  ip
              )
          ),
          remoteLearned(
              !isNullAddress(ip) &&
              targetPort != 0
          )
    {
        clearRemoteEndpoint();
    }

    ~UdpAdapter() override
    {
        stop();
    }

    UdpAdapter(
        const UdpAdapter&) =
            delete;

    UdpAdapter& operator=(
        const UdpAdapter&) =
            delete;

    // ========================================================
    // SOCKET CONTEXT
    // ========================================================

    void setSocketContext(
        DMNetworkManager& manager,
        SocketManager::OwnerId owner)
    {
        stop();

        networkManager =
            &manager;

        socketOwner =
            owner;

        socketSlot =
            -1;

        socketAcquired =
            false;

        started =
            false;

        online =
            false;

        lastRx =
            0;

        clearRemoteEndpoint();
    }

    // ========================================================
    // BEGIN
    // ========================================================

    bool begin() override
    {
        if (started)
        {
            return true;
        }

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
                "Socket owner unavailable"
            );

            return false;
        }

        if (localPort == 0)
        {
            LOG_EF(
                "UdpAdapter",
                "Invalid local UDP port"
            );

            return false;
        }

        if (
            !dynamicRemote &&
            (
                isNullAddress(
                    configuredRemoteIP
                ) ||
                configuredRemotePort == 0
            )
        )
        {
            LOG_EF(
                "UdpAdapter",
                "Invalid static remote endpoint"
            );

            return false;
        }

        if (!DMEthernet::linkUp())
        {
            LOG_WF(
                "UdpAdapter",
                "Network link unavailable: backend=%s",
                DMEthernet::backendName()
            );

            return false;
        }

        if (!acquireSocket())
        {
            return false;
        }

        if (dynamicRemote)
        {
            LOG_IF(
                "UdpAdapter",
                "Opening dynamic UDP socket: "
                "backend=%s localPort=%u remote=auto",
                DMEthernet::backendName(),
                static_cast<unsigned>(
                    localPort
                )
            );
        }
        else
        {
            LOG_IF(
                "UdpAdapter",
                "Opening static UDP socket: "
                "backend=%s localPort=%u remote=%s:%u",
                DMEthernet::backendName(),
                static_cast<unsigned>(
                    localPort
                ),
                configuredRemoteIP
                    .toString()
                    .c_str(),
                static_cast<unsigned>(
                    configuredRemotePort
                )
            );
        }

        const bool opened =
            udp.begin(
                localPort
            );

        if (!opened)
        {
            LOG_WF(
                "UdpAdapter",
                "UDP socket open failed: "
                "backend=%s localPort=%u",
                DMEthernet::backendName(),
                static_cast<unsigned>(
                    localPort
                )
            );

            releaseSocket();

            started =
                false;

            online =
                false;

            return false;
        }

        started =
            true;

        online =
            false;

        lastRx =
            0;

        LOG_IF(
            "UdpAdapter",
            "UDP socket opened: backend=%s local=%s:%u",
            DMEthernet::backendName(),
            DMEthernet::localIP()
                .toString()
                .c_str(),
            static_cast<unsigned>(
                localPort
            )
        );

        return true;
    }

    // ========================================================
    // STOP
    // ========================================================

    void stop()
    {
        if (started)
        {
            udp.stop();

            started =
                false;

            LOG_IF(
                "UdpAdapter",
                "UDP socket closed: localPort=%u",
                static_cast<unsigned>(
                    localPort
                )
            );
        }

        releaseSocket();

        online =
            false;

        lastRx =
            0;

        if (dynamicRemote)
        {
            clearRemoteEndpoint();
        }
    }

    // ========================================================
    // STATUS / CONFIGURATION
    // ========================================================

    size_t maxPacketSize() const override
    {
        return
            MAX_PACKET_SIZE;
    }

    bool isStarted() const
    {
        return
            started;
    }

    bool isDynamicEndpoint() const
    {
        return
            dynamicRemote;
    }

    bool hasRemoteEndpoint() const
    {
        return
            remoteLearned &&
            !isNullAddress(
                remoteIP
            ) &&
            remotePort != 0;
    }

    const IPAddress& getConfiguredRemoteIP() const
    {
        return
            configuredRemoteIP;
    }

    uint16_t getConfiguredRemotePort() const
    {
        return
            configuredRemotePort;
    }

    const IPAddress& getRemoteIP() const
    {
        return
            remoteIP;
    }

    uint16_t getRemotePort() const
    {
        return
            remotePort;
    }

    uint16_t getLocalPort() const
    {
        return
            localPort;
    }

    int getSocketSlot() const
    {
        return
            socketSlot;
    }

    // ========================================================
    // SEND
    // ========================================================

    bool send(
        const String& payload) override
    {
        if (!started)
        {
            return false;
        }

        if (
            payload.length() == 0 ||
            !hasRemoteEndpoint()
        )
        {
            return false;
        }

        if (!DMEthernet::linkUp())
        {
            LOG_WF(
                "UDP",
                "TX denied: network link unavailable"
            );

            return false;
        }

        if (
            udp.beginPacket(
                remoteIP,
                remotePort
            ) != 1
        )
        {
            LOG_WF(
                "UDP",
                "beginPacket failed: %s:%u",
                remoteIP
                    .toString()
                    .c_str(),
                static_cast<unsigned>(
                    remotePort
                )
            );

            return false;
        }

        const size_t expected =
            payload.length();

        const size_t written =
            udp.write(
                reinterpret_cast<
                    const uint8_t*
                >(
                    payload.c_str()
                ),
                expected
            );

        if (written != expected)
        {
            LOG_WF(
                "UDP",
                "Partial UDP write: written=%u expected=%u",
                static_cast<unsigned>(
                    written
                ),
                static_cast<unsigned>(
                    expected
                )
            );

            /*
             * Chiude comunque il datagramma per lasciare il
             * backend UDP in uno stato definito.
             */
            (void)udp.endPacket();

            return false;
        }

        const bool sent =
            udp.endPacket() ==
            1;

        if (sent)
        {
            LOG_DF(
                "UDP::TX",
                "TX -> %s:%u bytes=%u",
                remoteIP
                    .toString()
                    .c_str(),
                static_cast<unsigned>(
                    remotePort
                ),
                static_cast<unsigned>(
                    expected
                )
            );
        }

        return sent;
    }

    // ========================================================
    // REQUEST
    // ========================================================

    bool request(
        const String& payload,
        ResponseCallback callback,
        unsigned long requestTimeoutMs =
            3000UL) override
    {
        /*
         * Il bridge usa send()/receive().
         * Il pattern request/callback non è implementato qui.
         */
        (void)payload;
        (void)callback;
        (void)requestTimeoutMs;

        return false;
    }

protected:
    // ========================================================
    // RECEIVE
    // ========================================================

    bool rawReceive(
        String& output) override
    {
        output =
            "";

        if (!started)
        {
            return false;
        }

        const int packetSize =
            udp.parsePacket();

        if (packetSize <= 0)
        {
            return false;
        }

        const IPAddress senderIP =
            udp.remoteIP();

        const uint16_t senderPort =
            udp.remotePort();

        if (
            !acceptRemoteEndpoint(
                senderIP,
                senderPort
            )
        )
        {
            LOG_WF(
                "UDP",
                "Packet from unauthorized sender: "
                "%s:%u expected=%s:%u dynamic=%u",
                senderIP
                    .toString()
                    .c_str(),
                static_cast<unsigned>(
                    senderPort
                ),
                remoteIP
                    .toString()
                    .c_str(),
                static_cast<unsigned>(
                    remotePort
                ),
                dynamicRemote
                    ? 1U
                    : 0U
            );

            discardCurrentPacket();

            return false;
        }

        const size_t limit =
            maxPacketSize();

        if (
            static_cast<size_t>(
                packetSize
            ) >
            limit
        )
        {
            LOG_WF(
                "UDP",
                "Packet too large: received=%d maximum=%u",
                packetSize,
                static_cast<unsigned>(
                    limit
                )
            );

            discardCurrentPacket();

            return false;
        }

        static char buffer[
            MAX_PACKET_SIZE +
            1U
        ] {};

        const int received =
            udp.read(
                reinterpret_cast<uint8_t*>(
                    buffer
                ),
                limit
            );

        if (received <= 0)
        {
            discardCurrentPacket();

            return false;
        }

        buffer[
            static_cast<size_t>(
                received
            )
        ] = '\0';

        if (
            !output.reserve(
                static_cast<size_t>(
                    received
                ) +
                1U
            )
        )
        {
            LOG_WF(
                "UDP",
                "Unable to allocate RX string: bytes=%d",
                received
            );

            discardCurrentPacket();

            return false;
        }

        output =
            buffer;

        discardCurrentPacket();

        LOG_DF(
            "UDP::RX",
            "RX <- %s:%u bytes=%u",
            senderIP
                .toString()
                .c_str(),
            static_cast<unsigned>(
                senderPort
            ),
            static_cast<unsigned>(
                received
            )
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

        if (!started)
        {
            return;
        }

        if (!DMEthernet::linkUp())
        {
            online =
                false;

            return;
        }

        /*
         * Il timeout RX di TransportBase determina soltanto
         * lo stato online/offline del trasporto.
         *
         * Non deve invalidare l'endpoint UDP dinamico appreso:
         * un peer può trasmettere con un intervallo superiore a
         * timeoutMs e l'indirizzo resta comunque valido.
         *
         * L'endpoint dinamico viene azzerato esclusivamente da
         * stop() o setSocketContext(), cioè quando il socket o
         * la relativa sessione vengono realmente reinizializzati.
         */
    }
};

#endif
