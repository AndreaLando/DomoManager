#ifndef DMPLC_H
#define DMPLC_H

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

#include <vector>
#include <unordered_map>


#include "DMBuffers.hpp"
#include "DMSignal.hpp"
#include "DMBaseClassCore.hpp"
#include "DMEthernet.hpp"
#include "DMPlatformModbus.hpp"
#include "DMPlatform.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class GenericSensor {
public:
    using Config = GenericSensorConfig;

    static int readRaw(const Config& cfg, Buffer& buf) {
        switch (cfg.type) {
            case Config::Type::ANALOGIC:
                return analogRead(cfg.value);

            case Config::Type::DIGITAL:
                return digitalRead(cfg.value);

            case Config::Type::BUFFER:
                return buf.getValueFast(cfg.value);

            case Config::Type::CONSTANT:
                return cfg.value;
        }
        return 0;
    }

    static float read(const Config& cfg, Buffer& buf) {
        int raw = readRaw(cfg, buf);
        return raw * cfg.scale;
    }

};


class GenericPrgDevice
{
public:
    enum GenericPrgDeviceEnum
    {
        NOTSET = -1,
        AI = 0,
        AO = 1,
        DI = 2,
        DO = 3
    };


    enum GenericPrgDeviceHwEnum
    {
        Coil = 0,
        Input = 1,
        Hold = 2,
        Discrete = 3
    };


    struct GenericPrgDeviceChannel
    {
        GenericPrgDeviceEnum type;

        GenericPrgDeviceHwEnum hwType;

        int startingAddr;

        int items;

        int ItemsPerCall;
    };


    struct structRead
    {
        short itemsPerCall;

        short items;

        int startIndex;

        bool ok;
    };


    GenericPrgDevice(
        const char* name,
        DMIPAddress ip,
        unsigned int deviceAddress,
        std::vector<GenericPrgDeviceChannel> channels,
        std::vector<int> ioAreas,
        short errorCount,
        Priority priority,
        const char* zone = nullptr,
        const char* installationLocation = nullptr
    );


    bool Run();


    structRead Read(
        DMPlatform::Modbus::TCPClient& mb,
        int channel,
        uint16_t* outBuffer,
        unsigned long now
    );


    bool Write(
        DMPlatform::Modbus::TCPClient& mb,
        int channel,
        int address,
        int value,
        unsigned long now
    );


    int GetArea(
        int channel,
        int address
    );


    bool FindChannelByArea(
        int area,
        int& channel,
        int& item
    );


    const GenericPrgDeviceChannel&
    GetChannelInfo(
        int channel
    ) const;


    const DMIPAddress& GetIp() const;

    size_t GetChannelsSize() const;
    Priority GetPriority() const;


    const char* GetName() const;

    const char* GetZone() const;

    const char* GetInstallationLocation() const;

    unsigned int GetDeviceAddress() const;


    Errors& GetError()
    {
        return Error;
    }


    const Errors& GetError() const
    {
        return Error;
    }


    void SetErrorCallback(
        std::function<void(GenericPrgDevice*)> fn)
    {
        _onErrorChanged =
            fn;


        Error.SetStateChangedCallback(
            [this]()
            {
                if (_onErrorChanged)
                {
                    _onErrorChanged(
                        this
                    );
                }
            }
        );
    }


private:
    short bank;


    std::vector<int>
        _ioAreas;


    Priority
        _priority;


    const char*
        _name;


    const char*
        _zone;


    const char*
        _installationLocation;


    unsigned int
        _deviceAddress;


    DMIPAddress
        _ip;


    std::vector<GenericPrgDeviceChannel>
        _channels;


    Errors
        Error;


    const short
        MAX_CALLS = 8;


    std::function<void(GenericPrgDevice*)>
        _onErrorChanged = nullptr;
};

class GenericPrgDeviceManager {
public:
    void BuildPriorityCache(std::vector<GenericPrgDevice>& devices);
    void DebugPriorityCache(std::vector<GenericPrgDevice>& devices);
    
    const std::vector<int>& GetDevicesByPriority(
        Priority priority,
        const DMIPAddress& ip) const;

    bool HasDevicesByPriority(
        Priority priority,
        const DMIPAddress& ip) const;
private:
    // Chiave: (IP << 8) | priority
    std::unordered_map<uint64_t, std::vector<int>> _cachePriorityIndex;
    bool _cacheBuilt = false;
};

class DeviceManager {
private:
    int nextArea;
    std::vector<GenericPrgDevice> PrgDevices;
public:
    // ************ IO BUFFER *******************************
    //RESERVED
    static const int AREA_TRASH=0;
    static const int AREA_SYSTEM_ERRORS=1;
    static const int AREA_SYSTEM_RUNNING_T=2;
    
    static const int AREA_LAST_RESERVED=9; //Bit flags

    static inline bool IsReservedArea(int area) {
        return area >= DeviceManager::AREA_TRASH &&
            area <= DeviceManager::AREA_LAST_RESERVED;
    }

    // END RESERVED

    DeviceManager()
    {
        nextArea = AREA_LAST_RESERVED+1;   // prime 10 aree riservate
    }

    // ---------------------------------------------------------
    // AUTO
    // ---------------------------------------------------------

    void addDeviceAuto(
        const char* name,
        const IPAddress& ip,
        int deviceAddress,
        const std::vector<
            GenericPrgDevice::GenericPrgDeviceChannel
        >& channels,
        int retry,
        Priority priority,
        const char* zone = nullptr,
        const char* installationLocation = nullptr)
    {
        int count = 0;

        for (const auto& channel : channels)
            count += channel.items;

        std::vector<int> areas;
        areas.reserve(count);

        for (int i = 0; i < count; ++i)
            areas.push_back(nextArea++);

        PrgDevices.emplace_back(
            name,
            ip,
            static_cast<unsigned int>(deviceAddress),
            channels,
            areas,
            static_cast<short>(retry),
            priority,
            zone,
            installationLocation
        );
    }

    // ---------------------------------------------------------
    // MANUAL
    // ---------------------------------------------------------

    void addDeviceManual(
        const char* name,
        const IPAddress& ip,
        int deviceAddress,
        const std::vector<
            GenericPrgDevice::GenericPrgDeviceChannel
        >& channels,
        const std::vector<int>& areas,
        int retry,
        Priority priority,
        const char* zone = nullptr,
        const char* installationLocation = nullptr)
    {
        PrgDevices.emplace_back(
            name,
            ip,
            static_cast<unsigned int>(
                deviceAddress
            ),
            channels,
            areas,
            static_cast<short>(
                retry
            ),
            priority,
            zone,
            installationLocation
        );

        if (areas.empty())
            return;

        const auto maxAreaIt =
            std::max_element(
                areas.begin(),
                areas.end()
            );

        if (maxAreaIt != areas.end() &&
            *maxAreaIt + 1 > nextArea)
        {
            nextArea =
                *maxAreaIt + 1;
        }
    }


    // ---------------------------------------------------------
    // ACCESSO
    // ---------------------------------------------------------
    std::vector<GenericPrgDevice>& getDevices() {
        return PrgDevices;
    }

    const std::vector<GenericPrgDevice>& getDevices() const {
        return PrgDevices;
    }

    // ---------------------------------------------------------
    // DIAGNOSTICA
    // ---------------------------------------------------------
    int GetMaxReadSize() {
        int maxSize = 1;

        for (auto& dev : PrgDevices) {
            int channels = dev.GetChannelsSize();

            for (int ch = 0; ch < channels; ch++) {
                //auto info = dev.GetChannelInfo(ch);
                const auto& info = dev.GetChannelInfo(ch);
                int size = info.items * info.ItemsPerCall;
                if (size > maxSize)
                    maxSize = size;
            }
        }

        return maxSize;
    }

    int DeviceHasErrors()
    {
        static uint64_t errorMask = 0;

        int errorCount = 0;

        for (size_t index = 0;
            index < PrgDevices.size();
            ++index)
        {
            GenericPrgDevice& device =
                PrgDevices[index];

            const bool inError =
                device.GetError().IsInError();

            if (inError)
            {
                ++errorCount;

                if (index >= 64)
                    continue;

                const uint64_t bit =
                    1ULL << index;

                if ((errorMask & bit) != 0)
                    continue;

                LOG_WF(
                    "DeviceManager",
                    "DEVICE EXCLUSION"
                    " | Index=%u"
                    " | %s"
                    " | Zona=%s"
                    " | Installazione=%s"
                    " | IP=%u.%u.%u.%u"
                    " | Addr=%u",
                    static_cast<unsigned>(index),
                    device.GetName(),
                    device.GetZone(),
                    device.GetInstallationLocation(),
                    static_cast<unsigned>(device.GetIp()[0]),
                    static_cast<unsigned>(device.GetIp()[1]),
                    static_cast<unsigned>(device.GetIp()[2]),
                    static_cast<unsigned>(device.GetIp()[3]),
                    static_cast<unsigned>(
                        device.GetDeviceAddress()
                    )
                );

                errorMask |= bit;
            }
            else if (index < 64)
            {
                errorMask &=
                    ~(1ULL << index);
            }
        }

        return errorCount;
    }

    int getMaxAreaUsed() {
        int maxArea = 0;

        for (auto& dev : PrgDevices) {
            int channels = dev.GetChannelsSize();

            for (int ch = 0; ch < channels; ch++) {
                //auto info = dev.GetChannelInfo(ch);
                const auto& info = dev.GetChannelInfo(ch);
                for (int i = 0; i < info.items; i++) {
                    int area = dev.GetArea(ch, i);
                    if (area > maxArea)
                        maxArea = area;
                }
            }
        }

        return maxArea;
    }

    // =========================================================
    // DIAGNOSTIC
    // =========================================================

    class Diagnostic
    {
    public:
        static void ReportErrors(
            const DeviceManager& manager)
        {
            const auto& devices =
                manager.getDevices();

            uint64_t errorMask =
                0;

            uint64_t parkedMask =
                0;

            size_t totalErrors =
                0;

            size_t totalParked =
                0;

            size_t totalOk =
                0;


            Serial.println(
                "\n===== DEVICE ERROR REPORT ====="
            );


            for (size_t index = 0;
                index < devices.size();
                ++index)
            {
                const GenericPrgDevice& device =
                    devices[index];

                const Errors& error =
                    device.GetError();

                const DMIPAddress& ip =
                    device.GetIp();


                if (error.IsVisibleParked())
                {
                    ++totalParked;

                    if (index < 64)
                    {
                        parkedMask |=
                            1ULL << index;
                    }


                    PrintDevice(
                        index,
                        "PARKED",
                        device,
                        ip
                    );

                    continue;
                }


                if (error.IsVisibleError())
                {
                    ++totalErrors;

                    if (index < 64)
                    {
                        errorMask |=
                            1ULL << index;
                    }


                    PrintDevice(
                        index,
                        "ERROR",
                        device,
                        ip
                    );

                    continue;
                }


                ++totalOk;
            }


            Serial.println(
                "\n____ SUMMARY ____"
            );

            Serial.print(
                "Total devices: "
            );

            Serial.println(
                devices.size()
            );


            Serial.print(
                "OK: "
            );

            Serial.println(
                totalOk
            );


            Serial.print(
                "Errors: "
            );

            Serial.println(
                totalErrors
            );


            Serial.print(
                "Parked: "
            );

            Serial.println(
                totalParked
            );


            PrintMask(
                "Error bitmask",
                errorMask
            );

            PrintMask(
                "Parked bitmask",
                parkedMask
            );


            if (totalErrors == 0 &&
                totalParked == 0)
            {
                Serial.println(
                    "Nessun device in errore o parked."
                );
            }


            Serial.println(
                "===== END DEVICE ERROR REPORT =====\n"
            );
        }


    private:
        static void PrintDevice(
            size_t index,
            const char* state,
            const GenericPrgDevice& device,
            const DMIPAddress& ip)
        {
            DMPlatform::SerialPrintf(
                "Device %u -> %s"
                " | %s"
                " | Zona=%s"
                " | Installazione=%s"
                " | IP=%u.%u.%u.%u"
                " | Addr=%u"
                "\n",
                static_cast<unsigned>(
                    index
                ),
                state
                    ? state
                    : "?",
                device.GetName(),
                device.GetZone(),
                device.GetInstallationLocation(),
                static_cast<unsigned>(
                    ip[0]
                ),
                static_cast<unsigned>(
                    ip[1]
                ),
                static_cast<unsigned>(
                    ip[2]
                ),
                static_cast<unsigned>(
                    ip[3]
                ),
                static_cast<unsigned>(
                    device.GetDeviceAddress()
                )
            );
        }


        static void PrintMask(
            const char* label,
            uint64_t mask)
        {
            Serial.print(
                label
                    ? label
                    : "Mask"
            );

            Serial.print(
                " = 0x"
            );


            const uint32_t high =
                static_cast<uint32_t>(
                    mask >> 32
                );

            const uint32_t low =
                static_cast<uint32_t>(
                    mask &
                    0xFFFFFFFFULL
                );


            if (high != 0)
            {
                DMPlatform::SerialPrintf(
                    "%08lX%08lX\n",
                    static_cast<unsigned long>(
                        high
                    ),
                    static_cast<unsigned long>(
                        low
                    )
                );
            }
            else
            {
                DMPlatform::SerialPrintf(
                    "%lX\n",
                    static_cast<unsigned long>(
                        low
                    )
                );
            }
        }
    };
};

class ToggleManager {
public:

    struct ToggleSignalItem {
        ToggleSignal Toggle;
        int areaRead;
        std::vector<int> forwardsFromAreas;
    };

private:

    std::vector<ToggleSignalItem> toggles;

    // Cache: areaRead → indice nel vector
    std::unordered_map<int, size_t> cacheByArea;

    // Cache: forward area → indice nel vector
    std::unordered_map<int, size_t> cacheByForwardArea;

    // Cache utilizzata dalla logica esistente
    std::unordered_set<int> forwardAreasCache;


public:

    ToggleManager() {}


    // ============================================================
    // AREA GESTITA DA TOGGLE?
    // ============================================================

    bool handlesArea(int area) const
    {
        return cacheByArea.count(area) > 0 ||
               cacheByForwardArea.count(area) > 0;
    }


    // ============================================================
    // FORWARD AREA
    // ============================================================

    bool isForwardArea(int area) const
    {
        return forwardAreasCache.count(area) > 0;
    }


    // ============================================================
    // ACCESSO
    // ============================================================

    const std::vector<ToggleSignalItem>& getAll() const
    {
        return toggles;
    }


    // ============================================================
    // ADD TOGGLE
    // ============================================================

    void add(
        int areaRead,
        std::vector<int> forwards = {})
    {
        toggles.push_back({
            ToggleSignal(),
            areaRead,
            forwards
        });

        const size_t index = toggles.size() - 1;

        // Cache areaRead → Toggle
        cacheByArea[areaRead] = index;

        // Cache forwardArea → Toggle
        for (int fwd : forwards)
        {
            forwardAreasCache.insert(fwd);
            cacheByForwardArea[fwd] = index;
        }
    }


    // ============================================================
    // ACCESSO PER INDICE
    // ============================================================

    ToggleSignalItem* operator[](size_t index)
    {
        return index < toggles.size()
            ? &toggles[index]
            : nullptr;
    }


    size_t size() const
    {
        return toggles.size();
    }


    // ============================================================
    // GET PER INDICE
    // ============================================================

    ToggleSignalItem* get(size_t index)
    {
        return index < toggles.size()
            ? &toggles[index]
            : nullptr;
    }


    // ============================================================
    // GET PER AREA READ
    // ============================================================

    inline ToggleSignalItem* get(int areaRead)
    {
        auto it = cacheByArea.find(areaRead);

        if (it == cacheByArea.end())
            return nullptr;

        const size_t idx = it->second;

        return idx < toggles.size()
            ? &toggles[idx]
            : nullptr;
    }


    // ============================================================
    // GET PER FORWARD AREA
    // ============================================================

    inline ToggleSignalItem* getByForwardArea(int area)
    {
        auto it = cacheByForwardArea.find(area);

        if (it == cacheByForwardArea.end())
            return nullptr;

        const size_t idx = it->second;

        return idx < toggles.size()
            ? &toggles[idx]
            : nullptr;
    }


    // ============================================================
    // GET FORWARD VALUE
    // ============================================================

    inline int getForwardValue(
        int area,
        Buffer &buffer)
    {
        const int max = buffer.size();

        ToggleSignalItem* t = get(area);

        if (!t)
            return 0;

        for (int fwdArea : t->forwardsFromAreas)
        {
            if (fwdArea >= 0 &&
                fwdArea < max &&
                buffer.getValueFast(fwdArea) > 0)
            {
                return 1;
            }
        }

        return 0;
    }


    // ============================================================
    // RESULT
    // ============================================================

    struct ToggleResult
    {
        // Area fisica del Buffer.
        //
        // Per Toggle normale:
        //     areaToWrite
        //
        // Per Toggle Route-only:
        //     Buffer::NO_AREA
        int area = Buffer::NO_AREA;

        long value = 0;

        // Area logica del Toggle.
        //
        // Esempio:
        //     Toggle 90 con forward 82
        //
        // Evento ricevuto:
        //     area=82
        //
        // toggleArea:
        //     90
        int toggleArea = Buffer::NO_AREA;
    };


    // ============================================================
    // PROCESS EVENT
    // ============================================================

    bool processEvent(
        int area,
        long value,
        Buffer& buffer,
        unsigned long now,
        ToggleResult& result)
    {
        (void)now;

        ToggleSignalItem* toggle = get(area);

        if (!toggle)
            toggle = getByForwardArea(area);

        if (!toggle)
            return false;

        // Area logica/primaria del Toggle.
        result.toggleArea = toggle->areaRead;

        long signalIn = value;

        // ========================================================
        // FORWARD
        // ========================================================

        if (!toggle->forwardsFromAreas.empty())
        {
            signalIn = getForwardValue(
                toggle->areaRead,
                buffer
            );

            if (signalIn == 0)
                signalIn = value;
        }

        // ========================================================
        // CERCA AreaToWrite
        // ========================================================

        const int areaToWrite =
            buffer.GetAreaToWrite(toggle->areaRead);


        // ========================================================
        // TOGGLE CON AreaToWrite
        //
        // COMPORTAMENTO ORIGINALE
        // ========================================================

        if (areaToWrite > 0)
        {
            BufferSourceInfo outputInfo;

            if (!buffer.GetData(
                    areaToWrite,
                    outputInfo))
            {
                return false;
            }

            long toggleOut = outputInfo.value;

            if (!toggle->Toggle.change(
                    signalIn,
                    toggleOut))
            {
                return false;
            }

            result.area = areaToWrite;
            result.value = toggleOut;

            return true;
        }


        // ========================================================
        // TOGGLE SENZA AreaToWrite
        //
        // L'uscita viene gestita da Route.
        // ========================================================

        long toggleOut = 0;

        if (!toggle->Toggle.changeRoute(
                signalIn,
                toggleOut))
        {
            return false;
        }

        result.area = Buffer::NO_AREA;
        result.value = toggleOut;

        return true;
    }
};

class RouteManager
{
public:

    // ============================================================
    // STRUTTURE
    // ============================================================

    struct RouteAction
    {
        int targetArea;
        int value;
    };

    struct RouteCase
    {
        int triggerValue;
        std::vector<RouteAction> actions;
    };

    struct RouteConfig
    {
        String name;
        int triggerArea;
        std::vector<RouteCase> cases;
    };

    struct SafeRoute
    {
        std::vector<RouteCase> cases;
    };


private:

    std::unordered_map<int, SafeRoute> routes;


public:

    // ============================================================
    // LOAD
    // ============================================================

    void load(const std::vector<RouteConfig>& cfg)
    {
        for (const auto& r : cfg)
        {
            routes[r.triggerArea] = {
                r.cases
            };
        }
    }


    // ============================================================
    // LOOKUP
    // ============================================================

    inline bool hasRoute(int area) const
    {
        return routes.find(area) != routes.end();
    }


    // ============================================================
    // EXECUTE
    // ============================================================

    std::vector<RouteAction> execute(
        int srcArea,
        long value,
        Buffer& buffer) const
    {
        std::vector<RouteAction> actions;

        const auto it = routes.find(srcArea);

        if (it == routes.end())
            return actions;

        const auto& route = it->second;

        const int bufferSize =
            static_cast<int>(buffer.size());

        for (const auto& rc : route.cases)
        {
            if (rc.triggerValue != value)
                continue;

            for (const auto& act : rc.actions)
            {
                if (act.targetArea < 0 ||
                    act.targetArea >= bufferSize)
                {
                    LOG_WF(
                        "RouteManager",
                        "Skip route write: invalid targetArea=%d (srcArea=%d)",
                        act.targetArea,
                        srcArea
                    );

                    continue;
                }

                if (DeviceManager::IsReservedArea(
                        act.targetArea))
                {
                    LOG_WF(
                        "RouteManager",
                        "Skip route write: reserved targetArea=%d (srcArea=%d)",
                        act.targetArea,
                        srcArea
                    );

                    continue;
                }

                actions.push_back(act);
            }

            break;
        }

        return actions;
    }
};

class DeviceProfiles {
public:
    using Channel = GenericPrgDevice::GenericPrgDeviceChannel;

    struct ProfileEntry {
        const char* name;
        std::vector<Channel> channels;
    };

private:
    std::vector<ProfileEntry> profiles;

public:
    DeviceProfiles() {
        loadBuiltins();
    }

    // ------------------------------------------------------------
    // Aggiunge un profilo custom (controllo duplicati)
    // ------------------------------------------------------------
    bool add(const char* name, const std::vector<Channel>& channels) {
        if (exists(name)) {
            LOG_EF("DeviceProfiles",
                   "ERRORE: Profilo '%s' già esistente. Duplicato NON aggiunto.",
                   name);
            return false;
        }

        profiles.push_back({ name, channels });
        return true;
    }

    // ------------------------------------------------------------
    // Recupera un profilo (controllo nome valido)
    // ------------------------------------------------------------
    const std::vector<Channel>* get(const char* name) const {
        for (auto& p : profiles) {
            if (strcmp(p.name, name) == 0)
                return &p.channels;
        }

        LOG_EF("DeviceProfiles",
               "ERRORE: Profilo '%s' non trovato! Controllare Initialization.",
               name);
        return nullptr;
    }

    // ------------------------------------------------------------
    // Lista dei profili disponibili
    // ------------------------------------------------------------
    std::vector<const char*> list() const {
        std::vector<const char*> out;
        out.reserve(profiles.size());
        for (auto& p : profiles)
            out.push_back(p.name);
        return out;
    }

private:
    bool exists(const char* name) const {
        for (auto& p : profiles)
            if (strcmp(p.name, name) == 0)
                return true;
        return false;
    }

    // ------------------------------------------------------------
    // Built‑in profiles
    // ------------------------------------------------------------
    void loadBuiltins() {

        add("N4DIH32", {
            {GenericPrgDevice::DI, GenericPrgDevice::Hold, 128, 32, 1}
        });

        add("MA01_XACX0440", {
            {GenericPrgDevice::DI, GenericPrgDevice::Discrete, 0, 4, 1},
            {GenericPrgDevice::DO, GenericPrgDevice::Coil,     0, 4, 1}
        });

        add("MA01_AXCX4020", {
            {GenericPrgDevice::DI, GenericPrgDevice::Discrete, 0, 4, 1},
            {GenericPrgDevice::DO, GenericPrgDevice::Coil,     0, 2, 1}
        });

        add("MA01_AXCX0080", {
            {GenericPrgDevice::DO, GenericPrgDevice::Coil, 0, 8, 1}
        });

        add("LE_01MQ", {
            {GenericPrgDevice::AI, GenericPrgDevice::Input, 0, 1, 2},
            {GenericPrgDevice::AI, GenericPrgDevice::Input, 6, 1, 2},
            {GenericPrgDevice::AI, GenericPrgDevice::Input, 12, 1, 2},
            {GenericPrgDevice::AI, GenericPrgDevice::Input, 24, 1, 2},
            {GenericPrgDevice::AI, GenericPrgDevice::Input, 70, 1, 2}
        });

        add("R421A08", {
            {GenericPrgDevice::DO, GenericPrgDevice::Hold, 1, 8, 1}
        });

        add("GENERIC_4DI_4DO", {
            {GenericPrgDevice::DI, GenericPrgDevice::Discrete, 0, 4, 1},
            {GenericPrgDevice::DO, GenericPrgDevice::Coil,     0, 4, 1}
        });

        add("CWT_SLTH_6W_S", {
            {GenericPrgDevice::AI, GenericPrgDevice::Hold, 0, 3, 1}
        });

        add("CWT_SLTH_6W_S_C", {
            {GenericPrgDevice::AI, GenericPrgDevice::Hold, 0, 2, 1},
            {GenericPrgDevice::AI, GenericPrgDevice::Hold, 6, 1, 1}
        });

        add("CWT_THCO_2K", {
            {GenericPrgDevice::AI, GenericPrgDevice::Hold, 0, 3, 1}
        });

        add("R414A01", {
            {GenericPrgDevice::AI, GenericPrgDevice::Hold, 0, 2, 1}
        });

        add("IR_210", {
            {GenericPrgDevice::AO, GenericPrgDevice::Hold, 109, 12, 1}
        });

        add("CTR4A01", {
            {GenericPrgDevice::AI, GenericPrgDevice::Input, 0, 1, 1}
        });

        add("PTA8C04", {
            {GenericPrgDevice::AI, GenericPrgDevice::Hold, 0, 4, 1}
        });

        add("RESI_LED", {
            {GenericPrgDevice::AO, GenericPrgDevice::Hold, 0, 12, 1}
        });

        add("LSA_H4P40YBM", {
            {GenericPrgDevice::AO, GenericPrgDevice::Hold,  0, 4, 1},
            {GenericPrgDevice::AI, GenericPrgDevice::Input, 0, 4, 1}
        });

        add("PROBE01", {
            {GenericPrgDevice::AI, GenericPrgDevice::Hold, 0, 2, 1}
        });

        add("CWT_MB308K", {
            {GenericPrgDevice::DI, GenericPrgDevice::Discrete, 0, 16, 1},
            {GenericPrgDevice::DO, GenericPrgDevice::Coil,     0, 12, 1}
        });
    }
};



#endif
