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
class Sensor
{
public:

    // ============================================================
    // SENSOR OUTPUTS
    // ============================================================
    //
    // Uscite LOGICHE già post-processate dal sensore.
    //
    // rt
    //     uscita del canale RT
    //
    // h24
    //     uscita H24 complessiva:
    //     H24 OR MASK
    //
    // ============================================================

    struct SensorOutputs
    {
        bool rt  = false;
        bool h24 = false;

        // Uscite memorizzate
        bool rtMem = false;
        bool h24Mem = false;
    };


    std::vector<SensorChannel> channels;

    std::unordered_map<
        SensorChannelType,
        SensorChannel*
    > lookup;

    std::array<
        SensorChannel*,
        4
    > channelByType{};

    bool _engageRT = false;
    bool _engageH24 = false;

    bool _disabled = true;


    unsigned long startupInhibitMs = 2000;

    TON startupInhibit =
        TON(
            startupInhibitMs,
            TimerBase::Milliseconds
        );


    SensorOutputs outputs;


    // ============================================================
    // CONSTRUCTOR
    // ============================================================

    Sensor(
        std::initializer_list<SensorChannel> list)
        : channels(list)
    {
        channelByType.fill(nullptr);

        for (auto& ch : channels)
        {
            lookup[ch.type] = &ch;

            const size_t index =
                static_cast<size_t>(ch.type);

            if (index < channelByType.size())
            {
                channelByType[index] = &ch;
            }
        }
    }


    // ============================================================
    // OUTPUT ACCESS
    // ============================================================

    const SensorOutputs& Outputs() const
    {
        return outputs;
    }


    // ============================================================
    // STARTUP INHIBIT
    // ============================================================

    void SetStartupInhibit(
        unsigned long ms)
    {
        startupInhibitMs = ms;

        startupInhibit =
            TON(
                ms,
                TimerBase::Milliseconds
            );
    }


    // ============================================================
    // CHANNEL ACCESS
    // ============================================================

    SensorChannel* Get(
        SensorChannelType type)
    {
        const size_t index =
            static_cast<size_t>(type);

        if (index >= channelByType.size())
            return nullptr;

        return channelByType[index];
    }


    const SensorChannel* Get(
        SensorChannelType type) const
    {
        const size_t index =
            static_cast<size_t>(type);

        if (index >= channelByType.size())
            return nullptr;

        return channelByType[index];
    }


    // ============================================================
    // ENGAGE
    // ============================================================

    void EngageRT(bool mode)
    {
        _engageRT = mode;
    }

    void EngageH24(bool mode)
    {
        _engageH24 = mode;
    }

    // ============================================================
    // ENABLE
    // ============================================================

    void Enable(
        bool mode)
    {
        if (mode)
        {
            startupInhibit.Run(true);

            // Uscite correnti azzerate.
            outputs.rt  = false;
            outputs.h24 = false;
        }
        else
        {
            for (auto& ch : channels)
            {
                ch.timer.Run(false);
                ch.mem = false;
                ch.lastDebounced = false;
            }

            // Sensore disabilitato:
            // nessuna uscita e nessuna memoria.
            outputs.rt    = false;
            outputs.h24   = false;
            outputs.rtMem = false;
            outputs.h24Mem = false;
        }

        _disabled = !mode;
    }


    // ============================================================
    // STATE
    // ============================================================

    bool IsEnabled() const
    {
        return !_disabled;
    }


    bool IsEngagedRT() const
    {
        return _engageRT;
    }

    bool IsEngagedH24() const
    {
        return _engageH24;
    }


    // ============================================================
    // RESET
    // ============================================================

    void Reset()
    {
        for (auto& ch : channels)
        {
            ch.timer.Run(false);
            ch.mem = false;
            ch.lastDebounced = false;
        }

        outputs.rt     = false;
        outputs.h24    = false;
        outputs.rtMem  = false;
        outputs.h24Mem = false;
    }


    // ============================================================
    // CHANNEL ALARM
    // ============================================================
    //
    // Questo è lo stato già elaborato del singolo canale.
    //
    // ============================================================

    bool ChannelAlarm(
        SensorChannelType type) const
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
            (
                ch->timer.Q() &&
                !ch->inhibit &&
                startupInhibit.Q()
            )
            ||
            ch->mem;
    }


    // ============================================================
    // RUN
    // ============================================================
    bool ChannelOutput(
        SensorChannelType type) const
    {
        const SensorChannel* ch =
            Get(type);

        if (!ch)
            return false;

        return
            ch->timer.Q() &&
            !ch->inhibit &&
            startupInhibit.Q();
    }

    bool Run(
        const bool* inputs,
        size_t count,
        unsigned long now)
    {
        const SensorOutputs oldOutputs =
            outputs;

        bool readerChanged = false;


        // ========================================================
        // STARTUP INHIBIT
        // ========================================================

        startupInhibit.Run(
            true,
            now
        );


        const bool inhibit =
            !startupInhibit.Q();


        // ========================================================
        // SENSOR DISABLED
        // ========================================================

        if (_disabled)
        {
            for (auto& ch : channels)
            {
                ch.timer.Run(false);
                ch.mem = false;
                ch.lastDebounced = false;
            }

            outputs.rt     = false;
            outputs.h24    = false;
            outputs.rtMem  = false;
            outputs.h24Mem = false;

            return
                readerChanged ||
                oldOutputs.rt     != outputs.rt ||
                oldOutputs.h24    != outputs.h24 ||
                oldOutputs.rtMem  != outputs.rtMem ||
                oldOutputs.h24Mem != outputs.h24Mem;
        }


        // ========================================================
        // CHANNEL LOOP
        // ========================================================

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


            // ----------------------------------------------------
            // READER STATE CHANGE
            // ----------------------------------------------------

            if (debounced !=
                ch.lastDebounced)
            {
                ch.lastDebounced =
                    debounced;

                readerChanged = true;
            }


            // ----------------------------------------------------
            // STARTUP INHIBIT
            // ----------------------------------------------------

            if (inhibit)
            {
                ch.timer.Stop();
                ch.mem = false;

                continue;
            }


            // ----------------------------------------------------
            // INPUT NOT ACTIVE
            // ----------------------------------------------------

            if (!debounced)
            {
                ch.timer.Stop();

                // NON cancelliamo le output memories del sensore.
                //
                // La memoria rtMem/h24Mem viene cancellata
                // solo da Reset() o Disable().
                continue;
            }


            // ----------------------------------------------------
            // TIMER
            // ----------------------------------------------------

            ch.timer.Run(
                true,
                now
            );
        }


        // ========================================================
        // POST PROCESS
        // ========================================================
        //
        // QUI vengono generate le uscite correnti del sensore.
        //
        // RT:
        //     uscita corrente RT
        //
        // H24:
        //     uscita corrente H24 OR MASK
        //
        // ========================================================

        outputs.rt =
            ChannelOutput(
                SensorChannelType::RT
            );


        outputs.h24 =
            ChannelOutput(
                SensorChannelType::H24
            )
            ||
            ChannelOutput(
                SensorChannelType::MASK
            );


        // ========================================================
        // OUTPUT MEMORY
        // ========================================================
        //
        // La memoria viene impostata SOLO sul fronte di salita
        // dell'uscita e SOLO quando il sensore è engaged.
        //
        // RT
        //     RT 0 -> 1 + engaged -> rtMem = 1
        //
        // H24
        //     H24 0 -> 1 + engaged -> h24Mem = 1
        //
        // H24 comprende già MASK.
        //
        // ========================================================

        if (_engageRT &&
            outputs.rt)
        {
            outputs.rtMem = true;
        }

        if (_engageH24 &&
            outputs.h24)
        {
            outputs.h24Mem = true;
        }


        // ========================================================
        // RESULT
        // ========================================================

        return
            readerChanged ||
            oldOutputs.rt     != outputs.rt ||
            oldOutputs.h24    != outputs.h24 ||
            oldOutputs.rtMem  != outputs.rtMem ||
            oldOutputs.h24Mem != outputs.h24Mem;
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
class ZoneManager
{
public:

    using ZoneName = std::string;


    // ============================================================
    // ZONE INFO
    // ============================================================
    //
    // Una zona contiene:
    //   - i sensori associati
    //   - area di status HMI
    //   - stato ENABLED
    //   - stato ENGAGED
    //
    // enabled  = la zona partecipa alla sicurezza
    // engaged  = la zona consente l'ingaggio degli allarmi
    //
    // ============================================================

    struct ZoneInfo
    {
        std::vector<Sensor*> sensors;

        int statusArea = -1;

        bool enabled = true;
        bool engaged = false;
    };


    // ============================================================
    // REGISTRY
    // ============================================================

    std::vector<Sensor*> sensors;

    std::unordered_map<
        ZoneName,
        ZoneInfo
    > zones;


    // ============================================================
    // SENSOR -> ZONE
    // ============================================================
    //
    // Un sensore può appartenere:
    //   - ad una zona
    //   - a nessuna zona
    //
    // La presenza nella mappa identifica l'appartenenza ad una
    // zona. Se il sensore non è presente -> sensore unzoned.
    //
    // ============================================================

    std::unordered_map<
        Sensor*,
        ZoneName
    > sensorZones;


    // ============================================================
    // LAST ALARM STATE PER TYPE
    // ============================================================

    std::unordered_map<
        SensorChannelType,
        bool
    > lastState;


    // ============================================================
    // SNOOZED ALARMS PER TYPE
    // ============================================================

    std::unordered_map<
        SensorChannelType,
        bool
    > snoozed;


    // ============================================================
    // DISPATCHER
    // ============================================================

    AlarmDispatcher dispatcher;


    // ============================================================
    // REGISTRATION
    // ============================================================

    void AddSensor(
        Sensor* sensor)
    {
        if (!sensor)
            return;

        // Evita registrazioni duplicate
        for (auto* existing : sensors)
        {
            if (existing == sensor)
                return;
        }

        sensors.push_back(sensor);
    }


    void AddToZone(
        const ZoneName& zone,
        Sensor* sensor)
    {
        if (!sensor)
            return;

        // Stringa vuota = sensore senza zona
        if (zone.empty())
            return;

        // Assicura che la zona esista
        auto& info = zones[zone];

        // Evita duplicati nella zona
        for (auto* existing : info.sensors)
        {
            if (existing == sensor)
            {
                sensorZones[sensor] = zone;
                return;
            }
        }

        info.sensors.push_back(sensor);

        // Memorizza l'associazione inversa
        sensorZones[sensor] = zone;
    }


    // ============================================================
    // SENSOR -> ZONE QUERY
    // ============================================================

    const ZoneName* GetSensorZone(
        const Sensor* sensor) const
    {
        if (!sensor)
            return nullptr;

        auto it =
            sensorZones.find(
                const_cast<Sensor*>(sensor)
            );

        if (it == sensorZones.end())
            return nullptr;

        return &it->second;
    }


    bool IsSensorZoned(
        const Sensor* sensor) const
    {
        return GetSensorZone(sensor) != nullptr;
    }


    // ============================================================
    // ZONE STATE QUERY
    // ============================================================

    bool IsZoneEnabled(
        const ZoneName& zone) const
    {
        auto it =
            zones.find(zone);

        if (it == zones.end())
            return false;

        return it->second.enabled;
    }


    bool IsZoneEngaged(
        const ZoneName& zone) const
    {
        auto it =
            zones.find(zone);

        if (it == zones.end())
            return false;

        return it->second.engaged;
    }


    // ============================================================
    // EFFECTIVE SENSOR SECURITY STATE
    // ============================================================
    //
    // enabled:
    //
    //   sensore unzoned:
    //       sensor.enabled
    //
    //   sensore zonato:
    //       sensor.enabled && zone.enabled
    //
    //
    // engaged:
    //
    //   sensore unzoned:
    //       sensor.engaged
    //
    //   sensore zonato:
    //       sensor.engaged && zone.engaged
    //
    // ============================================================

    bool IsSensorSecurityEnabled(
        const Sensor* sensor) const
    {
        if (!sensor)
            return false;

        if (!sensor->IsEnabled())
            return false;

        const ZoneName* zone =
            GetSensorZone(sensor);

        // Sensore senza zona
        if (!zone)
            return true;

        // Sensore appartenente ad una zona
        return IsZoneEnabled(*zone);
    }


    bool IsSensorSecurityEngaged(
        const Sensor* sensor,
        SensorChannelType type) const
    {
        if (!sensor)
            return false;

        bool engaged = false;

        switch (type)
        {
            case SensorChannelType::RT:
                engaged =
                    sensor->IsEngagedRT();
                break;

            case SensorChannelType::H24:
            case SensorChannelType::MASK:
                engaged =
                    sensor->IsEngagedH24();
                break;

            default:
                return false;
        }

        if (!engaged)
            return false;

        const ZoneName* zone =
            GetSensorZone(sensor);

        if (!zone)
            return true;

        return IsZoneEngaged(*zone);
    }


    bool IsSensorSecurityAlarm(
        const Sensor* sensor,
        SensorChannelType type) const
    {
        if (!sensor)
            return false;

        if (!IsSensorSecurityEnabled(sensor))
            return false;

        if (!IsSensorSecurityEngaged(
                sensor,
                type))
        {
            return false;
        }

        const auto& outputs =
            sensor->Outputs();

        switch (type)
        {
            case SensorChannelType::RT:
                return outputs.rt;

            case SensorChannelType::H24:
            case SensorChannelType::MASK:
                return outputs.h24;

            case SensorChannelType::LEN:
                return sensor->ChannelAlarm(
                    SensorChannelType::LEN
                );

            default:
                return false;
        }
    }


    // ============================================================
    // ZONE STATUS AREA
    // ============================================================

    bool SetZoneStatusArea(
        const ZoneName& zone,
        int statusArea)
    {
        if (statusArea < 0)
            return false;

        auto it =
            zones.find(zone);

        if (it == zones.end())
            return false;

        it->second.statusArea =
            statusArea;

        return true;
    }


    int GetZoneStatusArea(
        const ZoneName& zone) const
    {
        auto it =
            zones.find(zone);

        if (it == zones.end())
            return -1;

        return it->second.statusArea;
    }


    // ============================================================
    // GET ZONE
    // ============================================================

    const std::vector<Sensor*>& GetZone(
        const ZoneName& zone) const
    {
        static const std::vector<Sensor*> empty;

        auto it =
            zones.find(zone);

        if (it == zones.end())
            return empty;

        return it->second.sensors;
    }


    // ============================================================
    // ENABLE / DISABLE ZONE
    // ============================================================

    bool EnableZone(
        const ZoneName& zone,
        bool mode)
    {
        auto it =
            zones.find(zone);

        if (it == zones.end())
            return false;

        it->second.enabled =
            mode;

        return true;
    }


    void EnableAll(
        bool mode)
    {
        for (auto& [zoneName, zoneInfo] : zones)
        {
            (void)zoneName;

            zoneInfo.enabled =
                mode;
        }
    }


    // ============================================================
    // ENGAGE / DISENGAGE ZONE
    // ============================================================

    bool EngageZone(
        const ZoneName& zone,
        bool mode)
    {
        auto it =
            zones.find(zone);

        if (it == zones.end())
            return false;

        it->second.engaged =
            mode;

        return true;
    }


    void EngageAll(
        bool mode)
    {
        for (auto& [zoneName, zoneInfo] : zones)
        {
            (void)zoneName;

            zoneInfo.engaged =
                mode;
        }
    }


    // ============================================================
    // ALARM QUERIES
    // ============================================================

    bool ZoneAlarm(
        const ZoneName& zone) const
    {
        auto itZone =
            zones.find(zone);

        if (itZone == zones.end())
            return false;

        if (!itZone->second.enabled)
            return false;

        for (auto* sensor : itZone->second.sensors)
        {
            if (!sensor)
                continue;

            if (!sensor->IsEnabled())
                continue;

            if (!sensor->Outputs().rt &&
                !sensor->Outputs().h24)
            {
                continue;
            }

            return true;
        }

        return false;
    }


    bool AnyAlarm() const
    {
        for (auto* sensor : sensors)
        {
            if (!sensor)
                continue;

            if (!IsSensorSecurityEnabled(sensor))
                continue;

            if (!sensor->Outputs().rt &&
                !sensor->Outputs().h24)
            {
                continue;
            }

            return true;
        }

        return false;
    }


    bool ZoneAlarmByType(
        const ZoneName& zone,
        SensorChannelType type) const
    {
        auto itZone =
            zones.find(zone);

        if (itZone == zones.end())
            return false;

        if (!itZone->second.enabled)
            return false;

        for (auto* sensor : itZone->second.sensors)
        {
            if (!sensor)
                continue;

            if (!sensor->IsEnabled())
                continue;

            const auto& out =
                sensor->Outputs();

            bool active = false;

            switch (type)
            {
                case SensorChannelType::RT:
                    active = out.rt;
                    break;

                case SensorChannelType::H24:
                    active = out.h24;
                    break;

                default:
                    active = sensor->ChannelAlarm(type);
                    break;
            }

            if (active)
                return true;
        }

        return false;
    }


    bool AnyAlarmByType(
        SensorChannelType type) const
    {
        for (auto* sensor : sensors)
        {
            if (!sensor)
                continue;

            if (!IsSensorSecurityAlarm(
                    sensor,
                    type))
            {
                continue;
            }

            return true;
        }

        return false;
    }


    std::vector<Sensor*> SensorsInAlarm(
        SensorChannelType type) const
    {
        std::vector<Sensor*> out;

        for (auto* sensor : sensors)
        {
            if (!sensor)
                continue;

            if (!IsSensorSecurityAlarm(
                    sensor,
                    type))
            {
                continue;
            }

            out.push_back(sensor);
        }

        return out;
    }


    // ============================================================
    // NEW ALARM DETECTION + SNOOZE
    // ============================================================

    bool ProcessAllTypes()
    {
        static const SensorChannelType types[] =
        {
            SensorChannelType::RT,
            SensorChannelType::H24,
            SensorChannelType::MASK,
            SensorChannelType::LEN
        };

        bool changed = false;


        // ========================================================
        // PROCESS EVERY REGISTERED SENSOR
        // ========================================================

        for (SensorChannelType type : types)
        {
            bool current = false;

            const bool isSnoozed =
                snoozed[type];


            for (auto* sensor : sensors)
            {
                if (!sensor)
                    continue;


                SensorChannel* ch =
                    sensor->Get(type);

                if (!ch)
                    continue;


                // ------------------------------------------------
                // SECURITY STATE
                // ------------------------------------------------

                const bool sensorCurrent =
                    IsSensorSecurityAlarm(
                        sensor,
                        type
                    );


                if (sensorCurrent)
                    current = true;


                // ------------------------------------------------
                // PREVIOUS STATE
                // ------------------------------------------------

                const bool sensorPrevious =
                    ch->GetLastAlarmState();


                if (sensorCurrent ==
                    sensorPrevious)
                {
                    continue;
                }


                // ------------------------------------------------
                // STATE CHANGED
                // ------------------------------------------------

                changed = true;

                ch->SetLastAlarmState(
                    sensorCurrent
                );


                // ------------------------------------------------
                // ZONE NAME
                // ------------------------------------------------

                const ZoneName* zone =
                    GetSensorZone(sensor);

                const ZoneName emptyZone;

                const ZoneName& dispatchZone =
                    zone
                        ? *zone
                        : emptyZone;


                // ------------------------------------------------
                // ACTIVATION
                // ------------------------------------------------

                if (sensorCurrent)
                {
                    if (isSnoozed)
                        continue;

                    std::vector<Sensor*> affected;

                    affected.push_back(
                        sensor
                    );

                    dispatcher.Dispatch(
                        dispatchZone,
                        type,
                        affected,
                        true
                    );

                    continue;
                }


                // ------------------------------------------------
                // RESTORATION
                // ------------------------------------------------

                std::vector<Sensor*> affected;

                affected.push_back(
                    sensor
                );

                dispatcher.Dispatch(
                    dispatchZone,
                    type,
                    affected,
                    false
                );
            }


            // ----------------------------------------------------
            // SNOOZE RESET
            // ----------------------------------------------------

            if (!current)
                snoozed[type] = false;


            // ----------------------------------------------------
            // SAVE GLOBAL TYPE STATE
            // ----------------------------------------------------

            lastState[type] =
                current;
        }


        return changed;
    }


    // ============================================================
    // NEW ALARM BY TYPE
    // ============================================================

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


        // --------------------------------------------------------
        // DISPATCH ALL ACTIVE EFFECTIVE SENSORS
        // --------------------------------------------------------

        for (auto* sensor : sensors)
        {
            if (!sensor)
                continue;


            if (!IsSensorSecurityAlarm(
                    sensor,
                    type))
            {
                continue;
            }


            const ZoneName* zone =
                GetSensorZone(sensor);

            const ZoneName emptyZone;

            const ZoneName& dispatchZone =
                zone
                    ? *zone
                    : emptyZone;


            std::vector<Sensor*> affected;

            affected.push_back(
                sensor
            );


            dispatcher.Dispatch(
                dispatchZone,
                type,
                affected,
                true
            );
        }


        return true;
    }


    // ============================================================
    // SNOOZE
    // ============================================================

    void Snooze(
        SensorChannelType type)
    {
        snoozed[type] = true;
    }


    // ============================================================
    // RESET
    // ============================================================

    void ResetZone(
        const ZoneName& zone)
    {
        for (auto* sensor : GetZone(zone))
        {
            if (!sensor)
                continue;

            sensor->Reset();
        }
    }


    void ResetAll()
    {
        for (auto* sensor : sensors)
        {
            if (!sensor)
                continue;

            sensor->Reset();
        }
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
        int statusArea = -1;
    };

    using ConfigType = WiredSensorConfig;

private:
    ZoneManager zones;
    std::vector<Sensor*> sensors;

    const ConfigType* config = nullptr;
    size_t configCount = 0;

    // mappe generate automaticamente
    std::unordered_map<std::string, std::vector<Sensor*>> zoneMap;

    // ---------------------------------------------------------
    // ORDINE STABILE DELLE ZONE
    // ---------------------------------------------------------

    std::vector<std::string> zoneNames;

    // =============================================================
    // INIT COMUNE
    // =============================================================

    void initInternal(
        const ConfigType* cfg,
        size_t count,
        uint32_t startupInhibitMs)
    {
        config = cfg;
        configCount = count;

        sensors.reserve(count);
        zoneNames.reserve(count);

        if (!cfg || count == 0)
        {
            LOG_DF(
                "WiredSensors",
                "Init: nessun sensore configurato"
            );

            return;
        }


        // ============================================================
        // CREATE SENSORS
        // ============================================================

        for (size_t i = 0; i < count; ++i)
        {
            const auto& c =
                cfg[i];


            // --------------------------------------------------------
            // SENSOR
            // --------------------------------------------------------

            Sensor* s =
                new Sensor(c.channels);

            if (startupInhibitMs != 0)
            {
                s->SetStartupInhibit(
                    startupInhibitMs
                );
            }

            sensors.push_back(s);


            // --------------------------------------------------------
            // ZONE MANAGER
            // --------------------------------------------------------

            zones.AddSensor(s);


            // --------------------------------------------------------
            // OPTIONAL ZONE
            // --------------------------------------------------------
            //
            // nullptr oppure stringa vuota =
            // sensore senza zona.
            //
            // In questo caso:
            //   - non viene creato alcun oggetto zona
            //   - non viene inserito nella zoneMap
            //   - non viene inserito nello zoneNames
            //
            // Il sensore rimane comunque registrato globalmente.
            //
            // --------------------------------------------------------

            const char* zone =
                c.zone;


            if (zone &&
                zone[0] != '\0')
            {
                const std::string zoneName =
                    zone;


                // ----------------------------------------------------
                // ZONE MANAGER
                // ----------------------------------------------------

                zones.AddToZone(
                    zoneName,
                    s
                );


                // ----------------------------------------------------
                // ZONE MAP
                // ----------------------------------------------------

                zoneMap[zoneName].push_back(
                    s
                );


                // ----------------------------------------------------
                // STABLE ZONE INDEX
                // ----------------------------------------------------

                if (std::find(
                        zoneNames.begin(),
                        zoneNames.end(),
                        zoneName
                    ) == zoneNames.end())
                {
                    zoneNames.push_back(
                        zoneName
                    );
                }
            }
        }


        // ============================================================
        // INIT REPORT
        // ============================================================

        if (startupInhibitMs != 0)
        {
            LOG_DF(
                "WiredSensors",
                "Init: configurati %u sensori "
                "(startup inhibit=%u ms)",
                (unsigned)count,
                (unsigned)startupInhibitMs
            );
        }
        else
        {
            LOG_DF(
                "WiredSensors",
                "Init: configurati %u sensori",
                (unsigned)count
            );
        }


        // ============================================================
        // OPTIONAL DIAGNOSTIC
        // ============================================================

        size_t zonedSensors = 0;
        size_t unzonedSensors = 0;

        for (size_t i = 0; i < count; ++i)
        {
            const auto& c =
                cfg[i];

            if (c.zone &&
                c.zone[0] != '\0')
            {
                ++zonedSensors;
            }
            else
            {
                ++unzonedSensors;
            }
        }

        LOG_DF(
            "WiredSensors",
            "Init: zoned=%u | unzoned=%u | zones=%u",
            (unsigned)zonedSensors,
            (unsigned)unzonedSensors,
            (unsigned)zoneNames.size()
        );
    }


public:
     // =============================================================
    // INIT STANDARD
    // =============================================================

    void Init(
        const ConfigType* cfg,
        size_t count)
    {
        initInternal(
            cfg,
            count,
            0
        );
    }


    // =============================================================
    // INIT CON STARTUP INHIBIT
    // =============================================================

    void Init(
        const ConfigType* cfg,
        size_t count,
        uint32_t startupInhibitMs)
    {
        initInternal(
            cfg,
            count,
            startupInhibitMs
        );
    }

    // --- Aggregate state computed from wired sensors
    struct AggregateSignals
    {
        bool intrusion = false;

        bool intrusionH24 = false;

        bool flood = false;

        bool smoke = false;

        bool windowsOpen = false;

        bool doorsOpen = false;
    };


    struct AggregateState
    {
        // ============================================================
        // REALTIME
        // ============================================================

        AggregateSignals normal;


        // ============================================================
        // MEMORY
        // ============================================================

        AggregateSignals mem;


        // ============================================================
        // OTHER
        // ============================================================

        bool mask = false;

        bool tamper = false;
    };

    struct ProcessResult
    {
        bool changed = false;
        bool zoneChanged = false;
    };

        ProcessResult Process(uint32_t now)
    {
        ProcessResult result;

        const ConfigType* cfg =
            config;

        const size_t count =
            configCount;

        if (!cfg || count == 0)
            return result;

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
            {
                result.changed = true;

                // Se il sensore appartiene a una zona,
                // anche la zona deve essere considerata variata.
                if (cfg[i].zone &&
                    cfg[i].zone[0] != '\0')
                {
                    result.zoneChanged = true;
                }
            }
        }

        return result;
    }

    // --- Compute aggregated state from current sensors
    AggregateState ComputeAggregate() const
    {
        AggregateState st;


        const ConfigType* cfg =
            config;

        const size_t count =
            configCount;


        // ============================================================
        // VALIDATION
        // ============================================================

        if (!cfg || count == 0)
            return st;


        // ============================================================
        // SENSOR LOOP
        // ============================================================

        for (size_t i = 0; i < count; ++i)
        {
            const ConfigType& c =
                cfg[i];

            Sensor* s =
                sensors[i];


            if (!s)
                continue;


            // ========================================================
            // SENSOR ENABLE
            // ========================================================

            if (!s->IsEnabled())
                continue;


            // ========================================================
            // OPTIONAL ZONE LOOKUP
            // ========================================================

            const ZoneManager::ZoneInfo* zoneInfo =
                nullptr;


            if (c.zone &&
                c.zone[0] != '\0')
            {
                auto itZone =
                    zones.zones.find(
                        c.zone
                    );

                if (itZone == zones.zones.end())
                    continue;


                zoneInfo =
                    &itZone->second;
            }


            // ========================================================
            // EFFECTIVE SECURITY ENABLE
            // ========================================================

            if (zoneInfo &&
                !zoneInfo->enabled)
            {
                continue;
            }


            // ========================================================
            // CATEGORY
            // ========================================================

            switch (c.category)
            {
                // ====================================================
                // PIR
                // ====================================================

                case SensorCategory::PIR:
                {
                    // ------------------------------------------------
                    // REALTIME
                    // ------------------------------------------------

                    if (s->Outputs().rt)
                    {
                        st.normal.intrusion = true;
                    }


                    const SensorChannel* h24 =
                        s->Get(
                            SensorChannelType::H24
                        );


                    if (h24 &&
                        h24->IsActive() &&
                        !h24->IsInhibit())
                    {
                        st.normal.intrusionH24 = true;
                    }


                    // ------------------------------------------------
                    // MEMORY
                    // ------------------------------------------------

                    if (s->Outputs().rtMem)
                    {
                        st.mem.intrusion = true;
                    }


                    if (s->Outputs().h24Mem)
                    {
                        st.mem.intrusionH24 = true;
                    }


                    // ------------------------------------------------
                    // MASK
                    // ------------------------------------------------

                    if (s->ChannelAlarm(
                            SensorChannelType::MASK))
                    {
                        st.mask = true;
                    }

                    break;
                }


                // ====================================================
                // WINDOW
                // ====================================================

                case SensorCategory::WINDOW:
                {
                    // ------------------------------------------------
                    // REALTIME
                    // ------------------------------------------------

                    if (s->Outputs().rt ||
                        s->Outputs().h24)
                    {
                        st.normal.windowsOpen = true;
                    }


                    // ------------------------------------------------
                    // MEMORY
                    // ------------------------------------------------

                    if (s->Outputs().rtMem ||
                        s->Outputs().h24Mem)
                    {
                        st.mem.windowsOpen = true;
                    }

                    break;
                }


                // ====================================================
                // DOOR
                // ====================================================

                case SensorCategory::DOOR:
                {
                    // ------------------------------------------------
                    // REALTIME
                    // ------------------------------------------------

                    if (s->Outputs().rt ||
                        s->Outputs().h24)
                    {
                        st.normal.doorsOpen = true;
                    }


                    // ------------------------------------------------
                    // MEMORY
                    // ------------------------------------------------

                    if (s->Outputs().rtMem ||
                        s->Outputs().h24Mem)
                    {
                        st.mem.doorsOpen = true;
                    }

                    break;
                }


                // ====================================================
                // FLOOD
                // ====================================================

                case SensorCategory::FLOOD:
                {
                    // ------------------------------------------------
                    // REALTIME
                    // ------------------------------------------------

                    if (s->Outputs().rt ||
                        s->Outputs().h24)
                    {
                        st.normal.flood = true;
                    }


                    // ------------------------------------------------
                    // MEMORY
                    // ------------------------------------------------

                    if (s->Outputs().rtMem ||
                        s->Outputs().h24Mem)
                    {
                        st.mem.flood = true;
                    }

                    break;
                }


                // ====================================================
                // SMOKE
                // ====================================================

                case SensorCategory::SMOKE:
                {
                    // ------------------------------------------------
                    // REALTIME
                    // ------------------------------------------------

                    if (s->Outputs().rt ||
                        s->Outputs().h24)
                    {
                        st.normal.smoke = true;
                    }


                    // ------------------------------------------------
                    // MEMORY
                    // ------------------------------------------------

                    if (s->Outputs().rtMem ||
                        s->Outputs().h24Mem)
                    {
                        st.mem.smoke = true;
                    }

                    break;
                }


                // ====================================================
                // TAMPER
                // ====================================================

                case SensorCategory::TAMPER:
                {
                    if (s->Outputs().rt ||
                        s->Outputs().h24)
                    {
                        st.tamper = true;
                    }

                    break;
                }


                // ====================================================
                // UNKNOWN / NOT HANDLED
                // ====================================================

                default:
                    break;
            }
        }


        // ============================================================
        // RETURN
        // ============================================================

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

    const char* GetZoneName(size_t index) const
    {
        if (index >= zoneNames.size())
            return nullptr;

        return zoneNames[index].c_str();
    }

    int GetZoneIndex(const char* zone) const
    {
        if (!zone)
            return -1;

        for (size_t i = 0; i < zoneNames.size(); ++i)
        {
            if (zoneNames[i] == zone)
                return static_cast<int>(i);
        }

        return -1;
    }

    size_t GetZoneCount() const
    {
        return zoneNames.size();
    }

    // accesso al ZoneManager interno
    ZoneManager& Zones()
    {
        return zones;
    }

    const ZoneManager& Zones() const
    {
        return zones;
    }

    // accesso diretto ai sensori
    Sensor* GetSensor(size_t i) { return sensors[i]; }
    Sensor* GetSensor(size_t i) const { return sensors[i]; }


    size_t Count() const { return configCount; }
    const ConfigType* GetConfig() const { return config; }
};

class AlarmBitmaskManager
{
public:

    using AlarmCallback = void (*)(
        uint64_t newMask,
        uint64_t currentMask,
        uint64_t memMask,
        size_t bitIndex,
        size_t sensorIndex,
        SensorChannelType type
    );


    struct SignalInfo
    {
        size_t sensorIndex;
        SensorChannelType type;
    };


    // =========================================================
    // MASK
    // =========================================================

    uint64_t currentMask  = 0;
    uint64_t previousMask = 0;
    uint64_t memMask      = 0;
    uint64_t newMask      = 0;


    bool engage = false;


    // =========================================================
    // CALLBACK
    // =========================================================

    AlarmCallback onNewAlarm = nullptr;


    // =========================================================
    // MAP
    // bit -> sensor + channel
    // =========================================================

    std::vector<SignalInfo> map;


    // =========================================================
    // BUILD MAP
    // =========================================================

    bool BuildMap(WiredSensorsManager& ws)
    {
        map.clear();

        currentMask  = 0;
        previousMask = 0;
        memMask      = 0;
        newMask      = 0;

        size_t bit = 0;

        for (size_t sensorIndex = 0;
            sensorIndex < ws.Count();
            ++sensorIndex)
        {
            Sensor* s =
                ws.GetSensor(sensorIndex);

            if (!s)
                continue;

            for (const auto& ch : s->channels)
            {
                if (bit >= 64)
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


    // =========================================================
    // ENGAGE
    // =========================================================

    void SetEngage(bool mode)
    {
        engage = mode;
    }


    bool IsEngaged() const
    {
        return engage;
    }


    // =========================================================
    // RESET MEMORY
    // =========================================================

    void ResetMemory()
    {
        memMask = 0;
    }

    // =========================================================
    // RESET SELECTED MEMORY
    //
    // Cancella dalla memoria solo i bit specificati.
    //
    // Gli altri allarmi memorizzati rimangono invariati.
    // =========================================================

    void ResetMemory(
        uint64_t mask)
    {
        memMask &=
            ~mask;
    }
    
    // =========================================================
    // CURRENT MASK
    // =========================================================

    bool ComputeCurrent(
        WiredSensorsManager& ws)
    {
        const uint64_t oldCurrent =
            currentMask;

        currentMask = 0;


        // -----------------------------------------------------
        // CURRENT MASK
        // -----------------------------------------------------
        //
        // Il canale entra nel CURRENT MASK solo se è
        // effettivamente attivo nella SECURITY LOGIC:
        //
        //   Sensor ENABLED
        //   +
        //   Zone ENABLED     (se il sensore è zonato)
        //   +
        //   Sensor ENGAGED
        //   +
        //   Zone ENGAGED     (se il sensore è zonato)
        //   +
        //   ChannelAlarm()
        //
        // Il mapping bit -> sensor/channel resta invariato.
        // -----------------------------------------------------

        auto& zoneManager =
            ws.Zones();


        for (size_t bit = 0;
            bit < map.size();
            ++bit)
        {
            const auto& info =
                map[bit];


            Sensor* s =
                ws.GetSensor(
                    info.sensorIndex
                );


            if (!s)
                continue;


            if (!zoneManager.IsSensorSecurityAlarm(
                    s,
                    info.type))
            {
                continue;
            }


            currentMask |=
                (uint64_t(1) << bit);
        }


        // -----------------------------------------------------
        // NEW = fronte di salita
        // -----------------------------------------------------

        newMask =
            currentMask &
            ~previousMask;


        // -----------------------------------------------------
        // MEMORY
        // -----------------------------------------------------

        if (engage)
        {
            memMask |=
                newMask;
        }


        // -----------------------------------------------------
        // SAVE CURRENT
        // -----------------------------------------------------

        previousMask =
            currentMask;


        // -----------------------------------------------------
        // CALLBACK
        // -----------------------------------------------------

        if (newMask != 0 && onNewAlarm)
        {
            for (size_t bitIndex = 0;
                bitIndex < map.size();
                ++bitIndex)
            {
                const uint64_t bitMask =
                    (uint64_t(1) << bitIndex);


                if (!(newMask & bitMask))
                    continue;


                const auto& info =
                    map[bitIndex];


                onNewAlarm(
                    newMask,
                    currentMask,
                    memMask,
                    bitIndex,
                    info.sensorIndex,
                    info.type
                );
            }
        }


        return
            oldCurrent != currentMask;
    }


    // =========================================================
    // CALLBACK
    // =========================================================

    void SetCallback(
        AlarmCallback cb)
    {
        onNewAlarm = cb;
    }

    void ClearNewMask(
        uint64_t mask)
    {
        newMask &=
            ~mask;
    }

    void ReportMap(
        const WiredSensorsManager& ws) const
    {
        LOG_IF(
            "AlarmBitmaskManager",
            "===== ALARM BITMAP ====="
        );

        for (size_t bit = 0;
            bit < map.size();
            ++bit)
        {
            const auto& info =
                map[bit];

            const auto* cfg =
                ws.GetConfig();

            const char* name = "?";
            const char* zone = "?";

            if (cfg &&
                info.sensorIndex < ws.Count())
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

            LOG_IF(
                "AlarmBitmaskManager",
                "bit=%u sensor=%u name=%s zone=%s type=%u",
                (unsigned)bit,
                (unsigned)info.sensorIndex,
                name,
                zone,
                (unsigned)info.type
            );
        }

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
};



#endif
