
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

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class TaskEngine;


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



class SecuritySensorEngine {
private:

    // ------------------------------------------------------------
    // CALLBACKS
    // ------------------------------------------------------------
    static void onAnyAlarm(const std::string& zone,
                           SensorChannelType type,
                           const std::vector<Sensor*>& sensors)
    {
        LOG_IF("SecurityFrontend",
               "[GLOBAL] Alarm zone=%s type=%d sensors=%u",
               zone.c_str(), (int)type, (unsigned)sensors.size());
    }

    static void onZoneAlarm(const std::string& zone,
                            SensorChannelType type,
                            const std::vector<Sensor*>& sensors)
    {
        LOG_IF("SecurityFrontend",
               "[ZONE] Alarm in zone=%s type=%d sensors=%u",
               zone.c_str(), (int)type, (unsigned)sensors.size());
    }

    static void onTypeAlarm(const std::string& zone,
                            SensorChannelType type,
                            const std::vector<Sensor*>& sensors)
    {
        LOG_IF("SecurityFrontend",
               "[TYPE] Alarm type=%d in zone=%s sensors=%u",
               (int)type, zone.c_str(), (unsigned)sensors.size());
    }

    static void onZoneTypeAlarm(const std::string& zone,
                                SensorChannelType type,
                                const std::vector<Sensor*>& sensors)
    {
        LOG_IF("SecurityFrontend",
               "[ZONE+TYPE] Alarm zone=%s type=%d sensors=%u",
               zone.c_str(), (int)type, (unsigned)sensors.size());
    }

    // ------------------------------------------------------------
    // DYNAMIC CALLBACK REGISTRATION
    // ------------------------------------------------------------
    static void registerSecurityCallbacks() {

        auto& wired = SecurityOrchestrator::getWiredSensors();

        // Global
        SecurityOrchestrator::RegisterCallbackAny(onAnyAlarm);

        // Types
        static const SensorChannelType allTypes[] = {
            SensorChannelType::RT,
            SensorChannelType::H24,
            SensorChannelType::MASK,
            SensorChannelType::LEN
        };

        for (auto t : allTypes)
            SecurityOrchestrator::RegisterCallbackType(t, onTypeAlarm);

        // Dynamic zones
        for (const auto& entry : wired.GetZoneMap()) {
            const std::string& zoneName = entry.first;

            SecurityOrchestrator::RegisterCallbackZone(zoneName, onZoneAlarm);

            for (auto t : allTypes)
                SecurityOrchestrator::RegisterCallbackZoneType(zoneName, t, onZoneTypeAlarm);
        }

        LOG_IF("SecurityFrontend", "Dynamic security callbacks registered");
    }

public:

    // ------------------------------------------------------------
    // SETUP
    // ------------------------------------------------------------
    static void Setup(const FrontendConfig::Security& cfg) {

        SecurityOrchestrator::Setup(&cfg);

        registerSecurityCallbacks();

        LOG_IF("SecurityFrontend", "Security frontend setup completed");
    }

    // ------------------------------------------------------------
    // LOOP
    // ------------------------------------------------------------
    static bool Loop(unsigned long now) {
        return SecurityOrchestrator::Loop(now);
    }

    // ============================================================
    // ACCESSORS
    // ============================================================
    static SecurityOrchestrator::SystemManager& getSystem() { return SecurityOrchestrator::getSystem(); }
    static WiredSensorsManager& getWiredSensors() { return SecurityOrchestrator::getWiredSensors(); }

    // ============================================================
    // COMMANDS
    // ============================================================
    static void ApplySecurityCommands(int area) {
        SecurityOrchestrator::ApplySecurityCommands(area);
    }

    // ============================================================
    // DIAGNOSTICA
    // ============================================================
    class Diagnostic {
    public:
        static void FullReport() {
            SecurityOrchestrator::Diagnostic::FullReport();
        }
    };
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
        if (buf.GetData(33, Field, info))
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

class TaskEngine {
private:
    // AEE appartiene al frontend, NON al backend
    static inline AEERegistry* aee = nullptr;
    static inline AEEManagement* aeeMgr = nullptr;

private:
    
    // ------------------------------------------------------------------------
    //  TASK: Jobs
    // ------------------------------------------------------------------------
    static void Task_Jobs(DomoManager& manager, unsigned long now) {
        JobsEngine::Loop(now);
    }

    // ------------------------------------------------------------------------
    //  TASK: Bridge STATUS
    // ------------------------------------------------------------------------
    
    static void Task_Bridge_Status(DomoManager& manager,
                            unsigned long now) {
        // ============================================================
        // 1) Serializza SOLO le variabili AEE cambiate
        // ============================================================
        if (!TaskEngine::aee)
            return;

        String json = AEEProtocol::serializeChangedJSON(*TaskEngine::aee);

        // Nessun cambiamento → niente invio
        if (json.length() == 0)
            return;

        // 🔥 2) Invia JSON in un unico pacchetto
        BridgeEngine::SendRaw(json);

        LOG_IF("AEE", "[SEND] %s", json.c_str());
    }


    // ------------------------------------------------------------------------
    //  TASK: HVAC
    // ------------------------------------------------------------------------
    static void Task_HVAC(DomoManager& manager, unsigned long now)
    {
        const auto& cfg = TaskEngineOrchestrator::getCfg().hvac;
        
        // 2) Costruisci HVACTime usando TimeManager (NON getRTC!)
        struct tm t;
        manager.getTimeManager().getDateTime(t);

        HeatPumpController::HVACTime ht;
        ht.dayOfWeek = t.tm_wday;
        ht.hour      = t.tm_hour;
        ht.minute    = t.tm_min;

        // 3) Temperature per zona dal buffer del DomoManager
        auto& buf = manager.getBuffer();

        // dimensione massima ragionevole; se hai un limite noto, mettilo qui
        static float zoneTemps[16];

        for (size_t i = 0; i < cfg.zoneCount && i < 16; ++i) {
            int area = cfg.zones[i].temperatureArea;
            zoneTemps[i] = buf.getValueFast(area) / 10.0f;   // scaling tipico /10
        }

        // 4) Temperature globali e finestra
        float tInterna      = cfg.readIndoorTemp  ? cfg.readIndoorTemp()  : 0.0f;
        float tEsterna      = cfg.readOutdoorTemp ? cfg.readOutdoorTemp() : 0.0f;
        bool finestraAperta = cfg.readWindowOpen  ? cfg.readWindowOpen()  : false;

        // 5) Chiamata al motore HVAC (wrapper DMHVAC)
        HVACEngine::Loop(
            now,
            zoneTemps,
            tInterna,
            tEsterna,
            finestraAperta,
            ht
        );
    }


    // ------------------------------------------------------------------------
    //  TASK: Averages
    // ------------------------------------------------------------------------
    static void Task_Averages(DomoManager& manager, unsigned long now) {
        const auto& cfg = TaskEngineOrchestrator::getCfg();

        auto& buffer = manager.getBuffer();
        auto& averages  = manager.getAverages();

        // ============================================================
        // 1. Per ogni gruppo configurato
        // ============================================================
        for (size_t g = 0; g < cfg.averages.gruppiCount; g++) {
            const auto& gruppo = cfg.averages.gruppi[g];

            // 2. Aggiungi tutte le misure dei sensori del gruppo
            for (size_t i = 0; i < gruppo.count; i++) {
                const auto& s = gruppo.sensori[i];

                float raw = buffer.getValueFast(s.area);
                float value = raw * s.factor;

                averages.addMeasurement(gruppo.nome, value);
            }

            // 3. Calcola la media del gruppo
            float average = averages.groupAverage(gruppo.nome);

            // 4. Scrivi nell’area configurata (se presente)
            if (gruppo.areaOut >= 0) {
                buffer.WriteElement(
                    gruppo.areaOut,
                    ToPanel,
                    average * gruppo.outScale,
                    now
                );
            }

            LOG_DF("AVERAGES", "%s = %.2f", gruppo.nome, average);
        }

        // eventuale invio bridge...
    }

    // ------------------------------------------------------------------------
    //  TASK: METEO
    // ------------------------------------------------------------------------
    static void Task_Meteo(DomoManager& manager, unsigned long now) {
        WeatherEngine::Loop(now);
    }
    

    // ------------------------------------------------------------------------
    //  TASK: POWER
    // ------------------------------------------------------------------------
    static void Task_Power(DomoManager& manager, unsigned long now) {
        auto& buffer   = manager.getBuffer();
        auto& averages = manager.getAverages();

        // --- LETTURE ---
        int   gridPower        = buffer.getValueFast(13, 100);
        float lux              = 800;   // TODO: sensore reale
        float tempExt          = 7;     // TODO: sensore reale
        float actualProduction = 0;     // TODO: lettura inverter

        struct tm t;
        manager.getTimeManager().getDateTime(t);

        // --- CICLO POWER COMPLETO ---
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

        // --- DIAGNOSTICA ---
        auto& pm = PowerEngine::Get();

        LOG_DF("Power",
            "grid=%0.2f solar=%0.2f tempExt=%0.2f lux=%0.2f",
            pm.getGridPower(),
            pm.getSolarPower(),
            tempExt,
            lux
        );
    }


    // ------------------------------------------------------------------------
    //  TASK: SENSORS
    // ------------------------------------------------------------------------
    static void Task_Sensors(DomoManager& manager, unsigned long now) {
        // esegui solo dopo il ciclo di power-on
        if (!manager.getPowerOnCycleCompleted())
            return;

        // --- 1) ESECUZIONE SECURITY ENGINE ---
        bool changed = SecuritySensorEngine::Loop(now);

        if (!changed)
            return;   // nessun cambiamento → nessun update

        auto& sys = SecurityOrchestrator::getSystem();
        
        // --- 3) SCRITTURA BITMASK NEL BUFFER ---
        auto& buffer = manager.getBuffer();
        buffer.WriteElement(
            AREA_SECURITY_STATUS,
            BufferFlagType::Field,
            sys.getBitmask(),
            now
        );
    }

    static void Task_PowerSupervisor(DomoManager& manager, unsigned long now) {
        PowerSupervisorEngine::Loop(now);
    }

    static void Task_AEE_Monitor(DomoManager& manager, unsigned long now) {
        if (!TaskEngine::aee)
            return;

        TaskEngine::aee->forEachChanged([](AEEVariableBase* v){
            switch (v->def.varType) {
                case AEEVarType::BOOL:
                    LOG_IF("AEE-MON", "%s = %d", 
                        v->def.name, 
                        as<bool>(v)->get());
                    break;

                case AEEVarType::INT:
                    LOG_IF("AEE-MON", "%s = %d", 
                        v->def.name, 
                        as<int>(v)->get());
                    break;

                case AEEVarType::FLOAT:
                    LOG_IF("AEE-MON", "%s = %.2f", 
                        v->def.name, 
                        as<float>(v)->get());
                    break;

                default:
                    break;
            }
        });

        TaskEngine::aee->clearAllChanged();
    }

    static void Task_AEE_Dump(DomoManager& manager, unsigned long now) {
        if (!TaskEngine::aee)
            return;

        StaticJsonDocument<1024> doc;

        TaskEngine::aee->forEach([&](AEEVariableBase* v){
            v->toJson(doc);
        });

        String out;
        serializeJson(doc, out);

        LOG_IF("AEE-DUMP", "%s", out.c_str());
    }

    static void Task_Communication(DomoManager& manager, unsigned long now) 
    {
        #if HOTSTANDBY_ENABLED
            // Se non sei MASTER → NON devi fare comunicazione
            if (!manager.isClusterMaster())
                return;
        #endif

        // Il nodo MASTER deve comunque attendere il primo ciclo completo
        if (!manager.getPowerOnCycleCompleted())
            return;

        const auto& cfg = TaskEngineOrchestrator::getCfg();
        // Round‑robin index
        static uint8_t rr = 0;

        switch (rr)
        {
            case 0:
                if (cfg.webApi.enabled)
                    WebAPIEngine::Loop(now);
                break;

            case 1:
                if (cfg.bridge.enabled)
                    BridgeEngine::Loop(now);
                break;

            case 2:
                if (cfg.mqtt.enabled)
                    MQTTEngine::Loop(manager, now);
                break;
        }

        rr++;
        if (rr > 2) rr = 0;
    }


public:

    // ------------------------------------------------------------
    //  AEE ATTACH (solo frontend)
    // ------------------------------------------------------------
    static void AttachAEE(AEERegistry* reg, AEEManagement* mgr) {
        aee = reg;
        aeeMgr = mgr;
    }

    // ------------------------------------------------------------
    //  SETUP: registra i task standard
    // ------------------------------------------------------------
    static void Setup(const FrontendConfig& cfg) {

        TaskEngineOrchestrator::Setup(cfg);

        // Task standard del frontend
        TaskEngineOrchestrator::AddTask(Task_Sensors,         cfg.security.intervalMs, cfg.security.enabled );
        TaskEngineOrchestrator::AddTask(Task_HVAC,            cfg.hvac.intervalMs, cfg.hvac.enabled);
        TaskEngineOrchestrator::AddTask(Task_Meteo,           cfg.weather.intervalMs, cfg.weather.enabled);
        TaskEngineOrchestrator::AddTask(Task_Power,           cfg.power.intervalMs, cfg.power.enabled);
        TaskEngineOrchestrator::AddTask(Task_PowerSupervisor, cfg.ps.intervalMs, cfg.ps.enabled);
        TaskEngineOrchestrator::AddTask(Task_Averages,        cfg.averages.intervalMs, cfg.averages.enabled);
        TaskEngineOrchestrator::AddTask(Task_Bridge_Status,   cfg.bridge.intervalMs, cfg.bridge.enabled);
        TaskEngineOrchestrator::AddTask(Task_Communication,   100, true); // round robin interno
        TaskEngineOrchestrator::AddTask(Task_Jobs,            cfg.jobs.intervalMs, cfg.jobs.enabled);
    }

    // ------------------------------------------------------------
    //  AGGIUNTA TASK CUSTOM DAL FRONTEND
    // ------------------------------------------------------------
    static void AddCustomTask(TaskEngineOrchestrator::TaskFn fn, uint32_t intervalMs, bool enabled) {
        TaskEngineOrchestrator::AddTask(fn, intervalMs, enabled);
    }

    // ------------------------------------------------------------
    //  LOOP FRONTEND
    // ------------------------------------------------------------
    static void Loop(DomoManager& manager, unsigned long now) {
        TaskEngineOrchestrator::Loop(manager, now);
    }

    static bool hasFrontendCycleCompleted() {
        return TaskEngineOrchestrator::hasFrontendCycleCompleted();
    }

    static void resetFrontendCycleFlag() {
        TaskEngineOrchestrator::resetFrontendCycleFlag();
    }

};


class DomoManagerFrontendEngine {
private:
    // ------------------------------------------------------------
    //  ETHERNET + MODBUS
    // ------------------------------------------------------------
    static EthernetServer& ethServer() {
        static EthernetServer server; 
        return server;
    }

    static EthernetClient& ethClient() {
        static EthernetClient client;
        return client;
    }

    static ModbusTCPClient& modbusTCPClient() {
        static ModbusTCPClient client(ethClient());
        return client;
    }

    // --- istanza del pulsante ---
    static inline ButtonManager* btnUSER;

    static inline AEERegistry* aee = nullptr;
    static inline AEEManagement* aeeMgr = nullptr;
        
    static inline bool backendDone;
    static inline bool frontendDone;
    static inline void (*fullCycleCallback)(DomoManager&) = nullptr;
    static inline FrontendConfig config;
    static inline DomoManager* Manager;

    static void OnOwnerModeChanged(
        OwnerManager::Mode oldMode,
        OwnerManager::Mode newMode,
        OwnerManager::Reason reason)
    {
        auto& dm = *DomoManager::instance;
        auto& owner = dm.getOwner();

        // Attiva/disattiva log
        if (newMode == OwnerManager::DEVELOPER)
            LogManager::enable();
        else
            LogManager::disable();

        const char* newModeStr =
            (newMode == OwnerManager::DEVELOPER) ? "DEVELOPER" :
            (newMode == OwnerManager::GUEST)     ? "GUEST" :
                                                "OWNER";

        LOG_IF("FrontendEngine",
            "Callback OwnerModeChanged: %s → %s",
            owner.getModeName(),
            newModeStr);
    }

    static void CheckFullCycle(unsigned long now) {
        auto dm = DomoManager::instance;

        bool backend = dm->hasBackendCycleCompleted();
        bool frontend = TaskEngine::hasFrontendCycleCompleted();

        if (backend && frontend) {
            dm->resetBackendCycleFlag();
            TaskEngine::resetFrontendCycleFlag();

            if (fullCycleCallback)
                fullCycleCallback(*DomoManager::instance);
        }
    }

    static void SomethingChanged() {
        auto& manager = *DomoManager::instance;
        auto& buffer  = manager.getBuffer();
        
        // 1) Logiche interne
        auto changedFromPanel = buffer.getChangedMapByType(FromPanel);
        if (!changedFromPanel.empty()) {
            LOG_WF("Main", "SomethingChanged PANEL: %d elementi modificati", changedFromPanel.size());

            for (const auto &kv : changedFromPanel) {
                int key = kv.first;
                int area = key / BufferFlagType_Count;

                SecurityOrchestrator::ApplySecurityCommands(area);
                
                switch (area) {
                    // Nessun case definito nel codice originale
                }
            }
        }
    }

    static void watchdogHandler(const Watchdog::WatchdogStatus& st) {
        LOG_WF("Main", "===== WATCHDOG EVENT =====");
        LOG_WF("Main", "Reason: %s", st.reason);
        LOG_WF("Main", "Value: %ld", st.value);

        if (st.blocked)   LOG_WF("Main", "Type: BLOCKED");
        if (st.overload)  LOG_WF("Main", "Type: OVERLOAD");
        if (st.unstable)  LOG_WF("Main", "Type: UNSTABLE");
        if (st.inactive)  LOG_WF("Main", "Type: INACTIVE");

        if (st.blocked) {
            LOG_EF("Main", "<<<<<<<<+>>>>>>>>");
            LOG_EF("Main", "Action: System reset due to BLOCKED callback");
            NVIC_SystemReset();
        }

        if (st.overload && st.value > 200) {
            LOG_WF("Main", "Action: Severe overload detected");
        }

        if (st.unstable) {
            LOG_WF("Main", "Action: System unstable, logging event");
        }

        LOG_WF("Main", "==========================");
    }


    // ------------------------------------------------------------
    //  ETHERNET SETUP
    // ------------------------------------------------------------
    static void SetupEthernet(const FrontendConfig& cfg) {
        uint8_t mac[6];
        for (int i = 0; i < 6; ++i)
            mac[i] = cfg.net.mac[i];

        Ethernet.begin(mac, cfg.net.ip, cfg.net.gateway, cfg.net.subnet);

        if (Ethernet.hardwareStatus() == EthernetNoHardware) {
            LOG_EF("DomoManagerFrontendEngine", "Ethernet shield not found");
            while (true) { delay(100); }
        }

        if (Ethernet.linkStatus() == LinkOFF) {
            LOG_EF("DomoManagerFrontendEngine", "Ethernet cable is not connected");
            digitalWrite(LED_USER, true);
        } else {
            LOG_IF("DomoManagerFrontendEngine", "Ethernet interface started");
            LOG_IF("DomoManagerFrontendEngine", "My IP address: %d.%d.%d.%d",
                Ethernet.localIP()[0],
                Ethernet.localIP()[1],
                Ethernet.localIP()[2],
                Ethernet.localIP()[3]);
        }

        if(cfg.domoManager.hmi.enabled )
            ethServer().begin(cfg.domoManager.hmi.port);
    }

    // ------------------------------------------------------------
    //  REGISTRAZIONE DIAGNOSTICA FRONTEND
    // ------------------------------------------------------------
    static void RegisterFrontendDiagnostics(const FrontendConfig& cfg) {
        Diagnostic::frontendDiagnosticCallback() = [cfg]() {
            Serial.println("\n===== FRONTEND DIAGNOSTIC =====");

            if (cfg.ps.enabled)
                PowerSupervisor::Diagnostic::Report(PowerSupervisorOrchestrator::Get());
            
            if (cfg.power.enabled)
                PowerManager::Diagnostic::FullReport(PowerEngine::Get());
            
            if (cfg.hvac.enabled)
                HeatPumpController ::Diagnostic::Report(HVACEngine::GetHP());
            
            if (cfg.weather.enabled)
                WeatherStation::Diagnostic::Report(WeatherEngine::Get());
            
            if (cfg.security.enabled) {
                SecurityOrchestrator::Diagnostic::FullReport();
                SecuritySensorEngine::getSystem().DiagnosticReport();

            }
            Serial.println("===== END FRONTEND DIAGNOSTIC =====\n");
        };
    }

    // ------------------------------------------------------------
    //  INIT (solo Engine)
    // ------------------------------------------------------------
    static void InitEngines(DomoManager& manager) {
        if(config.security.enabled) {
            SecuritySensorEngine::Setup(config.security);
        }

        if(config.hvac.enabled)
            HVACEngine::Setup(config.hvac);
        
        if(config.weather.enabled)
            WeatherEngine::Setup(config.weather);
        
        if(config.power.enabled)
            PowerEngine::Setup(config.power);

        if(config.ps.enabled)
            PowerSupervisorEngine::Setup(config.ps);   

        JobsEngine::Setup();
        
        if (config.domoManager.automation.json) {
            manager.loadAutomationJson(config.domoManager.automation.json);
            LOG_I("Automation", "Automation JSON loaded from FrontendConfig");
        }

        WebAPIEngine::Setup(config.webApi);

        if(config.watch.enabled)
            WatchEngine::attach(manager, config.watch);
        
        RegisterFrontendDiagnostics(config);    
    }

    static void SetFullCycleCallback(void (*fn)(DomoManager&)) {
        fullCycleCallback = fn;
    }

public:  

    // ------------------------------------------------------------
    //  SETUP COMPLETO
    // ------------------------------------------------------------
    static void Setup(const FrontendConfig& cfg)
    {
        // 🔥 Salva la configurazione globale
        config = cfg;

        // --- USER BUTTON ---
        btnUSER = new ButtonManager(cfg.pins.userButton);
        btnUSER->begin();

        LOG_I("DomoManagerFrontendEngine", "DomoManager is starting...");

        AEEEngine::instance().Setup(cfg.bridge.aee);

        Manager = new DomoManager(
            cfg.pins.leds
        );

        // --- ETHERNET ---
        SetupEthernet(cfg);

        // --- MODBUS TCP ---
        modbusTCPClient().setTimeout(cfg.modbus.timeoutMs);
        LOG_IF("DomoManagerFrontendEngine", "Modbus TCP to RTU timeout set %d mSec.", cfg.modbus.timeoutMs);
        delay(2000);

        // --- TASK ENGINE ---
        TaskEngine::Setup(cfg);

        // --- WATCHDOG CALLBACK ---
        Manager->SetWatchdogCallback(watchdogHandler);
        
        // --- OPERAZIONI DI BOOT (spostate dal main) ---
        auto& dm = *DomoManager::instance;

        // --- DOMO MANAGER CORE ---
        if(!Manager->setup(SomethingChanged, TaskEngine::Loop, config.domoManager )) {
            while(1);
        };
        
        dm.getOwner().setCallback(DomoManagerFrontendEngine::OnOwnerModeChanged);
                
        // --- INIZIALIZZA TUTTI GLI ENGINE ---
        InitEngines(*Manager);   

        LOG_IF("AEE", "AEE initialized with %d variables", cfg.bridge.aee.count);

        // --- HOTSTANDBY / MQTT / Bridge ---
        #if HOTSTANDBY_ENABLED
            Manager.enableHotStandby(false);
        #else
            if (cfg.mqtt.enabled)
                MQTTEngine::Setup(*Manager, ethClient(), cfg.mqtt);

            if (cfg.bridge.enabled)  {
                static UdpAdapter transport(
                    cfg.bridge.ip,
                    cfg.bridge.localPort,
                    cfg.bridge.remotePort
                );


                BridgeEngine::Init(&transport, AEEEngine::getAEE());
            }

        #endif

        DomoManagerFrontendEngine::SetFullCycleCallback([](DomoManager& dm) {
            auto& buffer  = dm.getBuffer();
            auto& changed = buffer.getChangedMap();

            if (changed.empty())
                return;

            std::vector<DMAEE::Update> updates;
            updates.reserve(32);

            bool hasUpdates = DMAEE::BuildUpdatesFromBuffer(
                AEEEngine::getMgr(),
                buffer,
                changed,
                updates
            );

            if (!hasUpdates) {
                buffer.ResetAll(5000);
                return;
            }

            DMAEE::ApplyUpdates(
                updates,
                millis()
            );

            buffer.ResetAll(5000);
        });

        // --- WATCHDOG ---
        Manager->enableWatchdog();
        LOG_IF("DOMO MANAGER", "Sistema inizializzato");

        dm.getOwner().setDeveloper(OwnerManager::SYSTEM_REBOOT);
    }

    // ------------------------------------------------------------
    //  LOOP COMPLETO
    // ------------------------------------------------------------
    static void loop() {
        unsigned long now = Manager->getTimeManager().nowMs();
        
        auto st = btnUSER->update(now);
        if (st.pressedAtStartup) {
            LOG_IF("BUTTON", "Pulsante tenuto premuto allo startup");
            Manager->getWatchDiag().onButtonPressed(*Manager, 0, config.diagnostic );
        } else  if (st.pressedNow) {
            LOG_IF("BUTTON", "Pulsante premuto durante il loop");
            Manager->getWatchDiag().onButtonPressed(*Manager, 1, config.diagnostic);
        }
        
        #if HOTSTANDBY_ENABLED
            static bool lastMaster = false;
            bool isMaster = Manager->isClusterMaster();

            // Transizione di ruolo → gestisco Ethernet SOLO QUI
            if (isMaster != lastMaster) {
                if (isMaster) {
                    LOG_I("Main", "Passo a MASTER → abilito Ethernet");
                    SetupEthernet(config);
                } else {
                    LOG_I("Main", "Passo a SLAVE → disabilito Ethernet");
                    ethServer().end();
                    Ethernet.end();
                }
                lastMaster = isMaster;
            }
        #endif
        // Se SLAVE → niente logica applicativa, solo standby (già gestito da Manager.loop)
        static EthernetClient client;
        #if HOTSTANDBY_ENABLED
            if (isMaster) {
        #else
            if (true) {
        #endif
            if(config.domoManager.hmi.enabled) {
                if (!client || !client.connected()) {
                    client = ethServer().available();
                    if (client) {
                        LOG_IF("Main", "Nuovo client connesso");
                    }
                }
            }

            Manager->loop(client, modbusTCPClient(), now);

        } else {
            // SLAVE → nessun client, nessun Bridge, solo:
            Manager->loop(client, modbusTCPClient(), now);
        }
        // 🔥 dopo aver chiamato Manager->loop
        CheckFullCycle(now);  

    }
};

   

#endif
