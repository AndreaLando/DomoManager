#ifndef DMAEECore_HPP
#define DMAEECore_HPP


/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑06‑20
   Note:
                    • Nessuna

   ============================================================================ */

#include <Arduino.h>
#include <ArduinoJson.h>

#include "DMBaseClass.hpp"
// ============================================================
//  CONFIGURAZIONE VARIABILI
// ============================================================

enum class AEEVarType { BOOL, INT, FLOAT, STRING };

enum class AEEDirection {
    FrontendToModule,
    ModuleToFrontend,
    Bidirectional
};

enum class AEEVarSourceType {
    None,
    BufferArea,
    GenericSensor,
    Function
};

// ============================================================
//  DEFINIZIONE VARIABILE AEE
// ============================================================

    struct AEEVarDef {
        const char* name;
        AEEDirection direction;
        AEEVarSourceType sourceType = AEEVarSourceType::None;

        int bufferArea = -1;
        int bitIndex   = -1;

        GenericSensorConfig sensorCfg;

        float (*fnFloat)() = nullptr;
        int   (*fnInt)()   = nullptr;
        bool  (*fnBool)()  = nullptr;

        AEEVarType varType = AEEVarType::BOOL;

        float minDelta = 0.0f;
        float scale    = 1.0f;

        AEEVarDef(
        const char* n,
        AEEDirection dir,
        AEEVarSourceType src,
        int area,
        int bit,
        const GenericSensorConfig& cfg,
        float (*ff)(),
        int (*fi)(),
        bool (*fb)(),
        AEEVarType vt,
        float md = 0.0f,
        float sc = 1.0f
    )
        : name(n),
        direction(dir),
        sourceType(src),
        bufferArea(area),
        bitIndex(bit),
        sensorCfg(cfg),
        fnFloat(ff),
        fnInt(fi),
        fnBool(fb),
        varType(vt),
        minDelta(md),
        scale(sc)
    {}

    // ------------------------------------------------------------
    // COSTRUTTORE SHORT-FORM (per il tuo main)
    // ------------------------------------------------------------
    AEEVarDef(const char* n, const char* typeStr, AEEDirection dir)
        : name(n),
          direction(dir),
          sourceType(AEEVarSourceType::None),
          bufferArea(-1),
          bitIndex(-1),
          sensorCfg({ GenericSensorConfig::Type::CONSTANT, 0, 1.0f }),
          fnFloat(nullptr),
          fnInt(nullptr),
          fnBool(nullptr),
          varType(
              strcmp(typeStr, "bool") == 0  ? AEEVarType::BOOL  :
              strcmp(typeStr, "int") == 0   ? AEEVarType::INT   :
              strcmp(typeStr, "float") == 0 ? AEEVarType::FLOAT :
                                              AEEVarType::STRING
          ),
          minDelta(0.0f),
          scale(1.0f)
    {}

    // ------------------------------------------------------------
    // COSTRUTTORE DA JSON (per schema dinamico lato SLAVE)
    // ------------------------------------------------------------
    AEEVarDef(const JsonObject& o)
    {
        name = strdup(o["name"] | "");

        const char* t = o["type"] | "bool";
        varType =
            strcmp(t, "bool") == 0  ? AEEVarType::BOOL  :
            strcmp(t, "int") == 0   ? AEEVarType::INT   :
            strcmp(t, "float") == 0 ? AEEVarType::FLOAT :
                                      AEEVarType::STRING;

        const char* d = o["dir"] | "M2F";
        if (strcmp(d, "F2M") == 0) direction = AEEDirection::FrontendToModule;
        else if (strcmp(d, "M2F") == 0) direction = AEEDirection::ModuleToFrontend;
        else direction = AEEDirection::Bidirectional;

        sourceType = AEEVarSourceType::None;
        bufferArea = -1;
        bitIndex   = -1;

        sensorCfg = { GenericSensorConfig::Type::CONSTANT, 0, 1.0f };

        fnFloat = nullptr;
        fnInt   = nullptr;
        fnBool  = nullptr;

        minDelta = 0.0f;
        scale    = 1.0f;
    }

    AEEVarDef() = default;
};

// ============================================================
//  BASE CLASS
// ============================================================

class AEEVariableBase {
public:
    AEEVarDef def;
    uint32_t lastChange = 0;

    AEEVariableBase(const AEEVarDef& d) : def(d) {}
    virtual ~AEEVariableBase() {}

    virtual bool hasChanged() = 0;
    virtual void clearChanged() = 0;

    virtual void toJson(JsonDocument& doc) const = 0;
    virtual bool fromJson(const JsonVariant& v) = 0;

    virtual void forceChanged(unsigned long now) = 0;

};

// ============================================================
//  TEMPLATE VARIABILE
// ============================================================

template<typename T>
class AEEVariable : public AEEVariableBase {
private:
    T value;
    bool changed = false;

public:
    using Callback = std::function<void(const T&)>;
    Callback onChange;

    AEEVariable(const AEEVarDef& d) : AEEVariableBase(d) {}

    void set(const T& v, unsigned long now) {
        if (v == value) return;

        if constexpr (std::is_same<T,int>::value || std::is_same<T,float>::value) {
            float delta = fabs((float)v - (float)value);
            if (def.minDelta > 0.0f && delta < def.minDelta)
                return;
        }

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

    void forceChanged(unsigned long now) override {
        lastChange = now;
        changed = true;
    }

};

// ============================================================
//  REGISTRY
// ============================================================

class AEERegistry {
private:
    std::vector<AEEVariableBase*> vars;

public:
    void add(AEEVariableBase* v) { vars.push_back(v); }

    AEEVariableBase* find(const String& name) {
        for (auto* v : vars)
            if (strcmp(name.c_str(), v->def.name) == 0)
                return v;
        return nullptr;
    }

    template<typename F>
    void forEach(F fn) const {
        for (auto* v : vars)
            fn(v);
    }

    template<typename F>
    void forEachChanged(F fn) {
        for (auto* v : vars)
            if (v->hasChanged())
                fn(v);
    }

    void clearAllChanged() {
        for (auto* v : vars)
            v->clearChanged();
    }

    size_t size() const {
        return vars.size();
    }

    bool hasChanges() const {
        for (auto* v : vars)
            if (v->hasChanged())
                return true;
        return false;
    }

    void forceAllChanged(unsigned long now) {
        for (auto* v : vars)
            v->forceChanged(now);
    }

};

// ============================================================
//  MANAGEMENT
// ============================================================

class AEEManagement {
public:
    std::vector<AEEVariableBase*> vars;

    AEEManagement(const AEEVarDef* defs, size_t count) {
        vars.reserve(count);
        for (size_t i = 0; i < count; i++) {
            const auto& d = defs[i];
            AEEVariableBase* v = nullptr;

            switch (d.varType) {
                case AEEVarType::BOOL:  v = new AEEVariable<bool>(d); break;
                case AEEVarType::INT:   v = new AEEVariable<int>(d); break;
                case AEEVarType::FLOAT: v = new AEEVariable<float>(d); break;
                case AEEVarType::STRING:v = new AEEVariable<String>(d); break;
            }

            vars.push_back(v);
        }
    }

    void registerAll(AEERegistry& reg) {
        for (auto* v : vars)
            reg.add(v);
    }
};

// ============================================================
//  PROTOCOL
// ============================================================

class AEEProtocol {
private:
    bool isMaster=false;
    uint32_t seqCounter = 0;

public:
    AEEProtocol() = default;              // 🔥 default ctor
    AEEProtocol(bool master) : isMaster(master) {}

    String serializeChangedJSON(AEERegistry& reg) {
        DynamicJsonDocument doc(1024);

        reg.forEachChanged([&](AEEVariableBase* v){
            AEEDirection dir = v->def.direction;

            bool shouldSend =
                (isMaster  && (dir == AEEDirection::FrontendToModule || dir == AEEDirection::Bidirectional)) ||
                (!isMaster && (dir == AEEDirection::ModuleToFrontend  || dir == AEEDirection::Bidirectional));

            if (shouldSend)
                v->toJson(doc);
        });

        if (doc.size() == 0) return "";

        doc["seq"] = ++seqCounter;

        String out;
        serializeJson(doc, out);
        return out;
    }

    bool parseJSON(const String& json, AEERegistry& reg) {
        StaticJsonDocument<1024> doc;
        if (deserializeJson(doc, json)) return false;

        for (auto kv : doc.as<JsonObject>()) {
            const char* key = kv.key().c_str();
            if (strcmp(key, "seq") == 0) continue;

            AEEVariableBase* v = reg.find(key);
            if (!v) continue;

            AEEDirection dir = v->def.direction;
            bool canApply =
                (isMaster  && (dir == AEEDirection::ModuleToFrontend || dir == AEEDirection::Bidirectional)) ||
                (!isMaster && (dir == AEEDirection::FrontendToModule || dir == AEEDirection::Bidirectional));

            if (canApply) {
                bool changedBefore = v->hasChanged();
                v->fromJson(kv.value());

                // 🔥 SOLO SLAVE deve azzerare i changed
                // MASTER deve mantenere changed per Task_AEE_Monitor
                if (!isMaster) {
                    v->clearChanged();
                }
            }

        }

        return true;
    }

        // ============================================================
    //  SCHEMA EXPORT / IMPORT
    // ============================================================
    void ExportSchema(const AEEVarDef* defs, size_t count, JsonArray out)
    {
        for (size_t i = 0; i < count; i++) {
            const auto& d = defs[i];
            JsonObject o = out.add<JsonObject>();
            o["name"] = d.name;

            switch (d.varType) {
                case AEEVarType::BOOL:  o["type"] = "bool"; break;
                case AEEVarType::INT:   o["type"] = "int"; break;
                case AEEVarType::FLOAT: o["type"] = "float"; break;
                case AEEVarType::STRING:o["type"] = "string"; break;
            }

            switch (d.direction) {
                case AEEDirection::FrontendToModule: o["dir"] = "F2M"; break;
                case AEEDirection::ModuleToFrontend: o["dir"] = "M2F"; break;
                case AEEDirection::Bidirectional:    o["dir"] = "BIDIR"; break;
            }
        }
    }

    // ------------------------------------------------------------
    // OVERLOAD: esporta schema direttamente dal registry
    // ------------------------------------------------------------
    void ExportSchema(const AEERegistry& reg, JsonArray out)
    {
        reg.forEach([&](AEEVariableBase* v){
            const auto& d = v->def;

            JsonObject o = out.add<JsonObject>();
            o["name"] = d.name;

            switch (d.varType) {
                case AEEVarType::BOOL:  o["type"] = "bool"; break;
                case AEEVarType::INT:   o["type"] = "int"; break;
                case AEEVarType::FLOAT: o["type"] = "float"; break;
                case AEEVarType::STRING:o["type"] = "string"; break;
            }

            switch (d.direction) {
                case AEEDirection::FrontendToModule: o["dir"] = "F2M"; break;
                case AEEDirection::ModuleToFrontend: o["dir"] = "M2F"; break;
                case AEEDirection::Bidirectional:    o["dir"] = "BIDIR"; break;
            }
        });
    }

    AEEVarDef* ImportSchema(const JsonArray& arr, size_t& outCount)
    {
        outCount = arr.size();
        AEEVarDef* defs = new AEEVarDef[outCount];

        for (size_t i = 0; i < outCount; i++) {
            defs[i] = AEEVarDef(arr[i].as<JsonObject>());
        }

        return defs;
    }

    // ============================================================
    //  COSTRUZIONE COMPLETA DELLA AEE DA UNO SCHEMA JSON (SLAVE)
    // ============================================================
    AEEManagement* BuildManagementFromSchema(const String& json, AEERegistry& reg)
    {
        StaticJsonDocument<2048> doc;
        if (deserializeJson(doc, json))
            return nullptr;

        if (!doc.containsKey("schema"))
            return nullptr;

        JsonArray arr = doc["schema"].as<JsonArray>();

        size_t count = 0;
        AEEVarDef* defs = ImportSchema(arr, count);

        AEEManagement* mgr = new AEEManagement(defs, count);
        mgr->registerAll(reg);

        delete[] defs;
        return mgr;
    }

};


// ============================================================
//  HELPER CAST
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
//  IMPLEMENTAZIONI TEMPLATE
// ============================================================

template<typename T>
bool AEEVariable<T>::fromJson(const JsonVariant& v) {
    if (!v.is<T>()) return false;

    T newVal = v.as<T>();
    if (newVal == this->get()) return false;

    LOG_IF("SLAVE::AEE::APPLY",
           "Applying %s = %s",
           def.name,
           String(newVal).c_str());
           
    this->set(newVal, millis());
    return true;
}

template<>
inline bool AEEVariable<String>::fromJson(const JsonVariant& v) {
    if (!v.is<const char*>()) return false;

    const char* incoming = v.as<const char*>();
    if (strcmp(this->get().c_str(), incoming) == 0) return false;

    set(String(incoming), millis());
    return true;
}

template<>
inline void AEEVariable<String>::toJson(JsonDocument& doc) const {
    doc[def.name] = value;
}

#endif
