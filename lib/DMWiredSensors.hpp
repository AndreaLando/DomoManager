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
    SensorChannelType type;

private:
    TON timer;
    FastDebounce debounce;

    bool mem = false;
    bool inhibit = false;

    bool lastDebounced = false;
    bool lastAlarmState = false;

    friend class Sensor;

public:
    SensorChannel(
        int pin,
        float delayMs,
        SensorChannelType type)
        : pin(pin),
          type(type),
          timer(delayMs, TimerBase::Milliseconds),
          debounce(20),
          lastDebounced(false),
          lastAlarmState(false)
    {}

    inline bool IsActive() const
    {
        return timer.Q() || mem;
    }

    inline bool IsInhibit() const
    {
        return inhibit;
    }

    inline bool GetLastAlarmState() const
    {
        return lastAlarmState;
    }

    inline void SetLastAlarmState(bool state)
    {
        lastAlarmState = state;
    }
};


// -------------------------------------------------------------
//  Sensor
// -------------------------------------------------------------
class Sensor {
public:
    std::vector<SensorChannel> channels;
    std::unordered_map<SensorChannelType, SensorChannel*> lookup;

    std::array<SensorChannel*, 4> channelByType{};

    bool _engage = false;
    bool _disabled = false;
    bool alarmOut = false;

    unsigned long startupInhibitMs = 2000;
    TON startupInhibit =
        TON(startupInhibitMs, TimerBase::Milliseconds);

    Sensor(std::initializer_list<SensorChannel> list)
        : channels(list)
    {
        channelByType.fill(nullptr);

        for (auto& ch : channels)
        {
            lookup[ch.type] = &ch;

            const size_t index =
                static_cast<size_t>(ch.type);

            if (index < channelByType.size())
                channelByType[index] = &ch;
        }
    }

    void SetStartupInhibit(unsigned long ms)
    {
        startupInhibitMs = ms;
        startupInhibit =
            TON(ms, TimerBase::Milliseconds);
    }

    SensorChannel* Get(SensorChannelType type)
    {
        const size_t index =
            static_cast<size_t>(type);

        if (index >= channelByType.size())
            return nullptr;

        return channelByType[index];
    }

    const SensorChannel* Get(SensorChannelType type) const
    {
        const size_t index =
            static_cast<size_t>(type);

        if (index >= channelByType.size())
            return nullptr;

        return channelByType[index];
    }

    void Engage(bool mode)
    {
        _engage = mode;
    }

    void Enable(bool mode)
    {
        if (mode)
        {
            startupInhibit.Run(true);
        }
        else
        {
            for (auto& ch : channels)
            {
                ch.timer.Run(false);
                ch.mem = false;
                ch.lastDebounced = false;
            }
        }

        _disabled = !mode;
    }

    bool IsEnabled() const
    {
        return !_disabled;
    }

    bool IsEngaged() const
    {
        return _engage;
    }

    void Reset()
    {
        for (auto& ch : channels)
        {
            ch.timer.Run(false);
            ch.mem = false;
            ch.lastDebounced = false;
        }
    }

    bool ChannelAlarm(SensorChannelType type) const
    {
        const size_t index =
            static_cast<size_t>(type);

        if (index >= channelByType.size())
            return false;

        const SensorChannel* ch =
            channelByType[index];

        if (!ch)
            return false;

        return
            (ch->timer.Q() &&
             !ch->inhibit &&
             startupInhibit.Q())
            ||
            ch->mem;
    }

    bool Run(
        const bool* inputs,
        size_t count,
        unsigned long now)
    {
        const bool oldAlarmOut = alarmOut;

        bool readerChanged = false;
        bool tempAlarm = false;
        bool tempMem = false;

        startupInhibit.Run(
            true,
            now
        );

        const bool inhibit =
            !startupInhibit.Q();

        if (!_disabled)
        {
            const size_t channelCount =
                channels.size();

            for (size_t i = 0;
                 i < channelCount;
                 ++i)
            {
                SensorChannel& ch =
                    channels[i];

                const bool raw =
                    (i < count)
                    ? inputs[i]
                    : false;

                const bool debounced =
                    ch.debounce.update(
                        raw,
                        now
                    );

                if (debounced != ch.lastDebounced)
                {
                    ch.lastDebounced =
                        debounced;

                    readerChanged = true;
                }

                if (inhibit)
                {
                    ch.timer.Stop();
                    ch.mem = false;
                    continue;
                }

                if (!debounced)
                {
                    ch.timer.Stop();
                    ch.mem = false;
                    continue;
                }

                ch.timer.Run(
                    true,
                    now
                );

                if (ch.timer.Q() &&
                    !ch.inhibit)
                {
                    tempAlarm = true;

                    if (_engage)
                        ch.mem = true;
                }

                if (ch.mem)
                    tempMem = true;
            }
        }
        else
        {
            for (auto& ch : channels)
                ch.mem = false;
        }

        alarmOut =
            tempAlarm ||
            tempMem;

        return
            readerChanged ||
            (oldAlarmOut != alarmOut);
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
        const std::vector<Sensor*>& sensors,
        bool active
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
        const std::vector<Sensor*>& sensors,
        bool active)
    {
        // ---------------------------------------------------------
        // GLOBAL CALLBACKS
        // ---------------------------------------------------------
        for (const auto& cb : globalCallbacks)
        {
            cb(
                zone,
                type,
                sensors,
                active
            );
        }

        // ---------------------------------------------------------
        // TYPE-SPECIFIC CALLBACKS
        // ---------------------------------------------------------
        const auto itType =
            typeCallbacks.find(type);

        if (itType != typeCallbacks.end())
        {
            for (const auto& cb : itType->second)
            {
                cb(
                    zone,
                    type,
                    sensors,
                    active
                );
            }
        }

        // ---------------------------------------------------------
        // ZONE-SPECIFIC CALLBACKS
        // ---------------------------------------------------------
        const auto itZone =
            zoneCallbacks.find(zone);

        if (itZone != zoneCallbacks.end())
        {
            for (const auto& cb : itZone->second)
            {
                cb(
                    zone,
                    type,
                    sensors,
                    active
                );
            }
        }

        // ---------------------------------------------------------
        // ZONE + TYPE CALLBACKS
        // ---------------------------------------------------------
        const auto itZT =
            zoneTypeCallbacks.find(zone);

        if (itZT != zoneTypeCallbacks.end())
        {
            const auto itZT2 =
                itZT->second.find(type);

            if (itZT2 != itZT->second.end())
            {
                for (const auto& cb : itZT2->second)
                {
                    cb(
                        zone,
                        type,
                        sensors,
                        active
                    );
                }
            }
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

    // ============================================================
    // LAST SENSOR STATE
    //
    // Stato precedente dello stato di allarme di ogni singolo
    // sensore per ogni tipo di canale.
    //
    // Questo evita il problema dello stato globale per tipo:
    // se A è attivo e B entra in allarme, B genera comunque
    // il proprio evento.
    // ============================================================

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
    bool ProcessAllTypes()
    {
        static const SensorChannelType types[] = {
            SensorChannelType::RT,
            SensorChannelType::H24,
            SensorChannelType::MASK,
            SensorChannelType::LEN
        };

        bool changed = false;

        for (SensorChannelType type : types)
        {
            bool current = false;

            const bool isSnoozed =
                snoozed[type];

            for (auto& [zoneName, zoneSensors] : zones)
            {
                for (auto* sensor : zoneSensors)
                {
                    if (!sensor)
                        continue;

                    SensorChannel* ch =
                        sensor->Get(type);

                    if (!ch)
                        continue;

                    const bool sensorCurrent =
                        sensor->ChannelAlarm(type);

                    if (sensorCurrent)
                        current = true;

                    const bool sensorPrevious = ch->GetLastAlarmState();
                        
                    if (sensorCurrent ==
                        sensorPrevious)
                    {
                        continue;
                    }

                    changed = true;

                    ch->SetLastAlarmState(sensorCurrent);

                    // ------------------------------------------------
                    // ATTIVAZIONE
                    // ------------------------------------------------

                    if (sensorCurrent)
                    {
                        if (isSnoozed)
                            continue;

                        std::vector<Sensor*> affected;
                        affected.push_back(sensor);

                        dispatcher.Dispatch(
                            zoneName,
                            type,
                            affected,
                            true
                        );

                        continue;
                    }

                    // ------------------------------------------------
                    // RIPRISTINO
                    // ------------------------------------------------

                    std::vector<Sensor*> affected;
                    affected.push_back(sensor);

                    dispatcher.Dispatch(
                        zoneName,
                        type,
                        affected,
                        false
                    );
                }
            }

            if (!current)
                snoozed[type] = false;

            lastState[type] =
                current;
        }

        return changed;
    }

    bool NewAlarmByType(
        SensorChannelType type)
    {
        const bool current =
            AnyAlarmByType(type);


        const bool previous =
            lastState[type];


        const bool isSnoozed =
            snoozed[type];


        if (!current)
            snoozed[type] = false;


        lastState[type] =
            current;


        const bool newAlarm =
            current &&
            !previous &&
            !isSnoozed;


        if (!newAlarm)
            return false;


        for (auto& [zoneName, zoneSensors] : zones)
        {
            for (auto* sensor : zoneSensors)
            {
                if (!sensor)
                    continue;


                if (!sensor->ChannelAlarm(type))
                    continue;


                std::vector<Sensor*> affected;

                affected.push_back(
                    sensor
                );


                dispatcher.Dispatch(
                    zoneName,
                    type,
                    affected,
                    true
                );
            }
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
        const char* name;
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

    bool Process(uint32_t now)
    {
        bool changed = false;

        const ConfigType* cfg =
            config;

        const size_t count =
            configCount;

        for (size_t i = 0; i < count; ++i)
        {
            Sensor* s =
                sensors[i];

            if (!s)
                continue;

            const auto& readers =
                cfg[i].readers;

            bool tmp[8];
            size_t n = 0;

            for (const auto& fn : readers)
            {
                if (n >= 8)
                    break;

                tmp[n++] = fn();
            }

            if (n == 0)
                continue;

            if (s->Run(tmp, n, now))
                changed = true;
        }

        return changed;
    }

    // --- Compute aggregated state from current sensors
    AggregateState ComputeAggregate() const
    {
        AggregateState st;

        const ConfigType* cfg = config;
        const size_t count = configCount;

        if (!cfg || count == 0)
            return st;

        for (size_t i = 0; i < count; ++i)
        {
            const ConfigType& c = cfg[i];
            Sensor* s = sensors[i];

            if (!s)
                continue;

            switch (c.category)
            {
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
                {
                    if (s->alarmOut)
                        st.windowsOpen = true;

                    break;
                }

                case SensorCategory::DOOR:
                {
                    if (s->alarmOut)
                        st.doorsOpen = true;

                    break;
                }

                case SensorCategory::FLOOD:
                {
                    if (s->alarmOut)
                        st.flood = true;

                    break;
                }

                case SensorCategory::SMOKE:
                {
                    if (s->alarmOut)
                        st.smoke = true;

                    break;
                }

                case SensorCategory::TAMPER:
                {
                    if (s->alarmOut)
                        st.tamper = true;

                    break;
                }

                default:
                    break;
            }
        }

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
    void ComputeCurrent(WiredSensorsManager& ws)
    {
        currentMask = 0;

        size_t bit = 0;

        for (size_t i = 0; i < ws.Count(); ++i)
        {
            Sensor* s =
                ws.GetSensor(i);

            if (!s)
                continue;

            for (auto& ch : s->channels)
            {
                if (ch.IsActive())
                {
                    currentMask |=
                        (uint64_t(1) << bit);
                }

                ++bit;
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
