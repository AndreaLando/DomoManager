#ifndef DMModbus_HPP
#define DMModbus_HPP

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

//Modbus Client, uso libreria ARDUINO
#include <ArduinoRS485.h> // ArduinoModbus depends on the ArduinoRS485 library
#include <ArduinoModbus.h>

#include <Arduino.h>

#include "DMPLC.h"
#include "DMNetwork.hpp"
#include "DMBaseClassUtils.hpp"


#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class ModbusManager
{
private:

    NetworkManager* net = nullptr;
    int modbusProtocolId = -1;

    // ============================================================
    // STATO CLIENT PER-IP
    // ============================================================

    enum class ClientStep
    {
        READ,
        WRITE
    };

    struct ClientStepState
    {
        ClientStep step = ClientStep::READ;
        unsigned long lastReadTime = 0;
    };

    std::vector<ClientStepState> clientStates;


    // ============================================================
    // CONNESSIONE MODBUS PERSISTENTE
    // ============================================================

    class PersistentModbusConnection
    {
    private:

        static constexpr unsigned long INACTIVITY = 600;

        ModbusTCPClient& cli;

        IPAddress lastIp;

        bool hasConnection;

        unsigned long lastActivity;

        // ========================================================
        // SOCKET MANAGER
        // ========================================================

        NetworkManager* net = nullptr;

        SocketManager::OwnerId socketOwner = -1;

        bool socketAcquired = false;


    public:

        enum class ReconnectReason
        {
            NONE,
            NOT_CONNECTED,
            IP_CHANGED,
            INACTIVITY
        };


        explicit PersistentModbusConnection(
            ModbusTCPClient& client)
            : cli(client),
            lastIp(0, 0, 0, 0),
            hasConnection(false),
            lastActivity(0),
            net(nullptr),
            socketOwner(-1),
            socketAcquired(false)
        {
        }


        // ========================================================
        // SOCKET CONTEXT
        // ========================================================

        void setSocketContext(
            NetworkManager& networkManager,
            SocketManager::OwnerId owner)
        {
            net = &networkManager;
            socketOwner = owner;
        }


        // ========================================================
        // RELEASE SOCKET
        // ========================================================

        void releaseSocket()
        {
            if (!net)
            {
                socketAcquired = false;
                return;
            }

            if (socketOwner < 0)
            {
                socketAcquired = false;
                return;
            }

            if (!socketAcquired)
                return;

            net->sockets().release(
                socketOwner,
                0,
                SocketManager::SocketKind::TCP_CLIENT
            );

            socketAcquired = false;
        }


        // ========================================================
        // ACQUIRE SOCKET
        // ========================================================

        bool acquireSocket()
        {
            if (!net)
            {
                LOG_EF(
                    "MDB::Socket",
                    "NetworkManager unavailable"
                );

                return false;
            }


            if (socketOwner < 0)
            {
                LOG_EF(
                    "MDB::Socket",
                    "Modbus socket owner invalid"
                );

                return false;
            }


            // ----------------------------------------------------
            // Già acquisita
            // ----------------------------------------------------

            if (socketAcquired)
                return true;


            // ----------------------------------------------------
            // Acquire
            // ----------------------------------------------------

            const int slot =
                net->sockets().acquire(
                    socketOwner,
                    0,
                    SocketManager::SocketKind::TCP_CLIENT
                );


            if (slot < 0)
            {
                LOG_WF(
                    "MDB::Socket",
                    "Modbus TCP socket unavailable"
                );

                return false;
            }


            socketAcquired = true;

            return true;
        }


        // ========================================================
        // TOUCH
        // ========================================================

        inline void touch(
            unsigned long now)
        {
            lastActivity = now;
        }


        // ========================================================
        // RECONNECT REASON
        // ========================================================

        inline ReconnectReason getReconnectReason(
            const IPAddress& ip,
            unsigned long now) const
        {
            if (!cli.connected())
                return ReconnectReason::NOT_CONNECTED;


            if (lastIp != ip)
                return ReconnectReason::IP_CHANGED;


            if (now - lastActivity > INACTIVITY)
                return ReconnectReason::INACTIVITY;


            return ReconnectReason::NONE;
        }


        // ========================================================
        // ENSURE CONNECTION
        // ========================================================

        bool ensure(
            int ipIndex,
            IpManager& ipManager,
            int port,
            unsigned long now)
        {
            auto& ips =
                ipManager.GetIps();


            // ----------------------------------------------------
            // VALID IP INDEX
            // ----------------------------------------------------

            if (ipIndex < 0 ||
                static_cast<size_t>(ipIndex) >= ips.size())
            {
                LOG_EF(
                    "MDB::ensure",
                    "INVALID ipIndex=%d",
                    ipIndex
                );

                return false;
            }


            auto& ipStruct =
                ips[ipIndex];


            const IPAddress ip =
                ipStruct.IP;


            // ----------------------------------------------------
            // SHOULD QUERY
            // ----------------------------------------------------

            if (!ipManager.ShouldQuery(
                    ipIndex,
                    now))
            {
                hasConnection = false;

                return false;
            }


            // ----------------------------------------------------
            // RECONNECT REASON
            // ----------------------------------------------------

            const ReconnectReason reason =
                getReconnectReason(
                    ip,
                    now
                );


            // ----------------------------------------------------
            // CONNESSIONE ANCORA VALIDA
            // ----------------------------------------------------

            if (reason == ReconnectReason::NONE)
            {
                hasConnection = true;

                // In teoria socketAcquired deve essere true.
                // Se per qualunque motivo non lo fosse,
                // riallineiamo il tracker.
                if (!socketAcquired)
                {
                    if (!acquireSocket())
                    {
                        hasConnection = false;

                        return false;
                    }
                }


                if (ipStruct.state !=
                    IpManager::IpState::OK)
                {
                    ipManager.ReportSuccess(
                        ipIndex
                    );
                }


                return true;
            }


            // ====================================================
            // CONNESSIONE DA CHIUDERE
            // ====================================================

            if (reason == ReconnectReason::IP_CHANGED ||
                reason == ReconnectReason::INACTIVITY)
            {
                if (cli.connected())
                {
                    cli.stop();
                }

                hasConnection = false;

                releaseSocket();
            }


            // ====================================================
            // NOT CONNECTED
            // ====================================================

            if (reason == ReconnectReason::NOT_CONNECTED)
            {
                hasConnection = false;

                // Nel caso la socket sia caduta esternamente,
                // allineiamo il SocketManager.
                releaseSocket();
            }


            // ====================================================
            // ACQUIRE PRIMA DEL CONNECT
            // ====================================================

            if (!acquireSocket())
            {
                hasConnection = false;

                return false;
            }


            // ====================================================
            // CONNECT
            // ====================================================

            const bool connected =
                cli.begin(
                    ip,
                    port
                );


            if (!connected)
            {
                hasConnection = false;

                // Il connect non è riuscito:
                // la reservation non deve rimanere occupata.
                releaseSocket();


                ipManager.ReportError(
                    ipIndex,
                    now
                );


                return false;
            }


            // ====================================================
            // CONNECT SUCCESS
            // ====================================================

            lastIp =
                ip;


            lastActivity =
                now;


            hasConnection =
                true;


            if (ipStruct.state !=
                IpManager::IpState::OK)
            {
                ipManager.ReportSuccess(
                    ipIndex
                );
            }
            
            return true;
        }
    };


    // ============================================================
    // AREA → DEVICE MAP
    // ============================================================

    class AreaDeviceMap
    {
    public:

        struct Entry
        {
            int devIndex;
            int channel;
            int itemIndex;
        };


        inline void buildOnce(
            std::vector<GenericPrgDevice>& devices)
        {
            if (initialized)
                return;

            buildInternal(devices);
            initialized = true;
        }


        inline bool find(
            int area,
            Entry& out) const
        {
            const auto it =
                map.find(area);

            if (it == map.end())
                return false;

            out = it->second;
            return true;
        }


        inline bool isInitialized() const
        {
            return initialized;
        }


    private:

        std::unordered_map<int, Entry> map;
        bool initialized = false;


        void buildInternal(
            std::vector<GenericPrgDevice>& devices)
        {
            map.clear();

            map.reserve(
                devices.size() * 8
            );

            for (int d = 0;
                 d < static_cast<int>(devices.size());
                 ++d)
            {
                auto& dev = devices[d];

                const int channels =
                    dev.GetChannelsSize();

                for (int ch = 0;
                     ch < channels;
                     ++ch)
                {
                    const auto chInfo =
                        dev.GetChannelInfo(ch);

                    const int items =
                        chInfo.items;

                    for (int i = 0;
                         i < items;
                         ++i)
                    {
                        const int area =
                            dev.GetArea(ch, i);

                        map[area] = Entry{
                            d,
                            ch,
                            i
                        };
                    }
                }
            }
        }
    };


    // ============================================================
    // INTERNAL PACING
    // ============================================================

    inline bool InternalPacingCheck(
        IpManager& ipManager,
        int ipIndex,
        unsigned long startTime,
        int deviceIndex,
        int items,
        unsigned long now)
    {
        if (!net)
            return false;

        auto& protocol =
            net->getProtocol(modbusProtocolId);

        if (!protocol.pacingEnabled)
            return false;

        const unsigned long slotDuration =
            net->getSlotDuration(modbusProtocolId);

        if (now - startTime <= slotDuration)
            return false;


        LOG_WF(
            "INTERNAL PACING",
            "SlotDuration superato: ip=%d devIdx=%d",
            ipIndex,
            deviceIndex
        );


        net->onSlotDurationExceeded(
            modbusProtocolId,
            now
        );


        if (protocol.safeMode)
        {
            LOG_WF(
                "INTERNAL PACING::SafeMode",
                "Bypass interruzione → pacing troppo stretto (ip=%d)",
                ipIndex
            );

            return false;
        }


        ipManager.UpdatePriorityAfterRead(
            ipIndex,
            true,
            deviceIndex,
            items
        );

        return true;
    }


    // ============================================================
    // PERFORMANCE
    // ============================================================

    DriverTimingStats timing;

    PerformanceProfiler profiler{5, 8};

    uint8_t PROF_ENSURE;
    uint8_t PROF_READ;
    uint8_t PROF_PROCESS;
    uint8_t PROF_WRITE;


    // ============================================================
    // WRITE PENDING PER IP
    // ============================================================

    inline bool hasWriteForIp(int ipIndex)
    {
        auto& buf  = *buffer;
        auto& devs = *prgDevices;

        const auto& targetIp =
            ipManager->GetIps()[ipIndex].IP;

        const auto changed =
            buf.getChangedMap(true);

        if (changed.empty())
            return false;


        for (const auto& area : changed)
        {
            AreaDeviceMap::Entry entry;

            if (!areaMap.find(area, entry))
                continue;


            const int devIdx =
                entry.devIndex;

            if (devIdx < 0 ||
                devIdx >= static_cast<int>(devs.size()))
            {
                continue;
            }


            auto& dev =
                devs[devIdx];


            if (dev.GetIp() != targetIp)
                continue;


            // Il pending rimane nel Buffer,
            // ma un device in errore non deve
            // monopolizzare lo scheduler.
            if (dev.GetError().IsInError())
                continue;


            const auto chType =
                dev.GetChannelInfo(
                    entry.channel
                ).type;


            if (chType == GenericPrgDevice::DO ||
                chType == GenericPrgDevice::AO)
            {
                return true;
            }
        }

        return false;
    }


public:

    // ============================================================
    // TIMING
    // ============================================================

    inline void reportDeviceTiming()
    {
        timing.reportIpTiming(
            "Modbus Device Cycle"
        );
    }


    // ============================================================
    // MAP
    // ============================================================

    AreaDeviceMap areaMap;

  
    // ============================================================
    // CALLBACK
    // ============================================================

    using SomethingChangedFn =
        void (*)();


    using FieldChangedCallback =
        void (*)(int area, long value);


    using ReadAreaPolicyFn =
        bool (*)(int area,
                 long value,
                 Buffer& buffer);


    // ============================================================
    // RIFERIMENTI ESTERNI
    // ============================================================

    Buffer* buffer = nullptr;

    std::vector<GenericPrgDevice>* prgDevices =
        nullptr;

    AnalogThresholdManager* thresholds =
        nullptr;

    IpManager* ipManager =
        nullptr;


    // ============================================================
    // CALLBACK INSTANCE
    // ============================================================

    SomethingChangedFn somethingChanged =
        nullptr;

    FieldChangedCallback fieldChangedCallback =
        nullptr;

    ReadAreaPolicyFn readAreaPolicy =
        nullptr;

    // ============================================================
    // LED
    // ============================================================

    LedController* m_ledController =
        nullptr;


    // ============================================================
    // INIT
    // ============================================================

    void Begin(
        LedController& ledsController,
        Buffer& buffer,
        std::vector<GenericPrgDevice>& prgDevices,
        IpManager& ipManager,
        AnalogThresholdManager& tresholds,
        SomethingChangedFn sc,
        NetworkManager& netManager,
        int modbusId)
    {
        this->m_ledController = &ledsController;
        this->buffer         = &buffer;
        this->prgDevices     = &prgDevices;
        this->thresholds     = &tresholds;
        this->ipManager      = &ipManager;
        this->somethingChanged = sc;
        this->net              = &netManager;
        this->modbusProtocolId = modbusId;


        // Stato READ/WRITE indipendente per ogni IP.
        clientStates.clear();
        clientStates.resize(
            ipManager.GetIps().size()
        );


        // Costruzione mappa area → device.
        areaMap.buildOnce(
            prgDevices
        );


        // Performance profiler.
        PROF_ENSURE =
            profiler.addStage("ENS");

        PROF_READ =
            profiler.addStage("READ");

        PROF_PROCESS =
            profiler.addStage("PROC");

        PROF_WRITE =
            profiler.addStage("WRITE");
    }


    // ============================================================
    // CLIENT STATE
    // ============================================================

    enum class ClientState
    {
        READ_DONE,
        WRITE_DONE,
        CYCLE_OK,
        DEVICE_ERROR,
        ERROR
    };


    // ============================================================
    // STATE MAPPING
    // ============================================================

    inline NetworkManager::Protocol::State
    mapClientState(ClientState state)
    {
        using CS =
            ModbusManager::ClientState;

        using PS =
            NetworkManager::Protocol::State;

        switch (state)
        {
            case CS::CYCLE_OK:
                return PS::CYCLE_OK;

            case CS::DEVICE_ERROR:
                return PS::DEVICE_ERROR;

            case CS::ERROR:
                return PS::ERROR;

            case CS::WRITE_DONE:
                return PS::WRITE_DONE;

            case CS::READ_DONE:
                return PS::READ_DONE;

            default:
                return PS::ERROR;
        }
    }


    // ============================================================
    // CALLBACK SETTERS
    // ============================================================

    inline void setFieldChangedCallback(
        FieldChangedCallback callback)
    {
        fieldChangedCallback = callback;
    }


    inline void setReadAreaPolicy(
        ReadAreaPolicyFn callback)
    {
        readAreaPolicy = callback;
    }


    // ============================================================
    // RUN CLIENT
    // ============================================================

    ClientState RunClient(
        ModbusTCPClient& cli,
        short ipIndex,
        int port,
        std::vector<uint16_t>& mbRead,
        unsigned long now)
    {
        // --------------------------------------------------------
        // VALIDAZIONE IP
        // --------------------------------------------------------

        if (ipIndex < 0 ||
            static_cast<size_t>(ipIndex) >= clientStates.size())
        {
            LOG_EF(
                "ModbusManager",
                "RunClient INVALID ipIndex=%d states=%d",
                ipIndex,
                (int)clientStates.size()
            );

            return ClientState::ERROR;
        }


        ClientStepState& clientState =
            clientStates[ipIndex];

        ClientStep& step =
            clientState.step;

        unsigned long& lastReadTime =
            clientState.lastReadTime;


         // --------------------------------------------------------
        // CONNESSIONE MODBUS PERSISTENTE
        // --------------------------------------------------------

        static PersistentModbusConnection conn(cli);


        // --------------------------------------------------------
        // SOCKET MANAGER CONTEXT
        // --------------------------------------------------------

        if (net)
        {
            conn.setSocketContext(
                *net,
                net->getProtocol(
                    modbusProtocolId
                ).socketOwner
            );
        }


        auto& ip =
            ipManager->GetIps()[ipIndex];


        // --------------------------------------------------------
        // ENSURE CONNECTION
        // --------------------------------------------------------

        profiler.begin(
            PROF_ENSURE,
            ipIndex,
            millis()
        );

        const bool ensureOk =
            conn.ensure(
                ipIndex,
                *ipManager,
                port,
                now
            );

        profiler.end(
            PROF_ENSURE,
            ipIndex,
            millis()
        );


        if (!ensureOk)
        {
            LOG_WF(
                "ModbusManager",
                "Modbus TCP non connesso → salto lettura "
                "(IP=%d.%d.%d.%d)",
                ip.IP[0],
                ip.IP[1],
                ip.IP[2],
                ip.IP[3]
            );

            step = ClientStep::READ;

            if (net)
            {
                net->updateProtocolState(
                    modbusProtocolId,
                    NetworkManager::Protocol::State::ERROR
                );
            }

            return ClientState::ERROR;
        }


        // ========================================================
        // READ
        // ========================================================

        if (step == ClientStep::READ)
        {
            LOG_DF(
                "MDB",
                "READ phase start (ip=%d)",
                ipIndex
            );


            timing.startDevice(now);


            profiler.begin(
                PROF_READ,
                ipIndex,
                millis()
            );

            const bool readOk =
                DeviceRead(
                    cli,
                    ipIndex,
                    mbRead,
                    now
                );

            profiler.end(
                PROF_READ,
                ipIndex,
                millis()
            );


            if (!readOk)
            {
                step = ClientStep::READ;

                if (net)
                {
                    net->updateProtocolState(
                        modbusProtocolId,
                        NetworkManager::Protocol::State::DEVICE_ERROR
                    );
                }

                return ClientState::DEVICE_ERROR;
            }


            conn.touch(now);
            lastReadTime = now;


            // ----------------------------------------------------
            // PROCESS BUFFER
            // ----------------------------------------------------

            profiler.begin(
                PROF_PROCESS,
                ipIndex,
                millis()
            );


            const auto& changedMap =
                buffer->getChangedMap();


            for (const auto& area : changedMap)
            {
                const BufferSourceInfo& entry =
                    buffer->getFieldEntry(area);

                const uint16_t value =
                    static_cast<uint16_t>(entry.value);


                const int areaToWrite =
                    buffer->GetAreaToWrite(area);

                if (areaToWrite > 0)
                {
                    buffer->WriteElement(
                        areaToWrite,
                        value,
                        now
                    );
                }
            }


            if (!changedMap.empty() &&
                somethingChanged)
            {
                somethingChanged();
            }


            profiler.end(
                PROF_PROCESS,
                ipIndex,
                millis()
            );


            // Passa alla WRITE.
            step = ClientStep::WRITE;


            if (net)
            {
                net->updateProtocolState(
                    modbusProtocolId,
                    NetworkManager::Protocol::State::READ_DONE
                );
            }


            return ClientState::READ_DONE;
        }


        // ========================================================
        // WRITE
        // ========================================================

        if (step == ClientStep::WRITE)
        {
            constexpr unsigned long fixedDelay = 2;


            if (now - lastReadTime < fixedDelay)
            {
                if (net)
                {
                    net->updateProtocolState(
                        modbusProtocolId,
                        NetworkManager::Protocol::State::WRITE_DONE
                    );
                }

                return ClientState::WRITE_DONE;
            }


            profiler.begin(
                PROF_WRITE,
                ipIndex,
                millis()
            );


            const bool writeOk =
                DeviceWrite(
                    cli,
                    ip.IP,
                    now
                );


            profiler.end(
                PROF_WRITE,
                ipIndex,
                millis()
            );


            if (!writeOk)
            {
                // Il ciclo corrente è terminato con errore.
                // La pending rimasta nel Buffer verrà ritentata
                // successivamente.
                step = ClientStep::READ;

                if (net)
                {
                    net->updateProtocolState(
                        modbusProtocolId,
                        NetworkManager::Protocol::State::DEVICE_ERROR
                    );
                }

                return ClientState::DEVICE_ERROR;
            }


            timing.endDevice(
                now,
                ipIndex
            );


            ipManager->setLastCycleDuration(
                ipIndex,
                now - lastReadTime
            );


            conn.touch(now);

            step = ClientStep::READ;


            if (net)
            {
                net->updateProtocolState(
                    modbusProtocolId,
                    NetworkManager::Protocol::State::CYCLE_OK
                );
            }


            return ClientState::CYCLE_OK;
        }


        if (net)
        {
            net->updateProtocolState(
                modbusProtocolId,
                NetworkManager::Protocol::State::ERROR
            );
        }


        return ClientState::ERROR;
    }


    // ============================================================
    // AREA MAP DIAGNOSTICS
    // ============================================================

    void DumpAreaMap()
    {
        if (!buffer || !prgDevices)
        {
            LOG_WF(
                "AreaMap",
                "DumpAreaMap: buffer o prgDevices non inizializzati"
            );

            return;
        }


        LOG_WF(
            "AreaMap",
            "===== DUMP AREA → DEVICE MAP ====="
        );


        const int totalAreas =
            static_cast<int>(buffer->size());


        for (int area = 0;
             area < totalAreas;
             ++area)
        {
            AreaDeviceMap::Entry entry;

            if (!areaMap.find(area, entry))
            {
                LOG_WF(
                    "AreaMap",
                    "area=%d NOT MAPPED",
                    area
                );

                continue;
            }


            auto& dev =
                (*prgDevices)[entry.devIndex];

            const auto ip =
                dev.GetIp();


            LOG_WF(
                "AreaMap",
                "area=%d devIndex=%d channel=%d "
                "itemIndex=%d devName=%s "
                "ip=%d.%d.%d.%d",
                area,
                entry.devIndex,
                entry.channel,
                entry.itemIndex,
                dev.GetName(),
                ip[0],
                ip[1],
                ip[2],
                ip[3]
            );
        }


        LOG_WF(
            "AreaMap",
            "===== END DUMP ====="
        );
    }


    // ============================================================
    // PENDING WRITE PUBLIC API
    // ============================================================

    inline bool hasPendingWritesForIp(
        int ipIndex)
    {
        return hasWriteForIp(ipIndex);
    }


private:

    // ============================================================
    // DEVICE READ
    // ============================================================

    inline bool DeviceRead(
        ModbusTCPClient& cli,
        short ipIndex,
        std::vector<uint16_t>& mbRead,
        unsigned long now)
    {
        return DeviceManagement_Read(
            m_ledController,
            cli,
            *ipManager,
            ipIndex,
            *buffer,
            *prgDevices,
            *thresholds,
            now,
            mbRead,
            millis()
        );
    }


    // ============================================================
    // DEVICE WRITE
    // ============================================================

    inline bool DeviceWrite(
        ModbusTCPClient& cli,
        arduino::IPAddress ip,
        unsigned long now)
    {
        return DeviceManagement_Write(
            m_ledController,
            cli,
            ip,
            *buffer,
            *prgDevices,
            now
        );
    }


    // ============================================================
    // DEVICE MANAGEMENT WRITE
    // ============================================================

    bool DeviceManagement_Write(
        LedController* ledsController,
        ModbusTCPClient& modbusTCPCli,
        arduino::IPAddress ip,
        Buffer& buffer,
        std::vector<GenericPrgDevice>& prgDevices,
        unsigned long now)
    {
        bool inError = false;


        // --------------------------------------------------------
        // DEVICE ASSOCIATI ALL'IP
        // --------------------------------------------------------

        auto devicesForIP =
            ipManager->GetDevicesByIP(ip);

        if (!devicesForIP ||
            devicesForIP->empty())
        {
            if (ledsController &&
                ledsController->hasChannel(
                    LedController::TWO))
            {
                ledsController->set(
                    LedController::TWO,
                    false
                );
            }

            return true;
        }


        // --------------------------------------------------------
        // CHANGED MAP
        // --------------------------------------------------------

        const auto& changedMap =
            buffer.getChangedMap(true);

        if (changedMap.empty())
        {
            if (ledsController &&
                ledsController->hasChannel(
                    LedController::TWO))
            {
                ledsController->set(
                    LedController::TWO,
                    false
                );
            }

            return true;
        }


        // --------------------------------------------------------
        // DEVICE LOOKUP PER IP
        // --------------------------------------------------------

        static std::unordered_set<int> devSet;

        devSet.clear();

        for (const int idx : *devicesForIP)
            devSet.insert(idx);


        // --------------------------------------------------------
        // WRITE
        // --------------------------------------------------------

        bool done = false;


        for (const auto& area : changedMap)
        {
            AreaDeviceMap::Entry entry;


            if (!areaMap.find(area, entry))
            {
                LOG_WF(
                    "ModbusManager",
                    "areaMap not found → area=%d",
                    area
                );

                continue;
            }


            if (!devSet.count(entry.devIndex))
                continue;


            GenericPrgDevice& dev =
                prgDevices[entry.devIndex];


            // ----------------------------------------------------
            // DEVICE IN ERRORE
            // ----------------------------------------------------

            if (dev.GetError().IsInError())
            {
                dev.GetError().Loop(
                    true,
                    now
                );

                LOG_WF(
                    "WRITE",
                    "Skip WRITE (device in error): %s "
                    "area=%d IP=%d.%d.%d.%d",
                    dev.GetName(),
                    area,
                    dev.GetIp()[0],
                    dev.GetIp()[1],
                    dev.GetIp()[2],
                    dev.GetIp()[3]
                );

                // IMPORTANTISSIMO:
                // la pending NON viene cancellata.
                continue;
            }


            // ----------------------------------------------------
            // SOLO DO / AO
            // ----------------------------------------------------

            const auto chType =
                dev.GetChannelInfo(
                    entry.channel
                ).type;


            if (chType != GenericPrgDevice::DO &&
                chType != GenericPrgDevice::AO)
            {
                continue;
            }


            // ----------------------------------------------------
            // VALORE
            // ----------------------------------------------------

            const BufferSourceInfo& info =
                buffer.getFieldEntry(area);


            // ----------------------------------------------------
            // WRITE MODBUS
            // ----------------------------------------------------

            const bool writeOk =
                dev.Write(
                    modbusTCPCli,
                    entry.channel,
                    entry.itemIndex,
                    info.value,
                    now
                ) != 0;


            if (!writeOk)
            {
                inError = true;

                LOG_EF(
                    "ModbusManager",
                    "FAIL to WRITE → area=%d "
                    "channel=%d itemIndex=%d value=%d",
                    area,
                    entry.channel,
                    entry.itemIndex,
                    info.value
                );

                // NON resettiamo:
                // retry successivo.
                continue;
            }


            // ----------------------------------------------------
            // WRITE CONSUMATA
            // ----------------------------------------------------

            buffer.ResetElement(area);

            done = true;
        }


        // --------------------------------------------------------
        // LED
        // --------------------------------------------------------

        if (ledsController &&
            ledsController->hasChannel(
                LedController::TWO))
        {
            static bool ledState = false;

            if (done)
                ledState = !ledState;
            else
                ledState = false;

            ledsController->set(
                LedController::TWO,
                ledState
            );
        }


        return !inError;
    }


    // ============================================================
    // READ SET OUT
    // ============================================================

    void DeviceManagement_Read_SetOut(
        Buffer& buffer,
        int area,
        long value,
        unsigned long now,
        bool skipForward = false)
    {
        // Scrittura reale della sorgente.
        buffer.WriteElement(
            area,
            value,
            now
        );


        // Evento verso EventManager / consumer.
        if (fieldChangedCallback)
        {
            fieldChangedCallback(
                area,
                value
            );
        }


        // Toggle / Split / altri consumer:
        // niente forward automatico.
        if (skipForward)
            return;


        // --------------------------------------------------------
        // FORWARD AUTOMATICO
        // --------------------------------------------------------

        const int outArea =
            buffer.GetAreaToWrite(area);


        if (outArea >= 0)
        {
            buffer.WriteElement(
                outArea,
                value,
                now
            );


            if (fieldChangedCallback)
            {
                fieldChangedCallback(
                    outArea,
                    value
                );
            }


            LOG_WF(
                "ModbusManager",
                "Forward: area=%d areaToWrite=%d value=%d",
                area,
                outArea,
                value
            );
        }
        else if (outArea != Buffer::NO_AREA)
        {
            LOG_WF(
                "ModbusManager",
                "Skip forward: area=%d areaToWrite=%d",
                area,
                outArea
            );
        }
    }


    // ============================================================
    // DEVICE READ PLANNER
    // ============================================================

    class DeviceReadPlanner
    {
    public:

        DeviceReadPlanner()
            : cacheBuilt(false)
        {
        }


        inline void buildCacheIfNeeded(
            std::vector<GenericPrgDevice>& prgDevices)
        {
            if (cacheBuilt)
                return;

            manager.BuildPriorityCache(
                prgDevices
            );

            cacheBuilt = true;
        }


        inline const GenericPrgDeviceManager&
        getDeviceManager() const
        {
            return manager;
        }


        inline const std::vector<int>&
        getDevicesForIp(
            PriorityMgmt& priority,
            IpManager& ipManager,
            short ipIndex)
        {
            return manager.GetDevicesByPriority(
                priority.priority,
                ipManager.GetIp(ipIndex).IP
            );
        }


        struct Range
        {
            int start;
            int end;
        };


        Range computeRange(
            IpManager::PriorityMgmtEx& priorityEx,
            int items,
            int ipIndex,
            IpManager& ipManager)
        {
            Range r{0, items};


            // ----------------------------------------------------
            // HIGH
            // ----------------------------------------------------

            if (priorityEx.Base.priority == High)
            {
                return r;
            }


            auto& cursor =
                priorityEx.Cursor;

            cursor.init();


            // ----------------------------------------------------
            // MEDIUM
            // ----------------------------------------------------

            if (priorityEx.Base.priority == Medium)
            {
                const unsigned long lastCycle =
                    ipManager.getLastCycleDuration(
                        ipIndex
                    );

                int count;

                if (lastCycle > 60)
                    count = 1;
                else if (lastCycle < 30)
                    count = 3;
                else
                    count = 2;


                const int start =
                    cursor.get();

                r.start = start;
                r.end = min(
                    start + count,
                    items
                );

                cursor.advance(items);

                return r;
            }


            // ----------------------------------------------------
            // NORMAL
            // ----------------------------------------------------

            if (priorityEx.Base.priority == Normal)
            {
                const unsigned long lastCycle =
                    ipManager.getLastCycleDuration(
                        ipIndex
                    );

                int count;

                if (lastCycle > 60)
                    count = 2;
                else if (lastCycle < 30)
                    count = 4;
                else
                    count = 3;


                const int start =
                    cursor.get();

                r.start = start;
                r.end = min(
                    start + count,
                    items
                );

                cursor.advance(items);

                return r;
            }


            // ----------------------------------------------------
            // LOW
            // ----------------------------------------------------

            const int start =
                cursor.get();

            r.start = start;
            r.end = min(
                start + 1,
                items
            );

            cursor.advance(items);

            if (r.start < 0)
                r.start = 0;

            if (r.end > items)
                r.end = items;

            return r;
        }


    private:

        GenericPrgDeviceManager manager;
        bool cacheBuilt;
    };


    // ============================================================
    // DEVICE MANAGEMENT READ
    // ============================================================

    bool DeviceManagement_Read(
        LedController* ledsController,
        ModbusTCPClient& modbusTCPCli,
        IpManager& ipManager,
        short ipIndex,
        Buffer& buffer,
        std::vector<GenericPrgDevice>& prgDevices,
        AnalogThresholdManager& thresholds,
        unsigned long now,
        std::vector<uint16_t>& mbRead,
        unsigned long slotStart)
    {
        bool error = false;


        static DeviceReadPlanner planner;

        planner.buildCacheIfNeeded(
            prgDevices
        );


        // --------------------------------------------------------
        // PRIORITÀ
        // --------------------------------------------------------

        auto& prioEx =
            ipManager.GetCurrentPriority(
                ipIndex,
                prgDevices,
                planner.getDeviceManager()
            );

        PriorityMgmt& actualPriority =
            prioEx.Base;


        // --------------------------------------------------------
        // DEVICE DELL'IP
        // --------------------------------------------------------

        const std::vector<int>& devices =
            planner.getDevicesForIp(
                actualPriority,
                ipManager,
                ipIndex
            );


        const int items =
            static_cast<int>(
                devices.size()
            );


        if (items <= 0)
        {
            LOG_EF(
                "ModbusManager",
                "Nothing to read | priority=%d | "
                "ip=%d.%d.%d.%d",
                actualPriority.priority,
                ipManager.GetIp(ipIndex).IP[0],
                ipManager.GetIp(ipIndex).IP[1],
                ipManager.GetIp(ipIndex).IP[2],
                ipManager.GetIp(ipIndex).IP[3]
            );

            return true;
        }


        // --------------------------------------------------------
        // LED READ
        // --------------------------------------------------------

        if (ledsController &&
            ledsController->hasChannel(
                LedController::ONE))
        {
            static bool ledState = false;

            ledState = !ledState;

            ledsController->set(
                LedController::ONE,
                ledState
            );
        }


        // --------------------------------------------------------
        // RANGE
        // --------------------------------------------------------

        const auto range =
            planner.computeRange(
                prioEx,
                items,
                ipIndex,
                ipManager
            );


        // --------------------------------------------------------
        // RESUME PACING
        // --------------------------------------------------------

        int startIdx;

        if (prioEx.Interrupted)
        {
            startIdx =
                prioEx.Base.deviceIndex;

            if (startIdx < range.start)
                startIdx = range.start;

            if (startIdx >= range.end)
                startIdx = range.end - 1;
        }
        else
        {
            startIdx =
                range.start;
        }


        // --------------------------------------------------------
        // DEVICE LOOP
        // --------------------------------------------------------

        for (int deviceIndex = startIdx;
             deviceIndex < range.end;
             ++deviceIndex)
        {
            auto& dev =
                prgDevices[
                    devices[deviceIndex]
                ];


            // ----------------------------------------------------
            // DEVICE IN ERRORE
            // ----------------------------------------------------

            if (dev.GetError().IsInError())
            {
                dev.GetError().Loop(
                    true,
                    now
                );

                if (prioEx.Interrupted)
                {
                    prioEx.Cursor.save(
                        deviceIndex
                    );
                }

                continue;
            }


            // ----------------------------------------------------
            // PACING
            // ----------------------------------------------------

            if (InternalPacingCheck(
                    ipManager,
                    ipIndex,
                    slotStart,
                    deviceIndex,
                    items,
                    now))
            {
                return true;
            }


            #ifdef DEBUG_VIEW

                        LOG_IF(
                            "MDB::READ",
                            "Device %d: %s IP=%d.%d.%d.%d PRIOR=%d",
                            deviceIndex,
                            dev.GetName(),
                            dev.GetIp()[0],
                            dev.GetIp()[1],
                            dev.GetIp()[2],
                            dev.GetIp()[3],
                            (int)dev.GetPriority()
                        );

                        delay(500);

            #endif


            // ----------------------------------------------------
            // CHANNELS
            // ----------------------------------------------------

            const int channels =
                dev.GetChannelsSize();


            for (int channel = 0;
                 channel < channels;
                 ++channel)
            {
                const auto chInfo =
                    dev.GetChannelInfo(channel);


                if (chInfo.type != GenericPrgDevice::DI &&
                    chInfo.type != GenericPrgDevice::AI)
                {
                    continue;
                }


                // ------------------------------------------------
                // MODBUS READ
                // ------------------------------------------------

                const auto read =
                    dev.Read(
                        modbusTCPCli,
                        channel,
                        mbRead.data(),
                        now
                    );


                if (!read.ok)
                {
                    LOG_DF(
                        "READ::ERR",
                        "Errore lettura → ip=%d "
                        "devIdx=%d addr=%d → ciclo si ferma",
                        ipIndex,
                        devices[deviceIndex],
                        dev.GetDeviceAddress()
                    );

                    error = true;
                    break;
                }


                // ------------------------------------------------
                // PROCESS READ DATA
                // ------------------------------------------------

                for (int j = 0;
                     j < read.items;
                     ++j)
                {
                    const int index =
                        read.startIndex + j;

                    const int area =
                        dev.GetArea(
                            channel,
                            index
                        );


                    const BufferSourceInfo& bufferEntry =
                        buffer.getFieldEntry(area);


                    long value;


                    // ------------------------------------------------
                    // DIGITAL / ANALOG
                    // ------------------------------------------------

                    if (read.itemsPerCall == 1)
                    {
                        value =
                            mbRead[j];
                    }
                    else
                    {
                        const uint32_t raw =
                            (static_cast<uint32_t>(mbRead[j]) << 16) |
                            mbRead[j + 1];

                        float f;

                        memcpy(
                            &f,
                            &raw,
                            sizeof(float)
                        );

                        value =
                            f < 0
                                ? 0
                                : static_cast<unsigned long>(
                                      f * 100
                                  );
                    }


                    bool process = false;


                    if (chInfo.type ==
                        GenericPrgDevice::AI)
                    {
                        const int threshold =
                            thresholds.getThreshold(
                                area,
                                value
                            );

                        process =
                            abs(
                                value -
                                bufferEntry.value
                            ) > threshold;

                        process =
                            process ||
                            (
                                bufferEntry.value == 0 &&
                                value != 0
                            );
                    }
                    else
                    {
                        if (buffer.IsReverse(area))
                            value = !value;

                        process =
                            value !=
                            bufferEntry.value;
                    }


                    if (!process)
                        continue;


                    // ------------------------------------------------
                    // LOG EVENTO REALE
                    // ------------------------------------------------

                    if (value > 0 &&
                        area >= 10)
                    {
                        const int devIdx =
                            devices[deviceIndex];

                        LOG_IF(
                            "ModbusManager",
                            "-> %s | channel=%d | index=%d "
                            "| value=%ld | buf.val=%ld "
                            "| buf.prev=%ld | area=%d | areaName=%s",
                            prgDevices[devIdx].GetName(),
                            channel,
                            index,
                            value,
                            bufferEntry.value,
                            bufferEntry.prevValue,
                            area,
                            buffer.GetName(area)
                        );
                    }


                    // ------------------------------------------------
                    // CONSUMER POLICY
                    // ------------------------------------------------

                    const bool skipForward =
                        readAreaPolicy &&
                        readAreaPolicy(
                            area,
                            value,
                            buffer
                        );


                    // ------------------------------------------------
                    // BUFFER + EVENT
                    // ------------------------------------------------

                    DeviceManagement_Read_SetOut(
                        buffer,
                        area,
                        value,
                        now,
                        skipForward
                    );


                    // ------------------------------------------------
                    // AREA GESTITA DA CONSUMER:
                    // NON DEVE RESTARE PENDING COME SORGENTE
                    // ------------------------------------------------

                    if (skipForward)
                    {
                        buffer.ResetElement(area);
                    }


                    // ------------------------------------------------
                    // WRITE LOCALE IMMEDIATA
                    // ------------------------------------------------

                    if (hasWriteForIp(ipIndex))
                    {
                        LOG_WF(
                            "ModbusManager",
                            "WRITE locale rilevata durante READ "
                            "→ interrompo DeviceRead (ip=%d)",
                            ipIndex
                        );

                        return true;
                    }
                }
            }


            if (error)
                break;
        }


        // --------------------------------------------------------
        // CICLO LETTURA COMPLETATO
        // --------------------------------------------------------

        if (!prioEx.Interrupted)
        {
            ipManager.UpdatePriorityAfterRead(
                ipIndex,
                false,
                -1,
                items
            );
        }


        return !error;
    }
};

#endif
