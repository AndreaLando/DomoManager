#ifndef DMBaseClass_HPP
#define DMBaseClass_HPP

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

/*===============================================================================
DMBaseClass — Core utilities for state tracking, averaging, thresholds, timers
===============================================================================
This module provides foundational building blocks for the entire system:
- Cell: reactive value container with change‑detection.
- safeRead: safe function wrapper with fallback.
- Group & AverageCalculator: moving averages and trend detection.
- AnalogThresholdManager: dynamic analog threshold selection.
- SplitOutManager: split‑output logic with TON timers and callbacks.
- Errors: automatic error accumulation and timed recovery.
- Watchdog: execution‑time monitoring, spike detection, overload alerts.
- LedController: non‑blocking LED control with blink and pulse modes.
- AreaRegistry: symbolic area registry with auto‑registration macros.
- AsyncScheduler: cooperative task engine with steps, branching, delays,
                  skip conditions, priorities and completion callbacks.
=============================================================================== */


#include <Arduino.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "DMSignal.hpp"
#include "DMLogger.hpp"

template<typename T>
    T safeRead(const std::function<T()>& fn, T fallback) {
        return fn ? fn() : fallback;
    }

template<typename T>
class Cell {
  private:
    T value;
    bool changed;

  public:
    Cell() : value{}, changed(false) {}
    Cell(const T& initial) : value(initial), changed(false) {}

    inline void setIfDiff(const T& newValue) {
      // Set change flag only if old and new values are differents
      if (value != newValue) {
          value = newValue;
          changed = true;
      }
    }

    inline void set(const T& newValue) {
      value = newValue;
      changed = true;
    }

    inline T get() {
        changed = false;
        return value;
    }

    // Lettura che NON resetta il flag 
    inline T preserveGet() const { 
      return value; 
    }

    inline bool hasChanged() {
        return changed;
    }

    inline void resetChanged() {
        changed = false;
    }
};

  //Calcolatore Medie
#define NUM_VARIATIONS 5
/* 29.05.2026 Migliorata in velocita
class Group {
public:
    String name;

    // Circular buffer for measurements
    std::vector<float> buffer;
    int maxSize;
    int head = 0;
    int count = 0;

    // Circular buffer for variations
    std::vector<float> variations;
    int headVar = 0;
    int countVar = 0;

    float threshold;

    enum Trend {
        CONSTANT,
        INCREASING,
        DECREASING
    };

    Group(String n, int size = 5, float sens = 0.1)
        : name(n), maxSize(size), threshold(sens)
    {
        buffer.resize(maxSize, 0);
        variations.resize(NUM_VARIATIONS, 0);
    }

    void updateMeasurement(float value) {
        // Compute variation compared to last measurement
        if (count > 0) {
            float last = buffer[(head - 1 + maxSize) % maxSize];
            float diff = value - last;
            updateVariation(diff);
        }

        // Write into circular buffer
        buffer[head] = value;
        head = (head + 1) % maxSize;

        if (count < maxSize) count++;
    }

    void updateVariation(float diff) {
        variations[headVar] = diff;
        headVar = (headVar + 1) % NUM_VARIATIONS;

        if (countVar < NUM_VARIATIONS) countVar++;
    }

    float average() const {
        if (count == 0) return 0;

        float sum = 0;
        for (int i = 0; i < count; i++) {
            sum += buffer[i];
        }
        return sum / count;
    }

    float averageVariations() const {
        if (countVar == 0) return 0;

        float sum = 0;
        for (int i = 0; i < countVar; i++) {
            sum += variations[i];
        }
        return sum / countVar;
    }

    Trend trend() const {
        if (countVar == 0) return CONSTANT;

        float m = averageVariations();

        if (m < threshold && m > -threshold) return CONSTANT;
        if (m > 0) return INCREASING;
        return DECREASING;
    }
};*/
class Group {
public:
    enum Trend {
        CONSTANT,
        INCREASING,
        DECREASING
    };

    // --- API pubblica invariata ---
    String name;

    Group(String n, int size = 5, float sens = 0.1)
        : name(n), maxSize(size), threshold(sens)
    {
        if (maxSize > MAX_SIZE) {
            LOG_WF("Group", "maxSize=%d supera MAX_SIZE=%d, ridimensiono", maxSize, MAX_SIZE);
            maxSize = MAX_SIZE;
        }

        // inizializza buffer
        for (int i = 0; i < MAX_SIZE; i++) buffer[i] = 0;
        for (int i = 0; i < MAX_VAR; i++) variations[i] = 0;
    }

    // ---------------------------------------------------------
    // API pubblica: updateMeasurement
    // ---------------------------------------------------------
    void updateMeasurement(float value) {
        // rimuovi valore vecchio dalla somma
        if (count == maxSize)
            sum -= buffer[head];

        // variazione rispetto al precedente
        if (count > 0) {
            float last = buffer[(head - 1 + maxSize) % maxSize];
            float diff = value - last;
            updateVariation(diff);
        }

        // inserisci nuovo valore
        buffer[head] = value;
        sum += value;

        head = (head + 1) % maxSize;
        if (count < maxSize) count++;
    }

    // ---------------------------------------------------------
    // API pubblica: average O(1)
    // ---------------------------------------------------------
    float average() const {
        return (count == 0) ? 0 : (sum / count);
    }

    // ---------------------------------------------------------
    // API pubblica: trend O(1)
    // ---------------------------------------------------------
    Trend trend() const {
        if (countVar == 0) return CONSTANT;

        float m = averageVariations();

        if (m < threshold && m > -threshold) return CONSTANT;
        if (m > 0) return INCREASING;
        return DECREASING;
    }

private:
    // =========================================================
    //  PRIVATE — tutto ciò che non serve all’esterno
    // =========================================================

    static constexpr int MAX_SIZE = 16;
    static constexpr int MAX_VAR  = NUM_VARIATIONS;

    float buffer[MAX_SIZE];
    float variations[MAX_VAR];

    int maxSize;
    int head = 0;
    int count = 0;

    int headVar = 0;
    int countVar = 0;

    float threshold;

    // somme mantenute per O(1)
    float sum = 0;
    float sumVar = 0;

    // ---------------------------------------------------------
    // updateVariation — O(1)
    // ---------------------------------------------------------
    void updateVariation(float diff) {
        if (countVar == MAX_VAR) {
            sumVar -= variations[headVar];
        }

        variations[headVar] = diff;
        sumVar += diff;

        headVar = (headVar + 1) % MAX_VAR;
        if (countVar < MAX_VAR) countVar++;
    }

    // ---------------------------------------------------------
    // averageVariations — O(1)
    // ---------------------------------------------------------
    float averageVariations() const {
        return (countVar == 0) ? 0 : (sumVar / countVar);
    }
};

/* 29.05.2026 Migliorata in velocita
class AverageCalculator {
public:
    std::vector<Group> groups;

    void createGroup(const String& name, int size = 5, float threshold = 0.1) {
        groups.emplace_back(name, size, threshold);
    }

    Group* findGroup(const String& name) {
        for (auto& g : groups) {
            if (g.name == name) return &g;
        }
        return nullptr;
    }

    void addMeasurement(const String& groupName, float value, float threshold = 0.1) {
        Group* g = findGroup(groupName);
        if (!g) {
            createGroup(groupName, 5, threshold);
            g = findGroup(groupName);
        }
        g->updateMeasurement(value);
    }

    float groupAverage(const String& groupName) {
        Group* g = findGroup(groupName);
        return g ? g->average() : 0;
    }

    Group::Trend groupTrend(const String& groupName) {
        Group* g = findGroup(groupName);
        return g ? g->trend() : Group::CONSTANT;
    }
};*/
class AverageCalculator {
public:
    std::vector<Group> groups;

private:
    // Lookup O(1) senza unordered_map
    static constexpr int MAX_GROUPS = 64;
    static constexpr int MAX_NAME_LEN = 32;

    // Tabella di lookup: nome → indice
    struct NameEntry {
        char name[MAX_NAME_LEN];
        int index;
    };

    NameEntry nameTable[MAX_GROUPS];
    uint8_t nameCount = 0;

public:

    AverageCalculator() {
        for (int i = 0; i < MAX_GROUPS; i++)
            nameTable[i].index = -1;
    }

    // ---------------------------------------------------------
    // createGroup — identico, ma con lookup O(1)
    // ---------------------------------------------------------
    void createGroup(const String& name, int size = 5, float threshold = 0.1) {
        if (groups.size() >= MAX_GROUPS) {
            LOG_WF("AverageCalculator", "MAX_GROUPS superato (%u)", MAX_GROUPS);
            return;
        }

        groups.emplace_back(name, size, threshold);

        // registra nome in lookup table
        strncpy(nameTable[nameCount].name, name.c_str(), MAX_NAME_LEN);
        nameTable[nameCount].name[MAX_NAME_LEN - 1] = '\0';
        nameTable[nameCount].index = groups.size() - 1;
        nameCount++;
    }

    // ---------------------------------------------------------
    // findGroup — ora O(1)
    // ---------------------------------------------------------
    Group* findGroup(const String& name) {
        const char* target = name.c_str();

        for (uint8_t i = 0; i < nameCount; i++) {
            if (strcmp(nameTable[i].name, target) == 0)
                return &groups[nameTable[i].index];
        }

        return nullptr;
    }

    // ---------------------------------------------------------
    // addMeasurement — ora velocissimo
    // ---------------------------------------------------------
    void addMeasurement(const String& groupName, float value, float threshold = 0.1) {
        Group* g = findGroup(groupName);

        if (!g) {
            createGroup(groupName, 5, threshold);
            g = findGroup(groupName);   // ora O(1)
        }

        g->updateMeasurement(value);
    }

    // ---------------------------------------------------------
    float groupAverage(const String& groupName) {
        Group* g = findGroup(groupName);
        return g ? g->average() : 0;
    }

    Group::Trend groupTrend(const String& groupName) {
        Group* g = findGroup(groupName);
        return g ? g->trend() : Group::CONSTANT;
    }
};

/* 29.05.2026 Migliorata in velocita
class AnalogThresholdManager {
public:
    struct Item {
        int area;
        int thresholdLow;
        int thresholdHigh;
    };

private:
    std::vector<Item> items;
    std::unordered_map<int, size_t> cache;

public:
    void add(int area, int low, int high = -1) {
        items.push_back({area, low, high});
        cache[area] = items.size() - 1;
    }

    // Ritorna nullptr se non esiste
    const Item* get(int area) const {
        auto it = cache.find(area);
        if (it == cache.end())
            return nullptr;
        return &items[it->second];
    }

    // Calcolo soglia effettiva
    int getThreshold(int area, long value) const {
        const Item* it = get(area);
        if (!it)
            return 20;   // fallback: soglia default

        if (it->thresholdHigh > 0 && value > 1000)
            return it->thresholdHigh;

        return it->thresholdLow;
    }
};*/
class AnalogThresholdManager {
public:
    struct Item {
        int area;
        int thresholdLow;
        int thresholdHigh;
    };

private:
    // --- Ottimizzazione 1: array statico ---
    static constexpr int MAX_ITEMS = 64;
    Item items[MAX_ITEMS];
    uint8_t count = 0;

    // --- Ottimizzazione 2: lookup O(1) senza unordered_map ---
    static constexpr int MAX_AREAS = 1024;
    int indexByArea[MAX_AREAS];

public:
    AnalogThresholdManager() {
        for (int i = 0; i < MAX_AREAS; i++)
            indexByArea[i] = -1;
    }

    // ---------------------------------------------------------
    // ADD — identico, ma senza allocazioni dinamiche
    // ---------------------------------------------------------
    void add(int area, int low, int high = -1) {
        if (count >= MAX_ITEMS) {
            LOG_WF("AnalogThresholdManager", "MAX_ITEMS superato (%u)", MAX_ITEMS);
            return;
        }

        items[count] = { area, low, high };
        indexByArea[area] = count;
        count++;
    }

    // ---------------------------------------------------------
    // GET — lookup O(1) reale
    // ---------------------------------------------------------
    const Item* get(int area) const {
        if (area < 0 || area >= MAX_AREAS) return nullptr;
        int idx = indexByArea[area];
        return (idx >= 0) ? &items[idx] : nullptr;
    }

    // ---------------------------------------------------------
    // THRESHOLD — velocissimo
    // ---------------------------------------------------------
    int getThreshold(int area, long value) const {
        const Item* it = get(area);
        if (!it)
            return 20;   // fallback

        if (it->thresholdHigh > 0 && value > 1000)
            return it->thresholdHigh;

        return it->thresholdLow;
    }
};

/* 29.05.2026 Migliorata in velocita
class SplitOutManager {
public:
    struct Split {
        unsigned long maxTime;
        int mainArea;
        std::vector<int> outAreas;
    };

    struct Item {
        Split split;
        bool running = false;
        TON timer;
    };

    using CallbackFn = void (*)(const Split&, bool isStart);

private:
    std::vector<Item> splits;
    std::unordered_map<int, size_t> cacheByArea;
    CallbackFn callback;
    std::unordered_set<int> splitTargets;

public:

    SplitOutManager(CallbackFn cb) : callback(cb) {}
    
    bool isSplitTarget(int area) const {
        return splitTargets.count(area) > 0;
    }

    bool hasActiveSplits() const {
        for (const auto& s : splits) {
            if (s.running)   // oppure s.timer.Q() se usi TON
                return true;
        }
        return false;
    }

    bool exist(int areaRead) const {
        return cacheByArea.find(areaRead) != cacheByArea.end();
    }

    const std::vector<Item>& getAll() const { 
        return splits; 
    }

    void add(int mainArea, std::vector<int> outAreas = {}, unsigned long maxTime = 0) {
        Item it;
        it.split = { maxTime, mainArea, outAreas };
        it.running = false;

        if (maxTime > 0)
            it.timer.SetPreset(maxTime, TimerBase::Milliseconds);

        splits.push_back(it);
        cacheByArea[mainArea] = splits.size() - 1;

        // 🔥 Registra tutte le aree target
        for (int out : outAreas)
            splitTargets.insert(out);

        LOG_DF("SplitOutManager", "Added split area=%d maxTime=%lu", mainArea, maxTime);
    }


    Item* get(int areaRead) {
        auto it = cacheByArea.find(areaRead);
        if (it == cacheByArea.end())
            return nullptr;
        return &splits[it->second];
    }

    // ---------------------------------------------------------
    // START — ritorna outAreas se l'area esiste
    // ---------------------------------------------------------
   void start(int areaRead) {
        Item* it = get(areaRead);
        if (!it) {
            LOG_EF("SplitOutManager", "Start failed: area %d not found", areaRead);
            return;
        }

        if (it->running) {
            LOG_IF("SplitOutManager", "Start ignored: area %d already running", areaRead);
            return;
        }

        it->running = true;

        LOG_IF("SplitOutManager", "Start split area=%d maxTime=%lu", it->split.mainArea, it->split.maxTime);

        // 🔥 callback START=1 SEMPRE
        callback(it->split, true);

        if (it->split.maxTime == 0) {
            it->running = false;
            return;
        }

        it->timer.Run(false);   // reset TON
        it->timer.SetPreset(it->split.maxTime, TimerBase::Milliseconds);
        it->timer.Run(true);
    }

    // ---------------------------------------------------------
    // RESET (contrario di start)
    // ---------------------------------------------------------
    void reset(int areaRead) {
        Item* it = get(areaRead);
        if (!it) {
            LOG_EF("SplitOutManager", "Reset failed: area %d not found", areaRead);
            return;
        }

        if (!it->running) {
            LOG_IF("SplitOutManager", "Reset ignored: area %d not running", areaRead);
            return;
        }

        it->running = false;
        it->timer.Run(false);  // reset TON

        LOG_DF("SplitOutManager", "Reset split area=%d", it->split.mainArea);

        // callback di reset
        callback(it->split, false);
    }

    // ---------------------------------------------------------
    // Deve essere chiamato ciclicamente nel loop()
    // ---------------------------------------------------------
    void update() {
        for (auto &it : splits) {
            if (!it.running) continue;

            if (it.split.maxTime == 0)
                continue;

            it.timer.Run(true);

            if (it.timer.Q()) {
                LOG_DF("SplitOutManager", "Timer expired for area=%d", it.split.mainArea);

                callback(it.split, false);  

                it.running = false;

                LOG_DF("SplitOutManager", "Running reset for area=%d", it.split.mainArea);
            }
        }
    }

    bool IsRunning(int areaRead) {
        Item* it = get(areaRead);
        bool r = it ? it->running : false;
        LOG_DF("SplitOutManager", "IsRunning area=%d → %d", areaRead, r);
        return r;
    }
};*/
class SplitOutManager {
public:
    struct Split {
        unsigned long maxTime;
        int mainArea;
        std::vector<int> outAreas;
    };

    struct Item {
        Split split;
        bool running = false;
        TON timer;
    };

    using CallbackFn = void (*)(const Split&, bool isStart);

private:
    // --- Ottimizzazione 1: preallocazione ---
    static constexpr size_t MAX_SPLITS = 64;

    std::vector<Item> splits;
    CallbackFn callback;

    // --- Ottimizzazione 2: lookup O(1) senza unordered_map ---
    static constexpr int MAX_AREAS = 1024;
    int indexByArea[MAX_AREAS];

    // --- Ottimizzazione 3: bitset invece di unordered_set ---
    bool splitTargets[MAX_AREAS];

    // --- Ottimizzazione 4: active list ---
    uint8_t activeList[MAX_SPLITS];
    uint8_t activeCount = 0;

public:

    SplitOutManager(CallbackFn cb) : callback(cb) {
        splits.reserve(MAX_SPLITS);

        for (int i = 0; i < MAX_AREAS; i++) {
            indexByArea[i] = -1;
            splitTargets[i] = false;
        }
    }

    bool isSplitTarget(int area) const {
        return (area >= 0 && area < MAX_AREAS) ? splitTargets[area] : false;
    }

    bool hasActiveSplits() const {
        return activeCount > 0;
    }

    bool exist(int areaRead) const {
        return (areaRead >= 0 && areaRead < MAX_AREAS) && indexByArea[areaRead] != -1;
    }

    const std::vector<Item>& getAll() const { 
        return splits; 
    }

    // ---------------------------------------------------------
    // ADD
    // ---------------------------------------------------------
    void add(int mainArea, std::vector<int> outAreas = {}, unsigned long maxTime = 0) {
        if (splits.size() >= MAX_SPLITS) {
            LOG_WF("SplitOutManager", "MAX_SPLITS superato (%u)", MAX_SPLITS);
            return;
        }

        Item it;
        it.split = { maxTime, mainArea, std::move(outAreas) };
        it.running = false;

        if (maxTime > 0)
            it.timer.SetPreset(maxTime, TimerBase::Milliseconds);

        indexByArea[mainArea] = splits.size();

        for (int out : it.split.outAreas)
            if (out >= 0 && out < MAX_AREAS)
                splitTargets[out] = true;

        splits.push_back(std::move(it));

        LOG_DF("SplitOutManager", "Added split area=%d maxTime=%lu", mainArea, maxTime);
    }

    Item* get(int areaRead) {
        if (areaRead < 0 || areaRead >= MAX_AREAS) return nullptr;
        int idx = indexByArea[areaRead];
        return (idx >= 0) ? &splits[idx] : nullptr;
    }

    // ---------------------------------------------------------
    // START
    // ---------------------------------------------------------
    void start(int areaRead) {
        Item* it = get(areaRead);
        if (!it) {
            LOG_WF("SplitOutManager", "Start failed: area %d not found", areaRead);
            return;
        }

        if (it->running) {
            LOG_IF("SplitOutManager", "Start ignored: area %d already running", areaRead);
            return;
        }

        it->running = true;
        callback(it->split, true);

        if (it->split.maxTime == 0) {
            it->running = false;
            return;
        }

        it->timer.Run(false);
        it->timer.SetPreset(it->split.maxTime, TimerBase::Milliseconds);
        it->timer.Run(true);

        // Active list
        activeList[activeCount++] = indexByArea[areaRead];
    }

    // ---------------------------------------------------------
    // RESET
    // ---------------------------------------------------------
    void reset(int areaRead) {
        Item* it = get(areaRead);
        if (!it) {
            LOG_WF("SplitOutManager", "Reset failed: area %d not found", areaRead);
            return;
        }

        if (!it->running) {
            LOG_IF("SplitOutManager", "Reset ignored: area %d not running", areaRead);
            return;
        }

        it->running = false;
        it->timer.Run(false);

        callback(it->split, false);

        // Rimuovi da active list
        int idx = indexByArea[areaRead];
        for (uint8_t i = 0; i < activeCount; i++) {
            if (activeList[i] == idx) {
                activeList[i] = activeList[--activeCount];
                break;
            }
        }
    }

    // ---------------------------------------------------------
    // UPDATE — ora ultra veloce
    // ---------------------------------------------------------
    void update() {
        for (uint8_t i = 0; i < activeCount; i++) {
            Item& it = splits[activeList[i]];

            it.timer.Run(true);

            if (it.timer.Q()) {
                callback(it.split, false);
                it.running = false;

                activeList[i] = activeList[--activeCount];
                i--;
            }
        }
    }

    bool IsRunning(int areaRead) {
        Item* it = get(areaRead);
        bool r = it ? it->running : false;
        LOG_DF("SplitOutManager", "IsRunning area=%d → %d", areaRead, r);
        return r;
    }
};

class Errors {
public:
    Errors(short maxErrors, unsigned long retryWindow = 60000)
        : _maxErrors(maxErrors),
          _retryWindow(retryWindow) {}

    bool Loop(bool inError, unsigned long now)
    {
        if (!_error) {
            if (inError) {
                IncrementError();
                if (_cnt >= _maxErrors)
                    enterError(now);
            }
            return !_error;
        }

        tryReset(now);
        return false;
    }

    bool IsInError() const { return _error; }

    void IncrementError()
    {
        _cnt++;
        LOG_DF(__FUNCTION__, "ERROR INCREMENT cnt=%d", _cnt);
    }

private:
    void enterError(unsigned long now)
    {
        _error = true;
        _lastError = now;
        LOG_DF(__FUNCTION__, "ERROR ENTER");
    }

    void tryReset(unsigned long now)
    {
        unsigned long retryAt = _lastError + (_retryWindow * _maxErrors);
        retryAt += random(0, 30000);

        LOG_DF(__FUNCTION__, "ERROR RETRY check=%lu now=%lu", retryAt, now);

        if (now >= retryAt)
            clear();
    }

    void clear()
    {
        _error = false;
        _cnt = 0;
        _lastError = 0;

        LOG_DF(__FUNCTION__, "ERROR RESET");
    }

private:
    short _maxErrors;
    short _cnt = 0;

    bool _error = false;
    unsigned long _lastError = 0;
    unsigned long _retryWindow;
};

class Watchdog {
public:
    struct Params {
        // ---- Activity Loop ----
        unsigned long activity_block_ms = 5000;
        unsigned long activity_avg_ms   = 170;

        unsigned int  max_spikes       = 10;
        unsigned long spikes_window_s  = 60;

        // Rate limit mode
        unsigned long rate_limit_min_interval   = 5000;
        unsigned long rate_limit_allowed_factor = 2;   // divisor (minInterval / 2)

        // ---- Update Cycle ----
        unsigned long update_slow_ms = 110;
        unsigned long update_avg_ms  = 80;

        // ---- Spike detection ----
        int spikeThresholdFactor_fp = 10 * 256; // Q8.8
    };

    struct ExecTiming {
        const char*   name      = nullptr;
        unsigned long last      = 0;
        int           avg_fp    = 0;   // exponential moving average (Q8.8)
        bool          spike     = false;
        unsigned long maxSpike  = 0;

        // Per Watchdog
        unsigned int  spikeCount    = 0;
        unsigned long lastSpikeTime = 0;
    };

    /*
    2.0-4.0  molto alta      rileva spike piccoli
    4.0-6.0  media (default) buon equilibrio
    7.0–10.0 bassa           rileva solo spike seri
    */
    struct CallbackTimings {
        ExecTiming somethingChanged;
        ExecTiming route;
        ExecTiming activityLoop;
        ExecTiming updateCycle;
    };

    struct WatchdogStatus {
        bool overload  = false;
        bool blocked   = false;
        bool unstable  = false;
        bool inactive  = false;

        const char*    reason = nullptr;
        unsigned long  value  = 0;
    };

    using WatchdogFn = void (*)(const WatchdogStatus&);

    explicit Watchdog(CallbackTimings* timings, const Params* params)
        : timings(timings), params(params) {}

    void setCallback(WatchdogFn fn) {
        callback = fn;
    }

    void setActivityLoopRateLimit(unsigned long minInterval) {
        if (minInterval > 0) {
            activityLoopRateLimited = true;
            activityLoopMinInterval = minInterval;
        } else {
            activityLoopRateLimited = false;
            activityLoopMinInterval = 0;
        }
    }

    void check(unsigned long now) {
        if (!timings || !params) return;

        WatchdogStatus st;

        // -------------------------
        //   ACTIVITY LOOP CHECKS
        // -------------------------
        ExecTiming& t = timings->activityLoop;

        if (activityLoopRateLimited) {
            handleActivityLoopRateLimited(t, st);
        } else {
            handleActivityLoopNormal(t, now, st);
        }

        // -------------------------
        //   UPDATE CYCLE CHECKS
        // -------------------------
        ExecTiming& u = timings->updateCycle;
        handleUpdateCycle(u, st);

        // -------------------------
        //   CALLBACK
        // -------------------------
        if (st.reason && callback) {
            callback(st);
        }
    }

private:
    CallbackTimings* timings = nullptr;
    const Params*    params  = nullptr;
    WatchdogFn       callback = nullptr;

    bool            activityLoopRateLimited = false;
    unsigned long   activityLoopMinInterval = 0;

    // Buffer per messaggi per istanza (no static condiviso)
    char reasonBuf[64] = {0};

    void setStatus(WatchdogStatus& st,
                   bool* flag,
                   const char* fmt,
                   unsigned long value,
                   unsigned long threshold) {
        if (!flag) return;
        *flag = true;
        snprintf(reasonBuf, sizeof(reasonBuf), fmt, threshold);
        st.reason = reasonBuf;
        st.value  = value;
    }

    void handleActivityLoopRateLimited(ExecTiming& t, WatchdogStatus& st) {
        // Se il fattore è 0, disabilita il controllo per sicurezza
        if (params->rate_limit_allowed_factor == 0) {
            return;
        }

        unsigned long avg = static_cast<unsigned long>(t.avg_fp >> 8);
        unsigned long allowed =
            activityLoopMinInterval / params->rate_limit_allowed_factor;

        if (avg > allowed && !st.reason) {
            setStatus(st,
                      &st.overload,
                      "activityLoop avg high (>%lums) [rate-limited]",
                      avg,
                      allowed);
        }
    }

    void handleActivityLoopNormal(ExecTiming& t,
                                  unsigned long now,
                                  WatchdogStatus& st) {
        // 1. Blocco
        if (!st.reason && t.last > params->activity_block_ms) {
            setStatus(st,
                      &st.blocked,
                      "activityLoop blocked (>%ums)",
                      t.last,
                      params->activity_block_ms);
            return;
        }

        // 2. Media troppo alta
        if (t.avg_fp > (params->activity_avg_ms << 8) && !st.reason) {
            const unsigned long avg = t.avg_fp >> 8;
            setStatus(st,
                      &st.overload,
                      "activityLoop avg too high (>%ums)",
                      avg,
                      params->activity_avg_ms);
            return;
        }

        // 3. Troppi spike in finestra
        const unsigned long windowMs = params->spikes_window_s * 1000UL;

        if (t.spikeCount > params->max_spikes &&
            (now - t.lastSpikeTime) < windowMs &&
            !st.reason) {
            st.unstable = true;
            snprintf(reasonBuf, sizeof(reasonBuf),
                     "too many spikes in %lus", params->spikes_window_s);
            st.reason = reasonBuf;
            st.value  = t.spikeCount;
        }
    }

    void handleUpdateCycle(ExecTiming& u, WatchdogStatus& st) {
        // Se hai già un motivo, puoi decidere se sovrascrivere o no.
        // Qui scelgo di NON sovrascrivere se c'è già un allarme activityLoop.
        if (st.reason) return;

        if (u.last > params->update_slow_ms) {
            setStatus(st,
                      &st.overload,
                      "Update cycle too slow (>%ums)",
                      u.last,
                      params->update_slow_ms);
            return;
        }

        if (u.avg_fp > (params->update_avg_ms << 8)) {
            unsigned long avg = static_cast<unsigned long>(u.avg_fp >> 8);
            setStatus(st,
                      &st.overload,
                      "Update cycle avg too high (>%ums)",
                      avg,
                      params->update_avg_ms);
        }
    }
};

class LedController {
public:
    static constexpr int NO_PIN = -1;

    struct LedPins {
        int read   = NO_PIN;
        int write = NO_PIN;
        int panel = NO_PIN;
        int err   = NO_PIN;

        constexpr LedPins() = default;
        constexpr LedPins(int r, int w, int p, int e) : read(r), write(w), panel(p), err(e) {}
        bool any() const { return read >= 0 || write >= 0 || panel >= 0 || err >= 0; }
    };

    enum Channel : uint8_t { READ = 0, WRITE = 1, PANEL = 2, ERR = 3, CHANNEL_COUNT = 4 };

    // Default constructor (no pins configured)
    LedController()
        : m_pins(), m_activeLow(false), m_initialized(false)
    {}

    // Constructor that accepts pins (no default parameter here)
    LedController(const LedPins& pins, bool activeLow = false)
        : m_pins(pins), m_activeLow(activeLow), m_initialized(false)
    {}

    // Call in setup() after Arduino core is ready
    void begin() {
        if (m_initialized) return;
        initPin(m_pins.read);
        initPin(m_pins.write);
        initPin(m_pins.panel);
        initPin(m_pins.err);
        setAll(false);
        m_initialized = true;
    }

    // Must be called periodically from loop with millis()
    void update(unsigned long now) {
        for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
            ChannelState& s = m_state[i];

            if (s.blinking) {
                if (now - s.lastToggle >= s.currentInterval) {
                    s.lastToggle = now;
                    s.on = !s.on;
                    applyChannel((Channel)i);
                    s.currentInterval = s.on ? s.blinkOnMs : s.blinkOffMs;
                }
            }

            if (s.pulsing) {
                if (now - s.pulseStart >= s.pulseDuration) {
                    s.pulsing = false;
                    s.on = s.pulseRestoreState;
                    applyChannel((Channel)i);
                }
            }
        }
    }

    // Basic control
    void set(Channel ch, bool on) {
        ChannelState& s = m_state[(uint8_t)ch];
        s.on = on;
        s.blinking = false;
        s.pulsing = false;
        applyChannel(ch);
    }

    bool isOn(Channel ch) const {
        return m_state[(uint8_t)ch].on;
    }

    void toggle(Channel ch) {
        bool oldState = isOn(ch);
        bool newState = !oldState;

        set(ch, newState);
    }

    // Blink non-blocking
    void blink(Channel ch, unsigned long onMs, unsigned long offMs) {
        ChannelState& s = m_state[(uint8_t)ch];
        if (onMs == 0 || offMs == 0) {
            s.blinking = false;
            return;
        }
        s.blinking = true;
        s.blinkOnMs = onMs;
        s.blinkOffMs = offMs;
        s.on = true;
        s.currentInterval = onMs;
        s.lastToggle = millis();
        applyChannel(ch);
    }

    void stopBlink(Channel ch) {
        ChannelState& s = m_state[(uint8_t)ch];
        s.blinking = false;
    }

    // Pulse: temporarily set to 'on' for durationMs, then restore previous state
    void pulse(Channel ch, unsigned long durationMs) {
        ChannelState& s = m_state[(uint8_t)ch];
        s.pulsing = true;
        s.pulseDuration = durationMs;
        s.pulseStart = millis();
        s.pulseRestoreState = s.on;
        s.on = true;
        s.blinking = false;
        applyChannel(ch);
    }

    void setAll(bool on) {
        for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
            set((Channel)i, on);
        }
    }

    LedPins pins() const { return m_pins; }
    void setPins(const LedPins& pins) {
        m_pins = pins;
        // do not call begin() automatically
    }

    bool hasChannel(Channel ch) const {
        return getPin(ch) >= 0;
    }

private:
    struct ChannelState {
        bool on = false;
        bool blinking = false;
        unsigned long blinkOnMs = 0;
        unsigned long blinkOffMs = 0;
        unsigned long currentInterval = 0;
        unsigned long lastToggle = 0;

        bool pulsing = false;
        unsigned long pulseDuration = 0;
        unsigned long pulseStart = 0;
        bool pulseRestoreState = false;
    };

    LedPins m_pins;
    bool m_activeLow = false;
    bool m_initialized = false;
    ChannelState m_state[CHANNEL_COUNT] = {};

    inline int getPin(Channel ch) const {
        switch (ch) {
            case READ:   return m_pins.read;
            case WRITE: return m_pins.write;
            case PANEL: return m_pins.panel;
            case ERR:   return m_pins.err;
            default:    return NO_PIN;
        }
    }

    inline void initPin(int pin) {
        if (pin >= 0) pinMode(pin, OUTPUT);
    }

    inline void applyChannel(Channel ch) {
        int pin = getPin(ch);
        if (pin < 0) return;
        bool outState = m_state[(uint8_t)ch].on;
        if (m_activeLow) outState = !outState;
        digitalWrite(pin, outState ? HIGH : LOW);
    }
};

 class ButtonManager {
    public:
        struct ButtonState {
            bool pressedNow = false;
            bool pressedAtStartup = false;  // evento una tantum
        };

    private:
        int pin;
        bool startupState = false;
        bool startupConsumed = false;   // <-- AGGIUNTO
        bool lastState = false;

    public:
        ButtonManager(int pin) : pin(pin) {}

        void begin() {
            pinMode(pin, INPUT);  // Opta: NO pullup
            delay(50);            // stabilizzazione PC13

            startupState = (digitalRead(pin) == LOW);
            lastState = startupState;
        }

        ButtonState update(unsigned long now) {
            ButtonState st;

            bool nowState = (digitalRead(pin) == LOW);

            // fronte di discesa
            st.pressedNow = (nowState && !lastState);

            // evento startup → SOLO UNA VOLTA
            if (!startupConsumed && startupState) {
                st.pressedAtStartup = true;
                startupConsumed = true;   // <-- evita ripetizioni
            }

            lastState = nowState;
            return st;
        }
};

/* Vecchia versione slow 29.05.2026
class AreaRegistry {
public:
    struct Entry {
        const char* name;
        int value;
    };

    static Entry* entries() {
        static Entry list[512];   // max aree
        return list;
    }

    static int& count() {
        static int c = 0;
        return c;
    }

    static void registerArea(const char* name, int value) {
        int idx = count();
        entries()[idx] = { name, value };
        count() = idx + 1;
    }

    static bool resolve(const String& key, int& out) {
        // Caso 1: numero
        if (key.length() > 0 && isDigit(key[0])) {
            out = key.toInt();
            return true;
        }

        // Caso 2: cerca tra le aree registrate
        for (int i = 0; i < count(); i++) {
            if (key.equals(entries()[i].name)) {
                out = entries()[i].value;
                return true;
            }
        }

        // Caso 3: errore
        LOG_EF("AreaRegistry", "Area '%s' non trovata!", key.c_str());
        return false;
    }

    static int resolve(const String& key) {
        // Caso numero
        if (key.length() > 0 && isDigit(key[0]))
            return key.toInt();

        // Caso nome simbolico
        for (int i = 0; i < count(); i++) {
            if (key.equals(entries()[i].name))
                return entries()[i].value;
        }

        LOG_EF("AreaRegistry", "Area '%s' non trovata!", key.c_str());
        return -1;
    }

    static void dump() {
        for (int i = 0; i < count(); i++) {
            LOG_IF("AreaRegistry", "%s = %d", entries()[i].name, entries()[i].value);
        }
    }

    static int maxValue() {
        int maxArea = 0;

        for (int i = 0; i < count(); i++) {
            int v = entries()[i].value;
            if (v > maxArea)
                maxArea = v;
        }

        return maxArea;
    }

}; */

/******************************************************
 * Funzione: AreaRegistry::registerArea()
 * Scopo   : Inserisce una nuova area nella tabella interna
 *           associando un nome simbolico a un valore numerico.
 * Ottimizzazioni:
 *   - Hash table con open addressing per lookup O(1)
 *   - Nessuna allocazione dinamica: usa arena statica
 *   - Copia del nome in buffer contiguo per cache locality
 *   - Collision handling lineare ultra-veloce
 *   - Controllo immediato dei valori riservati (0–9)
 ******************************************************/

class AreaRegistry {
public:
    // --- API PUBBLICA (minima, pulita, stabile) ---
    static void registerArea(const char* name, uint16_t value);
    static bool resolve(const char* key, uint16_t& out);
    static uint16_t resolve(const char* key);
    static uint16_t maxValue();
    static uint16_t resolve(const String& key) {
        return resolve(key.c_str());
    }

    static bool resolve(const String& key, uint16_t& out) {
        return resolve(key.c_str(), out);
    }

    // API pubbliche per compatibilità
    static uint16_t count() {
        uint16_t c = 0;
        Entry* t = table();
        for (uint16_t i = 0; i < TABLE_SIZE; i++)
            if (t[i].used) c++;
        return c;
    }

    // Restituisce valore area per indice logico
    static uint16_t getValueByIndex(uint16_t index) {
        uint16_t c = 0;
        Entry* t = table();

        for (uint16_t i = 0; i < TABLE_SIZE; i++) {
            if (!t[i].used) continue;
            if (c == index)
                return t[i].value;
            c++;
        }
        return 0xFFFF; // non trovato
    }

    // Restituisce nome area per indice logico
    static const char* getNameByIndex(uint16_t index) {
        uint16_t c = 0;
        Entry* t = table();

        for (uint16_t i = 0; i < TABLE_SIZE; i++) {
            if (!t[i].used) continue;
            if (c == index)
                return t[i].name;
            c++;
        }
        return nullptr;
    }
private:
    // --- IMPLEMENTAZIONE INTERNA (veloce, nascosta) ---

    struct Entry {
        const char* name;
        uint16_t value;
        bool used;
    };

    static constexpr uint16_t TABLE_SIZE = 1024;
    static constexpr size_t ARENA_SIZE = 8192;

    static Entry* table() {
        static Entry t[TABLE_SIZE];
        return t;
    }

    static char* arena() {
        static char buf[ARENA_SIZE];
        return buf;
    }

    static size_t& arenaOffset() {
        static size_t off = 0;
        return off;
    }

    static uint16_t hash(const char* s) {
        uint16_t h = 5381;
        while (*s) h = ((h << 5) + h) + *s++;
        return h % TABLE_SIZE;
    }

    static const char* storeName(const char* name) {
        size_t len = strlen(name) + 1;
        size_t& off = arenaOffset();

        if (off + len >= ARENA_SIZE) {
            LOG_EF("AreaRegistry", "Arena piena!");
            return "";
        }

        char* dest = arena() + off;
        memcpy(dest, name, len);
        off += len;
        return dest;
    }
};

// --------------------------------------------------
// IMPLEMENTAZIONE PUBBLICA
// --------------------------------------------------

inline void AreaRegistry::registerArea(const char* name, uint16_t value) {

    // Controllo range riservato
    if (value <= 9) {
        LOG_EF("AreaRegistry", "Area '%s' usa valore riservato %u!", name, value);
    }

    uint16_t idx = hash(name);
    Entry* t = table();

    for (uint16_t i = 0; i < TABLE_SIZE; i++) {
        uint16_t pos = (idx + i) % TABLE_SIZE;

        if (!t[pos].used) {
            t[pos].name  = storeName(name);
            t[pos].value = value;
            t[pos].used  = true;
            return;
        }
    }

    LOG_EF("AreaRegistry", "Hash table piena!");
}

inline bool AreaRegistry::resolve(const char* key, uint16_t& out) {

    // Caso numero
    if (key[0] >= '0' && key[0] <= '9') {
        out = atoi(key);
        return true;
    }

    uint16_t idx = hash(key);
    Entry* t = table();

    for (uint16_t i = 0; i < TABLE_SIZE; i++) {
        uint16_t pos = (idx + i) % TABLE_SIZE;

        if (!t[pos].used)
            return false;

        if (strcmp(t[pos].name, key) == 0) {
            out = t[pos].value;
            return true;
        }
    }

    return false;
}

inline uint16_t AreaRegistry::resolve(const char* key) {
    uint16_t out;
    return resolve(key, out) ? out : 0xFFFF;
}

inline uint16_t AreaRegistry::maxValue() {
    uint16_t maxV = 0;
    Entry* t = table();

    for (uint16_t i = 0; i < TABLE_SIZE; i++) {
        if (t[i].used && t[i].value > maxV)
            maxV = t[i].value;
    }
    return maxV;
}


#define DEFINE_AREA(name, value) \
    static const int name = value; \
    static struct AutoReg_##name { \
        AutoReg_##name() { AreaRegistry::registerArea(#name, value); } \
    } autoRegInstance_##name;

/* ============================================================
   AsyncScheduler (Generic Version)
   ============================================================ */
class AsyncScheduler {
public:
    typedef bool (*FunctionPointer)(void* ctx);
    typedef bool (*ConditionFunction)(void* ctx);
    typedef void (*CompletionCallback)();

    enum StepType { NORMAL_STEP, BRANCH_STEP };

    struct Step {
        StepType type = NORMAL_STEP;

        FunctionPointer fnc = nullptr;
        unsigned long delayAfterMs = 0;

        ConditionFunction condition = nullptr;
        int thenStep = -1;
        int elseStep = -1;

        ConditionFunction skipIf = nullptr;

        String description;
    };

    struct Job {
        std::vector<Step> steps;
        int currentStep = 0;
        bool active = false;
        bool cancelled = false;
        unsigned long nextRunTime = 0;
        int priority = 0;
        CompletionCallback onComplete = nullptr;
        uint8_t priorityWeight = 1;   // default: alta priorità
        uint8_t counter = 0;          // interno
        String name = "";
    };

protected:
    void* context = nullptr;
    std::vector<Job> jobs;

public:
    AsyncScheduler() = default;
    
    void setContext(void* ctx) {
        context = ctx;
    }

    int addJob(const Job& job, const String& name) {
        Job j = job;
        j.name = name;
        jobs.push_back(j);
        sortJobsByPriority();
        return jobs.size() - 1;
    }

    bool startJob(size_t index, unsigned long now) {
        if (index >= jobs.size()) return false;

        Job& job = jobs[index];

        if (job.active && !job.cancelled) {
            LOG_IF("AsyncScheduler", "startJob failed: job already running");
            return false;
        }

        job.active = true;
        job.cancelled = false;
        job.currentStep = 0;
        job.nextRunTime = now;
        return true;
    }

    void cancelJob(size_t index) {
        if (index >= jobs.size()) return;
        jobs[index].cancelled = true;
        jobs[index].active = false;
    }

    const std::vector<Job>& getJobs() const {
        return jobs;
    }

    void run(unsigned long now) {
        for (auto& job : jobs) {
            if (!job.active || job.cancelled) continue;

            // PRIORITÀ: esegui solo quando counter == 0
            if (job.counter > 0) {
                job.counter--;
                continue;
            }

            // Se c’è un delay temporale, rispettalo
            if (now < job.nextRunTime) continue;

            if (job.currentStep >= (int)job.steps.size()) {
                job.active = false;
                if (job.onComplete) job.onComplete();
                continue;
            }

            Step& step = job.steps[job.currentStep];

            // Branch step
            if (step.type == BRANCH_STEP) {
                bool result = step.condition ? step.condition(context) : false;

                auto valid = [&](int idx) {
                    return idx >= 0 && idx < (int)job.steps.size();
                };

                job.currentStep = result ?
                    (valid(step.thenStep) ? step.thenStep : job.currentStep + 1) :
                    (valid(step.elseStep) ? step.elseStep : job.currentStep + 1);

                job.nextRunTime = now;
                job.counter = job.priorityWeight;   // reset priorità
                continue;
            }

            // Skip
            if (step.skipIf && step.skipIf(context)) {
                job.currentStep++;
                job.nextRunTime = now;
                job.counter = job.priorityWeight;
                continue;
            }

            // Normal step
            bool done = step.fnc(context);

            if (done) {
                job.nextRunTime = now + step.delayAfterMs;
                job.currentStep++;
            } else {
                job.nextRunTime = now + 1;
            }

            job.counter = job.priorityWeight;   // reset priorità
        }
    }


protected:
    void sortJobsByPriority() {
        std::sort(jobs.begin(), jobs.end(),
            [](const Job& a, const Job& b) {
                return a.priority > b.priority;
            }
        );
    }
};
#endif
