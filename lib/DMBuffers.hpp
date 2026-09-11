#ifndef DMBuffers_H
#define DMBuffers_H

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
   

#include <Arduino.h>

#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================================
// CHANGE RATE MONITOR
// ============================================================================

class ChangeRateMonitor
{
public:
    ChangeRateMonitor()
    {
        counters.reserve(INITIAL_RESERVE);
        snapshot.reserve(INITIAL_RESERVE);
    }

    inline void onChanged(int area)
    {
        counters[area]++;
    }

    inline void tick(uint32_t now)
    {
        if (lastCheck == 0)
        {
            lastCheck = now;
            return;
        }

        if (now - lastCheck < REPORT_INTERVAL_MS)
            return;

        snapshot.clear();
        snapshot.reserve(counters.size());

        for (const auto& entry : counters)
        {
            const int area = entry.first;
            const uint16_t count = entry.second;

            snapshot.push_back({
                area,
                count
            });

            if (count > ANALOG_THRESHOLD)
            {
                LOG_WF(
                    "RATE_ANALOG",
                    "Area=%d changed=%u/s (HIGH RATE)",
                    area,
                    count
                );
            }
        }

        counters.clear();
        lastCheck = now;
    }

    struct RateEntry
    {
        int area;
        uint16_t count;
    };

    std::vector<RateEntry> getSortedReport() const
    {
        std::vector<RateEntry> sorted = snapshot;

        std::sort(
            sorted.begin(),
            sorted.end(),
            [](const RateEntry& a, const RateEntry& b)
            {
                return a.count > b.count;
            }
        );

        return sorted;
    }

private:
    static constexpr uint32_t REPORT_INTERVAL_MS = 1000;
    static constexpr uint16_t ANALOG_THRESHOLD = 25;
    static constexpr size_t INITIAL_RESERVE = 256;

    std::unordered_map<int, uint16_t> counters;
    std::vector<RateEntry> snapshot;

    uint32_t lastCheck = 0;
};


// ============================================================================
// AREA TRACKER
// ============================================================================

class AreaTracker
{
private:
    std::vector<int> initCount;

public:
    explicit AreaTracker(int totalAreas)
    {
        initCount.resize(totalAreas, 0);
    }

    void registerInit(int area)
    {
        if (area >= 0 &&
            area < static_cast<int>(initCount.size()))
        {
            initCount[area]++;
        }
        else
        {
            LOG_EF(
                "AreaTracker",
                "Fail to initialize area=%d",
                area
            );
        }
    }

    std::vector<int> getNeverInitialized() const
    {
        std::vector<int> result;

        for (int i = 0;
             i < static_cast<int>(initCount.size());
             ++i)
        {
            if (initCount[i] == 0)
                result.push_back(i);
        }

        return result;
    }

    std::vector<int> getInitializedMultipleTimes() const
    {
        std::vector<int> result;

        for (int i = 0;
             i < static_cast<int>(initCount.size());
             ++i)
        {
            if (initCount[i] > 1)
                result.push_back(i);
        }

        return result;
    }
};


// ============================================================================
// BUFFER TYPES
// ============================================================================

struct BufferSourceInfo
{
    unsigned long time;
    long value;
    long prevValue;
};


struct BufferInfo
{
    BufferSourceInfo field;

    bool Reverse = false;
    int areaToWrite = -1;
    String name;
    bool isVirtual = false;
};


// ============================================================================
// BUFFER
// ============================================================================

class Buffer
{
private:

    ChangeRateMonitor monitor;

    inline bool isValidArea(int area) const
    {
        return area >= 0 &&
               area < _items;
    }


public:

    // ========================================================================
    // CONSTANTS
    // ========================================================================

    static constexpr int NO_AREA = -1;


    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    explicit Buffer(unsigned int items)
        : tracker(static_cast<int>(items)),
          _items(static_cast<int>(items)),
          _buffer(nullptr)
    {
        if (items > 0)
            _buffer = new BufferInfo[items];

        for (int i = 0; i < _items; ++i)
        {
            auto& info = _buffer[i];

            info.field = {
                0,
                0,
                0
            };

            info.Reverse     = false;
            info.areaToWrite = NO_AREA;
            info.name        = String();
            info.isVirtual   = false;
        }
    }


    // ========================================================================
    // DESTRUCTOR
    // ========================================================================

    ~Buffer()
    {
        delete[] _buffer;
        _buffer = nullptr;
    }


    // ========================================================================
    // COPY DISABLED
    // ========================================================================

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;


    // ========================================================================
    // MOVE
    // ========================================================================

    Buffer(Buffer&& other) noexcept
        : monitor(std::move(other.monitor)),
          tracker(std::move(other.tracker)),
          _items(other._items),
          _buffer(other._buffer),
          _changed(std::move(other._changed))
    {
        other._items  = 0;
        other._buffer = nullptr;
        other._changed.clear();
    }


    Buffer& operator=(Buffer&& other) noexcept
    {
        if (this != &other)
        {
            delete[] _buffer;

            monitor = std::move(other.monitor);
            tracker = std::move(other.tracker);

            _items   = other._items;
            _buffer  = other._buffer;
            _changed = std::move(other._changed);

            other._items  = 0;
            other._buffer = nullptr;
            other._changed.clear();
        }

        return *this;
    }


    // ========================================================================
    // TICK
    // ========================================================================

    inline void tick(uint32_t now)
    {
        monitor.tick(now);
    }


    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    void SetElement(
        int area,
        int areaToWrite,
        bool Reverse,
        const char* name)
    {
        if (!isValidArea(area))
        {
            LOG_WFN(
                "Buffer",
                "SetElement ignorato: area=%d fuori range",
                area
            );

            return;
        }

        tracker.registerInit(area);

        auto& info = _buffer[area];

        if (name == nullptr || name[0] == '\0')
        {
            char autoName[32];

            snprintf(
                autoName,
                sizeof(autoName),
                "Auto_%d",
                area
            );

            info.name = autoName;
        }
        else
        {
            info.name = name;
        }

        info.Reverse     = Reverse;
        info.areaToWrite = areaToWrite;
    }


    // ========================================================================
    // DATA
    // ========================================================================

    inline bool Compare(
        int area,
        long value) const
    {
        if (!isValidArea(area))
            return false;

        return _buffer[area].field.value == value;
    }

    enum class WriteResult : uint8_t
    {
        CHANGED,
        EQUAL,
        ERROR
    };

    inline WriteResult WriteElement(
        int area,
        long value,
        unsigned long now)
    {
        return WriteElement(
            area,
            value,
            false,
            now
        );
    }


    inline WriteResult WriteElement(
        int area,
        long value,
        bool silent,
        unsigned long now)
    {
        if (!isValidArea(area))
        {
            LOG_WFN(
                "Buffer",
                "WriteElement ignorato: area %d fuori range",
                area
            );

            return WriteResult::ERROR;
        }

        auto& entry = _buffer[area].field;

        if (entry.value == value)
            return WriteResult::EQUAL;

        entry.prevValue = entry.value;
        entry.value     = value;
        entry.time      = now;

        if (!silent)
            markChanged(area);

        return WriteResult::CHANGED;
    }


    inline bool GetData(
        int area,
        BufferSourceInfo& out) const
    {
        if (!isValidArea(area))
        {
            LOG_WF(
                "Buffer",
                "GetData ignorato: area %d fuori range",
                area
            );

            out = {
                0,
                0,
                0
            };

            return false;
        }

        out = _buffer[area].field;
        return true;
    }


    // Accesso rapido interno:
    // usare quando l'area è già sicuramente valida.
    inline const BufferSourceInfo& getFieldEntry(
        int area) const
    {
        return _buffer[area].field;
    }


    inline BufferSourceInfo& getFieldEntry(
        int area)
    {
        return _buffer[area].field;
    }


    inline long getValueFast(
        int area,
        int divideFactor = 0) const
    {
        if (!isValidArea(area))
        {
            LOG_EF(
                "Buffer",
                "Area fuori range: %d",
                area
            );

            return 0;
        }

        const long value =
            _buffer[area].field.value;

        if (divideFactor == 0)
            return value;

        return value / divideFactor;
    }


    // ========================================================================
    // CHANGED
    // ========================================================================

    inline bool HasChanged(
        int area) const
    {
        if (!isValidArea(area))
            return false;

        return _changed.find(area) != _changed.end();
    }


    inline const std::unordered_set<int>&
    getChangedMap() const
    {
        return _changed;
    }


    // ========================================================================
    // CHANGED MAP VIEW
    // ========================================================================

    class ChangedMapView
    {
    public:

        class Iterator
        {
        public:

            using SetIter =
                std::unordered_set<int>::const_iterator;


            Iterator(
                SetIter current,
                SetIter end,
                const Buffer* parent,
                bool skipVirtual)
                : it(current),
                  endIt(end),
                  parent(parent),
                  skipVirtual(skipVirtual)
            {
                skipNonMatching();
            }


            inline int operator*() const
            {
                return *it;
            }


            inline Iterator& operator++()
            {
                ++it;
                skipNonMatching();
                return *this;
            }


            inline bool operator!=(
                const Iterator& other) const
            {
                return it != other.it;
            }


            inline bool operator==(
                const Iterator& other) const
            {
                return it == other.it;
            }


        private:

            inline void skipNonMatching()
            {
                if (!skipVirtual || !parent)
                    return;

                while (
                    it != endIt &&
                    parent->_buffer[*it].isVirtual)
                {
                    ++it;
                }
            }


            SetIter it;
            SetIter endIt;

            const Buffer* parent;
            bool skipVirtual;
        };


        ChangedMapView(
            const std::unordered_set<int>& changed,
            const Buffer* parent,
            bool skipVirtual)
            : set(changed),
              parent(parent),
              skipVirtual(skipVirtual)
        {
        }


        inline Iterator begin() const
        {
            return Iterator(
                set.begin(),
                set.end(),
                parent,
                skipVirtual
            );
        }


        inline Iterator end() const
        {
            return Iterator(
                set.end(),
                set.end(),
                parent,
                skipVirtual
            );
        }


        inline bool empty() const
        {
            if (!skipVirtual)
                return set.empty();

            return begin() == end();
        }


        size_t size() const
        {
            if (!skipVirtual)
                return set.size();

            size_t count = 0;

            for (int area : set)
            {
                if (!parent->_buffer[area].isVirtual)
                    ++count;
            }

            return count;
        }


    private:

        const std::unordered_set<int>& set;
        const Buffer* parent;
        bool skipVirtual;
    };


    inline ChangedMapView getChangedMap(
        bool skipVirtual) const
    {
        return ChangedMapView(
            _changed,
            this,
            skipVirtual
        );
    }


    // ========================================================================
    // RESET
    // ========================================================================

    inline void ResetElement(int area)
    {
        if (!isValidArea(area))
            return;

        _changed.erase(area);
    }


    void ResetAll(
        unsigned long now,
        unsigned long minAgeMs = 1000)
    {
        for (auto it = _changed.begin();
             it != _changed.end();)
        {
            const int area = *it;

            if (!isValidArea(area))
            {
                it = _changed.erase(it);
                continue;
            }

            const auto& entry =
                _buffer[area].field;

            if (now - entry.time > minAgeMs)
            {
                it = _changed.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }


    // ========================================================================
    // MAPPING / PROPERTIES
    // ========================================================================

    inline int GetAreaToWrite(
        int area) const
    {
        if (!isValidArea(area))
            return NO_AREA;

        return _buffer[area].areaToWrite;
    }


    inline bool IsReverse(
        int area) const
    {
        if (!isValidArea(area))
            return false;

        return _buffer[area].Reverse;
    }


    inline void SetVirtual(
        int area,
        bool value)
    {
        if (isValidArea(area))
            _buffer[area].isVirtual = value;
    }


    inline bool IsVirtual(
        int area) const
    {
        return isValidArea(area)
            ? _buffer[area].isVirtual
            : false;
    }


    inline bool Exists(
        int area) const
    {
        if (!isValidArea(area))
        {
            LOG_EF(
                "Buffer",
                "Exists(): area %d fuori range (items=%d)",
                area,
                _items
            );

            return false;
        }

        return _buffer[area].name.length() > 0;
    }


    inline const char* GetName(
        int area) const
    {
        if (!isValidArea(area))
            return "Unknown area";

        const auto& info =
            _buffer[area];

        if (!info.name.length())
            return "Undefined area";

        return info.name.c_str();
    }


    inline size_t size() const
    {
        return static_cast<size_t>(_items);
    }


    // ========================================================================
    // DIAGNOSTICS
    // ========================================================================

    std::vector<int> getNeverInitialized()
    {
        return tracker.getNeverInitialized();
    }


    std::vector<int> getInitializedMultipleTimes()
    {
        return tracker.getInitializedMultipleTimes();
    }


    class Diagnostics
    {
    public:

        static void ReportVirtualAreas(
            Buffer& buf)
        {
            Serial.println(
                "\n===== AREE VIRTUALI ====="
            );

            for (int area = 0;
                 area < static_cast<int>(buf.size());
                 ++area)
            {
                if (buf.IsVirtual(area))
                {
                    Serial.print("Area ");
                    Serial.print(area);
                    Serial.print(" -> VIRTUAL (");
                    Serial.print(buf.GetName(area));
                    Serial.println(")");
                }
            }
        }


        static void ReportUnusedBufferAreas(
            Buffer& buf)
        {
            Serial.println(
                "\n===== AREE NON MAPPATE ====="
            );

            for (int area = 0;
                 area < static_cast<int>(buf.size());
                 ++area)
            {
                if (buf.IsVirtual(area))
                {
                    Serial.print("Area ");
                    Serial.print(area);
                    Serial.print(" (");
                    Serial.print(buf.GetName(area));
                    Serial.println(")");
                }
            }
        }


        static void ReportNeverInitialized(
            Buffer& buf)
        {
            auto never =
                buf.getNeverInitialized();

            Serial.println(
                "\n===== AREE NON INIZIALIZZATE ====="
            );

            if (never.empty())
            {
                Serial.println(
                    "Nessuna area non inizializzata."
                );

                return;
            }

            for (int area : never)
            {
                Serial.print("Area ");
                Serial.print(area);
                Serial.print(" (");
                Serial.print(buf.GetName(area));
                Serial.println(")");
            }
        }


        static void ReportMultipleInitialized(
            Buffer& buf)
        {
            auto multi =
                buf.getInitializedMultipleTimes();

            Serial.println(
                "\n===== AREE INIZIALIZZATE PIU' VOLTE ====="
            );

            if (multi.empty())
            {
                Serial.println(
                    "Nessuna area inizializzata piu' volte."
                );

                return;
            }

            for (int area : multi)
            {
                Serial.print("Area ");
                Serial.print(area);
                Serial.print(" (");
                Serial.print(buf.GetName(area));
                Serial.println(")");
            }
        }
    };


private:

    // ========================================================================
    // MARK CHANGED
    // ========================================================================

    inline void markChanged(int area)
    {
        if (!isValidArea(area))
        {
            LOG_WF(
                "Buffer",
                "markChanged ignorato: area=%d fuori range",
                area
            );

            return;
        }

        _changed.insert(area);
        monitor.onChanged(area);
    }


    AreaTracker tracker;

    int _items = 0;

    BufferInfo* _buffer = nullptr;

    /*
     * SET DI AREE MODIFICATE.
     *
     * Ogni elemento è direttamente:
     *
     *     area
     *
     */
    std::unordered_set<int> _changed;
};

#endif
