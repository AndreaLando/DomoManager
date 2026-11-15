#pragma once

#include <Arduino.h>
#include <IPAddress.h>

using DMIPAddress = IPAddress;

// ============================================================
// NETWORK BACKEND SELECTION
// ============================================================

#define DM_NETWORK_BACKEND_ETHERNET 1
#define DM_NETWORK_BACKEND_WIFI     2

#ifndef DM_NETWORK_BACKEND
    #define DM_NETWORK_BACKEND \
        DM_NETWORK_BACKEND_ETHERNET
#endif

// ============================================================
// PLATFORM
// ============================================================

#if defined(ARDUINO_ESP32S3) || \
    defined(ESP32) || \
    defined(ARDUINO_ARCH_ESP32)

    #warning "ESP32 DETECTED"

    #if \
        DM_NETWORK_BACKEND == \
        DM_NETWORK_BACKEND_WIFI

        #include <WiFi.h>
        #include <WiFiUdp.h>

        using DMEthernetClient =
            WiFiClient;

        using DMEthernetServer =
            WiFiServer;

        using DMEthernetUDP =
            WiFiUDP;

    #elif \
        DM_NETWORK_BACKEND == \
        DM_NETWORK_BACKEND_ETHERNET

        #include <SPI.h>
        #include <Ethernet.h>
        #include <EthernetUdp.h>
        #include <Dns.h>

        using DMEthernetClient =
            EthernetClient;

        using DMEthernetServer =
            EthernetServer;

        using DMEthernetUDP =
            EthernetUDP;

    #else
        #error "DomoManager: unsupported network backend"
    #endif

    namespace DMEthernetConfig
    {
        constexpr int CS   = 14;
        constexpr int RST  = 9;
        constexpr int INT  = 10;

        constexpr int MISO = 12;
        constexpr int MOSI = 11;
        constexpr int SCK  = 13;
    }


#elif defined(ARDUINO_OPTA) || defined(ARDUINO_ARCH_MBED)

    #warning "MBED / OPTA DETECTED"

    #include <Ethernet.h>
    #include <EthernetUdp.h>

    using DMEthernetClient = EthernetClient;
    using DMEthernetServer = EthernetServer;
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
    // ========================================================
    // BACKEND
    // ========================================================

    enum class Backend : uint8_t
    {
        Ethernet,
        WiFi
    };

    // ========================================================
    // GENERIC HARDWARE STATUS
    // ========================================================

    enum class HardwareState : uint8_t
    {
        Unknown,
        Unavailable,
        Available
    };

    // ========================================================
    // ACTIVE BACKEND
    // ========================================================

    static constexpr Backend backend()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return Backend::WiFi;

#else

        return Backend::Ethernet;

#endif
    }

    // ========================================================
    // BACKEND NAME
    // ========================================================

    static const char* backendName()
    {
        return
            backend() ==
                    Backend::WiFi
                ? "WiFi"
                : "Ethernet";
    }

    // ========================================================
    // BEGIN ETHERNET
    //
    // Mantiene la firma originale.
    //
    // Sul backend Wi-Fi restituisce false perché MAC e
    // configurazione W5500 non sono applicabili.
    // ========================================================

    static bool begin(
        const uint8_t* mac,
        const IPAddress& ip,
        const IPAddress& gateway,
        const IPAddress& subnet)
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_ETHERNET

        if (!mac)
        {
            return false;
        }

    #if defined(ARDUINO_ARCH_ESP32)

        // ====================================================
        // W5500 RESET
        // ====================================================

        pinMode(
            DMEthernetConfig::RST,
            OUTPUT
        );

        digitalWrite(
            DMEthernetConfig::RST,
            LOW
        );

        delayMicroseconds(
            500
        );

        digitalWrite(
            DMEthernetConfig::RST,
            HIGH
        );

        delayMicroseconds(
            1000
        );

        // ====================================================
        // SPI
        // ====================================================

        SPI.begin(
            DMEthernetConfig::SCK,
            DMEthernetConfig::MISO,
            DMEthernetConfig::MOSI,
            DMEthernetConfig::CS
        );

        // ====================================================
        // W5500 CHIP SELECT
        // ====================================================

        Ethernet.init(
            DMEthernetConfig::CS
        );

        // ====================================================
        // STATIC IP
        //
        // Firma Ethernet ESP32:
        //
        // mac, ip, dns, gateway, subnet
        //
        // In assenza di un DNS specifico viene mantenuto il
        // comportamento originale usando il gateway come DNS.
        // ====================================================

        Ethernet.begin(
            const_cast<uint8_t*>(
                mac
            ),
            ip,
            gateway,
            gateway,
            subnet
        );

        return
            Ethernet.hardwareStatus() !=
            EthernetNoHardware;

    #elif defined(ARDUINO_ARCH_MBED)

        Ethernet.begin(
            const_cast<uint8_t*>(
                mac
            ),
            ip,
            gateway,
            subnet
        );

        return true;

    #else

        (void)ip;
        (void)gateway;
        (void)subnet;

        return false;

    #endif

#else

        /*
         * Firma Ethernet richiamata mentre lo standard è
         * compilato con backend Wi-Fi.
         */

        (void)mac;
        (void)ip;
        (void)gateway;
        (void)subnet;

        return false;

#endif
    }

    // ========================================================
    // BEGIN ETHERNET WITH DNS
    //
    // Overload opzionale per evitare di usare sempre il
    // gateway come server DNS.
    // ========================================================

    static bool begin(
        const uint8_t* mac,
        const IPAddress& ip,
        const IPAddress& dns,
        const IPAddress& gateway,
        const IPAddress& subnet)
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_ETHERNET

        if (!mac)
        {
            return false;
        }

    #if defined(ARDUINO_ARCH_ESP32)

        pinMode(
            DMEthernetConfig::RST,
            OUTPUT
        );

        digitalWrite(
            DMEthernetConfig::RST,
            LOW
        );

        delayMicroseconds(
            500
        );

        digitalWrite(
            DMEthernetConfig::RST,
            HIGH
        );

        delayMicroseconds(
            1000
        );

        SPI.begin(
            DMEthernetConfig::SCK,
            DMEthernetConfig::MISO,
            DMEthernetConfig::MOSI,
            DMEthernetConfig::CS
        );

        Ethernet.init(
            DMEthernetConfig::CS
        );

        Ethernet.begin(
            const_cast<uint8_t*>(
                mac
            ),
            ip,
            dns,
            gateway,
            subnet
        );

        return
            Ethernet.hardwareStatus() !=
            EthernetNoHardware;

    #elif defined(ARDUINO_ARCH_MBED)

        /*
         * La firma Ethernet disponibile su Opta non usa
         * necessariamente il parametro DNS separato.
         */

        (void)dns;

        Ethernet.begin(
            const_cast<uint8_t*>(
                mac
            ),
            ip,
            gateway,
            subnet
        );

        return true;

    #else

        (void)ip;
        (void)dns;
        (void)gateway;
        (void)subnet;

        return false;

    #endif

#else

        (void)mac;
        (void)ip;
        (void)dns;
        (void)gateway;
        (void)subnet;

        return false;

#endif
    }

    // ========================================================
    // BEGIN WI-FI
    //
    // Il timeout non blocca indefinitamente il firmware.
    //
    // Se timeoutMs == 0:
    //     avvia la connessione e ritorna immediatamente.
    //
    // Se timeoutMs > 0:
    //     attende fino alla connessione o alla scadenza.
    // ========================================================

    static bool begin(
        const char* ssid,
        const char* password,
        unsigned long timeoutMs = 0)
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        if (
            !ssid ||
            ssid[0] == '\0'
        )
        {
            return false;
        }

        WiFi.mode(
            WIFI_STA
        );

        WiFi.setAutoReconnect(
            true
        );

        WiFi.begin(
            ssid,
            password
                ? password
                : ""
        );

        if (timeoutMs == 0)
        {
            /*
             * Avvio asincrono.
             *
             * La connessione viene controllata dal loop
             * applicativo.
             */

            return true;
        }

        const unsigned long startedAt =
            millis();

        while (
            WiFi.status() !=
            WL_CONNECTED
        )
        {
            if (
                static_cast<unsigned long>(
                    millis() -
                    startedAt
                ) >=
                timeoutMs
            )
            {
                return false;
            }

            delay(
                10
            );
        }

        return true;

#else

        (void)ssid;
        (void)password;
        (void)timeoutMs;

        return false;

#endif
    }

    // ========================================================
    // BEGIN WI-FI WITH STATIC IP
    // ========================================================

    static bool begin(
        const char* ssid,
        const char* password,
        const IPAddress& ip,
        const IPAddress& gateway,
        const IPAddress& subnet,
        const IPAddress& dns1 =
            IPAddress(0, 0, 0, 0),
        const IPAddress& dns2 =
            IPAddress(0, 0, 0, 0),
        unsigned long timeoutMs = 0)
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        if (
            !ssid ||
            ssid[0] == '\0'
        )
        {
            return false;
        }

        WiFi.mode(
            WIFI_STA
        );

        WiFi.setAutoReconnect(
            true
        );

        const bool configured =
            WiFi.config(
                ip,
                gateway,
                subnet,
                dns1,
                dns2
            );

        if (!configured)
        {
            return false;
        }

        WiFi.begin(
            ssid,
            password
                ? password
                : ""
        );

        if (timeoutMs == 0)
        {
            return true;
        }

        const unsigned long startedAt =
            millis();

        while (
            WiFi.status() !=
            WL_CONNECTED
        )
        {
            if (
                static_cast<unsigned long>(
                    millis() -
                    startedAt
                ) >=
                timeoutMs
            )
            {
                return false;
            }

            delay(
                10
            );
        }

        return true;

#else

        (void)ssid;
        (void)password;
        (void)ip;
        (void)gateway;
        (void)subnet;
        (void)dns1;
        (void)dns2;
        (void)timeoutMs;

        return false;

#endif
    }

    // ========================================================
    // END
    // ========================================================

    static void end()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        WiFi.disconnect(
            true
        );

        WiFi.mode(
            WIFI_OFF
        );

#elif \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_ETHERNET

    #if defined(ARDUINO_ARCH_MBED)

        Ethernet.end();

    #elif defined(ARDUINO_ARCH_ESP32)

        /*
         * ESP32 + W5500:
         *
         * - non viene eseguito il reset hardware;
         * - non viene terminato il bus SPI;
         * - i singoli client e server gestiscono le proprie
         *   connessioni.
         */

        return;

    #endif

#endif
    }

    // ========================================================
    // LOCAL IP
    // ========================================================

    static IPAddress localIP()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            WiFi.localIP();

#else

        return
            Ethernet.localIP();

#endif
    }

    // ========================================================
    // GATEWAY
    // ========================================================

    static IPAddress gatewayIP()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            WiFi.gatewayIP();

#else

        return
            Ethernet.gatewayIP();

#endif
    }

    // ========================================================
    // SUBNET
    // ========================================================

    static IPAddress subnetMask()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            WiFi.subnetMask();

#else

        return
            Ethernet.subnetMask();

#endif
    }

    // ========================================================
    // DNS
    // ========================================================

    static IPAddress dnsServerIP(
        uint8_t index = 0)
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            WiFi.dnsIP(
                index
            );

#else

        (void)index;

        return
            Ethernet.dnsServerIP();

#endif
    }

    // ========================================================
    // LINK
    // ========================================================

    static bool linkUp()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            WiFi.status() ==
            WL_CONNECTED;

#else

        return
            Ethernet.linkStatus() ==
            LinkON;

#endif
    }

    // ========================================================
    // CONNECTING
    // ========================================================

    static bool connecting()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        const wl_status_t status =
            WiFi.status();

        return
            status ==
                WL_IDLE_STATUS ||
            status ==
                WL_DISCONNECTED;

#else

        /*
         * Ethernet non ha un equivalente diretto dello stato
         * associativo Wi-Fi.
         */

        return false;

#endif
    }

    // ========================================================
    // HARDWARE PRESENT
    // ========================================================

    static bool hardwarePresent()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        /*
         * Il controller radio fa parte dell'ESP32.
         * Il backend è disponibile quando la modalità non è
         * completamente disabilitata.
         */

        return
            WiFi.getMode() !=
            WIFI_MODE_NULL;

#else

        return
            Ethernet.hardwareStatus() !=
            EthernetNoHardware;

#endif
    }

    // ========================================================
    // GENERIC HARDWARE STATE
    // ========================================================

    static HardwareState hardwareState()
    {
        return
            hardwarePresent()
                ? HardwareState::Available
                : HardwareState::Unavailable;
    }

    // ========================================================
    // RAW STATUS
    //
    // Valore diagnostico portabile:
    //
    // Wi-Fi:
    //     wl_status_t convertito a int.
    //
    // Ethernet:
    //     EthernetLinkStatus convertito a int.
    // ========================================================

    static int linkStatusRaw()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            static_cast<int>(
                WiFi.status()
            );

#else

        return
            static_cast<int>(
                Ethernet.linkStatus()
            );

#endif
    }

    // ========================================================
    // RAW HARDWARE STATUS
    // ========================================================

    static int hardwareStatusRaw()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            hardwarePresent()
                ? 1
                : 0;

#else

        return
            static_cast<int>(
                Ethernet.hardwareStatus()
            );

#endif
    }

    // ========================================================
    // BACKWARD-COMPATIBLE ETHERNET STATUS
    //
    // Disponibili soltanto nel backend Ethernet per mantenere
    // il tipo originale senza introdurre tipi Ethernet nel
    // profilo Wi-Fi.
    // ========================================================

#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_ETHERNET

    static EthernetLinkStatus linkStatus()
    {
        return
            Ethernet.linkStatus();
    }

    static EthernetHardwareStatus hardwareStatus()
    {
        return
            Ethernet.hardwareStatus();
    }

#endif

    // ========================================================
    // RSSI
    //
    // Ethernet non fornisce RSSI.
    // ========================================================

    static int32_t RSSI()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        if (
            WiFi.status() !=
            WL_CONNECTED
        )
        {
            return 0;
        }

        return
            WiFi.RSSI();

#else

        return 0;

#endif
    }

    // ========================================================
    // SSID
    // ========================================================

    static String SSID()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            WiFi.SSID();

#else

        return
            String();
#endif
    }

    // ========================================================
    // MAC ADDRESS
    // ========================================================

    static String macAddress()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            WiFi.macAddress();

#else

        uint8_t mac[6] {};

        Ethernet.MACAddress(
            mac
        );

        char buffer[18] {};

        snprintf(
            buffer,
            sizeof(buffer),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0],
            mac[1],
            mac[2],
            mac[3],
            mac[4],
            mac[5]
        );

        return
            String(
                buffer
            );

#endif
    }

    // ========================================================
    // DNS / HOST BY NAME
    // ========================================================

    static int hostByName(
        const char* hostname,
        IPAddress& result)
    {
        result =
            IPAddress(
                0,
                0,
                0,
                0
            );

        if (
            !hostname ||
            hostname[0] == '\0'
        )
        {
            return 0;
        }

#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            WiFi.hostByName(
                hostname,
                result
            );

#elif defined(ARDUINO_ARCH_MBED)

        return
            Ethernet.hostByName(
                hostname,
                result
            );

#elif defined(ARDUINO_ARCH_ESP32)

        /*
         * L'implementazione Ethernet/W5500 usata dal progetto
         * non espone Ethernet.hostByName().
         *
         * Viene quindi utilizzato DNSClient.
         */

        const IPAddress dns =
            Ethernet.dnsServerIP();

        if (
            dns ==
            IPAddress(
                0,
                0,
                0,
                0
            )
        )
        {
            return 0;
        }

        DNSClient dnsClient;

        dnsClient.begin(
            dns
        );

        return
            dnsClient.getHostByName(
                hostname,
                result
            );

#else

        (void)hostname;

        return 0;

#endif
    }

    // ========================================================
    // MAINTAIN
    //
    // Ethernet:
    //     DHCP maintenance.
    //
    // Wi-Fi:
    //     nessuna manutenzione esplicita; auto reconnect è
    //     configurato in begin().
    // ========================================================

    static int maintain()
    {
#if \
    DM_NETWORK_BACKEND == \
    DM_NETWORK_BACKEND_WIFI

        return
            WiFi.status() ==
                    WL_CONNECTED
                ? 1
                : 0;

#else

        return
            Ethernet.maintain();

#endif
    }

    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    static void printDiagnostics()
    {
        const String local =
            localIP()
                .toString();

        const String gateway =
            gatewayIP()
                .toString();

        const String subnet =
            subnetMask()
                .toString();

        const String dns =
            dnsServerIP()
                .toString();

        char line[192] {};

        // ========================================================
        // BACKEND / LINK / HARDWARE
        // ========================================================

        snprintf(
            line,
            sizeof(line),
            "[DM::NET] backend=%s link=%u hardware=%u",
            backendName(),
            linkUp()
                ? 1U
                : 0U,
            hardwarePresent()
                ? 1U
                : 0U
        );

        line[
            sizeof(line) -
            1U
        ] = '\0';

        Serial.println(
            line
        );

        // ========================================================
        // NETWORK ADDRESSES
        // ========================================================

        snprintf(
            line,
            sizeof(line),
            "[DM::NET] IP=%s gateway=%s subnet=%s DNS=%s",
            local.c_str(),
            gateway.c_str(),
            subnet.c_str(),
            dns.c_str()
        );

        line[
            sizeof(line) -
            1U
        ] = '\0';

        Serial.println(
            line
        );

        // ========================================================
        // WI-FI DIAGNOSTICS
        //
        // Compilato esclusivamente quando il backend attivo
        // espone SSID, RSSI e MAC Wi-Fi.
        // ========================================================

        #if \
            DM_NETWORK_BACKEND == \
            DM_NETWORK_BACKEND_WIFI

            const String ssid =
                SSID();

            const String mac =
                macAddress();

            snprintf(
                line,
                sizeof(line),
                "[DM::NET] SSID=%s RSSI=%ld MAC=%s",
                ssid.c_str(),
                static_cast<long>(
                    RSSI()
                ),
                mac.c_str()
            );

            line[
                sizeof(line) -
                1U
            ] = '\0';

            Serial.println(
                line
            );

        #endif
    }
};