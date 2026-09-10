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
        };

        std::vector<Device> list;
    } devices;

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
    // 3) CONFIG MODBUS
    // -----------------------------
    struct Modbus {
        bool enabled = false;
        uint16_t timeoutMs = 150;
    } modbus;

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
                INT,
                FLOAT,
                BOOL,
                STRING,
                ENUM
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


    struct Power {
        bool enabled = false;
        uint32_t intervalMs = 15000;

        float limitSoft;
        float limitHard;
        bool autoTune;
        uint32_t tuneIntervalMs;

        struct Load {
            const char* name;
            int priority;
            float watt;
            int minOn;
            int minOff;
        };
        Load loads[8];
        size_t loadCount;

        struct Thermal {
            const char* name;
            bool enabled;
            float setpoint;
            float min;
            float max;
            int minOn;
            int minOff;
        } thermal;
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

    struct Security {
        bool enabled = false;
        uint32_t intervalMs = 3000;
        const WiredSensorsManager::WiredSensorConfig* sensors;
        size_t count;
        int statusArea=-1;
        uint32_t startupInhibitMs = 10000;   // <--- nuovo parametro
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

    struct webApiDeviceMessaging {
        bool enabled = false;
        uint32_t intervalMs = 2000;   // default 2s

        const DeviceMessageGroup* groups = nullptr;
        size_t groupCount = 0;

        const char* baseUrl = nullptr;
        uint32_t timeoutMs = 3000;
    } webApi;

    struct Jobs {
        bool enabled = false;
        uint32_t intervalMs = 1000;
    } jobs;

};

#endif
