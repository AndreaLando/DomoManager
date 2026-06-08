#ifndef DMAEE_HPP
#define DMAEE_HPP


/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑04‑24
   Note:
                    • Nessuna

   ============================================================================ */

#include <Arduino.h>
#include <ArduinoJson.h>

#include "DMPLC.h"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================
//  CONFIGURAZIONE VARIABILI
// ============================================================


enum class AEEVarType { BOOL, INT, FLOAT, STRING };

enum class AEEDirection {
    FrontendToModule,   // il frontend scrive → il modulo legge
    ModuleToFrontend,   // il modulo scrive → il frontend legge
    Bidirectional       // entrambi
};

enum class AEEVarSourceType {
    None,
    BufferArea,
    GenericSensor,
    Function
};

struct AEEVarDef {
    const char* name;
    AEEDirection direction;
    AEEVarSourceType sourceType = AEEVarSourceType::None;

    // --- Sorgenti possibili ---
    int bufferArea = -1;                       // per BufferArea
    int bitIndex   = -1;                        //Eventuale bit della buffer area
    
    GenericSensor::Config sensorCfg;           // per GenericSensor
    
    float (*fnFloat)() = nullptr;
    int   (*fnInt)()   = nullptr;
    bool  (*fnBool)()  = nullptr;

    AEEVarType varType = AEEVarType::BOOL;

    // 🔥 NUOVO: soglia per variabile
    float minDelta = 0.0f;   // default: nessuna soglia
    float scale = 1.0f;   // default: nessuna scala
};

// ============================================================
//  BASE CLASS — Interfaccia comune per tutte le variabili AEE
// ============================================================
class AEEVariableBase {
public:
    AEEVarDef def;
    uint32_t lastChange = 0;

    AEEVariableBase(const AEEVarDef& d)
        : def(d) {}

    virtual ~AEEVariableBase() {}

    virtual bool hasChanged() = 0;
    virtual void clearChanged() = 0;

    virtual void toJson(JsonDocument& doc) const = 0;
    virtual bool fromJson(const JsonVariant& v) = 0;
};

// ============================================================
//  TEMPLATE — Variabile tipizzata con change‑tracking
// ============================================================
template<typename T>
class AEEVariable : public AEEVariableBase {
private:
    T value;
    bool changed = false;

public:
    using Callback = std::function<void(const T&)>;
    Callback onChange;

    AEEVariable(const AEEVarDef& d)
        : AEEVariableBase(d)
    {
        if constexpr (std::is_same<T,bool>::value)   def.varType = AEEVarType::BOOL;
        if constexpr (std::is_same<T,int>::value)    def.varType = AEEVarType::INT;
        if constexpr (std::is_same<T,float>::value)  def.varType = AEEVarType::FLOAT;
        if constexpr (std::is_same<T,String>::value) def.varType = AEEVarType::STRING;
    }

    void set(const T& v, unsigned long now) {
        // 🔥 1) Se non cambia, ignora
        if (v == value) return;

        // 🔥 2) Soglia semplice per INT e FLOAT
        if constexpr (std::is_same<T, int>::value || std::is_same<T, float>::value) {
            float delta = fabs((float)v - (float)value);
            float minDelta = def.minDelta;

            if (minDelta > 0.0f && delta < minDelta) {
                // variazione troppo piccola → ignora
                return;
            }
        }

        // 🔥 3) Aggiornamento effettivo
        lastChange = now;
        value = v;
        changed = true;

        if (onChange) onChange(value);
    }

    const T& get() const { return value; }

    bool hasChanged() override { return changed; }
    void clearChanged() override { changed = false; }

    void toJson(JsonDocument& doc) const override {
        doc[def.name] = value;
    }

    bool fromJson(const JsonVariant& v) override;
};


// ============================================================
//  HELPER PER CAST SENZA RTTI (NO dynamic_cast su Arduino)
// ============================================================
template<typename T>
AEEVariable<T>* as(AEEVariableBase* v) {
    if constexpr (std::is_same<T,bool>::value)
        return (v->def.varType == AEEVarType::BOOL) ? static_cast<AEEVariable<bool>*>(v) : nullptr;

    if constexpr (std::is_same<T,int>::value)
        return (v->def.varType == AEEVarType::INT) ? static_cast<AEEVariable<int>*>(v) : nullptr;

    if constexpr (std::is_same<T,float>::value)
        return (v->def.varType == AEEVarType::FLOAT) ? static_cast<AEEVariable<float>*>(v) : nullptr;

    if constexpr (std::is_same<T,String>::value)
        return (v->def.varType == AEEVarType::STRING) ? static_cast<AEEVariable<String>*>(v) : nullptr;

    return nullptr;
}


// ============================================================
//  SPECIALIZZAZIONI NECESSARIE
// ============================================================
// ============================================================
//  IMPLEMENTAZIONE GENERICA DI fromJson()  (bool, int, float)
// ============================================================
template<typename T>
bool AEEVariable<T>::fromJson(const JsonVariant& v) {
    if (!v.is<T>()) return false;

    T newVal = v.as<T>();
    if (newVal == this->get()) return false;

    this->set(newVal, millis());
    return true;
}

template<>
inline bool AEEVariable<String>::fromJson(const JsonVariant& v) {
    if (!v.is<const char*>()) return false;

    const char* incoming = v.as<const char*>();
    if (strcmp(this->get().c_str(), incoming) == 0) return false;  // evita changed inutile

    set(String(incoming), millis());
    return true;
}

template<>
inline void AEEVariable<String>::toJson(JsonDocument& doc) const {
    doc[def.name] = value;
}


// ============================================================
//  REGISTRY — Contenitore centrale delle variabili AEE
// ============================================================
class AEERegistry {
private:
    std::vector<AEEVariableBase*> vars;

public:
    void add(AEEVariableBase* v) {
        vars.push_back(v);
    }

    AEEVariableBase* find(const String& name) {
        for (auto* v : vars)
            if (name == v->def.name)
                return v;
        return nullptr;
    }


    template<typename F>
    void forEach(F func) const {
        for (auto* v : vars)
            func(v);
    }

    template<typename F>
    void forEachChanged(F func) {
        for (auto* v : vars)
            if (v->hasChanged())
                func(v);
    }

    void clearAllChanged() {
        for (auto* v : vars)
            v->clearChanged();
    }
};


// ============================================================
//  CLASS 1 — PROTOCOLLO JSON
// ============================================================
class AEEProtocol {
private:
    inline static uint32_t seqCounter=0;
public:
    static String serializeChangedJSON(AEERegistry& reg) {
        size_t count = 0;
        reg.forEachChanged([&](AEEVariableBase* v){
            if (v->def.direction != AEEDirection::FrontendToModule &&
                v->def.direction != AEEDirection::Bidirectional)
                return;
            count++;
        });

        // calcolo dinamico
        DynamicJsonDocument doc(JSON_OBJECT_SIZE(count) + 128);

        reg.forEachChanged([&](AEEVariableBase* v){
            if (v->def.direction == AEEDirection::FrontendToModule ||
                v->def.direction == AEEDirection::Bidirectional)
            {
                v->toJson(doc);
            }
        });

        if (doc.size() == 0) return "";

        // 🔥 aggiungi sequence number
        doc["seq"] = ++seqCounter;

        size_t size = measureJson(doc) + 32;
        if (size > 900) {
            LOG_WF("AEE", "JSON troppo grande (%u bytes) → non inviato", size);
            return "";
        }

        String out;
        serializeJson(doc, out);
        return out;
    }

    static bool parseJSON(const String& json, AEERegistry& reg) {
        size_t size = json.length();
        if (size > 900) {
            LOG_WF("AEE", "JSON ricevuto troppo grande (%u bytes) → ignorato", size);
            return false;
        }

        StaticJsonDocument<1024> doc;
        auto err = deserializeJson(doc, json);

        if (err) {
            LOG_WF("AEE", "Errore JSON: %s", err.c_str());
            return false;
        }

        JsonObject obj = doc.as<JsonObject>();

        for (auto kv : obj) {
            const char* key = kv.key().c_str();
            AEEVariableBase* v = reg.find(key);
            if (!v) {
                LOG_WF("AEE", "Variabile sconosciuta '%s' → ignorata", key);
                continue;
            }

            if (v->def.direction == AEEDirection::ModuleToFrontend ||
                v->def.direction == AEEDirection::Bidirectional)
            {
                if (!v->fromJson(kv.value())) {
                    LOG_WF("AEE", "Tipo non valido per '%s'", key);
                } else {
                    v->clearChanged();   // evita loop di ritrasmissione
                }
            }
            else {
                LOG_DF("AEE", "Ignorata '%s' (direzione non compatibile)", key);
            }
        }

        return true;
    }
};


// ============================================================
//  CLASS 2 — MANAGEMENT (creazione + lookup + registrazione)
// ============================================================
class AEEManagement {
public:
    std::vector<AEEVariableBase*> vars;

    AEEManagement(const AEEVarDef* defs, size_t count) {
        vars.reserve(count);

        for (size_t i = 0; i < count; i++) {
            const auto& def = defs[i];

            AEEVariableBase* v = nullptr;

            switch (def.varType) {
                case AEEVarType::BOOL:  v = new AEEVariable<bool>(def); break;
                case AEEVarType::INT:   v = new AEEVariable<int>(def); break;
                case AEEVarType::FLOAT: v = new AEEVariable<float>(def); break;
                case AEEVarType::STRING:v = new AEEVariable<String>(def); break;
            }

            vars.push_back(v);
        }
    }

    AEEVariableBase* get(const char* name) {
        for (auto* v : vars)
            if (strcmp(v->def.name, name) == 0)
                return v;
        return nullptr;
    }


    void registerAll(AEERegistry& reg) {
        for (auto* v : vars)
            reg.add(v);
    }
};

class DMAEE {
public:

    // ------------------------------------------------------------
    // 1) Struttura per gli update AEE
    // ------------------------------------------------------------
    struct Update {
        AEEVariableBase* var;
        float rawValue;
        bool valid;
    };

    // ------------------------------------------------------------
    // 2) Costruzione degli update dal buffer
    // ------------------------------------------------------------
    template<typename ChangedView>
    static bool BuildUpdatesFromBuffer(
        AEEManagement& mgr,
        Buffer& buf,
        const ChangedView& changed,
        std::vector<Update>& out)
    {
        bool hasUpdates = false;

        // Numero aree del buffer
        const int AREA_COUNT = buf.size();

        // Precalcolo aree cambiate → O(1)
        bool areaChanged[AREA_COUNT];
        memset(areaChanged, 0, sizeof(areaChanged));

        for (auto& kv : changed) {
            int area = kv.first / BufferFlagType_Count;
            if ((unsigned)area < AREA_COUNT)
                areaChanged[area] = true;
        }

        for (auto* v : mgr.vars)
        {
            if (v->def.direction == AEEDirection::ModuleToFrontend)
                continue;

            Update u{ v, 0.0f, false };

            switch (v->def.sourceType)
            {
                case AEEVarSourceType::BufferArea:
                {
                    int area = v->def.bufferArea;

                    if ((unsigned)area >= AREA_COUNT)
                        break;

                    if (!areaChanged[area])
                        break;

                    long raw = buf.getValueFast(area);

                    if (v->def.bitIndex >= 0)
                        raw = (raw >> v->def.bitIndex) & 1;

                    u.rawValue = (float)raw;
                    u.valid = true;
                }
                break;

                case AEEVarSourceType::GenericSensor:
                {
                    int raw = GenericSensor::read(v->def.sensorCfg, buf);
                    u.rawValue = (float)raw;
                    u.valid = true;
                }
                break;

                case AEEVarSourceType::Function:
                {
                    if (v->def.fnFloat) {
                        u.rawValue = v->def.fnFloat();
                        u.valid = true;
                    }
                    else if (v->def.fnInt) {
                        u.rawValue = (float)v->def.fnInt();
                        u.valid = true;
                    }
                    else if (v->def.fnBool) {
                        u.rawValue = v->def.fnBool() ? 1.0f : 0.0f;
                        u.valid = true;
                    }
                }
                break;

                default:
                    break;
            }

            if (u.valid) {
                out.push_back(u);
                hasUpdates = true;
            }
        }

        return hasUpdates;
    }


    // ------------------------------------------------------------
    // 3) Applicazione degli update alle variabili AEE
    // ------------------------------------------------------------
    static void ApplyUpdates(
    const std::vector<Update>& updates,
    unsigned long now)
    {
        for (auto& u : updates)
        {
            auto* v = u.var;

            // BOOL
            if (auto* b = as<bool>(v)) {
                bool old = b->get();
                bool nv = (u.rawValue != 0.0f);
                b->set(nv, now);
                if (b->get() != old)
                    LOG_IF("AEE", "Updated %s = %d", v->def.name, nv);
            }

            // INT
            else if (auto* i = as<int>(v)) {
                int old = i->get();
                int nv = (int)(u.rawValue * v->def.scale);
                i->set(nv, now);
                if (i->get() != old)
                    LOG_IF("AEE", "Updated %s = %d", v->def.name, nv);
            }

            // FLOAT
            else if (auto* f = as<float>(v)) {
                float old = f->get();
                float nv = u.rawValue * v->def.scale;
                f->set(nv, now);
                if (f->get() != old)
                    LOG_IF("AEE", "Updated %s = %.2f", v->def.name, nv);
            }
        }
    }

};
#endif
