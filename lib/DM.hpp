#ifndef DM_HPP
#define DM_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑06‑24
   Note:
                    • Nessuna

   ============================================================================ */

#include <Arduino.h>

#include "DMFncs.hpp"
#include "DMAutomationBuilder.hpp"
#include "DMHStandby.hpp"
#include "DMDeclares.h"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class DomoManager;  // forward declarations
struct DiagnosticConfig;


// ============================================================
// CLASSI DERIVATE CHE AGGIUNGONO LE APPLY()
// ============================================================

class DomoManagerBufferEngineEx : public DomoManagerBufferEngine {
public:
    static void apply(DomoManager& manager, const AreasConfig& cfg);
};

class DomoManagerToggleEngineEx : public DomoManagerToggleEngine {
public:
    static void apply(DomoManager& manager, const TogglesConfig& cfg);
};

class DomoManagerSplitEngineEx : public DomoManagerSplitEngine {
public:
    static void apply(DomoManager& manager, const SplitsConfig& cfg);
};

//Solo dichiarazione, implementazione in fondo
class DiagnosticWatchAreas {
public:
    struct WatchedArea {
        int area;
        bool enabled;
        long lastValue;
    };
private:
    std::vector<WatchedArea> watched;
    bool autoPaused = false;

public:
    void addArea(int area, bool enabled = true) {
        watched.push_back({area, enabled, LONG_MIN});
    }

    bool isWatched(int area) const {
        if (autoPaused) 
            return false;   // 🔥 evita blocco permanente
        for (auto& w : watched)
            if (w.area == area && w.enabled)
                return true;
        return false;
    }

    bool isPaused() const { return autoPaused; }

    void enableArea(int area, bool en);
    void onValueChanged(DomoManager& manager, int area, long newValue);
    void onButtonPressed(DomoManager& manager, int mode, const DiagnosticConfig& cfg);

    void printAll(DomoManager& manager);
};

class Logic {
private:
    ToggleManager toggles;
    SplitOutManager splits;
    AnalogThresholdManager thresholds;
    ModbusManager mdb;   // contiene routeManager

public:
    Logic(SplitOutManager::CallbackFn cb)
        : toggles(),
          splits(cb),
          thresholds(),
          mdb()
    {}

    // -------------------------
    // GET
    // -------------------------
    ToggleManager& getToggles() { return toggles; }
    SplitOutManager& getSplits() { return splits; }
    RouteManager& getRoutes() { return mdb.routeManager; }
    AnalogThresholdManager& getThresholds() { return thresholds; }
    ModbusManager& getModbus() { return mdb; }

    // -------------------------
    // ADD
    // -------------------------
    void addToggle(int areaRead, std::vector<int> forwards = {}) {
        toggles.add(areaRead, forwards);
    }

    void addThreshold(int area, int low, int high = -1) {
        thresholds.add(area, low, high);
    }

    void addSplit(int mainArea,
                  std::vector<int> outAreas = {},
                  unsigned long maxTime = -1) {
        splits.add(mainArea, outAreas, maxTime);
    }

};

void mySplitCallback(const SplitOutManager::Split& s, bool isStart); //Forward Callback

class DomoConfigValidator {
private:
    static int CalculateMaxAreasFromConfig(const DomoManagerConfig& cfg) {
        int maxArea = 0;

        // 1) DEFINE_AREA (AreaRegistry)
        maxArea = (AreaRegistry::maxValue() > maxArea)
            ? AreaRegistry::maxValue()
            : maxArea;


        // 2) DeviceConfig
        for (auto& d : cfg.devices.list) {
            for (int a : d.areas) {
                if (a > maxArea)
                    maxArea = a;
            }
        }

        // 3) AreasConfig
        for (auto& a : cfg.areas.list) {
            if (a.area > maxArea)
                maxArea = a.area;
            if (a.forwardArea > maxArea)
                maxArea = a.forwardArea;
        }

        // 4) Toggles
        for (auto& t : cfg.toggles.list) {
            if (t.areaRead > maxArea)
                maxArea = t.areaRead;
            for (int f : t.forwards)
                if (f > maxArea)
                    maxArea = f;
        }

        // 5) Splits
        for (auto& s : cfg.splits.list) {
            if (s.mainArea > maxArea)
                maxArea = s.mainArea;
            for (int o : s.outAreas)
                if (o > maxArea)
                    maxArea = o;
        }

        return maxArea + 1;   // dimensione buffer richiesta dalla config
    }

    static bool isAreaKnownInConfig(int area, const DomoManagerConfig& cfg) {
        // 1) Aree dichiarate in AreasConfig
        for (auto& a : cfg.areas.list)
            if (a.area == area)
                return true;

        // 2) Aree mappate dai device
        for (auto& d : cfg.devices.list)
            for (int da : d.areas)
                if (da == area)
                    return true;

        // 3) Aree definite via DEFINE_AREA (AreaRegistry)
        for (int i = 0; i < AreaRegistry::count(); i++) {
            if (AreaRegistry::getValueByIndex(i) == area)
                return true;
        }

        return false;
    }

    static bool validateAreas(const DomoManagerConfig& cfg, DomoManager& dm);

public:
    enum class StaticValidationResult {
        OK,
        WARNING,
        ERROR
    };

    static bool validateStatic(const DomoManagerConfig& cfg, DomoManager& dm);

    static bool validateDynamic(const DomoManagerConfig& cfg, DomoManager& dm);

    static bool validate(const DomoManagerConfig& cfg, DomoManager& dm);

    static bool validateDefineAreas();
    static bool validateDeviceAreas(const DomoManagerConfig::Devices& cfg, DomoManager& dm);
    static bool validateDuplicateDeviceAreas(const DomoManagerConfig::Devices& cfg);
    static bool validateRoutes(const DomoManagerRouteEngine::RoutesConfig& cfg, DomoManager& dm);
    static bool validateSplits(const DomoManagerSplitEngineEx::SplitsConfig& cfg, DomoManager& dm);
    static bool validateToggles(const DomoManagerToggleEngineEx::TogglesConfig& cfg, DomoManager& dm);

    static int getRequiredAreas(const DomoManagerConfig& cfg) {
        return CalculateMaxAreasFromConfig(cfg);
    }

};


class DomoManager {
private:
    TimeManager timeManager;
    DeviceManager deviceManager;
public:
    NetworkManager net;   // 🔥 orchestratore di rete

    inline DeviceManager& devices() { return deviceManager; }

    using ActivityLoopFn = void (*)(DomoManager&, unsigned long);
    using SomethingChangedFn = void (*)();

    // Static instance for wrappers 
    static DomoManager* instance;

    void initAutomations(void (*fn)(DomoManager&)) {
        fn(*this);
    }
    void initDiagnostics(void (*fn)(DomoManager&)) {
        fn(*this);
    }

#if HOTSTANDBY_ENABLED
private:
    bool isClusterMasterFlag = false;
    HotStandbyManager hotStandby{false, &timeManager};
public:
    inline HotStandbyManager& getHotStandby() { return hotStandby; }
    inline bool isClusterMaster() const { return isClusterMasterFlag; }

    void enableHotStandby(bool startAsMaster) {
        hotStandby.isMaster = startAsMaster;
        isClusterMasterFlag = startAsMaster;
        hotStandby.begin(9600);
    }

    void setClusterMaster(bool m) {
        isClusterMasterFlag = m;
    }
#endif

private:
    enum class ProtocolType {
        MODBUS_RTU,
        MODBUS_HMI,
        COUNT
    };
    int protocolIds[(int)ProtocolType::COUNT];

    EthernetClient* hmiClient = nullptr;
    EthernetClient& getHMIClient() { return *hmiClient; }

    DomoManagerConfig config; 

    OwnerManager owner;
    
    IpManager ipManager;
    Buffer buffer;

    Logic logic;

    SomethingChangedFn somethingChanged;
    ActivityLoopFn activityLoop;

    // LedController instance
    LedController m_leds;

    int areaErrors, areaRunningT;

    // NEW: timing struct
    Watchdog::CallbackTimings timings;
    Watchdog watchdog;
    bool watchdogEnabled = false;

    //Buffer letture modbus
    std::vector<uint16_t> mbReadBuffer;
    
    DeviceProfiles profiles;
    DiagnosticWatchAreas WatchDiag;

    //Gestione flag per ogni ciclo completo del loop
    bool activityLoopCompleted = false;

    using FullCycleCallbackFn = void (*)(DomoManager&);
    FullCycleCallbackFn fullCycleCallback = nullptr;

    AutomationBuilder::AutomationConfig automationConfig;

    // ---------- Timing Helpers ----------
    
    template<typename Fn>
    unsigned long Measure(Fn f) {
        unsigned long now = timeManager.nowMs();
        f();
        return timeManager.nowMs() - now;
    }

    void UpdateTiming(Watchdog::ExecTiming &t,
                  unsigned long exec,
                  int thresholdFactor_fp)
    {
        const int SPIKE_READ_INTERVAL = 5000;

        t.last = exec;

        // ---------------------------------------------------------
        // 1) Inizializzazione media (prima esecuzione)
        // ---------------------------------------------------------
        if (t.avg_fp == 0) {
            t.avg_fp = exec << 8;   // Q8.8
            return;
        }

        // ---------------------------------------------------------
        // 2) EMA Q8.8 (Exponential Moving Average)
        //    ALPHA = 0.10 → media stabile ma reattiva
        // ---------------------------------------------------------
        constexpr int ALPHA_FP = 26;           // 0.10 in Q8.8
        constexpr int ONE_MINUS_ALPHA_FP = 230; // 0.90 in Q8.8

        int exec_fp = exec << 8;

        t.avg_fp = ((t.avg_fp * ONE_MINUS_ALPHA_FP) +
                    (exec_fp * ALPHA_FP)) >> 8;

        // ---------------------------------------------------------
        // 3) Calcolo soglia spike (Q8.8 → ms)
        // ---------------------------------------------------------
        int spikeThreshold_fp = (t.avg_fp * thresholdFactor_fp) >> 8;

        unsigned long avg_ms = (unsigned long)(t.avg_fp >> 8);
        unsigned long thr_ms = (unsigned long)(spikeThreshold_fp >> 8);

        // ---------------------------------------------------------
        // 4) Soglia minima per evitare spike falsi
        // ---------------------------------------------------------
        if (thr_ms < 2) thr_ms = 2;      // 🔥 evita thr=0
        if (avg_ms < 1) avg_ms = 1;      // 🔥 evita avg=0

        // ---------------------------------------------------------
        // 5) Spike detection reale
        // ---------------------------------------------------------
        bool wasSpike = t.spike;
        t.spike = (exec > thr_ms);

        if (t.spike) {
            unsigned long now = millis();

            if (exec > t.maxSpike)
                t.maxSpike = exec;

            t.spikeCount++;
            t.lastSpikeTime = now;

            // Logga solo il primo spike o ogni 5 secondi
            static unsigned long lastSpikeLog = 0;

            if (!wasSpike || (now - lastSpikeLog > SPIKE_READ_INTERVAL)) {
                lastSpikeLog = now;

                LOG_WF("DomoManager",
                    "[SPIKE] %s exec=%lu avg=%lu thr=%lu",
                    t.name,
                    exec,
                    avg_ms,
                    thr_ms);
            }
        }
    }


    // ---- WRAPPERS ----
    //Invece di passare i callback originali direttamente a ManageMdbCli, passi dei wrapper che misurano il tempo e poi chiamano il callback vero.
   static void SomethingChangedWrapper() {
        if (!instance) return;

        auto& changed = instance->getBuffer().getChangedMap();
        if (!changed.empty()) {
            for (auto& kv : changed) {
                int area = kv.second.area;
                int type = kv.second.type;

                // 🔥 controlla se quest’area è monitorata
                if (instance->WatchDiag.isWatched(area)) {
                    BufferSourceInfo info;
                    instance->getBuffer().GetData(area, (BufferFlagType)type, info);
                    long value = info.value;

                    instance->WatchDiag.onValueChanged(*instance, area, value);
                    break;   // 🔥 esci subito, velocissimo
                }
            }
        }

        // callback utente + timing
        if (instance->somethingChanged) {
            unsigned long exec = instance->Measure([&]() {
                instance->somethingChanged();
            });

            instance->UpdateTiming(
                instance->timings.somethingChanged,
                exec,
                instance->getConfig().watchdog.spikeThresholdFactor_fp
            );
        }
    }
   
    static void RouteTimingCallback(unsigned long exec) {
        if (instance) {
            instance->UpdateTiming(
                instance->timings.route,
                exec,
                instance->getConfig().watchdog.spikeThresholdFactor_fp
            );
        }
    }
    
    AutomationEngine automation;
    AsyncScheduler scheduler;
    
    AverageCalculator  averages;

    //Gestione automatica ciclo update
    using LoopTaskFn = void(*)(DomoManager*);
    struct LoopTask {
        LoopTaskFn fn;
        uint8_t weight;   // ogni quanti cicli eseguirlo
        uint8_t counter;  // countdown interno
    };
    static LoopTask loopTasks[];   // 🔥 dichiarazione dell’array
    static const uint8_t loopTaskCount;
    uint8_t loopTaskIndex = 0;
    bool ipCycleCompleted = false;
    bool activityPrepareReady = false;
    bool powerOnCycleCompleted = false;  // one-shot globale, MAI reset
    bool backendCycleDone = false;
    bool hmiCycleReady=false;

    //****************** TASK *****************
    static void Task_Scheduler(DomoManager* dm) {
        dm->getScheduler().run(dm->timeManager.nowMs());
    }

    static void Task_Splits(DomoManager* dm) {
        if (dm->getLogic().getSplits().hasActiveSplits())
            dm->getLogic().getSplits().update();
    }

    static void Task_Automation(DomoManager* dm) {
        if (!dm->ipCycleCompleted)
            return;

        dm->automation.update(dm->timeManager.nowMs());
    }

    static void Task_WatchdogAndRunningT(DomoManager* dm) {
        const int CHECK_INTERVAL=2000;
        static unsigned long lastWatchdogCheck = 0;
        unsigned long now = dm->timeManager.nowMs();

        if (now - lastWatchdogCheck >= CHECK_INTERVAL) {   // ogni 2 secondi
            if (dm->watchdogEnabled)
                dm->watchdog.check(now);

            lastWatchdogCheck = now;

            // Scrive il tempo dell’ultimo ciclo nel buffer
            dm->getBuffer().WriteElement(
                dm->areaRunningT,
                ToPanel,
                dm->getTimings().updateCycle.last,
                now
            );
        }
    }

    
    static void Task_RestartIP(DomoManager* dm) {
        const int CHECK_INTERVAL=5000;
        static unsigned long lastCheck = 0;
        unsigned long now = dm->timeManager.nowMs();

        // Controllo ogni 5 secondi (puoi aumentare se vuoi)
        if (now - lastCheck < CHECK_INTERVAL)
            return;

        lastCheck = now;

        auto& ips = dm->ipManager.GetIps();
        bool restartIP = true;

        for (short i = 0; i < ips.size(); i++) {
            bool ipInError = (ips[i].state == IpManager::IpState::COOLDOWN ||
                  ips[i].state == IpManager::IpState::EXCLUDED);

            if (!(ipInError && ips[i].Errors > 5)) {
                restartIP = false;
                break;
            }
        }

        if (restartIP) {
            LOG_EF("DomoManager", "Tutti gli IP in errore → System Reset");
            NVIC_SystemReset();
        }
    }

    static void Task_ActivityLoop(DomoManager* dm) {
        static unsigned long lastRun = 0;
        unsigned long now = dm->timeManager.nowMs();
        
        // 1. Deve essere stato completato almeno un ciclo IP
        if (!dm->activityPrepareReady)
            return;

        dm->watchdog.setActivityLoopRateLimit(dm->getConfig().watchdog.rate_limit_min_interval);

        // 2. Deve essere passato il tempo minimo
        if (now - lastRun < dm->getConfig().watchdog.rate_limit_min_interval)
            return;

        // OK → esegui il task
        lastRun = now;
        
        unsigned long exec = dm->Measure([&]() {
            dm->activityLoop(*dm, now);
        });

        dm->UpdateTiming(
            dm->timings.activityLoop,
            exec,
            dm->getConfig().watchdog.spikeThresholdFactor_fp
        );

        dm->activityPrepareReady = false;   // reset del flag
    }

    static void Task_HMI(DomoManager* dm)
    {
        // HMI disabilitata → consideriamo completato il ciclo di power-on
        if (!dm->config.hmi.enabled) {
            dm->powerOnCycleCompleted = true;
            return;
        }

        // L’HMI può girare solo dopo un ciclo Modbus completo
        if (!dm->hmiCycleReady)
            return;

        auto& net = dm->net;
        int hmiId = dm->protocolIds[(int)ProtocolType::MODBUS_HMI];
        unsigned long now = dm->timeManager.nowMs();

        // Pacing HMI (non bloccante)
        if (!net.canRun(hmiId, now))
            return;

        // La rete deve essere libera (Modbus ha priorità via scheduler)
        if (!net.tryAcquire(hmiId, now))
            return;

        
        static bool rw = false;

        LOG_DF("DomoManager", "HMI: %d", rw);

        dm->logic.getModbus().RunServer(
            dm->m_leds,
            dm->getHMIClient(),
            "Server1",
            rw,
            now
        );

        // WRITE → ciclo RW completato → fine power-on
        if (rw && !dm->powerOnCycleCompleted)
            dm->powerOnCycleCompleted = true;

        // alternanza READ/WRITE
        rw = !rw;

        // l’HMI ha consumato il ciclo
        dm->hmiCycleReady = false;

        net.release(hmiId, now);
    }

    void initBuffer() {
        // First 9 Areas are reserved, starts from 10.
        // Diagnostica
        DefineBufferElement(deviceManager.AREA_SYSTEM_ERRORS, 0, true, false, false, "Devices in error");  
        DefineBufferElement(deviceManager.AREA_SYSTEM_RUNNING_T, 0, true, false, false, "Current cycle"); 

        // 🔥 BUFFER DINAMICO: dimensione max del canale
        mbReadBuffer.resize(deviceManager.GetMaxReadSize() );
        buffer.Init();
        std::vector<int> multipleInit=buffer.getInitializedMultipleTimes();
        if (!multipleInit.empty()) { 
            for (int area : multipleInit) { 
                LOG_EF("DomoManager",
                "ERRORE: Area initialized more times: %d",
                area);
            } 
        }

        // ------------------------------------------------------------
        // AUTO-VIRTUAL: tutte le aree non mappate da device diventano virtual
        // ------------------------------------------------------------
        auto& buf = getBuffer();
        auto& devices = deviceManager.getDevices();   // serve getter in DomoManager

        std::unordered_set<int> mappedAreas;

        // 1. Raccogli tutte le aree mappate da device fisici
        for (auto& dev : devices) {
            int channels = dev.GetChannelsSize();
            for (int ch = 0; ch < channels; ch++) {
                auto info = dev.GetChannelInfo(ch);
                for (int i = 0; i < info.items; i++) {
                    int area = dev.GetArea(ch, i);
                    mappedAreas.insert(area);
                }
            }
        }

        // 2. Marca come virtual tutte le aree definite nel buffer ma non mappate
        std::vector<int> virtualAreas;

        for (int area = 0; area < buf.size(); area++) {
            if (!mappedAreas.count(area)) {
                buf.SetVirtual(area, true);
                virtualAreas.push_back(area);
            }
        }

        // 3. LOG riepilogativo
        if (!virtualAreas.empty()) {
            String list;
            for (int a : virtualAreas) {
                list += String(a) + " ";
            }
            LOG_IF("InitBuffer", "Auto-detected virtual areas: %s", list.c_str());
        } 
    }

    void mapDevices(DomoManager &dm, const DomoManagerConfig::Devices& cfg) {
        auto& profiles = dm.getProfiles();

        for (const auto& d : cfg.list) {
            auto* profile = profiles.get(d.profile.c_str());
            if (!profile) {
                LOG_EF("Profilo non trovato: %s", d.profile.c_str());
                continue;
            }

            // 1) Crea il device
            dm.devices().addDeviceManual(
                d.name.c_str(),
                d.address,
                d.slot,
                *profile,
                d.areas,
                d.retry,
                d.priority
            );

            // 2) Recupera il device appena creato
            auto& dev = dm.devices().getDevices().back();
            //dev.GetError().SetName(d.name.c_str());   // Solo per debug
            // 3) Aggancia il callback degli errori
            dev.GetError().SetStateChangedCallback([&dm, &dev]() {
                dm.OnDeviceErrorStateChanged(&dev);
            });
        }
    }

    void OnDeviceErrorStateChanged(GenericPrgDevice* dev)
    {
        auto& devs = deviceManager.getDevices();

        int totalErrors = 0;
        for (auto& dev : devs) {
            if (dev.GetError().IsVisibleError() || dev.GetError().IsVisibleParked())
                totalErrors++;
        }

        unsigned long now = timeManager.nowMs();

        // LED ERR
        m_leds.set(LedController::FOUR, totalErrors > 0);

        // Buffer diagnostico
        buffer.WriteElement(
            areaErrors,
            ToPanel,
            totalErrors,
            now
        );

        LOG_IF("DomoManager",
            "Device error state changed → totalErrors=%d",
            totalErrors);
    }

public:
    DomoManager(const LedController::LedPins& ledPins)
    : logic(mySplitCallback),
    timeManager(),
      deviceManager(),
      owner(),
      ipManager(),
      buffer(1),
      somethingChanged(nullptr),
      activityLoop(nullptr),
    m_leds(ledPins, false),
      areaErrors(DeviceManager::AREA_SYSTEM_ERRORS),
      areaRunningT(DeviceManager::AREA_SYSTEM_RUNNING_T),
      timings(),
      watchdog(&timings, &config.watchdog),
      watchdogEnabled(false),
      mbReadBuffer(),
      profiles(),
      WatchDiag(),
      automation(),
      scheduler(),
      averages(),
      loopTaskIndex(0)
    {
        powerOnCycleCompleted=false;
        activityPrepareReady=false;
        ipCycleCompleted=false;

        // nomi per il watchdog
        timings.somethingChanged.name = "somethingChanged";
        timings.route.name            = "route";
        timings.activityLoop.name     = "activityLoop";
        timings.updateCycle.name      = "updateCycle";

        #if HOTSTANDBY_ENABLED
            isClusterMasterFlag = false;
        #endif
    }


    bool setup(SomethingChangedFn somethingChanged, ActivityLoopFn activityLoop, DomoManagerConfig config) {
        this->somethingChanged = somethingChanged;
        this->activityLoop = activityLoop;
        this->config = config;

        // set static instance pointer 
        instance = this;

        // inizializza i led (gestisce NO_PIN internamente)
        m_leds.begin();

        //Pre controllo su configurazione vuota
        if (this->config.devices.list.empty() ||
            this->config.areas.list.empty())
        {
            m_leds.set(LedController::THREE, true);
            m_leds.set(LedController::TWO, true);
            LOG_EF("DomoManager",
                "CONFIGURAZIONE VUOTA → impossibile avviare il sistema, caricare in DmSetup.hpp una configurazione valida");
            return false;
        }

        mapDevices(*this, this->config.devices);
        
        // 🔥 Ricostruzione buffer con dimensione corretta
        {
            int totalAreas = DomoConfigValidator::getRequiredAreas(config);
            buffer = Buffer(totalAreas);
        }
        
        // 1) VALIDAZIONE STATICA PRIMA DI TUTTO
        if (!DomoConfigValidator::validateStatic(config, *this)) {
            LOG_EF("DomoManager", "AVVIO BLOCCATO (validazione statica)");
            return false;   // ERROR → blocca
        }

        // Se siamo qui, validateStatic() = true → OK o WARNING
        // Il warning è già stato loggato dentro validateStatic()

    
        // BUFFER
        DomoManagerBufferEngineEx::apply(*this, config.areas);
   
        // TOGGLES
        DomoManagerToggleEngineEx::apply(*this, config.toggles);

        // SPLITS
        DomoManagerSplitEngineEx::apply(*this, config.splits);
            
        // Carica le route nel motore
        DomoManagerRouteEngine::apply(logic.getRoutes(), config.routes);

        // 🔥 inizializzazione interna del buffer
        this->initBuffer();

        // 4) VALIDAZIONE DINAMICA
        if (!DomoConfigValidator::validateDynamic(config, *this)) {
            LOG_EF("DomoManager", "AVVIO BLOCCATO (validazione dinamica)");
            return false;
        }
        m_leds.setAll(true);

        // 🔥 Inizializzazione pacing protocolli
        protocolIds[(int)ProtocolType::MODBUS_RTU] =
            net.registerProtocol(25, 35); 

        protocolIds[(int)ProtocolType::MODBUS_HMI] =
            net.registerProtocol(config.hmi.pollingMs, 25);

        ipManager.BuildIps(deviceManager.getDevices());

        LOG_IF("DomoManager", "Hw items to query: %d", deviceManager.getDevices().size());

        logic.getModbus().Begin(
            m_leds,
            buffer,
            deviceManager.getDevices(),
            logic.getToggles(),
            logic.getSplits(),
            ipManager,
            logic.getThresholds(),
            &DomoManager::SomethingChangedWrapper,
            &DomoManager::RouteTimingCallback
        );

        automation.attachScheduler(&scheduler, &buffer, &averages, &timeManager);
        delay(1000);
        m_leds.setAll(false);

        return true;
    }


    void loadAutomationJson(const char* json) {
        // Parsing direttamente nella config persistente
        if (!AutomationBuilder::parseJson(json, this->automationConfig)) {
            LOG_EF("Automation", "Errore parsing automazioni!");
            return;
        }

        // ============================================================
        // AUTOMATION JSON CHECK
        // ============================================================
        {
            auto& buf = this->buffer;

            bool ok = true;
            const auto& autoCfg = automationConfig;

            // 🔥 Se non ci sono automazioni → skip totale
            if (autoCfg.scenes.empty() &&
                autoCfg.rules.empty() &&
                autoCfg.sequences.empty())
            {
                LOG_IF("Automation", "Nessuna automazione configurata → skip validazione");
            }
            else
            {
                // ---- Raccolta nomi scene (compatibile con Arduino) ----
                std::vector<String> sceneNames;
                sceneNames.reserve(autoCfg.scenes.size());
                for (const auto& s : autoCfg.scenes)
                    sceneNames.push_back(s.name);

                // Funzione di lookup semplice (niente lambda)
                auto sceneExists = [&](const String& name) -> bool {
                    for (size_t i = 0; i < sceneNames.size(); i++)
                        if (sceneNames[i] == name)
                            return true;
                    return false;
                };

                // ============================================================
                // 6.1 VALIDAZIONE SCENE
                // ============================================================
                for (const auto& s : autoCfg.scenes) {

                    if (s.actions.empty()) {
                        LOG_WF("Automation",
                            "Scene '%s' non contiene azioni",
                            s.name.c_str());
                    }

                    for (const auto& a : s.actions) {

                        if (a.area < 0 || a.area >= buf.size()) {
                            LOG_EF("Automation",
                                "Scene '%s' usa area %d fuori range",
                                s.name.c_str(), a.area);
                            ok = false;
                        }

                        if (buf.IsVirtual(a.area)) {
                            LOG_WF("Automation",
                                "Scene '%s' scrive su area virtuale %d",
                                s.name.c_str(), a.area);
                        }
                    }
                }

                // ============================================================
                // 6.2 VALIDAZIONE RULES
                // ============================================================
                for (const auto& r : autoCfg.rules) {
                    // ---- SceneTrue ----
                    if (!r.sceneTrue.isEmpty()) {

                        // ⭐ Se NON è built‑in e NON esiste → errore
                        if (!AutomationEngine::BuiltinScenes::isBuiltin(r.sceneTrue.c_str()) &&
                            !sceneExists(r.sceneTrue))
                        {
                            LOG_EF("Automation",
                                "Rule '%s' sceneTrue '%s' non esiste",
                                r.name.c_str(), r.sceneTrue.c_str());
                            ok = false;
                        }
                    }

                    // ---- SceneFalse ----
                    if (!r.sceneFalse.isEmpty()) {

                        // ⭐ Se NON è built‑in e NON esiste → errore
                        if (!AutomationEngine::BuiltinScenes::isBuiltin(r.sceneFalse.c_str()) &&
                            !sceneExists(r.sceneFalse))
                        {
                            LOG_EF("Automation",
                                "Rule '%s' sceneFalse '%s' non esiste",
                                r.name.c_str(), r.sceneFalse.c_str());
                            ok = false;
                        }
                    }

                    // ---- Trend ----
                    if (r.type == "trend") {
                        if (r.trend.area < 0 || r.trend.area >= buf.size()) {
                            LOG_EF("Automation",
                                "Rule '%s' trend area %d fuori range",
                                r.name.c_str(), r.trend.area);
                            ok = false;
                        }
                    }

                    // ---- Threshold ----
                    if (r.type == "threshold") {
                        if (r.threshold.area < 0 || r.threshold.area >= buf.size()) {
                            LOG_EF("Automation",
                                "Rule '%s' threshold area %d fuori range",
                                r.name.c_str(), r.threshold.area);
                            ok = false;
                        }
                    }

                    // ---- Bitmask ----
                    if (r.type == "bitmask") {
                        if (r.bitmask.area < 0 || r.bitmask.area >= buf.size()) {
                            LOG_EF("Automation",
                                "Rule '%s' bitmask area %d fuori range",
                                r.name.c_str(), r.bitmask.area);
                            ok = false;
                        }
                        if (r.bitmask.bitIndex < 0 || r.bitmask.bitIndex > 31) {
                            LOG_EF("Automation",
                                "Rule '%s' bitmask bitIndex %d fuori range",
                                r.name.c_str(), r.bitmask.bitIndex);
                            ok = false;
                        }
                    }

                    // ---- Multi ----
                    if (r.type == "multi") {
                        for (const auto& c : r.multi.conditions) {
                            if (c.area < 0 || c.area >= buf.size()) {
                                LOG_EF("Automation",
                                    "Rule '%s' multi condition area %d fuori range",
                                    r.name.c_str(), c.area);
                                ok = false;
                            }
                        }
                    }

                    // ---- Composite ----
                    if (r.type == "composite") {

                        // INPUTS
                        for (const auto& in : r.composite.inputs) {

                            if (in.area < 0 || in.area >= buf.size()) {
                                LOG_EF("Automation",
                                    "Rule '%s' composite input area %d fuori range",
                                    r.name.c_str(), in.area);
                                ok = false;
                            }

                            if (in.type == "bitmask" &&
                                (in.bitIndex < 0 || in.bitIndex > 31)) {
                                LOG_EF("Automation",
                                    "Rule '%s' composite bitIndex %d fuori range",
                                    r.name.c_str(), in.bitIndex);
                                ok = false;
                            }
                        }

                        // OUTPUT
                        if (r.composite.output.area < 0 ||
                            r.composite.output.area >= buf.size()) {
                            LOG_EF("Automation",
                                "Rule '%s' composite output area %d fuori range",
                                r.name.c_str(), r.composite.output.area);
                            ok = false;
                        }

                        if (r.composite.output.bitIndex < 0 ||
                            r.composite.output.bitIndex > 31) {
                            LOG_EF("Automation",
                                "Rule '%s' composite output bitIndex %d fuori range",
                                r.name.c_str(), r.composite.output.bitIndex);
                            ok = false;
                        }
                    }
                }

                // ============================================================
                // 6.3 VALIDAZIONE SEQUENCES
                // ============================================================
                for (const auto& seq : autoCfg.sequences) {
                    for (const auto& st : seq.steps) {

                        if (st.area < 0 || st.area >= buf.size()) {
                            LOG_EF("Automation",
                                "Sequence step usa area %d fuori range",
                                st.area);
                            ok = false;
                        }

                        if (buf.IsVirtual(st.area)) {
                            LOG_WF("Automation",
                                "Sequence step scrive su area virtuale %d",
                                st.area);
                        }
                    }
                }
            }
        }
        
        // Costruzione automazioni usando la config persistente
        AutomationBuilder builder;

        builder.build(
            this->automation,     // AutomationEngine&
            this->buffer,         // Buffer&
            this->averages,          // CalcolatoreMedie&
            this->timeManager,    // TimeManager&
            this->automationConfig );
    }


    const AutomationBuilder::AutomationConfig& getAutomationBuilderConfig() const {
        return automationConfig;
    }

    void SetWatchdogCallback(Watchdog::WatchdogFn fn) { 
        watchdog.setCallback(fn); 
    }

    DiagnosticWatchAreas& getWatchDiag() { return WatchDiag; }
    AutomationEngine& getAutomation() { return automation; }
    AsyncScheduler& getScheduler() { return scheduler; }
    Buffer& getBuffer() { return buffer; }
    AverageCalculator & getAverages() { return averages; }
    
    TimeManager& getTimeManager() { return timeManager; }

    DeviceProfiles& getProfiles() { return profiles; }
    inline const DomoManagerConfig& getConfig() const {
        return config;
    }

    bool isAreaFromDeviceInError(int area)
    {
        for (auto& dev : deviceManager.getDevices()) {

            // Device in errore?
            if (dev.GetError().IsInError()) {

                int ch = -1;
                int item = -1;

                // L’area appartiene a questo device?
                if (dev.FindChannelByArea(area, ch, item)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool hasBackendCycleCompleted() const {
        return backendCycleDone;
    }
    void resetBackendCycleFlag() {
        backendCycleDone = false;
    }

    bool getPowerOnCycleCompleted() { return powerOnCycleCompleted; }

    // public API
    LedController& getLedController() { return m_leds; }
    const LedController& getLedController() const { return m_leds; }

    const Watchdog::CallbackTimings& getTimings() const {
        return timings;
    }

    void loop(EthernetClient &client, ModbusTCPClient &modbusTCPClient,unsigned long now) {
        this->hmiClient = &client;

        #if HOTSTANDBY_ENABLED
            hotStandby.poll();
            if (!isClusterMasterFlag) {
                if (watchdogEnabled) watchdog.check();
                return;
            }
        #endif

        // MASTER → ciclo completo
        Update(client, modbusTCPClient, now);

        if(ipCycleCompleted) {
            LOG_DF("LOOP", "taskIndex=%d fn=%p", loopTaskIndex, loopTasks[loopTaskIndex].fn);

            //Task eseguiti solo dopo un ciclo completo di acquisizioni
            LoopTask& t = loopTasks[loopTaskIndex];

            // PRIORITÀ: esegui solo quando counter == 0
            if (t.counter == 0) {
                t.fn(this);
                t.counter = t.weight;   // reset
            } else {
                t.counter--;            // countdown
            }

            // Avanza automaticamente
            loopTaskIndex++;
            if (loopTaskIndex >= loopTaskCount) {
                loopTaskIndex = 0;
                backendCycleDone = true;   // 🔥 nessuna chiamata al frontend
            }
        }
    }
        
    void Update(EthernetClient &client,
            ModbusTCPClient &modbusTCPClient,
            unsigned long now)
    {
        unsigned long _runningT = now;
        static short ipIdx = 0;

        auto& ips = ipManager.GetIps();
        if (ips.empty()) return;

        auto& ip = ips[ipIdx];

        // ============================================================
        // 1) CIRCUIT BREAKER: NON tentare se in COOLDOWN o EXCLUDED
        // ============================================================
        if (!ipManager.ShouldQuery(ipIdx, now)) {
            ipIdx = (ipIdx + 1) % ips.size();
            return;
        }

        // ============================================================
        // 2) Se non ci sono device associati → escludi IP
        // ============================================================
        if (!ipManager.ExistDevicesByIp(deviceManager.getDevices(), ipIdx)) {
            LOG_WF("DomoManager",
                "No Devices to query: %d errors=%d",
                ipIdx,
                ip.Errors);

            ipManager.Exclude(ipIdx);
            ipIdx = (ipIdx + 1) % ips.size();
            return;
        }

        // ============================================================
        // 3) MODBUS CLIENT — ORCHESTRATO
        // ============================================================
        int modbusId = protocolIds[(int)ProtocolType::MODBUS_RTU];
        if (net.tryAcquire(modbusId, now)) {

            ModbusManager::ClientState state = logic.getModbus().RunClient(
                modbusTCPClient,
                ipIdx,
                502,
                mbReadBuffer,
                now
            );

            // ========================================================
            // 3.1) PACING DINAMICO
            // ========================================================
            switch (state) {

                case ModbusManager::ClientState::READ_DONE:
                    // Non avanza IP, non è un errore
                    net.setMinInterval(modbusId, 20);   // velocissimo ex 20
                    break;

                case ModbusManager::ClientState::WRITE_DONE:
                    // Non usato, ma coerente
                    net.setMinInterval(modbusId, 15);
                    break;

                case ModbusManager::ClientState::CYCLE_OK:
                    net.setMinInterval(modbusId, 15); // era 50
                    ipManager.ReportSuccess(ipIdx);
                    break;

                case ModbusManager::ClientState::DEVICE_ERROR:
                    net.setMinInterval(modbusId, 80); //Era 20
                    // nessun ReportError(ipIdx)
                    break;

                case ModbusManager::ClientState::ERROR:
                    net.setMinInterval(modbusId, 120); //Era 30
                    ipManager.ReportError(ipIdx, now);
                    break;
            }

            // ========================================================
            // 3.2) AVANZAMENTO IP
            // ========================================================
            if (state == ModbusManager::ClientState::CYCLE_OK ||
                state == ModbusManager::ClientState::ERROR ||
                state == ModbusManager::ClientState::DEVICE_ERROR)
            {
                // Avanza IP
                ipIdx = (ipIdx + 1) % ips.size();

                // Fine ciclo completo
                if (ipIdx == 0) {
                    activityPrepareReady=true;
                    ipCycleCompleted = true;
                    hmiCycleReady = true;   // 🔥 trigger Task_HMI
                }

                net.release(modbusId, now);
                return;
            }

            // Se arrivo qui → READ_DONE → NON avanza IP
            net.release(modbusId, now);
        }

        // ============================================================
        // 4) LEDS (non di rete)
        // ============================================================
        m_leds.update(now);

        // ============================================================
        // 5) WATCHDOG
        // ============================================================
        unsigned long _exec = now - _runningT;
        UpdateTiming(timings.updateCycle, _exec, getConfig().watchdog.spikeThresholdFactor_fp);
    }

    void DefineBufferElement(int modbusArea, int modbusAreaToWrite, bool WriteToPanel,
                             bool ReadFromPanel, bool Reverse, const char* name)  {
        buffer.SetElement(modbusArea, modbusAreaToWrite, WriteToPanel, ReadFromPanel, Reverse, name);
    }

    Logic& getLogic() { return logic; }

    void enableWatchdog() {
        watchdogEnabled = true;
    }

    OwnerManager& getOwner() { return owner; }

    bool isDeveloperMode() const {
        return owner.getMode() == OwnerManager::DEVELOPER;
    }
};

// ---- Static member definition ---- 
DomoManager* DomoManager::instance = nullptr;

void mySplitCallback(const SplitOutManager::Split& s, bool isStart) {
    auto& dm = DomoManager::instance->devices();
    Buffer* buf = &DomoManager::instance->getBuffer();
    unsigned long now = DomoManager::instance->getTimeManager().nowMs();

    LOG_DF("SplitCallback", 
           "Callback START=%d mainArea=%d outCount=%u",
           isStart ? 1 : 0,
           s.mainArea,
           (unsigned)s.outAreas.size());   

    for (int a : s.outAreas) {

        GenericPrgDevice* devFound = nullptr;
        int ch = -1;
        int item = -1;

        // 1️⃣ CERCA IL DEVICE CHE CONTIENE L’AREA
        for (auto& dev : dm.getDevices()) {
            if (dev.FindChannelByArea(a, ch, item)) {
                devFound = &dev;
                break;
            }
        }

        // 2️⃣ SE NON ESISTE → skip
        if (!devFound) {
            LOG_WF("SplitCallback", 
                   "Area %d non appartiene a nessun device", a);
            continue;
        }

        // 3️⃣ SE IL DEVICE È ESCLUSO → skip
        if (devFound->GetError().IsInError()) {
            LOG_WF("SplitCallback", 
                   "Skip WRITE: device %s area %d is excluded",
                   devFound->GetName(), a);
            continue;
        }

        // 4️⃣ SCRITTURA NEL BUFFER
        int value = isStart ? 1 : 0;
        buf->WriteElement(a, Field, value, now);

        LOG_DF("SplitCallback", 
               " → Write area=%d value=%d (isStart=%d)", 
               a, value, isStart ? 1 : 0);
    }
}


class DomoScheduler : public AsyncScheduler {
public:
    explicit DomoScheduler(DomoManager* mgr) {
        setContext(mgr);   // preferred
    }

    DomoManager* getManager() {
        return static_cast<DomoManager*>(context);
    }
};

DomoManager::LoopTask DomoManager::loopTasks[] = {
    { Task_Splits,     10, 0 },   // media
    { Task_Automation, 25, 0 },   // bassa priorità
    { Task_ActivityLoop,   5, 0 },
    { Task_Scheduler,  55, 0 },   // bassa priorità
    { Task_WatchdogAndRunningT, 15, 0 },   // ogni 5 cicli circa
    { Task_RestartIP, 120, 0 },   // ogni ~50 cicli, timer interno gestisce i 5s
    { Task_HMI,       30, 0 }   // eseguito ogni ~20 cicli, ma solo se hmiCycleReady==true
};

const uint8_t DomoManager::loopTaskCount =
    sizeof(DomoManager::loopTasks) / sizeof(DomoManager::loopTasks[0]);

// =====================================================
// IMPLEMENTAZIONI DOPO LA DEFINIZIONE DI DOMOMANAGER
// =====================================================

inline void DomoManagerBufferEngineEx::apply(
    DomoManager& manager,
    const AreasConfig& cfg)
{
    auto& L = manager.getLogic();

    for (const auto& a : cfg.list) {
        const auto& f = a.flags;

        manager.DefineBufferElement(
            a.area,
            a.forwardArea,
            f.hmiWritable,   // WriteToPanel
            f.hmiReadable,   // ReadFromPanel
            f.reverse,       // Reverse
            a.label.c_str()
        );

        if (a.threshold.low != -1) {
            L.getThresholds().add(a.area, a.threshold.low, a.threshold.high);
        }
    }
}

inline void DomoManagerToggleEngineEx::apply(
    DomoManager& manager,
    const TogglesConfig& cfg)
{
    auto& L = manager.getLogic();
    for (const auto& t : cfg.list)
        L.getToggles().add(t.areaRead, t.forwards);
}

inline void DomoManagerSplitEngineEx::apply(
    DomoManager& manager,
    const SplitsConfig& cfg)
{
    auto& L = manager.getLogic();
    for (const auto& s : cfg.list)
        L.getSplits().add(s.mainArea, s.outAreas, s.maxTime);
}

// ============================================================================
// IMPLEMENTAZIONE VALIDATORE
// ============================================================================
bool DomoConfigValidator::validateStatic(const DomoManagerConfig& cfg, DomoManager& dm) {
    bool ok = true;

    LOG_DF("Config", "VALIDAZIONE STATICA: avvio…");

    // ============================================================
    // 1) ERROR VERI → BLOCCANO
    // ============================================================
    ok &= validateDefineAreas();
    ok &= validateDuplicateDeviceAreas(cfg.devices);
    ok &= validateDeviceAreas(cfg.devices, dm);

    // ⭐ QUI: validazione aree reali dei device (riservate, duplicati, non definite)
    ok &= validateAreas(cfg, dm);

    ok &= validateRoutes(cfg.routes, dm);
    ok &= validateSplits(cfg.splits, dm);
    ok &= validateToggles(cfg.toggles, dm);

    if (!ok) {
        LOG_EF("Config", "VALIDAZIONE STATICA FALLITA (ERROR)");
        return false;
    }

    // ============================================================
    // 2) CONTROLLI AGGIUNTIVI (ERROR)
    // ============================================================

    // ---- 2.1 ForwardArea non definito ----
    for (auto& a : cfg.areas.list) {
        // forwardArea = 0 → significa "nessun forward", NON è un errore
        if (a.forwardArea > 0 && !isAreaKnownInConfig(a.forwardArea, cfg)) {
            LOG_EF("Config",
                "Area %d ha forwardArea %d non definita.",
                a.area, a.forwardArea);
            ok = false;
        }
    }

    // ---- 2.2 Aree duplicate in AreasConfig ----
    {
        std::unordered_set<int> seen;
        for (auto& a : cfg.areas.list) {
            if (!seen.insert(a.area).second) {
                LOG_EF("Config",
                       "Area %d definita due volte in AreasConfig.",
                       a.area);
                ok = false;
            }
        }
    }

    // ---- 2.3 Aree riservate 0..9 ----
    for (auto& a : cfg.areas.list) {
        if (a.area >= 0 && a.area <= 9) {
            LOG_EF("Config",
                   "Area %d è riservata e non può essere usata.",
                   a.area);
            ok = false;
        }
    }

    // ---- 2.4 Threshold invertiti ----
    for (auto& a : cfg.areas.list) {
        if (a.threshold.high != -1 &&
            a.threshold.low > a.threshold.high)
        {
            LOG_EF("Config",
                   "Area %d ha threshold invertiti (%d > %d).",
                   a.area, a.threshold.low, a.threshold.high);
            ok = false;
        }
    }

    if (!ok) {
        LOG_EF("Config", "VALIDAZIONE STATICA FALLITA (ERROR)");
        return false;
    }

    // ============================================================
    // 3) WARNING → NON BLOCCANO
    // ============================================================

    int requiredSize = CalculateMaxAreasFromConfig(cfg);
    int bufferSize   = DomoManager::instance->getBuffer().size();

    // ---- 3.1 Mismatch buffer/config ----
    if (requiredSize > bufferSize) {
        LOG_WF("Config",
               "VALIDAZIONE STATICA: WARNING → la config richiede %d aree, "
               "ma il buffer iniziale ne supporta solo %d. "
               "Le aree eccedenti saranno virtual.",
               requiredSize, bufferSize);
    }

    // ---- 3.2 Aree non mappate da device (saranno virtuali) ----
    for (auto& a : cfg.areas.list) {
        if (!isAreaKnownInConfig(a.area, cfg)) {
            LOG_WF("Config",
                   "Area %d non mappata da device → sarà virtuale.",
                   a.area);
        }
    }

    LOG_IF("Config", "VALIDAZIONE STATICA OK.");
    return true;
}

bool DomoConfigValidator::validateAreas(const DomoManagerConfig& cfg, DomoManager& dm)
{
    const int RESERVED_MAX = dm.devices().AREA_LAST_RESERVED;
    std::unordered_set<int> used;

    auto& devices = dm.devices().getDevices();
    for (auto& dev : devices)
    {
        int channels = dev.GetChannelsSize();

        for (int ch = 0; ch < channels; ch++)
        {
            auto info = dev.GetChannelInfo(ch);

            for (int i = 0; i < info.items; i++)
            {
                int area = dev.GetArea(ch, i);
 
                // 1. Aree riservate
                if (area <= RESERVED_MAX)
                {
                    LOG_EF("CONFIG",
                           "Device '%s' usa area riservata (%d <= %d)",
                           dev.GetName(),
                           area,
                           RESERVED_MAX);
                    return false;
                }

                // 2. Duplicati
                if (!used.insert(area).second)
                {
                    LOG_EF("CONFIG",
                           "Area duplicata trovata: %d (device '%s')",
                           area,
                           dev.GetName());
                    return false;
                }

                // 3. Aree non definite
                if (!isAreaKnownInConfig(area, cfg))
                {
                    LOG_EF("CONFIG",
                           "Area %d non definita in DEFINE_AREA, AreasConfig, Toggles o Splits",
                           area);
                    return false;
                }
            }
        }
    }

    return true;
}

bool DomoConfigValidator::validateDynamic(const DomoManagerConfig& cfg, DomoManager& dm) {
    bool ok = true;

    auto& buf  = dm.getBuffer();
    auto& devs = dm.devices().getDevices();

    LOG_DF("Config", "VALIDAZIONE DINAMICA: avvio…");

    // ============================================================
    // 1) DEVICE CHECK
    // ============================================================
    for (auto& d : devs) {

        // Device in errore
        if (d.GetError().IsInError()) {
            LOG_EF("Config",
                   "Device '%s' in errore durante l'inizializzazione",
                   d.GetName());
            ok = false;
        }

        // Aree mappate fuori range
        size_t channels = d.GetChannelsSize();
        for (size_t ch = 0; ch < channels; ch++) {
            auto info = d.GetChannelInfo(ch);
            for (int i = 0; i < info.items; i++) {

                int area = d.GetArea(ch, i);

                // 🔥 CONTROLLO CORRETTO: solo range, NON buf.Exists()
                if (area < 0 || area >= buf.size()) {
                    LOG_EF("Config",
                           "Device '%s' mappa area %d fuori range",
                           d.GetName(), area);
                    ok = false;
                }
            }
        }
    }

    // ============================================================
    // 2) BUFFER CHECK
    // ============================================================
    for (auto& a : cfg.areas.list) {

        // 🔥 CONTROLLO CORRETTO: solo range
        if (a.area < 0 || a.area >= buf.size()) {
            LOG_EF("Config",
                   "Area %d non esiste nel buffer",
                   a.area);
            ok = false;
            continue;
        }

        // Aree virtuali → solo warning
        if (buf.IsVirtual(a.area)) {
            LOG_WF("Config",
                   "Area %d è virtuale (nessun device la gestisce)",
                   a.area);
        }
    }

    // ============================================================
    // 3) TOGGLES CHECK
    // ============================================================
    for (auto& t : cfg.toggles.list) {

        if (t.areaRead < 0 || t.areaRead >= buf.size()) {
            LOG_EF("Config",
                   "Toggle legge area %d non esistente",
                   t.areaRead);
            ok = false;
        }

        for (int f : t.forwards) {
            if (f < 0 || f >= buf.size()) {
                LOG_EF("Config",
                       "Toggle forward verso area %d non esistente",
                       f);
                ok = false;
            }
        }
    }

    // ============================================================
    // 4) SPLITS CHECK
    // ============================================================
    for (auto& s : cfg.splits.list) {

        if (s.mainArea < 0 || s.mainArea >= buf.size()) {
            LOG_EF("Config",
                   "Split usa mainArea %d non esistente",
                   s.mainArea);
            ok = false;
        }

        for (int o : s.outAreas) {
            if (o < 0 || o >= buf.size()) {
                LOG_EF("Config",
                       "Split forward verso area %d non esistente",
                       o);
                ok = false;
            }
        }
    }

    // ============================================================
    // 5) ROUTES CHECK (compatibile con RouteManager reale)
    // ============================================================
    for (auto& r : cfg.routes.list) {

        if (r.triggerArea < 0 || r.triggerArea >= buf.size()) {
            LOG_EF("Config",
                   "Route '%s' usa triggerArea %d non esistente",
                   r.name.c_str(), r.triggerArea);
            ok = false;
        }

        for (auto& c : r.cases) {
            for (auto& act : c.actions) {

                // 🔥 Campo corretto: targetArea
                if (act.targetArea < 0 || act.targetArea >= buf.size()) {
                    LOG_EF("Config",
                           "Route '%s' ha action verso area %d non esistente",
                           r.name.c_str(), act.targetArea);
                    ok = false;
                }
            }
        }
    }

    // ============================================================
    // RISULTATO FINALE
    // ============================================================
    if (!ok) {
        LOG_EF("Config", "VALIDAZIONE DINAMICA FALLITA");
        return false;
    }

    LOG_IF("Config", "VALIDAZIONE DINAMICA OK");
    return true;
}



bool DomoConfigValidator::validate(const DomoManagerConfig& cfg, DomoManager& dm) {
    // 1) Validazione statica
    bool staticOk = validateStatic(cfg, dm);

    if (!staticOk) {
        // 🔥 ERROR → blocca
        return false;
    }

    // 2) Validazione dinamica
    bool dynamicOk = validateDynamic(cfg, dm);

    return dynamicOk;
}



// ----------------------------------------------------------------------------
// DEFINE_AREA
// ----------------------------------------------------------------------------
bool DomoConfigValidator::validateDefineAreas() {
    bool ok = true;
    std::unordered_set<int> seen;

    for (int i = 0; i < AreaRegistry::count(); i++) {
        int v = AreaRegistry::getValueByIndex(i);
        if (seen.count(v)) {
            LOG_EF("Config", "DEFINE_AREA duplicata: valore=%d", v);
            ok = false;
        }
        seen.insert(v);
    }
    return ok;
}

// ----------------------------------------------------------------------------
// DEVICE AREAS
// ----------------------------------------------------------------------------
bool DomoConfigValidator::validateDeviceAreas(const DomoManagerConfig::Devices& cfg, DomoManager& dm) {
    bool ok = true;

    for (auto& d : cfg.list) {
        auto* profile = dm.getProfiles().get(d.profile.c_str());
        if (!profile) {
            LOG_EF("Config", "Profilo '%s' non trovato", d.profile.c_str());
            ok = false;
            continue;
        }

        int expected = 0;
        for (auto& ch : *profile)
            expected += ch.items;

        if (expected != d.areas.size()) {
            LOG_EF("Config", "Device '%s' ha %d aree ma ne servono %d",
                   d.name.c_str(), d.areas.size(), expected);
            ok = false;
        }
    }
    return ok;
}

// ----------------------------------------------------------------------------
// DUPLICATE DEVICE AREAS
// ----------------------------------------------------------------------------
bool DomoConfigValidator::validateDuplicateDeviceAreas(const DomoManagerConfig::Devices& cfg) {
    bool ok = true;
    std::unordered_map<int, String> owner;

    for (auto& d : cfg.list) {
        for (int a : d.areas) {
            if (owner.count(a)) {
                LOG_EF("Config", "Area %d usata da '%s' e '%s'",
                       a, owner[a].c_str(), d.name.c_str());
                ok = false;
            }
            owner[a] = d.name;
        }
    }
    return ok;
}

// ----------------------------------------------------------------------------
// ROUTES
// ----------------------------------------------------------------------------
bool DomoConfigValidator::validateRoutes(
    const DomoManagerRouteEngine::RoutesConfig& cfg,
    DomoManager& dm)
{
    bool ok = true;
    const auto& fullCfg = dm.getConfig();   // per usare areas/devices

    std::unordered_set<int> logged;

    for (auto& r : cfg.list) {

        // --- TRIGGER AREA ---
        if (!isAreaKnownInConfig(r.triggerArea, fullCfg)) {
            if (!logged.count(r.triggerArea)) {
                LOG_EF("Config::validateRoutes",
                       "Route '%s': triggerArea %d non definita in config",
                       r.name.c_str(),
                       r.triggerArea);
                logged.insert(r.triggerArea);
            }
            ok = false;
        }

        // --- CASES / ACTIONS ---
        for (auto& c : r.cases) {
            for (auto& act : c.actions) {
                int area = act.targetArea;

                if (!isAreaKnownInConfig(area, fullCfg)) {
                    if (!logged.count(area)) {
                        LOG_EF("Config::validateRoutes",
                               "Route '%s': targetArea %d non definita in config",
                               r.name.c_str(),
                               area);
                        logged.insert(area);
                    }
                    ok = false;
                }
            }
        }
    }

    return ok;
}


// ----------------------------------------------------------------------------
// SPLITS
// ----------------------------------------------------------------------------
bool DomoConfigValidator::validateSplits(
    const DomoManagerSplitEngineEx::SplitsConfig& cfg,
    DomoManager& dm)
{
    bool ok = true;
    const auto& fullCfg = dm.getConfig();

    // 🔥 Set per evitare ripetizioni
    static std::unordered_set<int> loggedMain;
    static std::unordered_set<int> loggedOut;

    for (auto& s : cfg.list) {

        // --- MAIN AREA ---
        if (!isAreaKnownInConfig(s.mainArea, fullCfg)) {
            if (!loggedMain.count(s.mainArea)) {
                LOG_EF("Config::validateSplits",
                       "Split mainArea %d non definita in config",
                       s.mainArea);
                loggedMain.insert(s.mainArea);
            }
            ok = false;
        }

        // --- OUT AREAS ---
        for (int a : s.outAreas) {
            if (!isAreaKnownInConfig(a, fullCfg)) {
                if (!loggedOut.count(a)) {
                    LOG_EF("Config::validateSplits",
                           "Split outArea %d non definita in config",
                           a);
                    loggedOut.insert(a);
                }
                ok = false;
            }
        }
    }

    return ok;
}

// ----------------------------------------------------------------------------
// TOGGLES
// ----------------------------------------------------------------------------
bool DomoConfigValidator::validateToggles(
    const DomoManagerToggleEngineEx::TogglesConfig& cfg,
    DomoManager& dm)
{
    bool ok = true;
    const auto& fullCfg = dm.getConfig();

    for (auto& t : cfg.list) {

        if (!isAreaKnownInConfig(t.areaRead, fullCfg)) {
            LOG_EF("Config::validateToggles",
                   "Toggle areaRead %d non definita in config",
                   t.areaRead);
            ok = false;
        }

        for (int a : t.forwards) {
            if (!isAreaKnownInConfig(a, fullCfg)) {
                LOG_EF("Config::validateToggles",
                       "Toggle forwardArea %d non definita in config",
                       a);
                ok = false;
            }
        }
    }

    return ok;
}



#endif
