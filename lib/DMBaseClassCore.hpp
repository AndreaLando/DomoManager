#ifndef DMBaseClassCore_HPP
#define DMBaseClassCore_HPP

#pragma once

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

#include "DMSignal.hpp"
#include "DMLogger.hpp"
#include "DMOptaRTC.hpp"

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

class TimeManager {
public:
    TimeManager()
        : lastEpoch(0),
          lastMillis(millis())   // ⭐ fondamentale
    {}

    void update() {
        time_t e = rtc.getEpoch();

        if (rtc.isTimeSynced() && e != 0) {
            lastEpoch = e;
            lastMillis = millis();
        }
    }

    void updateFromEpoch(uint32_t epoch) {
        rtc.applyEpoch(epoch);   // allinea OptaRTC
        lastEpoch = epoch;       // aggiorna fallback
        lastMillis = millis();
    }

    time_t getEpoch() {
        time_t e = rtc.getEpoch();

        if (rtc.isTimeSynced() && e != 0) {
            lastEpoch = e;
            lastMillis = millis();
            return e;
        }

        return lastEpoch + (millis() - lastMillis) / 1000;
    }

    bool getDateTime(struct tm &t) {
        time_t e = getEpoch();
        if (e == 0) return false;

        gmtime_r(&e, &t);
        return rtc.isTimeSynced();
    }

    unsigned long nowMs() {
        return millis();
    }

    bool isRTCValid() const {
        return rtc.isTimeSynced();
    }

    OptaRTC& getRTC() { return rtc; }

private:
    OptaRTC rtc;
    time_t lastEpoch;
    unsigned long lastMillis;
};

struct GenericSensorConfig {
    enum class Type {
        ANALOG,
        DIGITAL,
        BUFFER,
        CONSTANT
    };

    Type  type;
    int   value;      // pin / bufferArea / constant
    float scale = 1.0f;
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

        // 🔥 Ripristino costruttore a 4 parametri
        constexpr LedPins(int r, int w, int p, int e)
            : read(r), write(w), panel(p), err(e) {}

        constexpr LedPins() = default;
    };

    enum Channel : uint8_t {
        ONE = 0, TWO = 1, THREE = 2, FOUR = 3, CHANNEL_COUNT = 4
    };

    LedController() = default;

    LedController(const LedPins& pins, bool activeLow = false)
        : m_pins(pins), m_activeLow(activeLow) {}

    void begin() {
        if (m_initialized) return;

        validRead  = (m_pins.read  != NO_PIN);
        validWrite = (m_pins.write != NO_PIN);
        validPanel = (m_pins.panel != NO_PIN);
        validErr   = (m_pins.err   != NO_PIN);

        if (validRead)  pinMode(m_pins.read,  OUTPUT);
        if (validWrite) pinMode(m_pins.write, OUTPUT);
        if (validPanel) pinMode(m_pins.panel, OUTPUT);
        if (validErr)   pinMode(m_pins.err,   OUTPUT);

        m_initialized = true;
    }

    // 🔥 Compatibilità: hasChannel()
    inline bool hasChannel(Channel ch) const {
        switch (ch) {
            case ONE:   return validRead;
            case TWO:   return validWrite;
            case THREE: return validPanel;
            case FOUR:  return validErr;
            default:    return false;
        }
    }

    // 🔥 Compatibilità: setAll()
    inline void setAll(bool on) {
        if (validRead)  set(ONE,   on);
        if (validWrite) set(TWO,   on);
        if (validPanel) set(THREE, on);
        if (validErr)   set(FOUR,  on);
    }

    // 🔥 Compatibilità: update()
    inline void update(unsigned long /*now*/) {
        // Se in futuro vuoi fare blinking o animazioni, qui è il posto giusto.
        // Per ora è un NO-OP veloce.
    }

    inline void set(Channel ch, bool on)
    {
        const uint8_t idx = static_cast<uint8_t>(ch);

        if (idx >= CHANNEL_COUNT)
            return;

        const int pin = getPin(ch);
        if (pin == NO_PIN)
            return;

        m_blink[idx].active = false;

        digitalWrite(pin, (on ^ m_activeLow) ? HIGH : LOW);
    }

    inline void toggle(Channel ch)
    {
        const uint8_t idx = static_cast<uint8_t>(ch);

        if (idx >= CHANNEL_COUNT)
            return;

        const int pin = getPin(ch);
        if (pin == NO_PIN)
            return;

        m_blink[idx].active = false;

        bool current = digitalRead(pin);

        digitalWrite(
            pin,
            (!current ^ m_activeLow) ? HIGH : LOW
        );
    }

    inline void blink(Channel ch,
                  unsigned long onTime,
                  unsigned long offTime,
                  unsigned long now)
    {
        const uint8_t idx = static_cast<uint8_t>(ch);

        if (idx >= CHANNEL_COUNT)
            return;

        const int pin = getPin(ch);
        if (pin == NO_PIN)
            return;

        BlinkState& state = m_blink[idx];

        // Prima chiamata
        if (!state.active)
        {
            state.active = true;
            state.on = true;
            state.lastChange = now;

            digitalWrite(pin, m_activeLow ? LOW : HIGH);
            return;
        }

        const unsigned long interval = state.on ? onTime : offTime;

        if (now - state.lastChange < interval)
            return;

        state.lastChange = now;
        state.on = !state.on;

        digitalWrite(
            pin,
            (state.on ^ m_activeLow) ? HIGH : LOW
        );
    }
private:
    LedPins m_pins;
    bool    m_activeLow = false;
    bool    m_initialized = false;

    bool validRead  = false;
    bool validWrite = false;
    bool validPanel = false;
    bool validErr   = false;

    inline int getPin(Channel ch) const {
        switch (ch) {
            case ONE:   return validRead  ? m_pins.read  : NO_PIN;
            case TWO:   return validWrite ? m_pins.write : NO_PIN;
            case THREE: return validPanel ? m_pins.panel : NO_PIN;
            case FOUR:  return validErr   ? m_pins.err   : NO_PIN;
            default:    return NO_PIN;
        }
    }

    struct BlinkState
    {
        unsigned long lastChange = 0;
        bool active = false;
        bool on = false;
    };

    BlinkState m_blink[CHANNEL_COUNT];
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
