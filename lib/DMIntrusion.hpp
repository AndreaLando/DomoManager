
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
                reportInconsistencies();
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

    void reportInconsistencies() const
    {
        reportHeader(
            "        ALARM PANEL INCONSISTENCIES REPORT"
        );


        bool found = false;


        // --------------------------------------------------------
        // 1. Connected ma communication fault
        //
        // Non sempre è un errore: dipende dal driver.
        // Viene quindi riportato come WARNING.
        // --------------------------------------------------------

        if (isConnected() &&
            isCommunicationFault())
        {
            LOG_IF(
                "AlarmPanelInterface",
                "WARNING: connected=YES while communication fault=ACTIVE"
            );

            found = true;
        }


        // --------------------------------------------------------
        // 2. Canale supervisionato ma non connesso
        //
        // Può essere temporaneo, quindi WARNING.
        // --------------------------------------------------------

        if (!isConnected() &&
            isChannelSupervised() &&
            !isCommunicationFault())
        {
            LOG_IF(
                "AlarmPanelInterface",
                "WARNING: channel supervised but panel is disconnected"
            );

            found = true;
        }


        // --------------------------------------------------------
        // 3. Dual path con entrambe le path DOWN
        // --------------------------------------------------------

        if (hasDualPath())
        {
            const bool path0 =
                getPathStatus(0);

            const bool path1 =
                getPathStatus(1);


            if (!path0 &&
                !path1 &&
                !isCommunicationFault())
            {
                LOG_IF(
                    "AlarmPanelInterface",
                    "WARNING: dual path active but both paths are DOWN while no communication fault is reported"
                );

                found = true;
            }
        }


        // --------------------------------------------------------
        // 4. Latency negativa
        //
        // Valore generalmente non valido.
        // --------------------------------------------------------

        if (getChannelLatencyMs() < 0)
        {
            LOG_IF(
                "AlarmPanelInterface",
                "ERROR: invalid negative channel latency"
            );

            found = true;
        }


        // --------------------------------------------------------
        // 5. Partizioni in ARM state UNKNOWN
        // --------------------------------------------------------

        const size_t partitionCount =
            getPartitionCount();


        for (size_t p = 0; p < partitionCount; ++p)
        {
            const int partition =
                static_cast<int>(p);


            if (getArmState(partition) ==
                ArmState::UNKNOWN)
            {
                LOG_IF(
                    "AlarmPanelInterface",
                    "WARNING: partition %d has UNKNOWN arm state",
                    partition
                );

                found = true;
            }
        }


        // --------------------------------------------------------
        // 6. Zone con informazioni anomale? NON assumiamo che
        // tamper/trouble/state siano mutuamente esclusivi.
        //
        // Non facciamo quindi controlli arbitrari.
        // --------------------------------------------------------


        if (!found)
        {
            LOG_IF(
                "AlarmPanelInterface",
                "No generic inconsistencies detected"
            );
        }


        reportFooter();
    }


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

        reportInconsistencies();


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
        NONE = 0,
        ARM_AWAY,
        ARM_STAY,
        ARM_NIGHT,
        DISARM
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
        if (!cfg || cfg->count == 0)
        {
            LOG_IF(
                "WiredSensors",
                "No wired sensors configured -> skipping validation"
            );

            return true;
        }


        for (size_t i = 0; i < cfg->count; ++i)
        {
            const auto& s =
                cfg->sensors[i];


            // ----------------------------------------------------
            // ZONE
            // ----------------------------------------------------

            if (!s.zone ||
                strlen(s.zone) == 0)
            {
                LOG_EF(
                    "WiredSensors",
                    "Sensor %u: invalid zone",
                    (unsigned)i
                );

                return false;
            }


            // ----------------------------------------------------
            // CHANNELS / READERS
            // ----------------------------------------------------

            if (s.channels.size() == 0 ||
                s.readers.size() == 0)
            {
                LOG_EF(
                    "WiredSensors",
                    "Sensor %u zone=%s: missing channels/readers",
                    (unsigned)i,
                    s.zone
                );

                return false;
            }


            if (s.readers.size() !=
                s.channels.size())
            {
                LOG_EF(
                    "WiredSensors",
                    "Sensor %u zone=%s: readers=%u but channels=%u",
                    (unsigned)i,
                    s.zone,
                    (unsigned)s.readers.size(),
                    (unsigned)s.channels.size()
                );

                return false;
            }


            // ----------------------------------------------------
            // CHANNEL TYPE UNIQUENESS
            // ----------------------------------------------------

            std::vector<SensorChannelType> seenTypes;


            for (const auto& ch :
                 s.channels)
            {
                if (ch.pin < -1)
                {
                    LOG_EF(
                        "WiredSensors",
                        "Sensor %u zone=%s: invalid pin=%d",
                        (unsigned)i,
                        s.zone,
                        ch.pin
                    );

                    return false;
                }


                if (std::find(
                        seenTypes.begin(),
                        seenTypes.end(),
                        ch.type) != seenTypes.end())
                {
                    LOG_EF(
                        "WiredSensors",
                        "Sensor %u zone=%s: duplicate channel type",
                        (unsigned)i,
                        s.zone
                    );

                    return false;
                }


                seenTypes.push_back(ch.type);
            }
        }


        return true;
    }


public:

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

        ws.Init(
            cfg->sensors,
            cfg->count,
            cfg->startupInhibitMs
        );

        alarmMask.BuildMap(ws);

        // Engage sempre attivo:
        // non serve rieseguirlo ad ogni Loop().
        alarmMask.SetEngage(true);

        ws.Zones().EnableAll(true);
        ws.Zones().EngageAll(true);

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
                LOG_IF(
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
            "Setup: sensors=%u startupInhibit=%u ms",
            (unsigned)cfg->count,
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

        if (ws.Process(now))
        {
            changes |=
                CHANGE_SENSOR;
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


            // ----------------------------------------------------
            // COMMAND DECODE
            // ----------------------------------------------------

            const bool enable =
                bitRead(value, 0);


            const bool engage =
                bitRead(value, 1);


            // ----------------------------------------------------
            // APPLY
            // ----------------------------------------------------

            s->Enable(enable);

            s->Engage(engage);


            LOG_IF(
                "SecurityOrchestrator",
                "ApplySecurityCommand: "
                "sensor[%u] zone=%s enable=%d engage=%d area=%d",
                (unsigned)i,
                c.zone ? c.zone : "?",
                enable ? 1 : 0,
                engage ? 1 : 0,
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
        bool engage,
        unsigned long now)
    {
        auto* dm =
            DomoManager::instance;


        if (!dm)
            return;


        auto& buffer =
            dm->getBuffer();


        const auto* cfg =
            ws.GetConfig();


        if (!cfg)
            return;


        for (size_t i = 0;
             i < ws.Count();
             ++i)
        {
            const auto& c =
                cfg[i];


            Sensor* s =
                ws.GetSensor(i);


            if (!s)
                continue;


            if (c.cmdArea < 0 ||
                c.cmdArea >= buffer.size())
            {
                continue;
            }


            long value =
                buffer.getValueFast(c.cmdArea);


            value =
                bitWrite(
                    value,
                    1,
                    engage
                );


            dm->forceInternalEvent(
                c.cmdArea,
                value
            );


            s->Engage(engage);


            LOG_IF(
                "SecurityOrchestrator",
                "ForceSecurityCommands: "
                "sensor[%u] engage=%d written to area=%d "
                "at=%lu",
                (unsigned)i,
                engage ? 1 : 0,
                c.cmdArea,
                now
            );
        }
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
            Header(
                "        SECURITY ORCHESTRATOR CONFIG REPORT"
            );


            if (!initialized)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "Status             : NOT INITIALIZED"
                );

                Footer();
                return;
            }


            LOG_IF(
                "SecurityOrchestrator",
                "Sensors configured : %u",
                (unsigned)ws.Count()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Startup inhibit    : %u ms",
                (unsigned)cfgCopy.startupInhibitMs
            );


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


            for (size_t i = 0;
                 i < ws.Count();
                 ++i)
            {
                const auto& c =
                    cfg[i];


                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensor %u | "
                    "name=%s | zone=%s | category=%d | "
                    "cmdArea=%d | channels=%u | readers=%u",
                    (unsigned)i,
                    c.name ? c.name : "?",
                    c.zone ? c.zone : "?",
                    static_cast<int>(c.category),
                    c.cmdArea,
                    (unsigned)c.channels.size(),
                    (unsigned)c.readers.size()
                );


                // ------------------------------------------------
                // CHANNELS
                // ------------------------------------------------

                size_t channelIndex = 0;

                for (const auto& ch : c.channels)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "   channel[%u] type=%u pin=%d",
                        (unsigned)channelIndex,
                        (unsigned)ch.type,
                        ch.pin
                    );

                    ++channelIndex;
                }


                // ------------------------------------------------
                // READERS
                // ------------------------------------------------

                size_t readerIndex = 0;

                for (const auto& reader : c.readers)
                {
                    (void)reader;

                    LOG_IF(
                        "SecurityOrchestrator",
                        "   reader[%u] configured",
                        (unsigned)readerIndex
                    );

                    ++readerIndex;
                }
            }


            Footer();
        }


        // ========================================================
        // SENSORS
        // ========================================================

        static void ReportSensors()
        {
            Header(
                "           SECURITY ORCHESTRATOR SENSORS REPORT"
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


            if (!cfg)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "ERROR: sensor configuration unavailable"
                );

                Footer();
                return;
            }


            size_t active =
                0;


            for (size_t i = 0;
                 i < ws.Count();
                 ++i)
            {
                const auto& c =
                    cfg[i];


                Sensor* s =
                    ws.GetSensor(i);


                if (!s)
                {
                    LOG_IF(
                        "SecurityOrchestrator",
                        "Sensor %u | INVALID SENSOR POINTER",
                        (unsigned)i
                    );

                    continue;
                }


                if (s->alarmOut)
                    ++active;


                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensor %u | "
                    "name=%s | zone=%s | category=%d | alarmOut=%s",
                    (unsigned)i,
                    c.name ? c.name : "?",
                    c.zone ? c.zone : "?",
                    static_cast<int>(c.category),
                    s->alarmOut ? "YES" : "NO"
                );
            }


            LOG_IF(
                "SecurityOrchestrator",
                "Total sensors : %u",
                (unsigned)ws.Count()
            );


            LOG_IF(
                "SecurityOrchestrator",
                "Active sensors: %u",
                (unsigned)active
            );


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
            Header(
                "          SECURITY ORCHESTRATOR COMMANDS REPORT"
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


            if (!cfg)
            {
                LOG_IF(
                    "SecurityOrchestrator",
                    "ERROR: sensor configuration unavailable"
                );

                Footer();
                return;
            }


            size_t commandCount =
                0;


            for (size_t i = 0;
                 i < ws.Count();
                 ++i)
            {
                const auto& c =
                    cfg[i];


                if (c.cmdArea < 0)
                    continue;


                ++commandCount;


                LOG_IF(
                    "SecurityOrchestrator",
                    "Sensor %u | "
                    "name=%s | zone=%s | cmdArea=%d",
                    (unsigned)i,
                    c.name ? c.name : "?",
                    c.zone ? c.zone : "?",
                    c.cmdArea
                );


                LOG_IF(
                    "SecurityOrchestrator",
                    "   bit0=ENABLE bit1=ENGAGE"
                );
            }


            LOG_IF(
                "SecurityOrchestrator",
                "Command areas: %u",
                (unsigned)commandCount
            );


            Footer();
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
                        s->alarmOut)
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


                // ------------------------------------------------
                // DUPLICATE ZONES
                // ------------------------------------------------

                for (size_t i = 0;
                     i < ws.Count();
                     ++i)
                {
                    if (!cfg[i].zone)
                        continue;


                    for (size_t j = i + 1;
                         j < ws.Count();
                         ++j)
                    {
                        if (!cfg[j].zone)
                            continue;


                        if (strcmp(
                                cfg[i].zone,
                                cfg[j].zone) == 0)
                        {
                            LOG_IF(
                                "SecurityOrchestrator",
                                "WARNING: duplicate zone '%s' "
                                "used by sensors %u and %u",
                                cfg[i].zone,
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

#pragma once

#include <Arduino.h>

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>


class DomoManagerAlarmPanel : public AlarmPanelInterface
{
private:

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


        // ========================================================
        // SECURITY ORCHESTRATOR -> ALARM PANEL
        // ========================================================
        //
        // Il callback riceve:
        //
        //   zone     = zona
        //   type     = RT / H24 / LEN / MASK
        //   sensors  = sensori coinvolti
        //   active   = true  -> attivazione
        //              false -> ripristino
        //
        // ========================================================

        SecurityOrchestrator::RegisterCallbackAny(
            [this](
                const std::string& zone,
                SensorChannelType type,
                const std::vector<Sensor*>& sensors,
                bool active)
            {
                (void)sensors;

                // ------------------------------------------------
                // TROUBLE PER ZONA
                // ------------------------------------------------

                if (type == SensorChannelType::LEN)
                {
                    const int index =
                        zoneIndex(zone);

                    if (index >= 0)
                    {
                        if (active)
                            activeTroubleZones.insert(index);
                        else
                            activeTroubleZones.erase(index);
                    }
                }


                // ------------------------------------------------
                // EVENT CREATION
                // ------------------------------------------------

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


                // ------------------------------------------------
                // DISPATCH
                // ------------------------------------------------

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
    // La supervisione della comunicazione reale viene effettuata
    // tramite notifyCommunicationSuccess() /
    // notifyCommunicationFault().
    //
    // Non viene più interpretato l'intervallo fra due poll come
    // una perdita della centrale.
    // ============================================================

    bool poll(
        unsigned long now) override
    {
        LOG_IF("SECURITY", "poll BEGIN now=%lu", now);

        lastPoll = now;

        LOG_IF("SECURITY", "poll BEFORE SecurityOrchestrator::Loop");

        const uint8_t changes =
            SecurityOrchestrator::Loop(now);

        LOG_IF("SECURITY",
            "poll AFTER SecurityOrchestrator::Loop changes=%u",
            (unsigned)changes);

        lastChanges =
            changes;

        LOG_IF("SECURITY",
            "poll AFTER lastChanges=%u",
            (unsigned)lastChanges);

        // --------------------------------------------------------
        // PANEL CHANGE
        //
        // Il SecurityOrchestrator produce variazioni locali.
        // --------------------------------------------------------

        if (changes !=
            SecurityOrchestrator::CHANGE_NONE)
        {
            LOG_IF("SECURITY", "poll setting panelStateChanged=true");
            panelStateChanged = true;
        }

        LOG_IF("SECURITY",
            "poll BEFORE RETURN changes=%u panelStateChanged=%d",
            (unsigned)changes,
            panelStateChanged ? 1 : 0);

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


    bool isReady(
        int partition = 0) const override
    {
        if (!validPartition(partition))
            return false;


        auto st =
            SecurityOrchestrator::
                getWiredSensors()
                .ComputeAggregate();


        // --------------------------------------------------------
        // Stato "ready" per armamento.
        //
        // Manteniamo la semantica esistente:
        // intrusion / intrusion H24 impediscono il ready.
        // --------------------------------------------------------

        return !st.intrusion &&
               !st.intrusionH24;
    }


    // ============================================================
    // ARM AWAY
    // ============================================================

    bool armAway(
        int partition = 0) override
    {
        if (!validPartition(partition))
            return false;


        if (!isReady(partition))
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "ARM AWAY rejected: partition %d not ready",
                partition
            );

            return false;
        }


        setArmState(
            ArmState::ARMED_AWAY
        );


        SecurityOrchestrator::
            ForceSecurityCommands(
                true,
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


        if (!isReady(partition))
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "ARM STAY rejected: partition %d not ready",
                partition
            );

            return false;
        }


        setArmState(
            ArmState::ARMED_STAY
        );


        SecurityOrchestrator::
            ForceSecurityCommands(
                true,
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


        if (!isReady(partition))
        {
            LOG_EF(
                "DomoManagerAlarmPanel",
                "ARM NIGHT rejected: partition %d not ready",
                partition
            );

            return false;
        }


        setArmState(
            ArmState::ARMED_NIGHT
        );


        SecurityOrchestrator::
            ForceSecurityCommands(
                true,
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

    bool disarm(
        int partition = 0) override
    {
        if (!validPartition(partition))
            return false;


        setArmState(
            ArmState::DISARMED
        );


        SecurityOrchestrator::
            ForceSecurityCommands(
                false,
                millis()
            );


        emitPartitionChanged(
            partition,
            "DISARM"
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


        LOG_IF(
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
            "ApplyPanelCommand: orchestrator not initialized"
        );

        return false;
    }


    // ============================================================
    // COMMAND AREA CONFIGURED
    // ============================================================

    if (cfgCopy.panelCommandArea < 0)
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: panelCommandArea not configured"
        );

        return false;
    }


    // ============================================================
    // AREA MATCH
    // ============================================================

    if (area != cfgCopy.panelCommandArea)
        return false;


    // ============================================================
    // COMMAND RANGE
    //
    // Evita di trasformare arbitrariamente un long in enum.
    // ============================================================

    if (value < static_cast<long>(AlarmPanelCommand::NONE) ||
        value > static_cast<long>(AlarmPanelCommand::DISARM))
    {
        LOG_EF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: invalid command value=%ld area=%d",
            value,
            area
        );

        return false;
    }


    // ============================================================
    // PANEL
    // ============================================================

    auto& panel =
        DomoManagerAlarmPanel::instance();


    // ============================================================
    // COMMAND
    // ============================================================

    const AlarmPanelCommand command =
        static_cast<AlarmPanelCommand>(value);


    bool result =
        false;


    switch (command)
    {
        case AlarmPanelCommand::ARM_AWAY:

            result =
                panel.armAway(0);

            break;


        case AlarmPanelCommand::ARM_STAY:

            result =
                panel.armStay(0);

            break;


        case AlarmPanelCommand::ARM_NIGHT:

            result =
                panel.armNight(0);

            break;


        case AlarmPanelCommand::DISARM:

            result =
                panel.disarm(0);

            break;


        case AlarmPanelCommand::NONE:
        default:

            result =
                false;

            break;
    }


    // ============================================================
    // CHANGE FLAGS
    //
    // Il comando è stato riconosciuto ed eseguito dal pannello.
    // ============================================================

    if (result)
    {
        // Il cambiamento applicativo è un PANEL change.
        // Il comando ricevuto è anche un COMMAND change.
        //
        // Questi flag sono riferiti al prossimo stato osservabile
        // dal runtime/security layer.

        lastChanges |= CHANGE_COMMAND;
        lastChanges |= CHANGE_PANEL;


        LOG_IF(
            "SecurityOrchestrator",
            "ApplyPanelCommand: area=%d value=%ld command=%u result=OK",
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