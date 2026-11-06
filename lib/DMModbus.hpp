#ifndef DMModbus_HPP
#define DMModbus_HPP

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.1
   Ultima modifica: 2026‑09‑11
   Note:
    • Effettuati affinamenti sulla performance

   ============================================================================ */


#include <Arduino.h>

#include "DMPLC.h"
#include "DMNetwork.hpp"
#include "DMBaseClassUtils.hpp"
#include "DMPlatformModbus.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class IpManager {
private:
    class PriorityDeviceCursor {
    public:
        int index = -1;

        void init() {
            if (index < 0)
                index = 0;
        }

        void advance(int items) {
            index++;
            if (index >= items)
                index = 0;
        }

        void save(int deviceIndex) {
            index = deviceIndex;
        }

        int get() const {
            return index;
        }
    };
public:
    struct Config {
        unsigned long baseCooldownMs = 5000;      // ex COOLDOWN
        unsigned long extendedCooldownMs = 120000; // ex IP_COOLDOWN
        int maxErrorsBeforeCooldown = 2;          // ex MAX_ERRORS
        int maxErrorsBeforeExclude = 20;          // ex AUTO_EXCLUDE
        int maxIps = 32;                          // ex array statico 32
        int maxPriorities = PriorityCount;                    
    };
    
    struct PriorityMgmtEx {
        PriorityMgmt Base;          // 🔥 contiene Priority + DeviceIndex

        bool Interrupted;           // 🔥 READ interrotta da pacing o write
        PriorityDeviceCursor Cursor;   // ⭐ cursore per questa priorità
        
        PriorityMgmtEx() :
            Interrupted(false)
        {
            Base.deviceIndex = -1;
            Base.priority = Priority::Normal;
        }
    };

    // -------------------------
    // 1. Stato IP più chiaro
    // -------------------------
    enum class IpState {
        OK,
        COOLDOWN,
        EXCLUDED
    };

    struct structPriority {
        std::vector<PriorityMgmtEx> Priorities;   // 🔥 usa la versione estesa
        size_t PrioritySize = 0;
        int Index = 0;

        structPriority() = default;

        structPriority(int maxPriorities)
            : Priorities(maxPriorities)
        {}
    };


    struct structIP {
        IPAddress IP;
        int Errors = 0;
        IpState state = IpState::OK;

        structPriority Priority;
        int priorityCycleCounter = 0;   // 🔥 nuovo contatore round‑robin perfetto

        unsigned long lastErrorTime = 0;
        unsigned long cooldown = 0;
        bool exclude = false;
        int consecutiveErrors = 0;
        unsigned long lastCycleDuration = 0;   // 🔥 nuovo campo

        structIP(int maxPriorities, unsigned long baseCooldown)
            : Priority(maxPriorities),
            cooldown(baseCooldown)
        {}
    };


    // -------------------------
    // LOG INTELLIGENTE
    // -------------------------
    void logSkip(int ipIdx, const structIP &ip, unsigned long now) {
        // Log solo se cambia stato
        if (lastState[ipIdx] != ip.state) {
            lastState[ipIdx] = ip.state;

            if (ip.state == IpState::COOLDOWN) {
                unsigned long remaining =
                    (ip.lastErrorTime + ip.cooldown > now)
                    ? (ip.lastErrorTime + ip.cooldown - now)
                    : 0;

                LOG_WF("IpManager",
                    "IP %s in COOLDOWN (%lus rimanenti)",
                    ip.IP.toString().c_str(),
                    remaining / 1000
                );
            }
            return;
        }

        // Log periodico ogni 10 secondi
        if (now - lastCooldownLog[ipIdx] >= 10000) {
            lastCooldownLog[ipIdx] = now;

            unsigned long remaining =
                (ip.lastErrorTime + ip.cooldown > now)
                ? (ip.lastErrorTime + ip.cooldown - now)
                : 0;

            LOG_DF("IpManager",
                "Skip IP %s (cooldown %lus)",
                ip.IP.toString().c_str(),
                remaining / 1000
            );
        }
    }

    void logRestore(int ipIdx, const structIP &ip) {
    
        if (wasInCooldown[ipIdx] && ip.state == IpState::OK) {
            LOG_IF("IpManager",
                "IP %s ripristinato",
                ip.IP.toString().c_str()
            );
        }

        wasInCooldown[ipIdx] = (ip.state == IpState::COOLDOWN);
    }

    IpManager()
        : config(Config())
    {
        initArrays();
    }

    IpManager(const Config& cfg)
        : config(cfg)
    {
        initArrays();
    }
private:
    std::vector<structIP> IPs;
    Config config;
    std::unordered_map<uint32_t, std::vector<int>> devicesByIP;

    // 🔥 Array privati dinamici
    std::vector<IpState> lastState;
    std::vector<unsigned long> lastCooldownLog;
    std::vector<bool> wasInCooldown;

    void initArrays() {
        lastState.resize(config.maxIps, IpState::OK);
        lastCooldownLog.resize(config.maxIps, 0);
        wasInCooldown.resize(config.maxIps, false);
    }

    // -------------------------
    // Helpers di stato
    // -------------------------
    const char* StateToString(IpState s) const {
        switch (s) {
            case IpState::OK: return "OK";
            case IpState::COOLDOWN: return "COOLDOWN";
            case IpState::EXCLUDED: return "EXCLUDED";
        }
        return "UNKNOWN";
    }

    bool ExistsIp(IPAddress ip) {
        for (auto& item : IPs)
            if (item.IP == ip)
                return true;
        return false;
    }

    int GetUsedPriorities(IPAddress ip,
                          std::vector<GenericPrgDevice>& prgDevices,
                          std::vector<PriorityMgmt>& items)
    {
        for (auto& prgDevice : prgDevices) {
            if (prgDevice.GetIp() != ip)
                continue;

            bool exist = false;
            for (auto& it : items)
                if (it.priority == prgDevice.GetPriority())
                    exist = true;

            if (!exist) {
                PriorityMgmt tmp;
                tmp.priority = prgDevice.GetPriority();
                tmp.deviceIndex = -1;
                items.push_back(tmp);
            }
        }
        return (int)items.size();
    }

    static inline uint32_t MakeIpKey(const IPAddress& ip)
    {
        return (uint32_t(ip[0]) << 24) |
            (uint32_t(ip[1]) << 16) |
            (uint32_t(ip[2]) << 8)  |
                uint32_t(ip[3]);
    }

    void BuildDevicesByIP(std::vector<GenericPrgDevice> &prgDevices) {
        devicesByIP.clear();

        for (int i = 0; i < prgDevices.size(); i++) {
            uint32_t key = MakeIpKey(prgDevices[i].GetIp());

            devicesByIP[key].push_back(i);
        }
    }

    bool HasDevicesForPriority(
        int ipIdx,
        Priority prio,
        const GenericPrgDeviceManager& deviceManager)
    {
        const auto& ip = IPs[ipIdx];

        return deviceManager.HasDevicesByPriority(
            prio,
            ip.IP
        );
    }

    void AdvancePriority(int ipIdx, int items) {
        auto &p = IPs[ipIdx].Priority;

        LOG_DF("PRIO", 
            "AdvancePriority(ip=%d) BEFORE: Index=%d DeviceIndex=%d items=%d",
            ipIdx,
            p.Index,
            p.Priorities[p.Index].Base.deviceIndex,
            items);

        p.Priorities[p.Index].Base.deviceIndex++;

        if (p.Priorities[p.Index].Base.deviceIndex >= items ||
            p.Priorities[p.Index].Base.deviceIndex < 0)
        {
            p.Priorities[p.Index].Base.deviceIndex = 0;
        }

        if (p.Index >= p.PrioritySize - 1)
            p.Index = 0;
        else
            p.Index++;

        LOG_DF("PRIO", 
            "AdvancePriority(ip=%d) AFTER: Index=%d DeviceIndex=%d",
            ipIdx,
            p.Index,
            p.Priorities[p.Index].Base.deviceIndex);
    }


public:
    void setLastCycleDuration(int ipIdx, unsigned long duration) {
        IPs[ipIdx].lastCycleDuration = duration;
    }

    unsigned long getLastCycleDuration(int ipIdx) const {
        return IPs[ipIdx].lastCycleDuration;
    }

    // -------------------------
    // Accesso ai device per IP
    // -------------------------
    std::vector<int>* GetDevicesByIP(IPAddress &ip) {
        uint32_t key = MakeIpKey(ip);

        auto it = devicesByIP.find(key);
        if (it == devicesByIP.end())
            return nullptr;

        return &it->second;
    }

    // -------------------------
    // Costruzione lista IP
    // -------------------------
        int BuildIps(std::vector<GenericPrgDevice>& prgDevices) {
        IPs.clear();

        for (auto& prgDevice : prgDevices) {
            auto ip = prgDevice.GetIp();

            if (!ExistsIp(ip)) {
                structIP newItem(config.maxPriorities, config.baseCooldownMs);
                newItem.IP = ip;

                // Priorità
                std::vector<PriorityMgmt> tmpPriorities;
                GetUsedPriorities(ip, prgDevices, tmpPriorities);

                for (size_t j = 0; j < tmpPriorities.size(); j++) {
                    newItem.Priority.Priorities[j].Base = tmpPriorities[j];
                    newItem.Priority.Priorities[j].Interrupted = false;
                }
                newItem.Priority.PrioritySize = tmpPriorities.size();
                newItem.Priority.Index = 0;
                
                IPs.push_back(newItem);
            }
        }

        static bool devicesByIPInitialized = false;
        if (!devicesByIPInitialized) {
            BuildDevicesByIP(prgDevices);
            devicesByIPInitialized = true;
        }

        return IPs.size();
    }
    
    // -------------------------
    // Query device per IP
    // -------------------------
    bool ExistDevicesByIp(int ipIdx) const
    {
        if (ipIdx < 0 || ipIdx >= static_cast<int>(IPs.size()))
            return false;

        auto ip = IPs[ipIdx].IP;
        uint32_t key = MakeIpKey(ip);
        auto it = devicesByIP.find(key);

        return it != devicesByIP.end() && !it->second.empty();
    }

    std::vector<structIP>& GetIps() { return IPs; }
    structIP& GetIp(int ipIdx) { return IPs[ipIdx]; }

    // -------------------------
    // ShouldQuery migliorato
    // -------------------------
    bool ShouldQuery(int ipIdx, unsigned long now) {
        auto &ip = IPs[ipIdx];

        // 1. Escluso manualmente o automaticamente
        if (ip.state == IpState::EXCLUDED)
            return false;

        // 2. In cooldown → NON tentare
        if (ip.state == IpState::COOLDOWN) {
            // 🔥 log automatico
            logSkip(ipIdx, ip, now);

            if (now - ip.lastErrorTime >= ip.cooldown) {
                // 🔥 Fine cooldown → prova UNA volta
                ip.state = IpState::OK;
                ip.consecutiveErrors = 0;

                // 🔥 log automatico
                logRestore(ipIdx, ip);
                return true;
            }
            return false;
        }

        // 3. Stato OK → interrogabile
        return true;
    }


    // -------------------------
    // Gestione errori migliorata
    // -------------------------
    void ReportError(int ipIdx, unsigned long now) {
        auto &ip = IPs[ipIdx];

        LOG_EF(
                "IpManager",
                "!!! ReportError ipIdx=%d IP=%s errors=%d consecutive=%d state=%s",
                ipIdx,
                ip.IP.toString().c_str(),
                ip.Errors,
                ip.consecutiveErrors,
                StateToString(ip.state)
            );

        ip.Errors++;
        ip.consecutiveErrors++;
        ip.lastErrorTime = now;

        // 1. Auto-esclusione dopo troppi errori totali
        if (ip.Errors > config.maxErrorsBeforeExclude) {
            ip.state = IpState::EXCLUDED;
            LOG_WF("IpManager", "IP auto-escluso: %s", ip.IP.toString().c_str());
            return;
        }

        // 2. Dopo MAX_ERRORS (2) → entra in COOLDOWN
        if (ip.consecutiveErrors >= config.maxErrorsBeforeCooldown) {
            ip.state = IpState::COOLDOWN;

            // 🔥 Cooldown fisso e sicuro (evita blocchi del TCP stack)
            ip.cooldown = config.extendedCooldownMs;;   

            // 🔥 log automatico
            logSkip(ipIdx, ip, now);
            return;
        }

        // 3. Prima dei 2 errori → non entra ancora in cooldown
    }


    // -------------------------
    // Ripristino migliorato
    // -------------------------
    void ReportSuccess(int ipIdx) {
        auto &ip = IPs[ipIdx];

        ip.consecutiveErrors = 0;

        if (ip.state == IpState::COOLDOWN) {
            ip.state = IpState::OK;

            // 🔥 log automatico
            logRestore(ipIdx, ip);
        }

        if (ip.cooldown > config.baseCooldownMs)
            ip.cooldown -= 1000;

        if (ip.cooldown < config.baseCooldownMs)
            ip.cooldown = config.baseCooldownMs;
    }


    // -------------------------
    // Esclusione manuale
    // -------------------------
    void Exclude(int ipIdx) {
        auto &ip = IPs[ipIdx];
        ip.state = IpState::EXCLUDED;
    }

    // -------------------------
    // Priorità
    // -------------------------
    PriorityMgmtEx& GetCurrentPriority(
        int ipIdx,
        std::vector<GenericPrgDevice>& prgDevices,
        const GenericPrgDeviceManager& deviceManager)
    {
        auto &ip = IPs[ipIdx];
        auto &p  = ip.Priority;

        // LOG MINIMO
        LOG_DF("PRIO", "GetCurrentPriority(ip=%d)", ipIdx);

        int prio = p.Priorities[p.Index].Base.priority;

        // ============================================================
        // 1. Resume se interrupted
        // ============================================================
        for (int i = 0; i < (int)p.PrioritySize; i++) {
            if (p.Priorities[i].Interrupted) {

                if (p.Priorities[i].Base.priority == Low) {
                    LOG_WF("LOW::RESUME",
                        "ip=%d → RESUME LOW (interrupted)",
                        ipIdx);
                }

                p.Index = i;
                
                return p.Priorities[i];
            }
        }

        // ============================================================
        // 2. Se la corrente è interrupted → NON ruotare
        // ============================================================
        if (p.Priorities[p.Index].Interrupted) {

            if (p.Priorities[p.Index].Base.priority == Low) {
                LOG_WF("LOW::STAY",
                    "ip=%d → LOW still interrupted",
                    ipIdx);
            }

            auto &prioEx = p.Priorities[p.Index];

            return prioEx;
        }

        prio = p.Priorities[p.Index].Base.priority;

        // ============================================================
        // 3. Check se ci sono device per questa priorità
        // ============================================================
        const bool has = HasDevicesForPriority(
            ipIdx,
            static_cast<Priority>(prio),
            deviceManager);

        if (!has) {

            if (prio == Low) {
                LOG_WF("LOW::NODEV",
                    "ip=%d → LOW has NO DEVICES → AdvancePriority()",
                    ipIdx);
            }

            AdvancePriority(ipIdx, 0);
            return GetCurrentPriority(ipIdx, prgDevices, deviceManager);
        }

        // ============================================================
        // 4. Allow logic
        // ============================================================
        bool allow = false;

        switch (prio) {
            case High:   allow = true; break;
            case Medium: allow = (ip.priorityCycleCounter % 2 == 0); break;
            case Low:    allow = (ip.priorityCycleCounter % 3 == 0); break;
            case Normal: allow = (ip.priorityCycleCounter % 2 == 0); break;
            default:     allow = true; break;
        }

        if (!allow) {
            AdvancePriority(ipIdx, 0);
            return GetCurrentPriority(ipIdx, prgDevices, deviceManager);
        }

        // ============================================================
        // 5. SELECT PRIORITY
        // ============================================================
        auto &prioEx = p.Priorities[p.Index];

        // ⭐ inizializza il cursore solo la prima volta
        prioEx.Cursor.init();

        return prioEx;
    }


    void UpdatePriorityAfterRead(int ipIdx,
                             bool interrupted,
                             int nextDeviceIndex,
                             int items)
    {
        auto &ip = IPs[ipIdx];
        auto &p  = ip.Priority.Priorities[ip.Priority.Index];

        if (interrupted) {
            // ⭐ Caso pacing: la READ è stata interrotta
            // → salva il punto esatto da cui riprendere
            p.Interrupted = true;
            p.Base.deviceIndex = nextDeviceIndex;
            p.Cursor.save(nextDeviceIndex);     // ⭐ salva nel cursore
            return;
        }

        // ⭐ Caso normale: la priorità NON è interrotta
        // → il round‑robin ha già avanzato il cursore in computeRange()
        p.Interrupted = false;

        // ⭐ Incrementa il ciclo round‑robin delle priorità
        ip.priorityCycleCounter++;

        // ⭐ Avanza alla prossima priorità
        AdvancePriority(ipIdx, items);
    }

    // -------------------------
    // Diagnostica avanzata
    // -------------------------
    void PrintStatus() {
        for (int i = 0; i < IPs.size(); i++) {
            auto &ip = IPs[i];
            LOG_IF("IpManager",
                "IP %d: %s  Errors=%d  Cooldown=%lu  State=%s",
                i,
                ip.IP.toString().c_str(),
                ip.Errors,
                ip.cooldown,
                StateToString(ip.state)
            );
        }
    }
};

class ModbusManager
{
private:
    DMNetworkManager* net = nullptr;
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

        // Istante dell'ultima READ completata per questo IP.
        // Deve essere per-IP e non una reference statica al parametro
        // locale di RunClient().
        unsigned long lastReadTime = 0;
    };


    std::vector<ClientStepState> clientStates;

    // ============================================================
    // CONNESSIONE MODBUS PERSISTENTE
    // ============================================================
    
    class PersistentModbusConnection
    {
    private:

        static constexpr unsigned long INACTIVITY = 1200;
        static constexpr unsigned long IP_SWITCH_GUARD_MS = 0;

        // ============================================================
        // CONNECTION STATE
        // ============================================================

        struct ConnectionState
        {
            IPAddress lastIp = IPAddress(0, 0, 0, 0);
            bool hasConnection = false;
            unsigned long lastActivity = 0;
        };

        // ============================================================
        // IP SWITCH STATE
        // ============================================================

        struct SwitchState
        {
            unsigned long time = 0;
            bool waiting = false;
        };

        // ============================================================
        // SOCKET STATE
        // ============================================================

        struct SocketState
        {
            DMNetworkManager* net = nullptr;
            SocketManager::OwnerId owner = -1;

            bool acquired = false;

            // IMPORTANTE:
            // memorizziamo lo slot realmente assegnato da SocketManager.
            int slot = -1;
        };

        ModbusTCPClient& modbusClient;

        ConnectionState connection;
        SwitchState switchState;
        SocketState socket;

    public:

        // ============================================================
        // ENSURE RESULT
        // ============================================================

        enum class EnsureResult : uint8_t
        {
            CONNECTED,
            WAITING,
            FAILED
        };

        enum class ReconnectReason
        {
            NONE,
            NOT_CONNECTED,
            IP_CHANGED,
            INACTIVITY
        };

        // ============================================================
        // CONSTRUCTOR
        // ============================================================

        explicit PersistentModbusConnection(
            DMPlatform::Modbus::TCPClient& client)
            : modbusClient(client)
        {
        }

        // ============================================================
        // RESET CONNECTION STATE
        // ============================================================

        inline void resetConnectionState()
        {
            connection.hasConnection = false;
            connection.lastIp = IPAddress(0, 0, 0, 0);
            connection.lastActivity = 0;
        }

        // ============================================================
        // SOCKET CONTEXT
        // ============================================================

        void setSocketContext(
            DMNetworkManager& networkManager,
            SocketManager::OwnerId owner)
        {
            // Il contesto non dovrebbe cambiare durante una
            // connessione persistente.
            //
            // Se però cambia realmente, non possiamo lasciare
            // lo slot associato al vecchio owner.

            if (socket.acquired &&
                (socket.net != &networkManager ||
                socket.owner != owner))
            {
                releaseSocket();
            }

            socket.net = &networkManager;
            socket.owner = owner;
        }

        // ============================================================
        // RELEASE SOCKET
        // ============================================================

        void releaseSocket()
        {
            if (!socket.acquired)
                return;

            if (!socket.net)
            {
                socket.acquired = false;
                socket.slot = -1;
                return;
            }

            if (socket.owner < 0)
            {
                socket.acquired = false;
                socket.slot = -1;
                return;
            }

            if (socket.slot < 0)
            {
                socket.acquired = false;
                return;
            }

            socket.net->sockets().release(
                socket.owner,
                socket.slot,
                SocketManager::SocketKind::TCP_CLIENT
            );

            socket.acquired = false;
            socket.slot = -1;
        }

        // ============================================================
        // ACQUIRE SOCKET
        // ============================================================

        bool acquireSocket()
        {
            if (!socket.net)
            {
                LOG_EF(
                    "MDB::Socket",
                    "NetworkManager unavailable"
                );

                return false;
            }

            if (socket.owner < 0)
            {
                LOG_EF(
                    "MDB::Socket",
                    "Modbus socket owner invalid"
                );

                return false;
            }

            if (socket.acquired)
                return true;

            const int slot =
                socket.net->sockets().acquire(
                    socket.owner,
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

            socket.slot = slot;
            socket.acquired = true;

            return true;
        }

        // ============================================================
        // TOUCH
        // ============================================================

        inline void touch(
            unsigned long now)
        {
            connection.lastActivity = now;
        }

        // ============================================================
        // RECONNECT REASON
        // ============================================================

        inline ReconnectReason getReconnectReason(
            const IPAddress& ip) const
        {
            // La connessione TCP non esiste più.
            if (!modbusClient.connected())
                return ReconnectReason::NOT_CONNECTED;

            // Connessione attiva ma verso un IP diverso.
            if (connection.lastIp != ip)
                return ReconnectReason::IP_CHANGED;

            // Timeout di inattività.
            if (millis() - connection.lastActivity > INACTIVITY)
                return ReconnectReason::INACTIVITY;

            return ReconnectReason::NONE;
        }

        // ============================================================
        // ENSURE CONNECTION
        // ============================================================

        EnsureResult ensure(
            int ipIndex,
            IpManager& ipManager,
            int port,
            unsigned long now)
        {
            auto& ips = ipManager.GetIps();

            // --------------------------------------------------------
            // VALID IP INDEX
            // --------------------------------------------------------

            if (ipIndex < 0 ||
                static_cast<size_t>(ipIndex) >= ips.size())
            {
                LOG_EF(
                    "MDB::ensure",
                    "INVALID ipIndex=%d",
                    ipIndex
                );

                return EnsureResult::FAILED;
            }

            // --------------------------------------------------------
            // SHOULD QUERY
            // --------------------------------------------------------

            if (!ipManager.ShouldQuery(
                    ipIndex,
                    now))
            {
                return EnsureResult::FAILED;
            }

            // ========================================================
            // ATTESA DOPO CAMBIO IP
            // ========================================================

            if (switchState.waiting)
            {
                if (now - switchState.time <
                    IP_SWITCH_GUARD_MS)
                {
                    return EnsureResult::WAITING;
                }

                switchState.waiting = false;
            }

            // --------------------------------------------------------
            // IP CORRENTE
            // --------------------------------------------------------

            auto& ipStruct = ips[ipIndex];

            const IPAddress ip =
                ipStruct.IP;

            // --------------------------------------------------------
            // RECONNECT REASON
            // --------------------------------------------------------

            const ReconnectReason reason =
                getReconnectReason(ip);

            // ========================================================
            // CONNESSIONE VALIDA
            // ========================================================

            if (reason == ReconnectReason::NONE)
            {
                connection.hasConnection = true;

                if (!socket.acquired)
                {
                    if (!acquireSocket())
                    {
                        connection.hasConnection = false;

                        return EnsureResult::FAILED;
                    }
                }

                if (ipStruct.state !=
                    IpManager::IpState::OK)
                {
                    ipManager.ReportSuccess(
                        ipIndex
                    );
                }

                return EnsureResult::CONNECTED;
            }

            // ========================================================
            // CAMBIO IP
            // ========================================================

            if (reason == ReconnectReason::IP_CHANGED)
            {
                if (modbusClient.connected())
                    modbusClient.stop();

                resetConnectionState();

                releaseSocket();

                // Non riconnettere nello stesso giro.
                switchState.time = now;
                switchState.waiting = true;

                return EnsureResult::WAITING;
            }

            // ========================================================
            // INACTIVITY
            // ========================================================

            if (reason == ReconnectReason::INACTIVITY)
            {
                if (modbusClient.connected())
                    modbusClient.stop();

                resetConnectionState();

                releaseSocket();
            }

            // ========================================================
            // NOT CONNECTED
            // ========================================================

            if (reason == ReconnectReason::NOT_CONNECTED)
            {
                resetConnectionState();

                // La connessione potrebbe essere caduta
                // esternamente: il socket manager deve
                // essere riallineato.
                releaseSocket();
            }

            // ========================================================
            // ACQUIRE PRIMA DEL CONNECT
            // ========================================================

            if (!acquireSocket())
            {
                connection.hasConnection = false;

                return EnsureResult::FAILED;
            }

            // ========================================================
            // CONNECT
            // ========================================================

            const unsigned long start =
                millis();

            const bool connected =
                modbusClient.begin(
                    ip,
                    port
                );

            const unsigned long duration =
                millis() - start;

            // --------------------------------------------------------
            // Log solo in caso di connect lento
            // --------------------------------------------------------

            if (duration > 100)
            {
                LOG_WF(
                    "MDB::CONNECT",
                    "ip=%s result=%d duration=%lu ms",
                    ip.toString().c_str(),
                    connected,
                    duration
                );
            }

            // ========================================================
            // CONNECT FALLITO
            // ========================================================

            if (!connected)
            {
                resetConnectionState();

                releaseSocket();

                ipManager.ReportError(
                    ipIndex,
                    now
                );

                return EnsureResult::FAILED;
            }

            // ========================================================
            // CONNECT SUCCESS
            // ========================================================

            connection.lastIp =
                ip;

            connection.lastActivity =
                now;

            connection.hasConnection =
                true;

            if (ipStruct.state !=
                IpManager::IpState::OK)
            {
                ipManager.ReportSuccess(
                    ipIndex
                );
            }

            return EnsureResult::CONNECTED;
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
            const auto& targetIp =
                ipManager->GetIps()[ipIndex].IP;

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
        DMNetworkManager& netManager,
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
        ERROR,
        WAITING
    };


    // ============================================================
    // STATE MAPPING
    // ============================================================

    inline DMNetworkManager::Protocol::State
    mapClientState(ClientState state)
    {
        using CS =
            ModbusManager::ClientState;

        using ProtocolState  =
            DMNetworkManager::Protocol::State;

        switch (state)
        {
            case CS::CYCLE_OK:
                return ProtocolState::CYCLE_OK;

            case CS::DEVICE_ERROR:
                return ProtocolState::DEVICE_ERROR;

            case CS::ERROR:
                return ProtocolState::ERROR;

            case CS::WAITING:
                return ProtocolState::WAITING;

            case CS::WRITE_DONE:
                return ProtocolState::WRITE_DONE;

            case CS::READ_DONE:
                return ProtocolState::READ_DONE;

            default:
                return ProtocolState::ERROR;
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
        DMPlatform::Modbus::TCPClient& modbusClient,
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


        // --------------------------------------------------------
        // CONNESSIONE MODBUS PERSISTENTE
        // --------------------------------------------------------

        static PersistentModbusConnection conn(modbusClient);


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

        const auto ensureResult =
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


        // --------------------------------------------------------
        // WAITING
        // --------------------------------------------------------

        if (ensureResult ==
            PersistentModbusConnection::EnsureResult::WAITING)
        {
            // Nessun errore.
            // Il giro corrente viene semplicemente rimandato.
            return ClientState::WAITING;
        }


        // --------------------------------------------------------
        // FAILED
        // --------------------------------------------------------

        if (ensureResult ==
            PersistentModbusConnection::EnsureResult::FAILED)
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
                    DMNetworkManager::Protocol::State::ERROR
                );
            }

            return ClientState::ERROR;
        }


        // ========================================================
        // READ
        // ========================================================

        if (step == ClientStep::READ)
        {
            timing.startDevice(now);

            profiler.begin(
                PROF_READ,
                ipIndex,
                millis()
            );

            const bool readOk =
                DeviceRead(
                    modbusClient,
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
                        DMNetworkManager::Protocol::State::DEVICE_ERROR
                    );
                }

                return ClientState::DEVICE_ERROR;
            }


            conn.touch(millis());

            // ----------------------------------------------------
            // FIX:
            // salva il tempo della READ nello stato dell'IP.
            //
            // NON usare:
            // static unsigned long& lastReadTime = now;
            //
            // perché 'now' è un parametro locale della funzione.
            // ----------------------------------------------------

            clientState.lastReadTime = now;


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

            bool somethingChangedNeeded = false;

            for (const auto& area : changedMap)
            {
                const int areaToWrite =
                    buffer->GetAreaToWrite(area);

                if (areaToWrite > 0)
                {
                    const BufferSourceInfo& entry =
                        buffer->getFieldEntry(area);

                    const uint16_t value =
                        static_cast<uint16_t>(entry.value);

                    const auto result =
                        buffer->WriteElement(
                            areaToWrite,
                            value,
                            now
                        );

                    if (result == Buffer::WriteResult::CHANGED)
                    {
                        somethingChangedNeeded = true;
                    }
                }
            }


            if (somethingChangedNeeded &&
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
                    DMNetworkManager::Protocol::State::READ_DONE
                );
            }


            return ClientState::READ_DONE;
        }


        // ========================================================
        // WRITE
        // ========================================================

        if (step == ClientStep::WRITE)
        {
            profiler.begin(
                PROF_WRITE,
                ipIndex,
                millis()
            );

            const bool writeOk =
                DeviceWrite(
                    modbusClient,
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
                        DMNetworkManager::Protocol::State::DEVICE_ERROR
                    );
                }

                return ClientState::DEVICE_ERROR;
            }


            timing.endDevice(
                now,
                ipIndex
            );


            // ----------------------------------------------------
            // FIX:
            // usa il timestamp appartenente all'IP corrente.
            // ----------------------------------------------------

            ipManager->setLastCycleDuration(
                ipIndex,
                now - clientState.lastReadTime
            );


            conn.touch(millis());
            step = ClientStep::READ;


            if (net)
            {
                net->updateProtocolState(
                    modbusProtocolId,
                    DMNetworkManager::Protocol::State::CYCLE_OK
                );
            }


            return ClientState::CYCLE_OK;
        }


        // --------------------------------------------------------
        // STATO IMPOSSIBILE
        // --------------------------------------------------------

        if (net)
        {
            net->updateProtocolState(
                modbusProtocolId,
                DMNetworkManager::Protocol::State::ERROR
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
        DMPlatform::Modbus::TCPClient& modbusClient,
        short ipIndex,
        std::vector<uint16_t>& mbRead,
        unsigned long now)
    {
        return DeviceManagement_Read(
            m_ledController,
            modbusClient,
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
        DMPlatform::Modbus::TCPClient& modbusClient,
        IPAddress ip,
        unsigned long now)
    {
        return DeviceManagement_Write(
            m_ledController,
            modbusClient,
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
        DMPlatform::Modbus::TCPClient& modbusTCPCli,
        IPAddress ip,
        Buffer& buffer,
        std::vector<GenericPrgDevice>& prgDevices,
        unsigned long now)
    {
        bool inError = false;
        bool done = false;

        // Massimo numero di transazioni Modbus eseguite
        // in una singola chiamata.
        constexpr uint8_t MAX_WRITES_PER_CYCLE = 2;

        uint8_t writesThisCycle = 0;

        // ========================================================
        // DEVICE ASSOCIATI ALL'IP
        // ========================================================

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

        // ========================================================
        // CHANGED MAP
        // ========================================================
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


        // ========================================================
        // DEVICE LOOKUP PER IP
        // ========================================================
        static std::unordered_set<int> devSet;

        devSet.clear();

        for (const int idx : *devicesForIP)
            devSet.insert(idx);

        // ========================================================
        // WRITE
        //
        // Regola:
        //   - massimo 2 WRITE riuscite per chiamata
        //   - se una WRITE fallisce, il ciclo viene interrotto
        //   - la pending della WRITE fallita rimane nel Buffer
        //
        // Questo impedisce che una raffica MQTT/Zigbee trasformi
        // una singola esecuzione in una lunga sequenza di timeout.
        // ========================================================

        for (const auto& area : changedMap)
        {
            // ----------------------------------------------------
            // BUDGET
            // ----------------------------------------------------
            if (writesThisCycle >= MAX_WRITES_PER_CYCLE)
                break;

            // ----------------------------------------------------
            // AREA MAP
            // ----------------------------------------------------
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

            // ----------------------------------------------------
            // DEVICE ASSOCIATO ALL'IP
            // ---------------------------------------------------
            if (!devSet.count(entry.devIndex))
                continue;

            if (entry.devIndex < 0 ||
                entry.devIndex >= static_cast<int>(prgDevices.size()))
            {
                LOG_WF(
                    "ModbusManager",
                    "Invalid device index=%d for area=%d",
                    entry.devIndex,
                    area
                );

                continue;
            }

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

                // La pending NON viene cancellata.
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
                );

            // ----------------------------------------------------
            // WRITE FALLITA
            // ----------------------------------------------------

            if (!writeOk)
            {
                inError = true;

                LOG_EF(
                    "ModbusManager",
                    "FAIL to WRITE → area=%d "
                    "channel=%d itemIndex=%d value=%ld ",
                    area,
                    entry.channel,
                    entry.itemIndex,
                    info.value
                );

                // NON resettiamo il Buffer:
                // la pending verrà ritentata successivamente.

                // IMPORTANTISSIMO:
                // non continuiamo con altre WRITE nello stesso ciclo.
                break;
            }

            // ----------------------------------------------------
            // WRITE RIUSCITA
            // ----------------------------------------------------

            buffer.ResetElement(area);

            done = true;
            ++writesThisCycle;
        }

        // ========================================================
        // LED
        // ========================================================

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


        // ========================================================
        // RESULT
        // ========================================================

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
        const auto result =
            buffer.WriteElement(
                area,
                value,
                now
            );

        // Evento solo se il valore è realmente cambiato.
        if (result == Buffer::WriteResult::CHANGED &&
            fieldChangedCallback)
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
            const auto result =
                buffer.WriteElement(
                    outArea,
                    value,
                    now
                );

            if (result == Buffer::WriteResult::CHANGED)
            {
                if (fieldChangedCallback)
                {
                    fieldChangedCallback(
                        outArea,
                        value
                    );
                }

                LOG_DF(
                    "ModbusManager",
                    "Forward: area=%d areaToWrite=%d value=%d",
                    area,
                    outArea,
                    value
                );
            }
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
/*
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

                    // ------------------------------------------------
                    // DIGITAL / ANALOG
                    // ------------------------------------------------
                    long value;
                    if (read.itemsPerCall == 1)
                    {
                        value = mbRead[j];
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

                    if (value > 0 && area >= 10)
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
                }

                // ------------------------------------------------
                // WRITE LOCALE IMMEDIATA
                // ------------------------------------------------
                if (hasWriteForIp(ipIndex))
                {
                    LOG_DF(
                        "ModbusManager",
                        "WRITE locale rilevata durante READ "
                        "→ interrompo DeviceRead (ip=%d)",
                        ipIndex
                    );

                    return true;
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
    } */

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
        // PRIORITY
        // --------------------------------------------------------

        auto& priorityExecution =
            ipManager.GetCurrentPriority(
                ipIndex,
                prgDevices,
                planner.getDeviceManager()
            );

        PriorityMgmt& actualPriority =
            priorityExecution.Base;


        // --------------------------------------------------------
        // DEVICES FOR CURRENT IP
        // --------------------------------------------------------

        const std::vector<int>& devices =
            planner.getDevicesForIp(
                actualPriority,
                ipManager,
                ipIndex
            );

        const int itemCount =
            static_cast<int>(
                devices.size()
            );


        if (itemCount <= 0)
        {
            LOG_EF(
                "ModbusManager",
                "Nothing to read"
                " | Priority=%d"
                " | IP=%u.%u.%u.%u",
                static_cast<int>(
                    actualPriority.priority
                ),
                static_cast<unsigned>(
                    ipManager.GetIp(ipIndex).IP[0]
                ),
                static_cast<unsigned>(
                    ipManager.GetIp(ipIndex).IP[1]
                ),
                static_cast<unsigned>(
                    ipManager.GetIp(ipIndex).IP[2]
                ),
                static_cast<unsigned>(
                    ipManager.GetIp(ipIndex).IP[3]
                )
            );

            return true;
        }


        // --------------------------------------------------------
        // READ LED
        // --------------------------------------------------------

        if (ledsController &&
            ledsController->hasChannel(
                LedController::ONE
            ))
        {
            static bool ledState =
                false;

            ledState =
                !ledState;

            ledsController->set(
                LedController::ONE,
                ledState
            );
        }


        // --------------------------------------------------------
        // DEVICE RANGE
        // --------------------------------------------------------

        const auto range =
            planner.computeRange(
                priorityExecution,
                itemCount,
                ipIndex,
                ipManager
            );


        // --------------------------------------------------------
        // RESUME PACING
        // --------------------------------------------------------

        int startIndex =
            range.start;

        if (priorityExecution.Interrupted)
        {
            startIndex =
                priorityExecution.Base.deviceIndex;

            if (startIndex < range.start)
                startIndex = range.start;

            if (startIndex >= range.end)
                startIndex = range.end - 1;
        }


        // --------------------------------------------------------
        // DEVICE LOOP
        // --------------------------------------------------------

        for (int deviceIndex = startIndex;
            deviceIndex < range.end;
            ++deviceIndex)
        {
            const int runtimeDeviceIndex =
                devices[deviceIndex];


            if (runtimeDeviceIndex < 0 ||
                runtimeDeviceIndex >=
                    static_cast<int>(
                        prgDevices.size()
                    ))
            {
                LOG_EF(
                    "ModbusManager",
                    "Invalid runtime device index"
                    " | DeviceIndex=%d"
                    " | RuntimeIndex=%d"
                    " | DeviceCount=%u",
                    deviceIndex,
                    runtimeDeviceIndex,
                    static_cast<unsigned>(
                        prgDevices.size()
                    )
                );

                error =
                    true;

                break;
            }


            GenericPrgDevice& device =
                prgDevices[
                    static_cast<size_t>(
                        runtimeDeviceIndex
                    )
                ];


            // ----------------------------------------------------
            // DEVICE ALREADY IN ERROR
            // ----------------------------------------------------

            if (device.GetError().IsInError())
            {
                LOG_DF(
                    "MDB::READ",
                    "Device skipped because it is in error"
                    " | DeviceIndex=%d"
                    " | RuntimeIndex=%d"
                    " | Name=%s"
                    " | Zone=%s"
                    " | Installation=%s"
                    " | IP=%u.%u.%u.%u"
                    " | Address=%u",
                    deviceIndex,
                    runtimeDeviceIndex,
                    device.GetName(),
                    device.GetZone(),
                    device.GetInstallationLocation(),
                    static_cast<unsigned>(
                        device.GetIp()[0]
                    ),
                    static_cast<unsigned>(
                        device.GetIp()[1]
                    ),
                    static_cast<unsigned>(
                        device.GetIp()[2]
                    ),
                    static_cast<unsigned>(
                        device.GetIp()[3]
                    ),
                    static_cast<unsigned>(
                        device.GetDeviceAddress()
                    )
                );


                device.GetError().Loop(
                    true,
                    now
                );


                if (priorityExecution.Interrupted)
                {
                    priorityExecution.Cursor.save(
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
                    itemCount,
                    now))
            {
                LOG_DF(
                    "MDB::READ",
                    "Read cycle interrupted by pacing"
                    " | DeviceIndex=%d"
                    " | Name=%s"
                    " | Zone=%s"
                    " | Installation=%s"
                    " | IP=%u.%u.%u.%u"
                    " | Address=%u",
                    deviceIndex,
                    device.GetName(),
                    device.GetZone(),
                    device.GetInstallationLocation(),
                    static_cast<unsigned>(
                        device.GetIp()[0]
                    ),
                    static_cast<unsigned>(
                        device.GetIp()[1]
                    ),
                    static_cast<unsigned>(
                        device.GetIp()[2]
                    ),
                    static_cast<unsigned>(
                        device.GetIp()[3]
                    ),
                    static_cast<unsigned>(
                        device.GetDeviceAddress()
                    )
                );


                return true;
            }


    #ifdef DEBUG_VIEW

            LOG_IF(
                "MDB::READ",
                "Reading device"
                " | DeviceIndex=%d"
                " | RuntimeIndex=%d"
                " | Name=%s"
                " | Zone=%s"
                " | Installation=%s"
                " | IP=%u.%u.%u.%u"
                " | Address=%u"
                " | Priority=%d",
                deviceIndex,
                runtimeDeviceIndex,
                device.GetName(),
                device.GetZone(),
                device.GetInstallationLocation(),
                static_cast<unsigned>(
                    device.GetIp()[0]
                ),
                static_cast<unsigned>(
                    device.GetIp()[1]
                ),
                static_cast<unsigned>(
                    device.GetIp()[2]
                ),
                static_cast<unsigned>(
                    device.GetIp()[3]
                ),
                static_cast<unsigned>(
                    device.GetDeviceAddress()
                ),
                static_cast<int>(
                    device.GetPriority()
                )
            );

            delay(500);

    #endif


            // ----------------------------------------------------
            // CHANNEL LOOP
            // ----------------------------------------------------

            const int channelCount =
                static_cast<int>(
                    device.GetChannelsSize()
                );


            for (int channel = 0;
                channel < channelCount;
                ++channel)
            {
                const auto& channelInfo =
                    device.GetChannelInfo(
                        channel
                    );


                if (channelInfo.type !=
                        GenericPrgDevice::DI &&
                    channelInfo.type !=
                        GenericPrgDevice::AI)
                {
                    continue;
                }


                // ------------------------------------------------
                // MODBUS READ
                // ------------------------------------------------

                const auto readResult =
                    device.Read(
                        modbusTCPCli,
                        channel,
                        mbRead.data(),
                        now
                    );


                if (!readResult.ok)
                {
                    LOG_DF(
                        "MDB::READ",
                        "Device read failed"
                        " | DeviceIndex=%d"
                        " | RuntimeIndex=%d"
                        " | Name=%s"
                        " | Zone=%s"
                        " | Installation=%s"
                        " | IP=%u.%u.%u.%u"
                        " | Address=%u"
                        " | Channel=%d"
                        " | ChannelType=%d"
                        " | HardwareType=%d",
                        deviceIndex,
                        runtimeDeviceIndex,
                        device.GetName(),
                        device.GetZone(),
                        device.GetInstallationLocation(),
                        static_cast<unsigned>(
                            device.GetIp()[0]
                        ),
                        static_cast<unsigned>(
                            device.GetIp()[1]
                        ),
                        static_cast<unsigned>(
                            device.GetIp()[2]
                        ),
                        static_cast<unsigned>(
                            device.GetIp()[3]
                        ),
                        static_cast<unsigned>(
                            device.GetDeviceAddress()
                        ),
                        channel,
                        static_cast<int>(
                            channelInfo.type
                        ),
                        static_cast<int>(
                            channelInfo.hwType
                        )
                    );


                    error =
                        true;

                    break;
                }


    #ifdef DEBUG_VIEW

                LOG_IF(
                    "MDB::READ",
                    "Channel read completed"
                    " | Name=%s"
                    " | Zone=%s"
                    " | Installation=%s"
                    " | Channel=%d"
                    " | StartIndex=%d"
                    " | Items=%d"
                    " | ItemsPerCall=%d",
                    device.GetName(),
                    device.GetZone(),
                    device.GetInstallationLocation(),
                    channel,
                    readResult.startIndex,
                    readResult.items,
                    readResult.itemsPerCall
                );

    #endif


                // ------------------------------------------------
                // PROCESS READ DATA
                // ------------------------------------------------

                for (int item = 0;
                    item < readResult.items;
                    ++item)
                {
                    const int itemIndex =
                        readResult.startIndex +
                        item;


                    const int area =
                        device.GetArea(
                            channel,
                            itemIndex
                        );


                    if (area < 0 ||
                        area >=
                            static_cast<int>(
                                buffer.size()
                            ))
                    {
                        LOG_EF(
                            "MDB::READ",
                            "Invalid area returned by device"
                            " | Name=%s"
                            " | Zone=%s"
                            " | Installation=%s"
                            " | Channel=%d"
                            " | ItemIndex=%d"
                            " | Area=%d",
                            device.GetName(),
                            device.GetZone(),
                            device.GetInstallationLocation(),
                            channel,
                            itemIndex,
                            area
                        );


                        error =
                            true;

                        break;
                    }


                    const BufferSourceInfo& bufferEntry =
                        buffer.getFieldEntry(
                            area
                        );


                    // --------------------------------------------
                    // DIGITAL / ANALOG VALUE
                    // --------------------------------------------

                    long value =
                        0;


                    if (readResult.itemsPerCall == 1)
                    {
                        value =
                            mbRead[item];
                    }
                    else
                    {
                        const int registerIndex =
                            item *
                            readResult.itemsPerCall;


                        if (registerIndex + 1 >=
                            static_cast<int>(
                                mbRead.size()
                            ))
                        {
                            LOG_EF(
                                "MDB::READ",
                                "Read buffer index out of range"
                                " | Name=%s"
                                " | Zone=%s"
                                " | Installation=%s"
                                " | Channel=%d"
                                " | Item=%d"
                                " | RegisterIndex=%d"
                                " | BufferSize=%u",
                                device.GetName(),
                                device.GetZone(),
                                device.GetInstallationLocation(),
                                channel,
                                item,
                                registerIndex,
                                static_cast<unsigned>(
                                    mbRead.size()
                                )
                            );


                            error =
                                true;

                            break;
                        }


                        const uint32_t raw =
                            (
                                static_cast<uint32_t>(
                                    mbRead[registerIndex]
                                ) << 16
                            ) |
                            mbRead[
                                registerIndex + 1
                            ];


                        float floatValue =
                            0.0f;


                        memcpy(
                            &floatValue,
                            &raw,
                            sizeof(float)
                        );


                        value =
                            floatValue < 0
                                ? 0
                                : static_cast<unsigned long>(
                                    floatValue * 100
                                );
                    }


                    bool process =
                        false;


                    // --------------------------------------------
                    // ANALOG INPUT
                    // --------------------------------------------

                    if (channelInfo.type ==
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

                    // --------------------------------------------
                    // DIGITAL INPUT
                    // --------------------------------------------

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


                    // --------------------------------------------
                    // REAL VALUE CHANGE LOG
                    // --------------------------------------------

                    if (value > 0 &&
                        area >= 10)
                    {
                        LOG_IF(
                            "ModbusManager",
                            "%s"
                            " | %s"
                            ", %s"
                            " | IP=%u.%u.%u.%u"
                            " | Addr.=%u"
                            " | Ch.=%d"
                            " | Item=%d"
                            " | Value=%ld"
                            " | Buf.=%ld"
                            " | Prev. Buf.=%ld"
                            " | Area=%d"
                            " | A.Name=%s",
                            device.GetName(),
                            device.GetZone(),
                            device.GetInstallationLocation(),
                            static_cast<unsigned>(
                                device.GetIp()[0]
                            ),
                            static_cast<unsigned>(
                                device.GetIp()[1]
                            ),
                            static_cast<unsigned>(
                                device.GetIp()[2]
                            ),
                            static_cast<unsigned>(
                                device.GetIp()[3]
                            ),
                            static_cast<unsigned>(
                                device.GetDeviceAddress()
                            ),
                            channel,
                            itemIndex,
                            value,
                            bufferEntry.value,
                            bufferEntry.prevValue,
                            area,
                            buffer.GetName(area)
                        );
                    }


                    // --------------------------------------------
                    // CONSUMER POLICY
                    // --------------------------------------------

                    const bool skipForward =
                        readAreaPolicy &&
                        readAreaPolicy(
                            area,
                            value,
                            buffer
                        );


                    // --------------------------------------------
                    // BUFFER + EVENT
                    // --------------------------------------------

                    DeviceManagement_Read_SetOut(
                        buffer,
                        area,
                        value,
                        now,
                        skipForward
                    );


                    // --------------------------------------------
                    // CONSUMED SOURCE AREA
                    // --------------------------------------------

                    if (skipForward)
                    {
                        buffer.ResetElement(
                            area
                        );
                    }
                }


                if (error)
                    break;


                // ------------------------------------------------
                // IMMEDIATE LOCAL WRITE
                // ------------------------------------------------

                if (hasWriteForIp(
                        ipIndex))
                {
                    LOG_DF(
                        "ModbusManager",
                        "Local write detected during read cycle"
                        " | Device=%s"
                        " | Zone=%s"
                        " | Installation=%s"
                        " | IP=%u.%u.%u.%u"
                        " | Address=%u"
                        " | IpIndex=%d",
                        device.GetName(),
                        device.GetZone(),
                        device.GetInstallationLocation(),
                        static_cast<unsigned>(
                            device.GetIp()[0]
                        ),
                        static_cast<unsigned>(
                            device.GetIp()[1]
                        ),
                        static_cast<unsigned>(
                            device.GetIp()[2]
                        ),
                        static_cast<unsigned>(
                            device.GetIp()[3]
                        ),
                        static_cast<unsigned>(
                            device.GetDeviceAddress()
                        ),
                        static_cast<int>(
                            ipIndex
                        )
                    );


                    return true;
                }
            }


            if (error)
                break;
        }


        // --------------------------------------------------------
        // READ CYCLE COMPLETED
        // --------------------------------------------------------

        if (!priorityExecution.Interrupted)
        {
            ipManager.UpdatePriorityAfterRead(
                ipIndex,
                false,
                -1,
                itemCount
            );
        }


        return !error;
    }
};

#endif
