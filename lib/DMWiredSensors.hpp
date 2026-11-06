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
#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "DMSignal.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

const unsigned long RT_DELAY      = 50;    // 50 ms
const unsigned long INITIAL_DELAY = 500;   // 500 ms

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
    PANIC,
    CAMERA,
    OTHER
};

// -------------------------------------------------------------
// SensorChannel
// -------------------------------------------------------------
class SensorChannel
{
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
    SensorChannel(int pin, float delayMs, SensorChannelType type)
        : pin(pin), type(type),
          timer(delayMs, TimerBase::Milliseconds),
          debounce(20)
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
// Sensor
// -------------------------------------------------------------
class Sensor
{
public:
    struct SensorOutputs
    {
        bool rt = false;
        bool h24 = false;
        bool rtMem = false;
        bool h24Mem = false;
    };

    std::vector<SensorChannel> channels;

    // Mantenuto per compatibilità esterna.
    // Internamente viene usato channelByType.
    std::unordered_map<SensorChannelType, SensorChannel*> lookup;

    std::array<SensorChannel*, 4> channelByType{};

    bool _engageRT = false;
    bool _engageH24 = false;
    bool _disabled = true;

    unsigned long startupInhibitMs = 2000;
    TON startupInhibit = TON(startupInhibitMs, TimerBase::Milliseconds);

    SensorOutputs outputs;

private:
    inline void clearRuntime(bool clearOutputMemory)
    {
        for (auto& ch : channels)
        {
            ch.timer.Run(false);
            ch.mem = false;
            ch.lastDebounced = false;
        }

        outputs.rt = false;
        outputs.h24 = false;

        if (clearOutputMemory)
        {
            outputs.rtMem = false;
            outputs.h24Mem = false;
        }
    }

public:
    Sensor(std::initializer_list<SensorChannel> list)
        : channels(list)
    {
        channelByType.fill(nullptr);

        for (auto& ch : channels)
        {
            lookup[ch.type] = &ch;

            const size_t index = static_cast<size_t>(ch.type);
            if (index < channelByType.size())
                channelByType[index] = &ch;
        }
    }

    inline const SensorOutputs& Outputs() const
    {
        return outputs;
    }

    void SetStartupInhibit(unsigned long ms)
    {
        startupInhibitMs = ms;
        startupInhibit = TON(ms, TimerBase::Milliseconds);
    }

    inline SensorChannel* Get(SensorChannelType type)
    {
        const size_t index = static_cast<size_t>(type);
        return index < channelByType.size() ? channelByType[index] : nullptr;
    }

    inline const SensorChannel* Get(SensorChannelType type) const
    {
        const size_t index = static_cast<size_t>(type);
        return index < channelByType.size() ? channelByType[index] : nullptr;
    }

    inline void EngageRT(bool mode)
    {
        _engageRT = mode;

        if(!mode)
            outputs.rtMem = false;
    }

    inline void EngageH24(bool mode)
    {
        _engageH24 = mode;

        if(!mode)
            outputs.h24Mem = false;
    }

    void Enable(bool mode)
    {
        if (IsEnabled() == mode)
            return;

        if (mode)
        {
            startupInhibit.Run(true);
            outputs.rt = false;
            outputs.h24 = false;
        }
        else
        {
            clearRuntime(true);
            _engageRT = false;
            _engageH24 = false;
        }

        _disabled = !mode;
    }

    inline bool IsEnabled() const
    {
        return !_disabled;
    }

    inline bool IsEngagedRT() const
    {
        return _engageRT;
    }

    inline bool IsEngagedH24() const
    {
        return _engageH24;
    }

    void Reset()
    {
        clearRuntime(true);
    }

    inline bool ChannelAlarm(SensorChannelType type) const
    {
        const SensorChannel* ch = Get(type);

        if (!ch || _disabled)
            return false;

        return (ch->timer.Q() &&
                !ch->inhibit &&
                startupInhibit.Q()) ||
            ch->mem;
    }

    inline bool ChannelOutput(SensorChannelType type) const
    {
        const SensorChannel* ch = Get(type);
        return ch &&
               ch->timer.Q() &&
               !ch->inhibit &&
               startupInhibit.Q();
    }

    bool Run(const bool* inputs, size_t count, unsigned long now)
    {
        if (_disabled)
            return false;

        const SensorOutputs oldOutputs = outputs;

        const SensorChannel* rt =
            Get(SensorChannelType::RT);

        const bool oldRtActive =
            rt ? rt->IsActive() : false;

        bool readerChanged = false;

        startupInhibit.Run(true, now);

        const bool inhibit =
            !startupInhibit.Q();

        const size_t channelCount =
            channels.size();

        for (size_t i = 0; i < channelCount; ++i)
        {
            SensorChannel& ch =
                channels[i];

            const bool raw =
                (inputs && i < count)
                    ? inputs[i]
                    : false;

            const bool debounced =
                ch.debounce.update(
                    raw,
                    now
                );

            if (debounced != ch.lastDebounced)
            {
                ch.lastDebounced = debounced;
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
                continue;
            }

            ch.timer.Run(true, now);
        }

        const SensorChannel* h24 =
            Get(SensorChannelType::H24);

        const SensorChannel* mask =
            Get(SensorChannelType::MASK);

        outputs.rt =
            rt &&
            rt->timer.Q() &&
            !rt->inhibit &&
            startupInhibit.Q();

        outputs.h24 =
            (h24 &&
            h24->timer.Q() &&
            !h24->inhibit &&
            startupInhibit.Q()) ||
            (mask &&
            mask->timer.Q() &&
            !mask->inhibit &&
            startupInhibit.Q());

        if (_engageRT && outputs.rt)
            outputs.rtMem = true;

        if (_engageH24 && outputs.h24)
            outputs.h24Mem = true;

        const bool newRtActive =
            rt ? rt->IsActive() : false;

        return readerChanged ||
            oldOutputs.rt != outputs.rt ||
            oldOutputs.h24 != outputs.h24 ||
            oldOutputs.rtMem != outputs.rtMem ||
            oldOutputs.h24Mem != outputs.h24Mem;
    }
};

// -------------------------------------------------------------
// AlarmDispatcher
// -------------------------------------------------------------
class AlarmDispatcher
{
public:
    using Callback = std::function<void(
        const std::string& zone,
        SensorChannelType type,
        const std::vector<Sensor*>& sensors,
        bool active
    )>;

    std::vector<Callback> globalCallbacks;

    std::unordered_map<
        SensorChannelType,
        std::vector<Callback>
    > typeCallbacks;

    std::unordered_map<
        std::string,
        std::vector<Callback>
    > zoneCallbacks;

    std::unordered_map<
        std::string,
        std::unordered_map<
            SensorChannelType,
            std::vector<Callback>
        >
    > zoneTypeCallbacks;

    void OnAnyAlarm(Callback cb)
    {
        globalCallbacks.push_back(std::move(cb));
    }

    void OnAlarmType(SensorChannelType type, Callback cb)
    {
        typeCallbacks[type].push_back(std::move(cb));
    }

    void OnZoneAlarm(const std::string& zone, Callback cb)
    {
        zoneCallbacks[zone].push_back(std::move(cb));
    }

    void OnZoneAlarmType(const std::string& zone,
                         SensorChannelType type,
                         Callback cb)
    {
        zoneTypeCallbacks[zone][type].push_back(std::move(cb));
    }

    void Dispatch(const std::string& zone,
                  SensorChannelType type,
                  const std::vector<Sensor*>& sensors,
                  bool active)
    {
        const auto dispatchList =
            [&](const auto& callbacks)
            {
                for (const auto& cb : callbacks)
                    cb(zone, type, sensors, active);
            };

        dispatchList(globalCallbacks);

        if (const auto it = typeCallbacks.find(type);
            it != typeCallbacks.end())
        {
            dispatchList(it->second);
        }

        if (const auto it = zoneCallbacks.find(zone);
            it != zoneCallbacks.end())
        {
            dispatchList(it->second);
        }

        if (const auto it = zoneTypeCallbacks.find(zone);
            it != zoneTypeCallbacks.end())
        {
            if (const auto jt = it->second.find(type);
                jt != it->second.end())
            {
                dispatchList(jt->second);
            }
        }
    }
};

// -------------------------------------------------------------
// ZoneManager
// -------------------------------------------------------------
class ZoneManager
{
public:
    using ZoneName = std::string;

    struct ZoneInfo
    {
        std::vector<Sensor*> sensors;
        int statusArea = -1;
        bool enabled = true;
        bool engaged = false;
    };

    std::vector<Sensor*> sensors;

    std::unordered_map<ZoneName, ZoneInfo> zones;

    std::unordered_map<Sensor*, ZoneName> sensorZones;

    std::unordered_map<SensorChannelType, bool> lastState;
    std::unordered_map<SensorChannelType, bool> snoozed;

    AlarmDispatcher dispatcher;

private:
    // ---------------------------------------------------------
    // Lookup diretto Sensor* -> ZoneInfo.
    // Evita la sequenza ripetuta:
    // sensorZones.find() + zones.find()
    // ---------------------------------------------------------
    const ZoneInfo* getSensorZoneInfo(const Sensor* sensor) const
    {
        if (!sensor)
            return nullptr;

        const auto it = sensorZones.find(
            const_cast<Sensor*>(sensor));

        if (it == sensorZones.end())
            return nullptr;

        const auto zoneIt = zones.find(it->second);
        return zoneIt != zones.end()
            ? &zoneIt->second
            : nullptr;
    }

    bool sensorEngaged(const Sensor* sensor,
                       SensorChannelType type) const
    {
        switch (type)
        {
            case SensorChannelType::RT:
                return sensor->IsEngagedRT();

            case SensorChannelType::H24:
            case SensorChannelType::MASK:
                return sensor->IsEngagedH24();

            default:
                return false;
        }
    }

public:
    void AddSensor(Sensor* sensor)
    {
        if (!sensor)
            return;

        for (auto* existing : sensors)
        {
            if (existing == sensor)
                return;
        }

        sensors.push_back(sensor);
    }

    void AddToZone(const ZoneName& zone, Sensor* sensor)
    {
        if (!sensor || zone.empty())
            return;

        auto& info = zones[zone];

        for (auto* existing : info.sensors)
        {
            if (existing == sensor)
            {
                sensorZones[sensor] = zone;
                return;
            }
        }

        info.sensors.push_back(sensor);
        sensorZones[sensor] = zone;
    }

    const ZoneName* GetSensorZone(const Sensor* sensor) const
    {
        if (!sensor)
            return nullptr;

        const auto it =
            sensorZones.find(const_cast<Sensor*>(sensor));

        return it != sensorZones.end()
            ? &it->second
            : nullptr;
    }

    inline bool IsSensorZoned(const Sensor* sensor) const
    {
        return GetSensorZone(sensor) != nullptr;
    }

    bool IsZoneEnabled(const ZoneName& zone) const
    {
        const auto it = zones.find(zone);
        return it != zones.end() && it->second.enabled;
    }

    bool IsZoneEngaged(const ZoneName& zone) const
    {
        const auto it = zones.find(zone);
        return it != zones.end() && it->second.engaged;
    }

    bool IsSensorSecurityEnabled(const Sensor* sensor) const
    {
        if (!sensor || !sensor->IsEnabled())
            return false;

        const ZoneInfo* zoneInfo = getSensorZoneInfo(sensor);
        return !zoneInfo || zoneInfo->enabled;
    }

    bool IsSensorSecurityEngaged(const Sensor* sensor,
                                 SensorChannelType type) const
    {
        if (!sensor || !sensorEngaged(sensor, type))
            return false;

        const ZoneInfo* zoneInfo = getSensorZoneInfo(sensor);
        return !zoneInfo || zoneInfo->engaged;
    }

    
    
    bool IsSensorSecurityAlarm(const Sensor* sensor,
                           SensorChannelType type) const
    {
        if (!sensor || !sensor->IsEnabled())
            return false;

        const ZoneInfo* zoneInfo =
            getSensorZoneInfo(sensor);

        if (zoneInfo && !zoneInfo->enabled)
            return false;

        const auto& out =
            sensor->Outputs();

        switch (type)
        {
            case SensorChannelType::RT:
                return out.rt;

            case SensorChannelType::H24:
                return out.h24;

            case SensorChannelType::MASK:
                return out.h24;

            case SensorChannelType::LEN:
                return sensor->ChannelAlarm(
                    SensorChannelType::LEN
                );

            default:
                return false;
        }
    }



    bool SetZoneStatusArea(const ZoneName& zone, int statusArea)
    {
        if (statusArea < 0)
            return false;

        const auto it = zones.find(zone);

        if (it == zones.end())
            return false;

        it->second.statusArea = statusArea;
        return true;
    }

    int GetZoneStatusArea(const ZoneName& zone) const
    {
        const auto it = zones.find(zone);
        return it != zones.end() ? it->second.statusArea : -1;
    }

    const std::vector<Sensor*>& GetZone(
        const ZoneName& zone) const
    {
        static const std::vector<Sensor*> empty;

        const auto it = zones.find(zone);
        return it != zones.end() ? it->second.sensors : empty;
    }

    bool EnableZone(const ZoneName& zone, bool mode)
    {
        const auto it = zones.find(zone);

        if (it == zones.end())
            return false;

        it->second.enabled = mode;
        return true;
    }

    void EnableAll(bool mode)
    {
        for (auto& entry : zones)
            entry.second.enabled = mode;
    }

    bool EngageZone(const ZoneName& zone, bool mode)
    {
        const auto it = zones.find(zone);

        if (it == zones.end())
            return false;

        it->second.engaged = mode;
        return true;
    }

    void EngageAll(bool mode)
    {
        for (auto& entry : zones)
            entry.second.engaged = mode;
    }

    
    bool ZoneAlarm(const ZoneName& zone) const
    {
        const auto it =
            zones.find(zone);

        if (it == zones.end() ||
            !it->second.enabled)
            return false;

        for (auto* sensor : it->second.sensors)
        {
            if (!sensor)
                continue;

            if (IsSensorSecurityAlarm(
                    sensor,
                    SensorChannelType::RT) ||
                IsSensorSecurityAlarm(
                    sensor,
                    SensorChannelType::H24))
            {
                return true;
            }
        }

        return false;
    }


    bool AnyAlarm() const
    {
        for (auto* sensor : sensors)
        {
            if (!sensor || !IsSensorSecurityEnabled(sensor))
                continue;

            const auto& out = sensor->Outputs();

            if (out.rt || out.h24)
                return true;
        }

        return false;
    }

    bool ZoneAlarmByType(const ZoneName& zone,
                     SensorChannelType type) const
    {
        const auto it =
            zones.find(zone);

        if (it == zones.end() ||
            !it->second.enabled)
            return false;

        for (auto* sensor : it->second.sensors)
        {
            if (sensor &&
                IsSensorSecurityAlarm(sensor, type))
            {
                return true;
            }
        }

        return false;
    }

    bool AnyAlarmByType(SensorChannelType type) const
    {
        for (auto* sensor : sensors)
        {
            if (sensor &&
                IsSensorSecurityAlarm(sensor, type))
            {
                return true;
            }
        }

        return false;
    }

    std::vector<Sensor*> SensorsInAlarm(
        SensorChannelType type) const
    {
        std::vector<Sensor*> result;

        result.reserve(sensors.size());

        for (auto* sensor : sensors)
        {
            if (sensor &&
                IsSensorSecurityAlarm(sensor, type))
            {
                result.push_back(sensor);
            }
        }

        return result;
    }

    // =========================================================
    // PROCESS ALL TYPES
    // =========================================================
    bool ProcessAllTypes()
    {
        static constexpr SensorChannelType types[] =
        {
            SensorChannelType::RT,
            SensorChannelType::H24,
            SensorChannelType::MASK,
            SensorChannelType::LEN
        };

        static const ZoneName emptyZone;

        bool changed = false;

        for (SensorChannelType type : types)
        {
            bool current = false;
            const bool isSnoozed = snoozed[type];

            for (auto* sensor : sensors)
            {
                if (!sensor ||
                    !sensor->IsEnabled())
                {
                    continue;
                }

                SensorChannel* ch =
                    sensor->Get(type);

                if (!ch)
                    continue;

                const bool sensorCurrent =
                    IsSensorSecurityAlarm(
                        sensor,
                        type
                    );

                current |= sensorCurrent;

                const bool sensorPrevious =
                    ch->GetLastAlarmState();

                if (sensorCurrent == sensorPrevious)
                    continue;

                changed = true;
                ch->SetLastAlarmState(sensorCurrent);

                const ZoneName* zone =
                    GetSensorZone(sensor);

                const ZoneName& dispatchZone =
                    zone ? *zone : emptyZone;

                if (sensorCurrent && isSnoozed)
                    continue;

                std::vector<Sensor*> affected;
                affected.reserve(1);
                affected.push_back(sensor);

                dispatcher.Dispatch(
                    dispatchZone,
                    type,
                    affected,
                    sensorCurrent
                );
            }

            if (!current)
                snoozed[type] = false;

            lastState[type] = current;
        }

        return changed;
    }

    // =========================================================
    // NEW ALARM BY TYPE
    // =========================================================
    bool NewAlarmByType(SensorChannelType type)
    {
        static const ZoneName emptyZone;

        const bool current =
            AnyAlarmByType(type);

        const bool previous =
            lastState[type];

        const bool isSnoozed =
            snoozed[type];

        if (!current)
            snoozed[type] = false;

        lastState[type] = current;

        if (!current ||
            previous ||
            isSnoozed)
        {
            return false;
        }

        for (auto* sensor : sensors)
        {
            if (!sensor ||
                !sensor->IsEnabled() ||
                !IsSensorSecurityAlarm(sensor, type))
            {
                continue;
            }

            const ZoneName* zone =
                GetSensorZone(sensor);

            const ZoneName& dispatchZone =
                zone ? *zone : emptyZone;

            std::vector<Sensor*> affected;
            affected.reserve(1);
            affected.push_back(sensor);

            dispatcher.Dispatch(
                dispatchZone,
                type,
                affected,
                true
            );
        }

        return true;
    }

    void Snooze(SensorChannelType type)
    {
        snoozed[type] = true;
    }

    void ResetZone(const ZoneName& zone)
    {
        for (auto* sensor : GetZone(zone))
        {
            if (sensor)
                sensor->Reset();
        }
    }

    void ResetAll()
    {
        for (auto* sensor : sensors)
        {
            if (sensor)
                sensor->Reset();
        }
    }
};

// -------------------------------------------------------------
// WiredSensorsManager
// -------------------------------------------------------------
class WiredSensorsManager
{
public:
    struct Reader
    {
        int area = -1;
        std::function<bool()> read;

        Reader() = default;

        Reader(std::function<bool()> fn)
            : area(-1), read(std::move(fn))
        {}

        Reader(int area, std::function<bool()> fn)
            : area(area), read(std::move(fn))
        {}

        inline bool operator()() const
        {
            return read ? read() : false;
        }

        explicit inline operator bool() const
        {
            return static_cast<bool>(read);
        }
    };

    template<typename F>
    static Reader MakeReader(int area, F&& fn)
    {
        return Reader(
            area,
            [area, fn = std::forward<F>(fn)]() mutable -> bool
            {
                return static_cast<bool>(fn(area));
            });
    }

    struct WiredSensorConfig
    {
        const char* name;
        const char* zone;
        std::initializer_list<SensorChannel> channels;
        std::initializer_list<Reader> readers;
        SensorCategory category;
        int cmdArea = -1;
        int statusArea = -1;
    };

    using ConfigType = WiredSensorConfig;

private:
    ZoneManager zones;

    std::vector<Sensor*> sensors;

    const ConfigType* config = nullptr;
    size_t configCount = 0;

    std::unordered_map<
        std::string,
        std::vector<Sensor*>
    > zoneMap;

    std::vector<std::string> zoneNames;

    // O(1): sensorIndex -> ZoneInfo
    std::vector<const ZoneManager::ZoneInfo*> sensorZoneInfo;

    // O(1): sensorIndex -> zone index
    std::vector<int> sensorZoneIndex;

    // O(1): zone name -> stable zone index
    std::unordered_map<std::string, int> zoneIndexMap;

    void clearInternal()
    {
        for (Sensor* sensor : sensors)
            delete sensor;

        sensors.clear();
        zoneMap.clear();
        zoneNames.clear();
        zoneIndexMap.clear();
        sensorZoneInfo.clear();
        sensorZoneIndex.clear();

        config = nullptr;
        configCount = 0;

        zones = ZoneManager();
    }

    void buildSensorZoneCache()
    {
        sensorZoneInfo.assign(configCount, nullptr);
        sensorZoneIndex.assign(configCount, -1);

        if (!config)
            return;

        for (size_t i = 0; i < configCount; ++i)
        {
            const char* zone = config[i].zone;

            if (!zone || !zone[0])
                continue;

            if (const auto it = zones.zones.find(zone);
                it != zones.zones.end())
            {
                sensorZoneInfo[i] = &it->second;
            }

            if (const auto it = zoneIndexMap.find(zone);
                it != zoneIndexMap.end())
            {
                sensorZoneIndex[i] = it->second;
            }
        }
    }

    void initInternal(const ConfigType* cfg,
                      size_t count,
                      uint32_t startupInhibitMs)
    {
        clearInternal();

        config = cfg;
        configCount = count;

        if (!cfg || count == 0)
        {
            LOG_DF("WiredSensors",
                   "Init: nessun sensore configurato");
            return;
        }

        sensors.reserve(count);
        zoneNames.reserve(count);
        zoneMap.reserve(count);
        zoneIndexMap.reserve(count);
        zones.sensors.reserve(count);

        for (size_t i = 0; i < count; ++i)
        {
            const auto& c = cfg[i];

            Sensor* s = new Sensor(c.channels);

            if (startupInhibitMs != 0)
                s->SetStartupInhibit(startupInhibitMs);

            sensors.push_back(s);
            zones.AddSensor(s);

            const char* zone = c.zone;

            if (!zone || !zone[0])
                continue;

            const std::string zoneName(zone);

            zones.AddToZone(zoneName, s);
            zoneMap[zoneName].push_back(s);

            // Inserimento della zona una sola volta.
            if (zoneIndexMap.find(zoneName) == zoneIndexMap.end())
            {
                const int index =
                    static_cast<int>(zoneNames.size());

                zoneNames.push_back(zoneName);
                zoneIndexMap.emplace(zoneName, index);
            }
        }

        buildSensorZoneCache();

        if (startupInhibitMs != 0)
        {
            LOG_DF("WiredSensors",
                   "Init: configurati %u sensori (startup inhibit=%u ms)",
                   (unsigned)count,
                   (unsigned)startupInhibitMs);
        }
        else
        {
            LOG_DF("WiredSensors",
                   "Init: configurati %u sensori",
                   (unsigned)count);
        }

        size_t zonedSensors = 0;

        for (size_t i = 0; i < count; ++i)
        {
            const auto& c = cfg[i];

            if (c.zone && c.zone[0] != '\0')
                ++zonedSensors;
        }

        LOG_DF("WiredSensors",
               "Init: zoned=%u | unzoned=%u | zones=%u",
               (unsigned)zonedSensors,
               (unsigned)(count - zonedSensors),
               (unsigned)zoneNames.size());
    }

public:
    WiredSensorsManager() = default;

    ~WiredSensorsManager()
    {
        clearInternal();
    }

    WiredSensorsManager(const WiredSensorsManager&) = delete;
    WiredSensorsManager& operator=(const WiredSensorsManager&) = delete;

    void Init(const ConfigType* cfg, size_t count)
    {
        initInternal(cfg, count, 0);
    }

    void Init(const ConfigType* cfg,
              size_t count,
              uint32_t startupInhibitMs)
    {
        initInternal(cfg, count, startupInhibitMs);
    }

    struct AggregateSignals
    {
        bool intrusion = false;
        bool intrusionH24 = false;
        bool flood = false;
        bool smoke = false;
        bool windowsOpen = false;
        bool doorsOpen = false;
        bool camera = false;
    };

    struct AggregateState
    {
        AggregateSignals normal;
        AggregateSignals mem;
        bool mask = false;
        bool tamper = false;
        bool panic = false;
        bool camera = false;
    };

    struct ProcessResult
    {
        bool changed = false;
        bool zoneChanged = false;
    };

    ProcessResult Process(uint32_t now)
    {
        ProcessResult result;

        const ConfigType* cfg = config;
        const size_t count = configCount;

        if (!cfg || count == 0)
            return result;

        for (size_t i = 0; i < count; ++i)
        {
            Sensor* s = sensors[i];

            if (!s)
                continue;

            const auto& readers = cfg[i].readers;
            bool tmp[8];
            size_t n = 0;

            for (const auto& reader : readers)
            {
                if (n >= 8)
                    break;

                tmp[n++] = reader();
            }

            if (n == 0)
                continue;

            if (s->Run(tmp, n, now))
            {
                result.changed = true;

                if (cfg[i].zone && cfg[i].zone[0] != '\0')
                    result.zoneChanged = true;
            }
        }

        return result;
    }

    
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
            const Sensor* s = sensors[i];

            if (!s || !s->IsEnabled())
                continue;

            const ZoneManager::ZoneInfo* zoneInfo =
                i < sensorZoneInfo.size()
                    ? sensorZoneInfo[i]
                    : nullptr;

            if (c.zone &&
                c.zone[0] != '\0' &&
                !zoneInfo)
            {
                continue;
            }

            if (zoneInfo && !zoneInfo->enabled)
                continue;

            const auto& out = s->Outputs();

            switch (c.category)
            {
                case SensorCategory::PIR:
                {
                    st.normal.intrusion |= out.rt;

                    const SensorChannel* h24 =
                        s->Get(SensorChannelType::H24);

                    if (h24 &&
                        h24->IsActive() &&
                        !h24->IsInhibit())
                    {
                        st.normal.intrusionH24 = true;
                    }

                    st.mem.intrusion |= out.rtMem;
                    st.mem.intrusionH24 |= out.h24Mem;

                    st.mask |=
                        s->ChannelAlarm(
                            SensorChannelType::MASK);

                    break;
                }

                case SensorCategory::PANIC:
                {
                    st.panic |=
                        out.rt || out.h24;

                    break;
                }

                case SensorCategory::CAMERA:
                {
                    st.normal.camera |=
                        out.rt || out.h24;

                    st.mem.camera |=
                        out.rtMem || out.h24Mem;

                    break;
                }


                case SensorCategory::WINDOW:
                    st.normal.windowsOpen |=
                        out.rt || out.h24;
                    st.mem.windowsOpen |=
                        out.rtMem || out.h24Mem;
                    break;

                case SensorCategory::DOOR:
                    st.normal.doorsOpen |=
                        out.rt || out.h24;
                    st.mem.doorsOpen |=
                        out.rtMem || out.h24Mem;
                    break;

                case SensorCategory::FLOOD:
                    st.normal.flood |=
                        out.rt || out.h24;
                    st.mem.flood |=
                        out.rtMem || out.h24Mem;
                    break;

                case SensorCategory::SMOKE:
                    st.normal.smoke |=
                        out.rt || out.h24;
                    st.mem.smoke |=
                        out.rtMem || out.h24Mem;
                    break;

                case SensorCategory::TAMPER:
                    st.tamper |=
                        out.rt || out.h24;
                    break;

                default:
                    break;
            }
        }

        return st;
    } 

    const std::unordered_map<
        std::string,
        std::vector<Sensor*>
    >& GetZoneMap() const
    {
        return zoneMap;
    }

    const std::vector<Sensor*>& GetByZone(
        const char* zone) const
    {
        static const std::vector<Sensor*> empty;

        if (!zone)
            return empty;

        const auto it = zoneMap.find(zone);
        return it != zoneMap.end() ? it->second : empty;
    }

    Sensor* GetFirst(const char* zone)
    {
        if (!zone)
            return nullptr;

        const auto it = zoneMap.find(zone);

        if (it == zoneMap.end() ||
            it->second.empty())
        {
            return nullptr;
        }

        return it->second.front();
    }

    const char* GetZoneName(size_t index) const
    {
        return index < zoneNames.size()
            ? zoneNames[index].c_str()
            : nullptr;
    }

    int GetZoneIndex(const char* zone) const
    {
        if(!zone || !zone[0])
            return -1;

        const auto it =
            zoneIndexMap.find(zone);

        return it != zoneIndexMap.end()
            ? it->second
            : -1;
    }

    int GetSensorZoneIndex(size_t sensorIndex) const
    {
        return sensorIndex < sensorZoneIndex.size()
            ? sensorZoneIndex[sensorIndex]
            : -1;
    }

    size_t GetZoneCount() const
    {
        return zoneNames.size();
    }

    ZoneManager& Zones()
    {
        return zones;
    }

    const ZoneManager& Zones() const
    {
        return zones;
    }

    Sensor* GetSensor(size_t i)
    {
        return i < sensors.size() ? sensors[i] : nullptr;
    }

    const Sensor* GetSensor(size_t i) const
    {
        return i < sensors.size() ? sensors[i] : nullptr;
    }

    size_t Count() const
    {
        return configCount;
    }

    const ConfigType* GetConfig() const
    {
        return config;
    }

    size_t GetReaderCount(size_t sensorIndex) const
    {
        if (!config || sensorIndex >= configCount)
            return 0;

        return config[sensorIndex].readers.size();
    }

    const Reader* GetReader(size_t sensorIndex,
                            size_t readerIndex) const
    {
        if (!config || sensorIndex >= configCount)
            return nullptr;

        const auto& readers = config[sensorIndex].readers;

        if (readerIndex >= readers.size())
            return nullptr;

        return readers.begin() + readerIndex;
    }

    int GetReaderArea(size_t sensorIndex,
                      size_t readerIndex) const
    {
        const Reader* reader =
            GetReader(sensorIndex, readerIndex);

        return reader ? reader->area : -1;
    }
};

 /*
    ============================================================

        ALARM BITMASK MANAGER

    ============================================================

    Scopo
    ------------------------------------------------------------

    Gestisce la rappresentazione compatta degli allarmi
    mediante bitmap a 64 bit.

    Ogni canale di ogni sensore viene associato ad un bit.

    Responsabilità:

      - costruzione mappa sensori/canali
      - rilevazione fronti salita
      - rilevazione fronti discesa
      - generazione memoria allarmi
      - gestione tacitazione allarmi
      - callback nuovi eventi
      - diagnostica

    ------------------------------------------------------------

    Maschere

      currentMask
        Stato realtime corrente.

      previousMask
        Stato realtime ciclo precedente.

      newMask
        Eventi appena comparsi.

      memMask
        Memoria storica degli allarmi.

      silencedMask
        Eventi riconosciuti mediante TACITA.

    ------------------------------------------------------------

    Ciclo operativo

      ComputeRealtimeMask()
              ↓
      ComputeTransitions()
              ↓
      UpdateSilencedState()
              ↓
      UpdateMemoryMask()
              ↓
      DispatchNewAlarms()

    ============================================================
    */

class AlarmBitmaskManager
{
public:

    using AlarmCallback = void (*)(uint64_t newMask,
                                   uint64_t currentMask,
                                   uint64_t memMask,
                                   size_t bitIndex,
                                   size_t sensorIndex,
                                   SensorChannelType type);

    struct SignalInfo
    {
        size_t sensorIndex;
        SensorChannelType type;
    };

public:

    // ========================================================
    // INITIALIZATION
    // ========================================================

    bool BuildMap(WiredSensorsManager& ws)
    {
        map.clear();
        map.reserve(64);

        currentMask  = 0;
        previousMask = 0;
        memMask      = 0;
        newMask      = 0;
        silencedMask = 0;

        size_t bit = 0;

        const size_t sensorCount =
            ws.Count();

        for(size_t sensorIndex = 0;
            sensorIndex < sensorCount;
            ++sensorIndex)
        {
            const Sensor* sensor =
                ws.GetSensor(sensorIndex);

            if(!sensor)
                continue;

            for(const auto& ch : sensor->channels)
            {
                if(bit >= 64)
                {
                    LOG_EF(
                        "AlarmBitmaskManager",
                        "Too many alarm channels: maximum is 64"
                    );

                    map.clear();

                    return false;
                }

                map.push_back({
                    sensorIndex,
                    ch.type
                });

                ++bit;
            }
        }

        return true;
    }

    // ========================================================
    // EXECUTION
    // ========================================================

    bool ComputeCurrent(WiredSensorsManager& ws)
    {
        const uint64_t oldCurrent =
            currentMask;

        ComputeRealtimeMask(ws);

        newMask =
            ComputeNewMask();

        const uint64_t clearedMask =
            ComputeClearedMask();

        UpdateSilencedState(
            clearedMask
        );

        const uint64_t effectiveNewMask =
            ComputeEffectiveNewMask();

        UpdateMemoryMask(
            effectiveNewMask
        );

        DispatchNewAlarms(
            effectiveNewMask
        );

        previousMask =
            currentMask;

        return oldCurrent != currentMask;
    }

    // ========================================================
    // CONFIGURATION
    // ========================================================

    inline void SetCallback(
        AlarmCallback cb)
    {
        onNewAlarm = cb;
    }

    inline void SetEngage(bool mode)
    {
        engage = mode;
    }

    inline bool IsEngaged() const
    {
        return engage;
    }

    // ========================================================
    // MEMORY MANAGEMENT
    // ========================================================

    inline void ResetMemory()
    {
        memMask = 0;
        silencedMask = 0;
    }

    inline void ResetMemory(uint64_t mask)
    {
        memMask &= ~mask;
    }

    inline void ClearNewMask(
        uint64_t mask)
    {
        newMask &= ~mask;
    }

    // ========================================================
    // SILENCE MANAGEMENT
    // ========================================================

    inline void SilenceCurrent()
    {
        silencedMask |=
            currentMask |
            memMask;
    }

    inline void ClearSilenced(
        uint64_t mask)
    {
        silencedMask &= ~mask;
    }

    inline bool HasUnsilencedMemory() const
    {
        return (memMask & ~silencedMask) != 0;
    }

    bool IsSilenced(
        size_t sensorIndex,
        SensorChannelType type) const
    {
        for(size_t bit = 0;
            bit < map.size();
            ++bit)
        {
            const auto& info =
                map[bit];

            if(info.sensorIndex != sensorIndex)
                continue;

            if(info.type != type)
                continue;

            return
                (silencedMask &
                (uint64_t(1) << bit)) != 0;
        }

        return false;
    }

    // ========================================================
    // SENSOR MANAGEMENT
    // ========================================================

    void ClearSensor(
        size_t sensorIndex)
    {
        for(size_t bit = 0;
            bit < map.size();
            ++bit)
        {
            if(map[bit].sensorIndex != sensorIndex)
                continue;

            const uint64_t mask =
                uint64_t(1) << bit;

            currentMask  &= ~mask;
            previousMask &= ~mask;
            newMask      &= ~mask;
            memMask      &= ~mask;
            silencedMask &= ~mask;
        }
    }

    // ========================================================
    // GETTERS
    // ========================================================

    inline uint64_t GetCurrentMask() const
    {
        return currentMask;
    }

    inline uint64_t GetMemoryMask() const
    {
        return memMask;
    }

    inline uint64_t GetNewMask() const
    {
        return newMask;
    }

    inline uint64_t GetSilencedMask() const
    {
        return silencedMask;
    }

    inline uint64_t GetEffectiveMemoryMask() const
    {
        return memMask & ~silencedMask;
    }

    // ========================================================
    // DIAGNOSTIC
    // ========================================================

    void ReportMap(
        const WiredSensorsManager& ws) const
    {
        LOG_IF(
            "AlarmBitmaskManager",
            "===== ALARM BITMAP ====="
        );

        const auto* cfg =
            ws.GetConfig();

        const size_t sensorCount =
            ws.Count();

        for(size_t bit = 0;
            bit < map.size();
            ++bit)
        {
            const auto& info =
                map[bit];

            const char* name = "?";
            const char* zone = "?";

            if(cfg &&
            info.sensorIndex < sensorCount)
            {
                name =
                    cfg[info.sensorIndex].name
                        ? cfg[info.sensorIndex].name
                        : "?";

                zone =
                    cfg[info.sensorIndex].zone
                        ? cfg[info.sensorIndex].zone
                        : "?";
            }

            const Sensor* sensor =
                ws.GetSensor(
                    info.sensorIndex
                );

            LOG_IF(
                "AlarmBitmaskManager",
                "bit=%u mask=0x%016llX sensor=%u name=%s zone=%s type=%u enabled=%d",
                (unsigned)bit,
                (unsigned long long)(
                    uint64_t(1) << bit
                ),
                (unsigned)info.sensorIndex,
                name,
                zone,
                (unsigned)info.type,
                (sensor &&
                sensor->IsEnabled()) ? 1 : 0
            );
        }

        LOG_IF(
            "AlarmBitmaskManager",
            "Masks current=0x%016llX previous=0x%016llX new=0x%016llX mem=0x%016llX silenced=0x%016llX",
            (unsigned long long)currentMask,
            (unsigned long long)previousMask,
            (unsigned long long)newMask,
            (unsigned long long)memMask,
            (unsigned long long)silencedMask
        );

        LOG_IF(
            "AlarmBitmaskManager",
            "Total bits: %u",
            (unsigned)map.size()
        );

        LOG_IF(
            "AlarmBitmaskManager",
            "========================"
        );
    }

private:

    // ========================================================
    // COMPUTE HELPERS
    // ========================================================

    void ComputeRealtimeMask(
        WiredSensorsManager& ws)
    {
        currentMask = 0;

        auto& zoneManager =
            ws.Zones();

        for(size_t bit = 0;
            bit < map.size();
            ++bit)
        {
            const auto& info =
                map[bit];

            const Sensor* sensor =
                ws.GetSensor(
                    info.sensorIndex
                );

            if(!sensor)
                continue;

            if(zoneManager.IsSensorSecurityAlarm(
                    sensor,
                    info.type))
            {
                currentMask |=
                    uint64_t(1) << bit;
            }
        }
    }

    uint64_t ComputeNewMask() const
    {
        return currentMask &
               ~previousMask;
    }

    uint64_t ComputeClearedMask() const
    {
        return previousMask &
               ~currentMask;
    }

    void UpdateSilencedState(
        uint64_t clearedMask)
    {
        if(clearedMask)
        {
            silencedMask &=
                ~clearedMask;
        }
    }

    uint64_t ComputeEffectiveNewMask() const
    {
        return newMask &
               ~silencedMask;
    }

    void UpdateMemoryMask(
        uint64_t effectiveNewMask)
    {
        if(engage)
        {
            memMask |=
                effectiveNewMask;
        }
    }

    void DispatchNewAlarms(
        uint64_t effectiveNewMask)
    {
        if(effectiveNewMask == 0 ||
           !onNewAlarm)
        {
            return;
        }

        uint64_t pending =
            effectiveNewMask;

        while(pending)
        {
            const unsigned bitIndex =
                static_cast<unsigned>(
                    __builtin_ctzll(
                        pending
                    )
                );

            pending &=
                pending - 1;

            if(bitIndex >= map.size())
                continue;

            const auto& info =
                map[bitIndex];

            onNewAlarm(
                effectiveNewMask,
                currentMask,
                memMask,
                bitIndex,
                info.sensorIndex,
                info.type
            );
        }
    }

private:

    // ========================================================
    // RUNTIME STATE
    // ========================================================

    uint64_t currentMask  = 0;
    uint64_t previousMask = 0;
    uint64_t memMask      = 0;
    uint64_t newMask      = 0;
    uint64_t silencedMask = 0;

    bool engage = false;

    AlarmCallback onNewAlarm = nullptr;

    std::vector<SignalInfo> map;
};

#endif