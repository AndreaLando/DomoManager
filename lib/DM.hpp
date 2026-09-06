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


#include "DMRuntime.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


class DomoManager;
struct DiagnosticConfig;

class DMBufferEngineEx : public DomoManagerBufferEngine {
public:
    static void apply(DomoManager& manager, const AreasConfig& cfg);
};

class DMToggleEngineEx : public DomoManagerToggleEngine {
public:
    static void apply(DomoManager& manager, const TogglesConfig& cfg);
};

class DMSplitEngineEx : public DomoManagerSplitEngine {
public:
    static void apply(DomoManager& manager, const SplitsConfig& cfg);
};

class DomoConfigValidator {
private:
    static int CalculateMaxAreasFromConfig(const DomoManagerConfig& cfg) {
        int maxArea = 0;
        maxArea = (AreaRegistry::maxValue() > maxArea) ? AreaRegistry::maxValue() : maxArea;

        for (const auto& d : cfg.devices.list)
            for (int a : d.areas)
                if (a > maxArea) maxArea = a;

        for (const auto& a : cfg.areas.list) {
            if (a.area > maxArea) maxArea = a.area;
            if (a.forwardArea > maxArea) maxArea = a.forwardArea;
        }

        for (const auto& t : cfg.toggles.list) {
            if (t.areaRead > maxArea) maxArea = t.areaRead;
            for (int f : t.forwards)
                if (f > maxArea) maxArea = f;
        }

        for (const auto& s : cfg.splits.list) {
            if (s.mainArea > maxArea) maxArea = s.mainArea;
            for (int o : s.outAreas)
                if (o > maxArea) maxArea = o;
        }

        return maxArea + 1;
    }

    static bool isAreaKnownInConfig(int area, const DomoManagerConfig& cfg) {
        for (const auto& a : cfg.areas.list)
            if (a.area == area) return true;
        for (const auto& d : cfg.devices.list)
            for (int da : d.areas)
                if (da == area) return true;
        for (int i = 0; i < AreaRegistry::count(); ++i)
            if (AreaRegistry::getValueByIndex(i) == area) return true;
        return false;
    }

    static bool validateAreas(const DomoManagerConfig& cfg, DomoManager& DomoManager);

public:
    enum class StaticValidationResult { OK, WARNING, ERROR };

    static bool validateStatic(const DomoManagerConfig& cfg, DomoManager& DomoManager);
    static bool validateDynamic(const DomoManagerConfig& cfg, DomoManager& DomoManager);
    static bool validate(const DomoManagerConfig& cfg, DomoManager& DomoManager);
    static bool validateDefineAreas();
    static bool validateDeviceAreas(const DomoManagerConfig::Devices& cfg, DomoManager& DomoManager);
    static bool validateDuplicateDeviceAreas(const DomoManagerConfig::Devices& cfg);
    static bool validateRoutes(const DomoManagerRouteEngine::RoutesConfig& cfg, DomoManager& DomoManager);
    static bool validateSplits(const DMSplitEngineEx::SplitsConfig& cfg, DomoManager& DomoManager);
    static bool validateToggles(const DMToggleEngineEx::TogglesConfig& cfg, DomoManager& DomoManager);
    static int getRequiredAreas(const DomoManagerConfig& cfg) { return CalculateMaxAreasFromConfig(cfg); }
};

class DomoManager {
private:
    TimeManager timeManager;
    DeviceManager deviceManager;
    NetworkManager::ProtocolId networkProtocolId = -1;

#if HOTSTANDBY_ENABLED
    bool isClusterMasterFlag = false;
    HotStandbyManager hotStandby{false, &timeManager};
#endif

    DomoManagerConfig config;
    OwnerManager owner;
    IpManager ipManager;
    Buffer buffer;
    LedController m_leds;
    DeviceProfiles profiles;
    DMRuntime runtime;

    void initBuffer() {
        DefineBufferElement(deviceManager.AREA_SYSTEM_ERRORS, 0, false, "Devices in error");
        DefineBufferElement(deviceManager.AREA_SYSTEM_RUNNING_T, 0, false, "Current cycle");

        const auto multipleInit = buffer.getInitializedMultipleTimes();
        for (int area : multipleInit)
            LOG_EF("DomoManager", "ERRORE: Area initialized more times: %d", area);

        std::unordered_set<int> mappedAreas;
        for (auto& dev : deviceManager.getDevices()) {
            const int channels = dev.GetChannelsSize();
            for (int ch = 0; ch < channels; ++ch) {
                const auto info = dev.GetChannelInfo(ch);
                for (int i = 0; i < info.items; ++i)
                    mappedAreas.insert(dev.GetArea(ch, i));
            }
        }

        std::vector<int> virtualAreas;
        for (int area = 0; area < buffer.size(); ++area) {
            if (!mappedAreas.count(area)) {
                buffer.SetVirtual(area, true);
                virtualAreas.push_back(area);
            }
        }

        if (!virtualAreas.empty()) {
            String list;
            for (int a : virtualAreas)
                list += String(a) + " ";
            LOG_IF("InitBuffer", "Auto-detected virtual areas: %s", list.c_str());
        }
    }

    void mapDevices(const DomoManagerConfig::Devices& cfg) {
        for (const auto& d : cfg.list) {
            auto* profile = profiles.get(d.profile.c_str());
            if (!profile) {
                LOG_EF("Profilo non trovato: %s", d.profile.c_str());
                continue;
            }

            deviceManager.addDeviceManual(
                d.name.c_str(), d.address, d.slot, *profile,
                d.areas, d.retry, d.priority);

            auto& dev = deviceManager.getDevices().back();
            dev.GetError().SetStateChangedCallback([this, &dev]() {
                OnDeviceErrorStateChanged(&dev);
            });
        }
    }

    void OnDeviceErrorStateChanged(GenericPrgDevice* dev) {
        (void)dev;
        int totalErrors = 0;
        for (auto& d : deviceManager.getDevices())
            if (d.GetError().IsVisibleError() || d.GetError().IsVisibleParked())
                ++totalErrors;

        m_leds.set(LedController::FOUR, totalErrors > 0);
        runtime.forceInternalEvent(DeviceManager::AREA_SYSTEM_ERRORS, totalErrors);
        LOG_IF("DomoManager", "Device error state changed → totalErrors=%d", totalErrors);
    }

public:
    using ActivityLoopFn = DMRuntime::ActivityLoopFn;
    using SomethingChangedFn = DMRuntime::SomethingChangedFn;
    static DomoManager* instance;

    NetworkManager net;

    explicit DomoManager(const LedController::LedPins& ledPins)
        : timeManager(),
          deviceManager(),
          networkProtocolId(-1),
          config(),
          owner(),
          ipManager(),
          buffer(1),
          m_leds(ledPins, false),
          profiles(),
          runtime(*this, timeManager, deviceManager, net, networkProtocolId,
                  ipManager, buffer, m_leds, profiles, config) {
        instance = this;
    }

    DeviceManager& devices() { return deviceManager; }
    const DeviceManager& devices() const { return deviceManager; }

    bool setup(SomethingChangedFn changed,
               ActivityLoopFn activity,
               DomoManagerConfig cfg,
               NetworkManager::ProtocolId protocolId) {
        networkProtocolId = protocolId;
        config = cfg;
        runtime.setCallbacks(changed, activity);
        instance = this;

        m_leds.begin();
        if (config.devices.list.empty() || config.areas.list.empty()) {
            m_leds.set(LedController::THREE, true);
            m_leds.set(LedController::TWO, true);
            LOG_EF("DomoManager", "CONFIGURAZIONE VUOTA → impossibile avviare il sistema, caricare in DmSetup.hpp una configurazione valida");
            return false;
        }

        mapDevices(config.devices);
        buffer = Buffer(DomoConfigValidator::getRequiredAreas(config));

        if (!DomoConfigValidator::validateStatic(config, *this)) {
            LOG_EF("DomoManager", "AVVIO BLOCCATO (validazione statica)");
            return false;
        }

        DMBufferEngineEx::apply(*this, config.areas);
        DMToggleEngineEx::apply(*this, config.toggles);
        DMSplitEngineEx::apply(*this, config.splits);
        DomoManagerRouteEngine::apply(runtime.getLogic().getRoutes(), config.routes);

        initBuffer();

        if (!DomoConfigValidator::validateDynamic(config, *this)) {
            LOG_EF("DomoManager", "AVVIO BLOCCATO (validazione dinamica)");
            return false;
        }

        m_leds.setAll(true);
        ipManager.BuildIps(deviceManager.getDevices());
        LOG_IF("DomoManager", "Hw items to query: %d", deviceManager.getDevices().size());

        if (!runtime.begin()) {
            LOG_EF("DomoManager", "Impossibile inizializzare il runtime/EventManager");
            return false;
        }

        delay(1000);
        m_leds.setAll(false);
        return true;
    }

    void Update(ModbusTCPClient& client, unsigned long now) { runtime.Update(client, now); }

    void loop(ModbusTCPClient& client) {
#if HOTSTANDBY_ENABLED
        hotStandby.poll();
        if (!isClusterMasterFlag) {
            runtime.checkWatchdog();
            return;
        }
#endif
        runtime.loop(client);
    }

    void forceInternalEvent(int area, long value) { runtime.forceInternalEvent(area, value); }
    void forceEvent(int area, long value, uint8_t source) { runtime.forceEvent(area, value, source); }

    void loadAutomationJson(const char* json) { runtime.loadAutomationJson(json); }
    const AutomationBuilder::AutomationConfig& getAutomationBuilderConfig() const { return runtime.getAutomationBuilderConfig(); }

    void SetWatchdogCallback(Watchdog::WatchdogFn fn) { runtime.SetWatchdogCallback(fn); }
    void enableWatchdog() { runtime.enableWatchdog(); }

    DiagnosticWatchAreas& getWatchDiag() { return runtime.getWatchDiag(); }
    AutomationEngine& getAutomation() { return runtime.getAutomation(); }
    AsyncScheduler& getScheduler() { return runtime.getScheduler(); }
    Buffer& getBuffer() { return buffer; }
    AverageCalculator& getAverages() { return runtime.getAverages(); }
    TimeManager& getTimeManager() { return timeManager; }
    DeviceProfiles& getProfiles() { return profiles; }
    const DomoManagerConfig& getConfig() const { return config; }
    DomoManagerConfig& getConfig() { return config; }
    Logic& getLogic() { return runtime.getLogic(); }
    EventManager& getEventManager() { return runtime.getEventManager(); }
    const Watchdog::CallbackTimings& getTimings() const { return runtime.getTimings(); }
        LedController& getLeds() {
        return m_leds;
    }
    bool hasBackendCycleCompleted() const { return runtime.hasBackendCycleCompleted(); }
    void resetBackendCycleFlag() { runtime.resetBackendCycleFlag(); }
    uint8_t getSplitSource() const { return runtime.getSplitSource(); }
    bool getPowerOnCycleCompleted() const { return runtime.getPowerOnCycleCompleted(); }

    void DefineBufferElement(int area, int areaToWrite, bool reverse, const char* name) {
        buffer.SetElement(area, areaToWrite, reverse, name);
    }

    OwnerManager& getOwner() { return owner; }
    bool isDeveloperMode() const { return owner.getMode() == OwnerManager::DEVELOPER; }

#if HOTSTANDBY_ENABLED
    HotStandbyManager& getHotStandby() { return hotStandby; }
    bool isClusterMaster() const { return isClusterMasterFlag; }
    void enableHotStandby(bool startAsMaster) {
        hotStandby.isMaster = startAsMaster;
        isClusterMasterFlag = startAsMaster;
        hotStandby.begin(9600);
    }
    void setClusterMaster(bool m) { isClusterMasterFlag = m; }
#endif

    void DumpDevicesByIP(IpManager& ipm) {
        LOG_IF("DumpDevicesByIP", "=== DUMP devicesByIP ===");
        auto& ips = ipm.GetIps();
        for (size_t i = 0; i < ips.size(); ++i) {
            auto& ip = ips[i].IP;
            LOG_IF("DumpDevicesByIP", "IP[%d] = %d.%d.%d.%d",
                   (int)i, ip[0], ip[1], ip[2], ip[3]);
            auto* list = ipm.GetDevicesByIP(ip);
            if (!list) {
                LOG_IF("DumpDevicesByIP", "   → nessun device");
                continue;
            }
            for (int devIdx : *list)
                LOG_IF("DumpDevicesByIP", "   devIdx=%d", devIdx);
        }
        LOG_IF("DumpDevicesByIP", "=== END DUMP ===");
    }
};

inline DomoManager* DomoManager::instance = nullptr;

inline void DMBufferEngineEx::apply(DomoManager& manager, const AreasConfig& cfg) {
    auto& L = manager.getLogic();
    for (const auto& a : cfg.list) {
        const auto& f = a.flags;
        int safeForwardArea = a.forwardArea;
        if (manager.devices().IsReservedArea(safeForwardArea))
            safeForwardArea = Buffer::NO_AREA;
        manager.DefineBufferElement(a.area, safeForwardArea, f.reverse, a.label.c_str());
        if (a.threshold.low != -1)
            L.getThresholds().add(a.area, a.threshold.low, a.threshold.high);
    }
}

inline void DMToggleEngineEx::apply(DomoManager& manager, const TogglesConfig& cfg) {
    auto& L = manager.getLogic();
    for (const auto& t : cfg.list)
        L.getToggles().add(t.areaRead, t.forwards);
}

inline void DMSplitEngineEx::apply(DomoManager& manager, const SplitsConfig& cfg) {
    auto& L = manager.getLogic();
    for (const auto& s : cfg.list)
        L.getSplits().add(s.mainArea, s.outAreas, s.maxTime);
}

inline void mySplitCallback(const SplitOutManager::Split& s, bool isStart) {
    if (!DomoManager::instance)
        return;

    auto& DomoManager = DomoManager::instance->devices();
    LOG_DF("SplitCallback", "Callback START=%d mainArea=%d outCount=%u",
           isStart ? 1 : 0, s.mainArea, (unsigned)s.outAreas.size());

    for (int a : s.outAreas) {
        GenericPrgDevice* devFound = nullptr;
        int ch = -1;
        int item = -1;

        for (auto& dev : DomoManager.getDevices()) {
            if (dev.FindChannelByArea(a, ch, item)) {
                devFound = &dev;
                break;
            }
        }

        if (!devFound) {
            LOG_WF("SplitCallback", "Area %d non appartiene a nessun device", a);
            continue;
        }
        if (devFound->GetError().IsInError()) {
            LOG_WF("SplitCallback", "Skip WRITE: device %s area %d is excluded", devFound->GetName(), a);
            continue;
        }

        DomoManager::instance->forceEvent(
            a, isStart ? 1 : 0, DomoManager::instance->getSplitSource());
    }
}

inline bool DomoConfigValidator::validateStatic(const DomoManagerConfig& cfg, DomoManager& DomoManager) {
    bool ok = true;
    LOG_DF("Config", "VALIDAZIONE STATICA: avvio…");

    ok &= validateDefineAreas();
    ok &= validateDuplicateDeviceAreas(cfg.devices);
    ok &= validateDeviceAreas(cfg.devices, DomoManager);
    ok &= validateAreas(cfg, DomoManager);
    ok &= validateRoutes(cfg.routes, DomoManager);
    ok &= validateSplits(cfg.splits, DomoManager);
    ok &= validateToggles(cfg.toggles, DomoManager);
    if (!ok) return false;

    for (const auto& a : cfg.areas.list) {
        if (a.forwardArea > 0 && !isAreaKnownInConfig(a.forwardArea, cfg)) {
            LOG_EF("Config", "Area %d ha forwardArea %d non definita.", a.area, a.forwardArea);
            ok = false;
        }
        if (a.area >= 0 && a.area <= DeviceManager::AREA_LAST_RESERVED) {
            LOG_EF("Config", "Area %d è riservata e non può essere usata.", a.area);
            ok = false;
        }
        if (a.threshold.high != -1 && a.threshold.low > a.threshold.high) {
            LOG_EF("Config", "Area %d ha threshold invertiti (%d > %d).",
                   a.area, a.threshold.low, a.threshold.high);
            ok = false;
        }
    }

    std::unordered_set<int> seen;
    for (const auto& a : cfg.areas.list) {
        if (!seen.insert(a.area).second) {
            LOG_EF("Config", "Area %d definita due volte in AreasConfig.", a.area);
            ok = false;
        }
    }

    if (!ok) return false;

    const int requiredSize = CalculateMaxAreasFromConfig(cfg);
    const int bufferSize = DomoManager.getBuffer().size();
    if (requiredSize > bufferSize)
        LOG_WF("Config", "VALIDAZIONE STATICA: WARNING → la config richiede %d aree, ma il buffer iniziale ne supporta solo %d. Le aree eccedenti saranno virtual.", requiredSize, bufferSize);

    for (const auto& a : cfg.areas.list)
        if (!isAreaKnownInConfig(a.area, cfg))
            LOG_WF("Config", "Area %d non mappata da device → sarà virtuale.", a.area);

    LOG_IF("Config", "VALIDAZIONE STATICA OK.");
    return true;
}

inline bool DomoConfigValidator::validateAreas(const DomoManagerConfig& cfg, DomoManager& DomoManager) {
    const int RESERVED_MAX = DomoManager.devices().AREA_LAST_RESERVED;
    std::unordered_set<int> used;

    for (auto& dev : DomoManager.devices().getDevices()) {
        const int channels = dev.GetChannelsSize();
        for (int ch = 0; ch < channels; ++ch) {
            const auto info = dev.GetChannelInfo(ch);
            for (int i = 0; i < info.items; ++i) {
                const int area = dev.GetArea(ch, i);
                if (area <= RESERVED_MAX) {
                    LOG_EF("CONFIG", "Device '%s' usa area riservata (%d <= %d)", dev.GetName(), area, RESERVED_MAX);
                    return false;
                }
                if (!used.insert(area).second) {
                    LOG_EF("CONFIG", "Area duplicata trovata: %d (device '%s')", area, dev.GetName());
                    return false;
                }
                if (!isAreaKnownInConfig(area, cfg)) {
                    LOG_EF("CONFIG", "Area %d non definita in DEFINE_AREA, AreasConfig, Toggles o Splits", area);
                    return false;
                }
            }
        }
    }
    return true;
}

inline bool DomoConfigValidator::validateDynamic(const DomoManagerConfig& cfg, DomoManager& DomoManager) {
    bool ok = true;
    auto& buf = DomoManager.getBuffer();
    auto& devs = DomoManager.devices().getDevices();
    LOG_DF("Config", "VALIDAZIONE DINAMICA: avvio…");

    for (auto& d : devs) {
        if (d.GetError().IsInError()) {
            LOG_EF("Config", "Device '%s' in errore durante l'inizializzazione", d.GetName());
            ok = false;
        }
        const size_t channels = d.GetChannelsSize();
        for (size_t ch = 0; ch < channels; ++ch) {
            const auto info = d.GetChannelInfo(ch);
            for (int i = 0; i < info.items; ++i) {
                const int area = d.GetArea(ch, i);
                if (area < 0 || area >= buf.size()) {
                    LOG_EF("Config", "Device '%s' mappa area %d fuori range", d.GetName(), area);
                    ok = false;
                }
            }
        }
    }

    for (auto& a : cfg.areas.list) {
        if (a.area < 0 || a.area >= buf.size()) {
            LOG_EF("Config", "Area %d non esiste nel buffer", a.area);
            ok = false;
            continue;
        }
        if (buf.IsVirtual(a.area))
            LOG_WF("Config", "Area %d è virtuale (nessun device la gestisce)", a.area);
    }

    for (auto& t : cfg.toggles.list) {
        if (t.areaRead < 0 || t.areaRead >= buf.size()) {
            LOG_EF("Config", "Toggle legge area %d non esistente", t.areaRead);
            ok = false;
        }
        for (int f : t.forwards)
            if (f < 0 || f >= buf.size()) {
                LOG_EF("Config", "Toggle forward verso area %d non esistente", f);
                ok = false;
            }
    }

    for (auto& s : cfg.splits.list) {
        if (s.mainArea < 0 || s.mainArea >= buf.size()) {
            LOG_EF("Config", "Split usa mainArea %d non esistente", s.mainArea);
            ok = false;
        }
        for (int o : s.outAreas)
            if (o < 0 || o >= buf.size()) {
                LOG_EF("Config", "Split forward verso area %d non esistente", o);
                ok = false;
            }
    }

    for (auto& r : cfg.routes.list) {
        if (r.triggerArea < 0 || r.triggerArea >= buf.size()) {
            LOG_EF("Config", "Route '%s' usa triggerArea %d non esistente", r.name.c_str(), r.triggerArea);
            ok = false;
        }
        for (auto& c : r.cases)
            for (auto& act : c.actions)
                if (act.targetArea < 0 || act.targetArea >= buf.size()) {
                    LOG_EF("Config", "Route '%s' ha action verso area %d non esistente", r.name.c_str(), act.targetArea);
                    ok = false;
                }
    }

    if (!ok) {
        LOG_EF("Config", "VALIDAZIONE DINAMICA FALLITA");
        return false;
    }
    LOG_IF("Config", "VALIDAZIONE DINAMICA OK");
    return true;
}

inline bool DomoConfigValidator::validate(const DomoManagerConfig& cfg, DomoManager& DomoManager) {
    return validateStatic(cfg, DomoManager) && validateDynamic(cfg, DomoManager);
}

inline bool DomoConfigValidator::validateDefineAreas() {
    bool ok = true;
    std::unordered_set<int> seen;
    for (int i = 0; i < AreaRegistry::count(); ++i) {
        const int v = AreaRegistry::getValueByIndex(i);
        if (!seen.insert(v).second) {
            LOG_EF("Config", "DEFINE_AREA duplicata: valore=%d", v);
            ok = false;
        }
    }
    return ok;
}

inline bool DomoConfigValidator::validateDeviceAreas(const DomoManagerConfig::Devices& cfg, DomoManager& DomoManager) {
    bool ok = true;
    for (const auto& d : cfg.list) {
        auto* profile = DomoManager.getProfiles().get(d.profile.c_str());
        if (!profile) {
            LOG_EF("Config", "Profilo '%s' non trovato", d.profile.c_str());
            ok = false;
            continue;
        }
        int expected = 0;
        for (const auto& ch : *profile) expected += ch.items;
        if (expected != static_cast<int>(d.areas.size())) {
            LOG_EF("Config", "Device '%s' ha %d aree ma ne servono %d", d.name.c_str(), (int)d.areas.size(), expected);
            ok = false;
        }
    }
    return ok;
}

inline bool DomoConfigValidator::validateDuplicateDeviceAreas(const DomoManagerConfig::Devices& cfg) {
    bool ok = true;
    std::unordered_map<int, String> owner;
    for (const auto& d : cfg.list) {
        for (int a : d.areas) {
            if (owner.count(a)) {
                LOG_EF("Config", "Area %d usata da '%s' e '%s'", a, owner[a].c_str(), d.name.c_str());
                ok = false;
            }
            owner[a] = d.name;
        }
    }
    return ok;
}

inline bool DomoConfigValidator::validateRoutes(const DomoManagerRouteEngine::RoutesConfig& cfg, DomoManager& DomoManager) {
    bool ok = true;
    const auto& fullCfg = DomoManager.getConfig();
    std::unordered_set<int> logged;
    for (const auto& r : cfg.list) {
        if (!isAreaKnownInConfig(r.triggerArea, fullCfg)) {
            if (logged.insert(r.triggerArea).second)
                LOG_EF("Config::validateRoutes", "Route '%s': triggerArea %d non definita in config", r.name.c_str(), r.triggerArea);
            ok = false;
        }
        for (const auto& c : r.cases) for (const auto& act : c.actions) {
            if (!isAreaKnownInConfig(act.targetArea, fullCfg)) {
                if (logged.insert(act.targetArea).second)
                    LOG_EF("Config::validateRoutes", "Route '%s': targetArea %d non definita in config", r.name.c_str(), act.targetArea);
                ok = false;
            }
        }
    }
    return ok;
}

inline bool DomoConfigValidator::validateSplits(const DMSplitEngineEx::SplitsConfig& cfg, DomoManager& DomoManager) {
    bool ok = true;
    const auto& fullCfg = DomoManager.getConfig();
    std::unordered_set<int> loggedMain;
    std::unordered_set<int> loggedOut;
    for (const auto& s : cfg.list) {
        if (!isAreaKnownInConfig(s.mainArea, fullCfg)) {
            if (loggedMain.insert(s.mainArea).second)
                LOG_EF("Config::validateSplits", "Split mainArea %d non definita in config", s.mainArea);
            ok = false;
        }
        for (int a : s.outAreas) {
            if (!isAreaKnownInConfig(a, fullCfg)) {
                if (loggedOut.insert(a).second)
                    LOG_EF("Config::validateSplits", "Split outArea %d non definita in config", a);
                ok = false;
            }
        }
    }
    return ok;
}

inline bool DomoConfigValidator::validateToggles(const DMToggleEngineEx::TogglesConfig& cfg, DomoManager& DomoManager) {
    bool ok = true;
    const auto& fullCfg = DomoManager.getConfig();
    for (const auto& t : cfg.list) {
        if (!isAreaKnownInConfig(t.areaRead, fullCfg)) {
            LOG_EF("Config::validateToggles", "Toggle areaRead %d non definita in config", t.areaRead);
            ok = false;
        }
        for (int a : t.forwards) {
            if (!isAreaKnownInConfig(a, fullCfg)) {
                LOG_EF("Config::validateToggles", "Toggle forwardArea %d non definita in config", a);
                ok = false;
            }
        }
    }
    return ok;
}



#endif
