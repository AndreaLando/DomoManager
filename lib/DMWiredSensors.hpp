#ifndef DMWiredSensors_HPP
#define DMWiredSensors_HPP

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

  

#include <Arduino.h>

#include "DMSignal.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

const unsigned long RT_DELAY      = 50;   // 0.25 sec
const unsigned long INITIAL_DELAY = 500;   // 0.5 sec

enum class SensorChannelType {
    RT,
    H24,
    LEN,
    MASK
};

enum class SensorCategory {
    PIR,
    WINDOW,
    DOOR,
    FLOOD,
    SMOKE,
    TAMPER,
    OTHER
};

// -------------------------------------------------------------
//  SensorChannel
// -------------------------------------------------------------
class SensorChannel {
public:
    int pin;
    TON timer;
    FastDebounce debounce;
    bool mem = false;
    bool inhibit = false;
    SensorChannelType type;

    SensorChannel(int pin, float delayMs, SensorChannelType type)
        : pin(pin),
          type(type),
          timer(delayMs, TimerBase::Milliseconds),
          debounce(20)   // default debounce 20 ms
    {}
};


// -------------------------------------------------------------
//  Sensor
// -------------------------------------------------------------
class Sensor {
public:
    std::vector<SensorChannel> channels;
    std::unordered_map<SensorChannelType, SensorChannel*> lookup;

    bool _engage = false;
    bool _disabled = false;
    bool alarmOut = false;

    unsigned long startupInhibitMs = 2000;    // ignore alarms for 2 seconds
    TON startupInhibit = TON(startupInhibitMs, TimerBase::Milliseconds);
 
    Sensor(std::initializer_list<SensorChannel> list)
        : channels(list)
    {
        for (auto& ch : channels)
            lookup[ch.type] = &ch;
    }

    void SetStartupInhibit(unsigned long ms) {
        startupInhibitMs = ms;
        startupInhibit = TON(ms, TimerBase::Milliseconds);
    }

    SensorChannel* Get(SensorChannelType type) {
        auto it = lookup.find(type);
        return (it != lookup.end()) ? it->second : nullptr;
    }

    void Engage(bool mode) {
        _engage = mode;
    }

    void Enable(bool mode) {
        if (mode) {
            startupInhibit.Run(true);
        } else {
            for (auto& ch : channels) {
                ch.timer.Run(false);
                ch.mem = false;
            }
        }
        _disabled = !mode;
    }

    bool IsEnabled() const { return !_disabled; }
    bool IsEngaged() const { return _engage; }

    void Reset() {
        for (auto& ch : channels) {
            ch.timer.Run(false);
            ch.mem = false;
        }
    }

    // Check alarm for a specific channel type
    bool ChannelAlarm(SensorChannelType type) const {
        auto it = lookup.find(type);
        if (it == lookup.end())
            return false;

        const SensorChannel* ch = it->second;

        bool active = ch->timer.Q() && !ch->inhibit && startupInhibit.Q();
        return active || ch->mem;
    }

    // Main processing
    void Run(const bool* inputs, size_t count, unsigned long now) {
        bool tempAlarm = false;

        startupInhibit.Run(true, now);
        bool inhibit = !startupInhibit.Q();

        if (!_disabled) {

            for (size_t i = 0; i < channels.size(); i++) {
                auto& ch = channels[i];

                bool raw = false;
                if (i < count)
                    raw = inputs[i];

                bool debounced = ch.debounce.update(raw, now);

                // Startup inhibit
                if (inhibit) {
                    ch.timer.Stop();
                    ch.mem = false;
                    continue;
                }

                // Reset per canale
                if (!debounced) {
                    ch.timer.Stop();
                    ch.mem = false;
                    continue;
                }

                // Timer normale
        
                ch.timer.Run(true, now);

                // Allarme canale
                if (ch.timer.Q() && !ch.inhibit) {
                    tempAlarm = true;
                    if (_engage)
                        ch.mem = true;
                }
            }

        } else {
            for (auto& ch : channels)
                ch.mem = false;
        }

        // Uscita aggregata
        alarmOut = tempAlarm;
        for (auto& ch : channels)
            alarmOut |= ch.mem;
    }


};


// -------------------------------------------------------------
//  AlarmDispatcher
// -------------------------------------------------------------
class AlarmDispatcher {
public:
    using Callback = std::function<void(
        const std::string& zone,
        SensorChannelType type,
        const std::vector<Sensor*>& sensors
    )>;

    // Global callbacks (any zone, any type)
    std::vector<Callback> globalCallbacks;

    // Callbacks per alarm type
    std::unordered_map<SensorChannelType, std::vector<Callback>> typeCallbacks;

    // Callbacks per zone (any type)
    std::unordered_map<std::string, std::vector<Callback>> zoneCallbacks;

    // Callbacks per (zone + type)
    std::unordered_map<std::string,
        std::unordered_map<SensorChannelType, std::vector<Callback>>
    > zoneTypeCallbacks;


    // -------------------------------
    // Registration
    // -------------------------------
    void OnAnyAlarm(Callback cb) {
        globalCallbacks.push_back(cb);
    }

    void OnAlarmType(SensorChannelType type, Callback cb) {
        typeCallbacks[type].push_back(cb);
    }

    void OnZoneAlarm(const std::string& zone, Callback cb) {
        zoneCallbacks[zone].push_back(cb);
    }

    void OnZoneAlarmType(const std::string& zone, SensorChannelType type, Callback cb) {
        zoneTypeCallbacks[zone][type].push_back(cb);
    }


    // -------------------------------
    // Dispatch
    // -------------------------------
    void Dispatch(
        const std::string& zone,
        SensorChannelType type,
        const std::vector<Sensor*>& sensors ) {
        
        // Global callbacks
        for (auto& cb : globalCallbacks)
            cb(zone, type, sensors);

        // Type-specific callbacks
        auto itType = typeCallbacks.find(type);
        if (itType != typeCallbacks.end())
            for (auto& cb : itType->second)
                cb(zone, type, sensors);

        // Zone-specific callbacks
        auto itZone = zoneCallbacks.find(zone);
        if (itZone != zoneCallbacks.end())
            for (auto& cb : itZone->second)
                cb(zone, type, sensors);

        // Zone + Type callbacks
        auto itZT = zoneTypeCallbacks.find(zone);
        if (itZT != zoneTypeCallbacks.end()) {
            auto itZT2 = itZT->second.find(type);
            if (itZT2 != itZT->second.end())
                for (auto& cb : itZT2->second)
                    cb(zone, type, sensors);
        }
    }
};


// -------------------------------------------------------------
//  ZoneManager
// -------------------------------------------------------------
class ZoneManager {
public:
    using ZoneName = std::string;

    std::vector<Sensor*> sensors;
    std::unordered_map<ZoneName, std::vector<Sensor*>> zones;

    // Track last alarm state per channel type
    std::unordered_map<SensorChannelType, bool> lastState;

    // Track snoozed alarms per channel type
    std::unordered_map<SensorChannelType, bool> snoozed;

    AlarmDispatcher dispatcher;


    // -------------------------------
    // Registration
    // -------------------------------
    void AddSensor(Sensor* sensor) {
        sensors.push_back(sensor);
    }

    void AddToZone(const ZoneName& zone, Sensor* sensor) {
        zones[zone].push_back(sensor);
    }

    const std::vector<Sensor*>& GetZone(const ZoneName& zone) const {
        static const std::vector<Sensor*> empty;
        auto it = zones.find(zone);
        return (it != zones.end()) ? it->second : empty;
    }


    // -------------------------------
    // Alarm Queries
    // -------------------------------
    bool ZoneAlarm(const ZoneName& zone) const {
        for (auto* s : GetZone(zone))
            if (s->alarmOut)
                return true;
        return false;
    }

    bool AnyAlarm() const {
        for (auto* s : sensors)
            if (s->alarmOut)
                return true;
        return false;
    }

    bool ZoneAlarmByType(const ZoneName& zone, SensorChannelType type) const {
        for (auto* s : GetZone(zone))
            if (s->ChannelAlarm(type))
                return true;
        return false;
    }

    bool AnyAlarmByType(SensorChannelType type) const {
        for (auto* s : sensors)
            if (s->ChannelAlarm(type))
                return true;
        return false;
    }

    std::vector<Sensor*> SensorsInAlarm(SensorChannelType type) const {
        std::vector<Sensor*> out;
        for (auto* s : sensors)
            if (s->ChannelAlarm(type))
                out.push_back(s);
        return out;
    }


    // -------------------------------
    // New Alarm Detection + Snooze
    // -------------------------------
    void ProcessAllTypes() {
        static const SensorChannelType types[] = {
            SensorChannelType::RT,
            SensorChannelType::H24,
            SensorChannelType::MASK,
            SensorChannelType::LEN
        };

        for (SensorChannelType type : types) {
            bool current = AnyAlarmByType(type);
            bool previous = lastState[type];
            bool isSnoozed = snoozed[type];

            if (!current)
                snoozed[type] = false;

            bool newAlarm = current && !previous && !isSnoozed;
            lastState[type] = current;

            if (!newAlarm)
                continue;

            // Dispatch per-zone events
            for (auto& [zoneName, zoneSensors] : zones) {
                std::vector<Sensor*> active;

                for (auto* s : zoneSensors)
                    if (s->ChannelAlarm(type))
                        active.push_back(s);

                if (!active.empty())
                    dispatcher.Dispatch(zoneName, type, active);
            }
        }
    }

    bool NewAlarmByType(SensorChannelType type) {
        bool current = AnyAlarmByType(type);
        bool previous = lastState[type];
        bool isSnoozed = snoozed[type];

        if (!current)
            snoozed[type] = false;

        bool newAlarm = current && !previous && !isSnoozed;
        lastState[type] = current;

        if (!newAlarm)
            return false;

        // Dispatch per-zone events
        for (auto& [zoneName, zoneSensors] : zones) {
            std::vector<Sensor*> active;

            for (auto* s : zoneSensors)
                if (s->ChannelAlarm(type))
                    active.push_back(s);

            if (!active.empty())
                dispatcher.Dispatch(zoneName, type, active);
        }

        return true;
    }

    void Snooze(SensorChannelType type) {
        snoozed[type] = true;
    }


    // -------------------------------
    // Reset
    // -------------------------------
    void ResetZone(const ZoneName& zone) {
        for (auto* s : GetZone(zone))
            s->Reset();
    }

    void ResetAll() {
        for (auto* s : sensors)
            s->Reset();
    }


    // -------------------------------
    // Enable / Disable
    // -------------------------------
    void EnableZone(const ZoneName& zone, bool mode) {
        for (auto* s : GetZone(zone))
            s->Enable(mode);
    }

    void EnableAll(bool mode) {
        for (auto* s : sensors)
            s->Enable(mode);
    }


    // -------------------------------
    // Engage / Disengage
    // -------------------------------
    void EngageZone(const ZoneName& zone, bool mode) {
        for (auto* s : GetZone(zone))
            s->Engage(mode);
    }

    void EngageAll(bool mode) {
        for (auto* s : sensors)
            s->Engage(mode);
    }
};

// -------------------------------------------------------------
//  GLOBAL CONFIG
// -------------------------------------------------------------
class WiredSensorsManager {
public:

    struct WiredSensorConfig {
        const char* zone;
        std::initializer_list<SensorChannel> channels;
        std::initializer_list<std::function<bool()>> readers;
        SensorCategory category;
        int cmdArea = -1;
    };

    using ConfigType = WiredSensorConfig;

    void Init(const ConfigType* cfg, size_t count) {
        config = cfg;
        configCount = count;

        sensors.reserve(count);

        for (size_t i = 0; i < count; i++) {
            Sensor* s = new Sensor(cfg[i].channels);
            sensors.push_back(s);

            // registra nel ZoneManager
            zones.AddSensor(s);
            zones.AddToZone(cfg[i].zone, s);

            // costruzione automatica della mappa zone → sensori
            zoneMap[cfg[i].zone].push_back(s);
        }

        LOG_DF("WiredSensors", "Init: configurati %u sensori", (unsigned)count);
    }

    void Init(const ConfigType* cfg, size_t count, uint32_t startupInhibitMs) {
        config = cfg;
        configCount = count;

        sensors.reserve(count);

        for (size_t i = 0; i < count; i++) {
            Sensor* s = new Sensor(cfg[i].channels);

            // 🔥 applica startup inhibit globale
            s->SetStartupInhibit(startupInhibitMs);

            sensors.push_back(s);

            zones.AddSensor(s);
            zones.AddToZone(cfg[i].zone, s);
            zoneMap[cfg[i].zone].push_back(s);
        }

        LOG_DF("WiredSensors",
            "Init: configurati %u sensori (startup inhibit=%u ms)",
            (unsigned)count, startupInhibitMs);
    }

    // --- Aggregate state computed from wired sensors
    struct AggregateState {
        bool intrusion = false;
        bool intrusionH24 = false;
        bool mask = false;
        bool flood = false;
        bool smoke = false;
        bool windowsOpen = false;
        bool doorsOpen = false;
        bool tamper = false;
    };

    void Process(uint32_t now) {
        const ConfigType* cfg = config;
        size_t count = configCount;

        for (size_t i = 0; i < count; ++i) {
            Sensor* s = sensors[i];
            if (!s) continue;

            const auto& c = cfg[i];

            bool tmp[8];
            size_t n = 0;

            // Lettura tramite funzioni configurate
            for (auto& fn : c.readers) {
                if (n >= 8) break;
                tmp[n++] = fn();
            }

            if (n > 0) {
                s->Run( tmp, n, now );
                LOG_DF("WiredSensors", "Process: sensor[%u] zone=%s alarmOut=%d",
                    (unsigned)i, c.zone, s->alarmOut ? 1 : 0);
            }
        }
    }


    // --- Compute aggregated state from current sensors
    AggregateState ComputeAggregate() const {
        AggregateState st;

        const ConfigType* cfg = config;
        size_t count = configCount;
        if (!cfg || count == 0) return st;

        for (size_t i = 0; i < count; ++i) {
            const auto& c = cfg[i];
            Sensor* s = sensors[i];
            if (!s) continue;

            switch (c.category) {
                case SensorCategory::PIR:                    
                {
                    // Movimento → Intrusione
                    if (s->ChannelAlarm(SensorChannelType::RT))
                        st.intrusion = true;

                    // Tamper PIR → Intrusione H24
                    if (s->ChannelAlarm(SensorChannelType::H24))
                        st.intrusionH24 = true;

                    // Anti-mask PIR → Mask
                    if (s->ChannelAlarm(SensorChannelType::MASK))
                        st.mask = true;

                    break;
                }

                case SensorCategory::WINDOW:
                    if (s->alarmOut)
                        st.windowsOpen = true;
                    break;

                case SensorCategory::DOOR:
                    if (s->alarmOut)
                        st.doorsOpen = true;
                    break;

                case SensorCategory::FLOOD:
                    if (s->alarmOut)
                        st.flood = true;
                    break;

                case SensorCategory::SMOKE:
                    if (s->alarmOut)
                        st.smoke = true;
                    break;

                case SensorCategory::TAMPER:
                    if (s->alarmOut)
                        st.tamper = true;
                    break;

                default:
                    break;
            }
        }

        LOG_DF("WiredSensors", "ComputeAggregate: intrusion=%d intrusionH24=%d flood=%d smoke=%d windows=%d doors=%d tamper=%d",
              st.intrusion ? 1 : 0, st.intrusionH24 ? 1 : 0, st.flood ? 1 : 0, st.smoke ? 1 : 0,
              st.windowsOpen ? 1 : 0, st.doorsOpen ? 1 : 0, st.tamper ? 1 : 0);

        return st;
    }

    // --- GETTER AUTOMATICI ---
    const std::unordered_map<std::string, std::vector<Sensor*>>& GetZoneMap() const {
        return zoneMap;
    }

    // tutti i sensori di una zona
    const std::vector<Sensor*>& GetByZone(const char* zone) {
        return zoneMap[zone];
    }

    // primo sensore di una zona
    Sensor* GetFirst(const char* zone) {
        auto& v = zoneMap[zone];
        return v.empty() ? nullptr : v[0];
    }

    // accesso al ZoneManager interno
    ZoneManager& Zones() { return zones; }

    // accesso diretto ai sensori
    Sensor* GetSensor(size_t i) { return sensors[i]; }
    Sensor* GetSensor(size_t i) const { return sensors[i]; }


    size_t Count() const { return configCount; }
    const ConfigType* GetConfig() const { return config; }

private:
    ZoneManager zones;
    std::vector<Sensor*> sensors;

    const ConfigType* config = nullptr;
    size_t configCount = 0;

    // mappe generate automaticamente
    std::unordered_map<std::string, std::vector<Sensor*>> zoneMap;
};

class AlarmBitmaskManager {
public:
    using AlarmCallback = void (*)(uint64_t newMask,
                                   uint64_t currentMask,
                                   uint64_t memMask,
                                   size_t bitIndex,
                                   size_t sensorIndex,
                                   SensorChannelType type);

    struct SignalInfo {
        size_t sensorIndex;
        SensorChannelType type;
    };

    // --- BITMASKS ---
    uint64_t currentMask = 0;
    uint64_t memMask     = 0;
    uint64_t newMask     = 0;

    bool engage = false;

    // --- CALLBACK ---
    AlarmCallback onNewAlarm = nullptr;

    // --- MAPPATURA BIT → SEGNALE ---
    std::vector<SignalInfo> map;

    // ---------------------------------------------------------
    // INIT: costruisce la mappa bitIndex → (sensore, canale)
    // ---------------------------------------------------------
    void BuildMap(WiredSensorsManager& ws) {
        map.clear();

        for (size_t i = 0; i < ws.Count(); i++) {
            Sensor* s = ws.GetSensor(i);

            for (auto& ch : s->channels) {
                map.push_back({ i, ch.type });
            }
        }
    }

    // ---------------------------------------------------------
    // ENGAGE
    // ---------------------------------------------------------
    void SetEngage(bool mode) {
        engage = mode;
    }

    // ---------------------------------------------------------
    // RESET
    // ---------------------------------------------------------
    void ResetMemory() {
        memMask = 0;
        Update();   // ricalcola newMask
    }

    // ---------------------------------------------------------
    // CALCOLO BITMASK ATTUALE
    // ---------------------------------------------------------
    void ComputeCurrent(WiredSensorsManager& ws) {
        currentMask = 0;

        size_t bit = 0;

        for (size_t i = 0; i < ws.Count(); i++) {
            Sensor* s = ws.GetSensor(i);

            for (auto& ch : s->channels) {
                bool active = (ch.timer.Q() || ch.mem);

                if (active) {
                    currentMask |= (uint64_t(1) << bit);
                }

                bit++;
            }
        }

        Update();
    }

    // ---------------------------------------------------------
    // CALLBACK + MEMORIZZAZIONE
    // ---------------------------------------------------------
    void Update() {
        if (engage) {
            memMask |= currentMask;
        }

        uint64_t oldNewMask = newMask;
        newMask = currentMask & ~memMask;

        // callback per ogni nuovo bit
        if (newMask != 0 && newMask != oldNewMask) {
            if (onNewAlarm) {
                for (size_t bit = 0; bit < map.size(); bit++) {
                    if (newMask & (uint64_t(1) << bit)) {
                        auto& info = map[bit];
                        onNewAlarm(newMask, currentMask, memMask,
                                   bit, info.sensorIndex, info.type);
                    }
                }
            }
        }
    }

    void SetCallback(AlarmCallback cb) {
        onNewAlarm = cb;
    }
};



#endif
