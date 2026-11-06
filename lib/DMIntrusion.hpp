
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <set>
#include <cstring>

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


class IntrusionBoardFaultManager
{
public:
    using AreaErrorProvider = std::function<bool(int)>;

private:
    std::vector<std::vector<int>> hardwareAreas;
    std::unordered_map<int, std::vector<size_t>> areaToSensors;

    std::vector<uint8_t> sensorAnomaly;
    std::vector<uint8_t> zoneAnomaly;

    // Buffer riutilizzato per evitare allocazioni in
    // recomputeZoneAnomaly().
    std::vector<uint8_t> zoneAnomalyWork;

    AreaErrorProvider areaErrorProvider = nullptr;

    bool recomputeZoneAnomaly(WiredSensorsManager& ws)
    {
        const size_t zoneCount = ws.GetZoneCount();
        const size_t sensorCount =
            std::min(ws.Count(), sensorAnomaly.size());

        if (zoneAnomaly.size() != zoneCount)
            zoneAnomaly.assign(zoneCount, 0);

        if (zoneAnomalyWork.size() != zoneCount)
            zoneAnomalyWork.resize(zoneCount);

        std::fill(
            zoneAnomalyWork.begin(),
            zoneAnomalyWork.end(),
            uint8_t(0)
        );

        for (size_t i = 0; i < sensorCount; ++i)
        {
            if (!sensorAnomaly[i])
                continue;

            const Sensor* sensor =
                ws.GetSensor(i);

            if (!sensor || !sensor->IsEnabled())
                continue;

            const int zoneIndex =
                ws.GetSensorZoneIndex(i);

            if (zoneIndex >= 0 &&
                static_cast<size_t>(zoneIndex) < zoneCount)
            {
                zoneAnomalyWork[
                    static_cast<size_t>(zoneIndex)
                ] = 1;
            }
        }

        if (zoneAnomalyWork == zoneAnomaly)
            return false;

        zoneAnomaly.swap(zoneAnomalyWork);
        return true;
    }

    bool updateSensorState(size_t sensorIndex)
    {
        if (sensorIndex >= hardwareAreas.size() ||
            sensorIndex >= sensorAnomaly.size())
        {
            return false;
        }

        uint8_t newState = 0;

        if (areaErrorProvider)
        {
            for (const int area : hardwareAreas[sensorIndex])
            {
                if (area >= 0 &&
                    areaErrorProvider(area))
                {
                    newState = 1;
                    break;
                }
            }
        }

        if (sensorAnomaly[sensorIndex] == newState)
            return false;

        sensorAnomaly[sensorIndex] = newState;
        return true;
    }

public:
    void Setup(
        WiredSensorsManager& ws,
        AreaErrorProvider provider)
    {
        areaErrorProvider =
            std::move(provider);

        const size_t sensorCount =
            ws.Count();

        const size_t zoneCount =
            ws.GetZoneCount();

        hardwareAreas.assign(
            sensorCount,
            {}
        );

        sensorAnomaly.assign(
            sensorCount,
            0
        );

        zoneAnomaly.assign(
            zoneCount,
            0
        );

        zoneAnomalyWork.assign(
            zoneCount,
            0
        );

        areaToSensors.clear();

        for (size_t i = 0;
             i < sensorCount;
             ++i)
        {
            const size_t readerCount =
                ws.GetReaderCount(i);

            auto& areas =
                hardwareAreas[i];

            areas.reserve(readerCount);

            for (size_t j = 0;
                 j < readerCount;
                 ++j)
            {
                const int area =
                    ws.GetReaderArea(i, j);

                if (area < 0)
                    continue;

                if (std::find(
                        areas.begin(),
                        areas.end(),
                        area) != areas.end())
                {
                    continue;
                }

                areas.push_back(area);
                areaToSensors[area].push_back(i);
            }
        }

        UpdateAll(ws);
    }

    bool UpdateArea(
        WiredSensorsManager& ws,
        int area)
    {
        if (area < 0)
            return false;

        const auto it =
            areaToSensors.find(area);

        if (it == areaToSensors.end())
            return false;

        bool changed = false;

        for (const size_t sensorIndex : it->second)
        {
            if (updateSensorState(sensorIndex))
                changed = true;
        }

        if (changed)
            recomputeZoneAnomaly(ws);

        return changed;
    }

    bool UpdateAll(
        WiredSensorsManager& ws)
    {
        bool changed = false;

        const size_t count =
            std::min(
                ws.Count(),
                sensorAnomaly.size()
            );

        for (size_t i = 0;
             i < count;
             ++i)
        {
            if (updateSensorState(i))
                changed = true;
        }

        if (changed)
            recomputeZoneAnomaly(ws);

        return changed;
    }

    bool RefreshZoneAnomaly(
        WiredSensorsManager& ws)
    {
        return recomputeZoneAnomaly(ws);
    }

    inline bool IsSensorAnomaly(
        size_t sensorIndex) const
    {
        return sensorIndex < sensorAnomaly.size() &&
               sensorAnomaly[sensorIndex] != 0;
    }

    inline bool IsZoneAnomaly(
        size_t zoneIndex) const
    {
        return zoneIndex < zoneAnomaly.size() &&
               zoneAnomaly[zoneIndex] != 0;
    }

    size_t GetEnabledSensorAnomalyCount(
        WiredSensorsManager& ws) const
    {
        const size_t sensorCount =
            std::min(
                sensorAnomaly.size(),
                ws.Count()
            );

        size_t count = 0;

        for (size_t i = 0;
            i < sensorCount;
            ++i)
        {
            if (!sensorAnomaly[i])
                continue;

            const Sensor* sensor =
                ws.GetSensor(i);

            if (!sensor ||
                !sensor->IsEnabled())
            {
                continue;
            }

            const int zoneIndex =
                ws.GetSensorZoneIndex(i);

            // Sensore non zonato oppure zona non trovata:
            // manteniamo il comportamento attuale.
            if (zoneIndex >= 0)
            {
                const char* zone =
                    ws.GetZoneName(
                        static_cast<size_t>(zoneIndex)
                    );

                if (zone &&
                    zone[0] &&
                    !ws.Zones().IsZoneEnabled(zone))
                {
                    continue;
                }
            }

            ++count;
        }

        return count;
    }

    size_t GetSensorAnomalyCount() const
    {
        size_t count = 0;

        for (const uint8_t state :
             sensorAnomaly)
        {
            if (state)
                ++count;
        }

        return count;
    }

    size_t GetEnabledZoneAnomalyCount(
        WiredSensorsManager& ws) const
    {
        const size_t zoneCount =
            std::min(
                zoneAnomaly.size(),
                ws.GetZoneCount()
            );

        size_t count = 0;

        for (size_t i = 0;
            i < zoneCount;
            ++i)
        {
            if (!zoneAnomaly[i])
                continue;

            const char* zone =
                ws.GetZoneName(i);

            if (!zone ||
                !zone[0])
            {
                continue;
            }

            // Una zona disabilitata NON contribuisce
            // all'anomalia globale della centrale.
            if (!ws.Zones().IsZoneEnabled(zone))
                continue;

            ++count;
        }

        return count;
    }

    size_t GetZoneAnomalyCount() const
    {
        size_t count = 0;

        for (const uint8_t state :
             zoneAnomaly)
        {
            if (state)
                ++count;
        }

        return count;
    }

    int GetSensorHardwareArea(
        size_t sensorIndex) const
    {
        if (sensorIndex >= hardwareAreas.size() ||
            hardwareAreas[sensorIndex].empty())
        {
            return -1;
        }

        return hardwareAreas[sensorIndex][0];
    }

    const std::vector<int>&
    GetSensorHardwareAreas(
        size_t sensorIndex) const
    {
        static const std::vector<int> empty;

        if (sensorIndex >= hardwareAreas.size())
            return empty;

        return hardwareAreas[sensorIndex];
    }

    void Report(
        WiredSensorsManager& ws) const
    {
        const auto* cfg =
            ws.GetConfig();

        const size_t sensorCount =
            ws.Count();

        LOG_IF(
            "IntrusionBoardFaultManager",
            "===== SENSOR ANOMALY MAP ====="
        );

        const size_t count =
            std::min(
                sensorCount,
                hardwareAreas.size()
            );

        for (size_t i = 0;
             i < count;
             ++i)
        {
            const char* name =
                (cfg && cfg[i].name)
                    ? cfg[i].name
                    : "?";

            const char* zone =
                (cfg && cfg[i].zone)
                    ? cfg[i].zone
                    : "?";

            LOG_IF(
                "IntrusionBoardFaultManager",
                "Sensor[%u] name=%s zone=%s anomaly=%d areas=%u",
                (unsigned)i,
                name,
                zone,
                IsSensorAnomaly(i) ? 1 : 0,
                (unsigned)hardwareAreas[i].size()
            );

            for (size_t j = 0;
                 j < hardwareAreas[i].size();
                 ++j)
            {
                LOG_IF(
                    "IntrusionBoardFaultManager",
                    "  area[%u]=%d",
                    (unsigned)j,
                    hardwareAreas[i][j]
                );
            }
        }

        LOG_IF(
            "IntrusionBoardFaultManager",
            "Sensor anomalies : %u",
            (unsigned)GetSensorAnomalyCount()
        );

        LOG_IF(
            "IntrusionBoardFaultManager",
            "Zone anomalies   : %u",
            (unsigned)GetZoneAnomalyCount()
        );

        LOG_IF(
            "IntrusionBoardFaultManager",
            "=============================="
        );
    }
};

class SecurityOrchestrator
{
public:
    // ============================================================
    // ALARM PANEL COMMAND
    // ============================================================
    enum PanelBitCommand : uint8_t { BITCMD_SILENCE_ALARM = 0 };
    enum AlarmPanelCommand : uint8_t { DISARM = 0, ARM_AWAY, ARM_STAY, ARM_NIGHT };

    // ============================================================
    // CHANGE FLAGS
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
        CORE, CONFIG, SENSORS, ZONES, COMMANDS, SYSTEM,
        SECURITY, HMI, INCONSISTENCIES, DIAGNOSTICS, FULL
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
            ALARM_INTRUSION_MEM,
            ALARM_INTRUSION_H24_MEM,
            ALARM_FLOOD_MEM,
            ALARM_SMOKE_MEM,
            WINDOWS_OPEN_MEM,
            DOORS_OPEN_MEM,
            ALARM_TAMPER,
            ALARM_PANIC,
            ALARM_CAMERA,
            GLOBAL_TROUBLE,
            FIELD_COUNT
        };

        struct Info { Cell<bool> f[FIELD_COUNT]; };
        Info info;

        SystemManager()
        {
            for(int i=0;i<FIELD_COUNT;++i)
                info.f[i].set(false);
        }

        void set(Field f,bool v)
        {
            info.f[f].setIfDiff(v);
        }

        bool hasChanged()
        {
            bool changed=false;

            for(int i=0;i<FIELD_COUNT;++i)
            {
                if(info.f[i].hasChanged())
                {
                    changed=true;
                    info.f[i].resetChanged();
                }
            }

            return changed;
        }

        int getBitmask() const
        {
            int mask=0;

            for(int i=0;i<FIELD_COUNT;++i)
            {
                if(info.f[i].get())
                    mask|=(1<<i);
            }

            return mask;
        }

       
        // ============================================================
        // COMPUTE SYSTEM STATE
        //
        // Le MEM 6..11 sono memoria della CENTRALE.
        // Non vengono prese da st.mem del sensore.
        //
        // Una memoria centrale viene impostata solo se:
        //   - la centrale è inserita
        //   - esiste il relativo allarme realtime
        //
        // Dopo il disinserimento rimane memorizzata fino al TACITA.
        // ============================================================

        void ComputeFrom(
            const WiredSensorsManager& ws,
            bool boardTrouble = false,
            bool alarmEngaged = false)
        {
            const auto st =
                ws.ComputeAggregate();

            // --------------------------------------------------------
            // REALTIME
            // --------------------------------------------------------

            set(
                ALARM_PANIC,
                st.panic
            );

            set(
                ALARM_CAMERA,
                st.camera
            );

            set(
                ALARM_INTRUSION,
                st.normal.intrusion
            );

            set(
                ALARM_INTRUSION_H24,
                st.normal.intrusionH24
            );

            set(
                ALARM_FLOOD,
                st.normal.flood
            );

            set(
                ALARM_SMOKE,
                st.normal.smoke
            );

            set(
                WINDOWS_OPEN,
                st.normal.windowsOpen
            );

            set(
                DOORS_OPEN,
                st.normal.doorsOpen
            );

            // --------------------------------------------------------
            // MEMORIA CENTRALE
            //
            // Si attiva SOLO durante centrale inserita.
            // Non viene mai ricopiata da st.mem.
            // --------------------------------------------------------

            if(alarmEngaged &&
                SecurityOrchestrator::HasUnsilencedMemory())
            {
                if(st.mem.intrusion)
                    set(ALARM_INTRUSION_MEM, true);

                if(st.mem.intrusionH24)
                    set(ALARM_INTRUSION_H24_MEM, true);

                if(st.mem.flood)
                    set(ALARM_FLOOD_MEM, true);

                if(st.mem.smoke)
                    set(ALARM_SMOKE_MEM, true);

                if(st.mem.windowsOpen)
                    set(WINDOWS_OPEN_MEM, true);

                if(st.mem.doorsOpen)
                    set(DOORS_OPEN_MEM, true);
            }


            // --------------------------------------------------------
            // SERVIZIO
            // --------------------------------------------------------

            set(
                ALARM_TAMPER,
                st.tamper
            );

            set(
                GLOBAL_TROUBLE,
                boardTrouble
            );
        }

        // ============================================================
        // RESET MEMORIE CENTRALE
        //
        // Il TACITA cancella le memorie della centrale,
        // ma NON le memorie dei singoli sensori.
        // ============================================================

        void ResetAlarmMemories()
        {
            set(ALARM_INTRUSION_MEM,     false);
            set(ALARM_INTRUSION_H24_MEM, false);
            set(ALARM_FLOOD_MEM,         false);
            set(ALARM_SMOKE_MEM,         false);
            set(WINDOWS_OPEN_MEM,        false);
            set(DOORS_OPEN_MEM,          false);

        }

        void Report() const
        {
            LOG_IF("SystemManager","===== SYSTEM STATE =====");
            LOG_IF("SystemManager","%-25s : %s","Intrusion REALTIME",info.f[ALARM_INTRUSION].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Intrusion H24 REALTIME",info.f[ALARM_INTRUSION_H24].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Flood REALTIME",info.f[ALARM_FLOOD].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Smoke REALTIME",info.f[ALARM_SMOKE].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Windows Open REALTIME",info.f[WINDOWS_OPEN].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Doors Open REALTIME",info.f[DOORS_OPEN].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Intrusion MEM",info.f[ALARM_INTRUSION_MEM].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Intrusion H24 MEM",info.f[ALARM_INTRUSION_H24_MEM].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Flood MEM",info.f[ALARM_FLOOD_MEM].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Smoke MEM",info.f[ALARM_SMOKE_MEM].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Windows Open MEM",info.f[WINDOWS_OPEN_MEM].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Doors Open MEM",info.f[DOORS_OPEN_MEM].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Tamper",info.f[ALARM_TAMPER].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Panic",info.f[ALARM_PANIC].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Camera",info.f[ALARM_CAMERA].get()?"TRUE":"false");
            LOG_IF("SystemManager","%-25s : %s","Global Trouble",info.f[GLOBAL_TROUBLE].get()?"TRUE":"false");
            LOG_IF("SystemManager","Bitmask                : 0x%08X",(unsigned)getBitmask());
            LOG_IF("SystemManager","========================");
        }

        void DiagnosticReport() const
        {
            Report();
        }
    };

private:
    // ============================================================
    // STATIC STATE
    // ============================================================
    static inline IntrusionBoardFaultManager boardFaults;
    static inline SystemManager system;
    static inline WiredSensorsManager ws;
    static inline FrontendConfig::Security cfgCopy;
    static inline bool initialized = false;
    static inline AlarmBitmaskManager alarmMask;
    static inline uint8_t lastChanges = CHANGE_NONE;
    static inline uint8_t pendingChanges = CHANGE_NONE;
    static inline unsigned long lastLoopAt = 0;

    // ============================================================
    // VALIDATION
    // ============================================================
    static bool validate(const FrontendConfig::Security* cfg)
    {
        if(!cfg)
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Validate: null security configuration"
            );
            return false;
        }

        if(!cfg->sensors || cfg->count == 0)
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Validate: no wired sensors configured"
            );
            return false;
        }

        for(size_t i = 0; i < cfg->count; ++i)
        {
            const auto& s = cfg->sensors[i];

            if(!s.name || !s.name[0])
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor[%u] has invalid name",
                    (unsigned)i
                );
                return false;
            }

            // Zona opzionale.
            //
            // nullptr = sensore globale/non zonato
            // ""      = configurazione errata
            //
            if(s.zone && !s.zone[0])
            {
                LOG_WF(
                    "SecurityOrchestrator",
                    "Validate: sensor[%u] '%s' has empty zone",
                    (unsigned)i,
                    s.name
                );

                return false;
            }

            if(s.channels.size() == 0)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor[%u] '%s' has no channels",
                    (unsigned)i,
                    s.name
                );
                return false;
            }

            if(s.channels.size() != s.readers.size())
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

            bool usedTypes[4] = {};

            for(size_t j = 0; j < s.channels.size(); ++j)
            {
                const auto& ch =
                    *(s.channels.begin() + j);

                const size_t typeIndex =
                    static_cast<size_t>(ch.type);

                if(typeIndex >= 4)
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

                if(usedTypes[typeIndex])
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

                if(!*(s.readers.begin() + j))
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
        // SENSOR COMMAND AREAS
        // --------------------------------------------------------

        for(size_t i = 0; i < cfg->count; ++i)
        {
            const int cmdArea =
                cfg->sensors[i].cmdArea;

            if(cmdArea < 0)
                continue;

            for(size_t j = i + 1;
                j < cfg->count;
                ++j)
            {
                if(cfg->sensors[j].cmdArea == cmdArea)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: duplicated sensor cmdArea=%d ('%s' and '%s')",
                        cmdArea,
                        cfg->sensors[i].name,
                        cfg->sensors[j].name
                    );
                    return false;
                }
            }
        }

        // --------------------------------------------------------
        // SENSOR STATUS AREAS
        // --------------------------------------------------------

        for(size_t i = 0; i < cfg->count; ++i)
        {
            const int statusArea =
                cfg->sensors[i].statusArea;

            if(statusArea < 0)
                continue;

            for(size_t j = i + 1;
                j < cfg->count;
                ++j)
            {
                if(cfg->sensors[j].statusArea == statusArea)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: duplicated sensor statusArea=%d ('%s' and '%s')",
                        statusArea,
                        cfg->sensors[i].name,
                        cfg->sensors[j].name
                    );
                    return false;
                }
            }

            if(cfg->statusArea >= 0 &&
            statusArea == cfg->statusArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' statusArea=%d collides with security statusArea",
                    cfg->sensors[i].name,
                    statusArea
                );
                return false;
            }
        }

        // --------------------------------------------------------
        // ZONE CONFIG
        // --------------------------------------------------------

        if(cfg->zoneCount > 0 && !cfg->zones)
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Validate: zoneCount=%u but zones is null",
                (unsigned)cfg->zoneCount
            );
            return false;
        }

        for(size_t i = 0; i < cfg->zoneCount; ++i)
        {
            const auto& z = cfg->zones[i];

            if(!z.name || !z.name[0])
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: zone[%u] has invalid name",
                    (unsigned)i
                );
                return false;
            }

            if(z.statusArea < 0)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: zone[%u] '%s' has invalid statusArea",
                    (unsigned)i,
                    z.name
                );
                return false;
            }

            for(size_t j = i + 1;
                j < cfg->zoneCount;
                ++j)
            {
                if(cfg->zones[j].name &&
                std::strcmp(
                    z.name,
                    cfg->zones[j].name
                ) == 0)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: duplicated zone name '%s'",
                        z.name
                    );
                    return false;
                }

                if(cfg->zones[j].statusArea ==
                z.statusArea)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: duplicated zone statusArea=%d ('%s' and '%s')",
                        z.statusArea,
                        z.name,
                        cfg->zones[j].name
                    );
                    return false;
                }
            }

            for(size_t j = 0;
                j < cfg->count;
                ++j)
            {
                if(cfg->sensors[j].statusArea >= 0 &&
                cfg->sensors[j].statusArea == z.statusArea)
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Validate: zone '%s' statusArea=%d collides with sensor '%s'",
                        z.name,
                        z.statusArea,
                        cfg->sensors[j].name
                    );
                    return false;
                }
            }

            if(cfg->statusArea >= 0 &&
            z.statusArea == cfg->statusArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: zone '%s' statusArea=%d collides with security statusArea",
                    z.name,
                    z.statusArea
                );
                return false;
            }
        }

        // --------------------------------------------------------
        // GLOBAL SECURITY AREAS
        // --------------------------------------------------------

        if(cfg->statusArea >= 0 &&
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

        if(cfg->eventArea >= 0 &&
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

        if(cfg->eventArea >= 0 &&
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
        // SENSOR COMMAND / STATUS COLLISIONS
        // --------------------------------------------------------

        for(size_t i = 0; i < cfg->count; ++i)
        {
            const auto& s = cfg->sensors[i];

            if(s.cmdArea < 0)
                continue;

            if(s.statusArea >= 0 &&
            s.cmdArea == s.statusArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' cmdArea=%d collides with its statusArea",
                    s.name,
                    s.cmdArea
                );
                return false;
            }

            if(cfg->statusArea >= 0 &&
            s.cmdArea == cfg->statusArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' cmdArea=%d collides with security statusArea",
                    s.name,
                    s.cmdArea
                );
                return false;
            }

            if(cfg->eventArea >= 0 &&
            s.cmdArea == cfg->eventArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' cmdArea=%d collides with eventArea",
                    s.name,
                    s.cmdArea
                );
                return false;
            }

            if(cfg->panelCommandArea >= 0 &&
            s.cmdArea == cfg->panelCommandArea)
            {
                LOG_EF(
                    "SecurityOrchestrator",
                    "Validate: sensor '%s' cmdArea=%d collides with panelCommandArea",
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
    // RESET MEMORIA ALLARMI
    //
    // Reset della memoria centrale + memoria bitmask.
    // Le memorie dei sensori rimangono indipendenti.
    // ============================================================

    static void ResetAlarmMemory(
        uint64_t mask)
    {
        //
        // reset memoria centrale
        //
        alarmMask.ResetMemory(mask);

        system.ResetAlarmMemories();

        //
        // reset memoria sensori rientrati
        //
        auto& ws = getWiredSensors();

        for(size_t i = 0; i < ws.Count(); ++i)
        {
            Sensor* sensor = ws.GetSensor(i);

            if(!sensor)
                continue;

            const auto& out =
                sensor->Outputs();

            if(!out.rt &&
            !out.h24)
            {
                sensor->Reset();
            }
        }

        pendingChanges |=
            CHANGE_SYSTEM |
            CHANGE_ZONE |
            CHANGE_COMMAND |
            CHANGE_SENSOR;
    }

    // ============================================================
    // SETUP
    // ============================================================
    
    static void Setup(const FrontendConfig::Security* cfg)
    {
        if(!validate(cfg))
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Setup aborted: invalid wired sensors configuration"
            );

            initialized = false;
            return;
        }

        cfgCopy = *cfg;
        pendingChanges = CHANGE_NONE;
        initialized = false;

        ws.Init(
            cfg->sensors,
            cfg->count,
            cfg->startupInhibitMs
        );

        for(size_t i = 0;
            i < ws.Count();
            ++i)
        {
            Sensor* sensor =
                ws.GetSensor(i);

            if(sensor)
                sensor->Enable(true);
        }

        ws.Zones().EnableAll(true);

        boardFaults.Setup(
            ws,
            [](int area) -> bool
            {
                return DomoManager::instance &&
                    DomoManager::instance->isAreaInError(area);
            }
        );

        if(cfg->zones && cfg->zoneCount > 0)
        {
            auto& zoneManager =
                ws.Zones();

            for(size_t i = 0;
                i < cfg->zoneCount;
                ++i)
            {
                const auto& zoneCfg =
                    cfg->zones[i];

                if(!zoneCfg.name ||
                zoneCfg.statusArea < 0)
                {
                    continue;
                }

                if(!zoneManager.SetZoneStatusArea(
                    zoneCfg.name,
                    zoneCfg.statusArea))
                {
                    LOG_EF(
                        "SecurityOrchestrator",
                        "Zone status area not assigned: zone=%s area=%d",
                        zoneCfg.name,
                        zoneCfg.statusArea
                    );
                }
            }
        }

        if(!alarmMask.BuildMap(ws))
        {
            LOG_EF(
                "SecurityOrchestrator",
                "Setup aborted: invalid alarm bitmask map"
            );

            initialized = false;
            return;
        }

        alarmMask.SetEngage(true);

        initialized = true;

        lastChanges =
            CHANGE_SENSOR |
            CHANGE_ZONE |
            CHANGE_SYSTEM;

        lastLoopAt = 0;

        alarmMask.SetCallback(
            [](uint64_t newMask,
            uint64_t currentMask,
            uint64_t memMask,
            size_t bitIndex,
            size_t sensorIndex,
            SensorChannelType type)
            {
                (void)newMask;
                (void)currentMask;
                (void)memMask;
                (void)bitIndex;
                (void)sensorIndex;
                (void)type;
            }
        );

        LOG_IF(
            "SecurityOrchestrator",
            "Setup: sensors=%u zones=%u startupInhibit=%u ms",
            (unsigned)cfg->count,
            (unsigned)cfg->zoneCount,
            (unsigned)cfg->startupInhibitMs
        );

        LOG_IF(
            "SecurityOrchestrator",
            "Setup: initial anomalies sensors=%u zones=%u",
            (unsigned)boardFaults.GetSensorAnomalyCount(),
            (unsigned)boardFaults.GetZoneAnomalyCount()
        );
    }

    // ============================================================
    // AREA ERROR EVENT
    //
    // Chiamata SOLO quando DomoManager / DMRuntime comunica
    // il cambio di stato di errore di una area hardware.
    // ============================================================
    
    static bool NotifyAreaErrorChanged(int area)
    {
        if(!initialized || area<0)
            return false;

        if(!boardFaults.UpdateArea(ws,area))
            return false;

        pendingChanges |= CHANGE_SENSOR | CHANGE_ZONE;

        return true;
    }



    // ============================================================
    // LOOP
    // ============================================================
 
    static uint8_t Loop(
        unsigned long now,
        bool alarmEngaged)
    {
        if(!initialized)
        {
            lastChanges = CHANGE_NONE;
            return CHANGE_NONE;
        }

        uint8_t changes =
            pendingChanges;

        pendingChanges =
            CHANGE_NONE;

        const auto sensorResult =
            ws.Process(now);

        if(sensorResult.changed)
            changes |= CHANGE_SENSOR;

        if(sensorResult.zoneChanged)
            changes |= CHANGE_ZONE;

        auto& zones =
            ws.Zones();

        if(zones.ProcessAllTypes())
        {
            changes |=
                CHANGE_SENSOR |
                CHANGE_ZONE;
        }

        // Le anomalie cambiano realmente in seguito a:
        // - variazioni hardware
        // - cambi di stato dei sensori
        // Non serve ricalcolarle ad ogni ciclo senza variazioni.
        if(changes & CHANGE_SENSOR)
        {
            if(boardFaults.RefreshZoneAnomaly(ws))
                changes |= CHANGE_ZONE;
        }

        const bool boardTrouble =
            boardFaults.GetEnabledSensorAnomalyCount(ws) > 0 ||
            boardFaults.GetEnabledZoneAnomalyCount(ws) > 0;

        system.ComputeFrom(
            ws,
            boardTrouble,
            alarmEngaged
        );

        alarmMask.ComputeCurrent(ws);

        if(system.hasChanged())
        {
            changes |=
                CHANGE_SYSTEM |
                CHANGE_ZONE;
        }

        lastChanges = changes;
        lastLoopAt = now;

        return changes;
    }

    // ============================================================
    // COMMANDS
    // ============================================================
    static bool ApplyPanelCommand(int area,long value);
    static bool ApplyPanelBitCommand(int area,long value);

    static bool ApplySecurityCommand(
        int area,
        long value)
    {
        if(!initialized ||
        !DomoManager::instance)
        {
            return false;
        }

        auto& buffer =
            DomoManager::instance->getBuffer();

        const auto* cfg =
            ws.GetConfig();

        if(!cfg ||
        area < 0 ||
        area >= buffer.size())
        {
            return false;
        }


        // ============================================================
        // SENSOR COMMAND
        // ============================================================

        for(size_t i = 0;
            i < ws.Count();
            ++i)
        {
            const auto& c =
                cfg[i];

            if(c.cmdArea != area)
                continue;

            Sensor* sensor =
                ws.GetSensor(i);

            if(!sensor)
                return false;

            const bool enable =
                !bitRead(value, 0);

            const bool engageRT =
                bitRead(value, 1);

            const bool engageH24 =
                bitRead(value, 2);

            const bool wasEnabled =
                sensor->IsEnabled();

            const bool wasEngageRT =
                sensor->IsEngagedRT();

            const bool wasEngageH24 =
                sensor->IsEngagedH24();

            sensor->Enable(enable);
            sensor->EngageRT(engageRT);
            sensor->EngageH24(engageH24);

            if(wasEnabled && !enable)
                alarmMask.ClearSensor(i);

            const bool changed =
                wasEnabled != enable ||
                wasEngageRT != engageRT ||
                wasEngageH24 != engageH24;

            if(changed)
            {
                pendingChanges |=
                    CHANGE_SENSOR |
                    CHANGE_ZONE |
                    CHANGE_SYSTEM |
                    CHANGE_COMMAND;
            }

            return true;
        }


        // ============================================================
        // ZONE COMMAND
        //
        // bit 0 = ENABLE
        //
        // 0 = DISABLE ZONE
        // 1 = ENABLE ZONE
        // ============================================================

        const FrontendConfig::Security& securityCfg =
            getConfig();

        for(size_t i = 0;
            i < securityCfg.zoneCount;
            ++i)
        {
            const auto& zoneCfg =
                securityCfg.zones[i];

            if(zoneCfg.cmdArea != area)
                continue;

            if(!zoneCfg.name)
                return false;

            const bool enable = !bitRead(value, 0);

            const bool wasEnabled =
                ws.Zones().IsZoneEnabled(
                    zoneCfg.name);

            if(!ws.Zones().EnableZone(
                    zoneCfg.name,
                    enable))
            {
                return false;
            }

            const bool changed =
                wasEnabled != enable;

            if(changed)
            {
                pendingChanges |=
                    CHANGE_SENSOR |
                    CHANGE_ZONE |
                    CHANGE_SYSTEM |
                    CHANGE_COMMAND;
            }

            return true;
        }

        return false;
    }

    // ============================================================
    // FORCE SECURITY COMMANDS
    // ============================================================
    
    static void ForceSecurityCommands(
        AlarmPanelInterface::ArmState state,
        unsigned long now)
    {
        auto& zoneManager = ws.Zones();

        const auto* cfg =
            ws.GetConfig();

        const bool zoneEngage =
            state != AlarmPanelInterface::ArmState::DISARMED;

        for(auto& entry : zoneManager.zones)
        {
            entry.second.engaged =
                entry.second.enabled &&
                zoneEngage;
        }

        for(size_t i = 0;
            i < ws.Count();
            ++i)
        {
            Sensor* sensor =
                ws.GetSensor(i);

            if(!sensor)
                continue;

            if(!sensor->IsEnabled())
            {
                sensor->EngageRT(false);
                sensor->EngageH24(false);
                continue;
            }

            bool engageRT  = false;
            bool engageH24 = false;

            const SensorCategory category =
                (cfg != nullptr)
                    ? cfg[i].category
                    : SensorCategory::OTHER;

            if(category == SensorCategory::TAMPER ||
            category == SensorCategory::PANIC ||
            category == SensorCategory::CAMERA )
            {
                sensor->EngageRT(true);
                sensor->EngageH24(true);
                continue;
            }

            switch(state)
            {
                case AlarmPanelInterface::ArmState::ARMED_AWAY:
                {
                    engageRT  = true;
                    engageH24 = true;
                    break;
                }

                case AlarmPanelInterface::ArmState::ARMED_STAY:
                {
                    //
                    // Manteniamo il comportamento originale
                    //
                    engageRT  = false;
                    engageH24 = true;
                    break;
                }

                case AlarmPanelInterface::ArmState::ARMED_NIGHT:
                {
                    engageH24 = true;

                    switch(category)
                    {
                        //
                        // PIR esclusi di notte
                        //
                        case SensorCategory::PIR:
                            engageRT = false;
                            break;

                        //
                        // Porte e finestre devono allarmare
                        //
                        case SensorCategory::DOOR:
                        case SensorCategory::WINDOW:
                            engageRT = true;
                            break;

                        //
                        // Sensori tecnologici
                        //
                        case SensorCategory::FLOOD:
                        case SensorCategory::SMOKE:
                        case SensorCategory::TAMPER:
                        case SensorCategory::PANIC:
                        case SensorCategory::CAMERA:
                            engageRT = true;
                            break;

                        default:
                            engageRT = true;
                            break;
                    }

                    break;
                }

                case AlarmPanelInterface::ArmState::DISARMED:
                default:
                {
                    engageRT  = false;
                    engageH24 = false;
                    break;
                }
            }

            sensor->EngageRT(engageRT);
            sensor->EngageH24(engageH24);
        }

        (void)now;
    }

    static uint64_t GetEffectiveAlarmMemoryMask()
    {
        return alarmMask.GetEffectiveMemoryMask();
    }

    // ============================================================
    // DIAGNOSTICA
    // ============================================================
    class Diagnostic
    {
    private:
        static void Header(const char* title)
        {
            LOG_IF("SecurityOrchestrator","================================================");
            LOG_IF("SecurityOrchestrator","%s",title);
            LOG_IF("SecurityOrchestrator","================================================");
        }

        static void Footer()
        {
            LOG_IF("SecurityOrchestrator","================================================");
        }

    public:
        static void ReportConfig()
        {
            LOG_IF("SecurityOrchestrator","===============================================");
            LOG_IF("SecurityOrchestrator","      SECURITY ORCHESTRATOR CONFIG REPORT");
            LOG_IF("SecurityOrchestrator","===============================================");

            if (!SecurityOrchestrator::isInitialized())
            {
                LOG_IF("SecurityOrchestrator","Initialized        : NO");
                LOG_IF("SecurityOrchestrator","===============================================");
                return;
            }

            const auto& securityCfg = SecurityOrchestrator::cfgCopy;
            const auto& ws = SecurityOrchestrator::getWiredSensors();
            const auto* sensorCfg = ws.GetConfig();

            size_t commandAreaCount = 0;
            size_t sensorStatusAreaCount = 0;

            if (sensorCfg)
            {
                for (size_t i = 0; i < ws.Count(); ++i)
                {
                    const auto& sensor = sensorCfg[i];
                    if (sensor.cmdArea >= 0) ++commandAreaCount;
                    if (sensor.statusArea >= 0) ++sensorStatusAreaCount;
                }
            }

            LOG_IF("SecurityOrchestrator","-----------------------------------------------");
            LOG_IF("SecurityOrchestrator","SECURITY GLOBAL CONFIGURATION");
            LOG_IF("SecurityOrchestrator","Enabled            : %s",securityCfg.enabled?"YES":"NO");
            LOG_IF("SecurityOrchestrator","Interval            : %lu ms",(unsigned long)securityCfg.intervalMs);
            LOG_IF("SecurityOrchestrator","Report on change    : %s",securityCfg.reportOnChange?"YES":"NO");
            LOG_IF("SecurityOrchestrator","Startup inhibit     : %lu ms",(unsigned long)securityCfg.startupInhibitMs);
            LOG_IF("SecurityOrchestrator","System status area  : %d",securityCfg.statusArea);
            LOG_IF("SecurityOrchestrator","Event area          : %d",securityCfg.eventArea);
            LOG_IF("SecurityOrchestrator","Panel command area  : %d",securityCfg.panelCommandArea);

            LOG_IF("SecurityOrchestrator","-----------------------------------------------");
            LOG_IF("SecurityOrchestrator","WIRED SENSORS");

            if (!sensorCfg)
            {
                LOG_IF("SecurityOrchestrator","Sensor configuration: NULL");
            }
            else
            {
                LOG_IF("SecurityOrchestrator","Sensors configured  : %u",(unsigned)ws.Count());

                for (size_t i = 0; i < ws.Count(); ++i)
                {
                    const auto& sensor = sensorCfg[i];
                    const Sensor* runtimeSensor = ws.GetSensor(i);

                    LOG_IF("SecurityOrchestrator",
                        "Sensor[%u] | name=%s | zone=%s | category=%u",
                        (unsigned)i,
                        sensor.name ? sensor.name : "<null>",
                        sensor.zone ? sensor.zone : "<null>",
                        (unsigned)sensor.category);

                    if (runtimeSensor)
                    {
                        LOG_IF("SecurityOrchestrator",
                            "  runtime: ENABLED=%s | ENGAGE_RT=%s | ENGAGE_H24=%s | ALARM=%s",
                            runtimeSensor->IsEnabled()?"YES":"NO",
                            runtimeSensor->IsEngagedRT()?"YES":"NO",
                            runtimeSensor->IsEngagedH24()?"YES":"NO",
                            (runtimeSensor->Outputs().rt || runtimeSensor->Outputs().h24)?"YES":"NO");
                    }
                    else
                    {
                        LOG_IF("SecurityOrchestrator","  runtime: INVALID SENSOR");
                    }

                    LOG_IF("SecurityOrchestrator",
                        "  cmdArea=%d | statusArea=%d",
                        sensor.cmdArea,sensor.statusArea);

                    LOG_IF("SecurityOrchestrator",
                        "  channels=%u | readers=%u",
                        (unsigned)sensor.channels.size(),
                        (unsigned)sensor.readers.size());

                    size_t channelIndex = 0;
                    for (const auto& channel : sensor.channels)
                    {
                        LOG_IF("SecurityOrchestrator",
                            "    channel[%u] type=%u pin=%d",
                            (unsigned)channelIndex,
                            (unsigned)channel.type,
                            channel.pin);
                        ++channelIndex;
                    }

                    size_t readerIndex = 0;
                    for (const auto& reader : sensor.readers)
                    {
                        LOG_IF("SecurityOrchestrator",
                            "    reader[%u] %s",
                            (unsigned)readerIndex,
                            reader ? "configured" : "NULL");
                        ++readerIndex;
                    }
                }
            }

            LOG_IF("SecurityOrchestrator","-----------------------------------------------");
            LOG_IF("SecurityOrchestrator","ZONES");
            LOG_IF("SecurityOrchestrator","Zones configured   : %u",(unsigned)securityCfg.zoneCount);

            if (securityCfg.zoneCount == 0)
            {
                LOG_IF("SecurityOrchestrator","  No explicit zone configuration");
            }
            else if (!securityCfg.zones)
            {
                LOG_IF("SecurityOrchestrator","  ERROR: zone configuration is NULL");
            }
            else
            {
                const auto& zones = ws.Zones();

                for (size_t i = 0; i < securityCfg.zoneCount; ++i)
                {
                    const auto& zoneCfg = securityCfg.zones[i];

                    if (!zoneCfg.name)
                    {
                        LOG_IF("SecurityOrchestrator",
                            "Zone[%u] | name=NULL | statusArea=%d",
                            (unsigned)i,zoneCfg.statusArea);
                        continue;
                    }

                    const int zoneIndex = ws.GetZoneIndex(zoneCfg.name);
                    const int runtimeStatusArea = zones.GetZoneStatusArea(zoneCfg.name);

                    LOG_IF("SecurityOrchestrator",
                        "Zone[%u] | name=%s | statusArea=%d | runtimeArea=%d | index=%d",
                        (unsigned)i,
                        zoneCfg.name,
                        zoneCfg.statusArea,
                        runtimeStatusArea,
                        zoneIndex);
                }
            }

            LOG_IF("SecurityOrchestrator","-----------------------------------------------");
            LOG_IF("SecurityOrchestrator","RUNTIME ZONE INDEX");
            LOG_IF("SecurityOrchestrator","Runtime zones      : %u",(unsigned)ws.GetZoneCount());

            const auto& runtimeZones = ws.Zones();

            for (size_t i = 0; i < ws.GetZoneCount(); ++i)
            {
                const char* zoneName = ws.GetZoneName(i);
                if (!zoneName)
                {
                    LOG_IF("SecurityOrchestrator","Zone[%u] | name=NULL",(unsigned)i);
                    continue;
                }

                const int statusArea = runtimeZones.GetZoneStatusArea(zoneName);
                const bool enabled = runtimeZones.IsZoneEnabled(zoneName);
                const bool engaged = runtimeZones.IsZoneEngaged(zoneName);

                LOG_IF("SecurityOrchestrator",
                    "Zone[%u] | name=%s | statusArea=%d | ENABLED=%s | ENGAGED=%s",
                    (unsigned)i,
                    zoneName,
                    statusArea,
                    enabled?"YES":"NO",
                    engaged?"YES":"NO");
            }

            LOG_IF("SecurityOrchestrator","-----------------------------------------------");
            LOG_IF("SecurityOrchestrator","SENSOR COMMAND / STATUS AREAS");

            if (!sensorCfg)
            {
                LOG_IF("SecurityOrchestrator","Sensor configuration: NULL");
            }
            else
            {
                for (size_t i = 0; i < ws.Count(); ++i)
                {
                    const auto& sensor = sensorCfg[i];

                    LOG_IF("SecurityOrchestrator",
                        "Sensor[%u] | %-24s | zone=%-10s | cmdArea=%d | statusArea=%d",
                        (unsigned)i,
                        sensor.name ? sensor.name : "?",
                        sensor.zone ? sensor.zone : "?",
                        sensor.cmdArea,
                        sensor.statusArea);
                }
            }

            LOG_IF("SecurityOrchestrator","-----------------------------------------------");
            LOG_IF("SecurityOrchestrator","SUMMARY");
            LOG_IF("SecurityOrchestrator","Sensors             : %u",(unsigned)ws.Count());
            LOG_IF("SecurityOrchestrator","Runtime zones       : %u",(unsigned)ws.GetZoneCount());
            LOG_IF("SecurityOrchestrator","Configured zones    : %u",(unsigned)securityCfg.zoneCount);
            LOG_IF("SecurityOrchestrator","Sensor cmd areas    : %u",(unsigned)commandAreaCount);
            LOG_IF("SecurityOrchestrator","Sensor status areas : %u",(unsigned)sensorStatusAreaCount);
            LOG_IF("SecurityOrchestrator","System status area  : %d",securityCfg.statusArea);
            LOG_IF("SecurityOrchestrator","Panel command area  : %d",securityCfg.panelCommandArea);
            LOG_IF("SecurityOrchestrator","Event area          : %d",securityCfg.eventArea);
            LOG_IF("SecurityOrchestrator","===============================================");
        }

        static void ReportSensors()
        {
            Header("           SECURITY ORCHESTRATOR SENSORS REPORT");

            if (!initialized)
            {
                LOG_IF("SecurityOrchestrator","Status : NOT INITIALIZED");
                Footer();
                return;
            }

            const auto* cfg = ws.GetConfig();

            if (!cfg)
            {
                LOG_IF("SecurityOrchestrator","ERROR: sensor configuration unavailable");
                Footer();
                return;
            }

            size_t enabled = 0, disabled = 0, engageRT = 0, disengageRT = 0;
            size_t engageH24 = 0, disengageH24 = 0, active = 0;
            size_t alarmRT = 0, alarmH24 = 0, memRT = 0, memH24 = 0;

            for (size_t i = 0; i < ws.Count(); ++i)
            {
                const auto& c = cfg[i];
                Sensor* s = ws.GetSensor(i);

                if (!s)
                {
                    LOG_IF("SecurityOrchestrator","Sensor %u | INVALID SENSOR POINTER",(unsigned)i);
                    continue;
                }

                const bool isEnabled = s->IsEnabled();
                const bool isEngagedRT = s->IsEngagedRT();
                const bool isEngagedH24 = s->IsEngagedH24();
                const bool isAlarmRT = s->Outputs().rt;
                const bool isAlarmH24 = s->Outputs().h24;
                const bool isAlarm = isAlarmRT || isAlarmH24;
                const bool isMemRT = s->Outputs().rtMem;
                const bool isMemH24 = s->Outputs().h24Mem;

                if (isEnabled) ++enabled; else ++disabled;
                if (isEngagedRT) ++engageRT; else ++disengageRT;
                if (isEngagedH24) ++engageH24; else ++disengageH24;
                if (isAlarm) ++active;
                if (isAlarmRT) ++alarmRT;
                if (isAlarmH24) ++alarmH24;
                if (isMemRT) ++memRT;
                if (isMemH24) ++memH24;

                LOG_IF("SecurityOrchestrator",
                    "Sensor %u | name=%s | zone=%s | category=%d | ENABLED=%s | ENGAGE_RT=%s | ENGAGE_H24=%s",
                    (unsigned)i,
                    c.name ? c.name : "?",
                    c.zone ? c.zone : "?",
                    static_cast<int>(c.category),
                    isEnabled?"YES":"NO",
                    isEngagedRT?"YES":"NO",
                    isEngagedH24?"YES":"NO");

                LOG_IF("SecurityOrchestrator",
                    "  outputs: RT=%s | H24=%s | ALARM=%s | RT_MEM=%s | H24_MEM=%s",
                    isAlarmRT?"YES":"NO",
                    isAlarmH24?"YES":"NO",
                    isAlarm?"YES":"NO",
                    isMemRT?"YES":"NO",
                    isMemH24?"YES":"NO");

                LOG_IF("SecurityOrchestrator",
                    "  cmdArea=%d | statusArea=%d",
                    c.cmdArea,c.statusArea);

                size_t channelIndex = 0;
                for (const auto& channel : c.channels)
                {
                    const SensorChannelType type = channel.type;
                    const SensorChannel* ch = s->Get(type);

                    if (!ch)
                    {
                        LOG_IF("SecurityOrchestrator",
                            "  channel[%u] type=%u | INVALID",
                            (unsigned)channelIndex,(unsigned)type);
                    }
                    else
                    {
                        LOG_IF("SecurityOrchestrator",
                            "  channel[%u] type=%u | ACTIVE=%s | ALARM=%s | INHIBIT=%s",
                            (unsigned)channelIndex,
                            (unsigned)type,
                            ch->IsActive()?"YES":"NO",
                            s->ChannelAlarm(type)?"YES":"NO",
                            ch->IsInhibit()?"YES":"NO");
                    }

                    ++channelIndex;
                }
            }

            LOG_IF("SecurityOrchestrator","-----------------------------------------------");
            LOG_IF("SecurityOrchestrator","Total sensors     : %u",(unsigned)ws.Count());
            LOG_IF("SecurityOrchestrator","Enabled sensors   : %u",(unsigned)enabled);
            LOG_IF("SecurityOrchestrator","Disabled sensors  : %u",(unsigned)disabled);
            LOG_IF("SecurityOrchestrator","ENGAGE RT         : %u",(unsigned)engageRT);
            LOG_IF("SecurityOrchestrator","DISENGAGE RT      : %u",(unsigned)disengageRT);
            LOG_IF("SecurityOrchestrator","ENGAGE H24        : %u",(unsigned)engageH24);
            LOG_IF("SecurityOrchestrator","DISENGAGE H24     : %u",(unsigned)disengageH24);
            LOG_IF("SecurityOrchestrator","Active sensors    : %u",(unsigned)active);
            LOG_IF("SecurityOrchestrator","Alarm RT          : %u",(unsigned)alarmRT);
            LOG_IF("SecurityOrchestrator","Alarm H24         : %u",(unsigned)alarmH24);
            LOG_IF("SecurityOrchestrator","Memory RT         : %u",(unsigned)memRT);
            LOG_IF("SecurityOrchestrator","Memory H24        : %u",(unsigned)memH24);
            Footer();
        }

        static void ReportZones()
        {
            Header("            SECURITY ORCHESTRATOR ZONES REPORT");

            if (!initialized)
            {
                LOG_IF("SecurityOrchestrator","Status : NOT INITIALIZED");
                Footer();
                return;
            }

            auto& zones = ws.Zones();
            const auto& zoneMap = ws.GetZoneMap();
            size_t alarmCount = 0;

            for (const auto& entry : zoneMap)
            {
                const std::string& zoneName = entry.first;
                const bool alarm = zones.ZoneAlarm(zoneName);

                if (alarm) ++alarmCount;

                LOG_IF("SecurityOrchestrator",
                    "%s -> %s",
                    zoneName.c_str(),
                    alarm ? "ALARM" : "OK");
            }

            LOG_IF("SecurityOrchestrator","Zone count  : %u",(unsigned)zoneMap.size());
            LOG_IF("SecurityOrchestrator","Alarm zones : %u",(unsigned)alarmCount);
            Footer();
        }

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

            if(!SecurityOrchestrator::isInitialized())
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

            if(!DomoManager::instance)
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

            auto& sensors =
                SecurityOrchestrator::getWiredSensors();

            const auto* sensorCfg =
                sensors.GetConfig();

            if(!sensorCfg)
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

            size_t commandAreaCount = 0;

            for(size_t i = 0;
                i < sensors.Count();
                ++i)
            {
                const auto& cfg =
                    sensorCfg[i];

                if(cfg.cmdArea < 0)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "Sensor %u | name=%s | zone=%s | cmdArea=DISABLED",
                        (unsigned)i,
                        cfg.name ? cfg.name : "?",
                        cfg.zone ? cfg.zone : "?"
                    );

                    continue;
                }

                ++commandAreaCount;

                const long value =
                    buffer.getValueFast(
                        cfg.cmdArea
                    );

                const bool enable =
                    (value & (1L << 0)) != 0;

                const bool engageRT =
                    (value & (1L << 1)) != 0;

                const bool engageH24 =
                    (value & (1L << 2)) != 0;

                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensor %u | name=%s | zone=%s | cmdArea=%d | value=0x%08lX",
                    (unsigned)i,
                    cfg.name ? cfg.name : "?",
                    cfg.zone ? cfg.zone : "?",
                    cfg.cmdArea,
                    (unsigned long)value
                );

                LOG_IF(
                    "SecurityOrchestrator",
                    "  ENABLE=%u | ENGAGE_RT=%u | ENGAGE_H24=%u",
                    enable ? 1 : 0,
                    engageRT ? 1 : 0,
                    engageH24 ? 1 : 0
                );

                const long reservedBits =
                    value & ~0x07L;

                if(reservedBits != 0)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "  WARNING: reserved command bits set: 0x%08lX",
                        (unsigned long)reservedBits
                    );
                }
            }

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

            if(panelCommandArea < 0)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "  panelCommandArea=DISABLED"
                );
            }
            else
            {
                const long value =
                    buffer.getValueFast(
                        panelCommandArea
                    );

                const uint8_t command =
                    static_cast<uint8_t>(value);

                LOG_IF(
                    "SecurityOrchestrator",
                    "  panelCommandArea=%d | value=0x%08lX | command=%u",
                    panelCommandArea,
                    (unsigned long)value,
                    (unsigned)command
                );

                switch(
                    static_cast<
                        SecurityOrchestrator::AlarmPanelCommand
                    >(command))
                {
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

        static void ReportSystem()
        {
            Header("           SECURITY ORCHESTRATOR SYSTEM REPORT");

            if (!initialized)
            {
                LOG_IF("SecurityOrchestrator","Status : NOT INITIALIZED");
                Footer();
                return;
            }

            system.Report();
            Footer();
        }

        static void ReportSecurity()
        {
            Header("         SECURITY ORCHESTRATOR SECURITY REPORT");

            if (!initialized)
            {
                LOG_IF("SecurityOrchestrator","Status : NOT INITIALIZED");
                Footer();
                return;
            }

            const auto* cfg = ws.GetConfig();
            const size_t sensorCount = ws.Count();
            size_t activeSensors = 0;
            size_t commandAreas = 0;

            if (cfg)
            {
                for (size_t i = 0; i < sensorCount; ++i)
                {
                    Sensor* s = ws.GetSensor(i);

                    if (s && (s->Outputs().rt || s->Outputs().h24))
                        ++activeSensors;

                    if (cfg[i].cmdArea >= 0)
                        ++commandAreas;
                }
            }

            auto& zones = ws.Zones();
            size_t activeZones = 0;

            for (const auto& entry : ws.GetZoneMap())
                if (zones.ZoneAlarm(entry.first))
                    ++activeZones;

            LOG_IF("SecurityOrchestrator","Initialized       : YES");
            LOG_IF("SecurityOrchestrator","Sensors           : %u",(unsigned)sensorCount);
            LOG_IF("SecurityOrchestrator","Active sensors    : %u",(unsigned)activeSensors);
            LOG_IF("SecurityOrchestrator","Zones             : %u",(unsigned)ws.GetZoneMap().size());
            LOG_IF("SecurityOrchestrator","Active zones      : %u",(unsigned)activeZones);
            LOG_IF("SecurityOrchestrator","Command areas     : %u",(unsigned)commandAreas);
            LOG_IF("SecurityOrchestrator","Intrusion         : %s",
                system.info.f[SystemManager::ALARM_INTRUSION].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Intrusion H24     : %s",
                system.info.f[SystemManager::ALARM_INTRUSION_H24].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Flood             : %s",
                system.info.f[SystemManager::ALARM_FLOOD].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Smoke             : %s",
                system.info.f[SystemManager::ALARM_SMOKE].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Windows open      : %s",
                system.info.f[SystemManager::WINDOWS_OPEN].get()?"YES":"NO");
            LOG_IF("SecurityOrchestrator","Doors open        : %s",
                system.info.f[SystemManager::DOORS_OPEN].get()?"YES":"NO");
            LOG_IF("SecurityOrchestrator","Tamper            : %s",
                system.info.f[SystemManager::ALARM_TAMPER].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Panic            : %s",
                system.info.f[SystemManager::ALARM_PANIC].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Camera            : %s",
                system.info.f[SystemManager::ALARM_CAMERA].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","System bitmask    : 0x%08X",(unsigned)system.getBitmask());

            Footer();
        }

        static void ReportInconsistencies()
        {
            Header("      SECURITY ORCHESTRATOR INCONSISTENCIES REPORT");

            bool found = false;

            if (!initialized)
            {
                LOG_IF("SecurityOrchestrator","WARNING: SecurityOrchestrator not initialized");
                found = true;
            }

            const auto* cfg = ws.GetConfig();

            if (initialized && !cfg)
            {
                LOG_IF("SecurityOrchestrator","ERROR: configuration pointer is NULL");
                found = true;
            }

            if (initialized && cfg)
            {
                for (size_t i = 0; i < ws.Count(); ++i)
                {
                    Sensor* s = ws.GetSensor(i);

                    if (!s)
                    {
                        LOG_IF("SecurityOrchestrator",
                            "ERROR: sensor %u has NULL runtime object",
                            (unsigned)i);
                        found = true;
                    }
                }

                for (size_t i = 0; i < ws.Count(); ++i)
                {
                    const auto& c = cfg[i];

                    if (!c.zone || strlen(c.zone) == 0)
                    {
                        LOG_IF("SecurityOrchestrator",
                            "ERROR: sensor %u has empty zone",
                            (unsigned)i);
                        found = true;
                    }

                    if (c.channels.size() == 0)
                    {
                        LOG_IF("SecurityOrchestrator",
                            "ERROR: sensor %u zone=%s has no channels",
                            (unsigned)i,c.zone ? c.zone : "?");
                        found = true;
                    }

                    if (c.readers.size() == 0)
                    {
                        LOG_IF("SecurityOrchestrator",
                            "ERROR: sensor %u zone=%s has no readers",
                            (unsigned)i,c.zone ? c.zone : "?");
                        found = true;
                    }

                    if (c.channels.size() != c.readers.size())
                    {
                        LOG_IF("SecurityOrchestrator",
                            "ERROR: sensor %u zone=%s channels=%u readers=%u",
                            (unsigned)i,
                            c.zone ? c.zone : "?",
                            (unsigned)c.channels.size(),
                            (unsigned)c.readers.size());
                        found = true;
                    }

                    for (const auto& ch : c.channels)
                    {
                        if (ch.pin < -1)
                        {
                            LOG_IF("SecurityOrchestrator",
                                "ERROR: sensor %u zone=%s invalid pin=%d",
                                (unsigned)i,
                                c.zone ? c.zone : "?",
                                ch.pin);
                            found = true;
                        }
                    }
                }

                for (size_t i = 0; i < ws.Count(); ++i)
                {
                    const int areaI = cfg[i].statusArea;
                    if (areaI < 0) continue;

                    for (size_t j = i + 1; j < ws.Count(); ++j)
                    {
                        const int areaJ = cfg[j].statusArea;
                        if (areaJ < 0) continue;

                        if (areaI == areaJ)
                        {
                            LOG_IF("SecurityOrchestrator",
                                "WARNING: duplicate status area %d used by sensors %u and %u",
                                areaI,(unsigned)i,(unsigned)j);
                            found = true;
                        }
                    }
                }

                for (size_t i = 0; i < ws.Count(); ++i)
                {
                    const int areaI = cfg[i].cmdArea;
                    if (areaI < 0) continue;

                    for (size_t j = i + 1; j < ws.Count(); ++j)
                    {
                        const int areaJ = cfg[j].cmdArea;
                        if (areaJ < 0) continue;

                        if (areaI == areaJ)
                        {
                            LOG_IF("SecurityOrchestrator",
                                "WARNING: duplicate command area %d used by sensors %u and %u",
                                areaI,(unsigned)i,(unsigned)j);
                            found = true;
                        }
                    }
                }
            }

            if (initialized)
            {
                const int bitmask = system.getBitmask();

                if (bitmask != 0)
                {
                    LOG_IF("SecurityOrchestrator",
                        "INFO: active system state bitmask 0x%08X",
                        (unsigned)bitmask);
                }
            }

            if (!found)
                LOG_IF("SecurityOrchestrator","No structural inconsistencies detected");

            Footer();
        }

        static void ReportAlarmBitmaskMap()
        {
            if (!initialized) return;
            alarmMask.ReportMap(ws);
        }

        static void ReportHmi();

        static void CoreReport()
        {
            Header("          SECURITY ORCHESTRATOR CORE REPORT");

            if (!initialized)
            {
                LOG_IF("SecurityOrchestrator","Status            : NOT INITIALIZED");
                Footer();
                return;
            }

            LOG_IF("SecurityOrchestrator","Initialized       : YES");
            LOG_IF("SecurityOrchestrator","Sensors           : %u",(unsigned)ws.Count());
            LOG_IF("SecurityOrchestrator","Zones             : %u",(unsigned)ws.GetZoneMap().size());
            LOG_IF("SecurityOrchestrator","System bitmask    : 0x%08X",(unsigned)system.getBitmask());
            LOG_IF("SecurityOrchestrator","Tamper            : %s",
                system.info.f[SystemManager::ALARM_TAMPER].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Panic            : %s",
                system.info.f[SystemManager::ALARM_PANIC].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Camera            : %s",
                system.info.f[SystemManager::ALARM_CAMERA].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Intrusion         : %s",
                system.info.f[SystemManager::ALARM_INTRUSION].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Intrusion H24     : %s",
                system.info.f[SystemManager::ALARM_INTRUSION_H24].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Flood             : %s",
                system.info.f[SystemManager::ALARM_FLOOD].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Smoke             : %s",
                system.info.f[SystemManager::ALARM_SMOKE].get()?"ACTIVE":"OK");
            LOG_IF("SecurityOrchestrator","Windows open      : %s",
                system.info.f[SystemManager::WINDOWS_OPEN].get()?"YES":"NO");
            LOG_IF("SecurityOrchestrator","Doors open        : %s",
                system.info.f[SystemManager::DOORS_OPEN].get()?"YES":"NO");
            LOG_IF("SecurityOrchestrator","Last loop at      : %lu",lastLoopAt);
            LOG_IF("SecurityOrchestrator","Last changes      : 0x%02X",(unsigned)lastChanges);
            LOG_IF("SecurityOrchestrator","  SENSOR          : %s",(lastChanges & CHANGE_SENSOR)?"YES":"NO");
            LOG_IF("SecurityOrchestrator","  ZONE            : %s",(lastChanges & CHANGE_ZONE)?"YES":"NO");
            LOG_IF("SecurityOrchestrator","  SYSTEM          : %s",(lastChanges & CHANGE_SYSTEM)?"YES":"NO");
            LOG_IF("SecurityOrchestrator","  PANEL           : %s",(lastChanges & CHANGE_PANEL)?"YES":"NO");
            LOG_IF("SecurityOrchestrator","  COMM            : %s",(lastChanges & CHANGE_COMM)?"YES":"NO");
            LOG_IF("SecurityOrchestrator","  COMMAND         : %s",(lastChanges & CHANGE_COMMAND)?"YES":"NO");

            LOG_IF("SecurityOrchestrator","--- SENSORS ---");
            ReportSensors();

            Footer();
        }

        static void ReportDiagnostics()
        {
            Header("        SECURITY ORCHESTRATOR DIAGNOSTICS");

            LOG_IF("SecurityOrchestrator","Initialized       : %s",initialized?"YES":"NO");
            LOG_IF("SecurityOrchestrator","Sensor count      : %u",(unsigned)ws.Count());
            LOG_IF("SecurityOrchestrator","Zone count        : %u",(unsigned)ws.GetZoneMap().size());
            LOG_IF("SecurityOrchestrator","System bitmask    : 0x%08X",(unsigned)system.getBitmask());
            LOG_IF("SecurityOrchestrator","Last loop at      : %lu",lastLoopAt);
            LOG_IF("SecurityOrchestrator","Last changes      : 0x%02X",(unsigned)lastChanges);

            LOG_IF("SecurityOrchestrator","--- CONFIGURATION ---");
            ReportConfig();

            LOG_IF("SecurityOrchestrator","--- SENSORS ---");
            ReportSensors();

            LOG_IF("SecurityOrchestrator","--- ZONES ---");
            ReportZones();

            LOG_IF("SecurityOrchestrator","--- COMMANDS ---");
            ReportCommands();

            LOG_IF("SecurityOrchestrator","--- SYSTEM ---");
            ReportSystem();

            LOG_IF("SecurityOrchestrator","--- SECURITY ---");
            ReportSecurity();

            LOG_IF("SecurityOrchestrator","--- INCONSISTENCIES ---");
            ReportInconsistencies();

            Footer();
        }

        static void FullReport()
        {
            Header("        SECURITY ORCHESTRATOR FULL REPORT");

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

            LOG_IF("SecurityOrchestrator","             END SECURITY REPORT");
            LOG_IF("SecurityOrchestrator","================================================");
        }
    };

    // ============================================================
    // UNIFIED REPORT DISPATCHER
    // ============================================================
    static void Report(ReportMode mode = ReportMode::FULL)
    {
        switch (mode)
        {
            case ReportMode::CORE:           Diagnostic::CoreReport(); break;
            case ReportMode::CONFIG:         Diagnostic::ReportConfig(); break;
            case ReportMode::SENSORS:        Diagnostic::ReportSensors(); break;
            case ReportMode::ZONES:          Diagnostic::ReportZones(); break;
            case ReportMode::COMMANDS:       Diagnostic::ReportCommands(); break;
            case ReportMode::SYSTEM:         Diagnostic::ReportSystem(); break;
            case ReportMode::SECURITY:       Diagnostic::ReportSecurity(); break;
            case ReportMode::HMI:            Diagnostic::ReportHmi(); break;
            case ReportMode::INCONSISTENCIES:Diagnostic::ReportInconsistencies(); break;
            case ReportMode::DIAGNOSTICS:    Diagnostic::ReportDiagnostics(); break;
            case ReportMode::FULL:
            default:                         Diagnostic::FullReport(); break;
        }
    }

    static void ReportAll() { Report(ReportMode::FULL); }
    static void ReportCoreOnly() { Report(ReportMode::CORE); }
    static void ReportConfigOnly() { Report(ReportMode::CONFIG); }
    static void ReportSensorsOnly() { Report(ReportMode::SENSORS); }
    static void ReportZonesOnly() { Report(ReportMode::ZONES); }
    static void ReportCommandsOnly() { Report(ReportMode::COMMANDS); }
    static void ReportSystemOnly() { Report(ReportMode::SYSTEM); }
    static void ReportSecurityOnly() { Report(ReportMode::SECURITY); }
    static void ReportInconsistenciesOnly() { Report(ReportMode::INCONSISTENCIES); }
    static void ReportDiagnosticsOnly() { Report(ReportMode::DIAGNOSTICS); }

    // ============================================================
    // ACCESSORS
    // ============================================================
    static SystemManager& getSystem() { return system; }
    static WiredSensorsManager& getWiredSensors() { return ws; }
    static IntrusionBoardFaultManager& getBoardFaults() { return boardFaults; }
    static bool isInitialized() { return initialized; }
    static uint8_t getLastChanges() { return lastChanges; }
    static unsigned long getLastLoopAt() { return lastLoopAt; }
    static int getPanelCommandArea() { return cfgCopy.panelCommandArea; }
    static const FrontendConfig::Security& getConfig() { return cfgCopy; }

    static bool IsSensorAnomaly(size_t sensorIndex)
    {
        const Sensor* sensor = ws.GetSensor(sensorIndex);

        if (!sensor || !sensor->IsEnabled())
            return false;

        return boardFaults.IsSensorAnomaly(sensorIndex);
    }

    static bool IsZoneAnomaly(size_t zoneIndex)
    {
        return boardFaults.IsZoneAnomaly(zoneIndex);
    }

    static uint64_t getCurrentAlarmMask() { return alarmMask.GetCurrentMask(); }
    static uint64_t getAlarmMemoryMask() { return alarmMask.GetMemoryMask(); }
    static bool HasUnsilencedMemory()
    {
        return alarmMask.HasUnsilencedMemory();
    }

    // --------------------------------------------------------
    // TACITAZIONE ALLARMI ATTUALI
    // --------------------------------------------------------

    static void SilenceCurrentAlarms()
    {
        alarmMask.SilenceCurrent();
    }

    // ============================================================
    // CALLBACKS
    // ============================================================
    static void RegisterCallbackAny(AlarmDispatcher::Callback cb)
    {
        ws.Zones().dispatcher.OnAnyAlarm(cb);
    }

    static void RegisterCallbackType(
        SensorChannelType type,
        AlarmDispatcher::Callback cb)
    {
        ws.Zones().dispatcher.OnAlarmType(type,cb);
    }

    static void RegisterCallbackZone(
        const std::string& zone,
        AlarmDispatcher::Callback cb)
    {
        ws.Zones().dispatcher.OnZoneAlarm(zone,cb);
    }

    static void RegisterCallbackZoneType(
        const std::string& zone,
        SensorChannelType type,
        AlarmDispatcher::Callback cb)
    {
        ws.Zones().dispatcher.OnZoneAlarmType(zone,type,cb);
    }
};

inline void AlarmPanelInterface::ReportInconsistencies()
{
    constexpr const char* TAG = "SecurityOrchestrator";
    LOG_IF(TAG, "===============================================");
    LOG_IF(TAG, "      SECURITY ORCHESTRATOR INCONSISTENCIES REPORT");
    LOG_IF(TAG, "===============================================");

    bool inconsistent = false;
    if (!SecurityOrchestrator::isInitialized())
    {
        LOG_IF(TAG, "WARNING: SecurityOrchestrator is not initialized");
        inconsistent = true;
    }

    const auto& ws = SecurityOrchestrator::getWiredSensors();
    const auto* sensorCfg = ws.GetConfig();
    const auto& securityCfg = SecurityOrchestrator::getConfig();
    const size_t sensorCount = ws.Count();

    // ============================================================
    // CONFIGURAZIONE SENSORI
    // ============================================================
    if (!sensorCfg)
    {
        LOG_IF(TAG, "ERROR: sensor configuration is null");
        inconsistent = true;
    }
    else
    {
        // --------------------------------------------------------
        // Validazione sensori + collisioni con aree globali
        // --------------------------------------------------------
        auto checkGlobalAreas = [&](size_t i, const char* name, int area, const char* field)
        {
            if (area < 0) return;

            if (securityCfg.statusArea >= 0 && area == securityCfg.statusArea)
            {
                LOG_IF(TAG, "WARNING: sensor[%u] '%s' %s=%d collides with security statusArea",
                       (unsigned)i, name, field, area);
                inconsistent = true;
            }
            if (securityCfg.eventArea >= 0 && area == securityCfg.eventArea)
            {
                LOG_IF(TAG, "WARNING: sensor[%u] '%s' %s=%d collides with security eventArea",
                       (unsigned)i, name, field, area);
                inconsistent = true;
            }
            if (securityCfg.panelCommandArea >= 0 && area == securityCfg.panelCommandArea)
            {
                LOG_IF(TAG, "WARNING: sensor[%u] '%s' %s=%d collides with panelCommandArea",
                       (unsigned)i, name, field, area);
                inconsistent = true;
            }
        };

        for (size_t i = 0; i < sensorCount; ++i)
        {
            const auto& cfg = sensorCfg[i];
            const char* name = cfg.name ? cfg.name : "<null>";

            if (!cfg.name || !cfg.name[0])
            {
                LOG_IF(TAG, "WARNING: sensor[%u] has invalid name", (unsigned)i);
                inconsistent = true;
            }

            if (!cfg.zone || !cfg.zone[0])
            {
                LOG_IF(TAG, "WARNING: sensor[%u] '%s' has no zone",
                       (unsigned)i, name);
                inconsistent = true;
            }
            else if (ws.GetZoneIndex(cfg.zone) < 0)
            {
                LOG_IF(TAG, "WARNING: sensor[%u] '%s' references unknown zone '%s'",
                       (unsigned)i, name, cfg.zone);
                inconsistent = true;
            }

            const size_t channels = cfg.channels.size();
            const size_t readers = cfg.readers.size();

            if (channels != readers)
            {
                LOG_IF(TAG, "WARNING: sensor[%u] '%s' channels=%u readers=%u",
                       (unsigned)i, name, (unsigned)channels, (unsigned)readers);
                inconsistent = true;
            }

            if (channels == 0)
            {
                LOG_IF(TAG, "WARNING: sensor[%u] '%s' has no channels",
                       (unsigned)i, name);
                inconsistent = true;
            }

            if (cfg.cmdArea < 0)
                LOG_IF(TAG, "INFO: sensor[%u] '%s' has no cmdArea",
                       (unsigned)i, name);

            if (cfg.statusArea < 0)
                LOG_IF(TAG, "INFO: sensor[%u] '%s' has no statusArea",
                       (unsigned)i, name);

            checkGlobalAreas(i, name, cfg.cmdArea, "cmdArea");
            checkGlobalAreas(i, name, cfg.statusArea, "statusArea");

            if (cfg.cmdArea >= 0 && cfg.statusArea >= 0 && cfg.cmdArea == cfg.statusArea)
            {
                LOG_IF(TAG, "WARNING: sensor[%u] '%s' cmdArea=%d collides with its statusArea",
                       (unsigned)i, name, cfg.cmdArea);
                inconsistent = true;
            }
        }

        // --------------------------------------------------------
        // Duplicati / collisioni tra sensori
        // Una sola scansione O(n²) invece di tre
        // --------------------------------------------------------
        for (size_t i = 0; i < sensorCount; ++i)
        {
            const auto& a = sensorCfg[i];

            if (a.cmdArea < 0 && a.statusArea < 0) continue;

            for (size_t j = i + 1; j < sensorCount; ++j)
            {
                const auto& b = sensorCfg[j];

                if (a.cmdArea >= 0 && a.cmdArea == b.cmdArea)
                {
                    LOG_IF(TAG, "WARNING: duplicate sensor cmdArea=%d used by sensors %u and %u",
                           a.cmdArea, (unsigned)i, (unsigned)j);
                    inconsistent = true;
                }

                if (a.statusArea >= 0 && a.statusArea == b.statusArea)
                {
                    LOG_IF(TAG, "WARNING: duplicate sensor statusArea=%d used by sensors %u and %u",
                           a.statusArea, (unsigned)i, (unsigned)j);
                    inconsistent = true;
                }
            }
        }
    }

    // ============================================================
    // ZONE CONFIGURATION
    // ============================================================
    const size_t zoneCount = securityCfg.zoneCount;

    if (zoneCount > 0 && !securityCfg.zones)
    {
        LOG_IF(TAG, "WARNING: zoneCount=%u but zone configuration is null",
               (unsigned)zoneCount);
        inconsistent = true;
    }
    else
    {
        // --------------------------------------------------------
        // Validazione zone + collisioni con aree globali
        // --------------------------------------------------------
        for (size_t i = 0; i < zoneCount; ++i)
        {
            const auto& zoneCfg = securityCfg.zones[i];

            if (!zoneCfg.name || !zoneCfg.name[0])
            {
                LOG_IF(TAG, "WARNING: configured zone[%u] has invalid name",
                       (unsigned)i);
                inconsistent = true;
                continue;
            }

            if (ws.GetZoneIndex(zoneCfg.name) < 0)
            {
                LOG_IF(TAG, "WARNING: configured zone '%s' does not exist in ZoneManager",
                       zoneCfg.name);
                inconsistent = true;
            }

            if (zoneCfg.statusArea < 0)
            {
                LOG_IF(TAG, "WARNING: zone '%s' has invalid statusArea=%d",
                       zoneCfg.name, zoneCfg.statusArea);
                inconsistent = true;
                continue;
            }

            if (securityCfg.statusArea >= 0 && zoneCfg.statusArea == securityCfg.statusArea)
            {
                LOG_IF(TAG, "WARNING: zone '%s' statusArea=%d collides with security statusArea",
                       zoneCfg.name, zoneCfg.statusArea);
                inconsistent = true;
            }

            if (securityCfg.eventArea >= 0 && zoneCfg.statusArea == securityCfg.eventArea)
            {
                LOG_IF(TAG, "WARNING: zone '%s' statusArea=%d collides with security eventArea",
                       zoneCfg.name, zoneCfg.statusArea);
                inconsistent = true;
            }

            if (securityCfg.panelCommandArea >= 0 &&
                zoneCfg.statusArea == securityCfg.panelCommandArea)
            {
                LOG_IF(TAG, "WARNING: zone '%s' statusArea=%d collides with panelCommandArea",
                       zoneCfg.name, zoneCfg.statusArea);
                inconsistent = true;
            }
        }

        // --------------------------------------------------------
        // Duplicati zone: nome + statusArea
        // --------------------------------------------------------
        for (size_t i = 0; i < zoneCount; ++i)
        {
            const auto& a = securityCfg.zones[i];
            if (!a.name) continue;

            for (size_t j = i + 1; j < zoneCount; ++j)
            {
                const auto& b = securityCfg.zones[j];
                if (!b.name) continue;

                if (std::strcmp(a.name, b.name) == 0)
                {
                    LOG_IF(TAG, "WARNING: duplicate zone configuration '%s'", a.name);
                    inconsistent = true;
                }

                if (a.statusArea >= 0 &&
                    b.statusArea >= 0 &&
                    a.statusArea == b.statusArea)
                {
                    LOG_IF(TAG, "WARNING: duplicate zone statusArea=%d used by '%s' and '%s'",
                           a.statusArea, a.name, b.name);
                    inconsistent = true;
                }
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
        LOG_IF(TAG, "WARNING: statusArea=%d collides with eventArea",
               securityCfg.statusArea);
        inconsistent = true;
    }

    if (securityCfg.statusArea >= 0 &&
        securityCfg.panelCommandArea >= 0 &&
        securityCfg.statusArea == securityCfg.panelCommandArea)
    {
        LOG_IF(TAG, "WARNING: statusArea=%d collides with panelCommandArea",
               securityCfg.statusArea);
        inconsistent = true;
    }

    if (securityCfg.eventArea >= 0 &&
        securityCfg.panelCommandArea >= 0 &&
        securityCfg.eventArea == securityCfg.panelCommandArea)
    {
        LOG_IF(TAG, "WARNING: eventArea=%d collides with panelCommandArea",
               securityCfg.eventArea);
        inconsistent = true;
    }

    // ============================================================
    // ZONE SHARING - VALID, NOT AN INCONSISTENCY
    // ============================================================
    /*
     * Più sensori nella stessa zona sono perfettamente validi.
     */

    // ============================================================
    // SYSTEM STATE
    // ============================================================
    const int systemMask = SecurityOrchestrator::getSystem().getBitmask();

    LOG_IF(TAG, "INFO: active system state bitmask 0x%08X",
           (unsigned)systemMask);

    // ============================================================
    // RESULT
    // ============================================================
    LOG_IF(TAG,
           inconsistent
               ? "WARNING: one or more inconsistencies detected"
               : "No inconsistencies detected");

    LOG_IF(TAG, "===============================================");
}

class DomoManagerAlarmPanel : public AlarmPanelInterface
{
private:
    // ============================================================
    // SILENCED ALARM MASK
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
    static AlarmType mapSensorAlarmType(SensorChannelType type)
    {
        switch(type)
        {
            case SensorChannelType::RT:   return AlarmType::INTRUSION;
            case SensorChannelType::H24:  return AlarmType::INTRUSION_H24;
            case SensorChannelType::LEN:  return AlarmType::TROUBLE;
            case SensorChannelType::MASK: return AlarmType::MASKING;
            default:                      return AlarmType::CUSTOM;
        }
    }

    // ============================================================
    // SENSOR CHANNEL + STATE -> EVENT TYPE
    // ============================================================
    static EventType mapSensorEventType(
        SensorChannelType type,
        bool active)
    {
        switch(type)
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
        const char* text = nullptr;

        switch(type)
        {
            case SensorChannelType::RT:
                text = active
                    ? "Intrusion alarm from zone "
                    : "Intrusion alarm restored from zone ";
                break;

            case SensorChannelType::H24:
                text = active
                    ? "24h intrusion alarm from zone "
                    : "24h intrusion alarm restored from zone ";
                break;

            case SensorChannelType::LEN:
                text = active
                    ? "Trouble from zone "
                    : "Trouble restored from zone ";
                break;

            case SensorChannelType::MASK:
                text = active
                    ? "Masking detected on zone "
                    : "Masking restored from zone ";
                break;

            default:
                text = active
                    ? "Alarm from zone "
                    : "Alarm restored from zone ";
                break;
        }

        std::string result;
        result.reserve(std::strlen(text) + zone.size());
        result = text;
        result += zone;
        return result;
    }

    // ============================================================
    // PARTITION VALIDATION
    // ============================================================
    static inline bool validPartition(int partition)
    {
        return partition == 0;
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

        SecurityOrchestrator::RegisterCallbackAny(
            [this](
                const std::string& zone,
                SensorChannelType type,
                const std::vector<Sensor*>& sensors,
                bool active)
            {
                (void)sensors;

                const int index = zoneIndex(zone);

                if(type == SensorChannelType::LEN &&
                   index >= 0)
                {
                    if(active)
                        activeTroubleZones.insert(index);
                    else
                        activeTroubleZones.erase(index);
                }

                Event ev;
                ev.category = EventCategory::ALARM;
                ev.alarmType = mapSensorAlarmType(type);
                ev.type = mapSensorEventType(type, active);
                ev.zone = index;
                ev.partition = 0;
                ev.description =
                    buildSensorDescription(zone, type, active);
                ev.timestamp = millis();

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
        const bool wasFault = commFault;
        const unsigned long now = millis();

        connected = true;
        commFault = false;
        lastPoll = now;
        lastCommunicationOk = now;

        if(wasFault)
            emitCommunicationRestored(
                now,
                "Communication restored on connect"
            );

        return true;
    }

    void disconnect() override
    {
        const unsigned long now = millis();

        connected = false;

        if(!commFault)
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
    // ============================================================
    void notifyCommunicationSuccess(
        unsigned long now,
        int latencyMs = DEFAULT_CHANNEL_LATENCY_MS)
    {
        const bool wasFault = commFault;

        connected = true;
        commFault = false;

        if(latencyMs >= 0)
            channelLatencyMs = latencyMs;

        lastCommunicationOk = now;

        if(wasFault)
            emitCommunicationRestored(
                now,
                "Communication restored"
            );
    }

    // ============================================================
    // COMMUNICATION FAULT
    // ============================================================
    void notifyCommunicationFault(
        unsigned long now,
        const char* description = "Communication fault")
    {
        if(commFault)
            return;

        commFault = true;

        emitCommunicationFault(
            now,
            description
        );
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

    bool getPathStatus(int pathIndex) const override
    {
        (void)pathIndex;
        return false;
    }

    // ============================================================
    // POLL
    // ============================================================
    bool poll(unsigned long now) override
    {
        lastPoll = now;
        lastChanges =
            SecurityOrchestrator::getLastChanges();

        const uint64_t currentMask =
            SecurityOrchestrator::getCurrentAlarmMask();

        synchronizeAlarmSilence(currentMask);

        if(lastChanges != SecurityOrchestrator::CHANGE_NONE)
            panelStateChanged = true;

        return lastChanges !=
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
        return validPartition(partition)
            ? armState
            : ArmState::UNKNOWN;
    }

    // ============================================================
    // READY
    // ============================================================
    bool isReady(int partition = 0) const override
    {
        if(!validPartition(partition))
            return false;

        const uint64_t currentMask =
            SecurityOrchestrator::getCurrentAlarmMask();

        return (currentMask & ~silencedAlarmMask) == 0;
    }

    // ============================================================
    // READY FOR ARM
    // ============================================================
    bool isReadyForArm(int partition = 0) const
    {
        if(!validPartition(partition))
            return false;

        auto& ws =
            SecurityOrchestrator::getWiredSensors();

        const auto state =
            ws.ComputeAggregate();

        // --------------------------------------------------------
        // PORTE / FINESTRE
        // --------------------------------------------------------

        if(state.normal.windowsOpen ||
        state.normal.doorsOpen)
        {
            return false;
        }

        // --------------------------------------------------------
        // AVARIE DEI SENSORI
        //
        // L'avaria blocca l'inserimento solo se il sensore è
        // abilitato e appartiene a una zona abilitata.
        //
        // Sensore disabilitato -> non blocca
        // Zona disabilitata    -> non blocca
        // --------------------------------------------------------

        const auto* cfg =
            ws.GetConfig();

        if(!cfg)
            return false;

        for(size_t i = 0;
            i < ws.Count();
            ++i)
        {
            const Sensor* sensor =
                ws.GetSensor(i);

            if(!sensor ||
            !sensor->IsEnabled())
            {
                continue;
            }

            if(!SecurityOrchestrator::IsSensorAnomaly(i))
                continue;

            const char* zone =
                cfg[i].zone;

            if(zone &&
            zone[0] &&
            !ws.Zones().IsZoneEnabled(zone))
            {
                continue;
            }

            return false;
        }

        // --------------------------------------------------------
        // AVARIE ZONA
        //
        // Una zona abilitata con anomalia blocca l'inserimento.
        // Una zona disabilitata viene ignorata.
        // --------------------------------------------------------

        for(size_t i = 0;
            i < ws.GetZoneCount();
            ++i)
        {
            const char* zone =
                ws.GetZoneName(i);

            if(!zone || !zone[0])
                continue;

            if(!ws.Zones().IsZoneEnabled(zone))
                continue;

            if(SecurityOrchestrator::IsZoneAnomaly(i))
                return false;
        }

        return true;
    }

private:
    // ============================================================
    // ARM COMMON
    // ============================================================
    bool arm(
        ArmState state,
        int partition,
        const char* description)
    {
        if(!validPartition(partition))
            return false;

        if(!isReadyForArm(partition))
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "ARM rejected: partition %d not ready for arm",
                partition
            );
            return false;
        }

        silencedAlarmMask = 0;
        armState = state;

        const unsigned long now = millis();

        SecurityOrchestrator::ForceSecurityCommands(
            state,
            now
        );

        emitPartitionChanged(
            partition,
            description,
            now
        );

        return true;
    }

public:
    // ============================================================
    // ARM AWAY / STAY / NIGHT
    // ============================================================
    bool armAway(int partition = 0) override
    {
        return arm(
            ArmState::ARMED_AWAY,
            partition,
            "ARM AWAY"
        );
    }

    bool armStay(int partition = 0) override
    {
        return arm(
            ArmState::ARMED_STAY,
            partition,
            "ARM STAY"
        );
    }

    bool armNight(int partition = 0) override
    {
        return arm(
            ArmState::ARMED_NIGHT,
            partition,
            "ARM NIGHT"
        );
    }

    // ============================================================
    // DISARM
    // ============================================================
    bool disarm(int partition = 0) override
    {
        if(!validPartition(partition))
            return false;

        armState = ArmState::DISARMED;

        const unsigned long now = millis();

        SecurityOrchestrator::ForceSecurityCommands(
            ArmState::DISARMED,
            now
        );

        emitPartitionChanged(
            partition,
            "DISARM",
            now
        );

        return true;
    }

    // ============================================================
    // SILENCE ALARM
    // ============================================================
    bool silenceAlarm(uint8_t partition)
    {
        // ----------------------------------------------------
        // Allarmi realtime attualmente presenti
        // ----------------------------------------------------

        const uint64_t currentMask =
            SecurityOrchestrator::getCurrentAlarmMask();

        // ----------------------------------------------------
        // Memorie allarme attualmente presenti
        // ----------------------------------------------------

        const uint64_t memoryMask =
            SecurityOrchestrator::getAlarmMemoryMask();

        // ----------------------------------------------------
        // Episodio complessivo:
        // realtime ancora attivi +
        // memorie già registrate
        // ----------------------------------------------------

        const uint64_t episodeMask =
            currentMask | memoryMask;

        // ----------------------------------------------------
        // Considera solo gli allarmi che non sono
        // già stati tacitati in precedenza.
        // ----------------------------------------------------

        const uint64_t unsilencedMask =
            episodeMask & ~silencedAlarmMask;

        // ----------------------------------------------------
        // Nessun nuovo allarme da tacitare.
        // ----------------------------------------------------

        if(unsilencedMask == 0)
            return false;

        // ----------------------------------------------------
        // Memorizza gli allarmi tacitati.
        //
        // Gli eventi già riconosciuti non dovranno
        // più riattivare la sirena fino a quando
        // non rientreranno completamente.
        // ----------------------------------------------------

        silencedAlarmMask |= unsilencedMask;

        // ----------------------------------------------------
        // Marca nel bitmask manager tutti gli eventi
        // attualmente attivi come "silenziati".
        //
        // Se il sensore rimane in allarme:
        //    RT = YES
        //
        // non verrà rigenerato un nuovo episodio.
        //
        // Quando il sensore rientra:
        //    RT = NO
        //
        // il silenziamento verrà automaticamente
        // eliminato dal relativo bit.
        // ----------------------------------------------------

        SecurityOrchestrator::SilenceCurrentAlarms();

        // ----------------------------------------------------
        // Cancella le memorie centrali associate
        // agli eventi appena riconosciuti.
        // ----------------------------------------------------

        SecurityOrchestrator::ResetAlarmMemory(
            unsilencedMask
        );

        (void)partition;

        return true;
    }

    void synchronizeAlarmSilence(uint64_t currentMask)
    {
        silencedAlarmMask &= currentMask;
    }

    // ============================================================
    // ZONE STATE
    // ============================================================
    bool getZoneState(int zone) const override
    {
        if(!validZone(zone))
            return false;

        const auto& ws =
            SecurityOrchestrator::getWiredSensors();

        const char* zoneName =
            ws.GetZoneName(static_cast<size_t>(zone));

        return zoneName &&
               ws.Zones().ZoneAlarm(zoneName);
    }

    // ============================================================
    // ZONE TAMPER
    // ============================================================
    bool getZoneTamper(int zone) const override
    {
        return validZone(zone) &&
               activeTamperZones.find(zone) !=
                   activeTamperZones.end();
    }

    // ============================================================
    // ZONE TROUBLE
    // ============================================================
    bool getZoneTrouble(int zone) const override
    {
        return validZone(zone) &&
               (
                   activeTroubleZones.find(zone) !=
                       activeTroubleZones.end() ||
                   SecurityOrchestrator::IsZoneAnomaly(
                       static_cast<size_t>(zone)
                   )
               );
    }

    bool isZoneBypassed(int zone) const override
    {
        return validZone(zone) &&
               bypassedZones.find(zone) !=
                   bypassedZones.end();
    }

    size_t getZoneCount() const override
    {
        return SecurityOrchestrator::getWiredSensors()
            .GetZoneCount();
    }

    // ============================================================
    // ZONE TAMPER NOTIFICATION
    // ============================================================
    void notifyZoneTamper(
        int zone,
        bool active,
        unsigned long now = 0)
    {
        if(!validZone(zone))
            return;

        if(active)
            activeTamperZones.insert(zone);
        else
            activeTamperZones.erase(zone);

        Event ev;

        ev.category = EventCategory::ALARM;
        ev.type = active
            ? EventType::TAMPER
            : EventType::TAMPER_RESTORED;
        ev.alarmType = AlarmType::TAMPER;
        ev.zone = zone;
        ev.partition = 0;
        ev.description = active
            ? "Tamper active"
            : "Tamper restored";
        ev.timestamp = now ? now : millis();

        emitEvent(ev);
    }

    // ============================================================
    // BYPASS
    // ============================================================
    bool bypassZone(int zone) override
    {
        if(!validZone(zone))
            return false;

        if(!bypassedZones.insert(zone).second)
            return true;

        Event ev;

        ev.category = EventCategory::APPLICATION;
        ev.type = EventType::ZONE_BYPASSED;
        ev.zone = zone;
        ev.partition = 0;
        ev.description = "Zone bypassed";
        ev.timestamp = millis();

        emitEvent(ev);
        return true;
    }

    bool clearBypass(int zone) override
    {
        if(!validZone(zone) ||
           bypassedZones.erase(zone) == 0)
        {
            return false;
        }

        Event ev;

        ev.category = EventCategory::APPLICATION;
        ev.type = EventType::ZONE_BYPASS_CLEARED;
        ev.zone = zone;
        ev.partition = 0;
        ev.description = "Zone bypass cleared";
        ev.timestamp = millis();

        emitEvent(ev);
        return true;
    }

    // ============================================================
    // GLOBAL TAMPER / TROUBLE
    // ============================================================
    bool getGlobalTamper() const override
    {
        return SecurityOrchestrator::getWiredSensors()
            .ComputeAggregate()
            .tamper;
    }

    bool getGlobalTrouble() const override
    {
        return !activeTroubleZones.empty();
    }

    // ============================================================
    // AUTHENTICATION PROVIDER
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
            std::move(provider);
    }

    void setPermissionProvider(
        PermissionProvider provider)
    {
        permissionProvider =
            std::move(provider);
    }

    // ============================================================
    // AUTHENTICATE USER
    // ============================================================
    bool authenticateUser(
        const std::string& user,
        const std::string& pin) override
    {
        const bool authenticated =
            authenticationProvider
                ? authenticationProvider(user, pin)
                : false;

        if(!authenticationProvider)
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "Authentication provider not configured"
            );
        }

        Event ev;

        ev.category = EventCategory::APPLICATION;
        ev.type = authenticated
            ? EventType::USER_AUTH_OK
            : EventType::USER_AUTH_FAIL;
        ev.zone = -1;
        ev.partition = -1;
        ev.user = user;
        ev.description = authenticated
            ? "Authentication successful"
            : "Authentication failed";
        ev.timestamp = millis();

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
        if(!permissionProvider)
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
        alarmCallback = std::move(cb);
    }

    void setEventCallback(
        EventCallback cb) override
    {
        callback = std::move(cb);
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

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Connected            : %s",
            connected ? "YES" : "NO"
        );

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Communication fault  : %s",
            commFault ? "ACTIVE" : "OK"
        );

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Arm state            : %s",
            armStateToString(armState)
        );

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Channel supervised   : %s",
            isChannelSupervised() ? "YES" : "NO"
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
            panelStateChanged ? "YES" : "NO"
        );

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

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Global tamper        : %s",
            getGlobalTamper() ? "ACTIVE" : "OK"
        );

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Global trouble       : %s",
            getGlobalTrouble() ? "ACTIVE" : "OK"
        );

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Ready                : %s",
            isReady(0) ? "YES" : "NO"
        );

        LOG_IF(
            "DomoManagerAlarmPanel",
            "Event log count      : %u / %u",
            (unsigned)eventLog.size(),
            (unsigned)EVENT_LOG_MAX
        );

        SecurityOrchestrator::Report(
            SecurityOrchestrator::ReportMode::CORE
        );

        SecurityOrchestrator::Report(
            SecurityOrchestrator::ReportMode::SECURITY
        );

        SecurityOrchestrator::Report(
            SecurityOrchestrator::ReportMode::INCONSISTENCIES
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
        return SecurityOrchestrator::getSystem()
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
    // ============================================================
    uint64_t getEffectiveAlarmMask() const
    {
        const uint64_t episodeMask =
            SecurityOrchestrator::getCurrentAlarmMask() |
            SecurityOrchestrator::getAlarmMemoryMask();

        return episodeMask & ~silencedAlarmMask;
    }

private:
    // ============================================================
    // RUNTIME STATE
    // ============================================================
    bool connected = false;
    bool commFault = false;

    ArmState armState = ArmState::DISARMED;

    unsigned long lastPoll = 0;
    unsigned long lastCommunicationOk = 0;
    int channelLatencyMs =
        DEFAULT_CHANNEL_LATENCY_MS;

    uint8_t lastChanges =
        SecurityOrchestrator::CHANGE_NONE;

    bool panelStateChanged = false;

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
    AuthenticationProvider authenticationProvider = nullptr;
    PermissionProvider permissionProvider = nullptr;

    // ============================================================
    // EVENT LOG
    // ============================================================
    std::vector<Event> eventLog;

    // ============================================================
    // VALID ZONE
    // ============================================================
    inline bool validZone(int zone) const
    {
        return zone >= 0 &&
               static_cast<size_t>(zone) < getZoneCount();
    }

    // ============================================================
    // ZONE NAME -> INDEX
    // Usa la cache di WiredSensorsManager.
    // ============================================================
    inline int zoneIndex(const std::string& zoneName) const
    {
        return SecurityOrchestrator::getWiredSensors()
            .GetZoneIndex(zoneName.c_str());
    }

    // ============================================================
    // EVENT LOG APPEND
    // ============================================================
    void appendEvent(const Event& ev)
    {
        if(eventLog.size() >= EVENT_LOG_MAX)
            eventLog.erase(eventLog.begin());

        eventLog.push_back(ev);
    }

    // ============================================================
    // EVENT DISPATCH
    // ============================================================
    void emitEvent(const Event& ev)
    {
        appendEvent(ev);

        if(ev.category == EventCategory::ALARM)
        {
            if(alarmCallback)
                alarmCallback(ev);

            return;
        }

        if(callback)
            callback(ev);
    }

    // ============================================================
    // COMMUNICATION EVENTS
    // ============================================================
    void emitCommunicationFault(
        unsigned long now,
        const char* description)
    {
        Event ev;

        ev.category = EventCategory::ALARM;
        ev.type = EventType::COMM_FAULT;
        ev.alarmType = AlarmType::COMMUNICATION;
        ev.zone = -1;
        ev.partition = -1;
        ev.description =
            description
                ? description
                : "Communication fault";
        ev.timestamp = now;

        emitEvent(ev);
    }

    void emitCommunicationRestored(
        unsigned long now,
        const char* description)
    {
        Event ev;

        ev.category = EventCategory::ALARM;
        ev.type = EventType::COMM_RESTORED;
        ev.alarmType = AlarmType::COMMUNICATION;
        ev.zone = -1;
        ev.partition = -1;
        ev.description =
            description
                ? description
                : "Communication restored";
        ev.timestamp = now;

        emitEvent(ev);
    }

    // ============================================================
    // PARTITION EVENT
    // ============================================================
    void emitPartitionChanged(
        int partition,
        const char* description,
        unsigned long now = 0)
    {
        Event ev;

        ev.category = EventCategory::APPLICATION;
        ev.type = EventType::PARTITION_CHANGED;
        ev.zone = -1;
        ev.partition = partition;
        ev.description =
            description
                ? description
                : "Partition state changed";
        ev.timestamp = now ? now : millis();

        emitEvent(ev);
    }

    // ============================================================
    // ARM STATE -> STRING
    // ============================================================
    static const char* armStateToString(
        ArmState state)
    {
        switch(state)
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
    if(!initialized)
        return false;

    const int panelArea =
        cfgCopy.panelCommandArea;

    if(panelArea < 0 ||
       area != panelArea)
    {
        return false;
    }

    if(value < static_cast<long>(DISARM) ||
       value > static_cast<long>(ARM_NIGHT))
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: invalid command=%ld area=%d",
            value,
            area
        );
        return false;
    }

    DomoManagerAlarmPanel& panel =
        DomoManagerAlarmPanel::instance();

    bool result = false;

    switch(static_cast<AlarmPanelCommand>(value))
    {
        case DISARM:
            result = panel.disarm(0);
            break;

        case ARM_AWAY:
            result = panel.armAway(0);
            break;

        case ARM_STAY:
            result = panel.armStay(0);
            break;

        case ARM_NIGHT:
            result = panel.armNight(0);
            break;

        default:
            return false;
    }

    if(!result)
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: command failed area=%d value=%ld",
            area,
            value
        );
    }

    return result;
}

inline bool SecurityOrchestrator::ApplyPanelBitCommand(
    int area,
    long value)
{
    if(!initialized)
        return false;

    const int panelArea =
        cfgCopy.panelBitCommandArea;

    if(panelArea < 0 ||
       area != panelArea)
    {
        return false;
    }

    if(value < 0)
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelBitCommand: invalid value=%ld area=%d",
            value,
            area
        );
        return false;
    }

    const uint32_t bits =
        static_cast<uint32_t>(value);

    const uint32_t silenceMask =
        (1UL << BITCMD_SILENCE_ALARM);

    const uint32_t reservedBits =
        bits & ~silenceMask;

    if(reservedBits != 0)
    {
        LOG_WF(
            "SecurityOrchestrator",
            "ApplyPanelBitCommand: reserved bits "
            "area=%d value=0x%08lX reserved=0x%08lX",
            area,
            (unsigned long)bits,
            (unsigned long)reservedBits
        );
    }

    if(bits & silenceMask)
    {
        const bool result =
            DomoManagerAlarmPanel::instance()
                .silenceAlarm(0);

        if(!result)
        {
            LOG_WF(
                "SecurityOrchestrator",
                "ApplyPanelBitCommand: SILENCE_ALARM "
                "area=%d no active alarm",
                area
            );
        }
    }

    return true;
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

        int cmdArea = -1;

        bool enabled = false;
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
        out.valid = true;
        out.index = sensorIndex;
        out.name = c.name;
        out.zone = c.zone;
        out.category = c.category;
        out.cmdArea = c.cmdArea;

        out.enabled =
            sensor->IsEnabled();

        out.engagedRT =
            sensor->IsEngagedRT();

        out.engagedH24 =
            sensor->IsEngagedH24();

        out.alarmOut =
            sensor->Outputs().rt ||
            sensor->Outputs().h24;

        out.rtMem =
            sensor->Outputs().rtMem;

        out.h24Mem =
            sensor->Outputs().h24Mem;

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

        if (sensor->IsEnabled())
        {
            const bool sensorAnomaly =
                SecurityOrchestrator::IsSensorAnomaly(
                    sensorIndex);

            if (sensorAnomaly)
            {
                const size_t len =
                    static_cast<size_t>(
                        SensorChannelType::LEN);

                out.active[len] = true;
                out.alarm[len] = true;
            }
        }

        return true;
    }


    // ============================================================
    // SENSOR COMMAND
    //
    // bit 0 = ENABLE
    // bit 1 = ENGAGE RT
    // bit 2 = ENGAGE H24
    // ============================================================

    bool setSensor(
        size_t sensorIndex,
        bool enable,
        bool engageRT,
        bool engageH24)
    {
        auto& sensors =
            SecurityOrchestrator::getWiredSensors();

        if (sensorIndex >= sensors.Count())
            return false;

        const auto* cfg =
            sensors.GetConfig();

        if (!cfg)
            return false;

        const int area =
            cfg[sensorIndex].cmdArea;

        if (area < 0)
            return false;

        long value = 0;

        bitWrite(value, 0, enable);
        bitWrite(value, 1, engageRT);
        bitWrite(value, 2, engageH24);

        return SecurityOrchestrator::ApplySecurityCommand(
            area,
            value);
    }


    bool enableSensor(
        size_t sensorIndex,
        bool enable)
    {
        SensorState state;

        if (!getSensorState(sensorIndex, state))
            return false;

        return setSensor(
            sensorIndex,
            enable,
            state.engagedRT,
            state.engagedH24);
    }


    bool engageRTSensor(
        size_t sensorIndex,
        bool engage)
    {
        SensorState state;

        if (!getSensorState(sensorIndex, state))
            return false;

        return setSensor(
            sensorIndex,
            state.enabled,
            engage,
            state.engagedH24);
    }


    bool engageH24Sensor(
        size_t sensorIndex,
        bool engage)
    {
        SensorState state;

        if (!getSensorState(sensorIndex, state))
            return false;

        return setSensor(
            sensorIndex,
            state.enabled,
            state.engagedRT,
            engage);
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
        out = ZoneState{};

        const auto& ws =
            SecurityOrchestrator::
                getWiredSensors();

        const char* zoneName =
            ws.GetZoneName(zoneIndex);

        if (!zoneName)
            return false;

        out.valid = true;
        out.index = zoneIndex;
        out.name = zoneName;

        const auto& zones =
            ws.Zones();

        out.enabled =
            zones.IsZoneEnabled(zoneName);

        // --------------------------------------------------------
        // Recupera cmdArea dalla configurazione SecurityZoneConfig
        // --------------------------------------------------------

        const FrontendConfig::Security& cfg =
            SecurityOrchestrator::getConfig();

        for (size_t i = 0;
            i < cfg.zoneCount;
            ++i)
        {
            const auto& zoneCfg =
                cfg.zones[i];

            if (!zoneCfg.name)
                continue;

            if (strcmp(
                    zoneCfg.name,
                    zoneName) == 0)
            {
                out.cmdArea =
                    zoneCfg.cmdArea;

                break;
            }
        }

        out.alarm =
            zones.ZoneAlarmByType(
                zoneName,
                SensorChannelType::RT);

        out.alarmH24 =
            zones.ZoneAlarmByType(
                zoneName,
                SensorChannelType::H24);

        out.trouble =
            DomoManagerAlarmPanel::
                instance()
                .getZoneTrouble(
                    static_cast<int>(zoneIndex));

        out.bypassed =
            !out.enabled;

        const std::vector<Sensor*>& zoneSensors =
            zones.GetZone(zoneName);

        bool allDisabled =
            zoneSensors.size() > 0;

        for (Sensor* sensor : zoneSensors)
        {
            if (!sensor)
                continue;

            if (sensor->IsEnabled())
            {
                allDisabled = false;
                break;
            }
        }

        if (allDisabled)
            out.bypassed = true;

        for (Sensor* sensor : zoneSensors)
        {
            if (!sensor ||
                !sensor->IsEnabled())
            {
                continue;
            }

            const Sensor::SensorOutputs& state =
                sensor->Outputs();

            if (state.rtMem)
                out.rtMem = true;

            if (state.h24Mem)
                out.h24Mem = true;
        }

        return true;
    }


    // ============================================================
    // ZONE ENABLE / DISABLE COMMAND
    //
    // Il comando arriva dall'HMI tramite cmdArea configurata
    // in FrontendConfig::Security::zones[].
    // ============================================================

   bool setZoneEnabled(
        size_t zoneIndex,
        bool enable)
    {
        const auto& ws =
            SecurityOrchestrator::
                getWiredSensors();

        const char* zoneName =
            ws.GetZoneName(zoneIndex);

        if (!zoneName)
            return false;

        const FrontendConfig::Security& cfg =
            SecurityOrchestrator::getConfig();

        for(size_t i = 0;
            i < cfg.zoneCount;
            ++i)
        {
            const auto& zoneCfg =
                cfg.zones[i];

            if(!zoneCfg.name)
                continue;

            if(strcmp(
                    zoneCfg.name,
                    zoneName) != 0)
            {
                continue;
            }

            if(zoneCfg.cmdArea < 0)
                return false;

            return SecurityOrchestrator::
                ApplySecurityCommand(
                    zoneCfg.cmdArea,
                    enable ? 1L : 0L);
        }

        return false;
    }


    bool disableZone(
        size_t zoneIndex)
    {
        return setZoneEnabled(
            zoneIndex,
            false);
    }


    bool enableZone(
        size_t zoneIndex)
    {
        return setZoneEnabled(
            zoneIndex,
            true);
    }


    // ============================================================
    // ZONE BYPASS
    // ============================================================

    bool bypassZone(
        size_t zoneIndex)
    {
        return DomoManagerAlarmPanel::
            instance()
            .bypassZone(
                static_cast<int>(zoneIndex));
    }


    bool clearZoneBypass(
        size_t zoneIndex)
    {
        return DomoManagerAlarmPanel::
            instance()
            .clearBypass(
                static_cast<int>(zoneIndex));
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

        const uint32_t securityMask =
            static_cast<uint32_t>(
                SecurityOrchestrator::getSystem()
                    .getBitmask());

        const bool securityTrouble =
            (securityMask &
             (1u <<
              SecurityOrchestrator::
                  SystemManager::
                  GLOBAL_TROUBLE)) != 0;

        out.globalTrouble =
            panel.getGlobalTrouble() ||
            securityTrouble;

        out.systemBitmask =
            static_cast<uint32_t>(
                panel.getSystemBitmask())
            |
            securityMask;

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
                    partition);

            case SecurityOrchestrator::
                AlarmPanelCommand::ARM_STAY:

                return commandPanel(
                    AlarmPanelInterface::
                        ArmState::ARMED_STAY,
                    partition);

            case SecurityOrchestrator::
                AlarmPanelCommand::ARM_NIGHT:

                return commandPanel(
                    AlarmPanelInterface::
                        ArmState::ARMED_NIGHT,
                    partition);

            case SecurityOrchestrator::
                AlarmPanelCommand::DISARM:

                return commandPanel(
                    AlarmPanelInterface::
                        ArmState::DISARMED,
                    partition);

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

        // --------------------------------------------------------
        // DECODED SECURITY SYSTEM FLAGS
        // --------------------------------------------------------

        const bool cameraAlarm =
            (
                state.systemBitmask &
                (
                    uint32_t(1) <<
                    SecurityOrchestrator::
                        SystemManager::
                        ALARM_CAMERA
                )
            ) != 0;

        LOG_IF(
            "SecurityOrchestrator",
            "  cameraAlarm=%u",
            cameraAlarm ? 1 : 0
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