#ifndef DMRuntime_HPP
#define DMRuntime_HPP

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

#include "DMModbus.hpp"
#include "DMNetwork.hpp"

#include "DMAutomationBuilder.hpp"
#include "DMHStandby.hpp"

#include "DMDeclares.h"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


// Runtime/orchestration layer of DM.
// It intentionally does not own the hardware/configuration objects; it keeps
// references to them and groups event processing, automation, scheduling,
// watchdog and Modbus-cycle execution in one place.

class DomoManager;
struct DiagnosticConfig;

void mySplitCallback(const SplitOutManager::Split& s, bool isStart);

class Logic {
private:
    ToggleManager toggles;
    SplitOutManager splits;
    AnalogThresholdManager thresholds;
    RouteManager routes;
    ModbusManager mdb;

public:
    explicit Logic(SplitOutManager::CallbackFn cb)
        : toggles(), splits(cb), thresholds(), routes(), mdb() {}

    ToggleManager& getToggles() { return toggles; }
    SplitOutManager& getSplits() { return splits; }
    RouteManager& getRoutes() { return routes; }
    AnalogThresholdManager& getThresholds() { return thresholds; }
    ModbusManager& getModbus() { return mdb; }

    void addToggle(int areaRead, std::vector<int> forwards = {}) {
        toggles.add(areaRead, forwards);
    }

    void addThreshold(int area, int low, int high = -1) {
        thresholds.add(area, low, high);
    }

    void addSplit(int mainArea, std::vector<int> outAreas = {}, unsigned long maxTime = -1) {
        splits.add(mainArea, outAreas, maxTime);
    }
};

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
            return false;
        for (const auto& w : watched)
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

class DMRuntime {
public:
    using ActivityLoopFn = void (*)(DomoManager&, unsigned long);
    using SomethingChangedFn = void (*)();

    struct ModbusCycleResult {
        ModbusManager::ClientState state;
        bool executed;
    };

private:
    DomoManager& manager;
    TimeManager& timeManager;
    DeviceManager& deviceManager;
    NetworkManager& net;
    NetworkManager::ProtocolId& networkProtocolId;
    IpManager& ipManager;
    Buffer& buffer;
    LedController& leds;
    DeviceProfiles& profiles;
    DomoManagerConfig& config;

    const int areaErrors;
    const int areaRunningT;

    EventManager eventManager;

    class InternalEventPublisher {
    private:
        Buffer& buffer;
        EventManager& events;
        uint8_t source = 255;

        static void InternalCallback(const EventManager::Event& event) {
            (void)event;
        }

    public:
        InternalEventPublisher(Buffer& b, EventManager& e) : buffer(b), events(e) {}

        void attach() {
            source = events.add(InternalCallback, "INTERNAL");
            if (source == 255)
                LOG_EF("DM", "Impossibile registrare INTERNAL Event source");
        }

        void force(int area, long value, unsigned long now) {
            if (source == 255)
                return;
            buffer.WriteElement(area, value, now);
            events.push(area, value, source);
        }
    };

    InternalEventPublisher internalEvents;
    Logic logic;

    SomethingChangedFn somethingChanged = nullptr;
    ActivityLoopFn activityLoop = nullptr;

    Watchdog::CallbackTimings timings;
    Watchdog watchdog;
    bool watchdogEnabled = false;

    std::vector<uint16_t> mbReadBuffer;
    DiagnosticWatchAreas watchDiag;

    AutomationBuilder::AutomationConfig automationConfig;
    AutomationEngine automation;
    AsyncScheduler scheduler;
    AverageCalculator averages;

    using LoopTaskFn = void(*)(DMRuntime*);
    struct LoopTask {
        LoopTaskFn fn;
        uint8_t weight;
        uint8_t counter;
    };

    static LoopTask loopTasks[];
    static const uint8_t loopTaskCount;
    uint8_t loopTaskIndex = 0;

    bool ipCycleCompleted = false;
    bool activityPrepareReady = false;
    bool backendCycleDone = false;
    bool backend1StCycleDone = false;

    static DMRuntime* instance;
    static int modbusRtuSource;
    static int toggleSource;
    static int splitSource;
    static int routeSource;
    static int automationSource;

    template<typename Fn>
    unsigned long Measure(Fn f) {
        const unsigned long start = timeManager.nowMs();
        f();
        return timeManager.nowMs() - start;
    }

    void UpdateTiming(Watchdog::ExecTiming& t, unsigned long exec, int thresholdFactor_fp) {
        constexpr int SPIKE_READ_INTERVAL = 5000;
        t.last = exec;

        if (t.avg_fp == 0) {
            t.avg_fp = static_cast<int64_t>(exec) << 8;
            return;
        }

        constexpr int64_t ALPHA_FP = 26;
        constexpr int64_t ONE_MINUS_ALPHA_FP = 230;
        const int64_t exec_fp = static_cast<int64_t>(exec) << 8;

        t.avg_fp = ((static_cast<int64_t>(t.avg_fp) * ONE_MINUS_ALPHA_FP) +
                    (exec_fp * ALPHA_FP)) >> 8;

        const int64_t threshold_fp =
            (static_cast<int64_t>(t.avg_fp) * thresholdFactor_fp) >> 8;

        const unsigned long avg_ms = static_cast<unsigned long>(t.avg_fp >> 8);
        unsigned long thr_ms = static_cast<unsigned long>(threshold_fp >> 8);
        if (thr_ms < 2)
            thr_ms = 2;

        const bool wasSpike = t.spike;
        t.spike = exec > thr_ms;

        if (!t.spike)
            return;

        const unsigned long now = millis();
        if (exec > t.maxSpike)
            t.maxSpike = exec;
        t.spikeCount++;
        t.lastSpikeTime = now;

        static unsigned long lastSpikeLog = 0;
        if (!wasSpike || (now - lastSpikeLog > SPIKE_READ_INTERVAL)) {
            lastSpikeLog = now;
            LOG_WF("DM", "[SPIKE] %s exec=%lu avg=%lu thr=%lu",
                   t.name, exec, avg_ms, thr_ms);
        }
    }

    static void SomethingChangedWrapper() {
        if (!instance)
            return;

        auto& rt = *instance;
        auto& changed = rt.buffer.getChangedMap();

        if (!changed.empty()) {
            for (const auto& area : changed) {
                if (!rt.watchDiag.isWatched(area))
                    continue;

                BufferSourceInfo info;
                rt.buffer.GetData(area, info);
                rt.watchDiag.onValueChanged(rt.manager, area, info.value);
                break;
            }
        }

        if (rt.somethingChanged) {
            const unsigned long exec = rt.Measure([&]() {
                rt.somethingChanged();
            });
            rt.UpdateTiming(rt.timings.somethingChanged, exec,
                            rt.config.watchdog.spikeThresholdFactor_fp);
        }
    }

    static void Task_Scheduler(DMRuntime* rt) {
        rt->scheduler.run(rt->timeManager.nowMs());
    }

    static void Task_Splits(DMRuntime* rt) {
        if (rt->logic.getSplits().hasActiveSplits())
            rt->logic.getSplits().update();
    }

    static void Task_Automation(DMRuntime* rt) {
        if (!rt->ipCycleCompleted)
            return;

        const unsigned long now = rt->timeManager.nowMs();
        const auto actions = rt->automation.update(now);

        for (const auto& action : actions) {
            rt->forceEvent(action.targetArea, action.value,
                           static_cast<uint8_t>(automationSource));
        }
    }

    static void Task_WatchdogAndRunningT(DMRuntime* rt) {
        constexpr unsigned long CHECK_INTERVAL = 2000;
        static unsigned long lastWatchdogCheck = 0;
        static unsigned long lastUpdateCycle = 0;

        const unsigned long now = rt->timeManager.nowMs();
        if (now - lastWatchdogCheck < CHECK_INTERVAL)
            return;

        if (rt->watchdogEnabled && rt->getPowerOnCycleCompleted())
            rt->watchdog.check(now);

        lastWatchdogCheck = now;

        const unsigned long updateCycle = rt->timings.updateCycle.last;
        if (updateCycle != lastUpdateCycle) {
            lastUpdateCycle = updateCycle;
            rt->forceInternalEvent(rt->areaRunningT, updateCycle);
        }
    }

    static void Task_RestartIP(DMRuntime* rt) {
        constexpr unsigned long CHECK_INTERVAL = 5000;
        static unsigned long lastCheck = 0;
        const unsigned long now = rt->timeManager.nowMs();

        if (now - lastCheck < CHECK_INTERVAL)
            return;
        lastCheck = now;

        auto& ips = rt->ipManager.GetIps();
        bool restartIP = true;
        for (size_t i = 0; i < ips.size(); ++i) {
            const bool ipInError =
                ips[i].state == IpManager::IpState::COOLDOWN ||
                ips[i].state == IpManager::IpState::EXCLUDED;
            if (!(ipInError && ips[i].Errors > 5)) {
                restartIP = false;
                break;
            }
        }

        if (restartIP) {
            LOG_EF("DM", "Tutti gli IP in errore → System Reset");
            NVIC_SystemReset();
        }
    }

    static void Task_ActivityLoop(DMRuntime* rt) {
        static unsigned long lastRun = 0;
        const unsigned long now = rt->timeManager.nowMs();

        if (!rt->activityPrepareReady || !rt->activityLoop)
            return;

        const auto& watchdogCfg = rt->config.watchdog;
        const auto interval = watchdogCfg.rate_limit_min_interval;
        rt->watchdog.setActivityLoopRateLimit(interval);

        if (now - lastRun < interval)
            return;

        lastRun = now;
        const unsigned long exec = rt->Measure([&]() {
            rt->activityLoop(rt->manager, now);
        });

        rt->UpdateTiming(rt->timings.activityLoop, exec,
                         watchdogCfg.spikeThresholdFactor_fp);
        rt->activityPrepareReady = false;
    }

    ModbusCycleResult RunModbusCycle(ModbusTCPClient& modbusTCPClient,
                                     unsigned long now) {
        auto& ips = ipManager.GetIps();
        if (ips.empty())
            return {ModbusManager::ClientState::ERROR, false};

        static size_t ipIdx = 0;

        int ipToWrite = -1;
        for (size_t i = 0; i < ips.size(); ++i) {
            if (logic.getModbus().hasPendingWritesForIp(i)) {
                ipToWrite = static_cast<int>(i);
                break;
            }
        }

        if (ipToWrite >= 0) {
            if (!ipManager.ShouldQuery(ipToWrite, now))
                return {ModbusManager::ClientState::READ_DONE, false};

            if (!net.tryAcquire(networkProtocolId, now))
                return {ModbusManager::ClientState::READ_DONE, false};

            const auto state = logic.getModbus().RunClient(
                modbusTCPClient, ipToWrite, 502, mbReadBuffer, now);

            net.updateProtocolState(networkProtocolId,
                                    logic.getModbus().mapClientState(state));

            switch (state) {
                case ModbusManager::ClientState::CYCLE_OK:
                    ipManager.ReportSuccess(ipToWrite);
                    break;
                case ModbusManager::ClientState::ERROR:
                    ipManager.ReportError(ipToWrite, now);
                    break;
                default:
                    break;
            }

            net.release(networkProtocolId, now);
            return {state, true};
        }

        auto& ip = ips[ipIdx];
        (void)ip;
        if (!ipManager.ShouldQuery(ipIdx, now)) {
            ipIdx = (ipIdx + 1) % ips.size();
            return {ModbusManager::ClientState::READ_DONE, false};
        }

        if (!net.tryAcquire(networkProtocolId, now))
            return {ModbusManager::ClientState::READ_DONE, false};

        const auto state = logic.getModbus().RunClient(
            modbusTCPClient, static_cast<int>(ipIdx), 502, mbReadBuffer, now);

        net.updateProtocolState(networkProtocolId,
                                logic.getModbus().mapClientState(state));

        switch (state) {
            case ModbusManager::ClientState::CYCLE_OK:
                ipManager.ReportSuccess(ipIdx);
                break;
            case ModbusManager::ClientState::ERROR:
                ipManager.ReportError(ipIdx, now);
                break;
            default:
                break;
        }

        if (state == ModbusManager::ClientState::CYCLE_OK ||
            state == ModbusManager::ClientState::ERROR ||
            state == ModbusManager::ClientState::DEVICE_ERROR) {
            ipIdx = (ipIdx + 1) % ips.size();
            if (ipIdx == 0) {
                activityPrepareReady = true;
                ipCycleCompleted = true;
                buffer.tick(now);
            }
        }

        net.release(networkProtocolId, now);
        return {state, true};
    }

    static bool ReadAreaPolicy(int area, long value, Buffer& b) {
        (void)value;
        (void)b;
        if (!instance)
            return false;

        auto& toggles = instance->logic.getToggles();
        auto& splits = instance->logic.getSplits();

        if (toggles.handlesArea(area))
            return true;
        if (splits.exist(area) && !splits.isSplitTarget(area))
            return true;
        return false;
    }

    static void OnModbusFieldChanged(int area, long value) {
        if (!instance)
            return;
        instance->eventManager.push(area, value,
                                   static_cast<uint8_t>(modbusRtuSource));
    }

    static void ModbusRtuEventCallback(const EventManager::Event& event) {
        if (!instance)
            return;
        const unsigned long now = instance->timeManager.nowMs();
        LOG_DF("ModbusRtuEvent",
               "HMI EVENT -> RTU: id=%lu area=%d value=%ld source=%u",
               (unsigned long)event.id, event.area, event.value, event.source);
        instance->buffer.WriteElement(event.area, event.value, now);
    }

    static void ToggleEventCallback(const EventManager::Event& event) {
        if (!instance)
            return;

        ToggleManager::ToggleResult result;
        if (!instance->logic.getToggles().processEvent(
                event.area, event.value, instance->buffer,
                instance->timeManager.nowMs(), result))
            return;

        instance->forceInternalEvent(result.area, result.value);
    }

    static void SplitEventCallback(const EventManager::Event& event) {
        if (!instance)
            return;

        auto& splits = instance->logic.getSplits();
        if (!splits.exist(event.area) || splits.isSplitTarget(event.area))
            return;

        if (event.value) {
            splits.start(event.area);
        } else if (splits.IsRunning(event.area)) {
            splits.reset(event.area);
        }

        instance->buffer.ResetElement(event.area);
    }

    static void RouteEventCallback(const EventManager::Event& event) {
        if (!instance)
            return;

        auto& routes = instance->logic.getRoutes();
        if (!routes.hasRoute(event.area))
            return;

        const auto actions = routes.execute(event.area, event.value, instance->buffer);
        for (const auto& action : actions) {
            instance->forceEvent(action.targetArea, action.value,
                                 static_cast<uint8_t>(routeSource));
        }
    }

    static void AutomationEventCallback(const EventManager::Event& event) {
        if (!instance)
            return;

        const unsigned long now = instance->timeManager.nowMs();
        const auto actions = instance->automation.onEvent(now, event.area);
        for (const auto& action : actions) {
            instance->forceEvent(action.targetArea, action.value,
                                 static_cast<uint8_t>(automationSource));
        }
    }

    bool validateAutomationConfig() {
        auto& buf = buffer;
        bool ok = true;
        const auto& autoCfg = automationConfig;

        if (autoCfg.scenes.empty() && autoCfg.rules.empty() && autoCfg.sequences.empty()) {
            LOG_IF("Automation", "Nessuna automazione configurata → skip validazione");
            return true;
        }

        std::vector<String> sceneNames;
        sceneNames.reserve(autoCfg.scenes.size());
        for (const auto& s : autoCfg.scenes)
            sceneNames.push_back(s.name);

        auto sceneExists = [&](const String& name) -> bool {
            for (const auto& n : sceneNames)
                if (n == name)
                    return true;
            return false;
        };

        for (const auto& s : autoCfg.scenes) {
            if (s.actions.empty())
                LOG_WF("Automation", "Scene '%s' non contiene azioni", s.name.c_str());
            for (const auto& a : s.actions) {
                if (a.area < 0 || a.area >= buf.size()) {
                    LOG_EF("Automation", "Scene '%s' usa area %d fuori range", s.name.c_str(), a.area);
                    ok = false;
                }
                if (DeviceManager::IsReservedArea(a.area)) {
                    LOG_EF("Automation", "Scene '%s' usa area riservata %d", s.name.c_str(), a.area);
                    ok = false;
                }
                if (a.area >= 0 && a.area < buf.size() && buf.IsVirtual(a.area))
                    LOG_WF("Automation", "Scene '%s' scrive su area virtuale %d", s.name.c_str(), a.area);
            }
        }

        for (const auto& r : autoCfg.rules) {
            if (!r.sceneTrue.isEmpty() &&
                !AutomationEngine::BuiltinScenes::isBuiltin(r.sceneTrue.c_str()) &&
                !sceneExists(r.sceneTrue)) {
                LOG_EF("Automation", "Rule '%s' sceneTrue '%s' non esiste", r.name.c_str(), r.sceneTrue.c_str());
                ok = false;
            }
            if (!r.sceneFalse.isEmpty() &&
                !AutomationEngine::BuiltinScenes::isBuiltin(r.sceneFalse.c_str()) &&
                !sceneExists(r.sceneFalse)) {
                LOG_EF("Automation", "Rule '%s' sceneFalse '%s' non esiste", r.name.c_str(), r.sceneFalse.c_str());
                ok = false;
            }

            if (r.type == "trend" && (r.trend.area < 0 || r.trend.area >= buf.size())) {
                LOG_EF("Automation", "Rule '%s' trend area %d fuori range", r.name.c_str(), r.trend.area);
                ok = false;
            }
            if (r.type == "threshold" && (r.threshold.area < 0 || r.threshold.area >= buf.size())) {
                LOG_EF("Automation", "Rule '%s' threshold area %d fuori range", r.name.c_str(), r.threshold.area);
                ok = false;
            }
            if (r.type == "bitmask") {
                if (r.bitmask.area < 0 || r.bitmask.area >= buf.size()) {
                    LOG_EF("Automation", "Rule '%s' bitmask area %d fuori range", r.name.c_str(), r.bitmask.area);
                    ok = false;
                }
                if (r.bitmask.bitIndex < 0 || r.bitmask.bitIndex > 31) {
                    LOG_EF("Automation", "Rule '%s' bitmask bitIndex %d fuori range", r.name.c_str(), r.bitmask.bitIndex);
                    ok = false;
                }
            }
            if (r.type == "multi") {
                for (const auto& c : r.multi.conditions) {
                    if (c.area < 0 || c.area >= buf.size()) {
                        LOG_EF("Automation", "Rule '%s' multi condition area %d fuori range", r.name.c_str(), c.area);
                        ok = false;
                    }
                }
            }
            if (r.type == "composite") {
                for (const auto& in : r.composite.inputs) {
                    if (in.area < 0 || in.area >= buf.size()) {
                        LOG_EF("Automation", "Rule '%s' composite input area %d fuori range", r.name.c_str(), in.area);
                        ok = false;
                    }
                    if (in.type == "bitmask" && (in.bitIndex < 0 || in.bitIndex > 31)) {
                        LOG_EF("Automation", "Rule '%s' composite bitIndex %d fuori range", r.name.c_str(), in.bitIndex);
                        ok = false;
                    }
                }
                if (r.composite.output.area < 0 || r.composite.output.area >= buf.size()) {
                    LOG_EF("Automation", "Rule '%s' composite output area %d fuori range", r.name.c_str(), r.composite.output.area);
                    ok = false;
                }
                if (r.composite.output.bitIndex < 0 || r.composite.output.bitIndex > 31) {
                    LOG_EF("Automation", "Rule '%s' composite output bitIndex %d fuori range", r.name.c_str(), r.composite.output.bitIndex);
                    ok = false;
                }
            }
        }

        for (const auto& seq : autoCfg.sequences) {
            for (const auto& st : seq.steps) {
                if (st.area < 0 || st.area >= buf.size()) {
                    LOG_EF("Automation", "Sequence step usa area %d fuori range", st.area);
                    ok = false;
                } else if (buf.IsVirtual(st.area)) {
                    LOG_WF("Automation", "Sequence step scrive su area virtuale %d", st.area);
                }
            }
        }
        return ok;
    }

public:
    DMRuntime(
        DomoManager& managerRef,
        TimeManager& tm,
        DeviceManager& dm,
        NetworkManager& network,
        NetworkManager::ProtocolId& protocolId,
        IpManager& ipm,
        Buffer& buf,
        LedController& led,
        DeviceProfiles& prof,
        DomoManagerConfig& cfg)
        : manager(managerRef),
          timeManager(tm),
          deviceManager(dm),
          net(network),
          networkProtocolId(protocolId),
          ipManager(ipm),
          buffer(buf),
          leds(led),
          profiles(prof),
          config(cfg),
          areaErrors(DeviceManager::AREA_SYSTEM_ERRORS),
          areaRunningT(DeviceManager::AREA_SYSTEM_RUNNING_T),
          eventManager(),
          internalEvents(buffer, eventManager),
          logic(mySplitCallback),
          timings(),
          watchdog(&timings, &config.watchdog),
          watchdogEnabled(false),
          mbReadBuffer(),
          watchDiag(),
          automationConfig(),
          automation(),
          scheduler(),
          averages(),
          loopTaskIndex(0),
          ipCycleCompleted(false),
          activityPrepareReady(false),
          backendCycleDone(false),
          backend1StCycleDone(false) {
        timings.somethingChanged.name = "somethingChanged";
        timings.route.name = "route";
        timings.activityLoop.name = "activityLoop";
        timings.updateCycle.name = "updateCycle";
        instance = this;
    }

    void setCallbacks(SomethingChangedFn changed, ActivityLoopFn activity) {
        somethingChanged = changed;
        activityLoop = activity;
    }

    bool begin() {
        instance = this;
        mbReadBuffer.resize(deviceManager.GetMaxReadSize());

        modbusRtuSource = eventManager.add(ModbusRtuEventCallback, "MODBUS_RTU");
        toggleSource = eventManager.add(ToggleEventCallback, "TOGGLE");
        splitSource = eventManager.add(SplitEventCallback, "SPLIT");
        routeSource = eventManager.add(RouteEventCallback, "ROUTE");
        automationSource = eventManager.add(AutomationEventCallback, "AUTOMATION");
        internalEvents.attach();

        if (modbusRtuSource == 255 || toggleSource == 255 || splitSource == 255 ||
            routeSource == 255 || automationSource == 255)
            return false;

        logic.getModbus().Begin(
            leds, buffer, deviceManager.getDevices(), ipManager,
            logic.getThresholds(), &SomethingChangedWrapper, net,
            networkProtocolId);
        logic.getModbus().setReadAreaPolicy(&ReadAreaPolicy);
        logic.getModbus().setFieldChangedCallback(OnModbusFieldChanged);

        automation.attachScheduler(&scheduler, &buffer, &averages, &timeManager);

        return true;
    }

    void loop(ModbusTCPClient& client) {
        const unsigned long now = timeManager.nowMs();
        if (networkProtocolId >= 0) {
            Update(client, now);
        } else {
            activityPrepareReady = true;
            ipCycleCompleted = true;
        }

        if (!ipCycleCompleted)
            return;

        LoopTask& t = loopTasks[loopTaskIndex];
        if (t.counter == 0) {
            t.fn(this);
            t.counter = t.weight;
        } else {
            --t.counter;
        }

        ++loopTaskIndex;
        if (loopTaskIndex >= loopTaskCount) {
            loopTaskIndex = 0;
            backendCycleDone = true;
            backend1StCycleDone = true;
        }
    }

    void Update(ModbusTCPClient& client, unsigned long now) {
        const unsigned long start_us = micros();
        const auto state = RunModbusCycle(client, now);
        leds.update(now);

        if (state.executed) {
            const unsigned long exec_us = micros() - start_us;
            const unsigned long exec_ms = (exec_us + 999UL) / 1000UL;
            UpdateTiming(timings.updateCycle, exec_ms,
                         config.watchdog.spikeThresholdFactor_fp);
        }
    }

    void forceInternalEvent(int area, long value) {
        internalEvents.force(area, value, timeManager.nowMs());
    }

    void forceEvent(int area, long value, uint8_t source) {
        buffer.WriteElement(area, value, timeManager.nowMs());
        eventManager.push(area, value, source);
    }

    void checkWatchdog() {
        if (watchdogEnabled)
            watchdog.check(timeManager.nowMs());
    }

    void enableWatchdog() { watchdogEnabled = true; }
    void SetWatchdogCallback(Watchdog::WatchdogFn fn) { watchdog.setCallback(fn); }

    void loadAutomationJson(const char* json) {
        if (!AutomationBuilder::parseJson(json, automationConfig)) {
            LOG_EF("Automation", "Errore parsing automazioni!");
            return;
        }

        validateAutomationConfig();

        AutomationBuilder builder;
        builder.build(automation, automationConfig);
    }

    const AutomationBuilder::AutomationConfig& getAutomationBuilderConfig() const {
        return automationConfig;
    }

    DiagnosticWatchAreas& getWatchDiag() { return watchDiag; }
    AutomationEngine& getAutomation() { return automation; }
    AsyncScheduler& getScheduler() { return scheduler; }
    Buffer& getBuffer() { return buffer; }
    AverageCalculator& getAverages() { return averages; }
    TimeManager& getTimeManager() { return timeManager; }
    DeviceProfiles& getProfiles() { return profiles; }
    const DomoManagerConfig& getConfig() const { return config; }
    DomoManagerConfig& getConfig() { return config; }
    Logic& getLogic() { return logic; }
    EventManager& getEventManager() { return eventManager; }
    const Watchdog::CallbackTimings& getTimings() const { return timings; }
    Watchdog::CallbackTimings& getTimings() { return timings; }

    bool hasBackendCycleCompleted() const { return backendCycleDone; }
    void resetBackendCycleFlag() { backendCycleDone = false; }
    bool getPowerOnCycleCompleted() const { return ipCycleCompleted && backend1StCycleDone; }
    uint8_t getSplitSource() const { return static_cast<uint8_t>(splitSource); }

    static int getModbusRtuSource() { return modbusRtuSource; }
    static int getToggleSource() { return toggleSource; }
    static int getRouteSource() { return routeSource; }
    static int getAutomationSource() { return automationSource; }

    friend class DiagnosticWatchAreas;
};

inline DMRuntime::LoopTask DMRuntime::loopTasks[] = {
    { DMRuntime::Task_Splits, 10, 0 },
    { DMRuntime::Task_Automation, 25, 0 },
    { DMRuntime::Task_ActivityLoop, 5, 0 },
    { DMRuntime::Task_Scheduler, 55, 0 },
    { DMRuntime::Task_WatchdogAndRunningT, 15, 0 },
    { DMRuntime::Task_RestartIP, 120, 0 }
};

inline const uint8_t DMRuntime::loopTaskCount =
    sizeof(DMRuntime::loopTasks) / sizeof(DMRuntime::loopTasks[0]);

inline DMRuntime* DMRuntime::instance = nullptr;
inline int DMRuntime::modbusRtuSource = 255;
inline int DMRuntime::toggleSource = 255;
inline int DMRuntime::splitSource = 255;
inline int DMRuntime::routeSource = 255;
inline int DMRuntime::automationSource = 255;




#endif