#ifndef DMFncs_HPP
#define DMFncs_HPP

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

/////////////////////////MODBUS TCP INIT
#include <SPI.h>
#include <Ethernet.h>

//Modbus Client, uso libreria ARDUINO
#include <ArduinoRS485.h> // ArduinoModbus depends on the ArduinoRS485 library
#include <ArduinoModbus.h>

//Modbus Server Pannello, uso libreria MgsModbus
#include "MgsModbus.h"

#include <Arduino.h>
#include <array>
#include <cstdint>
#include <algorithm>
#include <functional>

#include "DMPLC.h"
#include "DMBaseClass.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class NetworkManager {
public:
    struct Protocol {
        unsigned long lastExec = 0;      // ultimo timestamp di esecuzione
        unsigned long minInterval = 0;   // pacing minimo
        unsigned long maxDuration = 0;   // durata massima consentita
        bool active = false;             // protocollo in esecuzione
    };

private:
    struct Entry {
        int id;
        Protocol proto;
    };

    std::vector<Entry> protocols;

    int currentOwner = -1;
    unsigned long ownerSince = 0;
    unsigned long lastRelease = 0;

    const unsigned long minGapMs = 4;    // gap minimo tra protocolli

public:

    // ============================================================
    // REGISTRAZIONE DINAMICA
    // ============================================================
    int registerProtocol(unsigned long minInterval, unsigned long maxDuration) {
        int id = protocols.size();
        protocols.push_back({ id, Protocol{0, minInterval, maxDuration, false} });
        return id;
    }

    // ============================================================
    // PACING
    // ============================================================
    inline bool canRun(int id, unsigned long now) {
        auto& p = protocols[id].proto;
        return (now - p.lastExec >= p.minInterval);
    }

    // ============================================================
    // ACQUISIZIONE
    // ============================================================
    inline bool isExpired(int id, unsigned long now) {
        auto& p = protocols[id].proto;
        bool expired = p.active && (now - ownerSince > p.maxDuration);

        if (expired) {
            LOG_WF("NET", "[EXPIRED] id=%d active for %lu > %lu",
                id, now - ownerSince, p.maxDuration);
        }

        return expired;
    }


    inline bool tryAcquire(int id, unsigned long now) {
        auto& p = protocols[id].proto;

        int owner = currentOwner;

        // rete occupata da altro protocollo
        if (owner != -1 && owner != id) {

            if (isExpired(owner, now)) {
                LOG_WF("NET", "[TIMEOUT] owner=%d expired → forced release at %lu", owner, now);
                release(owner, now);
            } else {
                LOG_WF("NET", "[BLOCK] id=%d cannot acquire, owner=%d active", id, owner);
                return false;
            }
        }

        // gap minimo
        if (now - lastRelease < minGapMs) {
            LOG_DF("NET", "[GAP] id=%d blocked, gap=%lu < %lu",
                id, now - lastRelease, minGapMs);
            return false;
        }

        // pacing
        if (now - p.lastExec < p.minInterval) {
            LOG_DF("NET", "[PACER] id=%d blocked, interval=%lu < %lu",
                id, now - p.lastExec, p.minInterval);
            return false;
        }

        // acquisizione
        LOG_DF("NET", "[ACQUIRE] id=%d at %lu", id, now);

        currentOwner = id;
        ownerSince = now;
        p.active = true;
        return true;
    }

    // ============================================================
    // RILASCIO
    // ============================================================
    void release(int id, unsigned long now) {
        auto& p = protocols[id].proto;

        if (currentOwner != id) {
            // opzionale: log di diagnostica
            LOG_WF("NET::release", "IGNORED release for id=%d (owner=%d)", id, currentOwner);
            p.active = false;
            return;
        }

        currentOwner = -1;
        lastRelease = now;
        p.lastExec = now;
        p.active = false;
    }

    // ============================================================
    // MODIFICHE DINAMICHE DEL PACING
    // ============================================================
    void setMinInterval(int id, unsigned long ms) {
        protocols[id].proto.minInterval = ms;
    }

    void setMaxDuration(int id, unsigned long ms) {
        protocols[id].proto.maxDuration = ms;
    }

    // ============================================================
    // STATO
    // ============================================================
    bool isFree() const {
        return currentOwner == -1;
    }
};


template<int MAX_EVENTS, int MAX_CONSUMERS>
class EventManager {
public:
    struct Event {
        uint64_t id;
        int area;
        long value;
        unsigned long time;
        int eventType; // 0=normal, 1=alarm, 2=warning, 3=threshold, ecc.
    };

    struct Consumer {
        bool active = false;
        bool global = false;
        uint64_t cursor = 0;
        std::function<bool(const Event&)> filter;
    };

private:
    std::array<Event, MAX_EVENTS> _ring;
    uint64_t _nextId = 1;
    int _writeIndex = 0;

    std::array<Consumer, MAX_CONSUMERS> _consumers;

    // CALLBACK INTERNE
    std::function<bool(int area, long value)> _alarmCallback;
    std::function<bool(int area, long value)> _warningCallback;
    std::function<bool(int area, long value)> _thresholdCallback;

public:
    EventManager() {}

    // REGISTRA CONSUMER
    inline void registerConsumer(int id, bool global, std::function<bool(const Event&)> filter) {
        _consumers[id].active = true;
        _consumers[id].global = global;
        _consumers[id].cursor = 0;
        _consumers[id].filter = filter;
    }

    inline void unregisterConsumer(int id) {
        _consumers[id].active = false;
    }

    // REGISTRA CALLBACK INTERNE
    inline void onAlarm(std::function<bool(int,long)> cb) { _alarmCallback = cb; }
    inline void onWarning(std::function<bool(int,long)> cb) { _warningCallback = cb; }
    inline void onThreshold(std::function<bool(int,long)> cb) { _thresholdCallback = cb; }

    // PUSH EVENTO NORMALE
    inline void push(int area, long value, unsigned long time) {
        pushInternal(area, value, time, 0);
    }

    // PUSH EVENTO INTERNO
    inline void pushInternal(int area, long value, unsigned long time, int type) {
        Event &e = _ring[_writeIndex];
        e.id = _nextId++;
        e.area = area;
        e.value = value;
        e.time = time;
        e.eventType = type;

        _writeIndex = (_writeIndex + 1) % MAX_EVENTS;
    }

    // PUSH EVENTO CON CALLBACK
    inline void pushWithCallbacks(int area, long value, unsigned long time) {
        int type = 0;

        if (_alarmCallback && _alarmCallback(area, value))
            type = 1;

        else if (_warningCallback && _warningCallback(area, value))
            type = 2;

        else if (_thresholdCallback && _thresholdCallback(area, value))
            type = 3;

        pushInternal(area, value, time, type);
    }

    // GET NEXT EVENT
    inline bool getNext(int consumerId, Event &out) {
        Consumer &c = _consumers[consumerId];
        if (!c.active) return false;

        uint64_t last = c.cursor;

        for (int i = 0; i < MAX_EVENTS; i++) {
            const Event &e = _ring[i];
            if (e.id > last && c.filter(e)) {
                out = e;
                c.cursor = e.id;
                return true;
            }
        }

        return false;
    }

    // OVERFLOW DETECTION
    inline bool isOutOfSync(int consumerId) const {
        const Consumer &c = _consumers[consumerId];
        if (!c.active) return false;

        uint64_t last = c.cursor;
        uint64_t newest = _nextId - 1;
        uint64_t oldest = newest > MAX_EVENTS ? newest - MAX_EVENTS + 1 : 1;

        return last < oldest;
    }

    // CLEANUP BASATO SOLO SUI GLOBALI
    inline void cleanup() {
        uint64_t newest = _nextId - 1;
        uint64_t oldest = newest > MAX_EVENTS ? newest - MAX_EVENTS + 1 : 1;

        uint64_t minCursor = UINT64_MAX;

        for (auto &c : _consumers)
            if (c.active && c.global)
                minCursor = std::min(minCursor, c.cursor);

        if (minCursor == UINT64_MAX)
            return;

        // il ring sovrascrive automaticamente
    }
};


class OwnerManager {
public:
    enum Mode {
        OWNER,
        GUEST,
        DEVELOPER
    };


    enum Reason {
        USER_REQUEST,
        IOT_REQUEST,
        TIME_EXPIRED,
        SECURITY_EVENT,
        SYSTEM_REBOOT
    };

    using ModeChangedCallback = void (*)(Mode oldMode, Mode newMode, Reason reason);

private:
    Mode currentMode = OWNER;
    unsigned long guestExpireAt = 0;
    bool logsPaused = false;
    bool initialized = false;   // 🔥 evita reset involontario al primo ciclo

    ModeChangedCallback onModeChanged = nullptr;

    // IOT integration
    bool iotGuestRequest = false;
    unsigned long iotGuestDuration = 0;

    // ---------------------------------------------------------
    // INTERNAL HELPERS
    // ---------------------------------------------------------
    const char* modeToStr(Mode m) const {
        switch (m) {
            case OWNER:     return "OWNER";
            case GUEST:     return "GUEST";
            case DEVELOPER: return "DEVELOPER";
        }
        return "UNKNOWN";
    }


    const char* reasonToStr(Reason r) const {
        switch (r) {
            case USER_REQUEST:   return "USER_REQUEST";
            case IOT_REQUEST:    return "IOT_REQUEST";
            case TIME_EXPIRED:   return "TIME_EXPIRED";
            case SECURITY_EVENT: return "SECURITY_EVENT";
            case SYSTEM_REBOOT:  return "SYSTEM_REBOOT";
        }
        return "UNKNOWN";
    }

    void triggerCallback(Mode oldMode, Mode newMode, Reason reason) {
        LOG_DF("OwnerManager",
            "Cambio modalità: %s → %s (reason=%s)",
            modeToStr(oldMode),
            modeToStr(newMode),
            reasonToStr(reason));

        if (onModeChanged)
            onModeChanged(oldMode, newMode, reason);
    }

public:
    OwnerManager() {}

    // ---------------------------------------------------------
    // EVENT HOOKS
    // ---------------------------------------------------------
    void setCallback(ModeChangedCallback cb) {
        onModeChanged = cb;
        LOG_DF("OwnerManager", "Callback registrato");
    }

    bool hasCallback() const {
        return onModeChanged != nullptr;
    }

    // ---------------------------------------------------------
    // SET MODE
    // ---------------------------------------------------------
    void setOwner(Reason reason = USER_REQUEST) {
        if (currentMode == OWNER) return;

        Mode old = currentMode;
        currentMode = OWNER;

        triggerCallback(old, OWNER, reason);
    }

    void setGuest(unsigned long expireTimestamp, Reason reason = USER_REQUEST) {
        Mode old = currentMode;
        currentMode = GUEST;
        guestExpireAt = expireTimestamp;

        LOG_DF("OwnerManager",
               "Set mode → GUEST (expire in %lu ms)",
               (expireTimestamp > millis()) ? (expireTimestamp - millis()) : 0);

        triggerCallback(old, GUEST, reason);
    }

    void setDeveloper(Reason reason = USER_REQUEST) {
        if (currentMode == DEVELOPER) return;

        Mode old = currentMode;
        currentMode = DEVELOPER;

        // 🔥 Developer = log sempre attivi
        LogManager::enable();

        triggerCallback(old, DEVELOPER, reason);
    }

    // ---------------------------------------------------------
    // EXPIRATION / UPDATE
    // ---------------------------------------------------------
    bool isGuestExpired(unsigned long now) const {
        return (currentMode == GUEST) && (now > guestExpireAt);
    }

    void update(unsigned long now) {
        // 🔥 Primo ciclo → non fare nulla
        if (!initialized) {
            initialized = true;
            return;
        }

        if (currentMode == GUEST && now > guestExpireAt) {
            LOG_DF("OwnerManager", "Guest scaduto → ritorno a OWNER");
            setOwner(TIME_EXPIRED);
        }

        if (iotGuestRequest) {
            LOG_DF("OwnerManager",
                "Richiesta IOT → attivo Guest per %lu ms",
                iotGuestDuration);

            iotGuestRequest = false;
            setGuest(now + iotGuestDuration, IOT_REQUEST);
        }
    }


    bool hasPendingIOTRequest() const {
        return iotGuestRequest;
    }

    unsigned long getPendingIOTDuration() const {
        return iotGuestDuration;
    }

    // ---------------------------------------------------------
    // PERMISSIONS
    // ---------------------------------------------------------
    bool canModifySettings() const {
        return currentMode == OWNER;
    }

    bool canControlAllDevices() const {
        return currentMode == OWNER;
    }

    bool canAccessLogs() const {
        return currentMode == OWNER;
    }

    bool canUseAutomation() const {
        return currentMode == OWNER;
    }

    bool areLogsPaused() const {
        return logsPaused;
    }

    // ---------------------------------------------------------
    // ACCESSORS
    // ---------------------------------------------------------
    const char* getModeName() const {
        return modeToStr(currentMode);
    }

    Mode getMode() const {
        return currentMode;
    }

    unsigned long getGuestExpireAt() const {
        return guestExpireAt;
    }

    // ---------------------------------------------------------
    // LOGS
    // ---------------------------------------------------------
    void pauseLogs() {
        logsPaused = true;
        LogManager::disable();
    }

    void resumeLogs() {
        logsPaused = false;

        if (currentMode == DEVELOPER)
            LogManager::enable();
    }
};

class IpManager {
public:
    struct Config {
        unsigned long baseCooldownMs = 5000;      // ex COOLDOWN
        unsigned long extendedCooldownMs = 120000; // ex IP_COOLDOWN
        int maxErrorsBeforeCooldown = 2;          // ex MAX_ERRORS
        int maxErrorsBeforeExclude = 20;          // ex AUTO_EXCLUDE
        int maxIps = 32;                          // ex array statico 32
        int maxPriorities = 4;                    // ex Priorities[4]
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
        std::vector<PriorityMgmt> Priorities;
        size_t PrioritySize = 0;
        int Index = 0;

        structPriority() = default;

        structPriority(int maxPriorities)
            : Priorities(maxPriorities)
        {}
    };


    struct structIP {
        arduino::IPAddress IP;
        int Errors = 0;
        IpState state = IpState::OK;

        structPriority Priority;

        unsigned long lastErrorTime = 0;
        unsigned long cooldown = 0;

        bool exclude = false;
        int consecutiveErrors = 0;

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

    bool ExistsIp(arduino::IPAddress ip) {
        for (auto& item : IPs)
            if (item.IP == ip)
                return true;
        return false;
    }

    int GetUsedPriorities(arduino::IPAddress ip,
                          std::vector<GenericPrgDevice>& prgDevices,
                          std::vector<PriorityMgmt>& items)
    {
        for (auto& prgDevice : prgDevices) {
            if (prgDevice.GetIp() != ip)
                continue;

            bool exist = false;
            for (auto& it : items)
                if (it.Priority == prgDevice.GetPriority())
                    exist = true;

            if (!exist) {
                PriorityMgmt tmp;
                tmp.Priority = prgDevice.GetPriority();
                tmp.DeviceIndex = -1;
                items.push_back(tmp);
            }
        }
        return (int)items.size();
    }

    void BuildDevicesByIP(std::vector<GenericPrgDevice> &prgDevices) {
        devicesByIP.clear();

        for (int i = 0; i < prgDevices.size(); i++) {
            uint32_t key =
                (uint32_t(prgDevices[i].GetIp()[0]) << 24) |
                (uint32_t(prgDevices[i].GetIp()[1]) << 16) |
                (uint32_t(prgDevices[i].GetIp()[2]) << 8)  |
                 uint32_t(prgDevices[i].GetIp()[3]);

            devicesByIP[key].push_back(i);
        }
    }

public:
    // -------------------------
    // Accesso ai device per IP
    // -------------------------
    std::vector<int>* GetDevicesByIP(arduino::IPAddress &ip) {
        uint32_t key =
            (uint32_t(ip[0]) << 24) |
            (uint32_t(ip[1]) << 16) |
            (uint32_t(ip[2]) << 8)  |
             uint32_t(ip[3]);

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

                for (size_t j = 0; j < tmpPriorities.size(); j++)
                    newItem.Priority.Priorities[j] = tmpPriorities[j];

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
    bool ExistDevicesByIp(std::vector<GenericPrgDevice>& prgDevices, int ipIdx) {
        if (ipIdx < 0 || ipIdx >= IPs.size())
            return false;

        auto ip = IPs[ipIdx].IP;
        for (auto& prgDevice : prgDevices)
            if (prgDevice.GetIp() == ip)
                return true;

        return false;
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
    PriorityMgmt& GetCurrentPriority(int ipIdx) {
        return IPs[ipIdx].Priority.Priorities[ IPs[ipIdx].Priority.Index ];
    }

    void AdvancePriority(int ipIdx, int items) {
        auto &p = IPs[ipIdx].Priority;

        int jump = GetJump(p.Priorities[p.Index].Priority);

        if (p.Priorities[p.Index].DeviceIndex + jump >= items ||
            p.Priorities[p.Index].DeviceIndex == -1)
        {
            p.Priorities[p.Index].DeviceIndex = 0;
        } else {
            p.Priorities[p.Index].DeviceIndex += jump;
        }

        if (p.Index >= p.PrioritySize - 1)
            p.Index = 0;
        else
            p.Index++;
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




/* ============================================================================
   FASTCHANGEDTRACKER — SISTEMA O(1) PER LA GESTIONE DEI "CHANGED"
   ----------------------------------------------------------------------------
   • Sostituisce completamente unordered_map per tracciare gli elementi mutati.
   • Usa una maschera booleana (mask[]) per marcare rapidamente ogni chiave.
   • Mantiene una lista lineare (list[]) degli elementi modificati in ordine
     di arrivo, garantendo iterazioni estremamente veloci e cache‑friendly.
   • Ogni operazione (mark, reset, has, iterazione) è O(1) reale, senza hashing.
   • Nessuna allocazione dinamica: array statici dimensionati a compile‑time.
   • clearAll() azzera solo il contatore, evitando costi inutili di pulizia.
   • forEach() permette di iterare solo sugli elementi effettivamente mutati.
   • Ideale per cicli ad alta frequenza come Modbus, dove ogni millisecondo
     conta e le strutture STL diventano un collo di bottiglia.
   • Garantisce coerenza totale con WriteElement(), ResetElement e routing.
   • Riduce il tempo ciclo Modbus di 1–3 ms eliminando hashing e lookup lenti.
   • Progettato per essere drop‑in: Buffer lo integra senza modifiche invasive.
   • Nessun rischio di duplicati: mark() controlla mask[] prima di inserire.
   • Perfetto per sistemi embedded con RAM limitata e necessità di determinismo.
   • Comportamento stabile anche con centinaia di changed consecutivi.
   • Facilmente estendibile per debug, profiling o statistiche runtime.
   • Codice minimale, leggibile e completamente privo di overhead STL.
   • Pensato per sostituire definitivamente getChangedMapByType().
   • Garantisce massima prevedibilità temporale (no jitter).
   ============================================================================ */


class ModbusManager {     
private:
    const int NOT_VALID_AREA=0;
    
    class PersistentModbusConnection {
    private:
        ModbusTCPClient& cli;
        IPAddress lastIp;
        bool hasConnection = false;

        // 🔥 Timestamp dell’ultima attività reale (READ o WRITE)
        unsigned long lastActivity = 0;

    public:
        PersistentModbusConnection(ModbusTCPClient& client)
            : cli(client)
        {
            lastIp = IPAddress(0,0,0,0);
            lastActivity = 0;
        }

        // 🔥 Chiamare SEMPRE quando fai una READ o WRITE
        inline void touch(unsigned long now) {
            lastActivity = now;
        }

        bool ensure(int ipIndex, IpManager& ipManager, int port, unsigned long now)
        {
            auto& ipStruct = ipManager.GetIps()[ipIndex];
            IPAddress ip = ipStruct.IP;

            bool needReconnect = false;

            // 1) Socket non connessa → riconnetti
            if (!cli.connected())
                needReconnect = true;

            // 2) IP cambiato → riconnetti
            else if (lastIp != ip)
                needReconnect = true;

            // 3) Inattività troppo lunga → modulo DO ha chiuso la connessione
            else if (now - lastActivity > 300)   // 🔥 valore perfetto per Waveshare/USR
                needReconnect = true;

            if (!needReconnect) {
                // Connessione OK
                if (ipStruct.state != IpManager::IpState::OK)
                    ipManager.ReportSuccess(ipIndex);
                return true;
            }

            // 🔥 Riconnessione
            cli.stop();
            delay(5);

            if (!cli.begin(ip, port)) {
                hasConnection = false;
                ipManager.ReportError(ipIndex, now);
                return false;
            }

            // 🔥 Connessione OK
            lastIp = ip;
            lastActivity = now;
            hasConnection = true;

            if (ipStruct.state != IpManager::IpState::OK)
                ipManager.ReportSuccess(ipIndex);

            return true;
        }

        bool connected() const {
            return hasConnection && cli.connected();
        }
    };

    class AreaDeviceMap {
    public:
        struct Entry {
            int devIndex;
            int channel;
            int itemIndex;
        };

        // Costruisce la mappa solo la prima volta
        inline void buildOnce(std::vector<GenericPrgDevice> &devices) {
            if (!initialized) {
                buildInternal(devices);
                initialized = true;
            }
        }

        // Lookup O(1)
        inline bool find(int area, Entry &out) const {
            auto it = map.find(area);
            if (it == map.end())
                return false;
            out = it->second;
            return true;
        }

        inline bool isInitialized() const { return initialized; }
    private:
        std::unordered_map<int, Entry> map;
        bool initialized = false;

        void buildInternal(std::vector<GenericPrgDevice> &devices) {
            map.clear();
            map.reserve(devices.size() * 8); // ottimizzazione minima

            for (int d = 0; d < devices.size(); d++) {
                auto &dev = devices[d];
                int channels = dev.GetChannelsSize();

                for (int ch = 0; ch < channels; ch++) {
                    auto chInfo = dev.GetChannelInfo(ch);
                    int items = chInfo.items;

                    for (int i = 0; i < items; i++) {
                        int area = dev.GetArea(ch, i);

                        map[area] = Entry{
                            d,      // device index
                            ch,     // channel
                            i       // item index
                        };
                    }
                }
            }
        }
    };
public:
    AreaDeviceMap areaMap;
    RouteManager routeManager;

    // --- CALLBACKS ---
    using SomethingChangedFn = void (*)();
    
    // --- RIFERIMENTI A OGGETTI ESTERNI ---
    Buffer *buffer = nullptr;
    std::vector<GenericPrgDevice> *prgDevices = nullptr;
    ToggleManager *toggles = nullptr;
    SplitOutManager *splitsOut = nullptr;
    AnalogThresholdManager* thresholds = nullptr;
    IpManager *ipManager = nullptr;

    //Routing Callback
    using RouteTimingFn = void (*)(unsigned long exec);
    RouteTimingFn routeTiming = nullptr;

    // --- LED ---
    LedController* m_ledController = nullptr; // pointer to external controller (not owned)

    // --- INIT ---
    void Begin(LedController& ledsController,
            Buffer &buffer,
            std::vector<GenericPrgDevice> &prgDevices,
            ToggleManager &toggles,
            SplitOutManager &splitsOut,
            IpManager &ipManager,
            AnalogThresholdManager &tresholds,
            SomethingChangedFn sc,
            RouteTimingFn rt)   {
        this->m_ledController = &ledsController;
        this->buffer = &buffer;
        this->prgDevices = &prgDevices;
        this->toggles = &toggles;
        this->splitsOut = &splitsOut;
        this->thresholds = &tresholds;
        this->ipManager = &ipManager;
        this->somethingChanged = sc;
        this->routeTiming = rt;

        areaMap.buildOnce(prgDevices);
    }

    enum class ClientState {
        READ_DONE,     // lettura fatta, manca scrittura
        WRITE_DONE,    // scrittura fatta, manca lettura
        CYCLE_OK,      // read+write completate → avanza IP
        DEVICE_ERROR,
        ERROR          // errore → avanza IP
    };

    ClientState RunClient(ModbusTCPClient &cli,
                    short ipIndex, int port,
                    std::vector<uint16_t> &mbRead,
                    unsigned long now)
    {
        enum class Step { READ, WRITE };
        static Step step = Step::READ;
        static unsigned long lastReadTime = 0;

        auto &buf = *buffer;
        auto &devs = *prgDevices;
        static PersistentModbusConnection conn(cli);

        auto& ip = ipManager->GetIps()[ipIndex];

        LOG_DF("MDB", "RunClient(ip=%d, step=%d) at %lu", 
            ipIndex, (int)step, now);

        // --- CONNESSIONE ---
        if (!conn.ensure(ipIndex, *ipManager, port, now)) {
            LOG_WF("ModbusManager",
                "Modbus TCP non connesso → salto lettura (IP=%d.%d.%d.%d)",
                ip.IP[0], ip.IP[1], ip.IP[2], ip.IP[3]);

            step = Step::READ;
            return ClientState::ERROR;
        }

        // ============================================================
        // 1) FASE READ
        // ============================================================
        if (step == Step::READ) {
            LOG_DF("MDB", "READ phase start (ip=%d)", ipIndex);
            bool readOk = DeviceRead(cli, ipIndex, mbRead, now);
            if (!readOk) {
                step = Step::READ;
                return ClientState::DEVICE_ERROR;
            }
            conn.touch(now);
            lastReadTime = now;   // 🔥 timestamp della READ

            // --- PROCESS BUFFER (TUO CODICE ORIGINALE) ---
            const auto &changedMap = buffer->getChangedMapByType(Field);
            for (const auto &kv : changedMap) {
                int key = kv.first;
                int area = key / BufferFlagType_Count;
                
                const BufferSourceInfo &entry = buffer->getFieldEntry(area);
                const uint16_t value = entry.value;

                const int areaToWrite = buffer->GetAreaToWrite(area);
                const bool hasRoute = routeManager.hasRoute(area);

                LOG_DF("ModbusManager", " → Changed area=%d, To write=%d, Has route=%d",
                    area, areaToWrite, hasRoute);

                if (areaToWrite != 0 || toggles->isForwardArea(area) || hasRoute) {

                    if (areaToWrite > 0)
                        buffer->WriteElement(areaToWrite, Field, value, now);

                    bool changedOriginal = buffer->HasChanged(area, Field);

                    if (hasRoute && changedOriginal) {
                        unsigned long t0 = millis();
                        routeManager.execute(area, value, *buffer, now);

                        if (routeTiming)
                            routeTiming(millis() - t0);
                    }
                }

                if (!this->splitsOut->isSplitTarget(area)) {

                    if (this->splitsOut->exist(area)) {
                        if (value)
                            this->splitsOut->start(area);
                        else if (this->splitsOut->IsRunning(area))
                            this->splitsOut->reset(area);

                        LOG_DF("ModbusManager", " → Reset area=%d", area);
                        buffer->ResetElement(area, Field);
                    }
                    else if (areaToWrite != 0 || toggles->isForwardArea(area) || hasRoute) {
                        buffer->ResetElement(area, Field);
                        LOG_DF("ModbusManager", " → Reset area=%d", area);
                    }
                }
            }

            if (!changedMap.empty() && somethingChanged)
                somethingChanged();

            //LOG_DF("MDB", "READ phase start (ip=%d)", ipIndex);

            // PASSA ALLA SCRITTURA
            step = Step::WRITE;
            return ClientState::READ_DONE;
        }

        // ============================================================
        // 2) FASE WRITE
        // ============================================================
        if (step == Step::WRITE) {
            // Delay fisso minimo tra READ e WRITE
            const unsigned long fixedDelay = 5;  // 5 ms è perfetto per Waveshare/USR

            // Se non è ancora passato il delay → aspetta
            if (now - lastReadTime < fixedDelay) {
                LOG_WF("MDB", "WRITE DELAYED (ip=%d)", ipIndex);
                return ClientState::WRITE_DONE;
            }

            // Se è già passato (timeout di altri device) → scrivi subito            
            //LOG_DF("MDB", "WRITE DONE (ip=%d)", ipIndex);

            bool writeOk = DeviceWrite(cli, ip.IP, now);
            if (!writeOk) {
                step = Step::READ;
                return ClientState::DEVICE_ERROR;
            }

            conn.touch(now);

            // CICLO COMPLETO
            LOG_DF("MDB", "CYCLE_OK (ip=%d) → advance IP", ipIndex);

            step = Step::READ;
            return ClientState::CYCLE_OK;
        }

        return ClientState::ERROR;
    }


    void RunServer(LedController& ledsController,
                EthernetClient &client,
                char *itemName,
                bool mode,
                unsigned long now)  {
    

        static MgsModbus modbus( 502, 250 );

        if (ledsController.hasChannel(LedController::THREE)) {
            static bool ledState = false;
            ledState = !ledState;
            ledsController.set(LedController::THREE, ledState);
        }

        if (mode) {
            //WRITE
            auto changedToPanel = buffer->getChangedMapByType(ToPanel);
            if (!changedToPanel.empty()) {
                LOG_DF("ModbusManager", "Write to panel: %d elementi modificati", changedToPanel.size());

                for (const auto &kv : changedToPanel) {
                    int key = kv.first;
                    int area = key / BufferFlagType_Count;
                    int type = key % BufferFlagType_Count;
                    BufferFlagType flag = static_cast<BufferFlagType>(type);

                    BufferSourceInfo tmp;
                    buffer->GetData(area, flag, tmp);

                    modbus.MbData[area] = tmp.value;
                    buffer->ResetElement(area, ToPanel);
                    buffer->WriteElement(area, FromPanel, tmp.value, true, now);
                }
                modbus.MbsRun(client);
            }
        }
        else {
            //READ
            modbus.MbsRun(client); //Leggo prima da modbus -> Aggiorno il campo

            BufferArrayInfo arr = buffer->GetToReadFromPanel();
            int maxArea = buffer->size();
            int count = arr.size > maxArea ? maxArea : arr.size;

            for (int i = 0; i < count; i++) {
                int area = arr.itemsPtr[i];

                // area invalida → salta
                if (area < 0 || area >= maxArea) {
                    LOG_WF("ModbusManager",
                        "HMI READ skip: invalid area=%d (maxArea=%d)",
                        area, maxArea);
                    continue;
                }

                word val = modbus.MbData[area];

                if (buffer->Compare(area, FromPanel, val) != 2) {
                    buffer->WriteElement(area, FromPanel, val, now);

                    if (buffer->WriteElement(area, Field, val, now))
                        buffer->ResetElement(area, FromPanel);

                    if (buffer->CanWriteToPanel(area))
                        buffer->WriteElement(area, ToPanel, val, true, now);
                }
            }
        }
    }

private:
    SomethingChangedFn somethingChanged = nullptr;

    // --- LETTURA ---
    bool DeviceRead(ModbusTCPClient &cli,
                    short ipIndex,
                    std::vector<uint16_t> &mbRead,
                    unsigned long now) {
        return DeviceManagement_Read(
            m_ledController,
            cli,
            *ipManager,
            ipIndex,
            *buffer,
            *prgDevices,
            *toggles,
            *thresholds,
            now,
            mbRead
        );
    }

    // --- SCRITTURA ---
    bool DeviceWrite(ModbusTCPClient &cli,
                    arduino::IPAddress ip,
                    unsigned long now)  {
        return DeviceManagement_Write(
            m_ledController,
            cli,
            ip,
            *buffer,
            *prgDevices,
            now
        );
    }

    bool DeviceManagement_Write(LedController* ledsController,
                        ModbusTCPClient &modbusTCPCli,
                        arduino::IPAddress ip,
                        Buffer &buffer,
                        std::vector<GenericPrgDevice> &prgDevices,
                        unsigned long now) {

        bool inError = false;

        // 🔥 Ottieni i device associati all’IP corrente
        auto devicesForIP = ipManager->GetDevicesByIP(ip);
        if (!devicesForIP || devicesForIP->empty()) {
            if (ledsController && ledsController->hasChannel(LedController::TWO)) {
                ledsController->set(LedController::TWO, false);
            }
            return true; // nessun device → nessuna scrittura
        }

        // 🔥 Iterazione diretta sulla unordered_map
        const auto &changedMap = buffer.getChangedMapByType(Field, true); //Skip virtual
        if (changedMap.empty()) {
            if (ledsController && ledsController->hasChannel(LedController::TWO)) {
                ledsController->set(LedController::TWO, false);
            }
            return true;   // oppure continua, se vuoi solo loggare
        } 

        // 🔥 Prepara lookup O(1)
        static std::unordered_set<int> devSet;
        devSet.clear();
        for (int idx : *devicesForIP)
            devSet.insert(idx);

        bool done=false;
        for (const auto &kv : changedMap) {
            int key = kv.first;
            int area = key / BufferFlagType_Count;
            if(area==NOT_VALID_AREA)
                continue;
                    
            // 🔥 Lookup O(1) area → device/channel
            AreaDeviceMap::Entry entry;
            if (!this->areaMap.find(area, entry)) {
                LOG_WF("ModbusManager", "areaMap not found → key=%d area=%d", key, area);
                continue;
            }

            // 🔥 Filtra per IP (O(1))
            if (!devSet.count(entry.devIndex)){
                continue;
            }

            GenericPrgDevice &dev = prgDevices[entry.devIndex];
            // 🔥 Device in errore → NON fare WRITE reale
            if (dev.GetError().IsInError()) {

                // 🔥 Mantieni vivo il retry window
                dev.GetError().Loop(true, now);

                /*LOG_WF("WRITE",
                    "Skip WRITE (device in error): %s area=%d IP=%d.%d.%d.%d",
                    dev.GetName(),
                    area,
                    dev.GetIp()[0], dev.GetIp()[1], dev.GetIp()[2], dev.GetIp()[3]);*/

                // 🔥 NON resettare il flag → la WRITE verrà riprovata quando rientra
                continue;
            }

            
            // Scrivo solo DO/AO
            auto chType = dev.GetChannelInfo(entry.channel).type;
            if (chType != GenericPrgDevice::DO &&
                chType != GenericPrgDevice::AO)
                continue;

            // Recupero valore
            BufferSourceInfo info;
            buffer.GetData(area, Field, info);

            //LOG_DF("ModbusManager", "Device TEST → area=%d channel=%d itemIndex=%d value=%d", area, entry.channel, entry.itemIndex, info.value);

            // 🔥 Scrittura Modbus
            if (dev.Write(modbusTCPCli, entry.channel, entry.itemIndex, info.value, now) == 0) {
                inError = true;
            
                //LOG_EF("ModbusManager", "FAIL to WRITE → area=%d channel=%d itemIndex=%d value=%d", area, entry.channel, entry.itemIndex, info.value);
            }

            // 🔥 Reset flag
            buffer.ResetElement(area, Field);
            done=true;
        }
        
        if (ledsController && ledsController->hasChannel(LedController::TWO)) {
            static bool ledState = false;
            if (done) {
                ledState = !ledState;
            } else ledState=false;
            ledsController->set(LedController::TWO, ledState);
        }

        return !inError;
    }

    void DeviceManagement_Read_SetOut(Buffer& buffer, int area, long value, unsigned long now) {
        // scrivo sempre l’area sorgente
        buffer.WriteElement(area, Field, value, now);

        int outArea = buffer.GetAreaToWrite(area);
        if (outArea >= 0) {
            buffer.WriteElement(outArea, Field, value, now);
        } else {
            LOG_WF("ModbusManager",
                "Skip forward: area=%d areaToWrite=%d",
                area, outArea);
        }
    }

    bool DeviceManagement_Read(LedController* ledsController, ModbusTCPClient &modbusTCPCli, IpManager &ipManager, short ipIndex, Buffer &buffer, 
        std::vector<GenericPrgDevice> &prgDevices, ToggleManager &toggles, AnalogThresholdManager &tresholds, unsigned long now, std::vector<uint16_t> &mbRead) {

        bool _error=false;
        static GenericPrgDeviceManager manager;
        static bool cacheBuilt = false;

        if (!cacheBuilt) {
            manager.BuildPriorityCache(prgDevices);
            cacheBuilt = true;
        }
        
        PriorityMgmt &_actualPriority = ipManager.GetCurrentPriority(ipIndex);

        std::vector<int> _devices;
        int _items=manager.GetDevicesByPriority(_actualPriority.Priority, ipManager.GetIp(ipIndex).IP, _devices);  //Get all devices under same IP address (device under Waveshare)
        if(_items>0) {    
            int _start=0;
            int _end=_items;
            
            if (ledsController && ledsController->hasChannel(LedController::ONE)) {
                static bool ledState = false;
                ledState = !ledState;
                ledsController->set(LedController::ONE, ledState);
            }

            if(_actualPriority.DeviceIndex!=-1) {
                //Passa la prima inizializzazione nella quale faccio polling di tutte le periferiche
                
                int _jump=GetJump(_actualPriority.Priority);
                switch (_actualPriority.Priority)   {
                    case Low:
                    _start=_actualPriority.DeviceIndex;
                    _end=_actualPriority.DeviceIndex+_jump>=_items?_items:_actualPriority.DeviceIndex+_jump;
                    break;

                    case Medium:
                    _start=_actualPriority.DeviceIndex;
                    _end=_actualPriority.DeviceIndex+_jump>=_items?_items:_actualPriority.DeviceIndex+_jump;
                    break;
                    
                    case Normal:
                    _start=_actualPriority.DeviceIndex;
                    _end=_actualPriority.DeviceIndex+_jump>=_items?_items:_actualPriority.DeviceIndex+_jump;
                    break;
                }
            }

            for (int _deviceIndex = _start; _deviceIndex < _end; _deviceIndex++) {
                // 🔥 Recupero il device UNA SOLA VOLTA
                auto &dev = prgDevices[_devices[_deviceIndex]];

                if (dev.GetError().IsInError()) {
                    // 🔥 Manteniamo vivo il retry window
                    dev.GetError().Loop(true, now);

                    /*LOG_DF("MDB::READ", 
                        "Skip real read (device in error): %s IP=%d.%d.%d.%d Addr=%d",
                        dev.GetName(),
                        dev.GetIp()[0], dev.GetIp()[1], dev.GetIp()[2], dev.GetIp()[3],
                        dev.GetDeviceAddress());*/

                    // 🔥 NON bloccare il ciclo, passa al prossimo device
                    continue;
                }

                #ifdef DEBUG_VIEW
                LOG_IF("MDB::READ", 
                    "Device %d: %s IP=%d.%d.%d.%d PRIOR=%d",
                    _deviceIndex,
                    dev.GetName(),
                    dev.GetIp()[0], dev.GetIp()[1], dev.GetIp()[2], dev.GetIp()[3],
                    (int)dev.GetPriority());
                
                delay(500);
                #endif
                // Per ogni canale del device
                for (int channel = 0; channel < dev.GetChannelsSize(); channel++) { 
                    // 🔥 Recupero info del canale (veloce, evita chiamate ripetute)
                    auto chInfo = dev.GetChannelInfo(channel);
                    /*LOG_DF("MDB::READ::Channel",
                            "Dev=%d Ch=%d type=%d hw=%d items=%d start=%d",
                            _deviceIndex,
                            channel,
                            (int)chInfo.type,
                            (int)chInfo.hwType,
                            chInfo.items,
                            chInfo.startingAddr );*/

                    // Considero solo DI e AI
                    if (chInfo.type == GenericPrgDevice::DI ||
                        chInfo.type == GenericPrgDevice::AI) {
                        // 🔥 NUOVA READ: usa il buffer dinamico
                        auto _read= dev.Read(modbusTCPCli, channel, mbRead.data(), now);
                        if (_read.ok) 
                        {
                            // 🔥 Elaboro solo gli elementi realmente letti
                            for (int j = 0; j < _read.items; j++) {
                                int _index = _read.startIndex + j;
                                
                                LOG_DF("ModbusManager",
                                    "DMR: dev=%s | channel=%d | index=%d",
                                    dev.GetName(),
                                    channel,
                                    _index);
                                
                                // Area logica associata a questo item
                                int _area = dev.GetArea(channel, _index);
                                
                                bool _process = false;
                                BufferSourceInfo _buffer;
                                bool okData = buffer.GetData(_area, Field, _buffer);
                                if (!okData) {
                                    continue;
                                }

                                long _value = 0;
                                if(_read.itemsPerCall==1)
                                    _value = mbRead[j];
                                else {
                                    uint32_t raw = (uint32_t(mbRead[j]) << 16) | mbRead[j+1]; 
                                    float f; 
                                    memcpy(&f, &raw, sizeof(float)); 
                                    _value = f < 0 ? 0 : (unsigned long)(f * 100);
                                }

                                ToggleManager::ToggleSignalItem* _toggle = nullptr;

                                // --- ANALOGICO ---
                                if (chInfo.type == GenericPrgDevice::AI) {
                                    LOG_DF("ModbusManager","Pre treshold");

                                    int thr = thresholds->getThreshold(_area, _value);
                                    _process = abs(_value - _buffer.value) > thr;
                                    _process = _process || (_buffer.value==0 && _value!=0);

                                    LOG_DF("ModbusManager",
                                        "DMR Analog: dev=%s | channel=%d | index=%d | go=%d |thresh=%d",
                                        dev.GetName(),
                                        channel,
                                        _index, _process, thr);                                        
                                } 
                                else {
                                    LOG_DF("ModbusManager","Pre toggle");
                                    _toggle = toggles.get(_area);

                                    if (buffer.IsReverse(_area))
                                        _value = !_value;

                                    if (_toggle == nullptr) {
                                        _process = _value != _buffer.value;
                                    } 
                                    else {
                                        long _tmp = toggles.getForwardValue(_area, buffer);

                                        if (_tmp == 0)
                                            _tmp = _value;

                                        _process = (_tmp != _buffer.value ||
                                                    _tmp != _toggle->Toggle.getOldValue());
                                    }
                                } 

                                // 🔥 Se c’è variazione, aggiorno buffer e toggle
                                if (_process) {
                                    if(_value>0 && _area>=14){ //Prende sia digitali a 1 che analogiche
                                        int devIdx = _devices[_deviceIndex];

                                        LOG_IF("ModbusManager",
                                        "-> %s | channel=%d | index=%d | value=%d | buf.val=%d | buf.prev=%d | area=%d | areaName=%s",
                                        prgDevices[devIdx].GetName(),
                                        channel,
                                        _index,
                                        _value,
                                        _buffer.value,
                                        _buffer.prevValue,
                                        _area,
                                        buffer.GetName(_area));
                                    }
                                    
                                    buffer.ResetElement(_area, Field);

                                    if (_toggle != nullptr) {
                                        long _toggleOut = _buffer.value;
                                        long _signalIn = toggles.getForwardValue(_area, buffer);
                                        if (_signalIn == 0)
                                        _signalIn = _value;

                                        if (_toggle->Toggle.change(_signalIn, _toggleOut)) {
                                            DeviceManagement_Read_SetOut(buffer, _area, _toggleOut, now);
                                        }
                                    }
                                    else {
                                        DeviceManagement_Read_SetOut(buffer, _area, _value, now);
                                    }
                                } 
                            }
                        }
                        else {
                            LOG_DF("ModbusManager",
                                "Read error: %s | channel=%d | ip=%d.%d.%d.%d",
                                prgDevices[_devices[_deviceIndex]].GetName(),
                                channel,
                                ipManager.GetIp(ipIndex).IP[0],
                                ipManager.GetIp(ipIndex).IP[1],
                                ipManager.GetIp(ipIndex).IP[2],
                                ipManager.GetIp(ipIndex).IP[3]); 
                    
                            _error = true;
                            break;                        
                        }
                    }
                }
            }

            ipManager.AdvancePriority(ipIndex, _items);
        } 
        else {
            LOG_EF("ModbusManager",
                "Nothing to read | priority=%d | ip=%d.%d.%d.%d",
                _actualPriority.Priority,
                ipManager.GetIp(ipIndex).IP[0],
                ipManager.GetIp(ipIndex).IP[1],
                ipManager.GetIp(ipIndex).IP[2],
                ipManager.GetIp(ipIndex).IP[3]);
        }

        return !_error;
    }
};

#endif
