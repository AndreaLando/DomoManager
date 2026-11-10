
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
class PowerEngine
{
private:

    // ============================================================
    // REPORTING
    // ============================================================

    /**
     * @brief Abilita o disabilita i report diagnostici automatici.
     *
     * Il valore viene inizializzato da:
     *
     * FrontendConfig::Power::reporting
     */
    static inline bool reportingEnabled =
        false;

    /**
     * @brief Impedisce l'esecuzione ricorsiva del report.
     */
    static inline bool reportInProgress =
        false;

    /**
     * @brief Genera il report diagnostico completo del PowerManager.
     *
     * Il report viene eseguito solamente quando reportingEnabled
     * è true.
     *
     * La stampa effettiva viene delegata al DiagnosticManager
     * tramite PowerManager::Diagnostic.
     *
     * @param eventName Nome dell'evento che ha generato il report.
     */
    static void reportChange(
        const char* eventName)
    {
        if (!reportingEnabled)
            return;

        if (reportInProgress)
            return;

        reportInProgress =
            true;

        LOG_IF(
            "POWER",
            "Diagnostic report triggered by: %s",
            eventName != nullptr
                ? eventName
                : "UNKNOWN");

        PowerManager::Diagnostic::FullReport(
            PowerOrchestrator::Get());

        reportInProgress =
            false;
    }

    // ============================================================
    // CALLBACK CARICHI
    // ============================================================

    /**
     * @brief Riceve la conferma del cambio di stato di un carico.
     *
     * La callback viene eseguita sia per i carichi elettrici sia
     * per il carico termico.
     *
     * @param name Nome del carico.
     * @param state Nuovo stato reale.
     */
    static void onLoadChange(
        const String& name,
        bool state)
    {
        LOG_IF(
            "POWER",
            "[EVENT] %s -> %s",
            name.c_str(),
            state
                ? "ON"
                : "OFF");

        reportChange(
            state
                ? "LOAD_ON"
                : "LOAD_OFF");
    }

    // ============================================================
    // CALLBACK LIMITI
    // ============================================================

    /**
     * @brief Riceve la transizione nello stato WARNING.
     *
     * @param netPower Potenza netta corrente.
     * @param limit Limite operativo corrente.
     */
    static void onLimitWarning(
        float netPower,
        float limit)
    {
        LOG_IF(
            "POWER",
            "[WARNING] net=%0.2f W, limit=%0.2f W",
            netPower,
            limit);

        reportChange(
            "LIMIT_WARNING");
    }

    /**
     * @brief Riceve la transizione nello stato EXCEEDED.
     *
     * @param netPower Potenza netta corrente.
     * @param limit Limite operativo corrente.
     */
    static void onLimitExceeded(
        float netPower,
        float limit)
    {
        LOG_IF(
            "POWER",
            "[EXCEEDED] net=%0.2f W, limit=%0.2f W",
            netPower,
            limit);

        reportChange(
            "LIMIT_EXCEEDED");
    }

    // ============================================================
    // CALLBACK ERRORI
    // ============================================================

    /**
     * @brief Riceve un errore dal PowerManager.
     *
     * @param code Codice numerico dell'errore.
     * @param message Descrizione dell'errore.
     */
    static void onPowerError(
        int code,
        const String& message)
    {
        LOG_IF(
            "POWER",
            "[ERROR] code=%d msg=%s",
            code,
            message.c_str());

        reportChange(
            "POWER_ERROR");
    }

    // ============================================================
    // CALLBACK SUGGESTION
    // ============================================================

    /**
     * @brief Riceve una suggestion di attacco o distacco.
     *
     * La suggestion rappresenta una richiesta ancora in attesa
     * di conferma da parte dell'attuatore.
     *
     * @param suggestion Azione richiesta.
     * @param severity Severità della richiesta.
     * @param reason Motivazione della richiesta.
     */
    static void onSuggestion(
        const String& suggestion,
        int severity,
        const String& reason)
    {
        LOG_IF(
            "POWER",
            "[SUGGESTION] sev=%d %s reason=%s",
            severity,
            suggestion.c_str(),
            reason.c_str());

        /*
         * Il nome dell'evento diagnostico distingue i distacchi
         * causati dall'hard limit dalle suggestion ordinarie.
         */
        if (reason == "hard limit" ||
            reason == "hard limit forced")
        {
            reportChange(
                "HARD_LIMIT_SHED");
        }
        else
        {
            reportChange(
                "SUGGESTION");
        }
    }

public:

    // ============================================================
    // SETUP FRONTEND POWER
    // ============================================================

    /**
     * @brief Configura il frontend Power.
     *
     * La funzione:
     * - acquisisce il flag di reporting;
     * - inizializza PowerOrchestrator;
     * - registra tutte le callback frontend.
     *
     * Non genera un report durante il setup perché i dati runtime
     * potrebbero non essere ancora inizializzati.
     *
     * @param cfg Configurazione del sottosistema Power.
     */
    static void Setup(
        const FrontendConfig::Power& cfg)
    {
        reportingEnabled =
            cfg.reporting;

        reportInProgress =
            false;

        PowerOrchestrator::Setup(
            cfg,
            3000,
            3500,
            onLoadChange,
            onLimitWarning,
            onLimitExceeded,
            onPowerError,
            onSuggestion);

        LOG_IF(
            "POWER",
            "Frontend Power setup completed, reporting=%s",
            reportingEnabled
                ? "ON"
                : "OFF");
    }

    // ============================================================
    // LOOP FRONTEND POWER
    // ============================================================

    /**
     * @brief Esegue il ciclo del sottosistema Power.
     *
     * Il reporting non viene eseguito direttamente nel loop.
     * I report vengono prodotti solamente dalle callback quando
     * si verifica un cambiamento reale.
     *
     * @param now Timestamp corrente.
     * @param gridPower Potenza di scambio con la rete.
     */
    static void Loop(
        unsigned long now,
        int gridPower)
    {
        PowerOrchestrator::Loop(
            now,
            gridPower);
    }

    // ============================================================
    // CONTROLLO REPORTING RUNTIME
    // ============================================================

    /**
     * @brief Abilita o disabilita il reporting a runtime.
     *
     * @param enabled true per abilitare i report.
     */
    static void SetReporting(
        bool enabled)
    {
        reportingEnabled =
            enabled;

        LOG_IF(
            "POWER",
            "Reporting changed to %s",
            reportingEnabled
                ? "ON"
                : "OFF");
    }

    /**
     * @brief Restituisce lo stato corrente del reporting.
     */
    static bool IsReportingEnabled()
    {
        return
            reportingEnabled;
    }

    /**
     * @brief Forza manualmente un report diagnostico.
     *
     * Il metodo rispetta comunque il flag reportingEnabled.
     */
    static void Report()
    {
        reportChange(
            "MANUAL_REPORT");
    }

    // ============================================================
    // ACCESSOR
    // ============================================================

    /**
     * @brief Restituisce l'istanza del PowerManager.
     */
    static PowerManager& Get()
    {
        return
            PowerOrchestrator::Get();
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


// ============================================================
// DOMO MANAGER FRONTEND ENGINE - REFACTORED
//
// Requires these types to be defined before this class:
//   FrontendRuntime, FrontendNetwork, MQTTEngine,
//   HAMQTTTransport, DMHAOrchestrator, DomoIntrospection,
//   HMIEngine and the existing frontend engines/orchestrators.
// ============================================================

class DomoManagerFrontendEngine : public FrontendRuntime
{
private:
    static inline DomoManagerFrontendEngine* instance = nullptr;
    static inline FrontendNetwork* network = nullptr;
    
    // ========================================================
    // AEE / BRIDGE OWNERSHIP
    //
    // Le classi non sono singleton.
    // Le istanze appartengono al frontend concreto.
    //
    // L'ordine è intenzionale:
    // - AEEEngine viene costruito prima
    // - BridgeEngine viene costruito dopo
    // - in distruzione BridgeEngine viene eliminato prima
    // ========================================================

    AEEEngine aeeEngine;
    BridgeEngine peerBridgeEngine;

    static inline void (*fullCycleCallback)(DomoManager&) = nullptr;
    
    static inline DomoManager* Manager = nullptr;
    static HMIEngine hmiEngine;

    static inline DomoIntrospection introspection;
    static inline DMHAOrchestrator homeAssistantOrchestrator;
    static inline uint8_t mqttEventSource = 255;
            
    // ========================================================
    // NETWORK REGISTRATION
    // ========================================================

    static inline DMNetworkManager::ProtocolId
        webApiClientProtocolId = -1;

    static inline DMNetworkManager::ProtocolId
        webApiServerProtocolId = -1;

    static inline DMNetworkManager::ProtocolId
        solarProtocolId = -1;

    static inline DMNetworkManager::ProtocolId
        mqttProtocolId = -1;

    // ========================================================
    // CURRENT CONFIGURATION
    //
    // La configurazione è posseduta esclusivamente da
    // FrontendRuntime.
    //
    // I metodi statici del composition root accedono alla stessa
    // istanza attraverso il frontend corrente.
    // ========================================================

    static FrontendConfig& Config()
    {
        return
            instance->getConfig();
    }

    static const FrontendConfig& ConfigReadOnly()
    {
        return
            instance->getConfig();
    }

    // ========================================================
    // FRONTEND GENERAL CALLBACKS
    // ========================================================

    static void CheckFullCycle()
    {
        DomoManager* dm = DomoManager::instance;
        if (!dm)
            return;

        const bool backend = dm->hasBackendCycleCompleted();
        const bool frontend = TaskEngine::hasFrontendCycleCompleted();

        if (!backend || !frontend)
            return;

        dm->resetBackendCycleFlag();
        TaskEngine::resetFrontendCycleFlag();

        if (fullCycleCallback)
            fullCycleCallback(*dm);
    }

    // ========================================================
    // AREAS CHANGED CALLBACK
    //
    // Callback richiesta da DomoManager::setup().
    //
    // Le variazioni delle singole aree vengono già elaborate
    // dai sottosistemi registrati nel frontend.
    //
    // Il callback rimane intenzionalmente vuoto come punto di
    // estensione per eventuali elaborazioni aggregate future.
    // ========================================================

    static void OnAreasChanged(
        const std::unordered_set<int>& changed)
    {
        (void)changed;
    }

    static void watchdogHandler(const Watchdog::WatchdogStatus& st)
    {
        LOG_WF("Main", "===== WATCHDOG EVENT =====");
        LOG_WF("Main", "Reason: %s", st.reason);
        LOG_WF("Main", "Value: %ld", st.value);

        if (st.blocked)  LOG_WF("Main", "Type: BLOCKED");
        if (st.overload) LOG_WF("Main", "Type: OVERLOAD");
        if (st.unstable) LOG_WF("Main", "Type: UNSTABLE");
        if (st.inactive) LOG_WF("Main", "Type: INACTIVE");

        if (st.blocked)
        {
            LOG_EF("Main", "<<<<<<<<+>>>>>>>>");
            LOG_EF("Main", "Action: System reset due to BLOCKED callback");
            DMPlatform::Restart();
        }

        if (st.overload && st.value > 200)
            LOG_WF("Main", "Action: Severe overload detected");

        if (st.unstable)
            LOG_WF("Main", "Action: System unstable, logging event");

        LOG_WF("Main", "==========================");
    }

    static void SetupEthernet(const FrontendConfig& cfg)
    {
        uint8_t mac[6];
        for (uint8_t i = 0; i < 6; ++i)
            mac[i] = cfg.net.mac[i];

        if (!DMPlatform::Network::BeginEthernet(
                mac,
                cfg.net.ip,
                cfg.net.gateway,
                cfg.net.subnet))
        {
            LOG_EF("DomoManagerFrontendEngine", "Ethernet hardware unavailable");
            while (true)
                delay(100);
        }

        if (!DMPlatform::Network::WaitForLink(1500UL))
        {
            LOG_EF("DomoManagerFrontendEngine", "Ethernet link is DOWN");
            DMPlatform::WriteOutputPin(DMPlatform::LedUser, true);
            return;
        }

        LOG_IF("DomoManagerFrontendEngine", "Ethernet interface started");

        const IPAddress ip = DMPlatform::Network::LocalIP();
        LOG_IF(
            "DomoManagerFrontendEngine",
            "My IP address: %u.%u.%u.%u",
            ip[0], ip[1], ip[2], ip[3]
        );
    }

    static bool SetupPulseOutputs()
    {
        bool success =
            true;

        if (
            !PulseManager::Add(
                AREA_CUCINA_LUCE_1,
                1500UL
            )
        )
        {
            LOG_EF(
                "PULSE",
                "Unable to register entrance relay pulse"
            );

            success =
                false;
        }

        /*
        * Eventuali altre uscite:
        *
        * PulseManager::Add(
        *     AREA_RELAY_SECONDARIO,
        *     1500UL
        * );
        */

        return success;
    }

    static bool InitEngines(
        DomoManager& manager,
        uint8_t securitySource)
    {
        if (!instance)
        {
            LOG_EF(
                "FRONTEND",
                "InitEngines failed: "
                "frontend instance unavailable"
            );

            return false;
        }

        FrontendConfig& config =
            Config();

        // ========================================================
        // SECURITY
        // ========================================================

        if (config.security.enabled)
        {
            if (securitySource == 255)
            {
                LOG_EF(
                    "SECURITY",
                    "Security event source non valido"
                );

                return false;
            }

            if (
                !SecurityEngine::Setup(
                    manager,
                    config.security,
                    securitySource
                )
            )
            {
                LOG_EF(
                    "SECURITY",
                    "SecurityEngine setup fallito"
                );

                return false;
            }

            SecurityEngine::setAlarmCallback(
                SecurityAlarmCallback
            );

            LOG_IF(
                "SECURITY",
                "SecurityEngine inizializzato: "
                "source=%u sensors=%u zones=%u",
                static_cast<unsigned>(
                    securitySource
                ),
                static_cast<unsigned>(
                    config.security.count
                ),
                static_cast<unsigned>(
                    config.security.zoneCount
                )
            );
        }
        else
        {
            LOG_IF(
                "SECURITY",
                "SecurityEngine disabilitato"
            );
        }

        // ========================================================
        // HVAC
        // ========================================================

        if (config.hvac.enabled)
        {
            HVACEngine::Setup(
                config.hvac
            );
        }

        // ========================================================
        // WEATHER
        // ========================================================

        if (config.weather.enabled)
        {
            WeatherEngine::Setup(
                config.weather
            );
        }

        // ========================================================
        // POWER
        // ========================================================

        if (config.power.enabled)
        {
            PowerEngine::Setup(
                config.power
            );
        }

        // ========================================================
        // POWER SUPERVISOR
        // ========================================================

        if (config.ps.enabled)
        {
            PowerSupervisorEngine::Setup(
                config.ps
            );
        }

        // ========================================================
        // JOBS
        // ========================================================

        if (config.jobs.enabled)
        {
            JobsEngine::Setup();
        }

        // ========================================================
        // AUTOMATION
        // ========================================================

        if (config.domoManager.automation.json)
        {
            manager.loadAutomationJson(
                config.domoManager
                    .automation
                    .json
            );
        }

        // ========================================================
        // WATCH
        // ========================================================

        if (config.watch.enabled)
        {
            WatchEngine::attach(
                manager,
                config.watch
            );
        }

        // ========================================================
        // FRONTEND DIAGNOSTICS
        // ========================================================

        if (instance)
        {
            instance
                ->RegisterFrontendDiagnostics();
        }

        return true;
    }

    // ========================================================
    // SETUP WEB API
    //
    // Registra nel DMNetworkManager:
    //
    //   - HTTP client, se abilitato
    //   - HTTP server, se abilitato
    //
    // Gli endpoint server non appartengono più a FrontendConfig.
    // Vengono costruiti internamente da WebAPIEngine usando
    // la configurazione autonoma config.access.
    // ========================================================

    static void SetupWebAPI()
    {
        // ====================================================
        // RESET PROTOCOL IDS
        // ====================================================

        webApiClientProtocolId =
            -1;

        webApiServerProtocolId =
            -1;


        // ====================================================
        // VALIDATE MANAGER
        // ====================================================

        if (!Manager ||
            !instance)
        {
            LOG_EF(
                "WEBAPI",
                "Frontend runtime unavailable"
            );

            return;
        }

        const FrontendConfig& config =
            ConfigReadOnly();


        // ====================================================
        // CONFIGURATION STATUS
        // ====================================================

        const bool clientEnabled =
            config.webApi.client.enabled;

        const bool serverEnabled =
            config.webApi.server.enabled &&
            config.access.enabled;

        const bool accessEnabled =
            config.access.enabled;


        // ====================================================
        // COMPLETELY DISABLED
        // ====================================================

        if (
            !clientEnabled &&
            !serverEnabled &&
            !accessEnabled
        )
        {
            LOG_IF(
                "WEBAPI",
                "HTTP client, server and access control disabled"
            );

            return;
        }


        // ====================================================
        // CONFIGURATION CONSISTENCY
        // ====================================================

        if (
            accessEnabled &&
            !serverEnabled
        )
        {
            LOG_EF(
                "WEBAPI",
                "Access control enabled while HTTP server is disabled"
            );
        }


        // ====================================================
        // HTTP CLIENT / POLLING REGISTRATION
        // ====================================================

        if (clientEnabled)
        {
            const auto& clientCfg =
                config.webApi.client;

            if (
                !clientCfg.devices ||
                clientCfg.deviceCount == 0
            )
            {
                LOG_WF(
                    "WEBAPI-CLIENT",
                    "Client enabled without devices"
                );
            }
            else if (clientCfg.timeoutMs == 0)
            {
                LOG_EF(
                    "WEBAPI-CLIENT",
                    "Client enabled with invalid timeout"
                );
            }
            else
            {
                constexpr uint8_t
                    CLIENT_SOCKET_DEMAND = 1;

                webApiClientProtocolId =
                    Manager->net.registerProtocol(
                        "WebAPI-Client",
                        20,
                        20,
                        CLIENT_SOCKET_DEMAND,
                        true
                    );

                if (webApiClientProtocolId < 0)
                {
                    LOG_EF(
                        "WEBAPI-CLIENT",
                        "Protocol registration failed"
                    );
                }
                else
                {
                    LOG_IF(
                        "WEBAPI-CLIENT",
                        "Protocol registered: "
                        "id=%d devices=%u sockets=%u",
                        static_cast<int>(
                            webApiClientProtocolId
                        ),
                        static_cast<unsigned>(
                            clientCfg.deviceCount
                        ),
                        static_cast<unsigned>(
                            CLIENT_SOCKET_DEMAND
                        )
                    );
                }
            }
        }
        else
        {
            LOG_IF(
                "WEBAPI-CLIENT",
                "HTTP client disabled"
            );
        }


        // ====================================================
        // HTTP SERVER / WEBHOOK REGISTRATION
        // ====================================================

        if (serverEnabled)
        {
            const auto& serverCfg =
                config.webApi.server;

            if (serverCfg.port == 0)
            {
                LOG_EF(
                    "WEBAPI-SERVER",
                    "Server enabled with invalid port"
                );
            }
            else if (serverCfg.timeoutMs == 0)
            {
                LOG_EF(
                    "WEBAPI-SERVER",
                    "Server enabled with invalid timeout"
                );
            }
            else
            {
                /*
                * Domanda massima dichiarata:
                *
                *   1 socket listener TCP
                *   1 socket client HTTP in ingresso
                *
                * Il numero degli endpoint non influenza
                * la domanda socket.
                */
                constexpr uint8_t
                    SERVER_SOCKET_DEMAND = 2;

                webApiServerProtocolId =
                    Manager->net.registerProtocol(
                        "WebAPI-Server",
                        10,
                        10,
                        SERVER_SOCKET_DEMAND,
                        true
                    );

                if (webApiServerProtocolId < 0)
                {
                    LOG_EF(
                        "WEBAPI-SERVER",
                        "Protocol registration failed"
                    );
                }
                else
                {
                    LOG_IF(
                        "WEBAPI-SERVER",
                        "Protocol registered: "
                        "id=%d port=%u sockets=%u "
                        "access=%u",
                        static_cast<int>(
                            webApiServerProtocolId
                        ),
                        static_cast<unsigned>(
                            serverCfg.port
                        ),
                        static_cast<unsigned>(
                            SERVER_SOCKET_DEMAND
                        ),
                        accessEnabled
                            ? 1U
                            : 0U
                    );
                }
            }
        }
        else
        {
            LOG_IF(
                "WEBAPI-SERVER",
                "HTTP server disabled"
            );
        }


        // ====================================================
        // FRONTEND CALLBACKS
        // ====================================================
        SetupPulseOutputs();

        WebAPIEngine::SetWebRequestCallback(
            WebAPIRequestCallback
        );

        WebAPIEngine::SetAccessEventCallback(
            AccessEventCallback
        );

        WebAPIEngine::SetAccessAuthorizedCallback(
            AccessAuthorizedCallback
        );

        WebAPIEngine::SetDeviceResponseCallback(
            WebAPIDeviceResponseCallback
        );

        // ====================================================
        // ENGINE SETUP
        //
        // WebAPIEngine costruisce internamente:
        //
        //   - AccessControlService
        //   - FanvilWebAPI
        //   - X3WebAPI
        //   - endpoint HTTP runtime
        // ====================================================

        WebAPIEngine::Setup(
            config.webApi,
            config.access
        );


        // ====================================================
        // RESULT
        // ====================================================

        LOG_IF(
            "WEBAPI",
            "Setup completed: "
            "clientProtocol=%d serverProtocol=%d "
            "clientRunning=%u serverRunning=%u "
            "accessRunning=%u endpoints=%u",
            static_cast<int>(
                webApiClientProtocolId
            ),
            static_cast<int>(
                webApiServerProtocolId
            ),
            WebAPIEngine::IsClientRunning()
                ? 1U
                : 0U,
            WebAPIEngine::IsServerRunning()
                ? 1U
                : 0U,
            WebAPIEngine::IsAccessRunning()
                ? 1U
                : 0U,
            static_cast<unsigned>(
                WebAPIEngine::
                    GetServerEndpointCount()
            )
        );
    }

    static void SetFullCycleCallback(void (*fn)(DomoManager&))
    {
        fullCycleCallback = fn;
    }

    // ========================================================
    // EVENT CALLBACKS
    // ========================================================

    static void AEEEventCallback(
        const EventManager::Event& event)
    {
        if (!Manager ||
            !instance)
        {
            return;
        }

        AEEManagement* aeeManager =
            instance
                ->aeeEngine
                .getManager();

        if (!aeeManager)
        {
            LOG_WF(
                "AEE",
                "AEEEventCallback senza management"
            );

            return;
        }

        std::vector<DMAEE::Update>
            updates;

        updates.reserve(8);

        if (
            !DMAEE::
                BuildUpdatesFromBufferArea(
                    *aeeManager,
                    event.area,
                    event.value,
                    updates
                )
        )
        {
            return;
        }

        if (updates.empty())
            return;

        const unsigned long now =
            Manager
                ->getTimeManager()
                .nowMs();

        DMAEE::ApplyUpdates(
            updates,
            now
        );
    }

    static void SecurityAlarmCallback(
        bool engaged,
        bool active,
        uint64_t currentAlarmMask,
        uint64_t effectiveAlarmMask,
        uint64_t systemBitmask)
    {
       if (!Manager ||
            !instance)
        {
            return;
        }

        const FrontendConfig& config =
            ConfigReadOnly();

        if (!config.security.enabled)
        {
            return;
        }

        (void)active;
        (void)currentAlarmMask;
        (void)effectiveAlarmMask;

        const auto hasFlag =
            [&](SecurityOrchestrator::SystemManager::Field field)
            {
                return
                    (
                        systemBitmask &
                        (
                            1ULL <<
                            static_cast<uint64_t>(field)
                        )
                    ) != 0;
            };

        const bool alarmIntrusionMem =
            hasFlag(
                SecurityOrchestrator::
                    SystemManager::
                        ALARM_INTRUSION_MEM
            );

        const bool alarmIntrusionH24Mem =
            hasFlag(
                SecurityOrchestrator::
                    SystemManager::
                        ALARM_INTRUSION_H24_MEM
            );

        const bool alarmFloodMem =
            hasFlag(
                SecurityOrchestrator::
                    SystemManager::
                        ALARM_FLOOD_MEM
            );

        const bool alarmSmokeMem =
            hasFlag(
                SecurityOrchestrator::
                    SystemManager::
                        ALARM_SMOKE_MEM
            );

        const bool windowsOpenMem =
            hasFlag(
                SecurityOrchestrator::
                    SystemManager::
                        WINDOWS_OPEN_MEM
            );

        const bool doorsOpenMem =
            hasFlag(
                SecurityOrchestrator::
                    SystemManager::
                        DOORS_OPEN_MEM
            );

        const bool alarmTamper =
            hasFlag(
                SecurityOrchestrator::
                    SystemManager::
                        ALARM_TAMPER
            );

        const bool alarmPanic =
            hasFlag(
                SecurityOrchestrator::
                    SystemManager::
                        ALARM_PANIC
            );

        const bool alarmCamera =
            hasFlag(
                SecurityOrchestrator::
                    SystemManager::
                        ALARM_CAMERA
            );

        const bool serviceAlarm =
            alarmTamper ||
            alarmPanic;

        LOG_IF(
            "SECURITY",
            "AlarmCallback engaged=%u active=%u systemMask=0x%016llX",
            engaged ? 1 : 0,
            active ? 1 : 0,
            (unsigned long long)systemBitmask
        );

        if (alarmCamera)
        {
            LOG_IF(
                "SECURITY",
                "Camera detection active"
            );
        }

        if (!engaged && !serviceAlarm)
        {
            DMPlatform::WriteOutputPin(
                DMPlatform::SIREN,
                false
            );

            DMPlatform::WriteOutputPin(
                DMPlatform::ALARM,
                false
            );

            return;
        }

        const bool siren =
            alarmIntrusionMem ||
            windowsOpenMem ||
            doorsOpenMem ||
            alarmPanic ||
            alarmCamera;

        const bool alarmOutput =
            alarmIntrusionH24Mem ||
            alarmFloodMem ||
            alarmSmokeMem ||
            serviceAlarm;

        DMPlatform::WriteOutputPin(
            DMPlatform::SIREN,
            siren
        );

        DMPlatform::WriteOutputPin(
            DMPlatform::ALARM,
            alarmOutput
        );
    }

    static void HMIEventCallback(const EventManager::Event& event)
    {
        LOG_DF(
            "HMI",
            "RX area=%d value=%ld source=%u",
            event.area,
            event.value,
            event.source
        );

        if (!hmiEngine.enabled())
            return;

        hmiEngine.PushEvent(event);

        if (Manager &&
            Manager->getLeds().hasChannel(LedController::THREE))
        {
            static bool ledState = false;
            ledState = !ledState;
            Manager->getLeds().set(LedController::THREE, ledState);
        }
    }

    // ========================================================
    // SOLAR CALLBACKS
    // ========================================================

    static void OnSolarSnapshot(
        uint8_t orchestratorId,
        const SolarEngine::Snapshot& snapshot)
    {
        if (!instance)
        {
            return;
        }

        const FrontendConfig& config =
            ConfigReadOnly();

        const uint8_t configuredCount =
            config.solarInverters.count <
                FrontendConfig::
                    MAX_SOLAR_INVERTERS
                ? config.solarInverters.count
                : FrontendConfig::
                    MAX_SOLAR_INVERTERS;

        if (
            orchestratorId >=
            configuredCount
        )
        {
            return;
        }

        const auto& solar =
            config
                .solarInverters
                .inverter[
                    orchestratorId
                ];

        if (!solar.reportOnChange)
        {
            return;
        }

        LOG_IF(
            "SOLAR",
            "[%u] "
            "PV=%0.1fV %0.1fA %0.0fW "
            "BAT=%0.1fV %+0.1fA SOC=%d "
            "GRID=%0.1fV %0.1fHz "
            "OUT=%0.1fV %0.1fHz %0.0fW "
            "LOAD=%u%%",
            static_cast<unsigned>(
                orchestratorId
            ),
            snapshot.pv.electrical.voltage,
            snapshot.pv.electrical.current,
            snapshot.pv.electrical.power,
            snapshot.battery.electrical.voltage,
            snapshot.battery.electrical.current,
            snapshot.battery.capacity.soc,
            snapshot.grid.electrical.voltage,
            snapshot.grid.electrical.frequency,
            snapshot.output.electrical.voltage,
            snapshot.output.electrical.frequency,
            snapshot.output.electrical.activePower,
            static_cast<unsigned>(
                snapshot.output.load.percent
            )
        );

        static unsigned long lastDump[
            FrontendConfig::
                MAX_SOLAR_INVERTERS
        ] {};

        const unsigned long now =
            millis();

        if (
            now - lastDump[
                orchestratorId
            ] >
            30000UL
        )
        {
            lastDump[
                orchestratorId
            ] =
                now;

            SolarEngine::DiagnosticReport(
                orchestratorId
            );
        }
    }

    static void OnSolarFrame(
        uint8_t orchestratorId,
        const SolarEngine::Diagnostics::Frame& frame)
    {
        LOG_DF(
            "SOLAR",
            "[%u] %s result=%d latency=%lu "
            "TX=[%s] RX=[%s]",
            static_cast<unsigned>(
                orchestratorId
            ),
            frame.command.c_str(),
            static_cast<int>(
                frame.result
            ),
            frame.latencyMs,
            frame.tx.c_str(),
            frame.rx.c_str()
        );
    }

    static void OnSolarCommandState(
        uint8_t orchestratorId,
        const SolarEngine::Diagnostics::Command& command)
    {
        LOG_DF(
            "SOLAR",
            "[%u] %s state=%d "
            "ok=%lu fail=%lu",
            static_cast<unsigned>(
                orchestratorId
            ),
            command.name.c_str(),
            static_cast<int>(
                command.state
            ),
            command.successes,
            command.failures
        );
    }

    // ========================================================
    // AEE RECEIVE CALLBACK
    // ========================================================

    static void OnAEEVarChanged(
        AEEVariableBase* variable,
        unsigned long now)
    {
        if (!instance ||
            !variable)
        {
            return;
        }

        instance
            ->aeeEngine
            .applyReceived(
                variable,
                now
            );
    }

    // ========================================================
    // COMMUNICATION SERVICE: PEER BRIDGE
    // ========================================================

    static void Task_Bridge(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;

        if (!instance)
            return;

        const unsigned long startUs =
            micros();

        instance
            ->peerBridgeEngine
            .loop(now);

        const unsigned long elapsedUs =
            micros() - startUs;

        if (elapsedUs > 100000UL)
        {
            LOG_WF(
                "BRIDGE-TASK",
                "Bridge loop lento: %lu us",
                elapsedUs
            );
        }

        static unsigned long lastAliveLog = 0;

        if (
            lastAliveLog == 0 ||
            now - lastAliveLog >= 5000UL
        )
        {
            LOG_DF(
                "BRIDGE-TASK",
                "Bridge task attivo: now=%lu elapsed=%lu us state=%d",
                now,
                elapsedUs,
                static_cast<int>(
                    instance
                        ->peerBridgeEngine
                        .getBridge()
                        .getState()
                )
            );

            lastAliveLog = now;
        }
    }

    // ========================================================
    // TASK: MQTT
    //
    // MQTT viene eseguito direttamente dal task scheduler.
    // Non utilizza il CommunicationScheduler.
    // ========================================================

    static void Task_MQTT(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;

        MQTTEngine::Loop(
            now
        );
    }

    // ========================================================
    // OPTIONAL TASK: AEE DUMP
    // ========================================================

    static void Task_AEE_Dump(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;
        (void)now;

        if (!instance)
            return;

        AEERegistry& registry =
            instance
                ->aeeEngine
                .getRegistry();

        StaticJsonDocument<1024> doc;

        registry.forEach(
            [&](AEEVariableBase* variable)
            {
                if (variable)
                    variable->toJson(doc);
            }
        );

        String output;

        serializeJson(
            doc,
            output
        );

        LOG_IF(
            "AEE-DUMP",
            "%s",
            output.c_str()
        );
    }

    // ========================================================
    // MQTT / HOME ASSISTANT COMPOSITION ROOT
    // ========================================================

    static const FrontendConfig::MQTT::Client*
        FindHAClientConfig()
    {
        if (!instance)
        {
            return nullptr;
        }

        const FrontendConfig& config =
            ConfigReadOnly();

        if (
            !config.mqtt.clients ||
            config.mqtt.clientCount == 0
        )
        {
            return nullptr;
        }

        const size_t clientCount =
            config.mqtt.clientCount <
                MQTTEngine::MAX_CLIENTS
                ? config.mqtt.clientCount
                : MQTTEngine::MAX_CLIENTS;

        for (
            size_t index = 0;
            index < clientCount;
            ++index
        )
        {
            const FrontendConfig::MQTT::Client&
                client =
                    config
                        .mqtt
                        .clients[index];

            if (
                client.enabled &&
                client.backend ==
                    FrontendConfig::
                        MQTT::
                        Client::
                        Backend::
                        HOME_ASSISTANT
            )
            {
                return
                    &client;
            }
        }

        return nullptr;
    }

    static bool HAIsConnected(void* context)
    {
        (void)context;

        return MQTTEngine::IsConnected(
            FrontendConfig::MQTT::Client::Backend::HOME_ASSISTANT
        );
    }

    static bool HAPublish(
        void* context,
        const char* topic,
        const char* payload,
        bool retain)
    {
        (void)context;

        return MQTTEngine::PublishRaw(
            FrontendConfig::MQTT::Client::Backend::HOME_ASSISTANT,
            topic,
            payload,
            retain
        );
    }

    static bool HASubscribe(void* context, const char* topic)
    {
        (void)context;

        return MQTTEngine::SubscribeRaw(
            FrontendConfig::MQTT::Client::Backend::HOME_ASSISTANT,
            topic
        );
    }

    static void OnMQTTRawMessage(
        uint8_t clientIndex,
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        size_t length)
    {
        if (backend !=
            FrontendConfig::MQTT::Client::Backend::HOME_ASSISTANT)
        {
            return;
        }

        homeAssistantOrchestrator.onRawMessage(
            clientIndex,
            topic,
            payload,
            length
        );
    }

    static void OnHACommand(
        void* context,
        uint8_t originClientIndex,
        int area,
        long value)
    {
        (void)context;

        if (!Manager || area < 0)
            return;

        Manager->forceEvent(
            area,
            value,
            mqttEventSource
        );

        MQTTEngine::PublishEventFromMQTT(
            originClientIndex,
            area,
            value
        );

        LOG_IF(
            "HA_COMMAND",
            "Comando applicato dal frontend: client=%u area=%d value=%ld",
            static_cast<unsigned>(originClientIndex),
            area,
            value
        );
    }

    static void OnMQTTRoutedEvent(
        uint8_t originClientIndex,
        int area,
        long value)
    {
        (void)originClientIndex;
        homeAssistantOrchestrator.publishEvent(area, value);
    }

    static void HAServiceLoop(unsigned long now)
    {
        homeAssistantOrchestrator.loop(now);
    }

    static bool SetupHAOrchestrator()
    {
        if (!Manager)
        {
            LOG_EF(
                "HA_ORCHESTRATOR",
                "Setup fallito: Manager NULL"
            );

            return false;
        }

        const FrontendConfig::MQTT::Client*
            haClientConfig =
                FindHAClientConfig();

        if (!haClientConfig)
        {
            LOG_WF(
                "HA_ORCHESTRATOR",
                "Client HOME_ASSISTANT non configurato"
            );

            return false;
        }

        HAMQTTTransport transport;

        transport.context =
            nullptr;

        transport.connected =
            HAIsConnected;

        transport.publish =
            HAPublish;

        transport.subscribe =
            HASubscribe;

        DMHAOrchestrator::Config haConfig;

        haConfig.transport =
            transport;

        haConfig.mqttClientConfig =
            haClientConfig;

        haConfig.manager =
            Manager;

        haConfig.introspection =
            &introspection;

        haConfig.commandContext =
            nullptr;

        haConfig.commandCallback =
            OnHACommand;

        /*
        * I comandi Home Assistant già interpretati vengono
        * inoltrati allo stesso callback applicativo usato dai
        * normali mapping MQTT.
        */
        haConfig.inboundMessageCallback =
            OnMQTTInboundMessage;

        if (!homeAssistantOrchestrator.setup(
                haConfig))
        {
            LOG_EF(
                "HA_ORCHESTRATOR",
                "Inizializzazione fallita"
            );

            return false;
        }

        LOG_IF(
            "HA_ORCHESTRATOR",
            "Inizializzazione completata"
        );

        return true;
    }

    // ============================================================
    // MQTT INBOUND MESSAGE
    //
    // Punto applicativo unico per tutti i messaggi MQTT normalizzati.
    //
    // Origini supportate:
    //
    //   MAPPING_NUMERIC
    //   MAPPING_TEXT
    //   HOME_ASSISTANT_AREA
    //   HOME_ASSISTANT_RUNTIME
    //   RAW
    //
    // I puntatori topic, payload, device, mapping e runtimeId sono
    // validi solamente durante l'esecuzione sincrona della callback.
    // ============================================================

    static void OnMQTTInboundMessage(
        const MQTTInboundMessage& message)
    {
        using Origin =
            MQTTInboundMessage::Origin;

        if (!message.valid())
        {
            LOG_WF(
                "MQTT_IN",
                "Messaggio non valido: "
                "client=%u origin=%u",
                static_cast<unsigned>(
                    message.clientIndex
                ),
                static_cast<unsigned>(
                    message.origin
                )
            );

            return;
        }

        switch (message.origin)
        {
            // ====================================================
            // MAPPING NUMERICO
            // ====================================================

            case Origin::MAPPING_NUMERIC:
            {
                LOG_IF(
                    "MQTT_IN",
                    "Mapping numerico: "
                    "client=%u backend=%u "
                    "device=%s field=%s "
                    "area=%d value=%ld",
                    static_cast<unsigned>(
                        message.clientIndex
                    ),
                    static_cast<unsigned>(
                        message.backend
                    ),
                    message.device &&
                    message.device->id
                        ? message.device->id
                        : "?",
                    message.mapping &&
                    message.mapping->field
                        ? message.mapping->field
                        : "?",
                    message.area,
                    message.integerValue
                );

                break;
            }

            // ====================================================
            // MAPPING TESTUALE / JSON
            // ====================================================

            case Origin::MAPPING_TEXT:
            {
                LOG_IF(
                    "MQTT_IN",
                    "Mapping testuale: "
                    "client=%u backend=%u "
                    "device=%s field=%s "
                    "type=%u bytes=%u value='%.*s'",
                    static_cast<unsigned>(
                        message.clientIndex
                    ),
                    static_cast<unsigned>(
                        message.backend
                    ),
                    message.device &&
                    message.device->id
                        ? message.device->id
                        : "?",
                    message.mapping &&
                    message.mapping->field
                        ? message.mapping->field
                        : "?",
                    static_cast<unsigned>(
                        message.valueType
                    ),
                    static_cast<unsigned>(
                        message.payloadLength
                    ),
                    static_cast<int>(
                        message.payloadLength
                    ),
                    message.payload
                        ? message.payload
                        : ""
                );

                break;
            }

            // ====================================================
            // HOME ASSISTANT AREA
            // ====================================================

            case Origin::HOME_ASSISTANT_AREA:
            {
                LOG_IF(
                    "MQTT_IN",
                    "Home Assistant area: "
                    "client=%u area=%d value=%ld",
                    static_cast<unsigned>(
                        message.clientIndex
                    ),
                    message.area,
                    message.integerValue
                );

                break;
            }

            // ====================================================
            // HOME ASSISTANT RUNTIME STORE
            // ====================================================

            case Origin::HOME_ASSISTANT_RUNTIME:
            {
                if (message.runtimeHandle ==
                        MQTTInboundMessage::
                            INVALID_RUNTIME_HANDLE ||
                    !message.runtimeId)
                {
                    LOG_WF(
                        "MQTT_IN",
                        "Runtime message non valido: "
                        "client=%u handle=%u",
                        static_cast<unsigned>(
                            message.clientIndex
                        ),
                        static_cast<unsigned>(
                            message.runtimeHandle
                        )
                    );

                    return;
                }

                // ------------------------------------------------
                // ROOM 01 CARD CODE
                // ------------------------------------------------

                if (message.runtimeHandle ==
                    RuntimeHandles::room1Card)
                {
                    LOG_IF(
                        "MQTT_IN",
                        "Room1Card aggiornata: "
                        "client=%u handle=%u id=%s "
                        "type=%u bytes=%u value='%.*s'",
                        static_cast<unsigned>(
                            message.clientIndex
                        ),
                        static_cast<unsigned>(
                            message.runtimeHandle
                        ),
                        message.runtimeId,
                        static_cast<unsigned>(
                            message.runtimeType
                        ),
                        static_cast<unsigned>(
                            message.payloadLength
                        ),
                        message.runtimeSensitive
                            ? 8
                            : static_cast<int>(
                                message.payloadLength
                            ),
                        message.runtimeSensitive
                            ? "********"
                            : (
                                message.payload
                                    ? message.payload
                                    : ""
                            )
                    );

                    /*
                    * La scrittura nel RuntimeStore è già stata
                    * effettuata dal DMHAOrchestrator.
                    *
                    * Qui va solamente l'eventuale reazione
                    * applicativa al cambiamento.
                    */

                    break;
                }

                LOG_DF(
                    "MQTT_IN",
                    "Runtime aggiornato: "
                    "client=%u handle=%u id=%s "
                    "type=%u bytes=%u",
                    static_cast<unsigned>(
                        message.clientIndex
                    ),
                    static_cast<unsigned>(
                        message.runtimeHandle
                    ),
                    message.runtimeId,
                    static_cast<unsigned>(
                        message.runtimeType
                    ),
                    static_cast<unsigned>(
                        message.payloadLength
                    )
                );

                break;
            }

            // ====================================================
            // RAW NON RISOLTO
            // ====================================================

            case Origin::RAW:
            {
                LOG_DF(
                    "MQTT_IN",
                    "Raw: "
                    "client=%u backend=%u "
                    "topic=%s bytes=%u payload='%.*s'",
                    static_cast<unsigned>(
                        message.clientIndex
                    ),
                    static_cast<unsigned>(
                        message.backend
                    ),
                    message.topic
                        ? message.topic
                        : "?",
                    static_cast<unsigned>(
                        message.payloadLength
                    ),
                    static_cast<int>(
                        message.payloadLength
                    ),
                    message.payload
                        ? message.payload
                        : ""
                );

                break;
            }

            case Origin::UNKNOWN:
            default:
            {
                LOG_WF(
                    "MQTT_IN",
                    "Origine MQTT non gestita: "
                    "client=%u origin=%u",
                    static_cast<unsigned>(
                        message.clientIndex
                    ),
                    static_cast<unsigned>(
                        message.origin
                    )
                );

                break;
            }
        }
    }

    static void MQTTEventCallback(
        const EventManager::Event& event)
    {
        if (!instance)
        {
            return;
        }

        const FrontendConfig& config =
            ConfigReadOnly();

        if (!config.mqtt.enabled)
        {
            return;
        }

        MQTTEngine::PublishEvent(
            event
        );

        homeAssistantOrchestrator.publishEvent(
            event.area,
            event.value
        );

        if (
            Manager &&
            Manager
                ->getLeds()
                .hasChannel(
                    LedController::THREE
                )
        )
        {
            static bool ledState =
                false;

            ledState =
                !ledState;

            Manager
                ->getLeds()
                .set(
                    LedController::THREE,
                    ledState
                );
        }
    }

    // ========================================================
    // WEB API REQUEST CALLBACK
    //
    // Chiamata dopo ogni richiesta HTTP gestita da un endpoint.
    // ========================================================

    static void WebAPIRequestCallback(
        const char* endpointName,
        const HttpServerRequest& request,
        const HttpServerResponse& response)
    {
        LOG_IF(
            "FRONTEND-WEBAPI",
            "endpoint=%s method=%s path=%s "
            "status=%d response='%s'",
            endpointName
                ? endpointName
                : "",
            request.method.c_str(),
            request.path.c_str(),
            response.statusCode,
            response.body.c_str()
        );
    }

    static void WebAPIDeviceResponseCallback(
        const WebApiDevice& device,
        const DeviceMessageProfile::OutMessage& message,
        const String& rawResponse,
        const ParsedResponse& parsed)
    {
        // ====================================================
        // REOLINK AI STATE
        // ====================================================

        if (
            !ReolinkWebAPI::IsProfile(
                device.profile
            ) ||
            strcmp(
                message.key
                    ? message.key
                    : "",
                ReolinkWebAPI::MESSAGE_AI_STATE
            ) != 0
        )
        {
            return;
        }

        // ====================================================
        // JSON VALIDATION
        // ====================================================

        if (!parsed.jsonValid)
        {
            LOG_WF(
                "FRONTEND-REOLINK",
                "Invalid AI response: "
                "device=%s bytes=%u",
                device.name
                    ? device.name
                    : "",
                static_cast<unsigned>(
                    rawResponse.length()
                )
            );

            return;
        }

        // ====================================================
        // PARSE ALARM MASK
        // ====================================================

        uint8_t alarmMask =
            ReolinkWebAPI::ALARM_NONE;

        int channel =
            -1;

        if (
            !ReolinkWebAPI::ParseAlarmMask(
                parsed.json
                    .as<JsonVariantConst>(),
                alarmMask,
                channel
            )
        )
        {
            LOG_WF(
                "FRONTEND-REOLINK",
                "Unable to parse AI response: "
                "device=%s",
                device.name
                    ? device.name
                    : ""
            );

            return;
        }

        // ====================================================
        // CHANGE DETECTION
        //
        // La configurazione corrente prevede una sola
        // telecamera Reolink.
        // ====================================================

        static int previousAlarmMask =
            -1;

        const int currentAlarmMask =
            static_cast<int>(
                alarmMask
            );

        if (
            currentAlarmMask ==
            previousAlarmMask
        )
        {
            return;
        }

        const int oldAlarmMask =
            previousAlarmMask;

        previousAlarmMask =
            currentAlarmMask;

        // ====================================================
        // AREA UPDATE
        // ====================================================

        if (!DomoManager::instance)
        {
            LOG_WF(
                "FRONTEND-REOLINK",
                "Unable to publish AI alarm: "
                "DomoManager not initialized"
            );

            return;
        }

        DomoManager::instance
            ->forceInternalEvent(
                AREA_REOLINK_INGRESSO_ALARM,
                currentAlarmMask
            );

        // ====================================================
        // DIAGNOSTICS
        // ====================================================

        LOG_IF(
            "FRONTEND-REOLINK",
            "AI alarm changed: "
            "device=%s channel=%d "
            "old=0x%02X new=0x%02X "
            "person=%u vehicle=%u animal=%u",
            device.name
                ? device.name
                : "",
            channel,
            static_cast<unsigned>(
                oldAlarmMask < 0
                    ? 0
                    : oldAlarmMask
            ),
            static_cast<unsigned>(
                currentAlarmMask
            ),
            ReolinkWebAPI::HasPerson(
                alarmMask
            )
                ? 1U
                : 0U,
            ReolinkWebAPI::HasVehicle(
                alarmMask
            )
                ? 1U
                : 0U,
            ReolinkWebAPI::HasAnimal(
                alarmMask
            )
                ? 1U
                : 0U
        );
    }

    // ========================================================
    // ACCESS EVENT CALLBACK
    //
    // Chiamata per ogni decisione del controllo accessi.
    // ========================================================

    static void AccessEventCallback(
        const AccessControlService::Request& request,
        const AccessControlService::Decision& decision)
    {
        const char* readerName =
            (
                decision.reader &&
                decision.reader->name
            )
                ? decision.reader->name
                : request.readerName.c_str();

        LOG_IF(
            "FRONTEND-ACCESS",
            "reader=%s card=%s authorized=%u result=%s",
            readerName,
            decision.cardCode.c_str(),
            decision.authorized
                ? 1U
                : 0U,
            AccessControlService::ResultToString(
                decision.result
            )
        );
    }

    // ========================================================
    // ACCESS AUTHORIZED CALLBACK
    //
    // Chiamata solamente per una decisione positiva.
    // ========================================================

    static void AccessAuthorizedCallback(
        const AccessControlService::Request& request,
        const AccessControlService::Decision& decision)
    {
        // ====================================================
        // READER NAME
        // ====================================================

        const char* readerName =
            (
                decision.reader &&
                decision.reader->name
            )
                ? decision.reader->name
                : request.readerName.c_str();


        const int area =
            decision.reader
                ? decision.reader->area
                : -1;


        LOG_IF(
            "FRONTEND-ACCESS",
            "Authorized: reader=%s card=%s "
            "relay=%d time=%d area=%d",
            readerName,
            decision.cardCode.c_str(),
            decision.relayIndex,
            decision.relayTime,
            area
        );


        // ====================================================
        // READER VALIDATION
        // ====================================================

        if (!decision.reader)
        {
            LOG_EF(
                "FRONTEND-ACCESS",
                "Authorized decision without resolved reader: "
                "reader=%s card=%s",
                readerName,
                decision.cardCode.c_str()
            );

            return;
        }


        const AccessReader& reader =
            *decision.reader;


        // ====================================================
        // LOCAL DOMOMANAGER PULSE
        //
        // L'impulso locale è opzionale.
        //
        // Un errore o la sua mancata configurazione non deve
        // impedire l'eventuale comando remoto Fanvil.
        // ====================================================

        if (area >= 0)
        {
            if (
                !PulseManager::Exists(
                    area
                )
            )
            {
                LOG_EF(
                    "FRONTEND-ACCESS",
                    "Pulse output not registered: "
                    "reader=%s area=%d",
                    readerName,
                    area
                );
            }
            else if (
                !PulseManager::Trigger(
                    area
                )
            )
            {
                LOG_EF(
                    "FRONTEND-ACCESS",
                    "Unable to trigger relay pulse: "
                    "reader=%s area=%d",
                    readerName,
                    area
                );
            }
            else
            {
                LOG_IF(
                    "FRONTEND-ACCESS",
                    "Relay pulse triggered: "
                    "reader=%s area=%d card=%s",
                    readerName,
                    area,
                    decision.cardCode.c_str()
                );
            }
        }
        else
        {
            LOG_DF(
                "FRONTEND-ACCESS",
                "No local pulse configured: "
                "reader=%s",
                readerName
            );
        }


        // ====================================================
        // OUTBOUND AUTHORIZED COMMAND
        //
        // Il comportamento è interamente configurato nel reader.
        // Nessuna dipendenza dal protocollo concreto.
        // ====================================================

        if (
            reader.authorizedMessageKey &&
            reader.authorizedMessageKey[0] != '\0'
        )
        {
            if (
                !WebAPIEngine::IsClientRunning()
            )
            {
                LOG_EF(
                    "FRONTEND-ACCESS",
                    "Unable to send authorized command: "
                    "HTTP client not running "
                    "reader=%s message=%s",
                    readerName,
                    reader.authorizedMessageKey
                );

                return;
            }

            if (
                !WebAPIEngine::Send(
                    readerName,
                    reader.authorizedMessageKey
                )
            )
            {
                LOG_EF(
                    "FRONTEND-ACCESS",
                    "Unable to queue authorized command: "
                    "reader=%s message=%s",
                    readerName,
                    reader.authorizedMessageKey
                );

                return;
            }

            LOG_IF(
                "FRONTEND-ACCESS",
                "Authorized command queued: "
                "reader=%s message=%s",
                readerName,
                reader.authorizedMessageKey
            );
        }
    }

    // ========================================================
    // CUSTOM TASKS
    // ========================================================
    static void Task_Solar(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;

        SolarEngine::Loop(
            now
        );
    }

    static void Task_Jobs(DomoManager& manager, unsigned long now)
    {
        (void)manager;
        JobsEngine::Loop(now);
    }

    static void Task_Meteo(DomoManager& manager, unsigned long now)
    {
        (void)manager;
        WeatherEngine::Loop(now);
    }

    static void Task_PowerSupervisor(DomoManager& manager, unsigned long now)
    {
        (void)manager;
        PowerSupervisorEngine::Loop(now);
    }

    static void Task_Power(DomoManager& manager, unsigned long now)
    {
        auto& buffer = manager.getBuffer();
        //auto& averages = manager.getAverages();

        const int gridPower = buffer.getValueFast(AREA_INVERTER_PV_POWER, 100);
        
        PowerEngine::Loop(
            now,
            gridPower
        );

        auto& pm = PowerEngine::Get();

        LOG_DF(
            "Power",
            "grid=%0.2f",
            pm.getGridPower()
        );
    }

    DomoManagerFrontendEngine(
        DomoManager& dm,
        const FrontendConfig& cfg)
        : FrontendRuntime(dm, cfg),
        aeeEngine(),
        peerBridgeEngine()
    {
        peerBridgeEngine.setIdentity(
            0,
            "AEE-PEER"
        );
    }

    // ========================================================
    // INTROSPECTION
    // ========================================================
    static bool BuildIntrospection()
    {
        if (!Manager ||
            !instance)
        {
            LOG_E(
                "INTROSPECTION",
                "Build failed: runtime unavailable"
            );

            return false;
        }

        const FrontendConfig& config =
            instance->getConfig();

        const IntrospectionEngine::Result result =
            IntrospectionEngine::Build(
                introspection,
                *Manager,
                config,
                &instance
                    ->aeeEngine
                    .getRegistry()
            );

        if (!result.success)
        {
            LOG_E(
                "INTROSPECTION",
                "Introspection engine build failed"
            );

            return false;
        }

        LOG_IF(
            "INTROSPECTION",
            "Frontend model ready: "
            "entities=%u profiles=%u/%u",
            static_cast<unsigned>(
                result.entityCount
            ),
            static_cast<unsigned>(
                result.profilesFound
            ),
            static_cast<unsigned>(
                result.deviceCount
            )
        );

        return true;
    }
    

protected:
    void onFrontendDiagnostics() override
    {
        if (hmiEngine.enabled())
        {
            Serial.print("HMI enabled: ");
            Serial.println(hmiEngine.enabled() ? "YES" : "NO");
            Serial.print("HMI running: ");
            Serial.println(hmiEngine.running() ? "YES" : "NO");
            Serial.print("HMI loop enabled: ");
            Serial.println(hmiEngine.loopEnabled() ? "YES" : "NO");
            Serial.print("HMI port: ");
            Serial.println(hmiEngine.port());
            Serial.print("HMI active clients: ");
            Serial.println(hmiEngine.activeClients());
            Serial.print("HMI max clients: ");
            Serial.println(hmiEngine.maxClients());
        }

        if (config.weather.enabled)
            WeatherStation::Diagnostic::Report(WeatherEngine::Get());

        if (config.power.enabled)
            PowerManager::Diagnostic::FullReport(PowerEngine::Get());

        if (config.solarInverters.count > 0)
        {
            SolarEngine::DiagnosticReport();
        }
    }

    void onStartupButton(unsigned long now) override
    {
        (void)now;
    }

    void onButtonPressed(unsigned long now) override
    {
        (void)now;
    }

    void onDeveloperMode() override
    {
    }

#if HOTSTANDBY_ENABLED
    void onBecomeMaster() override
    {
        LOG_I("Main", "Passo a MASTER -> abilito Ethernet");
        SetupEthernet(config);
    }

    void onBecomeSlave() override
    {
        LOG_I("Main", "Passo a SLAVE -> disabilito Ethernet");
        DMPlatform::Network::EndEthernet();
    }
#endif

public:
    static void Setup(
        const FrontendConfig& cfg)
    {
        // ========================================================
        // RESET STATIC RUNTIME POINTERS / IDS
        // ========================================================

        Manager =
            nullptr;

        instance =
            nullptr;

        network =
            nullptr;

        webApiClientProtocolId =
            -1;

        webApiServerProtocolId =
            -1;

        solarProtocolId =
            -1;

        mqttProtocolId =
            -1;

        mqttEventSource =
            255;

        // ========================================================
        // CORE MANAGER
        // ========================================================

        Manager =
            new DomoManager(
                cfg.pins.leds
            );

        if (!Manager)
        {
            LOG_EF(
                "DomoManagerFrontendEngine",
                "Allocazione DomoManager fallita"
            );

            return;
        }

        // ========================================================
        // FRONTEND INSTANCE
        // ========================================================

        instance =
            new DomoManagerFrontendEngine(
                *Manager,
                cfg
            );

        if (!instance)
        {
            LOG_EF(
                "DomoManagerFrontendEngine",
                "Allocazione frontend fallita"
            );

            delete Manager;

            Manager =
                nullptr;

            return;
        }

        // ========================================================
        // FRONTEND NETWORK
        // ========================================================

        network =
            new FrontendNetwork(
                cfg.mqtt.clientCount
            );

        if (!network)
        {
            LOG_EF(
                "DomoManagerFrontendEngine",
                "Allocazione FrontendNetwork fallita"
            );

            delete instance;
            instance =
                nullptr;

            delete Manager;
            Manager =
                nullptr;

            return;
        }

        // ========================================================
        // AEE ENGINE
        // ========================================================

        if (
            !instance
                ->aeeEngine
                .setup(
                    cfg.bridge.aee
                )
        )
        {
            LOG_EF(
                "AEE",
                "Setup AEE fallito"
            );

            return;
        }

        LOG_I(
            "DomoManagerFrontendEngine",
            "DomoManager is starting..."
        );

        // ========================================================
        // ETHERNET
        // ========================================================

        SetupEthernet(
            cfg
        );

        // ========================================================
        // MODBUS TCP -> RTU CLIENT TIMEOUT
        // ========================================================

        network
            ->modbusTCP()
            .modbus
            .setTimeout(
                cfg
                    .domoManager
                    .modbus
                    .timeoutMs
            );

        LOG_IF(
            "DomoManagerFrontendEngine",
            "Modbus TCP to RTU timeout set %lu mSec.",
            static_cast<unsigned long>(
                cfg
                    .domoManager
                    .modbus
                    .timeoutMs
            )
        );

        delay(
            2000
        );

        // ========================================================
        // TASK ENGINE
        //
        // Registra solamente i task standard:
        // - Security
        // - HVAC
        // - Averages
        // ========================================================

        TaskEngine::Setup(
            cfg
        );

        // ========================================================
        // SOLAR TASK CONFIGURATION
        // ========================================================

        bool solarTaskEnabled =
            false;

        uint32_t solarTaskIntervalMs =
            1000UL;

        const uint8_t solarCount =
            cfg.solarInverters.count <
                FrontendConfig::
                    MAX_SOLAR_INVERTERS
                ? cfg.solarInverters.count
                : FrontendConfig::
                    MAX_SOLAR_INVERTERS;

        for (
            uint8_t index = 0;
            index < solarCount;
            ++index
        )
        {
            const auto& solar =
                cfg
                    .solarInverters
                    .inverter[index];

            if (!solar.enabled)
                continue;

            const uint32_t intervalMs =
                solar.intervalMs > 0
                    ? solar.intervalMs
                    : 1UL;

            if (!solarTaskEnabled)
            {
                solarTaskEnabled =
                    true;

                solarTaskIntervalMs =
                    intervalMs;

                continue;
            }

            if (
                intervalMs <
                solarTaskIntervalMs
            )
            {
                solarTaskIntervalMs =
                    intervalMs;
            }
        }

        // ========================================================
        // CUSTOM TASKS
        // ========================================================

        if (
            !TaskEngine::AddTask(
                Task_Solar,
                solarTaskIntervalMs,
                solarTaskEnabled
            )
        )
        {
            LOG_EF(
                "TASK-ENGINE",
                "Registrazione Task_Solar fallita"
            );

            return;
        }

        if (
            !TaskEngine::AddTask(
                Task_Jobs,
                cfg.jobs.intervalMs,
                cfg.jobs.enabled
            )
        )
        {
            LOG_EF(
                "TASK-ENGINE",
                "Registrazione Task_Jobs fallita"
            );

            return;
        }

        if (
            !TaskEngine::AddTask(
                [](
                    DomoManager& manager,
                    unsigned long now)
                {
                    hmiEngine.Sync(
                        manager,
                        now
                    );
                },
                10UL,
                cfg.domoManager.hmi.enabled
            )
        )
        {
            LOG_EF(
                "TASK-ENGINE",
                "Registrazione task HMI fallita"
            );

            return;
        }

        if (
            !TaskEngine::AddTask(
                Task_Meteo,
                cfg.weather.intervalMs,
                cfg.weather.enabled
            )
        )
        {
            LOG_EF(
                "TASK-ENGINE",
                "Registrazione Task_Meteo fallita"
            );

            return;
        }

        if (
            !TaskEngine::AddTask(
                Task_PowerSupervisor,
                cfg.ps.intervalMs,
                cfg.ps.enabled
            )
        )
        {
            LOG_EF(
                "TASK-ENGINE",
                "Registrazione Task_PowerSupervisor fallita"
            );

            return;
        }

        if (
            !TaskEngine::AddTask(
                Task_Power,
                cfg.power.intervalMs,
                cfg.power.enabled
            )
        )
        {
            LOG_EF(
                "TASK-ENGINE",
                "Registrazione Task_Power fallita"
            );

            return;
        }

        // ========================================================
        // WATCHDOG CALLBACK
        // ========================================================

        Manager->SetWatchdogCallback(
            watchdogHandler
        );

        // ========================================================
        // EVENT SOURCES
        // ========================================================

        const int solarSource =
            Manager
                ->getEventManager()
                .add(
                    "SOLAR"
                );

        const int securitySource =
            Manager
                ->getEventManager()
                .add(
                    SecurityEngine::EventCallback,
                    "SECURITY"
                );

        const int hmiSource =
            Manager
                ->getEventManager()
                .add(
                    HMIEventCallback,
                    "HMI"
                );

        const int aeeSource =
            Manager
                ->getEventManager()
                .add(
                    AEEEventCallback,
                    "AEE"
                );

        const int mqttSource =
            Manager
                ->getEventManager()
                .add(
                    MQTTEventCallback,
                    "MQTT"
                );

        // ========================================================
        // EVENT SOURCE VALIDATION
        // ========================================================
        if (securitySource < 0 ||
            securitySource > 254)
        {
            LOG_EF(
                "SECURITY",
                "Event source non valido: %d",
                securitySource
            );

            return;
        }

        if (
            solarSource < 0 ||
            solarSource > 254
        )
        {
            LOG_EF(
                "SOLAR",
                "Event source non valido: %d",
                solarSource
            );

            return;
        }

        if (
            hmiSource < 0 ||
            hmiSource > 254
        )
        {
            LOG_EF(
                "HMI",
                "Event source non valido: %d",
                hmiSource
            );

            return;
        }

        if (
            aeeSource < 0 ||
            aeeSource > 254
        )
        {
            LOG_EF(
                "AEE",
                "Event source non valido: %d",
                aeeSource
            );

            return;
        }

        if (
            mqttSource < 0 ||
            mqttSource > 254
        )
        {
            LOG_EF(
                "MQTT",
                "Event source non valido: %d",
                mqttSource
            );

            return;
        }

        mqttEventSource =
            static_cast<uint8_t>(
                mqttSource
            );

        // ========================================================
        // AEE RECEIVE CONTEXT
        // ========================================================

        instance
            ->aeeEngine
            .attachReceiveContext(
                Manager->getBuffer(),
                Manager->getEventManager(),
                aeeSource
            );

        // ========================================================
        // MODBUS RTU PROTOCOL
        // ========================================================

        DMNetworkManager::ProtocolId
            modbusRTUProtocol =
                -1;

        if (
            cfg
                .domoManager
                .modbus
                .enabled
        )
        {
            modbusRTUProtocol =
                Manager
                    ->net
                    .registerProtocol(
                        "Mbus-RTU",
                        200UL,
                        550UL,
                        1,
                        true
                    );

            if (
                modbusRTUProtocol < 0
            )
            {
                LOG_EF(
                    "MODBUS",
                    "Registrazione protocollo Modbus RTU fallita"
                );

                return;
            }

            Manager
                ->net
                .setPacingEnabled(
                    modbusRTUProtocol,
                    false
                );
        }

        // ========================================================
        // DOMO MANAGER SETUP
        // ========================================================

        if (
            !Manager->setup(
                OnAreasChanged,
                TaskEngine::Loop,
                cfg.domoManager,
                modbusRTUProtocol
            )
        )
        {
            LOG_EF(
                "DomoManagerFrontendEngine",
                "DomoManager setup failed"
            );

            while (true)
            {
                delay(
                    100
                );
            }
        }

        // ========================================================
        // MODBUS PERFORMANCE REPORTING
        // ========================================================

        Manager
            ->getLogic()
            .getModbus()
            .setTimingEnabled(
                cfg
                    .domoManager
                    .modbus
                    .reporting
            );

        // ========================================================
        // RUNTIME STORE
        // ========================================================

        if (
            !RegisterRuntimeTags(
                *Manager
            )
        )
        {
            LOG_EF(
                "RUNTIME",
                "Runtime tags initialization failed"
            );

            return;
        }

        // ========================================================
        // HMI
        // ========================================================

        if (
            cfg
                .domoManager
                .hmi
                .enabled
        )
        {
            const uint8_t hmiSocketDemand =
                static_cast<uint8_t>(
                    1U +
                    cfg
                        .domoManager
                        .hmi
                        .maxClients
                );

            const auto hmiProtocolId =
                Manager
                    ->net
                    .registerProtocol(
                        "HMI",
                        cfg
                            .domoManager
                            .hmi
                            .pollingMs,
                        60UL,
                        hmiSocketDemand,
                        true
                    );

            if (
                hmiProtocolId < 0
            )
            {
                LOG_EF(
                    "DomoManagerFrontendEngine",
                    "HMI protocol registration failed"
                );
            }
            else
            {
                const SocketManager::OwnerId
                    hmiSocketOwner =
                        Manager
                            ->net
                            .getProtocol(
                                hmiProtocolId
                            )
                            .socketOwner;

                if (
                    hmiSocketOwner < 0
                )
                {
                    LOG_EF(
                        "HMI",
                        "HMI socket owner non valido"
                    );
                }
                else
                {
                    hmiEngine.Setup(
                        cfg.domoManager.hmi,
                        Manager->net,
                        hmiSocketOwner,
                        hmiProtocolId,
                        cfg.domoManager.hmi.maxClients,
                        Manager->getBuffer().size(),
                        static_cast<uint8_t>(
                            hmiSource
                        )
                    );
                }
            }
        }

        // ========================================================
        // WEB API
        // ========================================================

        SetupWebAPI();

        // ========================================================
        // OWNER MODE CALLBACK
        // ========================================================

        Manager
            ->getOwner()
            .setCallback(
                FrontendOwnerMode::OnChanged
            );

        // ========================================================
        // FRONTEND ENGINES
        // ========================================================

        if (!InitEngines(
                *Manager,
                static_cast<uint8_t>(securitySource)))
        {
            LOG_EF(
                "DomoManagerFrontendEngine",
                "Frontend engines initialization failed"
            );

            return;
        }

        // ========================================================
        // SOLAR INVERTERS
        // ========================================================

        const uint8_t configuredSolarCount =
            cfg.solarInverters.count <
                FrontendConfig::
                    MAX_SOLAR_INVERTERS
                ? cfg.solarInverters.count
                : FrontendConfig::
                    MAX_SOLAR_INVERTERS;

        if (
            configuredSolarCount > 0
        )
        {
            bool anySolarEnabled =
                false;

            uint8_t solarSocketDemand =
                0;

            uint32_t minimumIntervalMs =
                1000UL;

            uint32_t maximumQueryTimeoutMs =
                3000UL;

            for (
                uint8_t index = 0;
                index < configuredSolarCount;
                ++index
            )
            {
                const auto& solar =
                    cfg
                        .solarInverters
                        .inverter[index];

                if (!solar.enabled)
                    continue;

                const uint32_t currentIntervalMs =
                    solar.intervalMs > 0
                        ? solar.intervalMs
                        : 1UL;

                const uint32_t currentTimeoutMs =
                    solar.queryTimeoutMs > 0
                        ? solar.queryTimeoutMs
                        : 3000UL;

                if (!anySolarEnabled)
                {
                    anySolarEnabled =
                        true;

                    minimumIntervalMs =
                        currentIntervalMs;

                    maximumQueryTimeoutMs =
                        currentTimeoutMs;
                }
                else
                {
                    if (
                        currentIntervalMs <
                        minimumIntervalMs
                    )
                    {
                        minimumIntervalMs =
                            currentIntervalMs;
                    }

                    if (
                        currentTimeoutMs >
                        maximumQueryTimeoutMs
                    )
                    {
                        maximumQueryTimeoutMs =
                            currentTimeoutMs;
                    }
                }

                if (
                    solar.transport ==
                    FrontendConfig::Solar::
                        TransportType::
                            TCP_CLIENT
                )
                {
                    ++solarSocketDemand;
                }
            }

            if (anySolarEnabled)
            {
                solarProtocolId =
                    Manager
                        ->net
                        .registerProtocol(
                            "Solar",
                            minimumIntervalMs,
                            maximumQueryTimeoutMs,
                            solarSocketDemand,
                            true
                        );

                if (
                    solarProtocolId < 0
                )
                {
                    LOG_EF(
                        "SOLAR",
                        "Protocol registration failed"
                    );
                }
                else
                {
                    LOG_IF(
                        "SOLAR",
                        "Protocol registered: "
                        "id=%d inverters=%u sockets=%u "
                        "interval=%lu timeout=%lu",
                        static_cast<int>(
                            solarProtocolId
                        ),
                        static_cast<unsigned>(
                            configuredSolarCount
                        ),
                        static_cast<unsigned>(
                            solarSocketDemand
                        ),
                        static_cast<unsigned long>(
                            minimumIntervalMs
                        ),
                        static_cast<unsigned long>(
                            maximumQueryTimeoutMs
                        )
                    );

                    SolarEngine::
                        SetSnapshotChangedCallback(
                            OnSolarSnapshot
                        );

                    SolarEngine::
                        SetFrameReceivedCallback(
                            OnSolarFrame
                        );

                    SolarEngine::
                        SetCommandStateCallback(
                            OnSolarCommandState
                        );

                    SolarEngine::Setup(
                        *Manager,
                        Manager->net,
                        *network,
                        cfg.solarInverters,
                        solarProtocolId,
                        static_cast<uint8_t>(
                            solarSource
                        )
                    );
                }
            }
            else
            {
                LOG_IF(
                    "SOLAR",
                    "All configured inverters are disabled"
                );
            }
        }
        else
        {
            LOG_IF(
                "SOLAR",
                "No solar inverters configured"
            );
        }

        // ========================================================
        // AEE STATUS
        // ========================================================

        LOG_IF(
            "AEE",
            "AEE initialized with %u variables",
            static_cast<unsigned>(
                instance
                    ->aeeEngine
                    .getRegistry()
                    .size()
            )
        );

        // ========================================================
        // INTROSPECTION
        //
        // Deve precedere SetupHAOrchestrator perché HA riceve
        // il puntatore a introspection.
        // ========================================================

        BuildIntrospection();

        // ========================================================
        // MQTT
        // ========================================================

        if (cfg.mqtt.enabled)
        {
            uint8_t mqttSocketDemand =
                0;

            if (
                cfg.mqtt.clients &&
                cfg.mqtt.clientCount > 0
            )
            {
                const size_t mqttConfigCount =
                    cfg.mqtt.clientCount <
                        MQTTEngine::MAX_CLIENTS
                        ? cfg.mqtt.clientCount
                        : MQTTEngine::MAX_CLIENTS;

                for (
                    size_t index = 0;
                    index < mqttConfigCount;
                    ++index
                )
                {
                    if (
                        cfg
                            .mqtt
                            .clients[index]
                            .enabled
                    )
                    {
                        ++mqttSocketDemand;
                    }
                }
            }

            if (
                !cfg.mqtt.clients ||
                cfg.mqtt.clientCount == 0
            )
            {
                LOG_EF(
                    "MQTT",
                    "MQTT enabled without configured clients"
                );
            }
            else if (
                mqttSocketDemand == 0
            )
            {
                LOG_WF(
                    "MQTT",
                    "MQTT enabled but all clients are disabled"
                );
            }
            else
            {
                mqttProtocolId =
                    Manager
                        ->net
                        .registerProtocol(
                            "MQTT",
                            50UL,
                            50UL,
                            mqttSocketDemand,
                            true
                        );

                if (
                    mqttProtocolId < 0
                )
                {
                    LOG_EF(
                        "MQTT",
                        "Registrazione protocollo MQTT fallita"
                    );

                    return;
                }

                LOG_IF(
                    "MQTT",
                    "Protocol registered: "
                    "id=%d configured=%u sockets=%u",
                    static_cast<int>(
                        mqttProtocolId
                    ),
                    static_cast<unsigned>(
                        cfg.mqtt.clientCount
                    ),
                    static_cast<unsigned>(
                        mqttSocketDemand
                    )
                );

                // ====================================================
                // MQTT ENGINE SETUP
                //
                // Setup azzera le callback frontend, quindi i setter
                // devono essere chiamati successivamente.
                // ====================================================

                MQTTEngine::Setup(
                    *Manager,
                    Manager->net,
                    *network,
                    cfg.mqtt.clientCount,
                    cfg.mqtt,
                    mqttProtocolId,
                    mqttEventSource
                );

                // ====================================================
                // MQTT CALLBACKS
                // ====================================================

                MQTTEngine::
                    setInboundMessageReceivedCallback(
                        OnMQTTInboundMessage
                    );

                MQTTEngine::
                    setRawMessageReceivedCallback(
                        OnMQTTRawMessage
                    );

                MQTTEngine::
                    setRoutedEventCallback(
                        OnMQTTRoutedEvent
                    );

                // ====================================================
                // HOME ASSISTANT
                // ====================================================

                const bool homeAssistantConfigured =
                    FindHAClientConfig() !=
                        nullptr;

                if (homeAssistantConfigured)
                {
                    if (
                        !SetupHAOrchestrator()
                    )
                    {
                        LOG_EF(
                            "HA_ORCHESTRATOR",
                            "Setup Home Assistant fallito"
                        );

                        return;
                    }

                    MQTTEngine::
                        setServiceLoopCallback(
                            HAServiceLoop
                        );
                }
                else
                {
                    MQTTEngine::
                        setServiceLoopCallback(
                            nullptr
                        );

                    LOG_IF(
                        "HA_ORCHESTRATOR",
                        "Client Home Assistant non configurato"
                    );
                }

                // ====================================================
                // MQTT TASK REGISTRATION
                // ====================================================

                if (
                    !TaskEngine::AddTask(
                        Task_MQTT,
                        50UL,
                        true
                    )
                )
                {
                    LOG_EF(
                        "MQTT",
                        "Registrazione Task_MQTT fallita"
                    );

                    return;
                }

                LOG_IF(
                    "MQTT",
                    "Task MQTT registrato: "
                    "interval=50 ms clients=%u mappings=%u",
                    static_cast<unsigned>(
                        MQTTEngine::
                            getClientCount()
                    ),
                    static_cast<unsigned>(
                        MQTTEngine::
                            getMappingCount()
                    )
                );
            }
        }
        else
        {
            LOG_IF(
                "MQTT",
                "MQTT disabled"
            );
        }

        // ========================================================
        // HOT STANDBY / BRIDGE
        // ========================================================

        #if HOTSTANDBY_ENABLED

            Manager
                ->enableHotStandby(
                    false
                );

            LOG_IF(
                "HOTSTANDBY",
                "Hot standby support enabled"
            );

        #else

            if (cfg.bridge.enabled)
            {
                const DMNetworkManager::ProtocolId
                    bridgeProtocolId =
                        Manager
                            ->net
                            .registerProtocol(
                                "Bridge",
                                50UL,
                                300UL,
                                1,
                                true
                            );

                if (
                    bridgeProtocolId < 0
                )
                {
                    LOG_EF(
                        "BRIDGE",
                        "Registrazione protocollo Bridge fallita"
                    );

                    return;
                }

                const SocketManager::OwnerId
                    bridgeSocketOwner =
                        Manager
                            ->net
                            .getProtocol(
                                bridgeProtocolId
                            )
                            .socketOwner;

                if (
                    bridgeSocketOwner < 0
                )
                {
                    LOG_EF(
                        "BRIDGE",
                        "Socket owner Bridge non valido"
                    );

                    return;
                }

                static UdpAdapter bridgeTransport(
                    cfg.bridge.ip,
                    cfg.bridge.localPort,
                    cfg.bridge.remotePort
                );

                bridgeTransport.setSocketContext(
                    Manager->net,
                    bridgeSocketOwner
                );

                instance
                    ->peerBridgeEngine
                    .setReceiveCallback(
                        OnAEEVarChanged
                    );

                instance
                    ->peerBridgeEngine
                    .init(
                        bridgeTransport,
                        instance
                            ->aeeEngine
                            .getRegistry(),
                        true
                    );

                if (
                    !instance
                        ->peerBridgeEngine
                        .setupPacer(
                            Manager->net,
                            bridgeProtocolId
                        )
                )
                {
                    LOG_EF(
                        "BRIDGE",
                        "Setup pacer Bridge peer fallito"
                    );

                    return;
                }

                // ====================================================
                // BRIDGE TASK REGISTRATION
                //
                // Il Bridge viene eseguito direttamente dal
                // TaskEngineOrchestrator.
                // ====================================================

                if (
                    !TaskEngine::AddTask(
                        Task_Bridge,
                        20UL,
                        true
                    )
                )
                {
                    LOG_EF(
                        "BRIDGE",
                        "Registrazione Task_Bridge fallita"
                    );

                    return;
                }

                LOG_IF(
                    "BRIDGE",
                    "Peer Bridge pronto: "
                    "protocol=%d localPort=%u remotePort=%u",
                    static_cast<int>(
                        bridgeProtocolId
                    ),
                    static_cast<unsigned>(
                        cfg.bridge.localPort
                    ),
                    static_cast<unsigned>(
                        cfg.bridge.remotePort
                    )
                );
            }
            else
            {
                LOG_IF(
                    "BRIDGE",
                    "Peer Bridge disabled"
                );
            }

        #endif

        // ========================================================
        // FULL CYCLE CALLBACK
        // ========================================================

        SetFullCycleCallback(
            [](
                DomoManager& domoManager)
            {
                static constexpr
                    unsigned long OLDER =
                        5000UL;

                if (!instance)
                    return;

                AEEManagement* aeeManager =
                    instance
                        ->aeeEngine
                        .getManager();

                if (!aeeManager)
                    return;

                Buffer& buffer =
                    domoManager
                        .getBuffer();

                std::vector<DMAEE::Update>
                    updates;

                updates.reserve(
                    16
                );

                const unsigned long now =
                    domoManager
                        .getTimeManager()
                        .nowMs();

                if (
                    DMAEE::
                        BuildUpdatesFromPolledSources(
                            *aeeManager,
                            buffer,
                            updates
                        )
                )
                {
                    DMAEE::ApplyUpdates(
                        updates,
                        now
                    );
                }

                buffer.ResetAll(
                    now,
                    OLDER
                );
            }
        );

        // ========================================================
        // TASK ENGINE FINAL REPORT
        // ========================================================

        LOG_IF(
            "TASK-ENGINE",
            "Setup completato: tasks=%u maxPerCycle=%u",
            static_cast<unsigned>(
                TaskEngine::
                    getTaskCount()
            ),
            static_cast<unsigned>(
                TaskEngine::
                    getMaxTasksPerCycle()
            )
        );

        // ========================================================
        // WATCHDOG
        // ========================================================

        Manager
            ->enableWatchdog();

        // ========================================================
        // STARTUP REPORT
        // ========================================================

        LOG_IF(
            "DOMO MANAGER",
            "********************* 10.2026 build 01"
        );

        Manager
            ->net
            .sockets()
            .dump();
    }

    // ============================================================
    // FRONTEND MAIN LOOP
    //
    // Sequenza:
    //   1. validazione runtime;
    //   2. aggiornamento FrontendRuntime;
    //   3. verifica ruolo Hot Standby;
    //   4. aggiornamento stato HMI;
    //   5. gestione rete HMI;
    //   6. esecuzione ciclo DomoManager;
    //   7. verifica completamento ciclo frontend/backend.
    //
    // MQTT, Bridge, Solar e gli altri servizi schedulati non
    // vengono richiamati direttamente qui.
    //
    // La loro esecuzione avviene attraverso:
    //
    //   Manager::loop()
    //       -> TaskEngine::Loop()
    //           -> TaskEngineOrchestrator::Loop()
    //
    // I servizi veloci, come PulseManager e WebAPI, vengono invece
    // gestiti da FrontendRuntime::updateRuntime().
    // ============================================================

    static void loop()
    {
        // ========================================================
        // RUNTIME VALIDATION
        // ========================================================

        if (
            !instance ||
            !Manager ||
            !network
        )
        {
            return;
        }

        // ========================================================
        // FRONTEND RUNTIME
        //
        // updateRuntime():
        // - acquisisce il timestamp corrente;
        // - esegue i fast services;
        // - gestisce il pulsante utente;
        // - applica la modalità developer;
        // - aggiorna lo stato Hot Standby.
        // ========================================================

        const unsigned long now =
            instance
                ->updateRuntime();

        // ========================================================
        // EXECUTION ROLE
        //
        // In Hot Standby solamente il MASTER esegue:
        // - HMI;
        // - ciclo DomoManager;
        // - task frontend;
        // - verifica full cycle.
        //
        // In configurazione normale il nodo è sempre operativo.
        // ========================================================

    #if HOTSTANDBY_ENABLED

        const bool executionEnabled =
            instance
                ->getIsMaster();

    #else

        constexpr bool executionEnabled =
            true;

    #endif

        if (!executionEnabled)
        {
            return;
        }

        // ========================================================
        // POWER-ON STATE
        //
        // L'HMI viene abilitata solamente dopo il completamento
        // del primo ciclo di inizializzazione del DomoManager.
        // ========================================================

        const bool powerOnCompleted =
            Manager
                ->getPowerOnCycleCompleted();

        hmiEngine.SetLoopEnabled(
            powerOnCompleted
        );

        // ========================================================
        // HMI NETWORK SERVICE
        //
        // Gestisce:
        // - accettazione delle connessioni TCP;
        // - rilascio dei client disconnessi;
        // - polling dei server Modbus TCP.
        //
        // ProcessNetwork() verifica internamente:
        // - configurazione HMI abilitata;
        // - engine correttamente avviato;
        // - loop HMI abilitato.
        // ========================================================

        hmiEngine.ProcessNetwork(
            *Manager,
            now
        );

        // ========================================================
        // DOMO MANAGER CYCLE
        //
        // Questo è il punto centrale del runtime.
        //
        // Il ciclo esegue:
        // - core DomoManager;
        // - polling Modbus;
        // - NetworkManager;
        // - EventManager;
        // - logica;
        // - callback TaskEngine::Loop;
        // - task registrati nel TaskEngineOrchestrator.
        //
        // Non richiamare direttamente qui:
        // - Task_MQTT;
        // - Task_Bridge;
        // - Task_Solar;
        // - Task_Meteo;
        // - Task_Power;
        // - Task_PowerSupervisor;
        // - Task_Jobs;
        // - hmiEngine.Sync.
        //
        // Sono già task schedulati.
        // ========================================================

        Manager->loop(
            network
                ->modbusTCP()
                .modbus
        );

        // ========================================================
        // FULL CYCLE SYNCHRONIZATION
        //
        // La callback applicativa viene eseguita solamente quando
        // risultano completati entrambi:
        // - ciclo backend;
        // - ciclo frontend.
        //
        // CheckFullCycle() provvede anche al reset dei due flag.
        // ========================================================

        CheckFullCycle();
    }
};

// ============================================================
// STATIC HMI INSTANCE
// ============================================================

HMIEngine DomoManagerFrontendEngine::hmiEngine;


#endif
