
#ifndef DMFrontend_HPP
#define DMFrontend_HPP

#pragma once

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

#include "DMFrontendEngines.hpp"
#include "DMAdapters.hpp"
#include "DMAdapterUDP.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


//Callbacks
static void OnAEEVarChanged(AEEVariableBase* v, unsigned long now) {
    AEEEngine::instance().applyReceived(
        v,
        now
    );
}

class PowerSupervisorEngine {
private:
    static inline GenericSensor::Config cfg12v;

    // ============================================================
    //  CALLBACK ALLARMI (rimane nel frontend)
    // ============================================================
    static void onAlarm(const String& name, long value) {
        LOG_WF("PowerSupervisorEngine", "[ALARM] %s = %ld", name.c_str(), value);

        if (name == PowerSupervisor::ALARM_MAIN_POWER) {
            LOG_WF("PowerSupervisorEngine", "Main power failure.");
        }
        else if (name == PowerSupervisor::ALARM_FAULT) {
            // frontend-specific logic
        }
        else if (name == PowerSupervisor::ALARM_BATTERY_MODE) {
            // frontend-specific logic
        }
        else if (name == PowerSupervisor::ALARM_24V_OK) {
            // frontend-specific logic
        }
    }

public:

    // ============================================================
    //  SETUP FRONTEND
    // ============================================================
    static void Setup(const FrontendConfig::PowerSupervisor& cfg) {

        // --- POWER SUPERVISOR ---
        PowerSupervisorOrchestrator::Setup(cfg, onAlarm);

        // Salvo la config del sensore 12V
        cfg12v = cfg.i12vOk;

        // Extra-signal "12V"
        PowerSupervisorOrchestrator::Get().addExtraSignal("12V", []() -> long {
            auto& buf = DomoManager::instance->getBuffer();
            return GenericSensor::read(cfg12v, buf) != 0 ? 1L : 0L;
        });

        LOG_IF("PowerSupervisorEngine", "Frontend setup completed");
    }

    // ============================================================
    //  LOOP FRONTEND
    // ============================================================
    static void Loop(unsigned long now) {
        PowerSupervisorOrchestrator::Loop(now);
    }
};


class WeatherEngine {
private:

    // ------------------------------------------------------------
    //  AUTOMATION: chiusura finestre
    // ------------------------------------------------------------
    static void scheduleWindowClose() {
        auto& manager = *DomoManager::instance;
        manager.getAutomation().triggerScene("WindowClose",
                                             manager.getTimeManager().nowMs());
    }

    // ------------------------------------------------------------
    //  CALLBACK EVENTI METEO
    // ------------------------------------------------------------
    static void weatherEventHandler(WeatherEvent event) {
        switch (event) {
            case WeatherEvent::RainStart:
                LOG_IF("METEO", "🌧️ Rain started");
                scheduleWindowClose();
                break;

            case WeatherEvent::RainStop:
                LOG_IF("METEO", "☀️ Rain stopped");
                break;

            case WeatherEvent::WindGustStart:
                LOG_IF("METEO", "💨 Wind gust!");
                scheduleWindowClose();
                break;

            case WeatherEvent::WindGustEnd:
                LOG_IF("METEO", "🍃 Wind gust ended");
                break;

            case WeatherEvent::DayStart:
                LOG_IF("METEO", "🌞 Day started");
                break;

            case WeatherEvent::NightStart:
                LOG_IF("METEO", "🌙 Night started");
                scheduleWindowClose();
                break;
        }
    }

    // ------------------------------------------------------------
    //  CALLBACK ALLARMI METEO
    // ------------------------------------------------------------
    static void weatherAlarmHandler(const WeatherAlarm* alarms, int count) {
        LOG_IF("METEO", "⚠️ Weather alarms changed");

        if (count == 0) {
            LOG_IF("METEO", " - No active alarms");
            return;
        }

        for (int i = 0; i < count; i++) {
            switch (alarms[i]) {
                case WeatherAlarm::TempLow:
                    LOG_IF("METEO", " - Temperature too low");
                    break;

                case WeatherAlarm::TempHigh:
                    LOG_IF("METEO", " - Temperature too high");
                    break;

                case WeatherAlarm::WindHigh:
                    LOG_IF("METEO", " - Strong wind");
                    scheduleWindowClose();
                    break;

                case WeatherAlarm::RainHigh:
                    LOG_IF("METEO", " - Heavy rain");
                    scheduleWindowClose();
                    break;
            }
        }
    }

public:

    // ------------------------------------------------------------
    //  SETUP FRONTEND
    // ------------------------------------------------------------
    static void Setup(const FrontendConfig::Weather& cfg) {

        WeatherOrchestrator::Setup(
            cfg,
            weatherEventHandler,
            weatherAlarmHandler
        );

        LOG_IF("METEO", "Frontend weather setup completed");
    }

    // ------------------------------------------------------------
    //  LOOP FRONTEND
    // ------------------------------------------------------------
    static void Loop(unsigned long now) {
        WeatherOrchestrator::Loop(now);
    }

    static WeatherStation& Get() {
        return WeatherOrchestrator::Get();
    }
};


// ============================================================
//  POWER FRONTEND
// ============================================================
class PowerEngine {
private:

    // ------------------------------------------------------------
    // CALLBACKS FRONTEND (rimangono qui)
    // ------------------------------------------------------------
    static void onLoadChange(const String& name, bool state) {
        LOG_IF("POWER", "[EVENT] %s -> %s",
               name.c_str(),
               state ? "ON" : "OFF");
    }

    static void onLimitWarning(float netPower, float limit) {
        LOG_IF("POWER", "[WARNING] net=%0.2f W, limit=%0.2f",
               netPower, limit);
    }

    static void onLimitExceeded(float netPower, float limit) {
        LOG_IF("POWER", "[EXCEEDED] net=%0.2f W, limit=%0.2f",
               netPower, limit);
    }

    static void onPowerError(int code, const String& msg) {
        LOG_IF("POWER", "[ERROR] code=%d msg=%s",
               code, msg.c_str());
    }

    static void onSuggestion(const String& suggestion, int severity, const String& reason) {
        LOG_IF("POWER", "[SUGGESTION] sev=%d %s reason=%s",
               severity,
               suggestion.c_str(),
               reason.c_str());
    }

public:

    // ------------------------------------------------------------
    // SETUP FRONTEND POWER
    // ------------------------------------------------------------
    static void Setup(const FrontendConfig::Power& cfg)
    {
        PowerOrchestrator::Setup(
            cfg,
            3000,      // fallbackSoft
            3500,      // fallbackHard (non usato dal nuovo PM)
            onLoadChange,
            onLimitWarning,
            onLimitExceeded,
            onPowerError,
            onSuggestion
        );

        LOG_IF("POWER", "Frontend Power setup completed");
    }

    // ------------------------------------------------------------
    // LOOP FRONTEND POWER
    // ------------------------------------------------------------
    static void Loop(unsigned long now,
                     int gridPower,
                     float lux,
                     float tempExt,
                     float actualProduction,
                     float meanTemperature,
                     int month,
                     int hour,
                     int minute)
    {
        PowerOrchestrator::Loop(
            now,
            gridPower,
            lux,
            tempExt,
            actualProduction,
            meanTemperature,
            month,
            hour,
            minute
        );
    }

    // ------------------------------------------------------------
    // ACCESSOR
    // ------------------------------------------------------------
    static PowerManager& Get() {
        return PowerOrchestrator::Get();
    }
};

class JobsEngine {
private:
    static inline AsyncScheduler scheduler;

    // ============================================================
    // 1. JOB FUNCTIONS
    // ============================================================

    static bool coolDown(void* ctx) {
        LOG_IF("Scheduler", "Cool down..");
        return true;
    }

    static bool heatUp(void* ctx) {
        static unsigned long start = 0;
        unsigned long now=millis();

        if (start == 0) {
            start = now;
            LOG_IF("Scheduler", "Heating started");
        }

        if (now - start >= 50000) {
            LOG_IF("Scheduler", "Heating finished");
            start = 0;
            return true;
        }
        return false;
    }

    static bool function3(void* ctx) {
        static unsigned long start = 0;
         unsigned long now=millis();

        if (start == 0) {
            start = now;
            LOG_IF("Scheduler", "Long task started");
        }

        if (now - start >= 5000) {
            LOG_IF("Scheduler", "Long task finished");
            start = 0;
            return true;
        }
        return false;
    }

    static void jobDone() {
        LOG_IF("Scheduler", "Job completed");
    }

    static bool isHot(void* ctx) {
        auto& buf = DomoManager::instance->getBuffer();

        BufferSourceInfo info;
        if (buf.GetData(33, info))
            return info.value > 35;

        return analogRead(A0) > 600;
    }

    static bool myExternalSkipCondition(void* ctx) {
        return true;
    }

public:

    // ============================================================
    // 2. SETUP
    // ============================================================
    static void Setup() {
        // ---------------- JOB 0 ----------------
        AsyncScheduler::Job job0;
        job0.priority = 10;
        job0.onComplete = jobDone;

        AsyncScheduler::Step branch;
        branch.type = AsyncScheduler::BRANCH_STEP;
        branch.description = "Verifica temperatura casa...";
        branch.condition = isHot;
        branch.thenStep = 1;
        branch.elseStep = 2;
        job0.steps.push_back(branch);

        AsyncScheduler::Step cool;
        cool.type = AsyncScheduler::NORMAL_STEP;
        cool.fnc = coolDown;
        cool.description = "Esecuzione ciclo raffreddamento";
        cool.delayAfterMs = 1000;
        job0.steps.push_back(cool);

        AsyncScheduler::Step heat;
        heat.type = AsyncScheduler::NORMAL_STEP;
        heat.fnc = heatUp;
        heat.description = "Esecuzione ciclo riscaldamento";
        heat.delayAfterMs = 1000;
        job0.steps.push_back(heat);

        scheduler.addJob(job0, "Quando arrivo a casa");

        // ---------------- JOB 1 ----------------
        AsyncScheduler::Job job1;
        job1.priority = 10;
        job1.onComplete = jobDone;

        AsyncScheduler::Step dummy;
        dummy.type = AsyncScheduler::NORMAL_STEP;
        dummy.fnc = heatUp;
        dummy.description = "Heating step";
        dummy.delayAfterMs = 1000;
        job1.steps.push_back(dummy);

        AsyncScheduler::Step longR;
        longR.type = AsyncScheduler::NORMAL_STEP;
        longR.fnc = function3;
        longR.description = "Prepara acqua calda";
        longR.delayAfterMs = 1000;
        longR.skipIf = myExternalSkipCondition;
        job1.steps.push_back(longR);

        scheduler.addJob(job1, "Avvio riscaldamento");

        LOG_IF("Scheduler", "JobsEngine inizializzato");
    }

    // ============================================================
    // 3. LOOP
    // ============================================================
    static void Loop(unsigned long now) {
        scheduler.run(now);
    }
};

class TaskEngine
{
public:

    using TaskFn =
        void (*)(DomoManager&, unsigned long);


    // ============================================================
    // SETUP
    // ============================================================

    static void Setup(
        const FrontendConfig& cfg)
    {
        TaskEngineBase::Setup(cfg);
    }


    // ============================================================
    // CUSTOM TASK
    // ============================================================

    static void AddTask(
        TaskFn fn,
        uint32_t intervalMs,
        bool enabled = true)
    {
        TaskEngineBase::AddTask(
            fn,
            intervalMs,
            enabled
        );
    }


    // ============================================================
    // LOOP
    // ============================================================

    static void Loop(
        DomoManager& manager,
        unsigned long now)
    {
        TaskEngineBase::Loop(
            manager,
            now
        );
    }


    // ============================================================
    // FRONTEND CYCLE
    // ============================================================

    static bool hasFrontendCycleCompleted()
    {
        return
            TaskEngineBase::
                hasFrontendCycleCompleted();
    }


    static void resetFrontendCycleFlag()
    {
        TaskEngineBase::
            resetFrontendCycleFlag();
    }
};

/*
class DomoManagerFrontendEngine {
private:
    static inline FrontendNetwork* network = nullptr;


    // ============================================================
    //  USER BUTTON
    // ============================================================

    static inline ButtonManager* btnUSER = nullptr;


    // ============================================================
    //  AEE
    // ============================================================

    static inline AEERegistry* aee = nullptr;


    // ============================================================
    //  CALLBACKS / CONFIG
    // ============================================================

    static inline void (*fullCycleCallback)(DomoManager&) = nullptr;

    static inline FrontendConfig config;

    static inline DomoManager* Manager = nullptr;

    static HMIEngine hmiEngine;

    

    // ============================================================
    //  FULL CYCLE
    // ============================================================

    static void CheckFullCycle()
    {
        auto* dm = DomoManager::instance;

        bool backend  = dm->hasBackendCycleCompleted();
        bool frontend = TaskEngine::hasFrontendCycleCompleted();

        if (backend && frontend)
        {
            dm->resetBackendCycleFlag();
            TaskEngine::resetFrontendCycleFlag();

            if (fullCycleCallback)
                fullCycleCallback(*DomoManager::instance);
        }
    }


    // ============================================================
    //  SOMETHING CHANGED
    // ============================================================

    static void SomethingChanged()
    {
        auto& manager = *DomoManager::instance;
        auto& buffer  = manager.getBuffer();

        // --------------------------------------------------------
        // Modifiche provenienti dal pannello HMI
        // --------------------------------------------------------

        auto changedFromPanel =
            buffer.getChangedMap();

        if (!changedFromPanel.empty())
        {
            for (const auto& area : changedFromPanel)
            {
                SecurityOrchestrator::ApplySecurityCommands(area);
            }
        }
    }


    // ============================================================
    //  WATCHDOG
    // ============================================================

    static void watchdogHandler(
        const Watchdog::WatchdogStatus& st)
    {
        LOG_WF(
            "Main",
            "===== WATCHDOG EVENT ====="
        );

        LOG_WF(
            "Main",
            "Reason: %s",
            st.reason
        );

        LOG_WF(
            "Main",
            "Value: %ld",
            st.value
        );


        if (st.blocked)
            LOG_WF("Main", "Type: BLOCKED");

        if (st.overload)
            LOG_WF("Main", "Type: OVERLOAD");

        if (st.unstable)
            LOG_WF("Main", "Type: UNSTABLE");

        if (st.inactive)
            LOG_WF("Main", "Type: INACTIVE");


        if (st.blocked)
        {
            LOG_EF(
                "Main",
                "<<<<<<<<+>>>>>>>>"
            );

            LOG_EF(
                "Main",
                "Action: System reset due to BLOCKED callback"
            );

            NVIC_SystemReset();
        }


        if (st.overload && st.value > 200)
        {
            LOG_WF(
                "Main",
                "Action: Severe overload detected"
            );
        }


        if (st.unstable)
        {
            LOG_WF(
                "Main",
                "Action: System unstable, logging event"
            );
        }


        LOG_WF(
            "Main",
            "=========================="
        );
    }


    // ============================================================
    //  ETHERNET SETUP
    // ============================================================

    static void SetupEthernet(
        const FrontendConfig& cfg)
    {
        uint8_t mac[6];

        for (uint8_t i = 0; i < 6; ++i)
            mac[i] = cfg.net.mac[i];


        Ethernet.begin(
            mac,
            cfg.net.ip,
            cfg.net.gateway,
            cfg.net.subnet
        );


        if (Ethernet.hardwareStatus() == EthernetNoHardware)
        {
            LOG_EF(
                "DomoManagerFrontendEngine",
                "Ethernet shield not found"
            );

            while (true)
            {
                delay(100);
            }
        }


        if (Ethernet.linkStatus() == LinkOFF)
        {
            LOG_EF(
                "DomoManagerFrontendEngine",
                "Ethernet cable is not connected"
            );

            digitalWrite(
                LED_USER,
                true
            );
        }
        else
        {
            LOG_IF(
                "DomoManagerFrontendEngine",
                "Ethernet interface started"
            );

            LOG_IF(
                "DomoManagerFrontendEngine",
                "My IP address: %d.%d.%d.%d",
                Ethernet.localIP()[0],
                Ethernet.localIP()[1],
                Ethernet.localIP()[2],
                Ethernet.localIP()[3]
            );
        }
    }


    // ============================================================
    //  FRONTEND DIAGNOSTICS
    // ============================================================

    static void RegisterFrontendDiagnostics(
        const FrontendConfig& cfg)
    {
        Diagnostic::frontendDiagnosticCallback() =
            [cfg]()
        {
            Serial.println(
                "\n===== FRONTEND DIAGNOSTIC ====="
            );


            if (cfg.ps.enabled)
            {
                PowerSupervisor::Diagnostic::Report(
                    PowerSupervisorOrchestrator::Get()
                );
            }


            if (cfg.power.enabled)
            {
                PowerManager::Diagnostic::FullReport(
                    PowerEngine::Get()
                );
            }


            if (cfg.hvac.enabled)
            {
                HeatPumpController::Diagnostic::Report(
                    HVACEngine::GetHP()
                );
            }


            if (cfg.weather.enabled)
            {
                WeatherStation::Diagnostic::Report(
                    WeatherEngine::Get()
                );
            }


            if (cfg.security.enabled)
            {
                SecurityOrchestrator::Diagnostic::FullReport();

                SecuritySensorEngine::getSystem()
                    .DiagnosticReport();
            }


            // ----------------------------------------------------
            // HMI diagnostics
            // ----------------------------------------------------

            if (hmiEngine.enabled())
            {
                Serial.print("HMI enabled: ");
                Serial.println(
                    hmiEngine.enabled()
                        ? "YES"
                        : "NO"
                );


                Serial.print("HMI loop enabled: ");
                Serial.println(
                    hmiEngine.loopEnabled()
                        ? "YES"
                        : "NO"
                );


                Serial.print("HMI port: ");
                Serial.println(
                    hmiEngine.port()
                );


                Serial.print("HMI active clients: ");
                Serial.println(
                    hmiEngine.activeClients()
                );


                Serial.print("HMI max clients: ");
                Serial.println(
                    hmiEngine.maxClients()
                );
            }


            Serial.println(
                "===== END FRONTEND DIAGNOSTIC =====\n"
            );
        };
    }

    // ============================================================
    //  INIT ENGINES
    // ============================================================

    static void InitEngines(
        DomoManager& manager)
    {
        if (config.security.enabled)
            SecuritySensorEngine::Setup(
                config.security
            );


        if (config.hvac.enabled)
            HVACEngine::Setup(
                config.hvac
            );


        if (config.weather.enabled)
            WeatherEngine::Setup(
                config.weather
            );


        if (config.power.enabled)
            PowerEngine::Setup(
                config.power
            );


        if (config.ps.enabled)
            PowerSupervisorEngine::Setup(
                config.ps
            );

        JobsEngine::Setup();

        if (config.domoManager.automation.json)
        {
            manager.loadAutomationJson(
                config.domoManager.automation.json
            );
        }


        WebAPIEngine::Setup(
            config.webApi
        );


        if (config.watch.enabled)
        {
            WatchEngine::attach(
                manager,
                config.watch
            );
        }


        RegisterFrontendDiagnostics(
            config
        );
    }


    // ============================================================
    //  FULL CYCLE CALLBACK
    // ============================================================

    static void SetFullCycleCallback(
        void (*fn)(DomoManager&))
    {
        fullCycleCallback = fn;
    }

    static void AEEEventCallback(
        const EventManager::Event& event)
    {
        if (!Manager)
            return;

        std::vector<DMAEE::Update> updates;

        if (!DMAEE::BuildUpdatesFromBufferArea(
                AEEEngine::getMgr(),
                event.area,
                event.value,
                updates))
        {
            return;
        }

        if (updates.empty())
            return;

        const unsigned long now =
            Manager->getTimeManager().nowMs();

        DMAEE::ApplyUpdates(
            updates,
            now
        );
    }

    static void MQTTEventCallback(
        const EventManager::Event& event)
    {
        if (!config.mqtt.enabled)
            return;

        MQTTEngine::PublishEvent(event);

        if (Manager->getLeds().hasChannel(LedController::THREE))
        {
            static bool ledState = false;

            ledState = !ledState;

            Manager->getLeds().set(
                LedController::THREE,
                ledState
            );
        }
    }

    static void HMIEventCallback(const EventManager::Event& event)
    {
        if (!hmiEngine.enabled())
            return;

        hmiEngine.PushEvent(event);

        if (Manager->getLeds().hasChannel(LedController::THREE))
        {
            static bool ledState = false;

            ledState = !ledState;

            Manager->getLeds().set(
                LedController::THREE,
                ledState
            );
        }
    }

    // ============================================================
    //  Custom task
    // ============================================================

    static void Task_Jobs(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;

        JobsEngine::Loop(now);
    }


    static void Task_Meteo(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;

        WeatherEngine::Loop(now);
    }

    static void Task_PowerSupervisor(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;

        PowerSupervisorEngine::Loop(now);
    }

    static void Task_Power(
        DomoManager& manager,
        unsigned long now)
    {
        auto& buffer =
            manager.getBuffer();

        auto& averages =
            manager.getAverages();


        // --------------------------------------------------------
        // INPUTS
        // --------------------------------------------------------

        const int gridPower =
            buffer.getValueFast(13, 100);


        const float lux =
            800.0f;


        const float tempExt =
            7.0f;


        const float actualProduction =
            0.0f;


        // --------------------------------------------------------
        // TIME
        // --------------------------------------------------------

        struct tm t;

        manager
            .getTimeManager()
            .getDateTime(t);


        // --------------------------------------------------------
        // POWER ENGINE
        // --------------------------------------------------------

        PowerEngine::Loop(
            now,
            gridPower,
            lux,
            tempExt,
            actualProduction,
            averages.groupAverage("Temperature"),
            t.tm_mon + 1,
            t.tm_hour,
            t.tm_min
        );


        // --------------------------------------------------------
        // DIAGNOSTICS
        // --------------------------------------------------------

        auto& pm =
            PowerEngine::Get();


        LOG_DF(
            "Power",
            "grid=%0.2f solar=%0.2f tempExt=%0.2f lux=%0.2f",
            pm.getGridPower(),
            pm.getSolarPower(),
            tempExt,
            lux
        );
    }
public:

        // ============================================================
        //  SETUP COMPLETO
        // ============================================================

        static void Setup(
            const FrontendConfig& cfg)
        {
            // --------------------------------------------------------
            // Config globale
            // --------------------------------------------------------

            config = cfg;

            // Istanzia FrontendNetwork usando il numero configurato di client MQTT
            network = new FrontendNetwork(cfg.mqtt.clientCount);

            // --------------------------------------------------------
            // USER BUTTON
            // --------------------------------------------------------

            btnUSER =
                new ButtonManager(
                    cfg.pins.userButton
                );

            btnUSER->begin();


            LOG_I(
                "DomoManagerFrontendEngine",
                "DomoManager is starting..."
            );


            // --------------------------------------------------------
            // AEE
            // --------------------------------------------------------

            AEEEngine::instance().Setup(
                cfg.bridge.aee
            );


            // --------------------------------------------------------
            // DOMO MANAGER
            // --------------------------------------------------------

            Manager =
                new DomoManager(
                    cfg.pins.leds
                );


            // --------------------------------------------------------
            // ETHERNET + HMI
            // --------------------------------------------------------

            SetupEthernet(cfg);


            // --------------------------------------------------------
            // MODBUS TCP CLIENT
            // --------------------------------------------------------

            network->modbusTCP().modbus.setTimeout(
                cfg.modbus.timeoutMs
            );


            LOG_IF(
                "DomoManagerFrontendEngine",
                "Modbus TCP to RTU timeout set %d mSec.",
                cfg.modbus.timeoutMs
            );


            delay(2000);


            // --------------------------------------------------------
            // TASK ENGINE
            // --------------------------------------------------------

            TaskEngine::Setup(cfg);
            TaskEngine::AddTask(
                [](DomoManager& dm, unsigned long now)
                {
                    (void)dm;

                    JobsEngine::Loop(now);
                },
                cfg.jobs.intervalMs,
                cfg.jobs.enabled
            );
            TaskEngine::AddTask(
                [](DomoManager& dm, unsigned long now)
                {
                    hmiEngine.Sync(dm, now);
                },
                10,
                cfg.domoManager.hmi.enabled
            );
            TaskEngine::AddTask(
                [](DomoManager& dm, unsigned long now)
                {
                    (void)dm;
                    WeatherEngine::Loop(now);
                },
                cfg.weather.intervalMs,
                cfg.weather.enabled
            );
            TaskEngine::AddTask(
                Task_PowerSupervisor,
                cfg.ps.intervalMs,
                cfg.ps.enabled
            );
            TaskEngine::AddTask(
                Task_Power,
                cfg.power.intervalMs,
                cfg.power.enabled
            );
            
            // --------------------------------------------------------
            // WATCHDOG CALLBACK
            // --------------------------------------------------------

            Manager->SetWatchdogCallback(
                watchdogHandler
            );


            // ============================================================
            // EVENT SOURCES
            // ============================================================

            static int hmiSource =Manager->getEventManager().add(
                HMIEventCallback,
                "HMI"
            );

            static int aeeSource = Manager->getEventManager().add(
                AEEEventCallback,
                "AEE"
            );
            AEEEngine::instance().attachReceiveContext(
                Manager->getBuffer(),
                Manager->getEventManager(),
                aeeSource
            );

            // --------------------------------------------------------
            // DOMO MANAGER CORE
            // --------------------------------------------------------
            NetworkManager::ProtocolId modbusRTUProtocol = -1;
            if (cfg.modbus.enabled)
            {
                modbusRTUProtocol =
                    Manager->net.registerProtocol(
                        "Mbus-RTU",
                        200,
                        550,
                        1
                    );
                Manager->net.setPacingEnabled(
                    modbusRTUProtocol,
                    false
                );
            }

            if (cfg.domoManager.hmi.enabled)
            {
                auto hmiProtocolId =
                    Manager->net.registerProtocol(
                        "HMI",
                        cfg.domoManager.hmi.pollingMs,
                        60,
                        cfg.domoManager.hmi.maxClients
                    );
                hmiEngine.Setup(
                    config.domoManager.hmi,
                    hmiProtocolId,
                    cfg.domoManager.hmi.maxClients, 300, hmiSource
                );
            }

            if (!Manager->setup(
                    SomethingChanged,
                    TaskEngine::Loop,
                    config.domoManager,
                    modbusRTUProtocol))
            {
                while (true)
                {
                    delay(100);
                }
            }

            Manager->getOwner().setCallback(
                FrontendOwnerMode::OnChanged
            );

        
            // --------------------------------------------------------
            // ENGINES
            // --------------------------------------------------------

            InitEngines(*Manager);


            // --------------------------------------------------------
            // AEE FRONTEND CALLBACK
            // --------------------------------------------------------

            BridgeEngine::get().onFrontendAEEChange =
                OnAEEVarChanged;


            LOG_IF(
                "AEE",
                "AEE initialized with %d variables",
                cfg.bridge.aee.count
            );


            // --------------------------------------------------------
            // HOTSTANDBY / MQTT / BRIDGE
            // --------------------------------------------------------

            #if HOTSTANDBY_ENABLED

                Manager->enableHotStandby(false);

            #else

                if (cfg.mqtt.enabled)
                {
                    int mqttProtocolId  =
                        Manager->net.registerProtocol(
                            "MQTT",
                            50,
                            20,
                            (uint8_t)cfg.mqtt.clientCount
                        );

                    static int mqttSource = Manager->getEventManager().add(
                        MQTTEventCallback,
                        "MQTT"
                    );

                    MQTTEngine::Setup(
                        *Manager,
                        Manager->net,
                        &network->mqtt(0).client,
                        &network->mqtt(0).pubSub,
                        network->mqttClientCount(),
                        cfg.mqtt,
                        mqttProtocolId,
                        (uint8_t)mqttSource
                    );

                    
                }


                if (cfg.bridge.enabled)
                {
                    static UdpAdapter transport(
                        cfg.bridge.ip,
                        cfg.bridge.localPort,
                        cfg.bridge.remotePort
                    );

                    BridgeEngine::get().init(
                        &transport,
                        AEEEngine::getAEE(),
                        true
                    );

                    int bridgeProtocolId =
                        Manager->net.registerProtocol(
                            "Bridge",
                            50,
                            300,
                            1
                        );

                    BridgeEngine::instance().setupPacer(
                        Manager->net,
                        bridgeProtocolId
                    );
                }

            #endif


            // --------------------------------------------------------
            // FULL CYCLE
            // --------------------------------------------------------

            DomoManagerFrontendEngine::SetFullCycleCallback(
                [](DomoManager& dm)
                {
                    const int OLDER = 5000;

                    auto& buffer = dm.getBuffer();

                    std::vector<DMAEE::Update> updates;
                    updates.reserve(16);

                    const unsigned long now =
                        dm.getTimeManager().nowMs();

                    // --------------------------------------------------------
                    // POLLING AEE
                    //
                    // Qui vengono gestite SOLO:
                    //   - GenericSensor
                    //   - Function
                    //
                    // Le BufferArea NON passano più da questo percorso:
                    // vengono aggiornate dal consumer AEE dell'EventManager.
                    // --------------------------------------------------------
                    const bool hasUpdates =
                        DMAEE::BuildUpdatesFromPolledSources(
                            AEEEngine::getMgr(),
                            buffer,
                            updates
                        );

                    if (hasUpdates)
                    {
                        DMAEE::ApplyUpdates(
                            updates,
                            now
                        );
                    }

                    // --------------------------------------------------------
                    // Pulizia delle variazioni Buffer già consumate dal ciclo.
                    // --------------------------------------------------------
                    buffer.ResetAll(
                        now,
                        OLDER
                    );
                }
            );


            // --------------------------------------------------------
            // WATCHDOG
            // --------------------------------------------------------

            Manager->enableWatchdog();

            LOG_IF(
                "DOMO MANAGER",
                "********************* 08.2026 build 02"
            );
        }


        // ============================================================
        //  LOOP COMPLETO
        // ============================================================

        static void loop()
        {
            unsigned long now =
                Manager->getTimeManager().nowMs();


            // ========================================================
            // USER BUTTON
            // ========================================================

            auto st =
                btnUSER->update(now);


            if (st.pressedAtStartup)
            {
                LOG_IF(
                    "BUTTON",
                    "Pulsante tenuto premuto allo startup"
                );


                Manager->getWatchDiag().onButtonPressed(
                    *Manager,
                    0,
                    config.diagnostic
                );
            }
            else if (st.pressedNow)
            {
                LOG_IF(
                    "BUTTON",
                    "Pulsante premuto durante il loop"
                );


                auto& wd =
                    Manager->getWatchDiag();


                wd.onButtonPressed(
                    *Manager,
                    1,
                    config.diagnostic
                );
            }


            // ========================================================
            // DEVELOPER MODE
            // ========================================================

            static bool devModeApplied = false;


            if (!devModeApplied)
            {
                Manager->getOwner().setDeveloper(
                    OwnerManager::SYSTEM_REBOOT
                );

                devModeApplied = true;
            }


            // ========================================================
            // HOTSTANDBY
            // ========================================================

            #if HOTSTANDBY_ENABLED

                static bool lastMaster = false;

                bool isMaster =
                    Manager->isClusterMaster();

                if (isMaster != lastMaster)
                {
                    if (isMaster)
                    {
                        LOG_I(
                            "Main",
                            "Passo a MASTER -> abilito Ethernet"
                        );


                        SetupEthernet(config);
                    }
                    else
                    {
                        LOG_I(
                            "Main",
                            "Passo a SLAVE -> disabilito Ethernet"
                        );


                        Ethernet.end();
                    }


                    lastMaster = isMaster;
                }

            #endif


            // ========================================================
            // APPLICATION LOOP
            // ========================================================

            #if HOTSTANDBY_ENABLED

                if (isMaster)

            #else

                if (true)

            #endif
            {
                // ----------------------------------------------------
                // HMI
                // ----------------------------------------------------

                hmiEngine.SetLoopEnabled(
                    Manager->getPowerOnCycleCompleted()
                );

                hmiEngine.ProcessNetwork(
                    *Manager,
                    now
                );


                // ----------------------------------------------------
                // DOMO MANAGER
                // ----------------------------------------------------

                Manager->loop(network->modbusTCP().modbus);
            }
            else
            {
                // ----------------------------------------------------
                // SLAVE
                // ----------------------------------------------------

                Manager->loop(network->modbusTCP().modbus);
            }


            // ========================================================
            // FULL CYCLE
            // ========================================================

            CheckFullCycle();
        }
};*/

class DomoManagerFrontendEngine
    : public FrontendRuntime
{
private:

    // ============================================================
    //  INSTANCE
    // ============================================================

    static inline DomoManagerFrontendEngine* instance = nullptr;


    // ============================================================
    //  NETWORK
    // ============================================================

    static inline FrontendNetwork* network = nullptr;


    // ============================================================
    //  AEE
    // ============================================================

    static inline AEERegistry* aee = nullptr;


    // ============================================================
    //  CALLBACKS / CONFIG
    // ============================================================

    static inline void (*fullCycleCallback)(DomoManager&) = nullptr;

    static inline FrontendConfig config;

    static inline DomoManager* Manager = nullptr;

    static HMIEngine hmiEngine;


    // ============================================================
    //  FULL CYCLE
    // ============================================================

    static void CheckFullCycle()
    {
        auto* dm = DomoManager::instance;

        if (!dm)
            return;

        bool backend =
            dm->hasBackendCycleCompleted();

        bool frontend =
            TaskEngine::hasFrontendCycleCompleted();

        if (backend && frontend)
        {
            dm->resetBackendCycleFlag();

            TaskEngine::resetFrontendCycleFlag();

            if (fullCycleCallback)
                fullCycleCallback(*DomoManager::instance);
        }
    }


    // ============================================================
    //  SOMETHING CHANGED
    // ============================================================

    static void SomethingChanged()
    {
        auto& manager =
            *DomoManager::instance;

        auto& buffer =
            manager.getBuffer();


        // --------------------------------------------------------
        // Modifiche provenienti dal pannello HMI
        // --------------------------------------------------------

        auto changedFromPanel =
            buffer.getChangedMap();

        if (!changedFromPanel.empty())
        {
            for (const auto& area :
                 changedFromPanel)
            {
                SecurityOrchestrator::
                    ApplySecurityCommands(area);
            }
        }
    }


    // ============================================================
    //  WATCHDOG
    // ============================================================

    static void watchdogHandler(
        const Watchdog::WatchdogStatus& st)
    {
        LOG_WF(
            "Main",
            "===== WATCHDOG EVENT ====="
        );

        LOG_WF(
            "Main",
            "Reason: %s",
            st.reason
        );

        LOG_WF(
            "Main",
            "Value: %ld",
            st.value
        );


        if (st.blocked)
            LOG_WF(
                "Main",
                "Type: BLOCKED"
            );


        if (st.overload)
            LOG_WF(
                "Main",
                "Type: OVERLOAD"
            );


        if (st.unstable)
            LOG_WF(
                "Main",
                "Type: UNSTABLE"
            );


        if (st.inactive)
            LOG_WF(
                "Main",
                "Type: INACTIVE"
            );


        if (st.blocked)
        {
            LOG_EF(
                "Main",
                "<<<<<<<<+>>>>>>>>"
            );

            LOG_EF(
                "Main",
                "Action: System reset due to BLOCKED callback"
            );

            NVIC_SystemReset();
        }


        if (st.overload && st.value > 200)
        {
            LOG_WF(
                "Main",
                "Action: Severe overload detected"
            );
        }


        if (st.unstable)
        {
            LOG_WF(
                "Main",
                "Action: System unstable, logging event"
            );
        }


        LOG_WF(
            "Main",
            "=========================="
        );
    }


    // ============================================================
    //  ETHERNET SETUP
    // ============================================================

    static void SetupEthernet(
        const FrontendConfig& cfg)
    {
        uint8_t mac[6];

        for (uint8_t i = 0; i < 6; ++i)
            mac[i] = cfg.net.mac[i];


        Ethernet.begin(
            mac,
            cfg.net.ip,
            cfg.net.gateway,
            cfg.net.subnet
        );


        if (Ethernet.hardwareStatus() ==
            EthernetNoHardware)
        {
            LOG_EF(
                "DomoManagerFrontendEngine",
                "Ethernet shield not found"
            );

            while (true)
            {
                delay(100);
            }
        }


        if (Ethernet.linkStatus() == LinkOFF)
        {
            LOG_EF(
                "DomoManagerFrontendEngine",
                "Ethernet cable is not connected"
            );

            digitalWrite(
                LED_USER,
                true
            );
        }
        else
        {
            LOG_IF(
                "DomoManagerFrontendEngine",
                "Ethernet interface started"
            );

            LOG_IF(
                "DomoManagerFrontendEngine",
                "My IP address: %d.%d.%d.%d",
                Ethernet.localIP()[0],
                Ethernet.localIP()[1],
                Ethernet.localIP()[2],
                Ethernet.localIP()[3]
            );
        }
    }

    // ============================================================
    //  INIT ENGINES
    // ============================================================

    static void InitEngines(
        DomoManager& manager)
    {
        if (config.security.enabled)
            SecuritySensorEngine::Setup(
                config.security
            );


        if (config.hvac.enabled)
            HVACEngine::Setup(
                config.hvac
            );


        if (config.weather.enabled)
            WeatherEngine::Setup(
                config.weather
            );


        if (config.power.enabled)
            PowerEngine::Setup(
                config.power
            );


        if (config.ps.enabled)
            PowerSupervisorEngine::Setup(
                config.ps
            );


        JobsEngine::Setup();


        if (config.domoManager.automation.json)
        {
            manager.loadAutomationJson(
                config.domoManager.automation.json
            );
        }


        WebAPIEngine::Setup(
            config.webApi
        );


        if (config.watch.enabled)
        {
            WatchEngine::attach(
                manager,
                config.watch
            );
        }


        instance->RegisterFrontendDiagnostics();
    }


    // ============================================================
    //  FULL CYCLE CALLBACK
    // ============================================================

    static void SetFullCycleCallback(
        void (*fn)(DomoManager&))
    {
        fullCycleCallback = fn;
    }


    // ============================================================
    //  AEE EVENT
    // ============================================================

    static void AEEEventCallback(
        const EventManager::Event& event)
    {
        if (!Manager)
            return;


        std::vector<DMAEE::Update> updates;


        if (!DMAEE::BuildUpdatesFromBufferArea(
                AEEEngine::getMgr(),
                event.area,
                event.value,
                updates))
        {
            return;
        }


        if (updates.empty())
            return;


        const unsigned long now =
            Manager->getTimeManager().nowMs();


        DMAEE::ApplyUpdates(
            updates,
            now
        );
    }


    // ============================================================
    //  MQTT EVENT
    // ============================================================

    static void MQTTEventCallback(
        const EventManager::Event& event)
    {
        if (!config.mqtt.enabled)
            return;


        MQTTEngine::PublishEvent(
            event
        );


        if (Manager->getLeds().hasChannel(
                LedController::THREE))
        {
            static bool ledState = false;

            ledState = !ledState;

            Manager->getLeds().set(
                LedController::THREE,
                ledState
            );
        }
    }


    // ============================================================
    //  HMI EVENT
    // ============================================================

    static void HMIEventCallback(
        const EventManager::Event& event)
    {
        if (!hmiEngine.enabled())
            return;


        hmiEngine.PushEvent(
            event
        );


        if (Manager->getLeds().hasChannel(
                LedController::THREE))
        {
            static bool ledState = false;

            ledState = !ledState;

            Manager->getLeds().set(
                LedController::THREE,
                ledState
            );
        }
    }


    // ============================================================
    //  CUSTOM TASK
    // ============================================================

    static void Task_Jobs(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;

        JobsEngine::Loop(
            now
        );
    }


    static void Task_Meteo(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;

        WeatherEngine::Loop(
            now
        );
    }


    static void Task_PowerSupervisor(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;

        PowerSupervisorEngine::Loop(
            now
        );
    }


    static void Task_Power(
        DomoManager& manager,
        unsigned long now)
    {
        auto& buffer =
            manager.getBuffer();


        auto& averages =
            manager.getAverages();


        // --------------------------------------------------------
        // INPUTS
        // --------------------------------------------------------

        const int gridPower =
            buffer.getValueFast(
                13,
                100
            );


        const float lux =
            800.0f;


        const float tempExt =
            7.0f;


        const float actualProduction =
            0.0f;


        // --------------------------------------------------------
        // TIME
        // --------------------------------------------------------

        struct tm t;


        manager
            .getTimeManager()
            .getDateTime(t);


        // --------------------------------------------------------
        // POWER ENGINE
        // --------------------------------------------------------

        PowerEngine::Loop(
            now,
            gridPower,
            lux,
            tempExt,
            actualProduction,
            averages.groupAverage(
                "Temperature"
            ),
            t.tm_mon + 1,
            t.tm_hour,
            t.tm_min
        );


        // --------------------------------------------------------
        // DIAGNOSTICS
        // --------------------------------------------------------

        auto& pm =
            PowerEngine::Get();


        LOG_DF(
            "Power",
            "grid=%0.2f solar=%0.2f tempExt=%0.2f lux=%0.2f",
            pm.getGridPower(),
            pm.getSolarPower(),
            tempExt,
            lux
        );
    }


    // ============================================================
    //  FRONTEND RUNTIME HOOKS
    // ============================================================

protected:

    void onFrontendDiagnostics() override
    {
        // HMI
        if (hmiEngine.enabled())
        {
            Serial.print("HMI enabled: ");
            Serial.println(
                hmiEngine.enabled()
                    ? "YES"
                    : "NO"
            );

            Serial.print("HMI loop enabled: ");
            Serial.println(
                hmiEngine.loopEnabled()
                    ? "YES"
                    : "NO"
            );

            Serial.print("HMI port: ");
            Serial.println(
                hmiEngine.port()
            );

            Serial.print("HMI active clients: ");
            Serial.println(
                hmiEngine.activeClients()
            );

            Serial.print("HMI max clients: ");
            Serial.println(
                hmiEngine.maxClients()
            );
        }

        if (config.weather.enabled)
        {
            WeatherStation::Diagnostic::Report(
                WeatherEngine::Get()
            );
        }
        
        if (config.power.enabled)
        {
            PowerManager::Diagnostic::FullReport(
                PowerEngine::Get()
            );
        }

        // eventuale diagnostica specifica del frontend
        // ...

        // eventuale diagnostica utente
        // ...
    }

    // ------------------------------------------------------------
    // USER BUTTON
    // ------------------------------------------------------------

    void onStartupButton(
        unsigned long now) override
    {
        (void)now;

        /*
         * Per ora nessuna logica aggiuntiva.
         *
         * La gestione generica del pulsante viene già eseguita
         * da FrontendRuntime.
         */
    }


    void onButtonPressed(
        unsigned long now) override
    {
        (void)now;

        /*
         * Punto di estensione per eventuale logica frontend
         * personalizzata sul pulsante.
         */
    }


    // ------------------------------------------------------------
    // DEVELOPER MODE
    // ------------------------------------------------------------

    void onDeveloperMode() override
    {
        /*
         * Punto di estensione.
         *
         * FrontendRuntime ha già eseguito:
         *
         * manager.getOwner().setDeveloper(
         *     OwnerManager::SYSTEM_REBOOT
         * );
         */
    }


    #if HOTSTANDBY_ENABLED

        // ------------------------------------------------------------
        // MASTER
        // ------------------------------------------------------------

        void onBecomeMaster() override
        {
            LOG_I(
                "Main",
                "Passo a MASTER -> abilito Ethernet"
            );


            SetupEthernet(
                config
            );
        }


        // ------------------------------------------------------------
        // SLAVE
        // ------------------------------------------------------------

        void onBecomeSlave() override
        {
            LOG_I(
                "Main",
                "Passo a SLAVE -> disabilito Ethernet"
            );


            Ethernet.end();
        }

    #endif


private:

    // ============================================================
    //  CONSTRUCTOR
    // ============================================================

    DomoManagerFrontendEngine(
        DomoManager& dm,
        const FrontendConfig& cfg)
        : FrontendRuntime(
            dm,
            cfg
        )
    {
    }


public:

    // ============================================================
    //  SETUP COMPLETO
    // ============================================================

    static void Setup(
        const FrontendConfig& cfg)
    {
        // --------------------------------------------------------
        // Config globale
        // --------------------------------------------------------

        config = cfg;


        // --------------------------------------------------------
        // AEE
        // --------------------------------------------------------

        AEEEngine::instance().Setup(
            cfg.bridge.aee
        );


        // --------------------------------------------------------
        // DOMO MANAGER
        // --------------------------------------------------------

        Manager =
            new DomoManager(
                cfg.pins.leds
            );


        // --------------------------------------------------------
        // FRONTEND RUNTIME
        // --------------------------------------------------------

        instance =
            new DomoManagerFrontendEngine(
                *Manager,
                cfg
            );


        // --------------------------------------------------------
        // NETWORK
        // --------------------------------------------------------

        network =
            new FrontendNetwork(
                cfg.mqtt.clientCount
            );


        LOG_I(
            "DomoManagerFrontendEngine",
            "DomoManager is starting..."
        );


        // --------------------------------------------------------
        // ETHERNET + HMI
        // --------------------------------------------------------

        SetupEthernet(
            cfg
        );


        // --------------------------------------------------------
        // MODBUS TCP CLIENT
        // --------------------------------------------------------

        network
            ->modbusTCP()
            .modbus
            .setTimeout(
                cfg.modbus.timeoutMs
            );


        LOG_IF(
            "DomoManagerFrontendEngine",
            "Modbus TCP to RTU timeout set %d mSec.",
            cfg.modbus.timeoutMs
        );


        delay(2000);


        // --------------------------------------------------------
        // TASK ENGINE
        // --------------------------------------------------------

        TaskEngine::Setup(
            cfg
        );


        TaskEngine::AddTask(
            [](DomoManager& dm, unsigned long now)
            {
                (void)dm;

                JobsEngine::Loop(
                    now
                );
            },
            cfg.jobs.intervalMs,
            cfg.jobs.enabled
        );


        TaskEngine::AddTask(
            [](DomoManager& dm, unsigned long now)
            {
                hmiEngine.Sync(
                    dm,
                    now
                );
            },
            10,
            cfg.domoManager.hmi.enabled
        );


        TaskEngine::AddTask(
            [](DomoManager& dm, unsigned long now)
            {
                (void)dm;

                WeatherEngine::Loop(
                    now
                );
            },
            cfg.weather.intervalMs,
            cfg.weather.enabled
        );


        TaskEngine::AddTask(
            Task_PowerSupervisor,
            cfg.ps.intervalMs,
            cfg.ps.enabled
        );


        TaskEngine::AddTask(
            Task_Power,
            cfg.power.intervalMs,
            cfg.power.enabled
        );


        // --------------------------------------------------------
        // WATCHDOG CALLBACK
        // --------------------------------------------------------

        Manager->SetWatchdogCallback(
            watchdogHandler
        );


        // ========================================================
        // EVENT SOURCES
        // ========================================================

        static int hmiSource =
            Manager->getEventManager().add(
                HMIEventCallback,
                "HMI"
            );


        static int aeeSource =
            Manager->getEventManager().add(
                AEEEventCallback,
                "AEE"
            );


        AEEEngine::instance()
            .attachReceiveContext(
                Manager->getBuffer(),
                Manager->getEventManager(),
                aeeSource
            );


        // --------------------------------------------------------
        // DOMO MANAGER CORE
        // --------------------------------------------------------

        NetworkManager::ProtocolId modbusRTUProtocol = -1;


        if (cfg.modbus.enabled)
        {
            modbusRTUProtocol =
                Manager->net.registerProtocol(
                    "Mbus-RTU",
                    200,
                    550,
                    1
                );


            Manager->net.setPacingEnabled(
                modbusRTUProtocol,
                false
            );
        }


        if (cfg.domoManager.hmi.enabled)
        {
            auto hmiProtocolId =
                Manager->net.registerProtocol(
                    "HMI",
                    cfg.domoManager.hmi.pollingMs,
                    60,
                    cfg.domoManager.hmi.maxClients
                );


            hmiEngine.Setup(
                config.domoManager.hmi,
                hmiProtocolId,
                cfg.domoManager.hmi.maxClients,
                300,
                hmiSource
            );
        }


        if (!Manager->setup(
                SomethingChanged,
                TaskEngine::Loop,
                config.domoManager,
                modbusRTUProtocol))
        {
            while (true)
            {
                delay(100);
            }
        }


        Manager->getOwner().setCallback(
            FrontendOwnerMode::OnChanged
        );


        // --------------------------------------------------------
        // ENGINES
        // --------------------------------------------------------

        InitEngines(
            *Manager
        );


        // --------------------------------------------------------
        // AEE FRONTEND CALLBACK
        // --------------------------------------------------------

        BridgeEngine::get()
            .onFrontendAEEChange =
                OnAEEVarChanged;


        LOG_IF(
            "AEE",
            "AEE initialized with %d variables",
            cfg.bridge.aee.count
        );


        // --------------------------------------------------------
        // HOTSTANDBY / MQTT / BRIDGE
        // --------------------------------------------------------

        #if HOTSTANDBY_ENABLED

                Manager->enableHotStandby(
                    false
                );

        #else

                if (cfg.mqtt.enabled)
                {
                    int mqttProtocolId =
                        Manager->net.registerProtocol(
                            "MQTT",
                            50,
                            20,
                            (uint8_t)
                                cfg.mqtt.clientCount
                        );


                    static int mqttSource =
                        Manager->getEventManager().add(
                            MQTTEventCallback,
                            "MQTT"
                        );


                    MQTTEngine::Setup(
                        *Manager,
                        Manager->net,
                        &network->mqtt(0).client,
                        &network->mqtt(0).pubSub,
                        network->mqttClientCount(),
                        cfg.mqtt,
                        mqttProtocolId,
                        (uint8_t)mqttSource
                    );
                }


                if (cfg.bridge.enabled)
                {
                    static UdpAdapter transport(
                        cfg.bridge.ip,
                        cfg.bridge.localPort,
                        cfg.bridge.remotePort
                    );


                    BridgeEngine::get().init(
                        &transport,
                        AEEEngine::getAEE(),
                        true
                    );


                    int bridgeProtocolId =
                        Manager->net.registerProtocol(
                            "Bridge",
                            50,
                            300,
                            1
                        );


                    BridgeEngine::instance()
                        .setupPacer(
                            Manager->net,
                            bridgeProtocolId
                        );
                }

        #endif


        // --------------------------------------------------------
        // FULL CYCLE
        // --------------------------------------------------------

        DomoManagerFrontendEngine::
            SetFullCycleCallback(
                [](DomoManager& dm)
                {
                    const int OLDER = 5000;


                    auto& buffer =
                        dm.getBuffer();


                    std::vector<DMAEE::Update>
                        updates;


                    updates.reserve(
                        16
                    );


                    const unsigned long now =
                        dm.getTimeManager()
                            .nowMs();


                    // ------------------------------------------------
                    // POLLING AEE
                    // ------------------------------------------------

                    const bool hasUpdates =
                        DMAEE::
                        BuildUpdatesFromPolledSources(
                            AEEEngine::getMgr(),
                            buffer,
                            updates
                        );


                    if (hasUpdates)
                    {
                        DMAEE::ApplyUpdates(
                            updates,
                            now
                        );
                    }


                    // ------------------------------------------------
                    // Pulizia variazioni Buffer
                    // ------------------------------------------------

                    buffer.ResetAll(
                        now,
                        OLDER
                    );
                }
            );


        // --------------------------------------------------------
        // WATCHDOG
        // --------------------------------------------------------

        Manager->enableWatchdog();


        LOG_IF(
            "DOMO MANAGER",
            "********************* 08.2026 build 02"
        );
    }


    // ============================================================
    //  LOOP COMPLETO
    // ============================================================

    static void loop()
    {
        if (!instance)
            return;


        // ========================================================
        // FRONTEND RUNTIME
        //
        // Qui vengono eseguiti:
        //
        //   - acquisizione now
        //   - ButtonManager
        //   - Developer Mode
        //   - HotStandby
        //
        // ========================================================

        const unsigned long now =
            instance->updateRuntime();


        // ========================================================
        // APPLICATION LOOP
        // ========================================================

        #if HOTSTANDBY_ENABLED

                if (instance->getIsMaster())

        #else

                if (true)

        #endif
        {
            // ----------------------------------------------------
            // HMI
            // ----------------------------------------------------

            hmiEngine.SetLoopEnabled(
                Manager->getPowerOnCycleCompleted()
            );


            hmiEngine.ProcessNetwork(
                *Manager,
                now
            );


            // ----------------------------------------------------
            // DOMO MANAGER
            // ----------------------------------------------------

            Manager->loop(
                network->
                    modbusTCP()
                    .modbus
            );
        }
        else
        {
            // ----------------------------------------------------
            // SLAVE
            // ----------------------------------------------------

            Manager->loop(
                network->
                    modbusTCP()
                    .modbus
            );
        }


        // ========================================================
        // FULL CYCLE
        // ========================================================

        CheckFullCycle();
    }
};

// ============================================================
// DEFINIZIONE ISTANZA HMI
// ============================================================

HMIEngine DomoManagerFrontendEngine::hmiEngine;

#endif
