#ifndef DMBuffers_H
#define DMBuffers_H

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

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class AreaTracker {
private:
    std::vector<int> initCount;

public:
    AreaTracker(int totalAreas) {
        initCount.resize(totalAreas, 0);
    }

    void registerInit(int area) {
        if (area >= 0 && area < initCount.size()) {
            initCount[area]++;
        } else LOG_EF("AreaTracker"," Fail to iniztialize area=%d", area);
    }

    std::vector<int> getNeverInitialized() const {
        std::vector<int> result;
        for (int i = 0; i < initCount.size(); i++) {
            if (initCount[i] == 0)
                result.push_back(i);
        }
        return result;
    }

    std::vector<int> getInitializedMultipleTimes() const {
        std::vector<int> result;
        for (int i = 0; i < initCount.size(); i++) {
            if (initCount[i] > 1)
                result.push_back(i);
        }
        return result;
    }
};

enum BufferFlagType 
{
  Field=0,
  FromPanel=1,
  ToPanel=2,
  BufferFlagType_Count=3 // <--- numero totale
};

typedef struct {
    BufferFlagType bufferType;
    unsigned long time; //in millis, that take 50 days to go back to zero. -1 means never write or no change
    long value;
    long prevValue;
  }BufferSourceInfo;

typedef struct {
    BufferSourceInfo Data[BufferFlagType_Count]; 
    bool exists[BufferFlagType_Count];

    bool Reverse;
    bool ReadFromPanel;
    bool WriteToPanel;
    int areaToWrite;
    String name;
    bool isVirtual = false;
    bool wasRead = false;
    bool wasWritten = false;
}BufferInfo;

typedef struct {
    BufferInfo Item;
    int area;
  }BufferItemInfo;

  typedef struct {
    BufferSourceInfo Item;
    int area;
  }BufferItemInfo2;

typedef struct {
    BufferInfo Item;
    bool done;
  }BufferReadElementInfo;

typedef struct {
    int *itemsPtr;
    int size;
  }BufferArrayInfo;

class Buffer
{
    private: 
    inline bool isValidArea(int area) const {
        return area >= 0 && area < _items;
    }

    struct ChangedEntry {
        int area;
        BufferFlagType type;
    };

    class ChangedTypeView {
    public:
        struct Iterator {
            using MapIter = std::unordered_map<int, ChangedEntry>::const_iterator;

            MapIter it, end;
            BufferFlagType filterType;
            const Buffer* parent;
            bool skipVirtual;

            Iterator(MapIter i,
                     MapIter e,
                     BufferFlagType t,
                     const Buffer* p,
                     bool skip)
                : it(i),
                  end(e),
                  filterType(t),
                  parent(p),
                  skipVirtual(skip)
            {
                skipNonMatching();
            }

            void skipNonMatching() {
                while (it != end) {
                    int type = it->first % BufferFlagType_Count;
                    if (type != filterType) {
                        ++it;
                        continue;
                    }

                    if (skipVirtual) {
                        int area = it->second.area;
                        if (parent && parent->_buffer[area].isVirtual) {
                            ++it;
                            continue;
                        }
                    }

                    // match trovato
                    break;
                }
            }

            const auto& operator*() const { return *it; }
            const auto* operator->() const { return &(*it); }

            Iterator& operator++() {
                ++it;
                skipNonMatching();
                return *this;
            }

            bool operator!=(const Iterator& other) const {
                return it != other.it;
            }
        };

        ChangedTypeView(const std::unordered_map<int, ChangedEntry>& m,
                        BufferFlagType t,
                        const Buffer* p,
                        bool skip)
            : map(m),
              filterType(t),
              parent(p),
              skipVirtual(skip)
        {}

        Iterator begin() const {
            return Iterator(map.begin(), map.end(), filterType, parent, skipVirtual);
        }

        Iterator end() const {
            return Iterator(map.end(), map.end(), filterType, parent, skipVirtual);
        }

        bool empty() const {
            for (auto it = map.begin(); it != map.end(); ++it) {
                int type = it->first % BufferFlagType_Count;
                if (type != filterType) continue;

                if (skipVirtual && parent && parent->_buffer[it->second.area].isVirtual)
                    continue;

                return false;
            }
            return true;
        }

        size_t size() const {
            size_t count = 0;
            for (auto it = map.begin(); it != map.end(); ++it) {
                int type = it->first % BufferFlagType_Count;
                if (type != filterType) continue;

                if (skipVirtual && parent && parent->_buffer[it->second.area].isVirtual)
                    continue;

                ++count;
            }
            return count;
        }

    private:
        const std::unordered_map<int, ChangedEntry>& map;
        BufferFlagType filterType;
        const Buffer* parent;
        bool skipVirtual;
    };

  public:             
    Buffer(unsigned int items);       
   
    BufferArrayInfo GetToReadFromPanel();
    void Init();
    bool HasChanged(int area, BufferFlagType type);
    void SetElement(int area, int areaToWrite, bool WriteToPanel, bool ReadFromPanel, bool Reverse, const char* name);
    void AddType(int area, long initialValue, BufferFlagType type);
    bool CanReadFromPanel(int area);
    bool CanWriteToPanel(int area);
    bool WriteElement(int area, BufferFlagType type, long value, bool silent, unsigned long now);
    bool WriteElement(int area, BufferFlagType type, long value, unsigned long now);
    bool IsReverse(int area);
    int GetAreaToWrite(int area);
    bool GetData(int area, BufferFlagType type, BufferSourceInfo &data);
    
    inline long getValueFast(int area, int divideFactor=0) { 
        if (!isValidArea(area)) {
            LOG_EF("Buffer", "Area fuori range: %d", area);
            return 0;
        }

        // Usa solo se il tipo Field esiste
        if (!_buffer[area].exists[Field]) {
            return 0;
        }

        long tmp = _buffer[area].Data[Field].value;
        
        // 🔥 Il valore è gia salvato NEGATO se reverse
        //if (_buffer[area].Reverse) 
        //    tmp = !tmp;

        _buffer[area].wasRead = true;

        // Se divideFactor = 0 → ritorna booleano o valore intero
        if (divideFactor == 0) {
            return tmp;
        }
        
        return tmp / divideFactor;
    }

    void SetVirtual(int area, bool v) {
        if (area >= 0 && area < _items)
            _buffer[area].isVirtual = v;
    }

    bool IsVirtual(int area) const {
        return (area >= 0 && area < _items) ? _buffer[area].isVirtual : false;
    }

    bool Exists(int area) const {
    if (area < 0 || area >= _items) {
        LOG_EF("Buffer", "Exists(): area %d fuori range (items=%d)", area, _items);
        return false;
    }

    if (_buffer[area].name == nullptr) {
        LOG_EF("Buffer", "Exists(): area %d NON definita (name=nullptr)", area);
        return false;
    }

    return true;
}


    bool WasEverRead(int area) const {
        return (area >= 0 && area < _items) ? _buffer[area].wasRead : false;
    }

    bool WasEverWritten(int area) const {
        return (area >= 0 && area < _items) ? _buffer[area].wasWritten : false;
    }

    BufferReadElementInfo ReadElement(int area, bool preserve, BufferFlagType type);
    void ResetElement(int area, BufferFlagType type);
    void ResetArea(int area);
    void ResetType(BufferFlagType type);
    void ResetAll();

    inline const std::unordered_map<int, ChangedEntry>& getChangedMap() const {
        //Come get changed ma torna la mappa direttamente, più veloce
        return _changed;
    }
     
    inline ChangedTypeView getChangedMapByType(BufferFlagType type) const {
        // compatibile con le chiamate esistenti: non salta i virtuali
        return ChangedTypeView(_changed, type, this, false);
    }

    inline ChangedTypeView getChangedMapByType(BufferFlagType type, bool skipVirtual) const {
        // nuova versione con flag per scartare i virtuali
        return ChangedTypeView(_changed, type, this, skipVirtual);
    }


    char* GetName(int area);
    int Compare(int area, BufferFlagType type, long value);
    std::vector<int> getNeverInitialized();
    std::vector<int> getInitializedMultipleTimes();

    size_t size() const {
        return _items;
    }

    inline const BufferSourceInfo& getFieldEntry(int area) const {
        return _buffer[area].Data[Field];
    }

    class Diagnostics {
    public:
        static void ReportVirtualAreas(Buffer& buf);
        static void ReportUnusedBufferAreas(Buffer& buf);
        static void ReportNeverInitialized(Buffer& buf);
        static void ReportMultipleInitialized(Buffer& buf);
    };

  private: 
    void markChanged(int area, BufferFlagType type);
    AreaTracker tracker;
    int _items;
    BufferInfo *_buffer;
    BufferArrayInfo _toPanelRead;
    
    std::unordered_map<int, ChangedEntry> _changed;

};

#endif
