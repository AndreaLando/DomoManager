#ifndef DMBaseClassCore_HPP
#define DMBaseClassCore_HPP

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑07‑24
   Note:
                    • Nessuna

   ============================================================================ */

#include <Arduino.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#pragma once

#include "DMSignal.hpp"
#include "DMLogger.hpp"
#include "DMPlatform.hpp"

enum Priority {
  Low=0, 
  Normal=1,
  Medium=2, 
  High=3 
};

constexpr int PriorityCount =
    static_cast<int>(Priority::High) + 1;

 typedef struct {
    int deviceIndex;
    Priority priority;
  }PriorityMgmt;


// ============================================================================
// CHANGE RATE MONITOR
//
// Conta le variazioni effettive senza std::sort e senza vector<RateEntry>.
// Mantiene un report compatto ordinato con inserimento lineare.
// ============================================================================
class ChangeRateMonitor
{
public:
    struct RateEntry
    {
        int area = -1;
        uint32_t count = 0;
        uint32_t ratePerSecond = 0;
    };

private:
    static constexpr uint32_t REPORT_INTERVAL_MS = 1000UL;
    static constexpr uint32_t HIGH_RATE_THRESHOLD = 25UL;
    static constexpr size_t MAX_REPORT_ENTRIES = 8U;

    std::vector<uint32_t> counters;
    std::vector<int> activeAreas;
    RateEntry report[MAX_REPORT_ENTRIES];
    size_t reportCount = 0;
    uint32_t lastCheck = 0;
    bool initialized = false;

public:
    explicit ChangeRateMonitor(size_t areaCount = 0)
        : counters(areaCount, 0)
    {
        activeAreas.reserve(areaCount);
    }

    inline void onChanged(int area)
    {
        if (area < 0)
            return;

        const size_t index =
            static_cast<size_t>(area);

        if (index >= counters.size())
            return;

        uint32_t& count = counters[index];

        if (count == 0)
            activeAreas.push_back(area);

        if (count != UINT32_MAX)
            ++count;
    }

    void tick(uint32_t now)
    {
        if (!initialized)
        {
            initialized = true;
            lastCheck = now;
            return;
        }

        const uint32_t elapsed =
            now - lastCheck;

        if (elapsed < REPORT_INTERVAL_MS)
            return;

        reportCount = 0;

        for (size_t activeIndex = 0;
             activeIndex < activeAreas.size();
             ++activeIndex)
        {
            const int area =
                activeAreas[activeIndex];

            uint32_t& counter =
                counters[static_cast<size_t>(area)];

            const uint32_t count = counter;
            counter = 0;

            if (count == 0)
                continue;

            const uint32_t ratePerSecond =
                calculateRatePerSecond(count, elapsed);

            if (ratePerSecond <= HIGH_RATE_THRESHOLD)
                continue;

            RateEntry entry;
            entry.area = area;
            entry.count = count;
            entry.ratePerSecond = ratePerSecond;

            insertReportEntry(entry);
        }

        activeAreas.clear();
        lastCheck = now;

        for (size_t index = 0;
             index < reportCount;
             ++index)
        {
            const RateEntry& entry = report[index];

            LOG_WF(
                "RATE_MONITOR",
                "Area=%d rate=%lu/s changes=%lu window=%lu ms",
                entry.area,
                static_cast<unsigned long>(entry.ratePerSecond),
                static_cast<unsigned long>(entry.count),
                static_cast<unsigned long>(elapsed)
            );
        }
    }

    inline size_t getReportCount() const
    {
        return reportCount;
    }

    inline const RateEntry* getReport() const
    {
        return report;
    }

private:
    static uint32_t calculateRatePerSecond(
        uint32_t count,
        uint32_t elapsed)
    {
        if (elapsed == 0)
            return 0;

        return static_cast<uint32_t>(
            (static_cast<uint64_t>(count) * 1000ULL) /
            elapsed
        );
    }

    void insertReportEntry(
        const RateEntry& entry)
    {
        size_t insertPosition = 0;

        while (insertPosition < reportCount &&
               report[insertPosition].ratePerSecond >=
                   entry.ratePerSecond)
        {
            ++insertPosition;
        }

        if (reportCount >= MAX_REPORT_ENTRIES &&
            insertPosition >= MAX_REPORT_ENTRIES)
        {
            return;
        }

        size_t destination;

        if (reportCount < MAX_REPORT_ENTRIES)
        {
            destination = reportCount;
            ++reportCount;
        }
        else
        {
            destination = MAX_REPORT_ENTRIES - 1;
        }

        while (destination > insertPosition)
        {
            report[destination] = report[destination - 1];
            --destination;
        }

        report[insertPosition] = entry;
    }
};

class OwnerManager {
public:
    enum Mode {
        OWNER,
        GUEST,
        DEVELOPER
    };


    enum Reason {
        USER_REQUEST,
        IOT_REQUEST,
        TIME_EXPIRED,
        SECURITY_EVENT,
        SYSTEM_REBOOT
    };

    using ModeChangedCallback = void (*)(Mode oldMode, Mode newMode, Reason reason);

private:
    Mode currentMode = OWNER;
    unsigned long guestExpireAt = 0;
    bool logsPaused = false;
    bool initialized = false;   // 🔥 evita reset involontario al primo ciclo

    ModeChangedCallback onModeChanged = nullptr;

    // IOT integration
    bool iotGuestRequest = false;
    unsigned long iotGuestDuration = 0;

    // ---------------------------------------------------------
    // INTERNAL HELPERS
    // ---------------------------------------------------------
    const char* modeToStr(Mode m) const {
        switch (m) {
            case OWNER:     return "OWNER";
            case GUEST:     return "GUEST";
            case DEVELOPER: return "DEVELOPER";
        }
        return "UNKNOWN";
    }


    const char* reasonToStr(Reason r) const {
        switch (r) {
            case USER_REQUEST:   return "USER_REQUEST";
            case IOT_REQUEST:    return "IOT_REQUEST";
            case TIME_EXPIRED:   return "TIME_EXPIRED";
            case SECURITY_EVENT: return "SECURITY_EVENT";
            case SYSTEM_REBOOT:  return "SYSTEM_REBOOT";
        }
        return "UNKNOWN";
    }

    void triggerCallback(Mode oldMode, Mode newMode, Reason reason) {
        LOG_DF("OwnerManager",
            "Cambio modalità: %s → %s (reason=%s)",
            modeToStr(oldMode),
            modeToStr(newMode),
            reasonToStr(reason));

        if (onModeChanged)
            onModeChanged(oldMode, newMode, reason);
    }

public:
    OwnerManager() {}

    // ---------------------------------------------------------
    // EVENT HOOKS
    // ---------------------------------------------------------
    void setCallback(ModeChangedCallback cb) {
        onModeChanged = cb;
        LOG_DF("OwnerManager", "Callback registrato");
    }

    bool hasCallback() const {
        return onModeChanged != nullptr;
    }

    // ---------------------------------------------------------
    // SET MODE
    // ---------------------------------------------------------
    void setOwner(Reason reason = USER_REQUEST) {
        if (currentMode == OWNER) return;

        Mode old = currentMode;
        currentMode = OWNER;

        triggerCallback(old, OWNER, reason);
    }

    void setGuest(unsigned long expireTimestamp, Reason reason = USER_REQUEST) {
        Mode old = currentMode;
        currentMode = GUEST;
        guestExpireAt = expireTimestamp;

        LOG_DF("OwnerManager",
               "Set mode → GUEST (expire in %lu ms)",
               (expireTimestamp > millis()) ? (expireTimestamp - millis()) : 0);

        triggerCallback(old, GUEST, reason);
    }

    void setDeveloper(Reason reason = USER_REQUEST) {
        if (currentMode == DEVELOPER) return;

        Mode old = currentMode;
        currentMode = DEVELOPER;

        // 🔥 Developer = log sempre attivi
        LogManager::enable();

        triggerCallback(old, DEVELOPER, reason);
    }

    // ---------------------------------------------------------
    // EXPIRATION / UPDATE
    // ---------------------------------------------------------
    bool isGuestExpired(unsigned long now) const {
        return (currentMode == GUEST) && (now > guestExpireAt);
    }

    void update(unsigned long now) {
        // 🔥 Primo ciclo → non fare nulla
        if (!initialized) {
            initialized = true;
            return;
        }

        if (currentMode == GUEST && now > guestExpireAt) {
            LOG_DF("OwnerManager", "Guest scaduto → ritorno a OWNER");
            setOwner(TIME_EXPIRED);
        }

        if (iotGuestRequest) {
            LOG_DF("OwnerManager",
                "Richiesta IOT → attivo Guest per %lu ms",
                iotGuestDuration);

            iotGuestRequest = false;
            setGuest(now + iotGuestDuration, IOT_REQUEST);
        }
    }


    bool hasPendingIOTRequest() const {
        return iotGuestRequest;
    }

    unsigned long getPendingIOTDuration() const {
        return iotGuestDuration;
    }

    // ---------------------------------------------------------
    // PERMISSIONS
    // ---------------------------------------------------------
    bool canModifySettings() const {
        return currentMode == OWNER;
    }

    bool canControlAllDevices() const {
        return currentMode == OWNER;
    }

    bool canAccessLogs() const {
        return currentMode == OWNER;
    }

    bool canUseAutomation() const {
        return currentMode == OWNER;
    }

    bool areLogsPaused() const {
        return logsPaused;
    }

    // ---------------------------------------------------------
    // ACCESSORS
    // ---------------------------------------------------------
    const char* getModeName() const {
        return modeToStr(currentMode);
    }

    Mode getMode() const {
        return currentMode;
    }

    unsigned long getGuestExpireAt() const {
        return guestExpireAt;
    }

    // ---------------------------------------------------------
    // LOGS
    // ---------------------------------------------------------
    void pauseLogs() {
        logsPaused = true;
        LogManager::disable();
    }

    void resumeLogs() {
        logsPaused = false;

        if (currentMode == DEVELOPER)
            LogManager::enable();
    }
};

struct GenericSensorConfig {
    enum class Type {
        ANALOGIC,
        DIGITAL,
        BUFFER,
        RUNTIME,
        CONSTANT
    };

    Type  type;
    int   value;      // pin / bufferArea / runtimeHandle / constant
    float scale = 1.0f;
    bool reverse = false; // false: split, true: compact
};

// ============================================================================
// VALUE ACCESS
//
// Interfaccia neutra condivisa con Peer. Non dipende da implementazioni concrete
// concrete. DomoManager fornisce la propria implementazione concreta.
// ============================================================================
class DMValueAccess
{
public:
    virtual ~DMValueAccess() = default;

    virtual int areaCount() const = 0;
    virtual long readAreaFast(int area) const = 0;
    virtual int areaToWrite(int area) const = 0;
    virtual bool isReservedArea(int area) const = 0;
};

// ============================================================================
// GENERIC SENSOR
// ============================================================================
class GenericSensor
{
public:
    using Config = GenericSensorConfig;

    static int readRaw(
        const Config& cfg,
        DMValueAccess& values)
    {
        switch (cfg.type)
        {
            case Config::Type::ANALOGIC:
                return analogRead(cfg.value);

            case Config::Type::DIGITAL:
                return digitalRead(cfg.value);

            case Config::Type::BUFFER:
                return static_cast<int>(
                    values.readAreaFast(cfg.value)
                );

            case Config::Type::CONSTANT:
                return cfg.value;

            case Config::Type::RUNTIME:
                return 0;
        }

        return 0;
    }

    static float read(
        const Config& cfg,
        DMValueAccess& values)
    {
        return static_cast<float>(
            readRaw(cfg, values)
        ) * cfg.scale;
    }
};

// ============================================================================
// FALLING EDGE RESET MANAGER
//
// Contenitore dinamico della configurazione Falling Edge Reset.
// Storage runtime piatto e gestione omogenea a BitSplitManager.
// Input e output utilizzano GenericSensor::Config.
// ============================================================================
class FallingEdgeResetManager
{
public:
    using Config = GenericSensor::Config;
    using EntryIndex = size_t;

    static constexpr EntryIndex INVALID_ENTRY =
        static_cast<EntryIndex>(-1);

    struct Output
    {
        Config target;
    };

    struct Entry
    {
        Config input;
        size_t firstOutput = 0;
        size_t outputCount = 0;
        bool initialized = false;
        bool previousState = false;
    };

private:
    std::vector<Entry> entries;
    std::vector<Output> outputs;
    bool configurationClosed = false;

public:
    FallingEdgeResetManager() = default;

    FallingEdgeResetManager(
        const FallingEdgeResetManager&) = delete;

    FallingEdgeResetManager& operator=(
        const FallingEdgeResetManager&) = delete;

    FallingEdgeResetManager(
        FallingEdgeResetManager&&) = default;

    FallingEdgeResetManager& operator=(
        FallingEdgeResetManager&&) = default;

    void beginConfiguration(
        size_t expectedEntries,
        size_t expectedOutputs)
    {
        if (configurationClosed)
            return;

        entries.reserve(expectedEntries);
        outputs.reserve(expectedOutputs);
    }

    EntryIndex add(
        const Config& input)
    {
        if (configurationClosed ||
            !isValidInput(input))
        {
            return INVALID_ENTRY;
        }

        Entry entry;
        entry.input = input;
        entry.firstOutput = outputs.size();
        entry.outputCount = 0;
        entry.initialized = false;
        entry.previousState = false;

        entries.push_back(entry);

        return entries.size() - 1;
    }

    bool add(
        EntryIndex entryIndex,
        const Config& target)
    {
        if (configurationClosed ||
            entryIndex >= entries.size() ||
            entryIndex != entries.size() - 1 ||
            !isValidTarget(target))
        {
            return false;
        }

        outputs.push_back({ target });
        ++entries[entryIndex].outputCount;

        return true;
    }

    void endConfiguration()
    {
        configurationClosed = true;
    }

    bool isConfigurationClosed() const
    {
        return configurationClosed;
    }

    size_t entryCount() const
    {
        return entries.size();
    }

    size_t outputCount() const
    {
        return outputs.size();
    }

    const Entry* getEntry(
        EntryIndex index) const
    {
        return index < entries.size()
            ? &entries[index]
            : nullptr;
    }

    const Output* getOutput(
        EntryIndex entryIndex,
        size_t outputIndex) const
    {
        const Entry* entry =
            getEntry(entryIndex);

        if (!entry ||
            outputIndex >= entry->outputCount)
        {
            return nullptr;
        }

        const size_t absoluteIndex =
            entry->firstOutput + outputIndex;

        return absoluteIndex < outputs.size()
            ? &outputs[absoluteIndex]
            : nullptr;
    }

    bool handlesArea(
        int area) const
    {
        for (const Entry& entry : entries)
        {
            if (entry.input.value == area)
                return true;
        }

        return false;
    }

    template<typename Writer>
    void processEvent(
        int area,
        long value,
        Writer&& writer)
    {
        const bool currentState =
            value != 0;

        for (EntryIndex entryIndex = 0;
             entryIndex < entries.size();
             ++entryIndex)
        {
            Entry& entry =
                entries[entryIndex];

            if (entry.input.value != area)
                continue;

            if (!entry.initialized)
            {
                entry.initialized = true;
                entry.previousState = currentState;
                continue;
            }

            const bool fallingEdge =
                entry.previousState &&
                !currentState;

            entry.previousState = currentState;

            if (!fallingEdge)
                continue;

            for (size_t outputIndex = 0;
                 outputIndex < entry.outputCount;
                 ++outputIndex)
            {
                const Output* output =
                    getOutput(entryIndex, outputIndex);

                if (!output)
                    continue;

                writer(output->target, 0L);
            }
        }
    }

private:
    static bool isValidInput(
        const Config& input)
    {
        return
            input.type == Config::Type::BUFFER ||
            input.type == Config::Type::RUNTIME;
    }

    static bool isValidTarget(
        const Config& target)
    {
        return
            target.type == Config::Type::BUFFER ||
            target.type == Config::Type::RUNTIME ||
            target.type == Config::Type::DIGITAL;
    }
};

// ============================================================================
// BIT SPLIT MANAGER - PATCH 2
//
// Contenitore dinamico della configurazione di sbittamento.
// Questa patch NON collega ancora il manager a DMRuntime, Buffer,
// DMRuntimeStore o EventManager e non crea istanze globali.
//
// Le allocazioni avvengono esclusivamente quando beginConfiguration()/add()
// vengono invocati esplicitamente dal futuro codice di setup.
// ============================================================================
class BitSplitManager
{
public:
    using Config = GenericSensor::Config;
    using SplitIndex = size_t;

    static constexpr SplitIndex INVALID_SPLIT =
        static_cast<SplitIndex>(-1);

    struct Output
    {
        Config target;
        uint8_t bitIndex = 0;
        bool inverted = false;
    };

    struct Split
    {
        Config source;
        bool reverse = false;
        size_t firstOutput = 0;
        size_t outputCount = 0;
    };

private:
    std::vector<Split> splits;
    std::vector<Output> outputs;
    bool configurationClosed = false;

public:
    BitSplitManager() = default;

    BitSplitManager(const BitSplitManager&) = delete;
    BitSplitManager& operator=(const BitSplitManager&) = delete;

    BitSplitManager(BitSplitManager&&) = default;
    BitSplitManager& operator=(BitSplitManager&&) = default;

    void beginConfiguration(
        size_t expectedSplits,
        size_t expectedOutputs)
    {
        if (configurationClosed)
            return;

        splits.reserve(expectedSplits);
        outputs.reserve(expectedOutputs);
    }

    SplitIndex add(
        const Config& source,
        bool reverse)
    {
        if (configurationClosed ||
            !isValidSource(source))
        {
            return INVALID_SPLIT;
        }

        Split split;

        split.source = source;
        split.reverse = reverse;
        split.firstOutput = outputs.size();
        split.outputCount = 0;

        splits.push_back(split);

        return splits.size() - 1;
    }


    bool add(
        SplitIndex splitIndex,
        const Config& target,
        uint8_t bitIndex,
        bool inverted = false)
    {
        if (configurationClosed ||
            splitIndex >= splits.size() ||
            bitIndex >= 32 ||
            !isValidTarget(target))
        {
            return false;
        }

        // Le uscite di ogni split devono essere aggiunte consecutivamente.
        // Questo mantiene tutte le destinazioni in un unico vettore compatto.
        if (splitIndex != splits.size() - 1)
            return false;

        outputs.push_back({
            target,
            bitIndex,
            inverted
        });

        ++splits[splitIndex].outputCount;
        return true;
    }

    void endConfiguration()
    {
        configurationClosed = true;
    }

    bool isConfigurationClosed() const
    {
        return configurationClosed;
    }

    size_t splitCount() const
    {
        return splits.size();
    }

    size_t outputCount() const
    {
        return outputs.size();
    }

    const Split* getSplit(SplitIndex index) const
    {
        return index < splits.size()
            ? &splits[index]
            : nullptr;
    }

    const Output* getOutput(
        SplitIndex splitIndex,
        size_t outputIndex) const
    {
        const Split* split = getSplit(splitIndex);

        if (!split || outputIndex >= split->outputCount)
            return nullptr;

        const size_t absoluteIndex =
            split->firstOutput + outputIndex;

        return absoluteIndex < outputs.size()
            ? &outputs[absoluteIndex]
            : nullptr;
    }

    bool handles(const Config& source) const
    {
        for (const Split& split : splits)
        {
            if (sameElement(split.source, source))
                return true;
        }

        return false;
    }

private:
    static bool isValidSource(const Config& source)
    {
        return
            source.type == Config::Type::BUFFER ||
            source.type == Config::Type::RUNTIME;
    }

    static bool isValidTarget(const Config& target)
    {
        return
            target.type == Config::Type::BUFFER ||
            target.type == Config::Type::RUNTIME ||
            target.type == Config::Type::DIGITAL;
    }

    static bool sameElement(
        const Config& left,
        const Config& right)
    {
        return
            left.type == right.type &&
            left.value == right.value;
    }
};

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

    inline T get() const {
        return value;
    }

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

class AnalogThresholdManager {
public:
    struct Item {
        int area;
        int thresholdLow;
        int thresholdHigh;
    };

private:
    // Limiti statici
    static constexpr int MAX_ITEMS = 64;
    static constexpr int MAX_AREAS = 1024;

    // Storage compatto
    Item items[MAX_ITEMS];
    uint8_t count = 0;

    // Lookup diretto area → indice
    int indexByArea[MAX_AREAS];

    // Fallback threshold
    int fallbackThreshold = 20;

public:
    AnalogThresholdManager() {
        // Inizializzazione veloce (memset è più rapido)
        memset(indexByArea, -1, sizeof(indexByArea));
    }

    // ---------------------------------------------------------
    // ADD — O(1), nessuna allocazione
    // ---------------------------------------------------------
    inline void add(int area, int low, int high = -1) {
        if (count >= MAX_ITEMS) {
            LOG_WF("AnalogThresholdManager", "MAX_ITEMS superato (%u)", MAX_ITEMS);
            return;
        }

        if (area < 0 || area >= MAX_AREAS) {
            LOG_WF("AnalogThresholdManager", "Area fuori range: %d", area);
            return;
        }

        items[count] = { area, low, high };
        indexByArea[area] = count;
        count++;
    }

    // ---------------------------------------------------------
    // GET — lookup O(1) reale
    // ---------------------------------------------------------
    inline const Item* get(int area) const {
        if ((unsigned)area >= MAX_AREAS) return nullptr;
        int idx = indexByArea[area];
        return (idx >= 0) ? &items[idx] : nullptr;
    }

    // ---------------------------------------------------------
    // THRESHOLD — hot-path ottimizzato
    // ---------------------------------------------------------
    inline int getThreshold(int area, long value) const {
        const Item* it = get(area);
        if (!it)
            return fallbackThreshold;

        // Branch predictabile: high threshold usato raramente
        if (it->thresholdHigh > 0 && value > 1000)
            return it->thresholdHigh;

        return it->thresholdLow;
    }

    // ---------------------------------------------------------
    // Fallback configurabile
    // ---------------------------------------------------------
    inline void setFallback(int t) { fallbackThreshold = t; }
    inline int getFallback() const { return fallbackThreshold; }
};


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

        //LOG_DF("SplitOutManager", "Added split area=%d maxTime=%lu", mainArea, maxTime);
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
        //LOG_DF("TIMING", "[SPLIT_START] main=%d time=%lu", areaRead, millis());

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
        //LOG_DF("TIMING", "[SPLIT_END] main=%d time=%lu", areaRead, millis());
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
        //LOG_DF("SplitOutManager", "IsRunning area=%d → %d", areaRead, r);
        return r;
    }
};

// ============================================================================
// PULSE OUTPUT MANAGER
//
// Gestisce impulsi non bloccanti su singole aree di uscita.
//
// Ogni uscita:
//   - viene attivata immediatamente con start()
//   - rimane attiva per pulseTime
//   - viene disattivata automaticamente da update()
//
// Una nuova chiamata start() durante l'impulso:
//   - mantiene l'uscita attiva
//   - riavvia il timer
// ============================================================================
class PulseOutputManager
{
public:

    struct Pulse
    {
        int area =
            -1;

        unsigned long pulseTime =
            0;
    };


    struct Item
    {
        Pulse pulse;

        bool running =
            false;

        unsigned long startedAt =
            0;
    };


    using CallbackFn =
        void (*)(
            const Pulse& pulse,
            bool active
        );


private:

    static constexpr size_t
        MAX_PULSES =
            32;

    static constexpr int
        MAX_AREAS =
            1024;


    Item items[
        MAX_PULSES
    ];


    size_t count =
        0;


    int indexByArea[
        MAX_AREAS
    ];


    uint8_t activeList[
        MAX_PULSES
    ];


    uint8_t activeCount =
        0;


    CallbackFn callback =
        nullptr;


public:

    explicit PulseOutputManager(
        CallbackFn cb)
        : callback(
              cb
          )
    {
        for (
            int area = 0;
            area < MAX_AREAS;
            ++area
        )
        {
            indexByArea[area] =
                -1;
        }
    }


    // ========================================================================
    // ADD
    // ========================================================================

    bool add(
        int area,
        unsigned long pulseTime)
    {
        if (
            area < 0 ||
            area >= MAX_AREAS
        )
        {
            LOG_WF(
                "PulseOutputManager",
                "Invalid area: %d",
                area
            );

            return false;
        }

        if (pulseTime == 0)
        {
            LOG_WF(
                "PulseOutputManager",
                "Invalid pulse time: area=%d",
                area
            );

            return false;
        }

        if (count >= MAX_PULSES)
        {
            LOG_WF(
                "PulseOutputManager",
                "MAX_PULSES exceeded: %u",
                static_cast<unsigned>(
                    MAX_PULSES
                )
            );

            return false;
        }

        if (indexByArea[area] >= 0)
        {
            LOG_WF(
                "PulseOutputManager",
                "Area already registered: %d",
                area
            );

            return false;
        }

        Item& item =
            items[count];

        item.pulse.area =
            area;

        item.pulse.pulseTime =
            pulseTime;

        item.running =
            false;

        item.startedAt =
            0;

        indexByArea[area] =
            static_cast<int>(
                count
            );

        ++count;

        return true;
    }


    // ========================================================================
    // EXISTS
    // ========================================================================

    bool exists(
        int area) const
    {
        return
            area >= 0 &&
            area < MAX_AREAS &&
            indexByArea[area] >= 0;
    }


    // ========================================================================
    // GET
    // ========================================================================

    Item* get(
        int area)
    {
        if (
            area < 0 ||
            area >= MAX_AREAS
        )
        {
            return nullptr;
        }

        const int index =
            indexByArea[area];

        if (index < 0)
        {
            return nullptr;
        }

        return
            &items[index];
    }


    const Item* get(
        int area) const
    {
        if (
            area < 0 ||
            area >= MAX_AREAS
        )
        {
            return nullptr;
        }

        const int index =
            indexByArea[area];

        if (index < 0)
        {
            return nullptr;
        }

        return
            &items[index];
    }


    // ========================================================================
    // START
    //
    // Se l'impulso è già attivo, riavvia il timeout senza
    // richiamare la callback di attivazione.
    // ========================================================================

    bool start(
        int area,
        unsigned long now)
    {
        Item* item =
            get(
                area
            );

        if (!item)
        {
            LOG_WF(
                "PulseOutputManager",
                "Start failed: area=%d not found",
                area
            );

            return false;
        }

        item->startedAt =
            now;

        if (item->running)
        {
            LOG_DF(
                "PulseOutputManager",
                "Pulse restarted: area=%d time=%lu",
                area,
                item->pulse.pulseTime
            );

            return true;
        }

        item->running =
            true;

        const int itemIndex =
            indexByArea[area];

        if (activeCount >= MAX_PULSES)
        {
            item->running =
                false;

            LOG_EF(
                "PulseOutputManager",
                "Active list full"
            );

            return false;
        }

        activeList[
            activeCount
        ] =
            static_cast<uint8_t>(
                itemIndex
            );

        ++activeCount;

        if (callback)
        {
            callback(
                item->pulse,
                true
            );
        }

        LOG_DF(
            "PulseOutputManager",
            "Pulse started: area=%d time=%lu",
            area,
            item->pulse.pulseTime
        );

        return true;
    }


    bool start(
        int area)
    {
        return start(
            area,
            millis()
        );
    }


    // ========================================================================
    // STOP
    // ========================================================================

    bool stop(
        int area)
    {
        Item* item =
            get(
                area
            );

        if (!item)
        {
            return false;
        }

        if (!item->running)
        {
            return true;
        }

        const int itemIndex =
            indexByArea[area];

        item->running =
            false;

        item->startedAt =
            0;

        removeActiveIndex(
            itemIndex
        );

        if (callback)
        {
            callback(
                item->pulse,
                false
            );
        }

        LOG_DF(
            "PulseOutputManager",
            "Pulse stopped: area=%d",
            area
        );

        return true;
    }


    // ========================================================================
    // UPDATE
    // ========================================================================

    void update(
        unsigned long now)
    {
        uint8_t activePosition =
            0;

        while (
            activePosition <
            activeCount
        )
        {
            const uint8_t itemIndex =
                activeList[
                    activePosition
                ];

            Item& item =
                items[
                    itemIndex
                ];

            if (!item.running)
            {
                activeList[
                    activePosition
                ] =
                    activeList[
                        --activeCount
                    ];

                continue;
            }

            if (
                static_cast<unsigned long>(
                    now -
                    item.startedAt
                ) <
                item.pulse.pulseTime
            )
            {
                ++activePosition;

                continue;
            }

            item.running =
                false;

            item.startedAt =
                0;

            if (callback)
            {
                callback(
                    item.pulse,
                    false
                );
            }

            LOG_DF(
                "PulseOutputManager",
                "Pulse expired: area=%d",
                item.pulse.area
            );

            activeList[
                activePosition
            ] =
                activeList[
                    --activeCount
                ];
        }
    }


    void update()
    {
        update(
            millis()
        );
    }


    // ========================================================================
    // STATUS
    // ========================================================================

    bool isRunning(
        int area) const
    {
        const Item* item =
            get(
                area
            );

        return
            item &&
            item->running;
    }


    bool hasActivePulses() const
    {
        return
            activeCount > 0;
    }


private:

    void removeActiveIndex(
        int itemIndex)
    {
        for (
            uint8_t activePosition = 0;
            activePosition < activeCount;
            ++activePosition
        )
        {
            if (
                activeList[
                    activePosition
                ] != itemIndex
            )
            {
                continue;
            }

            activeList[
                activePosition
            ] =
                activeList[
                    --activeCount
                ];

            return;
        }
    }
};

class Errors {
public:
    using StateChangedFn = std::function<void()>;

    Errors(short maxErrors, unsigned long retryWindow = 60000)
        : MAX_ERRORS(maxErrors),
          RETRY_WINDOW(retryWindow) {}

    inline void SetStateChangedCallback(StateChangedFn fn) {
        _onStateChanged = fn;
    }

    inline void SetName(const char* n) { _name = n; }

    inline bool Loop(bool inError, unsigned long now)
    {
        // Log solo quando cambia inError
        if (inError != _lastInError) {
            _lastInError = inError;
        }

        // Se la lettura è OK → il device ha risposto almeno una volta
        if (!inError && !_everSucceeded) {
            _everSucceeded = true;
        }

        // Cache locali (più veloci)
        bool error  = _error;
        bool parked = _parked;

        // 1) PARCHEGGIATO → early exit
        if (parked) {
            updateVisibleState(error, true);
            return false;
        }

        // 2) NON IN ERRORE
        if (!error) {

            if (inError) {
                if (++_cnt >= MAX_ERRORS) {
                    enterError(now);
                    LOG_DF("ERR", "[%s] ENTER INTERNAL ERROR (retryAt=%lu)", _name, _retryAt);
                    updateVisibleState(true, false);
                    return true;
                }
            } else {
                _cnt = 0;
            }

            updateVisibleState(false, false);
            return true;
        }

        // 3) IN ERRORE → retry window
        if (now >= _retryAt) {
            if (++_retryCycles >= MAX_RETRY_CYCLES) {
                _parked = true;
                LOG_DF("ERR", "[%s] ENTER PARKED", _name);
                updateVisibleState(true, true);
                return false;
            }

            LOG_DF("ERR", "[%s] CLEAR INTERNAL ERROR", _name);
            clear();
            updateVisibleState(false, false);
            return true;
        }

        // Ancora in errore, retry non scaduto
        updateVisibleState(true, false);
        return false;
    }

    inline bool IsInError() const  { return _error; }
    inline bool IsParked() const   { return _parked; }

    inline bool IsVisibleError() const  { return _visibleErrorState; }
    inline bool IsVisibleParked() const { return _visibleParkedState; }

private:

    inline void enterError(unsigned long now)
    {
        _error     = true;
        _lastError = now;
        _retryAt   = now + (RETRY_WINDOW * MAX_ERRORS);
    }

    inline void clear()
    {
        _error     = false;
        _cnt       = 0;
        _lastError = 0;
        _retryAt   = 0;
    }

    inline void updateVisibleState(bool newError, bool newParked)
    {
        // 🔥 Filtro: device mai riuscito → non può risultare OK
        if (!_everSucceeded && !newError) {
            return;
        }

        // Stato pending attuale
        bool pendingState = (_pendingErrorState || _pendingParkedState);
        bool newState     = (newError || newParked);

        // 1) Stato cambiato → inizializza pending
        if (newState != pendingState) {
            _pendingErrorState  = newError;
            _pendingParkedState = newParked;
            _pendingCount       = 1;
            return;
        }

        // 2) Stato invariato → incrementa pending
        if (++_pendingCount < 2)
            return;

        // 3) Conferma stato visibile solo se cambia
        if (_visibleErrorState != newError || _visibleParkedState != newParked) {
            _visibleErrorState  = newError;
            _visibleParkedState = newParked;

            if (_onStateChanged)
                _onStateChanged();
        }

        // Reset pending
        _pendingCount = 0;
    }


private:
    const char* _name = "dev";

    // 🔥 COSTANTI IN MAIUSCOLO
    const short         MAX_ERRORS;
    const unsigned long RETRY_WINDOW;
    const short         MAX_RETRY_CYCLES = 5;

    // Stato interno
    bool          _error       = false;
    short         _cnt         = 0;
    unsigned long _lastError   = 0;
    unsigned long _retryAt     = 0;

    bool          _parked        = false;
    short         _retryCycles   = 0;

    StateChangedFn _onStateChanged = nullptr;

    bool _visibleErrorState  = false;
    bool _visibleParkedState = false;

    bool _pendingErrorState  = false;
    bool _pendingParkedState = false;
    int  _pendingCount       = 0;

    bool _lastInError = false;

    // 🔥 Patch: un device che non ha mai risposto non può risultare OK
    bool _everSucceeded = false;
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
        int read  = NO_PIN;
        int write = NO_PIN;
        int panel = NO_PIN;
        int err   = NO_PIN;

        constexpr LedPins(
            int r,
            int w,
            int p,
            int e
        )
            : read(r),
              write(w),
              panel(p),
              err(e)
        {}

        constexpr LedPins() = default;
    };

    enum Channel : uint8_t {
        ONE = 0,
        TWO = 1,
        THREE = 2,
        FOUR = 3,
        CHANNEL_COUNT = 4
    };

    LedController() = default;

    LedController(
        const LedPins& pins,
        bool activeLow = false
    )
        : m_pins(pins),
          m_activeLow(activeLow)
    {}

    void begin()
    {
        if (m_initialized)
            return;

        validRead  = (m_pins.read  != NO_PIN);
        validWrite = (m_pins.write != NO_PIN);
        validPanel = (m_pins.panel != NO_PIN);
        validErr   = (m_pins.err   != NO_PIN);

        if (validRead)
        {
            DMPlatform::SetupOutputPin(
                m_pins.read
            );
        }

        if (validWrite)
        {
            DMPlatform::SetupOutputPin(
                m_pins.write
            );
        }

        if (validPanel)
        {
            DMPlatform::SetupOutputPin(
                m_pins.panel
            );
        }

        if (validErr)
        {
            DMPlatform::SetupOutputPin(
                m_pins.err
            );
        }

        // Stato logico iniziale di tutti i canali
        for (uint8_t i = 0; i < CHANNEL_COUNT; ++i)
        {
            m_state[i] = false;
            m_blink[i].active = false;
            m_blink[i].on = false;
            m_blink[i].lastChange = 0;
        }

        m_initialized = true;
    }

    inline bool hasChannel(Channel ch) const
    {
        switch (ch)
        {
            case ONE:
                return validRead;

            case TWO:
                return validWrite;

            case THREE:
                return validPanel;

            case FOUR:
                return validErr;

            default:
                return false;
        }
    }

    inline void setAll(bool on)
    {
        if (validRead)
            set(ONE, on);

        if (validWrite)
            set(TWO, on);

        if (validPanel)
            set(THREE, on);

        if (validErr)
            set(FOUR, on);
    }

    inline void update(unsigned long now)
    {
        for (uint8_t i = 0; i < CHANNEL_COUNT; ++i)
        {
            BlinkState& state = m_blink[i];

            if (!state.active)
                continue;

            const int pin = getPin(
                static_cast<Channel>(i)
            );

            if (pin == NO_PIN)
                continue;

            const unsigned long interval =
                state.on ? state.onTime : state.offTime;

            if ((now - state.lastChange) < interval)
                continue;

            state.lastChange = now;
            state.on = !state.on;

            m_state[i] = state.on;

            DMPlatform::WriteOutputPin(
                pin,
                state.on ^ m_activeLow
            );
        }
    }

    inline void set(
        Channel ch,
        bool on
    )
    {
        const uint8_t idx =
            static_cast<uint8_t>(ch);

        if (idx >= CHANNEL_COUNT)
            return;

        const int pin = getPin(ch);

        if (pin == NO_PIN)
            return;

        // Disabilita eventuale blinking
        m_blink[idx].active = false;

        // Salva stato logico
        m_state[idx] = on;

        DMPlatform::WriteOutputPin(
            pin,
            on ^ m_activeLow
        );
    }

    inline void toggle(Channel ch)
    {
        const uint8_t idx =
            static_cast<uint8_t>(ch);

        if (idx >= CHANNEL_COUNT)
            return;

        const int pin = getPin(ch);

        if (pin == NO_PIN)
            return;

        // Disabilita eventuale blinking
        m_blink[idx].active = false;

        // Il WS2812 non è leggibile come una normale GPIO.
        // Usiamo quindi lo stato logico memorizzato.
        m_state[idx] = !m_state[idx];

        DMPlatform::WriteOutputPin(
            pin,
            m_state[idx] ^ m_activeLow
        );
    }

    inline void blink(
        Channel ch,
        unsigned long onTime,
        unsigned long offTime,
        unsigned long now
    )
    {
        const uint8_t idx =
            static_cast<uint8_t>(ch);

        if (idx >= CHANNEL_COUNT)
            return;

        const int pin = getPin(ch);

        if (pin == NO_PIN)
            return;

        BlinkState& state = m_blink[idx];

        state.active = true;
        state.onTime = onTime;
        state.offTime = offTime;
        state.lastChange = now;
        state.on = true;

        m_state[idx] = true;

        DMPlatform::WriteOutputPin(
            pin,
            true ^ m_activeLow
        );
    }

private:
    LedPins m_pins;

    bool m_activeLow = false;
    bool m_initialized = false;

    bool validRead  = false;
    bool validWrite = false;
    bool validPanel = false;
    bool validErr   = false;

    // Stato logico dei quattro canali.
    //
    // Serve soprattutto per il WS2812:
    // il LED RGB non può essere letto con digitalRead().
    bool m_state[CHANNEL_COUNT] = {
        false,
        false,
        false,
        false
    };

    inline int getPin(Channel ch) const
    {
        switch (ch)
        {
            case ONE:
                return validRead
                    ? m_pins.read
                    : NO_PIN;

            case TWO:
                return validWrite
                    ? m_pins.write
                    : NO_PIN;

            case THREE:
                return validPanel
                    ? m_pins.panel
                    : NO_PIN;

            case FOUR:
                return validErr
                    ? m_pins.err
                    : NO_PIN;

            default:
                return NO_PIN;
        }
    }

    struct BlinkState
    {
        unsigned long lastChange = 0;
        unsigned long onTime = 0;
        unsigned long offTime = 0;

        bool active = false;
        bool on = false;
    };

    BlinkState m_blink[CHANNEL_COUNT];
};

class ButtonManager {
public:

    struct ButtonState {
        bool pressedNow = false;
        bool pressedAtStartup = false;
    };

private:

    int pin = DMPlatform::PIN_NOT_CONNECTED;

    bool startupState = false;
    bool startupConsumed = false;
    bool lastState = false;

public:

    explicit ButtonManager(int buttonPin)
        : pin(buttonPin)
    {
    }

    void begin()
    {
        // --------------------------------------------------------
        // Pulsante non presente sulla piattaforma
        // --------------------------------------------------------

        if (!DMPlatform::IsPinAvailable(pin))
        {
            startupState = false;
            startupConsumed = true;
            lastState = false;
            return;
        }

        // --------------------------------------------------------
        // Pulsante reale
        // --------------------------------------------------------

        DMPlatform::SetupInputPin(pin);

        delay(50);

        startupState =
            (DMPlatform::ReadInputPin(pin) == LOW);

        lastState =
            startupState;

        startupConsumed = false;
    }

    ButtonState update(unsigned long /*now*/)
    {
        ButtonState st;

        // --------------------------------------------------------
        // Pulsante non presente
        // --------------------------------------------------------

        if (!DMPlatform::IsPinAvailable(pin))
        {
            return st;
        }

        // --------------------------------------------------------
        // Lettura attuale
        // --------------------------------------------------------

        const bool nowState =
            (DMPlatform::ReadInputPin(pin) == LOW);

        // --------------------------------------------------------
        // Fronte di discesa
        // --------------------------------------------------------

        st.pressedNow =
            nowState && !lastState;

        // --------------------------------------------------------
        // Evento startup una sola volta
        // --------------------------------------------------------

        if (!startupConsumed && startupState)
        {
            st.pressedAtStartup = true;
            startupConsumed = true;
        }

        lastState =
            nowState;

        return st;
    }
};

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




// ============================================================================
// TOGGLE MANAGER
//
// Implementazione completa nel core. I metodi dipendenti dall accesso valori sono
// tramite DMValueAccess, senza dipendenze da implementazioni concrete.
// ============================================================================
class ToggleManager
{
public:
    struct ToggleSignalItem
    {
        ToggleSignal Toggle;
        int areaRead;
        std::vector<int> forwardsFromAreas;
    };

    struct ToggleResult
    {
        int area = -1;
        long value = 0;
        int toggleArea = -1;
    };

private:
    std::vector<ToggleSignalItem> toggles;
    std::unordered_map<int, size_t> cacheByArea;
    std::unordered_map<int, size_t> cacheByForwardArea;
    std::unordered_set<int> forwardAreasCache;

public:
    ToggleManager() = default;

    bool handlesArea(int area) const
    {
        return cacheByArea.count(area) > 0 ||
               cacheByForwardArea.count(area) > 0;
    }

    bool isForwardArea(int area) const
    {
        return forwardAreasCache.count(area) > 0;
    }

    const std::vector<ToggleSignalItem>& getAll() const
    {
        return toggles;
    }

    void add(int areaRead, std::vector<int> forwards = {})
    {
        toggles.push_back({ ToggleSignal(), areaRead, forwards });
        const size_t index = toggles.size() - 1;
        cacheByArea[areaRead] = index;

        for (int forwardArea : forwards)
        {
            forwardAreasCache.insert(forwardArea);
            cacheByForwardArea[forwardArea] = index;
        }
    }

    ToggleSignalItem* operator[](size_t index)
    {
        return index < toggles.size()
            ? &toggles[index]
            : nullptr;
    }

    size_t size() const
    {
        return toggles.size();
    }

    ToggleSignalItem* get(size_t index)
    {
        return index < toggles.size()
            ? &toggles[index]
            : nullptr;
    }

    ToggleSignalItem* get(int areaRead)
    {
        const auto it = cacheByArea.find(areaRead);
        if (it == cacheByArea.end())
            return nullptr;

        const size_t index = it->second;
        return index < toggles.size()
            ? &toggles[index]
            : nullptr;
    }

    ToggleSignalItem* getByForwardArea(int area)
    {
        const auto it = cacheByForwardArea.find(area);
        if (it == cacheByForwardArea.end())
            return nullptr;

        const size_t index = it->second;
        return index < toggles.size()
            ? &toggles[index]
            : nullptr;
    }

    int getForwardValue(
        int area,
        DMValueAccess& values)
    {
        const int maxArea =
            values.areaCount();

        ToggleSignalItem* toggle = get(area);
        if (!toggle)
            return 0;

        for (int forwardArea : toggle->forwardsFromAreas)
        {
            if (forwardArea >= 0 &&
                forwardArea < maxArea &&
                values.readAreaFast(forwardArea) > 0)
            {
                return 1;
            }
        }

        return 0;
    }

    bool processEvent(
        int area,
        long value,
        DMValueAccess& values,
        unsigned long now,
        ToggleResult& result)
    {
        (void)now;

        ToggleSignalItem* toggle = get(area);
        if (!toggle)
            toggle = getByForwardArea(area);
        if (!toggle)
            return false;

        result.toggleArea = toggle->areaRead;
        long signalIn = value;

        if (!toggle->forwardsFromAreas.empty())
        {
            signalIn =
                getForwardValue(
                    toggle->areaRead,
                    values
                );

            if (signalIn == 0)
                signalIn = value;
        }

        const int areaToWrite =
            values.areaToWrite(toggle->areaRead);

        if (areaToWrite > 0)
        {
            if (areaToWrite >=
                values.areaCount())
            {
                return false;
            }

            long toggleOut =
                values.readAreaFast(areaToWrite);

            if (!toggle->Toggle.change(
                    signalIn,
                    toggleOut))
            {
                return false;
            }

            result.area = areaToWrite;
            result.value = toggleOut;
            return true;
        }

        long toggleOut = 0;
        if (!toggle->Toggle.changeRoute(
                signalIn,
                toggleOut))
        {
            return false;
        }

        result.area = -1;
        result.value = toggleOut;
        return true;
    }
};

// ============================================================================
// ROUTE MANAGER
//
// Implementazione completa nel core tramite DMValueAccess.
// ============================================================================
class RouteManager
{
public:
    struct RouteAction
    {
        int targetArea;
        int value;
    };

    struct RouteCase
    {
        int triggerValue;
        std::vector<RouteAction> actions;
    };

    struct RouteConfig
    {
        String name;
        int triggerArea;
        std::vector<RouteCase> cases;
    };

    struct SafeRoute
    {
        std::vector<RouteCase> cases;
    };

private:
    std::unordered_map<int, SafeRoute> routes;

public:
    void load(const std::vector<RouteConfig>& config)
    {
        for (const auto& route : config)
        {
            routes[route.triggerArea] =
            {
                route.cases
            };
        }
    }

    bool hasRoute(int area) const
    {
        return routes.find(area) != routes.end();
    }

    std::vector<RouteAction> execute(
        int srcArea,
        long value,
        DMValueAccess& values) const
    {
        std::vector<RouteAction> actions;

        const auto routeIt = routes.find(srcArea);
        if (routeIt == routes.end())
            return actions;

        const SafeRoute& route = routeIt->second;
        const int bufferSize =
            values.areaCount();

        for (const RouteCase& routeCase : route.cases)
        {
            if (routeCase.triggerValue != value)
                continue;

            for (const RouteAction& action : routeCase.actions)
            {
                if (action.targetArea < 0 ||
                    action.targetArea >= bufferSize)
                {
                    LOG_WF(
                        "RouteManager",
                        "Skip route write: invalid targetArea=%d (srcArea=%d)",
                        action.targetArea,
                        srcArea
                    );
                    continue;
                }

                if (values.isReservedArea(
                        action.targetArea))
                {
                    LOG_WF(
                        "RouteManager",
                        "Skip route write: reserved targetArea=%d (srcArea=%d)",
                        action.targetArea,
                        srcArea
                    );
                    continue;
                }

                actions.push_back(action);
            }

            break;
        }

        return actions;
    }
};

#endif
