#ifndef DMDeclares_H
#define DMDeclares_H

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
   

#include "DMAEE.hpp"
#include "DMWebAPI.hpp"
#include "DMAccessControl.hpp"

#include "DMHVAC.h"
#include "DMWeather.hpp"
#include "DMWiredSensors.hpp"

struct DiagnosticConfig  {
    bool reportAutomationEngine     = true;
    bool reportScheduler            = true;
    bool reportSplitAnalysis        = true;
    bool reportMissingBufferDefs    = true;
    bool reportDeviceAreas          = true;
    bool reportIneffectiveRules     = true;
    bool reportDeviceErrors         = true;
    bool reportWatchdogs            = true;
    bool reportDeviceProfiles       = true;
    bool reportRTC                  = true;
    bool reportHotStandby           = true;
    bool reportVirtualAreas         = false;
    bool reportNeverInitialized     = false;
    bool reportMultipleInitialized  = true;
    bool reportAutomationConfig     = true;
    bool reportLogBuffer            = true;
    bool reportModbusTiming         = true;
};

// ************ DEFINIZIONE STRUTTURA DomoManager *******************************

class DomoManagerBufferEngine {
public:

    struct AreaFlags {
        bool reverse;        // negazione logica
    };

    struct ThresholdConfig {
        int low;
        int high;
    };

    struct AreaConfig {
        int area;
        int forwardArea;
        String label;
        AreaFlags flags;
        ThresholdConfig threshold;
    };

    struct AreasConfig {
        std::vector<AreaConfig> list;
    };
};

class DomoManagerToggleEngine {
public:

    struct ToggleConfig {
        int areaRead;
        std::vector<int> forwards;
    };

    struct TogglesConfig {
        std::vector<ToggleConfig> list;
    };

    //static void apply(DomoManager& manager, const TogglesConfig& cfg);
};


class DomoManagerSplitEngine {
public:

    struct SplitConfig {
        int mainArea;
        std::vector<int> outAreas;
        unsigned long maxTime;
    };

    struct SplitsConfig {
        std::vector<SplitConfig> list;
    };

    //static void apply(DomoManager& manager, const SplitsConfig& cfg);
};

class DomoManagerRouteEngine {
public:

    struct RoutesConfig {
        std::vector<RouteManager::RouteConfig> list;
    };

    // ============================================================
    // APPLICA LA CONFIGURAZIONE AL PLC
    // ============================================================
    static void apply(RouteManager& plcRouteManager,
                      const RoutesConfig& cfg)
    {
        // delega completamente al PLC
        plcRouteManager.load(cfg.list);
    }
};

struct DomoManagerConfig {
    struct HMI {
        bool enabled = false;
        uint16_t port = 502;
        uint16_t pollingMs = 500;
        int maxClients=1;
    } hmi;

    // --- MODBUS TCP to RTU ---
    struct Modbus {
        bool enabled = true;
        uint16_t timeoutMs = 200; //200ms non scendere mai sotto questo valore 15.05.2026
        /**
        * @brief Abilita il report diagnostico a ogni evento Power.
        */
        bool reporting = false;
    } modbus;

    Watchdog::Params watchdog;

    struct Devices {
        struct Device {
            String name;
            IPAddress  address;
            uint8_t slot;
            String profile;
            std::vector<int> areas;
            int retry;
            Priority priority;
            const char* suggestedArea = nullptr;
            const char* installPlace = nullptr;
        };

        std::vector<Device> list;
    } devices;

    // ========================================================================
    // LOCAL IO
    //
    // Gli I/O fisici vengono gestiti da DomoManager.
    //
    // INPUT:
    //   pin        = pin fisico o identificatore DMPlatform
    //   area       = area associata all'evento
    //   invert     = inversione logica dell'ingresso
    //   debounceMs = tempo di stabilizzazione
    //
    // OUTPUT:
    //   pin        = pin fisico o identificatore DMPlatform
    //   area       = area associata al comando
    //
    // La scansione viene effettuata da DomoManager::Update().
    // Gli eventi transitano attraverso l'EventManager centrale.
    // ========================================================================

    struct LocalIO
    {
        uint16_t pollingMs = 30;

        // ========================================================================
        // INPUT
        // ========================================================================

        struct Input
        {
            int pin = -1;
            int area = -1;
            uint16_t debounceMs = 10;

            Input(
                int p = -1,
                int a = -1,
                uint16_t debounce = 10)
                : pin(p),
                area(a),
                debounceMs(debounce)
            {
            }
        };

        // ========================================================================
        // OUTPUT
        // ========================================================================

        struct Output
        {
            int pin = -1;
            int area = -1;

            Output(
                int p = -1,
                int a = -1)
                : pin(p),
                area(a)
            {
            }
        };

        std::vector<Input> inputs;
        std::vector<Output> outputs;

    } localIO;

    DomoManagerRouteEngine::RoutesConfig routes;
    DomoManagerBufferEngine::AreasConfig  areas;
    DomoManagerToggleEngine::TogglesConfig toggles;
    DomoManagerSplitEngine::SplitsConfig splits;

    struct Automation {
        const char* json = nullptr;
    } automation;

};

// ************ DEFINIZIONE STRUTTURA FRONTEND*******************************
struct NetworkProtocolConfig {
    const char* name;

    unsigned long minInterval = 0;
    unsigned long maxDuration = 0;

    bool pacingEnabled = true;

    unsigned long slotDuration = 0;
};

// ============================================================================
// CONFIGURAZIONE MISURA CONSUMO CARICO
// ============================================================================

/**
 * @brief Configurazione opzionale della misura associata a un carico.
 *
 * La sorgente viene letta attraverso GenericSensor.
 * Il valore viene successivamente normalizzato in watt.
 */
struct LoadPowerSensorConfig
{
    /**
     * @brief Tipo fisico della misura fornita dal GenericSensor.
     */
    enum class MeasureType
    {
        /**
         * Nessuna misura configurata.
         *
         * Il sistema utilizza la potenza nominale quando necessario.
         */
        NONE,

        /**
         * Il valore acquisito rappresenta direttamente una potenza.
         */
        WATT,

        /**
         * Il valore acquisito rappresenta una corrente.
         *
         * La potenza viene calcolata come:
         *
         * watt = ampere * voltage * powerFactor
         */
        AMPERE,

        /**
         * Il valore acquisito rappresenta solamente lo stato ON/OFF.
         *
         * Se il valore supera onThreshold, la potenza viene stimata
         * utilizzando la potenza nominale del carico.
         */
        ON_OFF
    };

    /**
     * @brief Tipo fisico della misura.
     *
     * NONE disabilita la lettura.
     */
    MeasureType type =
        MeasureType::NONE;

    /**
     * @brief Configurazione del GenericSensor.
     */
    GenericSensor::Config sensor{};

    /**
     * @brief Fattore applicato al valore restituito da GenericSensor.
     *
     * Esempi:
     *
     * 1.0f   valore intero;
     * 0.1f   valore espresso in decimi;
     * 0.01f  valore espresso in centesimi;
     * 0.001f valore espresso in millesimi.
     */
    float scale = 1.0f;

    /**
     * @brief Tensione usata per convertire ampere in watt.
     *
     * Utilizzata solamente con MeasureType::AMPERE.
     */
    float voltage = 230.0f;

    /**
     * @brief Fattore di potenza.
     *
     * Utilizzato solamente con MeasureType::AMPERE.
     * Il valore valido è compreso tra 0.0 e 1.0.
     */
    float powerFactor = 1.0f;

    /**
     * @brief Soglia oltre la quale il carico viene considerato acceso.
     *
     * Significato:
     * - WATT: soglia espressa in watt;
     * - AMPERE: soglia espressa in ampere;
     * - ON_OFF: soglia sul valore logico acquisito.
     */
    float onThreshold = 0.5f;

    /**
     * @brief Indica se è configurata una misura.
     */
    bool enabled() const
    {
        return
            type != MeasureType::NONE;
    }
};

struct FrontendConfig {
    struct Pins {
        int userButton;
        LedController::LedPins leds;   // <--- AGGIUNTO
    } pins;

    // -----------------------------
    // 1) CONFIG RETE
    // -----------------------------Fsetup
    struct Net {
        std::array<byte,6> mac;
        IPAddress ip;
        IPAddress gateway;
        IPAddress subnet;
    } net;

    // -----------------------------
    // 2) CONFIG Bridge
    // -----------------------------
    struct Bridge {
        bool enabled = false;
        
        IPAddress ip;
        uint16_t localPort = 0;
        uint16_t remotePort = 0;

        struct AEE {
            const AEEVarDef* vars = nullptr;
            size_t count = 0;
        } aee;

    } bridge;

    // -----------------------------
    // 4) CONFIG MQTT (nuovo)
    // -----------------------------
    struct MQTT
    {
        bool enabled = false;

        // ============================================================
        // Device specific String <-> Value enum
        // ============================================================

        struct EnumValue
        {
            const char* text;
            int value;
        };

        struct EnumList
        {
            const EnumValue* data = nullptr;
            size_t count = 0;

            constexpr EnumList() = default;

            template <size_t N>
            constexpr EnumList(const EnumValue (&array)[N])
                : data(array),
                count(N)
            {
            }

            constexpr bool empty() const
            {
                return data == nullptr || count == 0;
            }
        };
        
        // ============================================================
        // Mapping
        // ============================================================

        struct Mapping
        {
            const char* field = nullptr;

            enum class Direction
            {
                READ,
                WRITE,
                READ_WRITE
            };

            Direction direction = Direction::READ;

            enum class DataType
            {
                BOOL,
                INT,
                FLOAT,
                ENUM,

                STRING,
                JSON
            };

            DataType type = DataType::INT;

            // Area Buffer DomoManager
            int area = -1;

            // Scaling
            float scale = 1.0f;

            // Tabella conversione specifica del dispositivo
            EnumList enums{};
        };

        // ============================================================
        // Device
        // ============================================================

        struct Device
        {
            static constexpr size_t MAX_MAPPINGS = 16;

            const char* id = nullptr;
            const char* name = nullptr;

            Mapping mappings[MAX_MAPPINGS]{};
        };

         struct Client
        {
            bool enabled = false;
            IPAddress broker;
            uint16_t port = 1883;

            enum class Backend
            {
                GENERIC,
                HOME_ASSISTANT,
                ZIGBEE2MQTT,
                SHELLY
            };

            Backend backend = Backend::GENERIC;

            const char* name = nullptr;
            const char* topicPrefix = nullptr;

            const Device* devices = nullptr;
            size_t deviceCount = 0;
        };

        const Client* clients = nullptr;
        size_t clientCount = 0;
    } mqtt;

    DomoManagerConfig domoManager;

    struct HVAC : public DMHVAC::HVACConfig {
        // Se vuoi aggiungere campi specifici del frontend, li metti qui.
        // Altrimenti lasci vuoto.
    } hvac;

    struct Weather {
        bool enabled = false;
        uint32_t intervalMs = 5000;
        // --- Parametri condivisi ---
        WeatherStation::Config config;

        // --- Parametri aggiuntivi del frontend ---
        bool autoCloseWindows;
    } weather;


    // ============================================================================
    // POWER MANAGEMENT
    // ============================================================================

    struct Power
    {
        /**
        * @brief Abilita il sottosistema PowerManager.
        */
        bool enabled = false;

        /**
        * @brief Abilita il report diagnostico a ogni evento Power.
        */
        bool reporting = false;

        /**
        * @brief Intervallo di esecuzione del PowerManager.
        */
        uint32_t intervalMs = 15000UL;

        /**
        * @brief Limite operativo di prelievo.
        */
        float limitSoft = 3000.0f;

        /**
        * @brief Limite assoluto di protezione.
        */
        float limitHard = 3500.0f;

        /**
        * @brief Abilita l'autotuning.
        */
        bool autoTune = false;

        /**
        * @brief Intervallo di esecuzione dell'autotuning.
        */
        uint32_t tuneIntervalMs =
            2UL * 60UL * 1000UL;

        /**
        * @brief Area buffer destinata alla word di stato distacco carichi.
        *
        * Layout per ciascun carico:
        *
        * bit index * 2     = richiesta di distacco pendente;
        * bit index * 2 + 1 = distacco confermato e attivo.
        *
        * Un valore negativo disabilita la pubblicazione.
        */
        int loadShedStatusArea = -1;

        // ========================================================================
        // CARICO ELETTRICO
        // ========================================================================

        struct Load
        {
            /**
            * @brief Nome univoco del carico.
            */
            const char* name = nullptr;

            /**
            * @brief Priorità del carico.
            *
            * Un valore maggiore indica una priorità maggiore.
            */
            int priority = 0;

            /**
            * @brief Potenza nominale del carico.
            *
            * Viene utilizzata:
            * - per prevedere l'assorbimento prima dell'accensione;
            * - come fallback quando la misura non è disponibile;
            * - come fallback quando la misura è solamente ON/OFF.
            */
            float watt = 0.0f;

            /**
            * @brief Tempo minimo ON espresso in secondi.
            */
            unsigned long minOn = 5UL;

            /**
            * @brief Tempo minimo OFF espresso in secondi.
            */
            unsigned long minOff = 5UL;

            /**
            * @brief Configurazione opzionale della misura reale.
            */
            LoadPowerSensorConfig measurement{};
        };

        /**
        * @brief Numero massimo di carichi configurabili.
        */
        static constexpr size_t MAX_LOADS = 8U;

        Load loads[MAX_LOADS]{};

        size_t loadCount = 0U;
    } power;

    struct PowerSupervisor {
        bool enabled = false;
        unsigned long intervalMs=1500;

        GenericSensor::Config mainPower;   // BUFFER / ANALOG / CONSTANT
        GenericSensor::Config i12vOk;      // DIGITAL / BUFFER
        GenericSensor::Config i24vOk;      // DIGITAL / BUFFER
        GenericSensor::Config fault;       // DIGITAL / BUFFER
        GenericSensor::Config battery;     // DIGITAL / BUFFER

        float mainPowerLow;   // soglia bassa
        float mainPowerHigh;  
    }ps;

    struct SecurityZoneConfig
    {
        const char* name;
        int statusArea = -1;
        int cmdArea = -1;
    };

    struct Security {
        bool enabled = false;
        bool reportOnChange=true;
        uint32_t intervalMs = 3000;
        const WiredSensorsManager::WiredSensorConfig* sensors;
        size_t count;
        
        const SecurityZoneConfig* zones = nullptr;
        size_t zoneCount = 0;

        // Stato aggregato security
        int statusArea = -1;
        int statusArea2 = -1;
        int armStateArea = -1;
        int eventArea = -1;
                
        // Comandi Alarm Panel
        int panelCommandArea = -1;
        int panelBitCommandArea = -1;
        
        uint32_t startupInhibitMs = 10000;  
    } security;

    DiagnosticConfig diagnostic;
    
    struct Averages {
        bool enabled = false;
        uint32_t intervalMs = 10000;

        struct Sensore {
            int area;       // area buffer
            float factor;   // scaling (es. 0.1)
        };

        struct Gruppo {
            const char* nome;          // es. "Temperature"
            float outScale;            // es. 10.0f per scrivere *10
            int areaOut;               // area buffer di output
            const Sensore* sensori;    // array sensori
            size_t count;              // numero sensori
        };

        const Gruppo* gruppi = nullptr;
        size_t gruppiCount = 0;
    } averages;

    struct Watch {
        bool enabled = false;
        const int* aree = nullptr;   // elenco aree da monitorare
        size_t count = 0;
    } watch;

    // ============================================================
    // WEB API CONFIGURATION
    // ============================================================

    struct webApiDeviceMessaging
    {
        // ========================================================
        // HTTP CLIENT / POLLING
        // ========================================================

        struct Client
        {
            bool enabled =
                false;

            const WebApiDevice* devices =
                nullptr;

            size_t deviceCount =
                0;

            uint32_t timeoutMs =
                3000;
        };


        // ========================================================
        // HTTP SERVER / WEBHOOK
        // ========================================================

        struct Server
        {
            bool enabled =
                false;

            uint16_t port =
                8080;

            uint32_t timeoutMs =
                3000;
        };


        // ========================================================
        // INSTANCES
        // ========================================================

        Client client;

        Server server;


        // ========================================================
        // STATUS
        // ========================================================

        bool enabled() const
        {
            return
                client.enabled ||
                server.enabled;
        }
    } webApi;

    // ============================================================
    // ACCESS CONTROL CONFIGURATION
    // ============================================================

    struct Access
    {
        bool enabled =
            false;

        const AccessReader* readers =
            nullptr;

        size_t readerCount =
            0;

        const char* commonCardCode =
            nullptr;

        int defaultRelayIndex =
            1;

        int defaultRelayTime =
            10;
    } access;

    // ============================================================
    // SOLAR INVERTER
    // ============================================================
    struct Solar
    {
        bool enabled = false;
        bool reportOnChange = false;
        enum class TransportType
        {
            SERIAL_PORT,
            TCP_CLIENT,
            UDP_CLIENT
        };

        TransportType transport =
            TransportType::SERIAL_PORT;

        bool autoDiscovery = true;

        bool diagnosticsEnabled = true;

        bool publishEvents = true;

        uint32_t queryTimeoutMs = 3000;

        uint32_t reconnectMs = 5000;

        uint32_t discoveryTimeoutMs = 1000;

        uint32_t intervalMs = 1000;     

        struct TCP
        {
            IPAddress host;

            uint16_t port = 8899;

            uint32_t timeoutMs = 3000;

        } tcp;

        struct Serial
        {
            HardwareSerial* port = nullptr;

            uint32_t baudrate = 2400;

        } serial;

        struct Areas
        {
            int pvVoltage = -1;
            int pvCurrent = -1;
            int pvPower = -1;

            int batteryVoltage = -1;
            int batterySoc = -1;

            int gridVoltage = -1;
            int gridPower = -1;

            int outputPower = -1;
            int loadPercent = -1;

            int inverterTemperature = -1;
            
            int status = -1;
            int frontendStatus = -1;
        } areas;

    };

    static constexpr uint8_t
        MAX_SOLAR_INVERTERS =
            4;

    struct SolarInverters
    {
        uint8_t
            count =
                0;

        Solar
            inverter[MAX_SOLAR_INVERTERS];

    } solarInverters;

    // ============================================================
    // J CONFIGURATION
    // ============================================================
    struct Jobs {
        bool enabled = false;
        uint32_t intervalMs = 5000;
    } jobs;

};

#endif
