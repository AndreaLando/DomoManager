#pragma once

#include <Arduino.h>
#include <IPAddress.h>

/*
===============================================================================
TEMPORARY ESP32 COMPATIBILITY PATCH - DOMOMANAGER
-------------------------------------------------------------------------------

IMPORTANT:
This block is intentionally DISABLED for the ESP32-S3-ETH porting.

ArduinoModbus includes the RTU/RS485 backend even when this project uses only
Modbus TCP. On ESP32, ArduinoRS485 tries to instantiate:

    RS485Class RS485(SERIAL_PORT_HARDWARE, ...);

but SERIAL_PORT_HARDWARE is not defined by the ESP32 core.

Resulting compilation error:

    error: 'SERIAL_PORT_HARDWARE' was not declared in this scope

The DomoManager application currently uses only:

    ModbusTCPClient
    ModbusTCPServer

and does NOT use:

    ModbusRTUClient
    ModbusRTUServer
    RS485

Therefore this global RS485 object is not required by the application.
It is only being compiled because ArduinoModbus includes the RTU backend.

Current ESP32 workaround:
leave this block commented out.

-------------------------------------------------------------------------------
ORIGINAL / ALTERNATIVE ESP32 VERSION
-------------------------------------------------------------------------------

#if defined(ARDUINO_ARCH_ESP32)

RS485Class RS485(Serial1, -1, -1, -1);

#elif defined(RS485_SERIAL_PORT)

RS485Class RS485(RS485_SERIAL_PORT,
                 RS485_DEFAULT_TX_PIN,
                 RS485_DEFAULT_DE_PIN,
                 RS485_DEFAULT_RE_PIN);

#else

RS485Class RS485(SERIAL_PORT_HARDWARE,
                 RS485_DEFAULT_TX_PIN,
                 RS485_DEFAULT_DE_PIN,
                 RS485_DEFAULT_RE_PIN);

#endif

-------------------------------------------------------------------------------
RESTORE INSTRUCTIONS
-------------------------------------------------------------------------------

1. Do NOT modify the DomoManager Modbus logic to solve this issue.
2. Keep the application on Modbus TCP.
3. Keep these project includes where applicable:

       #include <ModbusTCPClient.h>
       #include <ModbusTCPServer.h>

4. If ArduinoModbus is replaced/updated in the future and no longer forces
   compilation of the RS485 backend on ESP32, remove this workaround.
5. If real Modbus RTU / RS485 support is introduced in the application,
   this workaround must be reviewed and a proper ESP32 RS485 configuration
   must be implemented instead of simply disabling the global object.

-------------------------------------------------------------------------------
REFERENCE
-------------------------------------------------------------------------------

OPTA:
    ArduinoModbus + ArduinoRS485 compile normally.
    Do not alter the OPTA behaviour.

ESP32-S3-ETH Waveshare:
    W5500 Ethernet is used for the network path.
    RS485 is currently NOT used by DomoManager.

This is a temporary library compatibility workaround for the ESP32 port.
===============================================================================
*/


// ============================================================
// PLATFORM NETWORK BACKEND
// ============================================================
//
// Il progetto usa un unico tipo TCP astratto:
//
//     DMPlatform::Network::TCPClient
//     DMPlatform::Network::TCPServer
//
// Attualmente:
//
// OPTA / MBED
//     EthernetClient
//     EthernetServer
//
// ESP32-S3-ETH / W5500
//     EthernetClient
//     EthernetServer
//
// Il backend fisico viene inizializzato in BeginEthernet().
//
// ============================================================

#if defined(ARDUINO_ARCH_MBED)

    #include <Ethernet.h>

#elif defined(ARDUINO_ARCH_ESP32)

    #include <SPI.h>
    #include <Ethernet.h>

#else

    #error "Unsupported DomoManager platform for DMPlatformNetwork"

#endif


namespace DMPlatform
{
namespace Network
{

    // ========================================================
    // TCP TYPES
    // ========================================================

    using TCPClient =
        EthernetClient;


    using TCPServer =
        EthernetServer;


    // ========================================================
    // ESP32-S3-ETH / W5500 HARDWARE
    // ========================================================
    //
    // Waveshare ESP32-S3-ETH:
    //
    // W5500
    //   MISO = GPIO12
    //   MOSI = GPIO11
    //   SCLK = GPIO13
    //   CS   = GPIO14
    //   RST  = GPIO9
    //   INT  = GPIO10
    //
    // ========================================================

#if defined(ARDUINO_ARCH_ESP32)

    static constexpr int W5500_MISO =
        12;

    static constexpr int W5500_MOSI =
        11;

    static constexpr int W5500_SCLK =
        13;

    static constexpr int W5500_CS =
        14;

    static constexpr int W5500_RST =
        9;

    static constexpr int W5500_INT =
        10;

#endif


    // ========================================================
    // ETHERNET INITIALIZATION
    // ========================================================
    //
    // Configura l'interfaccia Ethernet con IP statica.
    //
    // NON espone all'esterno dettagli W5500/OPTA.
    //
    // ========================================================

    inline bool BeginEthernet(
        const uint8_t mac[6],
        const IPAddress& ip,
        const IPAddress& gateway,
        const IPAddress& subnet)
    {
        if (!mac)
            return false;


#if defined(ARDUINO_ARCH_ESP32)

        // ====================================================
        // W5500 RESET
        // ====================================================

        pinMode(
            W5500_RST,
            OUTPUT
        );


        digitalWrite(
            W5500_RST,
            LOW
        );


        delay(10);


        digitalWrite(
            W5500_RST,
            HIGH
        );


        delay(100);


        // ====================================================
        // SPI
        // ====================================================
        //
        // ESP32-S3-ETH / Waveshare:
        //
        // SCLK 13
        // MISO 12
        // MOSI 11
        //
        // ====================================================

        SPI.begin(
            W5500_SCLK,
            W5500_MISO,
            W5500_MOSI,
            W5500_CS
        );


        // ====================================================
        // ETHERNET CHIP SELECT
        // ====================================================

        Ethernet.init(
            W5500_CS
        );

#endif


        // ====================================================
        // STATIC IP
        // ====================================================
        //
        // Manteniamo la stessa API Ethernet già usata
        // dal progetto OPTA.
        //
        // Nella libreria Ethernet corrente il W5500 è
        // supportato esplicitamente.
        // ====================================================

        Ethernet.begin(
            const_cast<uint8_t*>(mac),
            ip,
            gateway,
            subnet
        );


        // ====================================================
        // HARDWARE CHECK
        // ====================================================

        if (Ethernet.hardwareStatus() ==
            EthernetNoHardware)
        {
            return false;
        }


        return true;
    }


    // ========================================================
    // HARDWARE PRESENT
    // ========================================================

    inline bool HardwarePresent()
    {
        return
            Ethernet.hardwareStatus() !=
            EthernetNoHardware;
    }


    // ========================================================
    // LINK STATUS
    // ========================================================

    inline bool LinkUp()
    {
        return
            Ethernet.linkStatus() !=
            LinkOFF;
    }

    inline bool WaitForLink(
        unsigned long timeoutMs)
    {
    #if defined(ARDUINO_ARCH_ESP32)
        delay (100);
        const unsigned long start = millis();

        while (!LinkUp())
        {
            if ((millis() - start) >= timeoutMs)
            {
                return false;
            }

            delay(10);
        }

        return true;

    #elif defined(ARDUINO_ARCH_MBED)

        // Su OPTA manteniamo il comportamento originale:
        // nessuna attesa aggiuntiva.
        delay(100);
        return LinkUp();

    #else

        return LinkUp();

    #endif
    }

    // ========================================================
    // LOCAL IP
    // ========================================================

    inline IPAddress LocalIP()
    {
        return Ethernet.localIP();
    }


    // ========================================================
    // SUBNET
    // ========================================================

    inline IPAddress SubnetMask()
    {
        return Ethernet.subnetMask();
    }


    // ========================================================
    // GATEWAY
    // ========================================================

    inline IPAddress GatewayIP()
    {
        return Ethernet.gatewayIP();
    }


    // ========================================================
    // HARDWARE STATUS
    // ========================================================

    inline EthernetHardwareStatus HardwareStatus()
    {
        return Ethernet.hardwareStatus();
    }


    // ========================================================
    // LINK STATUS RAW
    // ========================================================

    inline EthernetLinkStatus LinkStatus()
    {
        return Ethernet.linkStatus();
    }


    // ========================================================
    // ETHERNET END
    // ========================================================
    //
    // OPTA:
    //     Ethernet.end()
    //
    // ESP32:
    //     Ethernet library utilizzata dal progetto.
    //
    // La funzione resta nel layer di astrazione.
    //
    // ========================================================

    inline void EndEthernet()
    {
    #if defined(ARDUINO_ARCH_ESP32)

        // ESP32 + W5500:
        // EthernetClass non espone Ethernet.end().
        // Nessuna operazione necessaria.

    #elif defined(ARDUINO_ARCH_MBED)

        Ethernet.end();

    #endif
    }


    // ========================================================
    // DHCP / MAINTENANCE
    // ========================================================
    //
    // Non utilizzata attualmente dal tuo SetupEthernet(),
    // ma lasciata qui come parte del backend generico.
    //
    // ========================================================

    inline int MaintainEthernet()
    {
        return Ethernet.maintain();
    }


    // ========================================================
    // MAC ADDRESS
    // ========================================================

    inline void GetMAC(
        uint8_t mac[6])
    {
        if (!mac)
            return;

        Ethernet.MACAddress(
            mac
        );
    }


    // ========================================================
    // SET MAC ADDRESS
    // ========================================================

    inline void SetMAC(
        const uint8_t mac[6])
    {
        if (!mac)
            return;

        Ethernet.setMACAddress(
            mac
        );
    }


    // ========================================================
    // SET LOCAL IP
    // ========================================================

    inline void SetLocalIP(
        const IPAddress& ip)
    {
        Ethernet.setLocalIP(
            ip
        );
    }


    // ========================================================
    // SET SUBNET
    // ========================================================

    inline void SetSubnetMask(
        const IPAddress& subnet)
    {
        Ethernet.setSubnetMask(
            subnet
        );
    }


    // ========================================================
    // SET GATEWAY
    // ========================================================

    inline void SetGatewayIP(
        const IPAddress& gateway)
    {
        Ethernet.setGatewayIP(
            gateway
        );
    }


    // ========================================================
    // RESET W5500
    // ========================================================
    //
    // Esplicitamente disponibile per eventuali recovery.
    //
    // OPTA:
    //     nessuna operazione.
    //
    // ESP32-S3-ETH:
    //     reset hardware W5500.
    //
    // ========================================================

    inline void ResetController()
    {

#if defined(ARDUINO_ARCH_ESP32)

        pinMode(
            W5500_RST,
            OUTPUT
        );


        digitalWrite(
            W5500_RST,
            LOW
        );


        delay(10);


        digitalWrite(
            W5500_RST,
            HIGH
        );


        delay(100);

#else

        // ----------------------------------------------------
        // OPTA:
        // reset gestito dall'hardware/libreria.
        // ----------------------------------------------------

#endif
    }


    // ========================================================
    // TCP SERVER FACTORY
    // ========================================================
    //
    // Questa funzione serve a mantenere tutta la costruzione
    // del listener nel layer platform.
    //
    // Nota:
    // EthernetServer richiede la porta nel costruttore.
    //
    // ========================================================

    inline TCPServer*
    CreateTCPServer(
        uint16_t port)
    {
        return new TCPServer(
            port
        );
    }


    // ========================================================
    // TCP SERVER DESTROY
    // ========================================================

    inline void DestroyTCPServer(
        TCPServer*& server)
    {
        if (!server)
            return;


        delete server;

        server = nullptr;
    }


    // ========================================================
    // TCP SERVER START
    // ========================================================

    inline bool StartTCPServer(
        TCPServer& server)
    {
        server.begin();

        return true;
    }


    // ========================================================
    // TCP SERVER ACCEPT
    // ========================================================

    inline TCPClient AcceptTCPClient(
        TCPServer& server)
    {
        return server.accept();
    }


    // ========================================================
    // TCP CLIENT VALID
    // ========================================================

    inline bool IsConnected(
        TCPClient& client)
    {
        return
            client.connected();
    }


    // ========================================================
    // TCP CLIENT STOP
    // ========================================================

    inline void Stop(
        TCPClient& client)
    {
        client.stop();
    }


} // namespace Network
} // namespace DMPlatform