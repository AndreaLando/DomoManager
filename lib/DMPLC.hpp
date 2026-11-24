#ifndef DMPLC_HPP
#define DMPLC_HPP

/* ============================================================================
 * DMPLC.hpp
 *
 * Modulo header-only per la gestione dei dispositivi PLC/Modbus di
 * DomoManager. Contiene dichiarazioni e implementazioni precedentemente
 * separate tra DMPLC.h e DMPLC.c.
 *
 * Responsabilita:
 *   - descrizione e accesso ai dispositivi programmabili;
 *   - indicizzazione per IP e priorita;
 *   - assegnazione e ricerca delle aree;
 *   - diagnostica della comunicazione;
 *   - catalogo dei profili hardware predefiniti.
 *
 * Autore:          Andrea Lando
 * Contatto:        mail@domo-manager.it
 * Versione modulo: 1.1.0
 * Ultima modifica: 2026-11-22
 *
 * Nota header-only:
 *   le definizioni fuori classe sono marcate inline per evitare violazioni
 *   ODR quando il file viene incluso da piu unita di traduzione.
 * ============================================================================ */

#include <unordered_map>
#include <vector>

#include "DMBuffers.hpp"
#include "DMSignal.hpp"
#include "DMBaseClassCore.hpp"
#include "DMBaseClassUtils.hpp"
#include "DMEthernet.hpp"
#include "DMPlatformModbus.hpp"
#include "DMPlatform.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================================
// GENERIC PROGRAMMABLE DEVICE
// Modello di un dispositivo PLC/Modbus e dei relativi canali I/O.
// ============================================================================
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

// ============================================================================
// GENERIC PROGRAMMABLE DEVICE MANAGER
// Cache e selezione dei dispositivi per indirizzo IP e priorita.
// ============================================================================
class GenericPrgDeviceManager
{
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

// ============================================================================
// DEVICE MANAGER
// Costruzione dei device, assegnazione aree e accesso alla collezione runtime.
// ============================================================================
class DeviceManager
{
private:
    int nextArea;
    std::vector<GenericPrgDevice> PrgDevices;
public:
    // ************ IO BUFFER *******************************
    //RESERVED
    static constexpr int AREA_TRASH =
        Buffer::ReservedAreas::TRASH;
    static constexpr int AREA_SYSTEM_ERRORS =
        Buffer::ReservedAreas::SYSTEM_ERRORS;
    static constexpr int AREA_SYSTEM_RUNNING_T =
        Buffer::ReservedAreas::SYSTEM_RUNNING_T;
    static constexpr int AREA_LAST_RESERVED =
        Buffer::ReservedAreas::LAST;

    static constexpr bool IsReservedArea(int area)
    {
        return Buffer::ReservedAreas::contains(area);
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
};

// ============================================================================
// DEVICE MANAGER DIAGNOSTIC
/* Note:
                    • Analisi centralizzata dello stato dispositivi
                    • Classificazione READ_ONLY, WRITE_ONLY e READ_WRITE
                    • Report dispositivi Modbus
                    • Rilevamento indisponibilità globale device leggibili

   ============================================================================ */

// ============================================================================
// DEVICE MANAGER DIAGNOSTIC
// Classificazione, analisi e report dello stato di comunicazione.
// ============================================================================
class DeviceManagerDiagnostic
{
public:

    // ========================================================================
    // COMMUNICATION TYPE
    // ========================================================================

    enum class CommunicationType : uint8_t
    {
        NO_IO = 0,

        READ_ONLY,

        WRITE_ONLY,

        READ_WRITE
    };

    // ========================================================================
    // DEVICE STATE
    // ========================================================================

    enum class DeviceState : uint8_t
    {
        READ_OK = 0,

        NOT_VERIFIED,

        ERROR,

        PARKED,

        NO_IO
    };

    // ========================================================================
    // GLOBAL STATE
    // ========================================================================

    enum class GlobalState : uint8_t
    {
        NOT_VERIFIABLE = 0,

        OK,

        DEGRADED,

        ERROR
    };

    // ========================================================================
    // HEALTH SUMMARY
    // ========================================================================

    struct HealthSummary
    {
        size_t totalDevices =
            0;

        size_t readableDevices =
            0;

        size_t readableOk =
            0;

        size_t readableErrors =
            0;

        size_t readableParked =
            0;

        size_t outputOnlyDevices =
            0;

        size_t outputOnlyNotVerified =
            0;

        size_t outputOnlyErrors =
            0;

        size_t outputOnlyParked =
            0;

        size_t noIoDevices =
            0;

        size_t totalErrors =
            0;

        size_t totalParked =
            0;

        uint64_t errorMask =
            0;

        uint64_t parkedMask =
            0;

        GlobalState globalState =
            GlobalState::NOT_VERIFIABLE;

        bool allReadableUnavailable =
            false;

        bool allConfiguredUnavailable =
            false;
    };

    // ========================================================================
    // DEVICE CAPABILITIES
    // ========================================================================

    static bool HasReadableChannels(
        const GenericPrgDevice& device)
    {
        const int channelCount =
            static_cast<int>(
                device.GetChannelsSize()
            );

        for (
            int channel = 0;
            channel < channelCount;
            ++channel
        )
        {
            const auto& channelInfo =
                device.GetChannelInfo(
                    channel
                );

            if (
                channelInfo.type ==
                    GenericPrgDevice::DI ||
                channelInfo.type ==
                    GenericPrgDevice::AI
            )
            {
                return true;
            }
        }

        return false;
    }

    static bool HasWritableChannels(
        const GenericPrgDevice& device)
    {
        const int channelCount =
            static_cast<int>(
                device.GetChannelsSize()
            );

        for (
            int channel = 0;
            channel < channelCount;
            ++channel
        )
        {
            const auto& channelInfo =
                device.GetChannelInfo(
                    channel
                );

            if (
                channelInfo.type ==
                    GenericPrgDevice::DO ||
                channelInfo.type ==
                    GenericPrgDevice::AO
            )
            {
                return true;
            }
        }

        return false;
    }

    // ========================================================================
    // COMMUNICATION CLASSIFICATION
    // ========================================================================

    static CommunicationType GetCommunicationType(
        const GenericPrgDevice& device)
    {
        const bool readable =
            HasReadableChannels(
                device
            );

        const bool writable =
            HasWritableChannels(
                device
            );

        if (
            readable &&
            writable
        )
        {
            return
                CommunicationType::READ_WRITE;
        }

        if (readable)
        {
            return
                CommunicationType::READ_ONLY;
        }

        if (writable)
        {
            return
                CommunicationType::WRITE_ONLY;
        }

        return
            CommunicationType::NO_IO;
    }

    // ========================================================================
    // DEVICE STATE CLASSIFICATION
    // ========================================================================

    static DeviceState GetDeviceState(
        const GenericPrgDevice& device)
    {
        const Errors& error =
            device.GetError();

        if (error.IsVisibleParked())
        {
            return
                DeviceState::PARKED;
        }

        if (error.IsVisibleError())
        {
            return
                DeviceState::ERROR;
        }

        const CommunicationType type =
            GetCommunicationType(
                device
            );

        if (
            type ==
                CommunicationType::READ_ONLY ||
            type ==
                CommunicationType::READ_WRITE
        )
        {
            return
                DeviceState::READ_OK;
        }

        if (
            type ==
            CommunicationType::WRITE_ONLY
        )
        {
            /*
             * Un dispositivo solo uscita non può essere
             * verificato dal polling READ.
             */
            return
                DeviceState::NOT_VERIFIED;
        }

        return
            DeviceState::NO_IO;
    }

    // ========================================================================
    // ANALYZE
    // ========================================================================

    static HealthSummary Analyze(
        const DeviceManager& manager)
    {
        HealthSummary summary;

        const auto& devices =
            manager.getDevices();

        summary.totalDevices =
            devices.size();

        for (
            size_t index = 0;
            index < devices.size();
            ++index
        )
        {
            const GenericPrgDevice& device =
                devices[index];

            const CommunicationType type =
                GetCommunicationType(
                    device
                );

            const DeviceState state =
                GetDeviceState(
                    device
                );

            const bool readable =
                type ==
                    CommunicationType::READ_ONLY ||
                type ==
                    CommunicationType::READ_WRITE;

            const bool outputOnly =
                type ==
                    CommunicationType::WRITE_ONLY;

            // ------------------------------------------------------------
            // GLOBAL ERROR COUNTERS
            // ------------------------------------------------------------

            if (
                state ==
                DeviceState::ERROR
            )
            {
                ++summary.totalErrors;

                if (index < 64)
                {
                    summary.errorMask |=
                        1ULL << index;
                }
            }
            else if (
                state ==
                DeviceState::PARKED
            )
            {
                ++summary.totalParked;

                if (index < 64)
                {
                    summary.parkedMask |=
                        1ULL << index;
                }
            }

            // ------------------------------------------------------------
            // READABLE DEVICES
            // ------------------------------------------------------------

            if (readable)
            {
                ++summary.readableDevices;

                if (
                    state ==
                    DeviceState::ERROR
                )
                {
                    ++summary.readableErrors;
                }
                else if (
                    state ==
                    DeviceState::PARKED
                )
                {
                    ++summary.readableParked;
                }
                else
                {
                    ++summary.readableOk;
                }

                continue;
            }

            // ------------------------------------------------------------
            // OUTPUT-ONLY DEVICES
            // ------------------------------------------------------------

            if (outputOnly)
            {
                ++summary.outputOnlyDevices;

                if (
                    state ==
                    DeviceState::ERROR
                )
                {
                    ++summary.outputOnlyErrors;
                }
                else if (
                    state ==
                    DeviceState::PARKED
                )
                {
                    ++summary.outputOnlyParked;
                }
                else
                {
                    ++summary.outputOnlyNotVerified;
                }

                continue;
            }

            // ------------------------------------------------------------
            // NO-I/O DEVICE
            // ------------------------------------------------------------

            ++summary.noIoDevices;
        }

        // --------------------------------------------------------------------
        // GLOBAL READABLE STATE
        // --------------------------------------------------------------------

        summary.allReadableUnavailable =
            summary.readableDevices > 0 &&
            (
                summary.readableErrors +
                summary.readableParked
            ) ==
            summary.readableDevices;

        // --------------------------------------------------------------------
        // GLOBAL CONFIGURED STATE
        // --------------------------------------------------------------------

        summary.allConfiguredUnavailable =
            summary.totalDevices > 0 &&
            (
                summary.totalErrors +
                summary.totalParked
            ) ==
            summary.totalDevices;

        // --------------------------------------------------------------------
        // GLOBAL STATE
        // --------------------------------------------------------------------

        if (
            summary.readableDevices ==
            0
        )
        {
            summary.globalState =
                GlobalState::NOT_VERIFIABLE;
        }
        else if (
            summary.allReadableUnavailable
        )
        {
            summary.globalState =
                GlobalState::ERROR;
        }
        else if (
            summary.readableErrors > 0 ||
            summary.readableParked > 0
        )
        {
            summary.globalState =
                GlobalState::DEGRADED;
        }
        else
        {
            summary.globalState =
                GlobalState::OK;
        }

        return summary;
    }

    // ========================================================================
    // REPORT
    // ========================================================================

    static void Report(
        const DeviceManager& manager)
    {
        const HealthSummary summary =
            Analyze(
                manager
            );

        const auto& devices =
            manager.getDevices();

        Serial.println();

        Serial.println(
            "===== DEVICE COMMUNICATION REPORT ====="
        );

        // --------------------------------------------------------------------
        // DEVICE DETAILS
        // --------------------------------------------------------------------

        for (
            size_t index = 0;
            index < devices.size();
            ++index
        )
        {
            const GenericPrgDevice& device =
                devices[index];

            PrintDevice(
                index,
                device,
                GetCommunicationType(
                    device
                ),
                GetDeviceState(
                    device
                )
            );
        }

        // --------------------------------------------------------------------
        // SUMMARY
        // --------------------------------------------------------------------

        Serial.println();

        Serial.println(
            "____ COMMUNICATION SUMMARY ____"
        );

        PrintCount(
            "Configured devices",
            summary.totalDevices
        );

        Serial.println();

        PrintCount(
            "Readable devices",
            summary.readableDevices
        );

        PrintCount(
            "  Read OK",
            summary.readableOk
        );

        PrintCount(
            "  Read ERROR",
            summary.readableErrors
        );

        PrintCount(
            "  Read PARKED",
            summary.readableParked
        );

        Serial.println();

        PrintCount(
            "Output-only devices",
            summary.outputOnlyDevices
        );

        PrintCount(
            "  Not verified",
            summary.outputOnlyNotVerified
        );

        PrintCount(
            "  Output ERROR",
            summary.outputOnlyErrors
        );

        PrintCount(
            "  Output PARKED",
            summary.outputOnlyParked
        );

        if (
            summary.noIoDevices >
            0
        )
        {
            Serial.println();

            PrintCount(
                "Devices without I/O channels",
                summary.noIoDevices
            );
        }

        Serial.println();

        PrintCount(
            "Total visible errors",
            summary.totalErrors
        );

        PrintCount(
            "Total parked",
            summary.totalParked
        );

        // --------------------------------------------------------------------
        // GLOBAL STATE
        // --------------------------------------------------------------------

        Serial.println();

        Serial.print(
            "Global Modbus status: "
        );

        Serial.println(
            GlobalStateToString(
                summary.globalState
            )
        );

        if (
            summary.allReadableUnavailable
        )
        {
            Serial.println(
                "CRITICAL: all readable Modbus devices "
                "are unavailable."
            );
        }

        Serial.print(
            "All readable devices unavailable: "
        );

        Serial.println(
            summary.allReadableUnavailable
                ? "YES"
                : "NO"
        );

        Serial.print(
            "All configured devices unavailable: "
        );

        Serial.println(
            summary.allConfiguredUnavailable
                ? "YES"
                : "NO"
        );

        Serial.println();

        PrintMask(
            "Error bitmask",
            summary.errorMask
        );

        PrintMask(
            "Parked bitmask",
            summary.parkedMask
        );

        Serial.println(
            "===== END DEVICE COMMUNICATION REPORT ====="
        );

        Serial.println();
    }

    // ========================================================================
    // STRING HELPERS
    // ========================================================================

    static const char* CommunicationTypeToString(
        CommunicationType type)
    {
        switch (type)
        {
            case CommunicationType::READ_ONLY:
                return "READ_ONLY";

            case CommunicationType::WRITE_ONLY:
                return "WRITE_ONLY";

            case CommunicationType::READ_WRITE:
                return "READ_WRITE";

            case CommunicationType::NO_IO:
            default:
                return "NO_IO";
        }
    }

    static const char* DeviceStateToString(
        DeviceState state)
    {
        switch (state)
        {
            case DeviceState::READ_OK:
                return "READ_OK";

            case DeviceState::NOT_VERIFIED:
                return "NOT_VERIFIED";

            case DeviceState::ERROR:
                return "ERROR";

            case DeviceState::PARKED:
                return "PARKED";

            case DeviceState::NO_IO:
            default:
                return "NO_IO";
        }
    }

    static const char* GlobalStateToString(
        GlobalState state)
    {
        switch (state)
        {
            case GlobalState::OK:
                return "OK";

            case GlobalState::DEGRADED:
                return "DEGRADED";

            case GlobalState::ERROR:
                return "ERROR";

            case GlobalState::NOT_VERIFIABLE:
            default:
                return "NOT_VERIFIABLE";
        }
    }

private:

    // ========================================================================
    // PRINT DEVICE
    // ========================================================================

    static void PrintDevice(
        size_t index,
        const GenericPrgDevice& device,
        CommunicationType type,
        DeviceState state)
    {
        const DMIPAddress& ip =
            device.GetIp();

        DMPlatform::SerialPrintf(
            "Device %u"
            " | State=%s"
            " | Type=%s"
            " | Name=%s"
            " | Zone=%s"
            " | Installation=%s"
            " | IP=%u.%u.%u.%u"
            " | Address=%u"
            "\n",
            static_cast<unsigned>(
                index
            ),
            DeviceStateToString(
                state
            ),
            CommunicationTypeToString(
                type
            ),
            device.GetName()
                ? device.GetName()
                : "",
            device.GetZone()
                ? device.GetZone()
                : "",
            device.GetInstallationLocation()
                ? device.GetInstallationLocation()
                : "",
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

    // ========================================================================
    // PRINT COUNT
    // ========================================================================

    static void PrintCount(
        const char* label,
        size_t value)
    {
        Serial.print(
            label
                ? label
                : "Count"
        );

        Serial.print(
            ": "
        );

        Serial.println(
            static_cast<unsigned long>(
                value
            )
        );
    }

    // ========================================================================
    // PRINT MASK
    // ========================================================================

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

// ============================================================================
// DEVICE PROFILES
// Catalogo dei profili hardware e delle relative mappe canale.
// ============================================================================
class DeviceProfiles
{
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

        add("DOMO_4DI4DO01", {
            {GenericPrgDevice::DI, GenericPrgDevice::Hold,  0, 1, 1},
            {GenericPrgDevice::DO, GenericPrgDevice::Hold, 2, 1, 1}
        });
    }
};

// ============================================================================
// IMPLEMENTAZIONE HEADER-ONLY
// ============================================================================

// ----------------------------------------------------------------------------
// Utility chiave cache: IP + priorita
// ----------------------------------------------------------------------------

inline uint64_t MakeKey(const DMIPAddress& ip, uint8_t prio)
{
    uint32_t ipKey =
        ((uint32_t)ip[0] << 24) |
        ((uint32_t)ip[1] << 16) |
        ((uint32_t)ip[2] << 8)  |
        ((uint32_t)ip[3]);

    return ((uint64_t)ipKey << 8) | prio;
}

// ----------------------------------------------------------------------------
// GenericPrgDeviceManager
// ----------------------------------------------------------------------------
inline void GenericPrgDeviceManager::BuildPriorityCache(std::vector<GenericPrgDevice>& devices) {
    if (_cacheBuilt) return;

    _cachePriorityIndex.clear();

    for (size_t i = 0; i < devices.size(); i++) {
        auto& dev = devices[i];

        uint64_t key = MakeKey(dev.GetIp(), (uint8_t)dev.GetPriority());
        _cachePriorityIndex[key].push_back(i);
    }

    _cacheBuilt = true;

    //DebugPriorityCache(devices);
}

inline void GenericPrgDeviceManager::DebugPriorityCache(
    std::vector<GenericPrgDevice>& devices)
{
    LOG_IF(
        __FUNCTION__,
        "===== PRIORITY CACHE DEBUG ====="
    );

    for (const auto& entry : _cachePriorityIndex)
    {
        const uint64_t key = entry.first;
        const auto& indices = entry.second;

        const uint32_t ipRaw =
            static_cast<uint32_t>(key >> 8);

        const uint8_t priority =
            static_cast<uint8_t>(key & 0xFF);

        const DMIPAddress ip(
            (ipRaw >> 24) & 0xFF,
            (ipRaw >> 16) & 0xFF,
            (ipRaw >> 8) & 0xFF,
            ipRaw & 0xFF
        );

        LOG_IF(
            __FUNCTION__,
            "Key=%llu | IP=%u.%u.%u.%u | Priority=%u | Devices=%u",
            static_cast<unsigned long long>(key),
            static_cast<unsigned>(ip[0]),
            static_cast<unsigned>(ip[1]),
            static_cast<unsigned>(ip[2]),
            static_cast<unsigned>(ip[3]),
            static_cast<unsigned>(priority),
            static_cast<unsigned>(indices.size())
        );

        for (const int index : indices)
        {
            if (index < 0 ||
                index >= static_cast<int>(devices.size()))
            {
                LOG_WF(
                    __FUNCTION__,
                    "Indice device non valido: %d",
                    index
                );

                continue;
            }

            const GenericPrgDevice& device =
                devices[static_cast<size_t>(index)];

            LOG_IF(
                __FUNCTION__,
                "Device[%d] | %s | Zona=%s | Installazione=%s | "
                "IP=%u.%u.%u.%u | Addr=%u | Priority=%u",
                index,
                device.GetName(),
                device.GetZone(),
                device.GetInstallationLocation(),
                static_cast<unsigned>(device.GetIp()[0]),
                static_cast<unsigned>(device.GetIp()[1]),
                static_cast<unsigned>(device.GetIp()[2]),
                static_cast<unsigned>(device.GetIp()[3]),
                static_cast<unsigned>(device.GetDeviceAddress()),
                static_cast<unsigned>(device.GetPriority())
            );
        }
    }

    LOG_IF(
        __FUNCTION__,
        "===== END PRIORITY CACHE DEBUG ====="
    );
}

inline const std::vector<int>& GenericPrgDeviceManager::GetDevicesByPriority(
    Priority priority,
    const DMIPAddress& ip) const
{
    static const std::vector<int> empty;

    const uint64_t key = MakeKey(
        ip,
        static_cast<uint8_t>(priority)
    );

    LOG_DF("CACHE::LOOKUP",
        "LOOKUP ip=%d.%d.%d.%d prio=%d key=%llu",
        ip[0], ip[1], ip[2], ip[3],
        static_cast<int>(priority),
        static_cast<unsigned long long>(key));

    const auto it = _cachePriorityIndex.find(key);

    if (it == _cachePriorityIndex.end())
        return empty;

    LOG_DF("CACHE::FOUND",
        "FOUND %d devices for key=%llu",
        static_cast<int>(it->second.size()),
        static_cast<unsigned long long>(key));

    for (int idx : it->second) {
        LOG_DF("CACHE::DEV",
            "  devIdx=%d", idx);
    }

    return it->second;
}

////////////////////////////////////////////////////////// GenericDevice
// ----------------------------------------------------------------------------
// GenericPrgDevice
// ----------------------------------------------------------------------------
inline GenericPrgDevice::GenericPrgDevice(
    const char* name,
    DMIPAddress ip,
    unsigned int deviceAddress,
    std::vector<GenericPrgDeviceChannel> channels,
    std::vector<int> ioAreas,
    short errorCount,
    Priority priority,
    const char* zone,
    const char* installationLocation)
    : _ioAreas(
          std::move(
              ioAreas
          )
      ),
      _priority(
          priority
      ),
      _name(
          name
      ),
      _zone(
          zone
      ),
      _installationLocation(
          installationLocation
      ),
      _deviceAddress(
          deviceAddress
      ),
      _ip(
          ip
      ),
      _channels(
          std::move(
              channels
          )
      ),
      Error(
          errorCount,
          30000
      ),
      bank(
          0
      )
{
}

inline int GenericPrgDevice::GetArea(
    int channel,
    int address)
{
    if (_ioAreas.empty())
    {
        LOG_EF(
            __FUNCTION__,
            "EMPTY IO AREA: ch=%d addr=%d",
            channel,
            address
        );

        return -1;
    }

    if (channel < 0 ||
        channel >=
            static_cast<int>(
                _channels.size()
            ))
    {
        LOG_EF(
            __FUNCTION__,
            "Canale fuori range: "
            "device=%s channel=%d channels=%u",
            GetName(),
            channel,
            static_cast<unsigned>(
                _channels.size()
            )
        );

        return -1;
    }

    if (address < 0 ||
        address >=
            _channels[channel].items)
    {
        LOG_EF(
            __FUNCTION__,
            "Address fuori range: "
            "device=%s channel=%d "
            "address=%d items=%d",
            GetName(),
            channel,
            address,
            _channels[channel].items
        );

        return -1;
    }

    int index =
        address;

    for (int currentChannel = 0;
         currentChannel < channel;
         ++currentChannel)
    {
        index +=
            _channels[currentChannel].items;
    }

    if (index < 0 ||
        index >=
            static_cast<int>(
                _ioAreas.size()
            ))
    {
        LOG_EF(
            __FUNCTION__,
            "Indice area fuori range: "
            "device=%s channel=%d "
            "address=%d index=%d size=%u",
            GetName(),
            channel,
            address,
            index,
            static_cast<unsigned>(
                _ioAreas.size()
            )
        );

        return -1;
    }

    return _ioAreas[index];
}

inline const GenericPrgDevice::GenericPrgDeviceChannel& GenericPrgDevice::GetChannelInfo(int channel) const
{
    static const GenericPrgDeviceChannel invalidChannel = {
        NOTSET,
        Coil,
        0,
        0,
        0
    };

    if (channel >= 0 && channel < static_cast<int>(_channels.size()))
        return _channels[channel];

    return invalidChannel;
}

inline size_t GenericPrgDevice::GetChannelsSize() const
{
    return _channels.size();
}

inline Priority GenericPrgDevice::GetPriority() const
{
    return _priority;
}

inline const DMIPAddress& GenericPrgDevice::GetIp() const
{
    return _ip;
}

inline unsigned int GenericPrgDevice::GetDeviceAddress() const
{
    return _deviceAddress;
}

inline const char* GenericPrgDevice::GetName() const
{
    if (!_name ||
        _name[0] == '\0')
    {
        return "NO name Device";
    }

    return _name;
}

inline const char* GenericPrgDevice::GetZone() const
{
    if (!_zone ||
        _zone[0] == '\0')
    {
        return "Not set";
    }

    return _zone;
}

inline const char*
GenericPrgDevice::GetInstallationLocation() const
{
    if (!_installationLocation ||
        _installationLocation[0] == '\0')
    {
        return "Not set";
    }

    return _installationLocation;
}

inline bool GenericPrgDeviceManager::HasDevicesByPriority(
    Priority priority,
    const DMIPAddress& ip) const
{
    const uint64_t key =
        MakeKey(ip, static_cast<uint8_t>(priority));

    auto it = _cachePriorityIndex.find(key);

    return it != _cachePriorityIndex.end()
        && !it->second.empty();
}

inline bool GenericPrgDevice::FindChannelByArea(
    int area,
    int& channel,
    int& item)
{
    const int channelCount =
        static_cast<int>(
            GetChannelsSize()
        );

    for (int channelIndex = 0;
         channelIndex < channelCount;
         ++channelIndex)
    {
        const GenericPrgDeviceChannel& channelInfo =
            GetChannelInfo(
                channelIndex
            );

        for (int itemIndex = 0;
             itemIndex < channelInfo.items;
             ++itemIndex)
        {
            if (GetArea(
                    channelIndex,
                    itemIndex) != area)
            {
                continue;
            }

            channel =
                channelIndex;

            item =
                itemIndex;

            return true;
        }
    }

    return false;
}

inline GenericPrgDevice::structRead GenericPrgDevice::Read(
    DMPlatform::Modbus::TCPClient& cli,
    int channel,
    uint16_t* outBuffer,
    unsigned long now)
{
    structRead result{};
    result.ok = false;

    if (!outBuffer)
    {
        LOG_EF(__FUNCTION__,
            "outBuffer NULL | Device=%s | Zona=%s | Installazione=%s",
            GetName(),
            GetZone(),
            GetInstallationLocation());

        Error.Loop(true, now);
        return result;
    }

    if (channel < 0 ||
        channel >= static_cast<int>(_channels.size()))
    {
        LOG_EF(__FUNCTION__,
            "Canale fuori range | Device=%s | Zona=%s | Installazione=%s "
            "| IP=%u.%u.%u.%u | Addr=%u | Channel=%d | Channels=%u",
            GetName(),
            GetZone(),
            GetInstallationLocation(),
            static_cast<unsigned>(_ip[0]),
            static_cast<unsigned>(_ip[1]),
            static_cast<unsigned>(_ip[2]),
            static_cast<unsigned>(_ip[3]),
            static_cast<unsigned>(_deviceAddress),
            channel,
            static_cast<unsigned>(_channels.size()));

        Error.Loop(true, now);
        return result;
    }

    const GenericPrgDeviceChannel& channelInfo =
        _channels[channel];

    result.itemsPerCall =
        channelInfo.ItemsPerCall;

    if (channelInfo.items <= 0 ||
        channelInfo.ItemsPerCall <= 0)
    {
        LOG_EF(__FUNCTION__,
            "Configurazione canale non valida | Device=%s "
            "| Channel=%d | Items=%d | ItemsPerCall=%d",
            GetName(),
            channel,
            channelInfo.items,
            channelInfo.ItemsPerCall);

        Error.Loop(true, now);
        return result;
    }

    const int bankCount =
        (channelInfo.items + MAX_CALLS - 1) /
        MAX_CALLS;

    bank %= bankCount;

    const int jump =
        bank * MAX_CALLS;

    int startingAddr =
        channelInfo.startingAddr + jump;

    const int remaining =
        channelInfo.items - jump;

    result.startIndex =
        jump;

    result.items =
        remaining < MAX_CALLS
            ? remaining
            : MAX_CALLS;

    ++bank;

    int requestedItems =
        result.items;

    int readCount =
        0;

    switch (channelInfo.hwType)
    {
        case Hold:
        {
            if (Error.IsInError())
            {
                Error.Loop(true, now);
                return result;
            }

            requestedItems *=
                channelInfo.ItemsPerCall;

            readCount = cli.requestFrom(
                _deviceAddress,
                HOLDING_REGISTERS,
                startingAddr,
                requestedItems
            );

            if (readCount <= 0)
            {
                LOG_DF(__FUNCTION__,
                    "READ HOLD fallita | Device=%s | Zona=%s "
                    "| Installazione=%s | Errore=%s",
                    GetName(),
                    GetZone(),
                    GetInstallationLocation(),
                    cli.lastError());

                Error.Loop(true, now);
                return result;
            }

            for (int i = 0; i < readCount; ++i)
            {
                const long value =
                    cli.read();

                if (value < 0)
                {
                    LOG_DF(__FUNCTION__,
                        "READ HOLD valore non valido | Device=%s | Index=%d",
                        GetName(),
                        i);

                    Error.Loop(true, now);
                    return result;
                }

                outBuffer[i] =
                    static_cast<uint16_t>(value);
            }

            Error.Loop(false, now);
            result.ok = true;
            break;
        }

        case Input:
        {
            if (Error.IsInError())
            {
                Error.Loop(true, now);
                return result;
            }

            requestedItems *=
                channelInfo.ItemsPerCall;

            readCount = cli.requestFrom(
                _deviceAddress,
                INPUT_REGISTERS,
                startingAddr,
                requestedItems
            );

            if (readCount <= 0)
            {
                if (!Error.Loop(true, now))
                {
                    LOG_DF(__FUNCTION__,
                        "READ INPUT fallita | Device=%s | Zona=%s "
                        "| Installazione=%s | IP=%u.%u.%u.%u "
                        "| Addr=%u | Errore=%s",
                        GetName(),
                        GetZone(),
                        GetInstallationLocation(),
                        static_cast<unsigned>(_ip[0]),
                        static_cast<unsigned>(_ip[1]),
                        static_cast<unsigned>(_ip[2]),
                        static_cast<unsigned>(_ip[3]),
                        static_cast<unsigned>(_deviceAddress),
                        cli.lastError());
                }

                return result;
            }

            for (int i = 0; i < readCount; ++i)
            {
                const long value =
                    cli.read();

                if (value < 0)
                {
                    Error.Loop(true, now);
                    return result;
                }

                outBuffer[i] =
                    static_cast<uint16_t>(value);
            }

            Error.Loop(false, now);
            result.ok = true;
            break;
        }

        case Discrete:
        {
            if (Error.IsInError())
            {
                Error.Loop(true, now);
                return result;
            }

            readCount = cli.requestFrom(
                _deviceAddress,
                DISCRETE_INPUTS,
                startingAddr,
                requestedItems
            );

            if (readCount <= 0)
            {
                if (!Error.Loop(true, now))
                {
                    LOG_DF(__FUNCTION__,
                        "READ DISCRETE fallita | Device=%s | Zona=%s "
                        "| Installazione=%s | IP=%u.%u.%u.%u "
                        "| Addr=%u | Errore=%s",
                        GetName(),
                        GetZone(),
                        GetInstallationLocation(),
                        static_cast<unsigned>(_ip[0]),
                        static_cast<unsigned>(_ip[1]),
                        static_cast<unsigned>(_ip[2]),
                        static_cast<unsigned>(_ip[3]),
                        static_cast<unsigned>(_deviceAddress),
                        cli.lastError());
                }

                return result;
            }

            for (int i = 0; i < readCount; ++i)
            {
                const long value =
                    cli.read();

                if (value < 0)
                {
                    Error.Loop(true, now);
                    return result;
                }

                outBuffer[i] =
                    static_cast<uint16_t>(value);
            }

            Error.Loop(false, now);
            result.ok = true;
            break;
        }

        default:
        {
            LOG_EF(__FUNCTION__,
                "Funzione READ sconosciuta | Device=%s "
                "| Zona=%s | Installazione=%s | HwType=%d",
                GetName(),
                GetZone(),
                GetInstallationLocation(),
                static_cast<int>(channelInfo.hwType));

            Error.Loop(true, now);
            break;
        }
    }

    return result;
}

inline bool GenericPrgDevice::Write(
    DMPlatform::Modbus::TCPClient& mb,
    int channel,
    int address,
    int value,
    unsigned long now)
{
    if (channel < 0 ||
        channel >= static_cast<int>(_channels.size()))
    {
        LOG_EF(
            "GenericPrgDevice",
            "Write channel out of range: name=%s channel=%d channels=%u",
            GetName(),
            channel,
            static_cast<unsigned>(_channels.size())
        );

        return false;
    }

    const GenericPrgDeviceChannel& channelInfo =
        _channels[channel];

    if (channelInfo.type != DO &&
        channelInfo.type != AO)
    {
        LOG_EF(
            "GenericPrgDevice",
            "Write definition error: DevAddr=%u StartAddr=%d Value=%d Type=%d",
            _deviceAddress,
            channelInfo.startingAddr,
            value,
            static_cast<int>(channelInfo.type)
        );

        return false;
    }

    if (address < 0 ||
        address >= channelInfo.items)
    {
        LOG_EF(
            "GenericPrgDevice",
            "Write item out of range: name=%s channel=%d item=%d items=%d",
            GetName(),
            channel,
            address,
            channelInfo.items
        );

        return false;
    }

    /*
     * address è l'indice logico dell'item nel canale.
     *
     * L'indirizzo Modbus reale deve includere:
     *   - startingAddr del canale;
     *   - ItemsPerCall per item multiregistro.
     */
    const int modbusAddress =
        channelInfo.startingAddr +
        address * channelInfo.ItemsPerCall;

    int result = 0;

    switch (channelInfo.hwType)
    {
        case Hold:
        {
            result = mb.holdingRegisterWrite(
                _deviceAddress,
                modbusAddress,
                value
            );

            LOG_DF(
                "GenericPrgDevice",
                "WRITE HOLD name=%s unit=%u channel=%d item=%d register=%d value=%d result=%d",
                GetName(),
                static_cast<unsigned>(_deviceAddress),
                channel,
                address,
                modbusAddress,
                value,
                result
            );

            if (!Error.Loop(result == 0, now))
                return false;

            return result != 0;
        }

        case Coil:
        {
            result = mb.coilWrite(
                _deviceAddress,
                modbusAddress,
                value
            );

            LOG_DF(
                "GenericPrgDevice",
                "WRITE COIL name=%s unit=%u channel=%d item=%d address=%d value=%d result=%d",
                GetName(),
                static_cast<unsigned>(_deviceAddress),
                channel,
                address,
                modbusAddress,
                value,
                result
            );

            if (!Error.Loop(result == 0, now))
                return false;

            return result != 0;
        }

        default:
        {
            LOG_EF(
                "GenericPrgDevice",
                "Unknown write type: name=%s unit=%u hwType=%d",
                GetName(),
                static_cast<unsigned>(_deviceAddress),
                static_cast<int>(channelInfo.hwType)
            );

            return false;
        }
    }
}

#endif // DMPLC_HPP
