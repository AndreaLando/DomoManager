
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <set>

#include "DMWiredSensors.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"



// ============================================================
// NOTE
// ============================================================
//
// AlarmPanelInterface è il contratto comune per:
//
//   - comunicazione / supervisione
//   - stato centrale
//   - partizioni
//   - zone
//   - bypass
//   - tamper / trouble
//   - utenti / permessi
//   - eventi / audit trail
//   - diagnostica
//   - report runtime
//
// Il report è READ-ONLY:
// non deve eseguire arm/disarm/bypass/authentication.
//
// Le informazioni specifiche del driver/pannello concreto
// rimangono disponibili tramite diagnostic().
// ============================================================


class AlarmPanelInterface
{
public:

    // ============================================================
    // ENUMS
    // ============================================================

    enum class ArmState : uint8_t
    {
        DISARMED,
        ARMED_STAY,
        ARMED_AWAY,
        ARMED_NIGHT,
        ARMED_PARTIAL,
        NOT_READY,
        UNKNOWN
    };


    enum class EventCategory : uint8_t
    {
        ALARM,
        APPLICATION
    };


    enum class EventType
    {
        ZONE_OPEN,
        ZONE_RESTORED,

        ZONE_BYPASSED,
        ZONE_BYPASS_CLEARED,

        ALARM_TRIGGERED,
        ALARM_RESTORED,

        TAMPER,
        TAMPER_RESTORED,

        TROUBLE,
        TROUBLE_RESTORED,

        MASKING,
        MASKING_RESTORED,

        COMM_FAULT,
        COMM_RESTORED,

        PARTITION_CHANGED,
        PANEL_READY,
        PANEL_NOT_READY,

        USER_AUTH_OK,
        USER_AUTH_FAIL,

        CUSTOM
    };


    enum class AlarmType : uint8_t
    {
        INTRUSION,
        INTRUSION_H24,
        HOLD_UP,
        TAMPER,
        TROUBLE,
        COMMUNICATION,
        MASKING,

        FIRE,
        SMOKE,
        FLOOD,

        CUSTOM
    };


    // ============================================================
    // REPORT MODE
    // ============================================================

    enum class ReportMode : uint8_t
    {
        CORE,
        COMMUNICATION,
        SECURITY,
        PARTITIONS,
        ZONES,
        EVENTS,
        INCONSISTENCIES,
        DIAGNOSTICS,

        FULL
    };


    // ============================================================
    // EVENT
    // ============================================================

    struct Event
    {
        // --------------------------------------------------------
        // CATEGORY
        // --------------------------------------------------------

        EventCategory category =
            EventCategory::APPLICATION;


        // --------------------------------------------------------
        // TYPE
        // --------------------------------------------------------

        EventType type =
            EventType::CUSTOM;


        // --------------------------------------------------------
        // ALARM TYPE
        // --------------------------------------------------------

        AlarmType alarmType =
            AlarmType::INTRUSION;


        // --------------------------------------------------------
        // CONTEXT
        // --------------------------------------------------------

        int zone = -1;

        int partition = -1;


        // --------------------------------------------------------
        // USER / DESCRIPTION
        // --------------------------------------------------------

        std::string user;

        std::string description;


        // --------------------------------------------------------
        // TIMESTAMP
        // --------------------------------------------------------

        unsigned long timestamp = 0;


        // --------------------------------------------------------
        // DEFAULT
        // --------------------------------------------------------

        Event() = default;


        // --------------------------------------------------------
        // COMPATIBILITÀ VECCHIE CHIAMATE
        //
        // emitEvent({
        //     EventType::ZONE_OPEN,
        //     zone,
        //     partition,
        //     user,
        //     description,
        //     timestamp
        // });
        // --------------------------------------------------------

        Event(
            EventType t,
            int z,
            int p,
            const std::string& u,
            const std::string& d,
            unsigned long ts)
            : category(EventCategory::APPLICATION),
              type(t),
              alarmType(AlarmType::INTRUSION),
              zone(z),
              partition(p),
              user(u),
              description(d),
              timestamp(ts)
        {
        }


        // --------------------------------------------------------
        // COSTRUTTORE COMPLETO
        // --------------------------------------------------------

        Event(
            EventCategory c,
            EventType t,
            AlarmType a,
            int z,
            int p,
            const std::string& u,
            const std::string& d,
            unsigned long ts)
            : category(c),
              type(t),
              alarmType(a),
              zone(z),
              partition(p),
              user(u),
              description(d),
              timestamp(ts)
        {
        }


        // --------------------------------------------------------
        // HELPERS
        // --------------------------------------------------------

        bool isAlarm() const
        {
            return category == EventCategory::ALARM;
        }


        bool isApplication() const
        {
            return category == EventCategory::APPLICATION;
        }


        bool hasZone() const
        {
            return zone >= 0;
        }


        bool hasPartition() const
        {
            return partition >= 0;
        }
    };


    // ============================================================
    // CALLBACKS
    // ============================================================

    using AlarmCallback =
        std::function<void(const Event&)>;

    using EventCallback =
        std::function<void(const Event&)>;


    // ============================================================
    // DISTRUTTORE
    // ============================================================

    virtual ~AlarmPanelInterface() = default;


    // ============================================================
    // IDENTIFICAZIONE CENTRALE
    //
    // Default non-breaking:
    // le implementazioni esistenti NON sono obbligate a override.
    // ============================================================

    virtual const char* getManufacturer() const
    {
        return "UNKNOWN";
    }


    virtual const char* getModel() const
    {
        return "UNKNOWN";
    }


    virtual const char* getFirmwareVersion() const
    {
        return "UNKNOWN";
    }


    virtual const char* getSerialNumber() const
    {
        return "UNKNOWN";
    }


    // ============================================================
    // COMUNICAZIONE / SUPERVISIONE
    // ============================================================

    virtual bool connect() = 0;

    virtual void disconnect() = 0;

    virtual bool isConnected() const = 0;

    virtual bool poll(unsigned long now) = 0;


    // ------------------------------------------------------------
    // Supervisione canale
    // ------------------------------------------------------------

    virtual bool isCommunicationFault() const = 0;

    virtual bool isChannelSupervised() const = 0;

    virtual int getChannelLatencyMs() const = 0;


    // ------------------------------------------------------------
    // Ridondanza / dual path
    // ------------------------------------------------------------

    virtual bool hasDualPath() const = 0;

    virtual bool getPathStatus(int pathIndex) const = 0;


    // ============================================================
    // STATO CENTRALE
    // ============================================================

    virtual ArmState getArmState(
        int partition = 0) const = 0;


    virtual bool isReady(
        int partition = 0) const = 0;


    // ------------------------------------------------------------
    // Numero partizioni
    //
    // Default 1 per compatibilità con implementazioni esistenti.
    // ------------------------------------------------------------

    virtual size_t getPartitionCount() const
    {
        return 1;
    }


    // ------------------------------------------------------------
    // Comandi
    // ------------------------------------------------------------

    virtual bool armAway(
        int partition = 0) = 0;


    virtual bool armStay(
        int partition = 0) = 0;


    virtual bool armNight(
        int partition = 0) = 0;


    virtual bool disarm(
        int partition = 0) = 0;


    // ============================================================
    // ZONE / BYPASS
    // ============================================================

    virtual bool getZoneState(
        int zone) const = 0;


    virtual bool getZoneTamper(
        int zone) const = 0;


    virtual bool getZoneTrouble(
        int zone) const = 0;


    virtual bool isZoneBypassed(
        int zone) const = 0;


    virtual size_t getZoneCount() const = 0;


    virtual bool bypassZone(
        int zone) = 0;


    virtual bool clearBypass(
        int zone) = 0;


    // ============================================================
    // TAMPER / TROUBLE GLOBALI
    // ============================================================

    virtual bool getGlobalTamper() const = 0;

    virtual bool getGlobalTrouble() const = 0;


    // ============================================================
    // UTENTI / PERMESSI
    // ============================================================

    virtual bool authenticateUser(
        const std::string& user,
        const std::string& pin) = 0;


    virtual bool hasPermission(
        const std::string& user,
        const std::string& action) const = 0;


    // ============================================================
    // EVENTI / AUDIT TRAIL
    // ============================================================

    virtual void setAlarmCallback(
        AlarmCallback cb) = 0;


    virtual void setEventCallback(
        EventCallback cb) = 0;


    virtual std::vector<Event> getEventLog() const = 0;


    virtual void clearEventLog() = 0;


    // ============================================================
    // DIAGNOSTICA DRIVER-SPECIFICA
    // ============================================================

    virtual void diagnostic() const = 0;


    virtual int getSystemBitmask() const = 0;


    // ============================================================
    // PUBLIC REPORT API
    // ============================================================

    void report(
        ReportMode mode = ReportMode::FULL) const
    {
        switch (mode)
        {
            case ReportMode::CORE:
                reportCore();
                break;


            case ReportMode::COMMUNICATION:
                reportCommunication();
                break;


            case ReportMode::SECURITY:
                reportSecurity();
                break;


            case ReportMode::PARTITIONS:
                reportPartitions();
                break;


            case ReportMode::ZONES:
                reportZones();
                break;


            case ReportMode::EVENTS:
                reportEvents();
                break;


            case ReportMode::INCONSISTENCIES:
                ReportInconsistencies();
                break;


            case ReportMode::DIAGNOSTICS:
                reportDiagnostics();
                break;


            case ReportMode::FULL:
            default:
                reportFull();
                break;
        }
    }


    // ============================================================
    // BACKWARD / CONVENIENCE API
    // ============================================================

    void reportAll() const
    {
        report(ReportMode::FULL);
    }


    void reportCoreOnly() const
    {
        report(ReportMode::CORE);
    }


    void reportCommunicationOnly() const
    {
        report(ReportMode::COMMUNICATION);
    }


    void reportSecurityOnly() const
    {
        report(ReportMode::SECURITY);
    }


    void reportPartitionsOnly() const
    {
        report(ReportMode::PARTITIONS);
    }


    void reportZonesOnly() const
    {
        report(ReportMode::ZONES);
    }


    void reportEventsOnly() const
    {
        report(ReportMode::EVENTS);
    }


    void reportInconsistenciesOnly() const
    {
        report(ReportMode::INCONSISTENCIES);
    }


    void reportDiagnosticsOnly() const
    {
        report(ReportMode::DIAGNOSTICS);
    }


protected:

    // ============================================================
    // REPORT - HEADER
    // ============================================================

    void reportHeader(
        const char* title) const
    {
        LOG_IF(
            "AlarmPanelInterface",
            "================================================"
        );

        LOG_IF(
            "AlarmPanelInterface",
            "%s",
            title
        );

        LOG_IF(
            "AlarmPanelInterface",
            "================================================"
        );
    }


    void reportFooter() const
    {
        LOG_IF(
            "AlarmPanelInterface",
            "================================================"
        );
    }


    // ============================================================
    // REPORT - CORE
    // ============================================================

    void reportCore() const
    {
        reportHeader(
            "             ALARM PANEL CORE REPORT"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Manufacturer       : %s",
            getManufacturer()
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Model              : %s",
            getModel()
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Firmware           : %s",
            getFirmwareVersion()
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Serial             : %s",
            getSerialNumber()
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Connected          : %s",
            isConnected() ? "YES" : "NO"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Communication fault: %s",
            isCommunicationFault() ? "YES" : "NO"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Global tamper      : %s",
            getGlobalTamper() ? "ACTIVE" : "OK"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Global trouble     : %s",
            getGlobalTrouble() ? "ACTIVE" : "OK"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Partitions         : %u",
            static_cast<unsigned>(getPartitionCount())
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Zones              : %u",
            static_cast<unsigned>(getZoneCount())
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Dual path          : %s",
            hasDualPath() ? "YES" : "NO"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "System bitmask     : 0x%08X",
            static_cast<unsigned>(getSystemBitmask())
        );


        reportFooter();
    }


    // ============================================================
    // REPORT - COMMUNICATION
    // ============================================================

    void reportCommunication() const
    {
        reportHeader(
            "          ALARM PANEL COMMUNICATION REPORT"
        );


        const bool connected =
            isConnected();

        const bool fault =
            isCommunicationFault();

        const bool supervised =
            isChannelSupervised();

        const int latency =
            getChannelLatencyMs();

        const bool dualPath =
            hasDualPath();


        LOG_IF(
            "AlarmPanelInterface",
            "Connected           : %s",
            connected ? "YES" : "NO"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Communication fault : %s",
            fault ? "ACTIVE" : "OK"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Channel supervised  : %s",
            supervised ? "YES" : "NO"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Channel latency     : %d ms",
            latency
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Dual path           : %s",
            dualPath ? "YES" : "NO"
        );


        // --------------------------------------------------------
        // PATH 0
        // --------------------------------------------------------

        LOG_IF(
            "AlarmPanelInterface",
            "Path 0              : %s",
            getPathStatus(0) ? "UP" : "DOWN"
        );


        // --------------------------------------------------------
        // PATH 1
        // --------------------------------------------------------

        if (dualPath)
        {
            LOG_IF(
                "AlarmPanelInterface",
                "Path 1              : %s",
                getPathStatus(1) ? "UP" : "DOWN"
            );


            if (!getPathStatus(0) &&
                !getPathStatus(1))
            {
                LOG_IF(
                    "AlarmPanelInterface",
                    "WARNING             : ALL COMMUNICATION PATHS DOWN"
                );
            }
        }


        reportFooter();
    }


    // ============================================================
    // REPORT - SECURITY
    // ============================================================

    void reportSecurity() const
    {
        reportHeader(
            "             ALARM PANEL SECURITY REPORT"
        );


        size_t openZones = 0;
        size_t tamperZones = 0;
        size_t troubleZones = 0;
        size_t bypassedZones = 0;


        const size_t zoneCount =
            getZoneCount();


        for (size_t i = 0; i < zoneCount; ++i)
        {
            const int zone =
                static_cast<int>(i);


            if (getZoneState(zone))
                ++openZones;


            if (getZoneTamper(zone))
                ++tamperZones;


            if (getZoneTrouble(zone))
                ++troubleZones;


            if (isZoneBypassed(zone))
                ++bypassedZones;
        }


        LOG_IF(
            "AlarmPanelInterface",
            "Global tamper      : %s",
            getGlobalTamper() ? "ACTIVE" : "OK"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Global trouble     : %s",
            getGlobalTrouble() ? "ACTIVE" : "OK"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Open zones         : %u",
            static_cast<unsigned>(openZones)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Tamper zones       : %u",
            static_cast<unsigned>(tamperZones)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Trouble zones      : %u",
            static_cast<unsigned>(troubleZones)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Bypassed zones     : %u",
            static_cast<unsigned>(bypassedZones)
        );


        // --------------------------------------------------------
        // Security state per partition
        // --------------------------------------------------------

        const size_t partitionCount =
            getPartitionCount();


        for (size_t p = 0; p < partitionCount; ++p)
        {
            const int partition =
                static_cast<int>(p);


            LOG_IF(
                "AlarmPanelInterface",
                "Partition %d       : state=%s ready=%s",
                partition,
                armStateToString(
                    getArmState(partition)
                ),
                isReady(partition)
                    ? "YES"
                    : "NO"
            );
        }


        reportFooter();
    }


    // ============================================================
    // REPORT - PARTITIONS
    // ============================================================

    void reportPartitions() const
    {
        reportHeader(
            "            ALARM PANEL PARTITIONS REPORT"
        );


        const size_t count =
            getPartitionCount();


        LOG_IF(
            "AlarmPanelInterface",
            "Partition count    : %u",
            static_cast<unsigned>(count)
        );


        for (size_t p = 0; p < count; ++p)
        {
            const int partition =
                static_cast<int>(p);


            const ArmState state =
                getArmState(partition);


            const bool ready =
                isReady(partition);


            LOG_IF(
                "AlarmPanelInterface",
                "Partition %d | state=%s | ready=%s",
                partition,
                armStateToString(state),
                ready ? "YES" : "NO"
            );
        }


        reportFooter();
    }


    // ============================================================
    // REPORT - ZONES
    // ============================================================

    void reportZones() const
    {
        reportHeader(
            "               ALARM PANEL ZONES REPORT"
        );


        const size_t count =
            getZoneCount();


        LOG_IF(
            "AlarmPanelInterface",
            "Zone count : %u",
            static_cast<unsigned>(count)
        );


        for (size_t z = 0; z < count; ++z)
        {
            const int zone =
                static_cast<int>(z);


            const bool state =
                getZoneState(zone);


            const bool tamper =
                getZoneTamper(zone);


            const bool trouble =
                getZoneTrouble(zone);


            const bool bypass =
                isZoneBypassed(zone);


            LOG_IF(
                "AlarmPanelInterface",
                "Zone %d | state=%s | tamper=%s | trouble=%s | bypass=%s",
                zone,
                state ? "OPEN" : "RESTORED",
                tamper ? "YES" : "NO",
                trouble ? "YES" : "NO",
                bypass ? "YES" : "NO"
            );
        }


        reportFooter();
    }


    // ============================================================
    // REPORT - EVENT SUMMARY
    // ============================================================

    void reportEventSummary(
        const std::vector<Event>& events) const
    {
        size_t alarmCount = 0;
        size_t applicationCount = 0;

        size_t alarmTriggered = 0;
        size_t alarmRestored = 0;

        size_t tamperCount = 0;
        size_t troubleCount = 0;

        size_t commFault = 0;
        size_t commRestored = 0;

        size_t zoneOpen = 0;
        size_t zoneRestored = 0;

        size_t bypassed = 0;
        size_t bypassCleared = 0;

        size_t authOk = 0;
        size_t authFail = 0;


        for (const Event& event : events)
        {
            if (event.category ==
                EventCategory::ALARM)
            {
                ++alarmCount;
            }
            else
            {
                ++applicationCount;
            }


            switch (event.type)
            {
                case EventType::ALARM_TRIGGERED:
                    ++alarmTriggered;
                    break;


                case EventType::ALARM_RESTORED:
                    ++alarmRestored;
                    break;


                case EventType::TAMPER:
                case EventType::TAMPER_RESTORED:
                    ++tamperCount;
                    break;


                case EventType::TROUBLE:
                case EventType::TROUBLE_RESTORED:
                    ++troubleCount;
                    break;


                case EventType::COMM_FAULT:
                    ++commFault;
                    break;


                case EventType::COMM_RESTORED:
                    ++commRestored;
                    break;


                case EventType::ZONE_OPEN:
                    ++zoneOpen;
                    break;


                case EventType::ZONE_RESTORED:
                    ++zoneRestored;
                    break;


                case EventType::ZONE_BYPASSED:
                    ++bypassed;
                    break;


                case EventType::ZONE_BYPASS_CLEARED:
                    ++bypassCleared;
                    break;


                case EventType::USER_AUTH_OK:
                    ++authOk;
                    break;


                case EventType::USER_AUTH_FAIL:
                    ++authFail;
                    break;


                default:
                    break;
            }
        }


        LOG_IF(
            "AlarmPanelInterface",
            "Alarm events       : %u",
            static_cast<unsigned>(alarmCount)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Application events : %u",
            static_cast<unsigned>(applicationCount)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Alarm triggered    : %u",
            static_cast<unsigned>(alarmTriggered)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Alarm restored     : %u",
            static_cast<unsigned>(alarmRestored)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Tamper events      : %u",
            static_cast<unsigned>(tamperCount)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Trouble events     : %u",
            static_cast<unsigned>(troubleCount)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Comm faults        : %u",
            static_cast<unsigned>(commFault)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Comm restored      : %u",
            static_cast<unsigned>(commRestored)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Zone opened        : %u",
            static_cast<unsigned>(zoneOpen)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Zone restored      : %u",
            static_cast<unsigned>(zoneRestored)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Bypass activated   : %u",
            static_cast<unsigned>(bypassed)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Bypass cleared     : %u",
            static_cast<unsigned>(bypassCleared)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Authentication OK  : %u",
            static_cast<unsigned>(authOk)
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Authentication FAIL: %u",
            static_cast<unsigned>(authFail)
        );
    }


    // ============================================================
    // REPORT - EVENTS
    // ============================================================

    void reportEvents() const
    {
        reportHeader(
            "              ALARM PANEL EVENTS REPORT"
        );


        const std::vector<Event> events =
            getEventLog();


        LOG_IF(
            "AlarmPanelInterface",
            "Event count        : %u",
            static_cast<unsigned>(events.size())
        );


        LOG_IF(
            "AlarmPanelInterface",
            "--- EVENT SUMMARY ---"
        );


        reportEventSummary(events);


        LOG_IF(
            "AlarmPanelInterface",
            "--- EVENT DETAILS ---"
        );


        for (size_t i = 0; i < events.size(); ++i)
        {
            const Event& event =
                events[i];


            LOG_IF(
                "AlarmPanelInterface",
                "#%u ts=%lu category=%s type=%s alarm=%s zone=%d partition=%d",
                static_cast<unsigned>(i),
                event.timestamp,
                eventCategoryToString(event.category),
                eventTypeToString(event.type),
                alarmTypeToString(event.alarmType),
                event.zone,
                event.partition
            );


            if (!event.user.empty())
            {
                LOG_IF(
                    "AlarmPanelInterface",
                    "   user=%s",
                    event.user.c_str()
                );
            }


            if (!event.description.empty())
            {
                LOG_IF(
                    "AlarmPanelInterface",
                    "   description=%s",
                    event.description.c_str()
                );
            }
        }


        reportFooter();
    }


    // ============================================================
    // REPORT - INCONSISTENCIES
    // ============================================================
    static void ReportInconsistencies();
    
    // ============================================================
    // REPORT - DIAGNOSTICS
    // ============================================================

    void reportDiagnostics() const
    {
        reportHeader(
            "            ALARM PANEL DIAGNOSTICS REPORT"
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Manufacturer       : %s",
            getManufacturer()
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Model              : %s",
            getModel()
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Firmware           : %s",
            getFirmwareVersion()
        );


        LOG_IF(
            "AlarmPanelInterface",
            "Serial             : %s",
            getSerialNumber()
        );


        LOG_IF(
            "AlarmPanelInterface",
            "System bitmask     : 0x%08X",
            static_cast<unsigned>(getSystemBitmask())
        );


        // --------------------------------------------------------
        // Hook specifico del driver / centrale
        // --------------------------------------------------------

        LOG_IF(
            "AlarmPanelInterface",
            "--- DRIVER SPECIFIC DIAGNOSTICS ---"
        );


        diagnostic();


        reportFooter();
    }


    // ============================================================
    // REPORT - FULL
    // ============================================================

    void reportFull() const
    {
        reportHeader(
            "             ALARM PANEL FULL REPORT"
        );


        // --------------------------------------------------------
        // CORE
        // --------------------------------------------------------

        reportCore();


        // --------------------------------------------------------
        // COMMUNICATION
        // --------------------------------------------------------

        reportCommunication();


        // --------------------------------------------------------
        // SECURITY
        // --------------------------------------------------------

        reportSecurity();


        // --------------------------------------------------------
        // PARTITIONS
        // --------------------------------------------------------

        reportPartitions();


        // --------------------------------------------------------
        // ZONES
        // --------------------------------------------------------

        reportZones();


        // --------------------------------------------------------
        // EVENTS
        // --------------------------------------------------------

        reportEvents();


        // --------------------------------------------------------
        // INCONSISTENCIES
        // --------------------------------------------------------

        ReportInconsistencies();


        // --------------------------------------------------------
        // DRIVER DIAGNOSTICS
        // --------------------------------------------------------

        reportDiagnostics();


        reportFooter();
    }


    // ============================================================
    // STRING HELPERS
    // ============================================================

    static const char* armStateToString(
        ArmState state)
    {
        switch (state)
        {
            case ArmState::DISARMED:
                return "DISARMED";


            case ArmState::ARMED_STAY:
                return "ARMED_STAY";


            case ArmState::ARMED_AWAY:
                return "ARMED_AWAY";


            case ArmState::ARMED_NIGHT:
                return "ARMED_NIGHT";


            case ArmState::ARMED_PARTIAL:
                return "ARMED_PARTIAL";


            case ArmState::NOT_READY:
                return "NOT_READY";


            case ArmState::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }


    static const char* eventCategoryToString(
        EventCategory category)
    {
        switch (category)
        {
            case EventCategory::ALARM:
                return "ALARM";


            case EventCategory::APPLICATION:
                return "APPLICATION";


            default:
                return "UNKNOWN";
        }
    }


    static const char* eventTypeToString(
        EventType type)
    {
        switch (type)
        {
            case EventType::MASKING:
                return "MASKING";

            case EventType::MASKING_RESTORED:
                return "MASKING_RESTORED";

            case EventType::ZONE_OPEN:
                return "ZONE_OPEN";


            case EventType::ZONE_RESTORED:
                return "ZONE_RESTORED";


            case EventType::ZONE_BYPASSED:
                return "ZONE_BYPASSED";


            case EventType::ZONE_BYPASS_CLEARED:
                return "ZONE_BYPASS_CLEARED";


            case EventType::ALARM_TRIGGERED:
                return "ALARM_TRIGGERED";


            case EventType::ALARM_RESTORED:
                return "ALARM_RESTORED";


            case EventType::TAMPER:
                return "TAMPER";


            case EventType::TAMPER_RESTORED:
                return "TAMPER_RESTORED";


            case EventType::TROUBLE:
                return "TROUBLE";


            case EventType::TROUBLE_RESTORED:
                return "TROUBLE_RESTORED";


            case EventType::COMM_FAULT:
                return "COMM_FAULT";


            case EventType::COMM_RESTORED:
                return "COMM_RESTORED";


            case EventType::PARTITION_CHANGED:
                return "PARTITION_CHANGED";


            case EventType::PANEL_READY:
                return "PANEL_READY";


            case EventType::PANEL_NOT_READY:
                return "PANEL_NOT_READY";


            case EventType::USER_AUTH_OK:
                return "USER_AUTH_OK";


            case EventType::USER_AUTH_FAIL:
                return "USER_AUTH_FAIL";


            case EventType::CUSTOM:
            default:
                return "CUSTOM";
        }
    }


    static const char* alarmTypeToString(
        AlarmType type)
    {
        switch (type)
        {
            case AlarmType::INTRUSION:
                return "INTRUSION";


            case AlarmType::INTRUSION_H24:
                return "INTRUSION_H24";


            case AlarmType::HOLD_UP:
                return "HOLD_UP";


            case AlarmType::TAMPER:
                return "TAMPER";


            case AlarmType::TROUBLE:
                return "TROUBLE";


            case AlarmType::COMMUNICATION:
                return "COMMUNICATION";


            case AlarmType::MASKING:
                return "MASKING";


            case AlarmType::FIRE:
                return "FIRE";


            case AlarmType::SMOKE:
                return "SMOKE";


            case AlarmType::FLOOD:
                return "FLOOD";


            case AlarmType::CUSTOM:
            default:
                return "CUSTOM";
        }
    }
};



class SecurityOrchestrator
{
public:

    // ============================================================
    // ALARM PANEL COMMAND
    // ============================================================

    enum AlarmPanelCommand : uint8_t
    {
        DISARM = 0,
        ARM_AWAY,
        ARM_STAY,
        ARM_NIGHT,
        SILENCE_ALARM
    };


    // ============================================================
    // CHANGE FLAGS
    //
    // Indicano quale parte del sistema security ha rilevato
    // una variazione durante il ciclo.
    //
    // È un bitmask, quindi più variazioni possono coesistere.
    // ============================================================

    enum ChangeFlags : uint8_t
    {
        CHANGE_NONE    = 0,
        CHANGE_SENSOR  = 1 << 0,
        CHANGE_ZONE    = 1 << 1,
        CHANGE_SYSTEM  = 1 << 2,
        CHANGE_PANEL   = 1 << 3,
        CHANGE_COMM    = 1 << 4,
        CHANGE_COMMAND = 1 << 5
    };


    // ============================================================
    // REPORT MODE
    // ============================================================

    enum class ReportMode : uint8_t
    {
        CORE,
        CONFIG,
        SENSORS,
        ZONES,
        COMMANDS,
        SYSTEM,
        SECURITY,
        HMI,
        INCONSISTENCIES,
        DIAGNOSTICS,
        FULL
    };


    // ============================================================
    // SYSTEM MANAGER
    // ============================================================

    class SystemManager
    {
    public:

        enum Field
        {
            ALARM_INTRUSION,
            ALARM_INTRUSION_H24,
            ALARM_FLOOD,
            ALARM_SMOKE,
            WINDOWS_OPEN,
            DOORS_OPEN,
            ALARM_TAMPER,
            FIELD_COUNT
        };


        struct Info
        {
            Cell<bool> f[FIELD_COUNT];
        };


        Info info;


        // --------------------------------------------------------
        // CONSTRUCTOR
        // --------------------------------------------------------

        SystemManager()
        {
            for (int i = 0; i < FIELD_COUNT; ++i)
                info.f[i].set(false);
        }


        // --------------------------------------------------------
        // SET FIELD
        // --------------------------------------------------------

        void set(
            Field f,
            bool v)
        {
            info.f[f].setIfDiff(v);
        }


        // --------------------------------------------------------
        // HAS CHANGED
        //
        // Legge e consuma il flag di modifica.
        // --------------------------------------------------------

        bool hasChanged()
        {
            bool changed = false;

            for (int i = 0; i < FIELD_COUNT; ++i)
            {
                if (info.f[i].hasChanged())
                {
                    changed = true;
                    info.f[i].resetChanged();
                }
            }

            return changed;
        }


        // --------------------------------------------------------
        // BITMASK
        // --------------------------------------------------------

        int getBitmask() const
        {
            int mask = 0;
            for (int i = 0; i < FIELD_COUNT; ++i)
            {
                if (info.f[i].get())
                    mask |= (1 << i);
            }

            return mask;
        }


        // --------------------------------------------------------
        // COMPUTE FROM WIRED SENSORS
        // --------------------------------------------------------

        void ComputeFrom(
            const WiredSensorsManager& ws)
        {
            auto st = ws.ComputeAggregate();

            set(
                ALARM_INTRUSION,
                st.intrusion
            );

            set(
                ALARM_INTRUSION_H24,
                st.intrusionH24
            );

            set(
                ALARM_FLOOD,
                st.flood
            );

            set(
                ALARM_SMOKE,
                st.smoke
            );

            set(
                WINDOWS_OPEN,
                st.windowsOpen
            );

            set(
                DOORS_OPEN,
                st.doorsOpen
            );

            set(
                ALARM_TAMPER,
                st.tamper
            );
        }


        // --------------------------------------------------------
        // REPORT
        // --------------------------------------------------------

        void Report() const
        {
            LOG_IF(
                "SystemManager",
                "===== SYSTEM STATE ====="
            );

            LOG_IF(
                "SystemManager",
                "%-25s : %s",
                "Intrusion",
                info.f[ALARM_INTRUSION].get()
                    ? "TRUE"
                    : "false"
            );

            LOG_IF(
                "SystemManager",
                "%-25s : %s",
                "Intrusion H24",
                info.f[ALARM_INTRUSION_H24].get()
                    ? "TRUE"
                    : "false"
            );

            LOG_IF(
                "SystemManager",
                "%-25s : %s",
                "Flood",
                info.f[ALARM_FLOOD].get()
                    ? "TRUE"
                    : "false"
            );

            LOG_IF(
                "SystemManager",
                "%-25s : %s",
                "Smoke",
                info.f[ALARM_SMOKE].get()
                    ? "TRUE"
                    : "false"
            );

            LOG_IF(
                "SystemManager",
                "%-25s : %s",
                "Windows Open",
                info.f[WINDOWS_OPEN].get()
                    ? "TRUE"
                    : "false"
            );

            LOG_IF(
                "SystemManager",
                "%-25s : %s",
                "Doors Open",
                info.f[DOORS_OPEN].get()
                    ? "TRUE"
                    : "false"
            );

            LOG_IF(
                "SystemManager",
                "%-25s : %s",
                "Tamper",
                info.f[ALARM_TAMPER].get()
                    ? "TRUE"
                    : "false"
            );

            LOG_IF(
                "SystemManager",
                "Bitmask                : 0x%08X",
                (unsigned)getBitmask()
            );

            LOG_IF(
                "SystemManager",
                "========================"
            );
        }


        // --------------------------------------------------------
        // COMPATIBILITÀ
        // --------------------------------------------------------

        void DiagnosticReport() const
        {
            Report();
        }
    };


private:
    
    // ============================================================
    // STATIC STATE
    // ============================================================

    static inline SystemManager system;

    static inline WiredSensorsManager ws;

    static inline FrontendConfig::Security cfgCopy;

    static inline bool initialized = false;

    static inline AlarmBitmaskManager alarmMask;

    // Ultimo risultato di Loop()
    static inline uint8_t lastChanges = CHANGE_NONE;

    // Istante dell'ultimo Loop()
    static inline unsigned long lastLoopAt = 0;


    // ============================================================
    // VALIDATION
    // ============================================================

    static bool validate(
        const FrontendConfig::Security* cfg)
    {
        if (!cfg)
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Validate: null security configuration"
            );
            return false;
        }

        // --------------------------------------------------------
        // WIRED SENSORS
        // --------------------------------------------------------

        if (!cfg->sensors || cfg->count == 0)
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Validate: no wired sensors configured"
            );
            return false;
        }

        for (size_t i = 0; i < cfg->count; ++i)
        {
            const auto& s = cfg->sensors[i];

            if (!s.name || !s.name[0])
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor[%u] has invalid name",
                    (unsigned)i
                );
                return false;
            }

            if (!s.zone || !s.zone[0])
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor[%u] '%s' has invalid zone",
                    (unsigned)i,
                    s.name
                );
                return false;
            }

            if (s.channels.size() == 0)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor[%u] '%s' has no channels",
                    (unsigned)i,
                    s.name
                );
                return false;
            }

            if (s.channels.size() != s.readers.size())
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor[%u] '%s': channels=%u readers=%u",
                    (unsigned)i,
                    s.name,
                    (unsigned)s.channels.size(),
                    (unsigned)s.readers.size()
                );
                return false;
            }

            // Controllo duplicazione dei tipi di canale
            bool usedTypes[4] = { false, false, false, false };

            for (size_t j = 0; j < s.channels.size(); ++j)
            {
                const auto& ch = s.channels.begin()[j];

                const size_t typeIndex =
                    static_cast<size_t>(ch.type);

                if (typeIndex >= 4)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: sensor[%u] '%s': invalid channel type=%u",
                        (unsigned)i,
                        s.name,
                        (unsigned)typeIndex
                    );
                    return false;
                }

                if (usedTypes[typeIndex])
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: sensor[%u] '%s': duplicated channel type=%u",
                        (unsigned)i,
                        s.name,
                        (unsigned)typeIndex
                    );
                    return false;
                }

                usedTypes[typeIndex] = true;

                if (!s.readers.begin()[j])
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: sensor[%u] '%s': reader[%u] is empty",
                        (unsigned)i,
                        s.name,
                        (unsigned)j
                    );
                    return false;
                }
            }
        }

        // --------------------------------------------------------
        // SENSOR AREAS
        // --------------------------------------------------------

        // cmdArea: ogni sensore deve avere un'area diversa
        // quando l'area è effettivamente configurata.
        for (size_t i = 0; i < cfg->count; ++i)
        {
            const int cmdArea = cfg->sensors[i].cmdArea;

            if (cmdArea < 0)
                continue;

            for (size_t j = i + 1; j < cfg->count; ++j)
            {
                if (cfg->sensors[j].cmdArea == cmdArea)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: duplicated sensor cmdArea=%d "
                        "('%s' and '%s')",
                        cmdArea,
                        cfg->sensors[i].name,
                        cfg->sensors[j].name
                    );
                    return false;
                }
            }
        }

        // statusArea: un'area = un solo oggetto.
        for (size_t i = 0; i < cfg->count; ++i)
        {
            const int statusArea = cfg->sensors[i].statusArea;

            if (statusArea < 0)
                continue;

            // collisione tra sensori
            for (size_t j = i + 1; j < cfg->count; ++j)
            {
                if (cfg->sensors[j].statusArea == statusArea)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: duplicated sensor statusArea=%d "
                        "('%s' and '%s')",
                        statusArea,
                        cfg->sensors[i].name,
                        cfg->sensors[j].name
                    );
                    return false;
                }
            }

            // collisione con system status
            if (statusArea == cfg->statusArea && cfg->statusArea >= 0)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' statusArea=%d "
                    "collides with security statusArea",
                    cfg->sensors[i].name,
                    statusArea
                );
                return false;
            }
        }

        // --------------------------------------------------------
        // ZONE CONFIG
        // --------------------------------------------------------

        if (cfg->zoneCount > 0 && !cfg->zones)
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Validate: zoneCount=%u but zones is null",
                (unsigned)cfg->zoneCount
            );
            return false;
        }

        for (size_t i = 0; i < cfg->zoneCount; ++i)
        {
            const auto& z = cfg->zones[i];

            if (!z.name || !z.name[0])
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: zone[%u] has invalid name",
                    (unsigned)i
                );
                return false;
            }

            if (z.statusArea < 0)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: zone[%u] '%s' has invalid statusArea",
                    (unsigned)i,
                    z.name
                );
                return false;
            }

            // nome zona duplicato
            for (size_t j = i + 1; j < cfg->zoneCount; ++j)
            {
                if (cfg->zones[j].name &&
                    std::strcmp(z.name, cfg->zones[j].name) == 0)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: duplicated zone name '%s'",
                        z.name
                    );
                    return false;
                }

                // stessa statusArea assegnata a due zone
                if (cfg->zones[j].statusArea == z.statusArea)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: duplicated zone statusArea=%d "
                        "('%s' and '%s')",
                        z.statusArea,
                        z.name,
                        cfg->zones[j].name
                    );
                    return false;
                }
            }

            // collisione zona -> sensor statusArea
            for (size_t j = 0; j < cfg->count; ++j)
            {
                if (cfg->sensors[j].statusArea >= 0 &&
                    cfg->sensors[j].statusArea == z.statusArea)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: zone '%s' statusArea=%d "
                        "collides with sensor '%s'",
                        z.name,
                        z.statusArea,
                        cfg->sensors[j].name
                    );
                    return false;
                }
            }

            // collisione zona -> system status
            if (cfg->statusArea >= 0 &&
                z.statusArea == cfg->statusArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: zone '%s' statusArea=%d "
                    "collides with security statusArea",
                    z.name,
                    z.statusArea
                );
                return false;
            }
        }

        // --------------------------------------------------------
        // GLOBAL SECURITY AREAS
        // --------------------------------------------------------

        if (cfg->statusArea >= 0 &&
            cfg->panelCommandArea >= 0 &&
            cfg->statusArea == cfg->panelCommandArea)
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Validate: statusArea=%d collides with panelCommandArea",
                cfg->statusArea
            );
            return false;
        }

        if (cfg->eventArea >= 0 &&
            cfg->statusArea >= 0 &&
            cfg->eventArea == cfg->statusArea)
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Validate: eventArea=%d collides with statusArea",
                cfg->eventArea
            );
            return false;
        }

        if (cfg->eventArea >= 0 &&
            cfg->panelCommandArea >= 0 &&
            cfg->eventArea == cfg->panelCommandArea)
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Validate: eventArea=%d collides with panelCommandArea",
                cfg->eventArea
            );
            return false;
        }

        // --------------------------------------------------------
        // SENSOR COMMAND / STATUS AREA COLLISIONS
        // --------------------------------------------------------

        for (size_t i = 0; i < cfg->count; ++i)
        {
            const auto& s = cfg->sensors[i];

            if (s.cmdArea < 0)
                continue;

            if (s.statusArea >= 0 &&
                s.cmdArea == s.statusArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' cmdArea=%d "
                    "collides with its statusArea",
                    s.name,
                    s.cmdArea
                );
                return false;
            }

            if (cfg->statusArea >= 0 &&
                s.cmdArea == cfg->statusArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' cmdArea=%d "
                    "collides with security statusArea",
                    s.name,
                    s.cmdArea
                );
                return false;
            }

            if (cfg->eventArea >= 0 &&
                s.cmdArea == cfg->eventArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' cmdArea=%d "
                    "collides with eventArea",
                    s.name,
                    s.cmdArea
                );
                return false;
            }

            if (cfg->panelCommandArea >= 0 &&
                s.cmdArea == cfg->panelCommandArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' cmdArea=%d "
                    "collides with panelCommandArea",
                    s.name,
                    s.cmdArea
                );
                return false;
            }
        }

        return true;
    }


public:
    // ============================================================
    // RESET SELECTED ALARM MEMORY
    // ============================================================

    static void ResetAlarmMemory(
        uint64_t mask)
    {
        alarmMask.ResetMemory(
            mask
        );
    }
    
    // ============================================================
    // SETUP
    // ============================================================

    static void Setup(
        const FrontendConfig::Security* cfg)
    {
        if (!validate(cfg))
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Setup aborted: invalid wired sensors configuration"
            );

            initialized = false;
            return;
        }

        if (!cfg)
        {
            initialized = false;
            return;
        }

        cfgCopy = *cfg;

        // ============================================================
        // WIRED SENSORS / ZONES
        // ============================================================

        ws.Init(
            cfg->sensors,
            cfg->count,
            cfg->startupInhibitMs
        );

        // ============================================================
        // ZONE -> HMI STATUS AREA
        // ============================================================

        if (cfg->zones &&
            cfg->zoneCount > 0)
        {
            auto& zoneManager =
                ws.Zones();

            for (size_t i = 0;
                i < cfg->zoneCount;
                ++i)
            {
                const auto& zoneCfg =
                    cfg->zones[i];

                if (!zoneCfg.name)
                    continue;

                if (zoneCfg.statusArea < 0)
                    continue;

                if (!zoneManager.SetZoneStatusArea(
                        zoneCfg.name,
                        zoneCfg.statusArea))
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Zone status area not assigned: "
                        "zone=%s area=%d",
                        zoneCfg.name,
                        zoneCfg.statusArea
                    );
                }
            }
        }

        // --------------------------------------------------------
        // ALARM BITMASK MAP
        // --------------------------------------------------------

        if (!alarmMask.BuildMap(ws))
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Setup aborted: invalid alarm bitmask map"
            );

            initialized = false;
            return;
        }

       // --------------------------------------------------------
        // DEFAULT SECURITY STATE
        // --------------------------------------------------------
        //
        // SENSORI:
        //   ENABLE  = true
        //   ENGAGE  = non modificato
        //
        // ZONE:
        //   ENABLE  = true
        //   ENGAGE  = non modificato
        //
        // L'ENGAGE viene deciso successivamente
        // dallo stato della centrale.
        // --------------------------------------------------------

        alarmMask.SetEngage(true);

        // --------------------------------------------------------
        // SENSORI ABILITATI DI DEFAULT
        // --------------------------------------------------------

        for (size_t i = 0;
            i < ws.Count();
            ++i)
        {
            Sensor* sensor =
                ws.GetSensor(i);

            if (!sensor)
                continue;

            sensor->Enable(true);

            LOG_IF(
                "SecurityOrchestrator",
                "Setup: sensor[%u] ENABLED by default",
                (unsigned)i
            );
        }

        // --------------------------------------------------------
        // ZONE ABILITATE DI DEFAULT
        // --------------------------------------------------------

        ws.Zones().EnableAll(true);

        // NON fare EngageAll(true) qui.
        // L'engage delle zone viene deciso dalla centrale.

        initialized = true;

        lastChanges = CHANGE_NONE;
        lastLoopAt = 0;

        // --------------------------------------------------------
        // ALARM MASK CALLBACK
        // --------------------------------------------------------

        alarmMask.SetCallback(
            [](uint64_t newMask,
            uint64_t currentMask,
            uint64_t memMask,
            size_t bitIndex,
            size_t sensorIndex,
            SensorChannelType type)
            {
                LOG_DF(
                    "SecurityOrchestrator",
                    "NEW SIGNAL ALARM: "
                    "new=0x%llX current=0x%llX mem=0x%llX "
                    "bit=%u sensor=%u type=%u",
                    (unsigned long long)newMask,
                    (unsigned long long)currentMask,
                    (unsigned long long)memMask,
                    (unsigned)bitIndex,
                    (unsigned)sensorIndex,
                    (unsigned)type
                );
            }
        );

        LOG_IF(
            "SecurityOrchestrator",
            "Setup: sensors=%u "
            "zones=%u "
            "startupInhibit=%u ms",
            (unsigned)cfg->count,
            (unsigned)cfg->zoneCount,
            (unsigned)cfg->startupInhibitMs
        );
    }


    // ============================================================
    // LOOP
    //
    // Restituisce un bitmask che indica cosa è cambiato.
    //
    // CHANGE_SENSOR
    //     variazione rilevata da WiredSensorsManager::Process()
    //
    // CHANGE_ZONE
    //     variazione dello stato delle zone
    //
    // CHANGE_SYSTEM
    //     variazione del SystemManager
    // ============================================================

    static uint8_t Loop(
        unsigned long now)
    {
        if (!initialized)
        {
            lastChanges =
                CHANGE_NONE;

            return CHANGE_NONE;
        }

        uint8_t changes =
            CHANGE_NONE;

        // --------------------------------------------------------
        // WIRED SENSORS / READERS
        // --------------------------------------------------------

        const auto sensorResult =
            ws.Process(now);

        if (sensorResult.changed)
        {
            changes |=
                CHANGE_SENSOR;
        }

        if (sensorResult.zoneChanged)
        {
            changes |=
                CHANGE_ZONE;
        }

        // --------------------------------------------------------
        // ZONES / ALARMS
        // --------------------------------------------------------

        auto& zones =
            ws.Zones();

        if (zones.ProcessAllTypes())
        {
            changes |=
                CHANGE_SENSOR;

            changes |=
                CHANGE_ZONE;
        }

        // --------------------------------------------------------
        // SYSTEM AGGREGATE
        // --------------------------------------------------------

        system.ComputeFrom(ws);

        // --------------------------------------------------------
        // ALARM MASK
        //
        // Engage è configurato una sola volta in Setup().
        // --------------------------------------------------------

        alarmMask.ComputeCurrent(ws);

        // --------------------------------------------------------
        // SYSTEM CHANGE
        // --------------------------------------------------------

        if (system.hasChanged())
        {
            changes |=
                CHANGE_SYSTEM;

            changes |=
                CHANGE_ZONE;
        }

        // --------------------------------------------------------
        // SAVE LOOP STATE
        // --------------------------------------------------------

        lastChanges =
            changes;

        lastLoopAt =
            now;

        return changes;
    }


    // ============================================================
    // COMMANDS
    // ============================================================

    // ------------------------------------------------------------
    // ALARM PANEL
    //
    // Implementazione lasciata dopo la definizione completa
    // di DomoManagerAlarmPanel.
    // ------------------------------------------------------------

    static bool ApplyPanelCommand(
        int area,
        long value);


    // ------------------------------------------------------------
    // SINGLE SENSOR
    //
    // value:
    //
    //   bit 0 = Enable
    //   bit 1 = Engage
    //
    // Restituisce true se l'area appartiene effettivamente
    // ad un comando sensore e il comando è stato applicato.
    // ------------------------------------------------------------

    static bool ApplySecurityCommand(
        int area,
        long value)
    {
        if (!initialized)
            return false;


        auto* dm =
            DomoManager::instance;


        if (!dm)
            return false;


        auto& buffer =
            dm->getBuffer();


        const auto* cfg =
            ws.GetConfig();


        if (!cfg)
            return false;


        if (area < 0 ||
            area >= buffer.size())
        {
            return false;
        }


        for (size_t i = 0;
             i < ws.Count();
             ++i)
        {
            const auto& c =
                cfg[i];


            // ----------------------------------------------------
            // NOT THIS SENSOR
            // ----------------------------------------------------

            if (c.cmdArea != area)
                continue;


            Sensor* s =
                ws.GetSensor(i);


            if (!s)
                return false;


            // --------------------------------------------------------
            // COMMAND DECODE
            // --------------------------------------------------------

            const bool enable =
                bitRead(value, 0);

            const bool engageRT =
                bitRead(value, 1);

            const bool engageH24 =
                bitRead(value, 2);


            // --------------------------------------------------------
            // APPLY
            // --------------------------------------------------------

            s->Enable(enable);

            s->EngageRT(engageRT);
            s->EngageH24(engageH24);


            LOG_IF(
                "SecurityOrchestrator",
                "ApplySecurityCommand: "
                "sensor[%u] zone=%s "
                "enable=%d engageRT=%d engageH24=%d area=%d",
                (unsigned)i,
                c.zone ? c.zone : "?",
                enable ? 1 : 0,
                engageRT ? 1 : 0,
                engageH24 ? 1 : 0,
                area
            );


            return true;
        }


        return false;
    }


    // ------------------------------------------------------------
    // FORCE SECURITY COMMANDS
    // ------------------------------------------------------------

    static void ForceSecurityCommands(
        AlarmPanelInterface::ArmState state,
        unsigned long now)
    {
        auto& zoneManager =
            ws.Zones();

        bool engageRT = false;
        bool engageH24 = false;

        switch (state)
        {
            case AlarmPanelInterface::ArmState::ARMED_AWAY:
                engageRT = true;
                engageH24 = true;
                break;

            case AlarmPanelInterface::ArmState::ARMED_STAY:
            case AlarmPanelInterface::ArmState::ARMED_NIGHT:
                engageRT = false;
                engageH24 = true;
                break;

            case AlarmPanelInterface::ArmState::DISARMED:
            default:
                engageRT = false;
                engageH24 = false;
                break;
        }

        // Zone abilitate = engaged.
        const bool zoneEngage =
            state !=
            AlarmPanelInterface::ArmState::DISARMED;

        for (auto& entry : zoneManager.zones)
        {
            if (!entry.second.enabled)
            {
                entry.second.engaged = false;
                continue;
            }

            entry.second.engaged =
                zoneEngage;
        }

        const auto* cfg =
            ws.GetConfig();

        if (!cfg)
            return;

        auto* dm =
            DomoManager::instance;

        if (!dm)
            return;

        auto& buffer =
            dm->getBuffer();

        for (size_t i = 0;
            i < ws.Count();
            ++i)
        {
            Sensor* sensor =
                ws.GetSensor(i);

            if (!sensor)
                continue;

            if (!sensor->IsEnabled())
            {
                sensor->EngageRT(false);
                sensor->EngageH24(false);
                continue;
            }

            sensor->EngageRT(
                engageRT
            );

            sensor->EngageH24(
                engageH24
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Sensor[%u] zone=%s RT_ENGAGE=%d H24_ENGAGE=%d",
                (unsigned)i,
                cfg[i].zone
                    ? cfg[i].zone
                    : "<none>",
                engageRT ? 1 : 0,
                engageH24 ? 1 : 0
            );

            // Se vuoi mantenere sincronizzato il comando HMI,
            // il prossimo passo è definire i due bit di engage
            // nella cmdArea del sensore.
        }

        LOG_IF(
            "SecurityOrchestrator",
            "Security arm state=%u RT_ENGAGE=%d H24_ENGAGE=%d",
            (unsigned)state,
            engageRT ? 1 : 0,
            engageH24 ? 1 : 0
        );

        (void)now;
    }



    // ============================================================
    // DIAGNOSTICA
    // ============================================================

    class Diagnostic
    {
    private:

        // --------------------------------------------------------
        // HEADER
        // --------------------------------------------------------

        static void Header(
            const char* title)
        {
            LOG_IF(
                "SecurityOrchestrator",
                "================================================"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "%s",
                title
            );

            LOG_IF(
                "SecurityOrchestrator",
                "================================================"
            );
        }


        // --------------------------------------------------------
        // FOOTER
        // --------------------------------------------------------

        static void Footer()
        {
            LOG_IF(
                "SecurityOrchestrator",
                "================================================"
            );
        }


    public:

        // ========================================================
        // CONFIG
        // ========================================================

        static void ReportConfig()
        {
            // ============================================================
            // HEADER
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "==============================================="
            );

            LOG_IF(
                "SecurityOrchestrator",
                "      SECURITY ORCHESTRATOR CONFIG REPORT"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "==============================================="
            );


            // ============================================================
            // INITIALIZATION
            // ============================================================

            if (!SecurityOrchestrator::isInitialized())
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Initialized        : NO"
                );

                LOG_IF(
                    "SecurityOrchestrator",
                    "==============================================="
                );

                return;
            }


            // ============================================================
            // REFERENCES
            // ============================================================

            const auto& securityCfg =
                SecurityOrchestrator::cfgCopy;

            const auto& ws =
                SecurityOrchestrator::getWiredSensors();

            const auto* sensorCfg =
                ws.GetConfig();


            // ============================================================
            // AREA COUNTERS
            // ============================================================

            size_t commandAreaCount = 0;
            size_t sensorStatusAreaCount = 0;


            if (sensorCfg)
            {
                for (size_t i = 0;
                    i < ws.Count();
                    ++i)
                {
                    const auto& sensor =
                        sensorCfg[i];

                    if (sensor.cmdArea >= 0)
                        ++commandAreaCount;

                    if (sensor.statusArea >= 0)
                        ++sensorStatusAreaCount;
                }
            }


            // ============================================================
            // SECURITY GLOBAL CONFIGURATION
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "-----------------------------------------------"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "SECURITY GLOBAL CONFIGURATION"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Enabled            : %s",
                securityCfg.enabled
                    ? "YES"
                    : "NO"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Interval            : %lu ms",
                (unsigned long)securityCfg.intervalMs
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Report on change    : %s",
                securityCfg.reportOnChange
                    ? "YES"
                    : "NO"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Startup inhibit     : %lu ms",
                (unsigned long)securityCfg.startupInhibitMs
            );

            LOG_IF(
                "SecurityOrchestrator",
                "System status area  : %d",
                securityCfg.statusArea
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Event area          : %d",
                securityCfg.eventArea
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Panel command area  : %d",
                securityCfg.panelCommandArea
            );


            // ============================================================
            // WIRED SENSOR CONFIGURATION
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "-----------------------------------------------"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "WIRED SENSORS"
            );


            if (!sensorCfg)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensor configuration: NULL"
                );
            }
            else
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensors configured  : %u",
                    (unsigned)ws.Count()
                );


                for (size_t i = 0;
                    i < ws.Count();
                    ++i)
                {
                    const auto& sensor =
                        sensorCfg[i];


                    Sensor* runtimeSensor =
                        ws.GetSensor(i);


                    // ----------------------------------------------------
                    // SENSOR BASIC CONFIGURATION
                    // ----------------------------------------------------

                    LOG_IF(
                        "SecurityOrchestrator",
                        "Sensor[%u] | name=%s | zone=%s | category=%u",
                        (unsigned)i,
                        sensor.name
                            ? sensor.name
                            : "<null>",
                        sensor.zone
                            ? sensor.zone
                            : "<null>",
                        (unsigned)sensor.category
                    );


                    // ----------------------------------------------------
                    // RUNTIME SENSOR STATE
                    // ----------------------------------------------------

                    if (runtimeSensor)
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "  runtime: ENABLED=%s | ENGAGE_RT=%s | ENGAGE_H24=%s | ALARM=%s",
                            runtimeSensor->IsEnabled()
                                ? "YES"
                                : "NO",

                            runtimeSensor->IsEngagedRT()
                                ? "YES"
                                : "NO",

                            runtimeSensor->IsEngagedH24()
                                ? "YES"
                                : "NO",

                            (runtimeSensor->Outputs().rt ||
                            runtimeSensor->Outputs().h24)
                                ? "YES"
                                : "NO"
                        );
                    }
                    else
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "  runtime: INVALID SENSOR"
                        );
                    }


                    // ----------------------------------------------------
                    // AREAS
                    // ----------------------------------------------------

                    LOG_IF(
                        "SecurityOrchestrator",
                        "  cmdArea=%d | statusArea=%d",
                        sensor.cmdArea,
                        sensor.statusArea
                    );


                    // ----------------------------------------------------
                    // CHANNELS / READERS
                    // ----------------------------------------------------

                    LOG_IF(
                        "SecurityOrchestrator",
                        "  channels=%u | readers=%u",
                        (unsigned)sensor.channels.size(),
                        (unsigned)sensor.readers.size()
                    );


                    size_t channelIndex = 0;

                    for (const auto& channel :
                        sensor.channels)
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "    channel[%u] type=%u pin=%d",
                            (unsigned)channelIndex,
                            (unsigned)channel.type,
                            channel.pin
                        );

                        ++channelIndex;
                    }


                    size_t readerIndex = 0;

                    for (const auto& reader :
                        sensor.readers)
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "    reader[%u] %s",
                            (unsigned)readerIndex,
                            reader
                                ? "configured"
                                : "NULL"
                        );

                        ++readerIndex;
                    }
                }
            }


            // ============================================================
            // ZONE CONFIGURATION
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "-----------------------------------------------"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "ZONES"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Zones configured   : %u",
                (unsigned)securityCfg.zoneCount
            );


            if (securityCfg.zoneCount == 0)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "  No explicit zone configuration"
                );
            }
            else if (!securityCfg.zones)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "  ERROR: zone configuration is NULL"
                );
            }
            else
            {
                const auto& zones =
                    ws.Zones();


                for (size_t i = 0;
                    i < securityCfg.zoneCount;
                    ++i)
                {
                    const auto& zoneCfg =
                        securityCfg.zones[i];


                    if (!zoneCfg.name)
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "Zone[%u] | name=NULL | statusArea=%d",
                            (unsigned)i,
                            zoneCfg.statusArea
                        );

                        continue;
                    }


                    const int zoneIndex =
                        ws.GetZoneIndex(
                            zoneCfg.name
                        );


                    const int runtimeStatusArea =
                        zones.GetZoneStatusArea(
                            zoneCfg.name
                        );


                    LOG_IF(
                        "SecurityOrchestrator",
                        "Zone[%u] | name=%s | "
                        "statusArea=%d | "
                        "runtimeArea=%d | "
                        "index=%d",
                        (unsigned)i,
                        zoneCfg.name,
                        zoneCfg.statusArea,
                        runtimeStatusArea,
                        zoneIndex
                    );
                }
            }


            // ============================================================
            // RUNTIME ZONE INDEX
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "-----------------------------------------------"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "RUNTIME ZONE INDEX"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Runtime zones      : %u",
                (unsigned)ws.GetZoneCount()
            );


            const auto& runtimeZones =
                ws.Zones();


            for (size_t i = 0;
                i < ws.GetZoneCount();
                ++i)
            {
                const char* zoneName =
                    ws.GetZoneName(i);


                if (!zoneName)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "Zone[%u] | name=NULL",
                        (unsigned)i
                    );

                    continue;
                }


                const int statusArea =
                    runtimeZones.GetZoneStatusArea(
                        zoneName
                    );


                const bool enabled =
                    runtimeZones.IsZoneEnabled(
                        zoneName
                    );


                const bool engaged =
                    runtimeZones.IsZoneEngaged(
                        zoneName
                    );


                LOG_IF(
                    "SecurityOrchestrator",
                    "Zone[%u] | "
                    "name=%s | "
                    "statusArea=%d | "
                    "ENABLED=%s | "
                    "ENGAGED=%s",
                    (unsigned)i,
                    zoneName,
                    statusArea,
                    enabled
                        ? "YES"
                        : "NO",
                    engaged
                        ? "YES"
                        : "NO"
                );
            }


            // ============================================================
            // SENSOR COMMAND / STATUS AREAS
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "-----------------------------------------------"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "SENSOR COMMAND / STATUS AREAS"
            );


            if (!sensorCfg)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensor configuration: NULL"
                );
            }
            else
            {
                for (size_t i = 0;
                    i < ws.Count();
                    ++i)
                {
                    const auto& sensor =
                        sensorCfg[i];


                    LOG_IF(
                        "SecurityOrchestrator",
                        "Sensor[%u] | %-24s | zone=%-10s | "
                        "cmdArea=%d | statusArea=%d",
                        (unsigned)i,
                        sensor.name
                            ? sensor.name
                            : "?",
                        sensor.zone
                            ? sensor.zone
                            : "?",
                        sensor.cmdArea,
                        sensor.statusArea
                    );
                }
            }

            // ============================================================
            // SUMMARY
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "-----------------------------------------------"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "SUMMARY"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Sensors             : %u",
                (unsigned)ws.Count()
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Runtime zones       : %u",
                (unsigned)ws.GetZoneCount()
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Configured zones    : %u",
                (unsigned)securityCfg.zoneCount
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Sensor cmd areas    : %u",
                (unsigned)commandAreaCount
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Sensor status areas : %u",
                (unsigned)sensorStatusAreaCount
            );

            LOG_IF(
                "SecurityOrchestrator",
                "System status area  : %d",
                securityCfg.statusArea
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Panel command area  : %d",
                securityCfg.panelCommandArea
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Event area          : %d",
                securityCfg.eventArea
            );


            // ============================================================
            // FOOTER
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "==============================================="
            );
        }


        // ========================================================
        // SENSORS
        // ========================================================

        static void ReportSensors()
        {
            Header(
                "           SECURITY ORCHESTRATOR SENSORS REPORT"
            );


            // ============================================================
            // NOT INITIALIZED
            // ============================================================

            if (!initialized)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Status : NOT INITIALIZED"
                );

                Footer();
                return;
            }


            // ============================================================
            // CONFIGURATION
            // ============================================================

            const auto* cfg =
                ws.GetConfig();


            if (!cfg)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "ERROR: sensor configuration unavailable"
                );

                Footer();
                return;
            }


            // ============================================================
            // COUNTERS
            // ============================================================

            size_t enabled     = 0;
            size_t disabled    = 0;

            size_t engageRT    = 0;
            size_t disengageRT = 0;

            size_t engageH24    = 0;
            size_t disengageH24 = 0;

            size_t active       = 0;
            size_t alarmRT      = 0;
            size_t alarmH24     = 0;

            size_t memRT        = 0;
            size_t memH24       = 0;


            // ============================================================
            // SENSORS
            // ============================================================

            for (size_t i = 0;
                i < ws.Count();
                ++i)
            {
                const auto& c =
                    cfg[i];


                Sensor* s =
                    ws.GetSensor(i);


                // --------------------------------------------------------
                // INVALID SENSOR
                // --------------------------------------------------------

                if (!s)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "Sensor %u | INVALID SENSOR POINTER",
                        (unsigned)i
                    );

                    continue;
                }


                // --------------------------------------------------------
                // RUNTIME STATE
                // --------------------------------------------------------

                const bool isEnabled =
                    s->IsEnabled();

                const bool isEngagedRT =
                    s->IsEngagedRT();

                const bool isEngagedH24 =
                    s->IsEngagedH24();

                const bool isAlarmRT =
                    s->Outputs().rt;

                const bool isAlarmH24 =
                    s->Outputs().h24;

                const bool isAlarm =
                    isAlarmRT ||
                    isAlarmH24;

                const bool isMemRT =
                    s->Outputs().rtMem;

                const bool isMemH24 =
                    s->Outputs().h24Mem;


                // --------------------------------------------------------
                // COUNTERS
                // --------------------------------------------------------

                if (isEnabled)
                    ++enabled;
                else
                    ++disabled;


                if (isEngagedRT)
                    ++engageRT;
                else
                    ++disengageRT;


                if (isEngagedH24)
                    ++engageH24;
                else
                    ++disengageH24;


                if (isAlarm)
                    ++active;


                if (isAlarmRT)
                    ++alarmRT;


                if (isAlarmH24)
                    ++alarmH24;


                if (isMemRT)
                    ++memRT;


                if (isMemH24)
                    ++memH24;


                // --------------------------------------------------------
                // SENSOR SUMMARY
                // --------------------------------------------------------

                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensor %u | "
                    "name=%s | "
                    "zone=%s | "
                    "category=%d | "
                    "ENABLED=%s | "
                    "ENGAGE_RT=%s | "
                    "ENGAGE_H24=%s",
                    (unsigned)i,
                    c.name ? c.name : "?",
                    c.zone ? c.zone : "?",
                    static_cast<int>(c.category),
                    isEnabled
                        ? "YES"
                        : "NO",
                    isEngagedRT
                        ? "YES"
                        : "NO",
                    isEngagedH24
                        ? "YES"
                        : "NO"
                );


                // --------------------------------------------------------
                // OUTPUTS
                // --------------------------------------------------------

                LOG_IF(
                    "SecurityOrchestrator",
                    "  outputs: "
                    "RT=%s | "
                    "H24=%s | "
                    "ALARM=%s | "
                    "RT_MEM=%s | "
                    "H24_MEM=%s",
                    isAlarmRT
                        ? "YES"
                        : "NO",
                    isAlarmH24
                        ? "YES"
                        : "NO",
                    isAlarm
                        ? "YES"
                        : "NO",
                    isMemRT
                        ? "YES"
                        : "NO",
                    isMemH24
                        ? "YES"
                        : "NO"
                );


                // --------------------------------------------------------
                // AREAS
                // --------------------------------------------------------

                LOG_IF(
                    "SecurityOrchestrator",
                    "  cmdArea=%d | statusArea=%d",
                    c.cmdArea,
                    c.statusArea
                );


                // ========================================================
                // CHANNELS
                // ========================================================

                size_t channelIndex = 0;

                for (const auto& channel :
                    c.channels)
                {
                    const SensorChannelType type =
                        channel.type;


                    const SensorChannel* ch =
                        s->Get(type);


                    if (!ch)
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "  channel[%u] type=%u | INVALID",
                            (unsigned)channelIndex,
                            (unsigned)type
                        );

                        ++channelIndex;
                        continue;
                    }


                    LOG_IF(
                        "SecurityOrchestrator",
                        "  channel[%u] type=%u | "
                        "ACTIVE=%s | "
                        "ALARM=%s | "
                        "INHIBIT=%s",
                        (unsigned)channelIndex,
                        (unsigned)type,
                        ch->IsActive()
                            ? "YES"
                            : "NO",
                        s->ChannelAlarm(type)
                            ? "YES"
                            : "NO",
                        ch->IsInhibit()
                            ? "YES"
                            : "NO"
                    );


                    ++channelIndex;
                }
            }


            // ============================================================
            // SUMMARY
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "-----------------------------------------------"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Total sensors     : %u",
                (unsigned)ws.Count()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Enabled sensors   : %u",
                (unsigned)enabled
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Disabled sensors  : %u",
                (unsigned)disabled
            );


            LOG_IF(
                "SecurityOrchestrator",
                "ENGAGE RT         : %u",
                (unsigned)engageRT
            );


            LOG_IF(
                "SecurityOrchestrator",
                "DISENGAGE RT      : %u",
                (unsigned)disengageRT
            );


            LOG_IF(
                "SecurityOrchestrator",
                "ENGAGE H24        : %u",
                (unsigned)engageH24
            );


            LOG_IF(
                "SecurityOrchestrator",
                "DISENGAGE H24     : %u",
                (unsigned)disengageH24
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Active sensors    : %u",
                (unsigned)active
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Alarm RT          : %u",
                (unsigned)alarmRT
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Alarm H24         : %u",
                (unsigned)alarmH24
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Memory RT         : %u",
                (unsigned)memRT
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Memory H24        : %u",
                (unsigned)memH24
            );


            // ============================================================
            // FOOTER
            // ============================================================

            Footer();
        }


        // ========================================================
        // ZONES
        // ========================================================

        static void ReportZones()
        {
            Header(
                "            SECURITY ORCHESTRATOR ZONES REPORT"
            );


            if (!initialized)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Status : NOT INITIALIZED"
                );

                Footer();
                return;
            }


            auto& zones =
                ws.Zones();


            const auto& zoneMap =
                ws.GetZoneMap();


            size_t alarmCount =
                0;


            for (const auto& entry :
                 zoneMap)
            {
                const std::string& zoneName =
                    entry.first;


                const bool alarm =
                    zones.ZoneAlarm(zoneName);


                if (alarm)
                    ++alarmCount;


                LOG_IF(
                    "SecurityOrchestrator",
                    "%s -> %s",
                    zoneName.c_str(),
                    alarm ? "ALARM" : "OK"
                );
            }


            LOG_IF(
                "SecurityOrchestrator",
                "Zone count  : %u",
                (unsigned)zoneMap.size()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Alarm zones : %u",
                (unsigned)alarmCount
            );


            Footer();
        }


        // ========================================================
        // COMMANDS
        // ========================================================

        static void ReportCommands()
        {
            LOG_IF(
                "SecurityOrchestrator",
                "==============================================="
            );

            LOG_IF(
                "SecurityOrchestrator",
                "          SECURITY ORCHESTRATOR COMMANDS REPORT"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "==============================================="
            );

            if (!SecurityOrchestrator::isInitialized())
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Initialized        : NO"
                );

                LOG_IF(
                    "SecurityOrchestrator",
                    "==============================================="
                );

                return;
            }

            if (!DomoManager::instance)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "ERROR: DomoManager::instance is null"
                );

                LOG_IF(
                    "SecurityOrchestrator",
                    "==============================================="
                );

                return;
            }

            auto& manager =
                *DomoManager::instance;

            auto& buffer =
                manager.getBuffer();

            auto& ws =
                SecurityOrchestrator::getWiredSensors();

            const auto* sensorCfg =
                ws.GetConfig();

            if (!sensorCfg)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "ERROR: sensor configuration is NULL"
                );

                LOG_IF(
                    "SecurityOrchestrator",
                    "==============================================="
                );

                return;
            }

            // ============================================================
            // SENSOR COMMANDS
            // ============================================================

            size_t commandAreaCount = 0;

            for (size_t i = 0;
                i < ws.Count();
                ++i)
            {
                const auto& cfg =
                    sensorCfg[i];

                if (cfg.cmdArea < 0)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "Sensor %u | name=%s | zone=%s | cmdArea=DISABLED",
                        (unsigned)i,
                        cfg.name ? cfg.name : "<null>",
                        cfg.zone ? cfg.zone : "<null>"
                    );

                    continue;
                }

                ++commandAreaCount;

                const long value =
                    buffer.getValueFast(cfg.cmdArea);

                /*
                * Sensor command format:
                *
                * bit 0 = ENABLE
                * bit 1 = ENGAGE
                *
                * Other bits are currently reserved.
                */

                const bool enable =
                    (value & (1L << 0)) != 0;

                const bool engage =
                    (value & (1L << 1)) != 0;

                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensor %u | name=%s | zone=%s",
                    (unsigned)i,
                    cfg.name ? cfg.name : "<null>",
                    cfg.zone ? cfg.zone : "<null>"
                );

                LOG_IF(
                    "SecurityOrchestrator",
                    "  cmdArea=%d | value=0x%08lX",
                    cfg.cmdArea,
                    (unsigned long)value
                );

                LOG_IF(
                    "SecurityOrchestrator",
                    "  ENABLE=%u | ENGAGE=%u",
                    enable,
                    engage
                );

                // --------------------------------------------------------
                // Eventuali bit non utilizzati
                // --------------------------------------------------------

                const long reservedBits =
                    value & ~0x03L;

                if (reservedBits != 0)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "  WARNING: reserved command bits set: 0x%08lX",
                        (unsigned long)reservedBits
                    );
                }
            }

            // ============================================================
            // PANEL COMMAND
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "-----------------------------------------------"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "PANEL COMMAND"
            );

            const int panelCommandArea =
                SecurityOrchestrator::cfgCopy.panelCommandArea;

            if (panelCommandArea < 0)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "  panelCommandArea=DISABLED"
                );
            }
            else
            {
                const long value =
                    buffer.getValueFast(panelCommandArea);

                const uint8_t command =
                    static_cast<uint8_t>(value);

                LOG_IF(
                    "SecurityOrchestrator",
                    "  panelCommandArea=%d | value=0x%08lX | command=%u",
                    panelCommandArea,
                    (unsigned long)value,
                    (unsigned)command
                );

                switch (
                    static_cast<SecurityOrchestrator::AlarmPanelCommand>(
                        command))
                {
                    case SecurityOrchestrator::SILENCE_ALARM:
                        LOG_IF(
                            "SecurityOrchestrator",
                            "  decoded=SILENCE_ALARM"
                        );

                        break;

                    case SecurityOrchestrator::ARM_AWAY:
                        LOG_IF(
                            "SecurityOrchestrator",
                            "  decoded=ARM_AWAY"
                        );
                        break;

                    case SecurityOrchestrator::ARM_STAY:
                        LOG_IF(
                            "SecurityOrchestrator",
                            "  decoded=ARM_STAY"
                        );
                        break;

                    case SecurityOrchestrator::ARM_NIGHT:
                        LOG_IF(
                            "SecurityOrchestrator",
                            "  decoded=ARM_NIGHT"
                        );
                        break;

                    case SecurityOrchestrator::DISARM:
                        LOG_IF(
                            "SecurityOrchestrator",
                            "  decoded=DISARM"
                        );
                        break;

                    default:
                        LOG_IF(
                            "SecurityOrchestrator",
                            "  WARNING: unknown panel command=%u",
                            (unsigned)command
                        );
                        break;
                }

            }

            // ============================================================
            // SUMMARY
            // ============================================================

            LOG_IF(
                "SecurityOrchestrator",
                "-----------------------------------------------"
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Command areas      : %u",
                (unsigned)commandAreaCount
            );

            LOG_IF(
                "SecurityOrchestrator",
                "Panel command area : %d",
                panelCommandArea
            );

            LOG_IF(
                "SecurityOrchestrator",
                "==============================================="
            );
        }


        // ========================================================
        // SYSTEM
        // ========================================================

        static void ReportSystem()
        {
            Header(
                "           SECURITY ORCHESTRATOR SYSTEM REPORT"
            );


            if (!initialized)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Status : NOT INITIALIZED"
                );

                Footer();
                return;
            }


            system.Report();


            Footer();
        }


        // ========================================================
        // SECURITY
        //
        // Stato aggregato di sensors + zones + system.
        // ========================================================

        static void ReportSecurity()
        {
            Header(
                "         SECURITY ORCHESTRATOR SECURITY REPORT"
            );


            if (!initialized)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Status : NOT INITIALIZED"
                );

                Footer();
                return;
            }


            const auto* cfg =
                ws.GetConfig();


            size_t sensorCount =
                ws.Count();


            size_t activeSensors =
                0;


            size_t commandAreas =
                0;


            if (cfg)
            {
                for (size_t i = 0;
                     i < sensorCount;
                     ++i)
                {
                    Sensor* s =
                        ws.GetSensor(i);


                    if (s &&
                        (s->Outputs().rt ||
                        s->Outputs().h24))
                    {
                        ++activeSensors;
                    }


                    if (cfg[i].cmdArea >= 0)
                        ++commandAreas;
                }
            }


            auto& zones =
                ws.Zones();


            size_t activeZones =
                0;


            for (const auto& entry :
                 ws.GetZoneMap())
            {
                if (zones.ZoneAlarm(entry.first))
                    ++activeZones;
            }


            LOG_IF(
                "SecurityOrchestrator",
                "Initialized       : YES"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Sensors           : %u",
                (unsigned)sensorCount
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Active sensors    : %u",
                (unsigned)activeSensors
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Zones             : %u",
                (unsigned)ws.GetZoneMap().size()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Active zones      : %u",
                (unsigned)activeZones
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Command areas     : %u",
                (unsigned)commandAreas
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Intrusion         : %s",
                system.info.f[
                    SystemManager::ALARM_INTRUSION
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Intrusion H24     : %s",
                system.info.f[
                    SystemManager::ALARM_INTRUSION_H24
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Flood             : %s",
                system.info.f[
                    SystemManager::ALARM_FLOOD
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Smoke             : %s",
                system.info.f[
                    SystemManager::ALARM_SMOKE
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Windows open      : %s",
                system.info.f[
                    SystemManager::WINDOWS_OPEN
                ].get()
                    ? "YES"
                    : "NO"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Doors open        : %s",
                system.info.f[
                    SystemManager::DOORS_OPEN
                ].get()
                    ? "YES"
                    : "NO"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Tamper            : %s",
                system.info.f[
                    SystemManager::ALARM_TAMPER
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "System bitmask    : 0x%08X",
                (unsigned)system.getBitmask()
            );


            Footer();
        }


        // ========================================================
        // INCONSISTENCIES
        // ========================================================

        static void ReportInconsistencies()
        {
            Header(
                "      SECURITY ORCHESTRATOR INCONSISTENCIES REPORT"
            );


            bool found =
                false;


            // ----------------------------------------------------
            // NOT INITIALIZED
            // ----------------------------------------------------

            if (!initialized)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: SecurityOrchestrator not initialized"
                );

                found = true;
            }


            const auto* cfg =
                ws.GetConfig();


            // ----------------------------------------------------
            // CONFIG POINTER
            // ----------------------------------------------------

            if (initialized &&
                !cfg)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "ERROR: configuration pointer is NULL"
                );

                found = true;
            }


            if (initialized &&
                cfg)
            {
                // ------------------------------------------------
                // SENSOR POINTERS
                // ------------------------------------------------

                for (size_t i = 0;
                     i < ws.Count();
                     ++i)
                {
                    Sensor* s =
                        ws.GetSensor(i);


                    if (!s)
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "ERROR: sensor %u has NULL runtime object",
                            (unsigned)i
                        );

                        found = true;
                    }
                }


                // ------------------------------------------------
                // SENSOR CONFIGURATION
                // ------------------------------------------------

                for (size_t i = 0;
                     i < ws.Count();
                     ++i)
                {
                    const auto& c =
                        cfg[i];


                    if (!c.zone ||
                        strlen(c.zone) == 0)
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "ERROR: sensor %u has empty zone",
                            (unsigned)i
                        );

                        found = true;
                    }


                    if (c.channels.size() == 0)
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "ERROR: sensor %u zone=%s has no channels",
                            (unsigned)i,
                            c.zone ? c.zone : "?"
                        );

                        found = true;
                    }


                    if (c.readers.size() == 0)
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "ERROR: sensor %u zone=%s has no readers",
                            (unsigned)i,
                            c.zone ? c.zone : "?"
                        );

                        found = true;
                    }


                    if (c.channels.size() !=
                        c.readers.size())
                    {
                        LOG_IF(
                            "SecurityOrchestrator",
                            "ERROR: sensor %u zone=%s "
                            "channels=%u readers=%u",
                            (unsigned)i,
                            c.zone ? c.zone : "?",
                            (unsigned)c.channels.size(),
                            (unsigned)c.readers.size()
                        );

                        found = true;
                    }


                    for (const auto& ch :
                         c.channels)
                    {
                        if (ch.pin < -1)
                        {
                            LOG_IF(
                                "SecurityOrchestrator",
                                "ERROR: sensor %u zone=%s "
                                "invalid pin=%d",
                                (unsigned)i,
                                c.zone ? c.zone : "?",
                                ch.pin
                            );

                            found = true;
                        }
                    }
                }

                // ----------------------------------------------------
                // DUPLICATE STATUS AREAS
                // ----------------------------------------------------

                for (size_t i = 0;
                    i < ws.Count();
                    ++i)
                {
                    const int areaI =
                        cfg[i].statusArea;

                    if (areaI < 0)
                        continue;

                    for (size_t j = i + 1;
                        j < ws.Count();
                        ++j)
                    {
                        const int areaJ =
                            cfg[j].statusArea;

                        if (areaJ < 0)
                            continue;

                        if (areaI == areaJ)
                        {
                            LOG_IF(
                                "SecurityOrchestrator",
                                "WARNING: duplicate status area %d "
                                "used by sensors %u and %u",
                                areaI,
                                (unsigned)i,
                                (unsigned)j
                            );

                            found = true;
                        }
                    }
                }

                // ------------------------------------------------
                // DUPLICATE COMMAND AREAS
                // ------------------------------------------------

                for (size_t i = 0;
                     i < ws.Count();
                     ++i)
                {
                    const int areaI =
                        cfg[i].cmdArea;


                    if (areaI < 0)
                        continue;


                    for (size_t j = i + 1;
                         j < ws.Count();
                         ++j)
                    {
                        const int areaJ =
                            cfg[j].cmdArea;


                        if (areaJ < 0)
                            continue;


                        if (areaI == areaJ)
                        {
                            LOG_IF(
                                "SecurityOrchestrator",
                                "WARNING: duplicate command area %d "
                                "used by sensors %u and %u",
                                areaI,
                                (unsigned)i,
                                (unsigned)j
                            );

                            found = true;
                        }
                    }
                }
            }


            // ----------------------------------------------------
            // SYSTEM STATE
            // ----------------------------------------------------

            if (initialized)
            {
                const int bitmask =
                    system.getBitmask();


                if (bitmask != 0)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "INFO: active system state bitmask "
                        "0x%08X",
                        (unsigned)bitmask
                    );
                }
            }


            if (!found)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "No structural inconsistencies detected"
                );
            }


            Footer();
        }

        static void ReportAlarmBitmaskMap()
        {
            if (!initialized)
                return;

            alarmMask.ReportMap(ws);
        }

        // ========================================================
        // HMI
        // ========================================================

        static void ReportHmi(); //Solo dichiarazione

        // ========================================================
        // CORE
        //
        // Report compatto.
        // ========================================================

        static void CoreReport()
        {
            Header(
                "          SECURITY ORCHESTRATOR CORE REPORT"
            );


            if (!initialized)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Status            : NOT INITIALIZED"
                );

                Footer();
                return;
            }


            LOG_IF(
                "SecurityOrchestrator",
                "Initialized       : YES"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Sensors           : %u",
                (unsigned)ws.Count()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Zones             : %u",
                (unsigned)ws.GetZoneMap().size()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "System bitmask    : 0x%08X",
                (unsigned)system.getBitmask()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Tamper            : %s",
                system.info.f[
                    SystemManager::ALARM_TAMPER
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Intrusion         : %s",
                system.info.f[
                    SystemManager::ALARM_INTRUSION
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Intrusion H24     : %s",
                system.info.f[
                    SystemManager::ALARM_INTRUSION_H24
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Flood             : %s",
                system.info.f[
                    SystemManager::ALARM_FLOOD
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Smoke             : %s",
                system.info.f[
                    SystemManager::ALARM_SMOKE
                ].get()
                    ? "ACTIVE"
                    : "OK"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Windows open      : %s",
                system.info.f[
                    SystemManager::WINDOWS_OPEN
                ].get()
                    ? "YES"
                    : "NO"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Doors open        : %s",
                system.info.f[
                    SystemManager::DOORS_OPEN
                ].get()
                    ? "YES"
                    : "NO"
            );


            // ----------------------------------------------------
            // LAST LOOP
            // ----------------------------------------------------

            LOG_IF(
                "SecurityOrchestrator",
                "Last loop at      : %lu",
                lastLoopAt
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Last changes      : 0x%02X",
                (unsigned)lastChanges
            );


            LOG_IF(
                "SecurityOrchestrator",
                "  SENSOR          : %s",
                (lastChanges & CHANGE_SENSOR)
                    ? "YES"
                    : "NO"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "  ZONE            : %s",
                (lastChanges & CHANGE_ZONE)
                    ? "YES"
                    : "NO"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "  SYSTEM          : %s",
                (lastChanges & CHANGE_SYSTEM)
                    ? "YES"
                    : "NO"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "  PANEL           : %s",
                (lastChanges & CHANGE_PANEL)
                    ? "YES"
                    : "NO"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "  COMM            : %s",
                (lastChanges & CHANGE_COMM)
                    ? "YES"
                    : "NO"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "  COMMAND         : %s",
                (lastChanges & CHANGE_COMMAND)
                    ? "YES"
                    : "NO"
            );


            Footer();
        }


        // ========================================================
        // DIAGNOSTICS
        //
        // Dump completo del sottosistema SecurityOrchestrator.
        // ========================================================

        static void ReportDiagnostics()
        {
            Header(
                "        SECURITY ORCHESTRATOR DIAGNOSTICS"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Initialized       : %s",
                initialized
                    ? "YES"
                    : "NO"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Sensor count      : %u",
                (unsigned)ws.Count()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Zone count        : %u",
                (unsigned)ws.GetZoneMap().size()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "System bitmask    : 0x%08X",
                (unsigned)system.getBitmask()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Last loop at      : %lu",
                lastLoopAt
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Last changes      : 0x%02X",
                (unsigned)lastChanges
            );


            LOG_IF(
                "SecurityOrchestrator",
                "--- CONFIGURATION ---"
            );

            ReportConfig();


            LOG_IF(
                "SecurityOrchestrator",
                "--- SENSORS ---"
            );

            ReportSensors();


            LOG_IF(
                "SecurityOrchestrator",
                "--- ZONES ---"
            );

            ReportZones();


            LOG_IF(
                "SecurityOrchestrator",
                "--- COMMANDS ---"
            );

            ReportCommands();


            LOG_IF(
                "SecurityOrchestrator",
                "--- SYSTEM ---"
            );

            ReportSystem();


            LOG_IF(
                "SecurityOrchestrator",
                "--- SECURITY ---"
            );

            ReportSecurity();


            LOG_IF(
                "SecurityOrchestrator",
                "--- INCONSISTENCIES ---"
            );

            ReportInconsistencies();


            Footer();
        }


        // ========================================================
        // FULL REPORT
        // ========================================================

        static void FullReport()
        {
            Header(
                "        SECURITY ORCHESTRATOR FULL REPORT"
            );


            CoreReport();

            ReportConfig();

            ReportSensors();

            ReportZones();

            ReportCommands();

            ReportSystem();

            ReportSecurity();

            ReportInconsistencies();

            ReportAlarmBitmaskMap();

            ReportHmi();

            Footer();


            LOG_IF(
                "SecurityOrchestrator",
                "             END SECURITY REPORT"
            );


            LOG_IF(
                "SecurityOrchestrator",
                "================================================"
            );
        }
    };


    // ============================================================
    // UNIFIED REPORT DISPATCHER
    // ============================================================

    static void Report(
        ReportMode mode = ReportMode::FULL)
    {
        switch (mode)
        {
            case ReportMode::CORE:
                Diagnostic::CoreReport();
                break;


            case ReportMode::CONFIG:
                Diagnostic::ReportConfig();
                break;


            case ReportMode::SENSORS:
                Diagnostic::ReportSensors();
                break;


            case ReportMode::ZONES:
                Diagnostic::ReportZones();
                break;


            case ReportMode::COMMANDS:
                Diagnostic::ReportCommands();
                break;


            case ReportMode::SYSTEM:
                Diagnostic::ReportSystem();
                break;


            case ReportMode::SECURITY:
                Diagnostic::ReportSecurity();
                break;

            case ReportMode::HMI:
                Diagnostic::ReportHmi();
                break;

            case ReportMode::INCONSISTENCIES:
                Diagnostic::ReportInconsistencies();
                break;


            case ReportMode::DIAGNOSTICS:
                Diagnostic::ReportDiagnostics();
                break;


            case ReportMode::FULL:
            default:
                Diagnostic::FullReport();
                break;
        }
    }


    // ============================================================
    // CONVENIENCE REPORT API
    // ============================================================

    static void ReportAll()
    {
        Report(ReportMode::FULL);
    }


    static void ReportCoreOnly()
    {
        Report(ReportMode::CORE);
    }


    static void ReportConfigOnly()
    {
        Report(ReportMode::CONFIG);
    }


    static void ReportSensorsOnly()
    {
        Report(ReportMode::SENSORS);
    }


    static void ReportZonesOnly()
    {
        Report(ReportMode::ZONES);
    }


    static void ReportCommandsOnly()
    {
        Report(ReportMode::COMMANDS);
    }


    static void ReportSystemOnly()
    {
        Report(ReportMode::SYSTEM);
    }


    static void ReportSecurityOnly()
    {
        Report(ReportMode::SECURITY);
    }


    static void ReportInconsistenciesOnly()
    {
        Report(ReportMode::INCONSISTENCIES);
    }


    static void ReportDiagnosticsOnly()
    {
        Report(ReportMode::DIAGNOSTICS);
    }


    // ============================================================
    // ACCESSORS
    // ============================================================

    static SystemManager& getSystem()
    {
        return system;
    }


    static WiredSensorsManager& getWiredSensors()
    {
        return ws;
    }


    static bool isInitialized()
    {
        return initialized;
    }


    static uint8_t getLastChanges()
    {
        return lastChanges;
    }


    static unsigned long getLastLoopAt()
    {
        return lastLoopAt;
    }

    static int getPanelCommandArea()
    {
        return cfgCopy.panelCommandArea;
    }

    static const FrontendConfig::Security& getConfig()
    {
        return cfgCopy;
    }

    // ============================================================
    // CURRENT ALARM MASK
    //
    // Maschera degli allarmi attualmente presenti.
    //
    // La source of truth è AlarmBitmaskManager.
    // Non viene mantenuta una seconda activeAlarmMask.
    // ============================================================

    static uint64_t getCurrentAlarmMask()
    {
        return alarmMask.currentMask;
    }

    // ============================================================
    // CALLBACKS
    // ============================================================

    static void RegisterCallbackAny(
        AlarmDispatcher::Callback cb)
    {
        ws.Zones().dispatcher.OnAnyAlarm(cb);
    }


    static void RegisterCallbackType(
        SensorChannelType type,
        AlarmDispatcher::Callback cb)
    {
        ws.Zones().dispatcher.OnAlarmType(
            type,
            cb
        );
    }


    static void RegisterCallbackZone(
        const std::string& zone,
        AlarmDispatcher::Callback cb)
    {
        ws.Zones().dispatcher.OnZoneAlarm(
            zone,
            cb
        );
    }


    static void RegisterCallbackZoneType(
        const std::string& zone,
        SensorChannelType type,
        AlarmDispatcher::Callback cb)
    {
        ws.Zones().dispatcher.OnZoneAlarmType(
            zone,
            type,
            cb
        );
    }
};

inline void AlarmPanelInterface::ReportInconsistencies()
{
    LOG_IF(
        "SecurityOrchestrator",
        "==============================================="
    );

    LOG_IF(
        "SecurityOrchestrator",
        "      SECURITY ORCHESTRATOR INCONSISTENCIES REPORT"
    );

    LOG_IF(
        "SecurityOrchestrator",
        "==============================================="
    );

    bool inconsistent = false;

    if (!SecurityOrchestrator::isInitialized())
    {
        LOG_IF(
            "SecurityOrchestrator",
            "WARNING: SecurityOrchestrator is not initialized"
        );

        inconsistent = true;
    }

    const auto& ws =
        SecurityOrchestrator::getWiredSensors();

    const auto* sensorCfg =
        ws.GetConfig();

    const auto& securityCfg =
        SecurityOrchestrator::getConfig();

    // ============================================================
    // CONFIGURAZIONE SENSORI
    // ============================================================

    if (!sensorCfg)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "ERROR: sensor configuration is null"
        );

        inconsistent = true;
    }
    else
    {
        for (size_t i = 0; i < ws.Count(); ++i)
        {
            const auto& cfg =
                sensorCfg[i];

            // ----------------------------------------------------
            // Nome sensore
            // ----------------------------------------------------

            if (!cfg.name || !cfg.name[0])
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: sensor[%u] has invalid name",
                    (unsigned)i
                );

                inconsistent = true;
            }

            // ----------------------------------------------------
            // Zona
            // ----------------------------------------------------

            if (!cfg.zone || !cfg.zone[0])
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: sensor[%u] '%s' has no zone",
                    (unsigned)i,
                    cfg.name ? cfg.name : "<null>"
                );

                inconsistent = true;
            }
            else
            {
                const int zoneIndex =
                    ws.GetZoneIndex(cfg.zone);

                if (zoneIndex < 0)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: sensor[%u] '%s' references "
                        "unknown zone '%s'",
                        (unsigned)i,
                        cfg.name ? cfg.name : "<null>",
                        cfg.zone
                    );

                    inconsistent = true;
                }
            }

            // ----------------------------------------------------
            // Channels / readers
            // ----------------------------------------------------

            if (cfg.channels.size() != cfg.readers.size())
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: sensor[%u] '%s' channels=%u "
                    "readers=%u",
                    (unsigned)i,
                    cfg.name ? cfg.name : "<null>",
                    (unsigned)cfg.channels.size(),
                    (unsigned)cfg.readers.size()
                );

                inconsistent = true;
            }

            if (cfg.channels.size() == 0)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: sensor[%u] '%s' has no channels",
                    (unsigned)i,
                    cfg.name ? cfg.name : "<null>"
                );

                inconsistent = true;
            }

            // ----------------------------------------------------
            // Sensor command area
            // ----------------------------------------------------

            if (cfg.cmdArea < 0)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "INFO: sensor[%u] '%s' has no cmdArea",
                    (unsigned)i,
                    cfg.name ? cfg.name : "<null>"
                );
            }

            // ----------------------------------------------------
            // Sensor status area
            // ----------------------------------------------------

            if (cfg.statusArea < 0)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "INFO: sensor[%u] '%s' has no statusArea",
                    (unsigned)i,
                    cfg.name ? cfg.name : "<null>"
                );
            }
        }

        // ========================================================
        // DUPLICATE SENSOR COMMAND AREAS
        // ========================================================

        for (size_t i = 0; i < ws.Count(); ++i)
        {
            const int areaI =
                sensorCfg[i].cmdArea;

            if (areaI < 0)
                continue;

            for (size_t j = i + 1; j < ws.Count(); ++j)
            {
                const int areaJ =
                    sensorCfg[j].cmdArea;

                if (areaJ < 0)
                    continue;

                if (areaI == areaJ)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: duplicate sensor cmdArea=%d "
                        "used by sensors %u and %u",
                        areaI,
                        (unsigned)i,
                        (unsigned)j
                    );

                    inconsistent = true;
                }
            }
        }

        // ========================================================
        // DUPLICATE SENSOR STATUS AREAS
        // ========================================================

        for (size_t i = 0; i < ws.Count(); ++i)
        {
            const int areaI =
                sensorCfg[i].statusArea;

            if (areaI < 0)
                continue;

            for (size_t j = i + 1; j < ws.Count(); ++j)
            {
                const int areaJ =
                    sensorCfg[j].statusArea;

                if (areaJ < 0)
                    continue;

                if (areaI == areaJ)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: duplicate sensor statusArea=%d "
                        "used by sensors %u and %u",
                        areaI,
                        (unsigned)i,
                        (unsigned)j
                    );

                    inconsistent = true;
                }
            }
        }

        // ========================================================
        // SENSOR COMMAND / STATUS COLLISIONS
        // ========================================================

        for (size_t i = 0; i < ws.Count(); ++i)
        {
            const int cmdArea =
                sensorCfg[i].cmdArea;

            if (cmdArea < 0)
                continue;

            const int statusArea =
                sensorCfg[i].statusArea;

            if (statusArea >= 0 &&
                cmdArea == statusArea)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: sensor[%u] '%s' cmdArea=%d "
                    "collides with its statusArea",
                    (unsigned)i,
                    sensorCfg[i].name
                        ? sensorCfg[i].name
                        : "<null>",
                    cmdArea
                );

                inconsistent = true;
            }
        }

        // ========================================================
        // SENSOR AREA VS GLOBAL SECURITY AREAS
        // ========================================================

        for (size_t i = 0; i < ws.Count(); ++i)
        {
            const auto& cfg =
                sensorCfg[i];

            // ----------------------------------------------------
            // cmdArea
            // ----------------------------------------------------

            if (cfg.cmdArea >= 0)
            {
                if (securityCfg.statusArea >= 0 &&
                    cfg.cmdArea == securityCfg.statusArea)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: sensor[%u] '%s' cmdArea=%d "
                        "collides with security statusArea",
                        (unsigned)i,
                        cfg.name ? cfg.name : "<null>",
                        cfg.cmdArea
                    );

                    inconsistent = true;
                }

                if (securityCfg.eventArea >= 0 &&
                    cfg.cmdArea == securityCfg.eventArea)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: sensor[%u] '%s' cmdArea=%d "
                        "collides with security eventArea",
                        (unsigned)i,
                        cfg.name ? cfg.name : "<null>",
                        cfg.cmdArea
                    );

                    inconsistent = true;
                }

                if (securityCfg.panelCommandArea >= 0 &&
                    cfg.cmdArea == securityCfg.panelCommandArea)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: sensor[%u] '%s' cmdArea=%d "
                        "collides with panelCommandArea",
                        (unsigned)i,
                        cfg.name ? cfg.name : "<null>",
                        cfg.cmdArea
                    );

                    inconsistent = true;
                }
            }

            // ----------------------------------------------------
            // statusArea
            // ----------------------------------------------------

            if (cfg.statusArea >= 0)
            {
                if (securityCfg.statusArea >= 0 &&
                    cfg.statusArea == securityCfg.statusArea)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: sensor[%u] '%s' statusArea=%d "
                        "collides with security statusArea",
                        (unsigned)i,
                        cfg.name ? cfg.name : "<null>",
                        cfg.statusArea
                    );

                    inconsistent = true;
                }

                if (securityCfg.eventArea >= 0 &&
                    cfg.statusArea == securityCfg.eventArea)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: sensor[%u] '%s' statusArea=%d "
                        "collides with security eventArea",
                        (unsigned)i,
                        cfg.name ? cfg.name : "<null>",
                        cfg.statusArea
                    );

                    inconsistent = true;
                }

                if (securityCfg.panelCommandArea >= 0 &&
                    cfg.statusArea == securityCfg.panelCommandArea)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: sensor[%u] '%s' statusArea=%d "
                        "collides with panelCommandArea",
                        (unsigned)i,
                        cfg.name ? cfg.name : "<null>",
                        cfg.statusArea
                    );

                    inconsistent = true;
                }
            }
        }
    }

    // ============================================================
    // ZONE CONFIGURATION
    // ============================================================

    if (securityCfg.zoneCount > 0 &&
        !securityCfg.zones)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "WARNING: zoneCount=%u but zone configuration is null",
            (unsigned)securityCfg.zoneCount
        );

        inconsistent = true;
    }
    else
    {
        for (size_t i = 0;
            i < securityCfg.zoneCount;
            ++i)
        {
            const auto& zoneCfg =
                securityCfg.zones[i];

            if (!zoneCfg.name ||
                !zoneCfg.name[0])
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: configured zone[%u] has invalid name",
                    (unsigned)i
                );

                inconsistent = true;

                continue;
            }

            // ----------------------------------------------------
            // Zona configurata ma non presente nel manager
            // ----------------------------------------------------

            if (ws.GetZoneIndex(zoneCfg.name) < 0)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: configured zone '%s' "
                    "does not exist in ZoneManager",
                    zoneCfg.name
                );

                inconsistent = true;
            }

            // ----------------------------------------------------
            // statusArea
            // ----------------------------------------------------

            if (zoneCfg.statusArea < 0)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: zone '%s' has invalid statusArea=%d",
                    zoneCfg.name,
                    zoneCfg.statusArea
                );

                inconsistent = true;
            }
        }

        // ========================================================
        // DUPLICATE ZONE CONFIGURATION
        // ========================================================

        for (size_t i = 0;
            i < securityCfg.zoneCount;
            ++i)
        {
            const auto& zoneI =
                securityCfg.zones[i];

            if (!zoneI.name)
                continue;

            for (size_t j = i + 1;
                j < securityCfg.zoneCount;
                ++j)
            {
                const auto& zoneJ =
                    securityCfg.zones[j];

                if (!zoneJ.name)
                    continue;

                if (std::strcmp(
                        zoneI.name,
                        zoneJ.name) == 0)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: duplicate zone configuration '%s'",
                        zoneI.name
                    );

                    inconsistent = true;
                }

                if (zoneI.statusArea >= 0 &&
                    zoneI.statusArea == zoneJ.statusArea)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "WARNING: duplicate zone statusArea=%d "
                        "used by '%s' and '%s'",
                        zoneI.statusArea,
                        zoneI.name,
                        zoneJ.name
                    );

                    inconsistent = true;
                }
            }
        }

        // ========================================================
        // ZONE STATUS AREA VS GLOBAL AREAS
        // ========================================================

        for (size_t i = 0;
            i < securityCfg.zoneCount;
            ++i)
        {
            const auto& zoneCfg =
                securityCfg.zones[i];

            if (!zoneCfg.name ||
                zoneCfg.statusArea < 0)
            {
                continue;
            }

            if (securityCfg.statusArea >= 0 &&
                zoneCfg.statusArea == securityCfg.statusArea)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: zone '%s' statusArea=%d "
                    "collides with security statusArea",
                    zoneCfg.name,
                    zoneCfg.statusArea
                );

                inconsistent = true;
            }

            if (securityCfg.eventArea >= 0 &&
                zoneCfg.statusArea == securityCfg.eventArea)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: zone '%s' statusArea=%d "
                    "collides with security eventArea",
                    zoneCfg.name,
                    zoneCfg.statusArea
                );

                inconsistent = true;
            }

            if (securityCfg.panelCommandArea >= 0 &&
                zoneCfg.statusArea ==
                    securityCfg.panelCommandArea)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "WARNING: zone '%s' statusArea=%d "
                    "collides with panelCommandArea",
                    zoneCfg.name,
                    zoneCfg.statusArea
                );

                inconsistent = true;
            }
        }
    }

    // ============================================================
    // GLOBAL SECURITY AREAS
    // ============================================================

    if (securityCfg.statusArea >= 0 &&
        securityCfg.eventArea >= 0 &&
        securityCfg.statusArea == securityCfg.eventArea)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "WARNING: statusArea=%d collides with eventArea",
            securityCfg.statusArea
        );

        inconsistent = true;
    }

    if (securityCfg.statusArea >= 0 &&
        securityCfg.panelCommandArea >= 0 &&
        securityCfg.statusArea ==
            securityCfg.panelCommandArea)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "WARNING: statusArea=%d collides with "
            "panelCommandArea",
            securityCfg.statusArea
        );

        inconsistent = true;
    }

    if (securityCfg.eventArea >= 0 &&
        securityCfg.panelCommandArea >= 0 &&
        securityCfg.eventArea ==
            securityCfg.panelCommandArea)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "WARNING: eventArea=%d collides with "
            "panelCommandArea",
            securityCfg.eventArea
        );

        inconsistent = true;
    }

    // ============================================================
    // ZONE SHARING - VALID, NOT AN INCONSISTENCY
    // ============================================================

    /*
    * Più sensori nella stessa zona sono perfettamente validi.
    *
    * Esempio:
    *
    *   Cucina:
    *     - PIR Cucina
    *     - Porta Cucina
    *     - Allagamento Cucina
    *     - Fumo Cucina
    *
    * Non viene quindi più emesso alcun WARNING per
    * la condivisione della stessa zona.
    */

    // ============================================================
    // SYSTEM STATE
    // ============================================================

    const int systemMask =
        SecurityOrchestrator::
            getSystem()
            .getBitmask();

    LOG_IF(
        "SecurityOrchestrator",
        "INFO: active system state bitmask 0x%08X",
        (unsigned)systemMask
    );

    // ============================================================
    // RESULT
    // ============================================================

    if (!inconsistent)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "No inconsistencies detected"
        );
    }
    else
    {
        LOG_IF(
            "SecurityOrchestrator",
            "WARNING: one or more inconsistencies detected"
        );
    }

    LOG_IF(
        "SecurityOrchestrator",
        "==============================================="
    );
}

class DomoManagerAlarmPanel : public AlarmPanelInterface
{
private:
    // ============================================================
    // SILENCED ALARM MASK
    //
    // Bit degli allarmi attualmente attivi che sono già stati
    // tacitati.
    //
    // La maschera degli allarmi correnti NON è duplicata qui:
    // viene fornita da AlarmBitmaskManager.
    // ============================================================

    uint64_t silencedAlarmMask = 0;

    // ============================================================
    // COSTANTI
    // ============================================================

    static constexpr size_t EVENT_LOG_MAX = 128;

    static constexpr int DEFAULT_CHANNEL_LATENCY_MS = 5;


    // ============================================================
    // SENSOR CHANNEL -> ALARM TYPE
    // ============================================================
    //
    // RT
    //     Intrusione ordinaria
    //
    // H24
    //     Intrusione 24h
    //
    // LEN
    //     Trouble / supervisione linea
    //
    // MASK
    //     Masking
    //
    // ============================================================

    static AlarmType mapSensorAlarmType(
        SensorChannelType type)
    {
        switch (type)
        {
            case SensorChannelType::RT:
                return AlarmType::INTRUSION;

            case SensorChannelType::H24:
                return AlarmType::INTRUSION_H24;

            case SensorChannelType::LEN:
                return AlarmType::TROUBLE;

            case SensorChannelType::MASK:
                return AlarmType::MASKING;

            default:
                return AlarmType::CUSTOM;
        }
    }


    // ============================================================
    // SENSOR CHANNEL + STATE -> EVENT TYPE
    // ============================================================
    //
    // true  = attivazione
    // false = ripristino
    //
    // ============================================================

    static EventType mapSensorEventType(
        SensorChannelType type,
        bool active)
    {
        switch (type)
        {
            case SensorChannelType::RT:
            case SensorChannelType::H24:
                return active
                    ? EventType::ALARM_TRIGGERED
                    : EventType::ALARM_RESTORED;


            case SensorChannelType::LEN:
                return active
                    ? EventType::TROUBLE
                    : EventType::TROUBLE_RESTORED;


            case SensorChannelType::MASK:
                return active
                    ? EventType::MASKING
                    : EventType::MASKING_RESTORED;


            default:
                return active
                    ? EventType::ALARM_TRIGGERED
                    : EventType::ALARM_RESTORED;
        }
    }


    // ============================================================
    // EVENT DESCRIPTION
    // ============================================================

    static std::string buildSensorDescription(
        const std::string& zone,
        SensorChannelType type,
        bool active)
    {
        switch (type)
        {
            case SensorChannelType::RT:

                return active
                    ? "Intrusion alarm from zone " + zone
                    : "Intrusion alarm restored from zone " + zone;


            case SensorChannelType::H24:

                return active
                    ? "24h intrusion alarm from zone " + zone
                    : "24h intrusion alarm restored from zone " + zone;


            case SensorChannelType::LEN:

                return active
                    ? "Trouble from zone " + zone
                    : "Trouble restored from zone " + zone;


            case SensorChannelType::MASK:

                return active
                    ? "Masking detected on zone " + zone
                    : "Masking restored on zone " + zone;


            default:

                return active
                    ? "Alarm from zone " + zone
                    : "Alarm restored from zone " + zone;
        }
    }


    // ============================================================
    // PARTITION VALIDATION
    // ============================================================

    static bool validPartition(
        int partition)
    {
        return partition == 0;
    }

    // ============================================================
    // UPDATE SILENCED ALARM MASK
    //
    // Un allarme può rimanere tacitato SOLO finché è presente.
    //
    // Quando il relativo bit scompare da currentMask, viene
    // automaticamente rimosso da silencedAlarmMask.
    //
    // Questo permette di tacitare nuovamente l'allarme quando
    // successivamente ritorna.
    // ============================================================

    void updateSilencedAlarmMask(
        uint64_t currentMask)
    {
        silencedAlarmMask &=
            currentMask;
    }
public:

    // ============================================================
    // SINGLETON
    // ============================================================

    static DomoManagerAlarmPanel& instance()
    {
        static DomoManagerAlarmPanel inst;
        return inst;
    }


    // ============================================================
    // COSTRUTTORE
    // ============================================================

    DomoManagerAlarmPanel()
    {
        eventLog.reserve(EVENT_LOG_MAX);


        // ============================================================
        // SECURITY ORCHESTRATOR -> ALARM PANEL
        // ============================================================
        //
        // Il callback riceve:
        //
        //   zone     = zona
        //   type     = RT / H24 / LEN / MASK
        //   sensors  = sensori coinvolti
        //   active   = true  -> attivazione
        //              false -> ripristino
        //
        // ============================================================

        SecurityOrchestrator::RegisterCallbackAny(
            [this](
                const std::string& zone,
                SensorChannelType type,
                const std::vector<Sensor*>& sensors,
                bool active)
            {
                (void)sensors;

                // --------------------------------------------------------
                // TROUBLE PER ZONA
                // --------------------------------------------------------

                if (type == SensorChannelType::LEN)
                {
                    const int index =
                        zoneIndex(zone);


                    if (index >= 0)
                    {
                        if (active)
                        {
                            activeTroubleZones.insert(index);
                        }
                        else
                        {
                            activeTroubleZones.erase(index);
                        }
                    }
                }


                // --------------------------------------------------------
                // EVENT CREATION
                // --------------------------------------------------------

                Event ev;


                ev.category =
                    EventCategory::ALARM;


                ev.alarmType =
                    mapSensorAlarmType(type);


                ev.type =
                    mapSensorEventType(
                        type,
                        active
                    );


                ev.zone =
                    zoneIndex(zone);


                ev.partition =
                    0;


                ev.description =
                    buildSensorDescription(
                        zone,
                        type,
                        active
                    );


                ev.timestamp =
                    millis();


                // --------------------------------------------------------
                // DISPATCH
                // --------------------------------------------------------

                emitEvent(ev);
            }
        );
    }


    // ============================================================
    // IDENTIFICATION
    // ============================================================

    const char* getManufacturer() const override
    {
        return "DomoManager";
    }


    const char* getModel() const override
    {
        return "DomoManager Security Alarm Panel";
    }


    const char* getFirmwareVersion() const override
    {
        return "UNKNOWN";
    }


    const char* getSerialNumber() const override
    {
        return "UNKNOWN";
    }

    
    // ============================================================
    // CONNESSIONE / SUPERVISIONE
    // ============================================================

    bool connect() override
    {
        const bool wasFault =
            commFault;


        connected = true;

        commFault = false;

        lastPoll = millis();

        lastCommunicationOk =
            lastPoll;


        if (wasFault)
        {
            emitCommunicationRestored(
                lastPoll,
                "Communication restored on connect"
            );
        }


        LOG_IF(
            "DomoManagerAlarmPanel",
            "AlarmPanel::connect() "
            "connected=%d",
            connected ? 1 : 0
        );


        return true;
    }


    void disconnect() override
    {
        const unsigned long now =
            millis();


        connected = false;


        if (!commFault)
        {
            commFault = true;

            emitCommunicationFault(
                now,
                "Communication disconnected"
            );
        }
    }


    bool isConnected() const override
    {
        return connected;
    }


    // ============================================================
    // COMMUNICATION SUCCESS
    //
    // Da chiamare dal driver reale quando riceve correttamente
    // una risposta dalla centrale.
    // ============================================================

    void notifyCommunicationSuccess(
        unsigned long now,
        int latencyMs = DEFAULT_CHANNEL_LATENCY_MS)
    {
        const bool wasFault =
            commFault;


        connected = true;

        commFault = false;


        if (latencyMs >= 0)
        {
            channelLatencyMs =
                latencyMs;
        }


        lastCommunicationOk =
            now;


        if (wasFault)
        {
            emitCommunicationRestored(
                now,
                "Communication restored"
            );
        }
    }


    // ============================================================
    // COMMUNICATION FAULT
    //
    // Da chiamare dal driver reale quando fallisce la supervisione.
    // ============================================================

    void notifyCommunicationFault(
        unsigned long now,
        const char* description =
            "Communication fault")
    {
        if (!commFault)
        {
            commFault = true;


            emitCommunicationFault(
                now,
                description
            );
        }
    }


    bool isCommunicationFault() const override
    {
        return commFault;
    }


    bool isChannelSupervised() const override
    {
        return true;
    }


    int getChannelLatencyMs() const override
    {
        return channelLatencyMs;
    }


    bool hasDualPath() const override
    {
        return false;
    }


    bool getPathStatus(
        int pathIndex) const override
    {
        (void)pathIndex;
        return false;
    }


    // ============================================================
    // POLL
    //
    // Esegue il sottosistema SecurityOrchestrator.
    //
    // Dopo il Loop aggiorna la relazione:
    //
    //     currentMask XOR silencedAlarmMask
    //
    // ============================================================

    bool poll(
        unsigned long now) override
    {
        LOG_IF(
            "SECURITY",
            "poll BEGIN now=%lu",
            now
        );


        lastPoll = now;


        LOG_IF(
            "SECURITY",
            "poll BEFORE SecurityOrchestrator::Loop"
        );


        const uint8_t changes =
            SecurityOrchestrator::Loop(now);


        LOG_IF(
            "SECURITY",
            "poll AFTER SecurityOrchestrator::Loop changes=%u",
            (unsigned)changes
        );


        lastChanges =
            changes;


        LOG_IF(
            "SECURITY",
            "poll AFTER lastChanges=%u",
            (unsigned)lastChanges
        );


        // ========================================================
        // CURRENT ALARM MASK
        // ========================================================
        //
        // AlarmBitmaskManager è la source of truth.
        // ========================================================

        const uint64_t currentMask =
            SecurityOrchestrator::getCurrentAlarmMask();


        // ========================================================
        // CLEAN STALE SILENCED BITS
        // ========================================================

        updateSilencedAlarmMask(
            currentMask
        );


        // ========================================================
        // EFFECTIVE ALARM MASK
        // ========================================================
        //
        // Sono gli allarmi correnti NON tacitati.
        // ========================================================

        const uint64_t effectiveMask =
            currentMask ^
            silencedAlarmMask;


        LOG_IF(
            "SECURITY",
            "Alarm masks: "
            "current=0x%016llX "
            "silenced=0x%016llX "
            "effective=0x%016llX",
            (unsigned long long)currentMask,
            (unsigned long long)silencedAlarmMask,
            (unsigned long long)effectiveMask
        );


        // ========================================================
        // PANEL CHANGE
        // ========================================================

        if (changes !=
            SecurityOrchestrator::CHANGE_NONE)
        {
            LOG_IF(
                "SECURITY",
                "poll setting panelStateChanged=true"
            );

            panelStateChanged = true;
        }


        LOG_IF(
            "SECURITY",
            "poll BEFORE RETURN changes=%u panelStateChanged=%d",
            (unsigned)changes,
            panelStateChanged ? 1 : 0
        );


        return changes !=
            SecurityOrchestrator::CHANGE_NONE;
    }


    // ============================================================
    // STATO CENTRALE
    // ============================================================
    
    size_t getPartitionCount() const override
    {
        return 1;
    }


    ArmState getArmState(
        int partition = 0) const override
    {
        if (!validPartition(partition))
            return ArmState::UNKNOWN;


        return armState;
    }


    // ============================================================
    // READY
    //
    // READY operativo della centrale.
    //
    // Un allarme corrente già tacitato non impedisce il READY.
    //
    // currentMask
    //     = allarmi correnti
    //
    // silencedAlarmMask
    //     = allarmi correnti già tacitati
    //
    // effectiveMask
    //     = allarmi correnti NON tacitati
    // ============================================================

    bool isReady(
        int partition = 0) const override
    {
        if (!validPartition(partition))
            return false;


        const uint64_t currentMask =
            SecurityOrchestrator::getCurrentAlarmMask();


        const uint64_t validSilencedMask =
            silencedAlarmMask &
            currentMask;


        const uint64_t effectiveMask =
            currentMask ^
            validSilencedMask;


        return effectiveMask == 0;
    }


    // ============================================================
    // READY FOR ARM
    //
    // Determina se la centrale può essere inserita.
    //
    // NON utilizza la silencedAlarmMask.
    // NON utilizza le memorie di allarme.
    //
    // Una porta o finestra aperta impedisce l'inserimento.
    //
    // ============================================================

    bool isReadyForArm(
        int partition = 0) const
    {
        if (!validPartition(partition))
            return false;


        const auto st =
            SecurityOrchestrator::
                getWiredSensors()
                .ComputeAggregate();


        // --------------------------------------------------------
        // PORTE / FINESTRE APERTE
        //
        // Questi sono stati correnti, non memorie.
        // --------------------------------------------------------

        if (st.windowsOpen ||
            st.doorsOpen)
        {
            return false;
        }


        return true;
    }


    // ============================================================
    // ARM AWAY
    // ============================================================

    bool armAway(
        int partition = 0) override
    {
        if (!validPartition(partition))
            return false;


        if (!isReadyForArm(partition))
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "ARM AWAY rejected: partition %d not ready for arm",
                partition
            );

            return false;
        }


        // Nuovo ciclo di inserimento:
        // la tacitazione precedente non deve essere ereditata.

        silencedAlarmMask = 0;


        const ArmState state =
            ArmState::ARMED_AWAY;


        setArmState(
            state
        );


        SecurityOrchestrator::
            ForceSecurityCommands(
                state,
                millis()
            );


        emitPartitionChanged(
            partition,
            "ARM AWAY"
        );


        return true;
    }


    // ============================================================
    // ARM STAY
    // ============================================================

    bool armStay(
        int partition = 0) override
    {
        if (!validPartition(partition))
            return false;


        if (!isReadyForArm(partition))
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "ARM STAY rejected: partition %d not ready for arm",
                partition
            );

            return false;
        }


        // Nuovo ciclo di inserimento:
        // la tacitazione precedente non deve essere ereditata.

        silencedAlarmMask = 0;


        const ArmState state =
            ArmState::ARMED_STAY;


        setArmState(
            state
        );


        SecurityOrchestrator::
            ForceSecurityCommands(
                state,
                millis()
            );


        emitPartitionChanged(
            partition,
            "ARM STAY"
        );


        return true;
    }


    // ============================================================
    // ARM NIGHT
    // ============================================================

    bool armNight(
        int partition = 0) override
    {
        if (!validPartition(partition))
            return false;


        if (!isReadyForArm(partition))
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "ARM NIGHT rejected: partition %d not ready for arm",
                partition
            );

            return false;
        }


        // Nuovo ciclo di inserimento:
        // la tacitazione precedente non deve essere ereditata.

        silencedAlarmMask = 0;


        const ArmState state =
            ArmState::ARMED_NIGHT;


        setArmState(
            state
        );


        SecurityOrchestrator::
            ForceSecurityCommands(
                state,
                millis()
            );


        emitPartitionChanged(
            partition,
            "ARM NIGHT"
        );


        return true;
    }


    // ============================================================
    // DISARM
    // ============================================================
    //
    // DISARM normalmente richiede READY.
    //
    // Eccezione:
    // se la centrale è NOT READY ma l'allarme è stato
    // precedentemente tacitato, DISARM è consentito.
    //
    // La memoria dell'allarme NON viene cancellata.
    // ============================================================

    
    bool disarm(
        int partition = 0) override
    {
        if (!validPartition(partition))
            return false;


        const ArmState state =
            ArmState::DISARMED;


        setArmState(
            state
        );


        SecurityOrchestrator::
            ForceSecurityCommands(
                state,
                millis()
            );


        emitPartitionChanged(
            partition,
            "DISARM"
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "DISARM accepted: partition=%d",
            partition
        );


        return true;
    }

    // ============================================================
    // SILENCE ALARM
    //
    // Tacita gli allarmi attualmente attivi ma non ancora
    // tacitati.
    //
    // Inoltre cancella dalla memoria centrale SOLO i bit
    // corrispondenti agli allarmi tacitati.
    //
    // ============================================================

    bool silenceAlarm(
        int partition = 0)
    {
        if (!validPartition(partition))
            return false;


        // ========================================================
        // CURRENT ALARM MASK
        // ========================================================

        const uint64_t currentMask =
            SecurityOrchestrator::getCurrentAlarmMask();


        // ========================================================
        // CLEAN STALE SILENCED BITS
        // ========================================================

        updateSilencedAlarmMask(
            currentMask
        );


        // ========================================================
        // UNSILENCED ALARMS
        // ========================================================

        const uint64_t unsilencedMask =
            currentMask ^
            silencedAlarmMask;


        // ========================================================
        // NOTHING TO SILENCE
        // ========================================================

        if (unsilencedMask == 0)
        {
            LOG_IF(
                "DomoManagerAlarmPanel",
                "SILENCE ALARM ignored: "
                "no new unsilenced alarms "
                "current=0x%016llX "
                "silenced=0x%016llX",
                (unsigned long long)currentMask,
                (unsigned long long)silencedAlarmMask
            );

            return false;
        }


        // ========================================================
        // MARK ALARMS AS SILENCED
        // ========================================================

        silencedAlarmMask |=
            unsilencedMask;


        // ========================================================
        // RESET CENTRAL ALARM MEMORY
        //
        // IMPORTANT:
        // resettiamo SOLO i bit appena tacitati.
        // ========================================================

        SecurityOrchestrator::ResetAlarmMemory(
            unsilencedMask
        );


        // ========================================================
        // EFFECTIVE MASK
        // ========================================================

        const uint64_t effectiveMask =
            currentMask ^
            silencedAlarmMask;


        LOG_IF(
            "DomoManagerAlarmPanel",
            "SILENCE ALARM: "
            "current=0x%016llX "
            "silenced=0x%016llX "
            "memoryReset=0x%016llX "
            "effective=0x%016llX",
            (unsigned long long)currentMask,
            (unsigned long long)silencedAlarmMask,
            (unsigned long long)unsilencedMask,
            (unsigned long long)effectiveMask
        );


        return true;
    }

    // ============================================================
    // ZONE STATE
    // ============================================================

    bool getZoneState(
        int zone) const override
    {
        const auto& zoneMap =
            SecurityOrchestrator::
                getWiredSensors()
                .GetZoneMap();


        if (zone < 0 ||
            zone >= static_cast<int>(zoneMap.size()))
        {
            return false;
        }


        int index = 0;


        for (const auto& entry :
             zoneMap)
        {
            if (index == zone)
            {
                return SecurityOrchestrator::
                    getWiredSensors()
                    .Zones()
                    .ZoneAlarm(entry.first);
            }

            ++index;
        }


        return false;
    }


    // ============================================================
    // ZONE TAMPER
    //
    // Il backend attuale non espone una lettura per-zona del
    // tamper. Manteniamo quindi un set esplicito aggiornabile
    // dal driver tramite notifyZoneTamper().
    // ============================================================

    bool getZoneTamper(
        int zone) const override
    {
        if (!validZone(zone))
            return false;


        return activeTamperZones.count(zone) > 0;
    }


    // ============================================================
    // ZONE TROUBLE
    //
    // LEN viene mantenuto per zona dal callback SecurityOrchestrator.
    // ============================================================

    bool getZoneTrouble(
        int zone) const override
    {
        if (!validZone(zone))
            return false;


        return activeTroubleZones.count(zone) > 0;
    }


    bool isZoneBypassed(
        int zone) const override
    {
        if (!validZone(zone))
            return false;


        return bypassedZones.count(zone) > 0;
    }


    size_t getZoneCount() const override
    {
        return SecurityOrchestrator::
            getWiredSensors()
            .GetZoneMap()
            .size();
    }


    // ============================================================
    // ZONE TAMPER NOTIFICATION
    //
    // Hook per il layer che dispone del vero stato tamper
    // per-zona.
    // ============================================================

    void notifyZoneTamper(
        int zone,
        bool active,
        unsigned long now = 0)
    {
        if (!validZone(zone))
            return;


        if (active)
            activeTamperZones.insert(zone);
        else
            activeTamperZones.erase(zone);


        Event ev;

        ev.category =
            EventCategory::ALARM;


        ev.type =
            active
                ? EventType::TAMPER
                : EventType::TAMPER_RESTORED;


        ev.alarmType =
            AlarmType::TAMPER;


        ev.zone =
            zone;


        ev.partition =
            0;


        ev.description =
            active
                ? "Tamper active"
                : "Tamper restored";


        ev.timestamp =
            now != 0
                ? now
                : millis();


        emitEvent(ev);
    }


    // ============================================================
    // BYPASS
    // ============================================================

    bool bypassZone(
        int zone) override
    {
        if (!validZone(zone))
            return false;


        const auto result =
            bypassedZones.insert(zone);


        // Idempotente:
        // se già bypassata, operazione considerata riuscita,
        // ma non generiamo un nuovo evento.
        if (!result.second)
            return true;


        Event ev;

        ev.category =
            EventCategory::APPLICATION;


        ev.type =
            EventType::ZONE_BYPASSED;


        ev.zone =
            zone;


        ev.partition =
            0;


        ev.description =
            "Zone bypassed";


        ev.timestamp =
            millis();


        emitEvent(ev);


        return true;
    }


    bool clearBypass(
        int zone) override
    {
        if (!validZone(zone))
            return false;


        const size_t removed =
            bypassedZones.erase(zone);


        if (removed == 0)
            return false;


        Event ev;

        ev.category =
            EventCategory::APPLICATION;


        ev.type =
            EventType::ZONE_BYPASS_CLEARED;


        ev.zone =
            zone;


        ev.partition =
            0;


        ev.description =
            "Zone bypass cleared";


        ev.timestamp =
            millis();


        emitEvent(ev);


        return true;
    }


    // ============================================================
    // GLOBAL TAMPER
    // ============================================================

    bool getGlobalTamper() const override
    {
        return SecurityOrchestrator::
            getWiredSensors()
            .ComputeAggregate()
            .tamper;
    }


    // ============================================================
    // GLOBAL TROUBLE
    //
    // Derivato dai trouble LEN mantenuti per-zona.
    // ============================================================

    bool getGlobalTrouble() const override
    {
        return !activeTroubleZones.empty();
    }


    // ============================================================
    // AUTHENTICATION PROVIDER
    // ============================================================
    //
    // Nessun PIN hardcoded.
    //
    // Il backend può essere configurato dall'esterno.
    // ============================================================

    using AuthenticationProvider =
        std::function<bool(
            const std::string& user,
            const std::string& pin)>;


    using PermissionProvider =
        std::function<bool(
            const std::string& user,
            const std::string& action)>;


    void setAuthenticationProvider(
        AuthenticationProvider provider)
    {
        authenticationProvider =
            provider;
    }


    void setPermissionProvider(
        PermissionProvider provider)
    {
        permissionProvider =
            provider;
    }


    // ============================================================
    // AUTHENTICATE USER
    // ============================================================

    bool authenticateUser(
        const std::string& user,
        const std::string& pin) override
    {
        bool authenticated =
            false;


        if (authenticationProvider)
        {
            authenticated =
                authenticationProvider(
                    user,
                    pin
                );
        }
        else
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "Authentication provider not configured"
            );

            authenticated =
                false;
        }


        Event ev;

        ev.category =
            EventCategory::APPLICATION;


        ev.type =
            authenticated
                ? EventType::USER_AUTH_OK
                : EventType::USER_AUTH_FAIL;


        ev.zone = -1;

        ev.partition = -1;

        ev.user =
            user;


        ev.description =
            authenticated
                ? "Authentication successful"
                : "Authentication failed";


        ev.timestamp =
            millis();


        emitEvent(ev);


        return authenticated;
    }


    // ============================================================
    // PERMISSIONS
    // ============================================================

    bool hasPermission(
        const std::string& user,
        const std::string& action) const override
    {
        if (!permissionProvider)
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "Permission provider not configured"
            );

            return false;
        }


        return permissionProvider(
            user,
            action
        );
    }


    // ============================================================
    // EVENT CALLBACKS
    // ============================================================

    void setAlarmCallback(
        AlarmCallback cb) override
    {
        alarmCallback =
            cb;
    }


    void setEventCallback(
        EventCallback cb) override
    {
        callback =
            cb;
    }


    // ============================================================
    // EVENT LOG
    // ============================================================

    std::vector<Event> getEventLog() const override
    {
        return eventLog;
    }


    void clearEventLog() override
    {
        eventLog.clear();
    }


    size_t getEventCount() const
    {
        return eventLog.size();
    }


    // ============================================================
    // DIAGNOSTICA
    // ============================================================

    void diagnostic() const override
    {
        LOG_IF(
            "DomoManagerAlarmPanel",
            "================================================"
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "       DOMOMANAGER ALARM PANEL DIAGNOSTICS"
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "================================================"
        );


        // --------------------------------------------------------
        // PANEL
        // --------------------------------------------------------

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Connected            : %s",
            connected
                ? "YES"
                : "NO"
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Communication fault  : %s",
            commFault
                ? "ACTIVE"
                : "OK"
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Arm state            : %s",
            armStateToString(
                armState
            )
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Channel supervised   : %s",
            isChannelSupervised()
                ? "YES"
                : "NO"
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Channel latency      : %d ms",
            channelLatencyMs
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Last poll            : %lu",
            lastPoll
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Last communication OK: %lu",
            lastCommunicationOk
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Last changes         : 0x%02X",
            (unsigned)lastChanges
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Panel state changed  : %s",
            panelStateChanged
                ? "YES"
                : "NO"
        );


        // --------------------------------------------------------
        // ZONES
        // --------------------------------------------------------

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Zone count           : %u",
            (unsigned)getZoneCount()
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Bypassed zones       : %u",
            (unsigned)bypassedZones.size()
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Tamper zones         : %u",
            (unsigned)activeTamperZones.size()
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Trouble zones        : %u",
            (unsigned)activeTroubleZones.size()
        );


        // --------------------------------------------------------
        // SECURITY
        // --------------------------------------------------------

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Global tamper        : %s",
            getGlobalTamper()
                ? "ACTIVE"
                : "OK"
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Global trouble       : %s",
            getGlobalTrouble()
                ? "ACTIVE"
                : "OK"
        );


        LOG_IF(
            "DomoManagerAlarmPanel",
            "Ready                : %s",
            isReady(0)
                ? "YES"
                : "NO"
        );


        // --------------------------------------------------------
        // EVENT LOG
        // --------------------------------------------------------

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Event log count      : %u / %u",
            (unsigned)eventLog.size(),
            (unsigned)EVENT_LOG_MAX
        );


        // --------------------------------------------------------
        // SECURITY ORCHESTRATOR
        // --------------------------------------------------------

        SecurityOrchestrator::Report(
            SecurityOrchestrator::
                ReportMode::CORE
        );


        SecurityOrchestrator::Report(
            SecurityOrchestrator::
                ReportMode::SECURITY
        );


        SecurityOrchestrator::Report(
            SecurityOrchestrator::
                ReportMode::INCONSISTENCIES
        );

        LOG_IF(
            "DomoManagerAlarmPanel",
            "================================================"
        );
    }

    // ============================================================
    // SYSTEM BITMASK
    // ============================================================

    int getSystemBitmask() const override
    {
        return SecurityOrchestrator::
            getSystem()
            .getBitmask();
    }


    // ============================================================
    // LAST CHANGES
    // ============================================================

    uint8_t getLastChanges() const
    {
        return lastChanges;
    }


    void clearPanelStateChanged()
    {
        panelStateChanged = false;
    }

    // ============================================================
    // SILENCED ALARM MASK
    // ============================================================

    uint64_t getSilencedAlarmMask() const
    {
        return silencedAlarmMask;
    }


    // ============================================================
    // EFFECTIVE ALARM MASK
    //
    // current XOR silenced
    // ============================================================

    uint64_t getEffectiveAlarmMask() const
    {
        const uint64_t currentMask =
            SecurityOrchestrator::getCurrentAlarmMask();


        const uint64_t validSilencedMask =
            silencedAlarmMask &
            currentMask;


        return currentMask ^
            validSilencedMask;
    }
private:

    // ============================================================
    // RUNTIME STATE
    // ============================================================

    bool connected = false;

    bool commFault = false;


    // ------------------------------------------------------------
    // Arm state separato dallo stato allarme
    // ------------------------------------------------------------

    ArmState armState =
        ArmState::DISARMED;


    // ------------------------------------------------------------
    // Poll / communication
    // ------------------------------------------------------------

    unsigned long lastPoll = 0;

    unsigned long lastCommunicationOk = 0;


    int channelLatencyMs =
        DEFAULT_CHANNEL_LATENCY_MS;


    // ------------------------------------------------------------
    // Runtime changes
    // ------------------------------------------------------------

    uint8_t lastChanges =
        SecurityOrchestrator::CHANGE_NONE;


    bool panelStateChanged =
        false;


    // ============================================================
    // ZONE STATE
    // ============================================================

    std::set<int> bypassedZones;

    std::set<int> activeTamperZones;

    std::set<int> activeTroubleZones;


    // ============================================================
    // CALLBACKS
    // ============================================================

    AlarmCallback alarmCallback = nullptr;

    EventCallback callback = nullptr;


    // ============================================================
    // PROVIDERS
    // ============================================================

    AuthenticationProvider authenticationProvider =
        nullptr;


    PermissionProvider permissionProvider =
        nullptr;


    // ============================================================
    // EVENT LOG
    // ============================================================

    std::vector<Event> eventLog;


    // ============================================================
    // SET ARM STATE
    // ============================================================

    void setArmState(
        ArmState newState)
    {
        armState =
            newState;
    }


    // ============================================================
    // VALID ZONE
    // ============================================================

    bool validZone(
        int zone) const
    {
        return zone >= 0 &&
               zone <
                   static_cast<int>(
                       getZoneCount()
                   );
    }


    // ============================================================
    // ZONE NAME -> INDEX
    //
    // Mantiene la stessa convenzione utilizzata da getZoneState().
    //
    // La numerazione deriva dall'ordine della mappa del manager.
    // ============================================================

    int zoneIndex(
        const std::string& zoneName) const
    {
        int index = 0;


        const auto& zoneMap =
            SecurityOrchestrator::
                getWiredSensors()
                .GetZoneMap();


        for (const auto& entry :
             zoneMap)
        {
            if (entry.first ==
                zoneName)
            {
                return index;
            }


            ++index;
        }


        return -1;
    }


    // ============================================================
    // EVENT LOG APPEND
    //
    // Buffer limitato.
    //
    // Quando pieno, viene eliminato l'evento più vecchio.
    // ============================================================

    void appendEvent(
        const Event& ev)
    {
        if (eventLog.size() >=
            EVENT_LOG_MAX)
        {
            eventLog.erase(
                eventLog.begin()
            );
        }


        eventLog.push_back(ev);
    }


    // ============================================================
    // EVENT DISPATCH
    //
    // 1. salva nel log
    // 2. invia callback ALARM
    // 3. invia callback APPLICATION
    // ============================================================

    void emitEvent(
        const Event& ev)
    {
        appendEvent(ev);


        if (ev.category ==
            EventCategory::ALARM)
        {
            if (alarmCallback)
                alarmCallback(ev);

            return;
        }


        if (callback)
            callback(ev);
    }


    // ============================================================
    // COMMUNICATION FAULT EVENT
    // ============================================================

    void emitCommunicationFault(
        unsigned long now,
        const char* description)
    {
        Event ev;


        ev.category =
            EventCategory::ALARM;


        ev.type =
            EventType::COMM_FAULT;


        ev.alarmType =
            AlarmType::COMMUNICATION;


        ev.zone = -1;

        ev.partition = -1;


        ev.description =
            description
                ? description
                : "Communication fault";


        ev.timestamp =
            now;


        emitEvent(ev);
    }


    // ============================================================
    // COMMUNICATION RESTORED EVENT
    // ============================================================

    void emitCommunicationRestored(
        unsigned long now,
        const char* description)
    {
        Event ev;


        ev.category =
            EventCategory::ALARM;


        ev.type =
            EventType::COMM_RESTORED;


        ev.alarmType =
            AlarmType::COMMUNICATION;


        ev.zone = -1;

        ev.partition = -1;


        ev.description =
            description
                ? description
                : "Communication restored";


        ev.timestamp =
            now;


        emitEvent(ev);
    }


    // ============================================================
    // PARTITION EVENT
    // ============================================================

    void emitPartitionChanged(
        int partition,
        const char* description)
    {
        Event ev;

        ev.category =
            EventCategory::APPLICATION;


        ev.type =
            EventType::PARTITION_CHANGED;


        ev.zone = -1;

        ev.partition =
            partition;


        ev.description =
            description
                ? description
                : "Partition state changed";


        ev.timestamp =
            millis();


        emitEvent(ev);


        LOG_DF(
            "DomoManagerAlarmPanel",
            "Partition %d -> %s",
            partition,
            ev.description.c_str()
        );
    }


    // ============================================================
    // ARM STATE -> STRING
    // ============================================================

    static const char* armStateToString(
        ArmState state)
    {
        switch (state)
        {
            case ArmState::DISARMED:
                return "DISARMED";


            case ArmState::ARMED_STAY:
                return "ARMED_STAY";


            case ArmState::ARMED_AWAY:
                return "ARMED_AWAY";


            case ArmState::ARMED_NIGHT:
                return "ARMED_NIGHT";


            case ArmState::ARMED_PARTIAL:
                return "ARMED_PARTIAL";


            case ArmState::NOT_READY:
                return "NOT_READY";


            case ArmState::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }
};


// ============================================================
// REPORT DIAGNOSTICO ESTERNO
// ============================================================
//
// Questa funzione può essere utilizzata come entry point quando
// la classe completa è già disponibile.
//
// ============================================================

inline void ReportDomoManagerAlarmPanel()
{
    DomoManagerAlarmPanel::instance().diagnostic();
}


inline bool SecurityOrchestrator::ApplyPanelCommand(
    int area,
    long value)
{
    // ============================================================
    // SECURITY SUBSYSTEM INITIALIZED
    // ============================================================

    if (!initialized)
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: security not initialized"
        );

        return false;
    }

    // ============================================================
    // PANEL COMMAND AREA
    // ============================================================

    if (cfgCopy.panelCommandArea < 0)
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: panel command area disabled"
        );

        return false;
    }

    if (area != cfgCopy.panelCommandArea)
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: invalid area=%d expected=%d",
            area,
            cfgCopy.panelCommandArea
        );

        return false;
    }

    // ============================================================
    // VALIDATE COMMAND VALUE
    //
    // 0 = DISARM
    // 1 = ARM_AWAY
    // 2 = ARM_STAY
    // 3 = ARM_NIGHT
    // 4 = SILENCE_ALARM
    //
    // IMPORTANT:
    // validate BEFORE static_cast<AlarmPanelCommand>(value)
    // ============================================================

    if (value < static_cast<long>(DISARM) ||
        value > static_cast<long>(SILENCE_ALARM))
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: invalid command value=%ld area=%d",
            value,
            area
        );

        return false;
    }

    const AlarmPanelCommand command =
        static_cast<AlarmPanelCommand>(value);

    // ============================================================
    // ALARM PANEL
    // ============================================================

    DomoManagerAlarmPanel& panel =
        DomoManagerAlarmPanel::instance();

    bool result = false;

    // ============================================================
    // APPLY COMMAND
    // ============================================================

    switch (command)
    {
        // --------------------------------------------------------
        // DISARM
        // --------------------------------------------------------

        case DISARM:
        {
            result = panel.disarm(0);
            break;
        }

        // --------------------------------------------------------
        // ARM AWAY
        // --------------------------------------------------------

        case ARM_AWAY:
        {
            result = panel.armAway(0);
            break;
        }

        // --------------------------------------------------------
        // ARM STAY
        // --------------------------------------------------------

        case ARM_STAY:
        {
            result = panel.armStay(0);
            break;
        }

        // --------------------------------------------------------
        // ARM NIGHT
        // --------------------------------------------------------

        case ARM_NIGHT:
        {
            result = panel.armNight(0);
            break;
        }

        // --------------------------------------------------------
        // SILENCE CURRENT ALARM
        // --------------------------------------------------------

        case SILENCE_ALARM:
        {
            result = panel.silenceAlarm(0);
            break;
        }

        // --------------------------------------------------------
        // SHOULD NEVER HAPPEN
        // --------------------------------------------------------

        default:
        {
            LOG_EF(
                "SecurityOrchestrator",
                "ApplyPanelCommand: unsupported command=%ld area=%d",
                value,
                area
            );

            return false;
        }
    }

    // ============================================================
    // RESULT
    // ============================================================

    if (result)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: area=%d value=%ld command=%u result=ACCEPTED",
            area,
            value,
            static_cast<unsigned>(command)
        );
    }
    else
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: area=%d value=%ld command=%u result=FAILED",
            area,
            value,
            static_cast<unsigned>(command)
        );
    }

    return result;
}


class SecurityHmiInterface
{
public:

    // ============================================================
    // SENSOR HMI STATE
    // ============================================================

    struct SensorState
    {
        bool valid = false;

        size_t index = 0;

        const char* name = nullptr;
        const char* zone = nullptr;

        SensorCategory category{};

        int cmdArea = -1;

        bool enabled = false;

        bool engagedRT = false;
        bool engagedH24 = false;

        bool active[4] = {};
        bool alarm[4] = {};
        bool inhibit[4] = {};

        bool alarmOut = false;

        bool rtMem = false;
        bool h24Mem = false;
    };


    // ============================================================
    // ZONE HMI STATE
    // ============================================================

    struct ZoneState
    {
        bool valid = false;

        size_t index = 0;

        const char* name = nullptr;

        bool alarm = false;
        bool alarmH24 = false;
        bool trouble = false;
        bool bypassed = false;

        bool rtMem = false;
        bool h24Mem = false;
    };


    // ============================================================
    // PANEL HMI STATE
    // ============================================================

    struct PanelState
    {
        bool valid = false;

        bool connected = false;
        bool communicationFault = false;

        bool channelSupervised = false;
        int channelLatencyMs = 0;

        bool ready = false;
        bool readyForArm = false;

        AlarmPanelInterface::ArmState armState =
            AlarmPanelInterface::ArmState::UNKNOWN;

        bool globalTamper = false;
        bool globalTrouble = false;

        uint32_t systemBitmask = 0;

        uint8_t changeFlags =
            SecurityOrchestrator::CHANGE_NONE;

        int panelCommandArea = -1;

        size_t partitionCount = 0;
        size_t zoneCount = 0;
        size_t sensorCount = 0;
    };


    // ============================================================
    // PANEL COMMAND RESULT
    // ============================================================

    struct PanelCommandResult
    {
        bool accepted = false;

        AlarmPanelInterface::ArmState state =
            AlarmPanelInterface::ArmState::UNKNOWN;

        bool ready = false;

        bool communicationFault = false;
    };


    // ============================================================
    // SINGLETON
    // ============================================================

    static SecurityHmiInterface& instance()
    {
        static SecurityHmiInterface inst;
        return inst;
    }


    // ============================================================
    // STATUS
    // ============================================================

    bool isInitialized() const
    {
        return SecurityOrchestrator::isInitialized();
    }


    // ============================================================
    // SENSOR COUNT
    // ============================================================

    size_t sensorCount() const
    {
        auto& ws =
            SecurityOrchestrator::getWiredSensors();

        return ws.Count();
    }


    // ============================================================
    // SENSOR STATE
    // ============================================================

    bool getSensorState(
        size_t sensorIndex,
        SensorState& out) const
    {
        auto& ws =
            SecurityOrchestrator::getWiredSensors();

        if (sensorIndex >= ws.Count())
            return false;


        const auto* cfg =
            ws.GetConfig();

        if (!cfg)
            return false;


        Sensor* sensor =
            ws.GetSensor(sensorIndex);

        if (!sensor)
            return false;


        const auto& c =
            cfg[sensorIndex];


        out = SensorState{};

        out.valid =
            true;

        out.index =
            sensorIndex;

        out.name =
            c.name;

        out.zone =
            c.zone;

        out.category =
            c.category;

        out.cmdArea =
            c.cmdArea;


        // ============================================================
        // SENSOR STATE
        // ============================================================

        out.enabled =
            sensor->IsEnabled();

        out.engagedRT =
            sensor->IsEngagedRT();

        out.engagedH24 =
            sensor->IsEngagedH24();


        // ============================================================
        // FINAL SENSOR OUTPUT
        // ============================================================

        out.alarmOut =
            sensor->Outputs().rt ||
            sensor->Outputs().h24;


        // ============================================================
        // FINAL SENSOR MEMORIES
        // ============================================================

        out.rtMem =
            sensor->Outputs().rtMem;

        out.h24Mem =
            sensor->Outputs().h24Mem;


        // ============================================================
        // CHANNELS
        // ============================================================

        for (size_t i = 0; i < 4; ++i)
        {
            const auto type =
                static_cast<SensorChannelType>(i);


            const SensorChannel* ch =
                sensor->Get(type);


            if (!ch)
                continue;


            out.active[i] =
                ch->IsActive();

            out.alarm[i] =
                sensor->ChannelAlarm(type);

            out.inhibit[i] =
                ch->IsInhibit();
        }


        return true;
    }


    // ============================================================
    // SENSOR COMMAND
    //
    // bit 0 = ENABLE
    // bit 1 = ENGAGE
    // ============================================================

    bool setSensor(
        size_t sensorIndex,
        bool enable,
        bool engageRT,
        bool engageH24)
    {
        auto& ws =
            SecurityOrchestrator::getWiredSensors();

        if (sensorIndex >= ws.Count())
            return false;


        const auto* cfg =
            ws.GetConfig();

        if (!cfg)
            return false;


        const int area =
            cfg[sensorIndex].cmdArea;

        if (area < 0)
            return false;


        long value = 0;


        // ------------------------------------------------------------
        // COMMAND FORMAT
        // ------------------------------------------------------------
        //
        // bit 0 = ENABLE
        // bit 1 = ENGAGE RT
        // bit 2 = ENGAGE H24
        //
        // ------------------------------------------------------------

        bitWrite(
            value,
            0,
            enable
        );

        bitWrite(
            value,
            1,
            engageRT
        );

        bitWrite(
            value,
            2,
            engageH24
        );


        return SecurityOrchestrator::
            ApplySecurityCommand(
                area,
                value
            );
    }


    bool enableSensor(
        size_t sensorIndex,
        bool enable)
    {
        SensorState state;

        if (!getSensorState(
                sensorIndex,
                state))
        {
            return false;
        }


        return setSensor(
            sensorIndex,
            enable,
            state.engagedRT,
            state.engagedH24
        );
    }


    bool engageRTSensor(
        size_t sensorIndex,
        bool engage)
    {
        SensorState state;

        if (!getSensorState(
                sensorIndex,
                state))
        {
            return false;
        }


        return setSensor(
            sensorIndex,
            state.enabled,
            engage,
            state.engagedH24
        );
    }


    bool engageH24Sensor(
        size_t sensorIndex,
        bool engage)
    {
        SensorState state;

        if (!getSensorState(
                sensorIndex,
                state))
        {
            return false;
        }


        return setSensor(
            sensorIndex,
            state.enabled,
            state.engagedRT,
            engage
        );
    }


    // ============================================================
    // ZONES
    // ============================================================

    size_t zoneCount() const
    {
        return SecurityOrchestrator::
            getWiredSensors()
            .GetZoneCount();
    }


    bool getZoneState(
        size_t zoneIndex,
        ZoneState& out) const
    {
        auto& ws =
            SecurityOrchestrator::getWiredSensors();

        if (zoneIndex >= ws.GetZoneCount())
            return false;

        const char* zoneName =
            ws.GetZoneName(zoneIndex);

        if (!zoneName)
            return false;

        const auto& zones =
            ws.Zones();

        out = ZoneState{};

        out.valid = true;
        out.index = zoneIndex;
        out.name = zoneName;

        // --------------------------------------------------------
        // RT
        // --------------------------------------------------------

        out.alarm =
            zones.ZoneAlarmByType(
                zoneName,
                SensorChannelType::RT
            );

        // --------------------------------------------------------
        // H24
        // --------------------------------------------------------

        out.alarmH24 =
            zones.ZoneAlarmByType(
                zoneName,
                SensorChannelType::H24
            );

        // --------------------------------------------------------
        // MEMORIA ZONA
        //
        // OR delle memorie dei sensori appartenenti alla zona.
        // --------------------------------------------------------

        const auto& sensors =
            zones.GetZone(zoneName);

        for (auto* sensor : sensors)
        {
            if (!sensor)
                continue;

            out.rtMem |=
                sensor->Outputs().rtMem;

            out.h24Mem |=
                sensor->Outputs().h24Mem;
        }

        // --------------------------------------------------------
        // ANOMALIA
        // --------------------------------------------------------

        out.trouble =
            DomoManagerAlarmPanel::
                instance()
                .getZoneTrouble(
                    static_cast<int>(zoneIndex)
                );

        // --------------------------------------------------------
        // ESCLUSA
        // --------------------------------------------------------

        out.bypassed =
            DomoManagerAlarmPanel::
                instance()
                .isZoneBypassed(
                    static_cast<int>(zoneIndex)
                );

        return true;
    }


    bool bypassZone(
        size_t zoneIndex)
    {
        return DomoManagerAlarmPanel::
            instance()
            .bypassZone(
                static_cast<int>(zoneIndex)
            );
    }


    bool clearZoneBypass(
        size_t zoneIndex)
    {
        return DomoManagerAlarmPanel::
            instance()
            .clearBypass(
                static_cast<int>(zoneIndex)
            );
    }


    // ============================================================
    // PANEL STATE
    // ============================================================

    bool getPanelState(
        PanelState& out,
        int partition = 0) const
    {
        auto& panel =
            DomoManagerAlarmPanel::instance();

        out = PanelState{};

        out.valid = true;

        out.connected =
            panel.isConnected();

        out.communicationFault =
            panel.isCommunicationFault();

        out.channelSupervised =
            panel.isChannelSupervised();

        out.channelLatencyMs =
            panel.getChannelLatencyMs();

        out.ready =
            panel.isReady(partition);
            
        out.readyForArm =
            panel.isReadyForArm(partition);

        out.armState =
            panel.getArmState(partition);

        out.globalTamper =
            panel.getGlobalTamper();

        out.globalTrouble =
            panel.getGlobalTrouble();

        out.systemBitmask =
            static_cast<uint32_t>(
                panel.getSystemBitmask()
            );

        out.changeFlags =
            panel.getLastChanges();

        out.panelCommandArea =
            SecurityOrchestrator::
                getPanelCommandArea();

        out.partitionCount =
            panel.getPartitionCount();

        out.zoneCount =
            panel.getZoneCount();

        out.sensorCount =
            sensorCount();

        return true;
    }


    // ============================================================
    // PANEL COMMAND
    // ============================================================

    PanelCommandResult commandPanel(
        AlarmPanelInterface::ArmState desired,
        int partition = 0)
    {
        auto& panel =
            DomoManagerAlarmPanel::instance();

        PanelCommandResult result;

        switch (desired)
        {
            case AlarmPanelInterface::ArmState::ARMED_AWAY:

                result.accepted =
                    panel.armAway(partition);

                break;

            case AlarmPanelInterface::ArmState::ARMED_STAY:

                result.accepted =
                    panel.armStay(partition);

                break;

            case AlarmPanelInterface::ArmState::ARMED_NIGHT:

                result.accepted =
                    panel.armNight(partition);

                break;

            case AlarmPanelInterface::ArmState::DISARMED:

                result.accepted =
                    panel.disarm(partition);

                break;

            default:

                result.accepted = false;

                break;
        }

        result.state =
            panel.getArmState(partition);

        result.ready =
            panel.isReady(partition);

        result.communicationFault =
            panel.isCommunicationFault();

        return result;
    }


    // ============================================================
    // PANEL COMMAND ENUM
    // ============================================================

    PanelCommandResult commandPanel(
        SecurityOrchestrator::AlarmPanelCommand command,
        int partition = 0)
    {
        switch (command)
        {
            case SecurityOrchestrator::
                AlarmPanelCommand::ARM_AWAY:

                return commandPanel(
                    AlarmPanelInterface::
                        ArmState::ARMED_AWAY,
                    partition
                );

            case SecurityOrchestrator::
                AlarmPanelCommand::ARM_STAY:

                return commandPanel(
                    AlarmPanelInterface::
                        ArmState::ARMED_STAY,
                    partition
                );

            case SecurityOrchestrator::
                AlarmPanelCommand::ARM_NIGHT:

                return commandPanel(
                    AlarmPanelInterface::
                        ArmState::ARMED_NIGHT,
                    partition
                );

            case SecurityOrchestrator::
                AlarmPanelCommand::DISARM:

                return commandPanel(
                    AlarmPanelInterface::
                        ArmState::DISARMED,
                    partition
                );

            default:
                break;
        }

        return {};
    }
};

inline void SecurityOrchestrator::Diagnostic::ReportHmi()
{
    LOG_IF(
        "SecurityOrchestrator",
        "================ HMI STATUS ================"
    );

    if (!SecurityOrchestrator::isInitialized())
    {
        LOG_IF(
            "SecurityOrchestrator",
            "HMI: SecurityOrchestrator non initialized"
        );

        return;
    }

    if (!DomoManager::instance)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "HMI: DomoManager::instance is null"
        );

        return;
    }

    auto& manager =
        *DomoManager::instance;

    auto& buffer =
        manager.getBuffer();

    auto& ws =
        SecurityOrchestrator::getWiredSensors();

    const auto* sensorCfg =
        ws.GetConfig();

    // ============================================================
    // SENSORI
    // ============================================================

    LOG_IF(
        "SecurityOrchestrator",
        "---- SENSOR HMI STATUS ----"
    );

    if (!sensorCfg)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "HMI Sensors: configuration unavailable"
        );
    }
    else
    {
        for (size_t i = 0; i < ws.Count(); ++i)
        {
            const auto& cfg =
                sensorCfg[i];

            const int area =
                cfg.statusArea;

            if (area < 0)
            {
                LOG_DF(
                    "SecurityOrchestrator",
                    "Sensor[%u] '%s': statusArea=DISABLED",
                    (unsigned)i,
                    cfg.name ? cfg.name : "<null>"
                );

                continue;
            }

            SecurityHmiInterface::SensorState state;

            if (!SecurityHmiInterface::instance()
                    .getSensorState(i, state))
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensor[%u] '%s': unable to read state",
                    (unsigned)i,
                    cfg.name ? cfg.name : "<null>"
                );

                continue;
            }

            const long value =
                buffer.getValueFast(area);

            LOG_IF(
                "SecurityOrchestrator",
                "Sensor[%u] '%s' zone='%s' area=%d value=0x%08lX",
                (unsigned)i,
                state.name ? state.name : "<null>",
                state.zone ? state.zone : "<null>",
                area,
                (unsigned long)value
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  RT:   active=%u alarm=%u inhibit=%u",
                state.active[0],
                state.alarm[0],
                state.inhibit[0]
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  H24:  active=%u alarm=%u inhibit=%u",
                state.active[1],
                state.alarm[1],
                state.inhibit[1]
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  LEN:  active=%u alarm=%u inhibit=%u",
                state.active[2],
                state.alarm[2],
                state.inhibit[2]
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  MASK: active=%u alarm=%u inhibit=%u",
                state.active[3],
                state.alarm[3],
                state.inhibit[3]
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  alarmOut=%u",
                state.alarmOut
            );
        }
    }

    // ============================================================
    // ZONE
    // ============================================================

    LOG_IF(
        "SecurityOrchestrator",
        "---- ZONE HMI STATUS ----"
    );

    auto& zones =
        ws.Zones();

    for (size_t i = 0;
         i < ws.GetZoneCount();
         ++i)
    {
        const char* zoneName =
            ws.GetZoneName(i);

        if (!zoneName)
        {
            LOG_IF(
                "SecurityOrchestrator",
                "Zone[%u]: invalid name",
                (unsigned)i
            );

            continue;
        }

        const int area =
            zones.GetZoneStatusArea(zoneName);

        if (area < 0)
        {
            LOG_IF(
                "SecurityOrchestrator",
                "Zone[%u] '%s': statusArea=DISABLED",
                (unsigned)i,
                zoneName
            );

            continue;
        }

        SecurityHmiInterface::ZoneState state;

        if (!SecurityHmiInterface::instance()
                .getZoneState(i, state))
        {
            LOG_IF(
                "SecurityOrchestrator",
                "Zone[%u] '%s': unable to read state",
                (unsigned)i,
                zoneName
            );

            continue;
        }

        const long value =
            buffer.getValueFast(area);

        LOG_IF(
            "SecurityOrchestrator",
            "Zone[%u] '%s' area=%d value=0x%08lX",
            (unsigned)i,
            zoneName,
            area,
            (unsigned long)value
        );

        LOG_IF(
            "SecurityOrchestrator",
            "  alarm=%u tamper=%u trouble=%u bypassed=%u",
            state.alarm,
            state.alarmH24,
            state.trouble,
            state.bypassed
        );
    }

    // ============================================================
    // SISTEMA / CENTRALE
    // ============================================================

    LOG_IF(
        "SecurityOrchestrator",
        "---- SYSTEM / PANEL HMI STATUS ----"
    );

    const FrontendConfig::Security& securityCfg =
        SecurityOrchestrator::cfgCopy;

    const int systemArea =
        securityCfg.statusArea;

    if (systemArea < 0)
    {
        LOG_IF(
            "SecurityOrchestrator",
            "System: statusArea=DISABLED"
        );
    }
    else
    {
        SecurityHmiInterface::PanelState state;

        if (!SecurityHmiInterface::instance()
                .getPanelState(state, 0))
        {
            LOG_IF(
                "SecurityOrchestrator",
                "System: unable to read panel state"
            );
        }
        else
        {
            const long value =
                buffer.getValueFast(systemArea);

            LOG_IF(
                "SecurityOrchestrator",
                "System area=%d value=0x%08lX",
                systemArea,
                (unsigned long)value
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  systemBitmask=0x%08X",
                (unsigned)state.systemBitmask
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  connected=%u",
                state.connected
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  communicationFault=%u",
                state.communicationFault
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  channelSupervised=%u",
                state.channelSupervised
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  ready=%u",
                state.ready
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  globalTamper=%u",
                state.globalTamper
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  globalTrouble=%u",
                state.globalTrouble
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  armState=%u",
                (unsigned)state.armState
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  panelCommandArea=%d",
                state.panelCommandArea
            );

            LOG_IF(
                "SecurityOrchestrator",
                "  changeFlags=0x%02X",
                (unsigned)state.changeFlags
            );
        }
    }

    // ============================================================
    // GLOBAL SECURITY AREAS
    // ============================================================

    LOG_IF(
        "SecurityOrchestrator",
        "---- SECURITY HMI AREAS ----"
    );

    LOG_IF(
        "SecurityOrchestrator",
        "  statusArea=%d",
        securityCfg.statusArea
    );

    LOG_IF(
        "SecurityOrchestrator",
        "  eventArea=%d",
        securityCfg.eventArea
    );

    LOG_IF(
        "SecurityOrchestrator",
        "  panelCommandArea=%d",
        securityCfg.panelCommandArea
    );

    LOG_IF(
        "SecurityOrchestrator",
        "================ END HMI STATUS ================"
    );
}