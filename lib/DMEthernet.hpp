#pragma once

#include <Arduino.h>
#include <IPAddress.h>

using DMIPAddress = IPAddress;


// ============================================================
// PLATFORM
// ============================================================

#if defined(ARDUINO_ESP32S3) || defined(ESP32)

    #warning "ESP32 DETECTED"

    #include <SPI.h>
    #include <Ethernet.h>
    #include <EthernetUdp.h>

    using DMEthernetClient = EthernetClient;
    using DMEthernetUDP    = EthernetUDP;

    namespace DMEthernetConfig
    {
        constexpr int CS   = 14;
        constexpr int RST  = 9;
        constexpr int INT  = 10;   // attualmente non usato

        constexpr int MISO = 12;
        constexpr int MOSI = 11;
        constexpr int SCK  = 13;
    }


#elif defined(ARDUINO_OPTA) || defined(ARDUINO_ARCH_MBED)

    #warning "MBED / OPTA DETECTED"

    #include <Ethernet.h>
    #include <EthernetUdp.h>

    using DMEthernetClient = EthernetClient;
    using DMEthernetUDP    = EthernetUDP;


#else

    #error "DomoManager: unsupported Ethernet platform"

#endif


// ============================================================
// DMEthernet
// ============================================================

class DMEthernet
{
public:

    // --------------------------------------------------------
    // BEGIN
    // --------------------------------------------------------

    static bool begin(
        const uint8_t* mac,
        const IPAddress& ip,
        const IPAddress& gateway,
        const IPAddress& subnet)
    {

#if defined(ARDUINO_ARCH_ESP32)

        // ----------------------------------------------------
        // W5500 RESET
        // ----------------------------------------------------

        pinMode(DMEthernetConfig::RST, OUTPUT);

        digitalWrite(DMEthernetConfig::RST, LOW);
        delayMicroseconds(500);

        digitalWrite(DMEthernetConfig::RST, HIGH);
        delayMicroseconds(1000);


        // ----------------------------------------------------
        // SPI
        // ----------------------------------------------------

        SPI.begin(
            DMEthernetConfig::SCK,
            DMEthernetConfig::MISO,
            DMEthernetConfig::MOSI,
            DMEthernetConfig::CS
        );


        // ----------------------------------------------------
        // ETHERNET CS
        // ----------------------------------------------------

        Ethernet.init(DMEthernetConfig::CS);


        // ----------------------------------------------------
        // STATIC IP
        // ----------------------------------------------------

        Ethernet.begin(
            const_cast<uint8_t*>(mac),
            ip,
            gateway,
            gateway,
            subnet
        );

        return true;


#elif defined(ARDUINO_ARCH_MBED)

        Ethernet.begin(
            const_cast<uint8_t*>(mac),
            ip,
            gateway,
            subnet
        );

        return true;


#else

        return false;

#endif
    }


    // --------------------------------------------------------
    // END
    //
    // ATTENZIONE:
    // NON viene usato per chiudere una connessione Modbus.
    //
    // ModbusTCPClient::stop() deve gestire il socket TCP.
    //
    // Su ESP32/W5500 non facciamo Ethernet.end() perché
    // l'implementazione Ethernet utilizzata non lo espone.
    // NON facciamo nemmeno RESET del W5500.
    // --------------------------------------------------------

    static void end()
    {

#if defined(ARDUINO_ARCH_MBED)

        Ethernet.end();

#elif defined(ARDUINO_ARCH_ESP32)

        // Nessuna operazione.
        //
        // Il W5500 rimane inizializzato.
        // Le connessioni TCP vengono chiuse dai rispettivi
        // EthernetClient / ModbusTCPClient.

        return;

#endif
    }


    // --------------------------------------------------------
    // LOCAL IP
    // --------------------------------------------------------

    static IPAddress localIP()
    {
        return Ethernet.localIP();
    }


    // --------------------------------------------------------
    // GATEWAY
    // --------------------------------------------------------

    static IPAddress gatewayIP()
    {
        return Ethernet.gatewayIP();
    }


    // --------------------------------------------------------
    // SUBNET
    // --------------------------------------------------------

    static IPAddress subnetMask()
    {
        return Ethernet.subnetMask();
    }


    // --------------------------------------------------------
    // DNS
    // --------------------------------------------------------

    static IPAddress dnsServerIP()
    {
        return Ethernet.dnsServerIP();
    }


    // --------------------------------------------------------
    // LINK
    // --------------------------------------------------------

    static bool linkUp()
    {
        return Ethernet.linkStatus() == LinkON;
    }


    static EthernetLinkStatus linkStatus()
    {
        return Ethernet.linkStatus();
    }


    // --------------------------------------------------------
    // HARDWARE
    // --------------------------------------------------------

    static EthernetHardwareStatus hardwareStatus()
    {
        return Ethernet.hardwareStatus();
    }


    // --------------------------------------------------------
    // DNS / HOST BY NAME
    // --------------------------------------------------------
    //
    // L'implementazione Ethernet ESP32 che stai usando non
    // espone Ethernet.hostByName().
    //
    // Non usiamo WiFi.hostByName(), perché DomoManager lavora
    // con Ethernet/W5500 e non vogliamo introdurre una
    // dipendenza dal WiFi.
    //
    // Se DomoManager usa hostname invece di IP, questa funzione
    // dovrà essere implementata diversamente.
    // --------------------------------------------------------

    static int hostByName(
        const char* hostname,
        IPAddress& result)
    {

#if defined(ARDUINO_ARCH_MBED)

        return Ethernet.hostByName(
            hostname,
            result
        );

#elif defined(ARDUINO_ARCH_ESP32)

        (void)hostname;
        (void)result;

        return 0;

#else

        (void)hostname;
        (void)result;

        return 0;

#endif
    }


    static int hostByName(
        const String& hostname,
        IPAddress& result)
    {
        return hostByName(
            hostname.c_str(),
            result
        );
    }
};