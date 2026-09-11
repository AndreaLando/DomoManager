#pragma once

#include <Arduino.h>
#include <IPAddress.h>

#if defined(ARDUINO_ARCH_MBED)

    // ============================================================
    // ARDUINO OPTA / MBED
    // ============================================================

    #include <Ethernet.h>
    #include <EthernetUdp.h>

    using DMEthernetClient = EthernetClient;
    using DMEthernetUDP    = EthernetUDP;


#elif defined(ARDUINO_ARCH_ESP32)

    // ============================================================
    // ESP32-S3 + W5500
    // ============================================================

    #include <SPI.h>
    #include <Ethernet.h>
    #include <EthernetUdp.h>

    using DMEthernetClient = EthernetClient;
    using DMEthernetUDP    = EthernetUDP;

    namespace DMEthernetConfig
    {
        // W5500 - Waveshare ESP32-S3-ETH
        constexpr int CS   = 14;
        constexpr int RST  = 9;
        constexpr int INT  = 10;   // attualmente non usato
        constexpr int MISO = 12;
        constexpr int MOSI = 11;
        constexpr int SCK  = 13;
    }

#else

    #error "DomoManager: unsupported Ethernet platform"

#endif


// ============================================================================
// DMEthernet
// ============================================================================

class DMEthernet
{
public:

    // ========================================================================
    // BEGIN
    // ========================================================================

    static bool begin(
        const uint8_t* mac,
        const IPAddress& ip,
        const IPAddress& gateway,
        const IPAddress& subnet)
    {
#if defined(ARDUINO_ARCH_MBED)

        Ethernet.begin(
            const_cast<uint8_t*>(mac),
            ip,
            gateway,
            subnet
        );

        return true;


#elif defined(ARDUINO_ARCH_ESP32)

        // ------------------------------------------------------------
        // RESET W5500
        // ------------------------------------------------------------

        pinMode(
            DMEthernetConfig::RST,
            OUTPUT
        );

        digitalWrite(
            DMEthernetConfig::RST,
            LOW
        );

        delayMicroseconds(500);

        digitalWrite(
            DMEthernetConfig::RST,
            HIGH
        );

        delayMicroseconds(1000);


        // ------------------------------------------------------------
        // SPI
        // ------------------------------------------------------------

        SPI.begin(
            DMEthernetConfig::SCK,
            DMEthernetConfig::MISO,
            DMEthernetConfig::MOSI,
            DMEthernetConfig::CS
        );


        // ------------------------------------------------------------
        // CHIP SELECT
        // ------------------------------------------------------------

        Ethernet.init(
            DMEthernetConfig::CS
        );


        // ------------------------------------------------------------
        // STATIC IP
        //
        // Per mantenere la stessa API a 4 parametri usiamo
        // il gateway anche come DNS.
        // ------------------------------------------------------------

        Ethernet.begin(
            const_cast<uint8_t*>(mac),
            ip,
            gateway,
            gateway,
            subnet
        );

        return true;

#else

        return false;

#endif
    }


    // ========================================================================
    // END
    // ========================================================================

    static void end()
    {
#if defined(ARDUINO_ARCH_MBED)

        Ethernet.end();

#elif defined(ARDUINO_ARCH_ESP32)

        Ethernet.end();

        pinMode(
            DMEthernetConfig::RST,
            OUTPUT
        );

        digitalWrite(
            DMEthernetConfig::RST,
            LOW
        );

#endif
    }


    // ========================================================================
    // IP
    // ========================================================================

    static IPAddress localIP()
    {
        return Ethernet.localIP();
    }

    static IPAddress gatewayIP()
    {
        return Ethernet.gatewayIP();
    }

    static IPAddress subnetMask()
    {
        return Ethernet.subnetMask();
    }

    static IPAddress dnsServerIP()
    {
        return Ethernet.dnsServerIP();
    }


    // ========================================================================
    // LINK
    // ========================================================================

    static bool linkUp()
    {
        return Ethernet.linkStatus() == LinkON;
    }

    static EthernetLinkStatus linkStatus()
    {
        return Ethernet.linkStatus();
    }


    // ========================================================================
    // HARDWARE
    // ========================================================================

    static EthernetHardwareStatus hardwareStatus()
    {
        return Ethernet.hardwareStatus();
    }


    // ========================================================================
    // DNS
    // ========================================================================

    static int hostByName(
        const char* hostname,
        IPAddress& result)
    {
        return Ethernet.hostByName(
            hostname,
            result
        );
    }

    static int hostByName(
        const String& hostname,
        IPAddress& result)
    {
        return Ethernet.hostByName(
            hostname.c_str(),
            result
        );
    }
};