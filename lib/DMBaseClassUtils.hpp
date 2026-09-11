#ifndef DMBaseClassUtils_HPP
#define DMBaseClassUtils_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑08‑04
   Note:
                    • Nessuna

   ============================================================================ */


#include <Arduino.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "DMSignal.hpp"
#include "DMLogger.hpp"


  //Calcolatore Medie
#define NUM_VARIATIONS 5
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
        // 🔥 Serve almeno 2 variazioni → 3 campioni
        if (countVar <2) return CONSTANT;

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

/* ============================================================================
   DRIVER TIMING STATS (REFRACTOR LIGHT VERSION)
   ============================================================================
   Misura SOLO il tempo di interrogazione del singolo device (READ/WRITE).
   Nessun timing globale, nessun batch timing, nessun overhead.

   Utilizzo:
      • ModbusManager → startDevice() / endDevice()
      • Diagnostica → reportIpTiming()

   Obiettivo:
      • Identificare device lenti
      • Identificare IP lenti
      • Ottimizzare il codice Modbus senza rallentare il ciclo
   ============================================================================ */

class DriverTimingStats {
public:

    /* ============================================================
       RISULTATI PER IP (READ+WRITE)
       ============================================================ */
    struct IpResult {
        unsigned long minTime = ULONG_MAX;
        unsigned long maxTime = 0;
        double avgTime = 0.0;
        unsigned long cycles = 0;

        inline void update(unsigned long dt) {
            if (dt < minTime) minTime = dt;
            if (dt > maxTime) maxTime = dt;

            avgTime = ((avgTime * cycles) + dt) / (cycles + 1);
            cycles++;
        }
    };

private:

    // --- DEVICE/IP TIMING ---
    unsigned long deviceStart = 0;
    std::unordered_map<int, IpResult> perIp;

public:

    DriverTimingStats() {}

    /* ============================================================
       1) DEVICE/IP TIMING (ULTRA-LIGHT)
       ============================================================ */

    inline void startDevice(unsigned long now) {
        deviceStart = now;
    }

    inline void endDevice(unsigned long now, int ipIndex) {
        if (deviceStart == 0) return;

        unsigned long dt = now - deviceStart;
        deviceStart = 0;

        perIp[ipIndex].update(dt);
    }

    const std::unordered_map<int, IpResult>& getIpResults() const {
        return perIp;
    }

    /* ============================================================
       2) REPORT DIAGNOSTICO (solo IP)
       ============================================================ */

    void reportIpTiming(const char* name) const {
        Serial.println("\n===== PER-IP TIMING REPORT =====");
        Serial.print("Driver: "); Serial.println(name);

        for (const auto &kv : perIp) {
            int ip = kv.first;
            const IpResult &r = kv.second;

            Serial.print("IP "); Serial.print(ip); Serial.println(":");
            Serial.print("  Cycles: "); Serial.println(r.cycles);
            Serial.print("  Min: "); Serial.println(r.minTime);
            Serial.print("  Max: "); Serial.println(r.maxTime);
            Serial.print("  Avg: "); Serial.println(r.avgTime, 2);
        }
    }
};

// ============================================================
// PERFORMANCE PROFILER
// ============================================================

struct StageTiming
{
    uint32_t sum = 0;
    uint16_t max = 0;
    uint16_t min = UINT16_MAX;
    uint16_t cycles = 0;

    inline void add(uint16_t ms)
    {
        sum += ms;

        if (ms > max)
            max = ms;

        if (ms < min)
            min = ms;

        if (cycles < UINT16_MAX)
            cycles++;
    }

    inline void reset()
    {
        sum = 0;
        max = 0;
        min = UINT16_MAX;
        cycles = 0;
    }

    inline uint16_t avg() const
    {
        return cycles ? sum / cycles : 0;
    }

    inline uint16_t getMin() const
    {
        return cycles ? min : 0;
    }
};


// ============================================================
// PROFILER
// ============================================================

class PerformanceProfiler
{
public:

    using StageId = uint8_t;

private:

    struct Stage
    {
        const char* name = nullptr;
        StageTiming* timings = nullptr;
    };

    Stage* stages = nullptr;

    uint8_t stageCount = 0;
    uint8_t maxStages = 0;
    uint8_t maxIps = 0;

    bool enabled = true;

    // Timestamp temporanei:
    // [stage][ip]
    uint32_t* startTimes = nullptr;

public:

    PerformanceProfiler(
        uint8_t maxIp,
        uint8_t maxStage)
        : maxStages(maxStage),
          maxIps(maxIp)
    {
        stages = new Stage[maxStages];

        startTimes =
            new uint32_t[
                static_cast<uint16_t>(maxStages) *
                static_cast<uint16_t>(maxIps)
            ];

        for (uint16_t i = 0;
             i < static_cast<uint16_t>(maxStages) *
                 static_cast<uint16_t>(maxIps);
             i++)
        {
            startTimes[i] = 0;
        }
    }

    ~PerformanceProfiler()
    {
        for (uint8_t i = 0; i < stageCount; i++)
            delete[] stages[i].timings;

        delete[] stages;
        delete[] startTimes;
    }

    // --------------------------------------------------------
    // CONFIGURAZIONE
    // --------------------------------------------------------

    StageId addStage(const char* name)
    {
        if (stageCount >= maxStages)
            return UINT8_MAX;

        StageId id = stageCount++;

        stages[id].name = name;
        stages[id].timings = new StageTiming[maxIps];

        return id;
    }

    // --------------------------------------------------------
    // ENABLE / DISABLE
    // --------------------------------------------------------

    inline void setEnabled(bool value)
    {
        enabled = value;
    }

    inline bool isEnabled() const
    {
        return enabled;
    }

    // --------------------------------------------------------
    // TIMING
    // --------------------------------------------------------

    inline void begin(
        StageId stage,
        uint8_t ip,
        uint32_t now)
    {
        if (!enabled)
            return;

        if (stage >= stageCount || ip >= maxIps)
            return;

        startTimes[
            static_cast<uint16_t>(stage) * maxIps + ip
        ] = now;
    }

    inline void end(
        StageId stage,
        uint8_t ip,
        uint32_t now)
    {
        if (!enabled)
            return;

        if (stage >= stageCount || ip >= maxIps)
            return;

        uint32_t index =
            static_cast<uint16_t>(stage) * maxIps + ip;

        uint32_t elapsed =
            now - startTimes[index];

        if (elapsed > UINT16_MAX)
            elapsed = UINT16_MAX;

        stages[stage].timings[ip].add(
            static_cast<uint16_t>(elapsed)
        );
    }

    // --------------------------------------------------------
    // ADD DIRETTO
    // Utile quando abbiamo già il tempo misurato
    // --------------------------------------------------------

    inline void add(
        StageId stage,
        uint8_t ip,
        uint16_t ms)
    {
        if (!enabled)
            return;

        if (stage >= stageCount || ip >= maxIps)
            return;

        stages[stage].timings[ip].add(ms);
    }

    // --------------------------------------------------------
    // RESET
    // --------------------------------------------------------

    void reset()
    {
        for (uint8_t s = 0; s < stageCount; s++)
        {
            for (uint8_t ip = 0; ip < maxIps; ip++)
                stages[s].timings[ip].reset();
        }
    }

    // --------------------------------------------------------
    // ACCESSO
    // --------------------------------------------------------

    inline const char* getStageName(StageId stage) const
    {
        if (stage >= stageCount)
            return "";

        return stages[stage].name;
    }

    inline const StageTiming& get(
        StageId stage,
        uint8_t ip) const
    {
        return stages[stage].timings[ip];
    }

    inline uint8_t getStageCount() const
    {
        return stageCount;
    }

    inline uint8_t getMaxIps() const
    {
        return maxIps;
    }

    // --------------------------------------------------------
    // REPORT
    // --------------------------------------------------------

    void report(const char* title = nullptr) const
    {
        if (title)
        {
            LOG_WF(
                "PROFILER",
                "===== %s =====",
                title
            );
        }

        for (uint8_t s = 0; s < stageCount; s++)
        {
            for (uint8_t ip = 0; ip < maxIps; ip++)
            {
                const StageTiming& t =
                    stages[s].timings[ip];

                if (t.cycles == 0)
                    continue;

                LOG_WF(
                    "PROFILER",
                    "%s IP%d  "
                    "Cycles=%u  "
                    "Min=%u  "
                    "Max=%u  "
                    "Avg=%u  "
                    "Sum=%lu",
                    stages[s].name,
                    ip,
                    t.cycles,
                    t.getMin(),
                    t.max,
                    t.avg(),
                    (unsigned long)t.sum
                );
            }
        }
    }
};


class EventManager
{
public:

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================

    static constexpr uint8_t INVALID_CONSUMER_ID = 255;

    static constexpr size_t EVENT_QUEUE_SIZE = 32;

    static constexpr size_t DEFAULT_PROCESS_BUDGET = 4;


    // ============================================================
    // EVENT
    // ============================================================

    struct Event
    {
        int area = -1;
        long value = 0;
        uint8_t source = INVALID_CONSUMER_ID;
        uint32_t id = 0;
    };


    // ============================================================
    // CALLBACK
    // ============================================================

    using Callback = void (*)(const Event&);


    // ============================================================
    // COALESCING POLICY
    //
    // La policy NON è definita qui.
    //
    // EventManager chiede al proprietario esterno
    // se il protocollo associato alla source deve
    // usare coalescing.
    // ============================================================

    using CoalescePolicyFn =
        bool (*)(void* context, int protocolId);


    // ============================================================
    // CONSUMER / SOURCE
    // ============================================================

    struct Consumer
    {
        uint8_t id = INVALID_CONSUMER_ID;

        Callback callback = nullptr;

        const char* label = nullptr;

        // Protocollo proprietario della source.
        //
        // -1 = nessun protocollo associato.
        int protocolId = -1;
    };


    // ============================================================
    // STATISTICHE
    // ============================================================

    struct Statistics
    {
        size_t totalQueued = 0;
        size_t totalProcessed = 0;
        size_t totalDropped = 0;
        size_t totalCoalesced = 0;
        size_t peakQueue = 0;
        size_t pending = 0;
    };


private:

    // ============================================================
    // CONSUMERS
    // ============================================================

    std::vector<Consumer> consumers;

    uint8_t nextConsumerId = 0;


    // ============================================================
    // POLICY PROVIDER
    // ============================================================

    CoalescePolicyFn coalescePolicy = nullptr;

    void* coalescePolicyContext = nullptr;


    // ============================================================
    // EVENT FIFO
    // ============================================================

    Event eventQueue[EVENT_QUEUE_SIZE];

    size_t eventHead = 0;

    size_t eventTail = 0;

    size_t eventCount = 0;


    // ============================================================
    // EVENT ID
    // ============================================================

    uint32_t nextEventId = 1;


    // ============================================================
    // STATISTICHE
    // ============================================================

    size_t totalQueued = 0;

    size_t totalProcessed = 0;

    size_t totalDropped = 0;

    size_t totalCoalesced = 0;

    size_t peakQueue = 0;


public:

    // ============================================================
    // CONSTRUCTOR
    // ============================================================

    EventManager() = default;


    // ============================================================
    // CONFIGURA POLICY PROVIDER
    //
    // Viene impostato una sola volta dal runtime.
    //
    // Esempio:
    //
    // eventManager.setCoalescePolicy(
    //     NetworkManager::EventCoalescePolicy,
    //     &network
    // );
    // ============================================================

    void setCoalescePolicy(
        CoalescePolicyFn fn,
        void* context)
    {
        coalescePolicy = fn;
        coalescePolicyContext = context;
    }


    // ============================================================
    // ADD CONSUMER / SOURCE
    //
    // protocolId collega la source al protocollo che l'ha
    // generata.
    //
    // La policy viene decisa dal NetworkManager, non qui.
    // ============================================================

    uint8_t add(
        Callback callback,
        const char* label,
        int protocolId = -1)
    {
        if (!callback)
            return INVALID_CONSUMER_ID;

        if (nextConsumerId == INVALID_CONSUMER_ID)
            return INVALID_CONSUMER_ID;

        const uint8_t id =
            nextConsumerId++;

        consumers.push_back({
            id,
            callback,
            label,
            protocolId
        });

        return id;
    }


    // ============================================================
    // UPDATE PROTOCOL ASSOCIATO ALLA SOURCE
    // ============================================================

    bool setProtocol(
        uint8_t source,
        int protocolId)
    {
        for (auto& consumer : consumers)
        {
            if (consumer.id == source)
            {
                consumer.protocolId = protocolId;
                return true;
            }
        }

        return false;
    }


    // ============================================================
    // GET SOURCE PROTOCOL
    // ============================================================

    int getProtocolId(
        uint8_t source) const
    {
        for (const auto& consumer : consumers)
        {
            if (consumer.id == source)
                return consumer.protocolId;
        }

        return -1;
    }


    // ============================================================
    // PUSH NUOVO EVENTO
    //
    // NON esegue callback.
    //
    // Ritorno:
    //   id > 0  -> evento inserito/coalesced
    //   0       -> evento perso perché FIFO piena
    // ============================================================

    uint32_t push(
        int area,
        long value,
        uint8_t source)
    {
        Event event;

        event.area = area;
        event.value = value;
        event.source = source;

        event.id = nextEventId++;

        if (nextEventId == 0)
            nextEventId = 1;


        if (!enqueue(event))
            return 0;


        return event.id;
    }


    // ============================================================
    // PUSH EVENTO ESISTENTE
    // ============================================================

    bool push(const Event& event)
    {
        return enqueue(event);
    }


    // ============================================================
    // PROCESS
    //
    // Dispatch differito.
    // ============================================================

    size_t process(
        size_t maxEvents = DEFAULT_PROCESS_BUDGET)
    {
        if (maxEvents == 0)
            return 0;


        size_t processed = 0;


        while (processed < maxEvents)
        {
            Event event;

            if (!dequeue(event))
                break;


            dispatch(event);

            ++processed;

            ++totalProcessed;
        }


        return processed;
    }


    // ============================================================
    // PROCESS ONE
    // ============================================================

    bool processOne()
    {
        Event event;

        if (!dequeue(event))
            return false;


        dispatch(event);

        ++totalProcessed;

        return true;
    }


    // ============================================================
    // FIFO STATUS
    // ============================================================

    bool empty() const
    {
        return eventCount == 0;
    }


    bool full() const
    {
        return eventCount >= EVENT_QUEUE_SIZE;
    }


    size_t pending() const
    {
        return eventCount;
    }


    constexpr size_t capacity() const
    {
        return EVENT_QUEUE_SIZE;
    }


    // ============================================================
    // CONSUMERS
    // ============================================================

    size_t size() const
    {
        return consumers.size();
    }


    const std::vector<Consumer>&
    getConsumers() const
    {
        return consumers;
    }


    // ============================================================
    // STATISTICHE
    // ============================================================

    Statistics getStatistics() const
    {
        Statistics stats;

        stats.totalQueued =
            totalQueued;

        stats.totalProcessed =
            totalProcessed;

        stats.totalDropped =
            totalDropped;

        stats.totalCoalesced =
            totalCoalesced;

        stats.peakQueue =
            peakQueue;

        stats.pending =
            eventCount;

        return stats;
    }


    size_t getTotalQueued() const
    {
        return totalQueued;
    }


    size_t getTotalProcessed() const
    {
        return totalProcessed;
    }


    size_t getTotalDropped() const
    {
        return totalDropped;
    }


    size_t getTotalCoalesced() const
    {
        return totalCoalesced;
    }


    size_t getPeakQueue() const
    {
        return peakQueue;
    }


    // ============================================================
    // RESET STATISTICHE
    // ============================================================

    void resetStatistics()
    {
        totalQueued = 0;

        totalProcessed = 0;

        totalDropped = 0;

        totalCoalesced = 0;

        peakQueue = eventCount;
    }


    // ============================================================
    // CLEAR FIFO
    // ============================================================

    void clear()
    {
        eventHead = 0;

        eventTail = 0;

        eventCount = 0;
    }


private:

    // ============================================================
    // SOURCE -> COALESCE?
    // ============================================================

    bool shouldCoalesce(
        uint8_t source) const
    {
        if (!coalescePolicy)
            return false;


        const int protocolId =
            getProtocolId(source);


        if (protocolId < 0)
            return false;


        return coalescePolicy(
            coalescePolicyContext,
            protocolId
        );
    }


    // ============================================================
    // UPDATE EVENTO ESISTENTE
    //
    // Chiave:
    //
    //      source + area
    //
    // Quindi:
    //
    // MQTT + area 127
    //
    // è diverso da:
    //
    // HMI + area 127
    // ============================================================

    bool updateExistingCoalesced(
        const Event& event)
    {
        for (size_t i = 0;
             i < eventCount;
             ++i)
        {
            const size_t index =
                (eventHead + i) %
                EVENT_QUEUE_SIZE;


            Event& queued =
                eventQueue[index];


            if (queued.source !=
                event.source)
            {
                continue;
            }


            if (queued.area !=
                event.area)
            {
                continue;
            }


            // ------------------------------------------------
            // COALESCE
            // ------------------------------------------------

            queued.value =
                event.value;

            queued.id =
                event.id;

            ++totalCoalesced;

            return true;
        }


        return false;
    }


    // ============================================================
    // ENQUEUE
    // ============================================================

    bool enqueue(
        const Event& event)
    {
        const bool coalesce =
            shouldCoalesce(event.source);


        // --------------------------------------------------------
        // COALESCE EVENTO GIÀ PENDENTE
        //
        // Importante:
        // anche se la FIFO è piena, una entry già esistente
        // può comunque essere aggiornata.
        // --------------------------------------------------------

        if (coalesce)
        {
            if (updateExistingCoalesced(event))
                return true;
        }


        // --------------------------------------------------------
        // FIFO PIENA
        // --------------------------------------------------------

        if (eventCount >=
            EVENT_QUEUE_SIZE)
        {
            ++totalDropped;

            return false;
        }


        // --------------------------------------------------------
        // INSERT
        // --------------------------------------------------------

        eventQueue[eventTail] =
            event;


        ++eventTail;


        if (eventTail >=
            EVENT_QUEUE_SIZE)
        {
            eventTail = 0;
        }


        ++eventCount;

        ++totalQueued;


        if (eventCount > peakQueue)
        {
            peakQueue =
                eventCount;
        }


        return true;
    }


    // ============================================================
    // DEQUEUE
    // ============================================================

    bool dequeue(
        Event& event)
    {
        if (eventCount == 0)
            return false;


        event =
            eventQueue[eventHead];


        ++eventHead;


        if (eventHead >=
            EVENT_QUEUE_SIZE)
        {
            eventHead = 0;
        }


        --eventCount;

        return true;
    }


    // ============================================================
    // DISPATCH
    // ============================================================

    void dispatch(
        const Event& event)
    {
        const size_t consumerCount =
            consumers.size();


        for (size_t i = 0;
             i < consumerCount;
             ++i)
        {
            // Copia locale per evitare
            // riferimenti invalidati da eventuali
            // modifiche al vector durante la callback.

            const Consumer consumer =
                consumers[i];


            if (!consumer.callback)
                continue;


            // Il source non riceve il proprio evento.
            if (consumer.id ==
                event.source)
            {
                continue;
            }


            consumer.callback(event);
        }
    }
};

class CommunicationScheduler
{
public:

    using Callback = void (*)(unsigned long now);

    static constexpr uint8_t STORAGE_MAX_SERVICES = 8;
    static constexpr uint8_t MAX_SERVICES_PER_UPDATE = 3;

    struct Service
    {
        Callback callback = nullptr;

        unsigned long intervalMs = 0;
        unsigned long lastRun = 0;

        uint8_t priority = 0;

        bool enabled = false;
        bool used = false;
    };

private:

    Service services[STORAGE_MAX_SERVICES];

    uint8_t maxServices = 0;
    uint8_t serviceCount = 0;

    uint8_t rrCursor = 0;

public:

    explicit CommunicationScheduler(uint8_t requestedMax)
    {
        maxServices =
            (requestedMax > STORAGE_MAX_SERVICES)
                ? STORAGE_MAX_SERVICES
                : requestedMax;

        if (requestedMax > STORAGE_MAX_SERVICES)
        {
            LOG_WF(
                "COMM-SCHED",
                "Capacita richiesta=%u, limitata a %u",
                requestedMax,
                STORAGE_MAX_SERVICES
            );
        }
    }

    // ============================================================
    // ADD
    // ============================================================

    int add(
        Callback callback,
        unsigned long intervalMs,
        uint8_t priority,
        bool enabled = true)
    {
        if (!callback)
        {
            LOG_WF(
                "COMM-SCHED",
                "Callback nullo"
            );

            return -1;
        }

        if (serviceCount >= maxServices)
        {
            LOG_WF(
                "COMM-SCHED",
                "Numero massimo servizi superato: %u/%u",
                serviceCount,
                maxServices
            );

            return -1;
        }

        for (uint8_t i = 0; i < maxServices; ++i)
        {
            Service& s = services[i];

            if (s.used)
                continue;

            s.callback   = callback;
            s.intervalMs = intervalMs;
            s.lastRun    = 0;
            s.priority   = priority;
            s.enabled    = enabled;
            s.used       = true;

            ++serviceCount;

            return i;
        }

        return -1;
    }

    // ============================================================
    // ENABLE
    // ============================================================

    void setEnabled(int id, bool enabled)
    {
        if (id < 0 || id >= maxServices)
            return;

        Service& s = services[id];

        if (!s.used)
            return;

        s.enabled = enabled;

        if (enabled)
            s.lastRun = 0;
    }

    // ============================================================
    // UPDATE
    // ============================================================

    bool update(unsigned long now)
    {
        uint8_t executed = 0;

        while (executed < MAX_SERVICES_PER_UPDATE)
        {
            const int selected = selectNext(now);

            if (selected < 0)
                break;

            Service& s = services[selected];

            s.lastRun = now;

            rrCursor =
                static_cast<uint8_t>(selected + 1);

            if (rrCursor >= maxServices)
                rrCursor = 0;

            ++executed;

            s.callback(now);
        }

        return executed > 0;
    }

private:

    // ============================================================
    // READY
    // ============================================================

    static bool isDue(
        const Service& s,
        unsigned long now)
    {
        if (!s.used || !s.enabled || !s.callback)
            return false;

        if (s.intervalMs == 0)
            return true;

        return
            static_cast<unsigned long>(
                now - s.lastRun
            ) >= s.intervalMs;
    }

    // ============================================================
    // SELECT NEXT
    // ============================================================

    int selectNext(unsigned long now) const
    {
        int best = -1;

        uint8_t bestPriority = 0;
        uint16_t bestDistance = UINT16_MAX;

        for (uint8_t i = 0; i < maxServices; ++i)
        {
            const Service& s = services[i];

            if (!isDue(s, now))
                continue;

            uint16_t distance;

            if (i >= rrCursor)
            {
                distance = i - rrCursor;
            }
            else
            {
                distance =
                    static_cast<uint16_t>(
                        maxServices - rrCursor + i
                    );
            }

            if (
                best < 0 ||
                s.priority > bestPriority ||
                (
                    s.priority == bestPriority &&
                    distance < bestDistance
                )
            )
            {
                best = i;
                bestPriority = s.priority;
                bestDistance = distance;
            }
        }

        return best;
    }
};
#endif
