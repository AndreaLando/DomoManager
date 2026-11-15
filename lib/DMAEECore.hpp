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

#include "DMBaseClassCore.hpp"

class AEERegistry;   // 🔥 forward declaration

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
        String name;

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
        name = o["name"] | "";

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
    AEERegistry* owner = nullptr;
};

// ============================================================
//  TEMPLATE VARIABILE
// ============================================================

template<typename T>
class AEEVariable : public AEEVariableBase {
private:
    T value{};
    bool changed = false;

public:
    using Callback = std::function<void(const T&)>;
    Callback onChange;

    explicit AEEVariable(const AEEVarDef& d)
        : AEEVariableBase(d)
    {
    }

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
        
        // 🔥 notifica al registry
        if (owner)
            owner->notifyChanged(this);
    }

    const T& get() const { return value; }

    bool hasChanged() override { return changed; }
    void clearChanged() override { changed = false; }

    void toJson(JsonDocument& doc) const override
    {
        doc[def.name.c_str()] = value;
    }

    bool fromJson(const JsonVariant& v) override;

    void forceChanged(unsigned long now) override {
        lastChange = now;
        changed = true;

        if (owner)
            owner->notifyChanged(this);
    }

};

// ============================================================
//  REGISTRY
// ============================================================

class AEERegistry {
private:
    std::vector<AEEVariableBase*> vars;
    std::vector<AEEVariableBase*> changedVars;   // 🔥 nuovo

public:
    void add(AEEVariableBase* v)
    {
        if (!v)
            return;

        for (auto* current : vars)
        {
            if (current == v)
                return;
        }

        vars.push_back(v);
    }

    void clear()
    {
        // Il registry non possiede le variabili:
        // elimina soltanto i riferimenti.
        vars.clear();
        changedVars.clear();
    }

    void notifyChanged(AEEVariableBase* v)
    {
        if (!v)
            return;

        for (auto* changed : changedVars)
        {
            if (changed == v)
                return;
        }

        changedVars.push_back(v);
    }

    template<typename F>
    void forEachChanged(F fn) {
        for (auto* v : changedVars)
            fn(v);
    }

    // 🔥 vecchia API (compatibilità)
    template<typename F>
    void forEach(F fn) const {
        for (auto* v : vars)
            fn(v);
    }

    // 🔥 nuova API
    void clearAllChanged() {
        for (auto* v : changedVars)
            v->clearChanged();
        changedVars.clear();
    }

    // ============================================================
    // FORCE ALL CHANGED
    //
    // forceChanged() notifica autonomamente il registry tramite:
    //
    //     owner->notifyChanged(this)
    //
    // Non deve quindi essere chiamata nuovamente notifyChanged().
    // ============================================================

    void forceAllChanged(
        unsigned long now)
    {
        for (
            AEEVariableBase* variable :
            vars
        )
        {
            if (!variable)
            {
                continue;
            }

            variable->forceChanged(
                now
            );
        }
    }
    
    bool hasChanges() const {
        return !changedVars.empty();
    }

    size_t size() const { return vars.size(); }

    AEEVariableBase* find(const String& name)
    {
        for (auto* v : vars)
        {
            if (v && name == v->def.name)
                return v;
        }

        return nullptr;
    }

    AEEVariableBase* find(const String& name) const
    {
        for (auto* v : vars)
        {
            if (v && name == v->def.name)
                return v;
        }

        return nullptr;
    }
};

// ============================================================
//  MANAGEMENT
// ============================================================

// ============================================================
// MANAGEMENT
// ============================================================

class AEEManagement
{
public:

    // ========================================================
    // VALIDATION RESULT
    // ========================================================

    struct ValidationResult
    {
        bool valid =
            false;

        size_t index =
            0;

        const char* reason =
            nullptr;
    };

private:

    bool _valid =
        false;

    // ========================================================
    // ENUM VALIDATION
    // ========================================================

    static bool isValidDirection(
        AEEDirection direction)
    {
        switch (direction)
        {
            case AEEDirection::FrontendToModule:
            case AEEDirection::ModuleToFrontend:
            case AEEDirection::Bidirectional:
                return true;

            default:
                return false;
        }
    }

    static bool isValidSourceType(
        AEEVarSourceType sourceType)
    {
        switch (sourceType)
        {
            case AEEVarSourceType::None:
            case AEEVarSourceType::BufferArea:
            case AEEVarSourceType::GenericSensor:
            case AEEVarSourceType::Function:
                return true;

            default:
                return false;
        }
    }

    static bool isValidVariableType(
        AEEVarType variableType)
    {
        switch (variableType)
        {
            case AEEVarType::BOOL:
            case AEEVarType::INT:
            case AEEVarType::FLOAT:
            case AEEVarType::STRING:
                return true;

            default:
                return false;
        }
    }

    // ========================================================
    // SINGLE DEFINITION VALIDATION
    // ========================================================

    static const char* validateDefinition(
        const AEEVarDef& definition)
    {
        // ----------------------------------------------------
        // NAME
        // ----------------------------------------------------

        if (definition.name.length() == 0)
        {
            return
                "variable name is empty";
        }

        // ----------------------------------------------------
        // ENUMS
        // ----------------------------------------------------

        if (!isValidDirection(
                definition.direction))
        {
            return
                "invalid direction";
        }

        if (!isValidSourceType(
                definition.sourceType))
        {
            return
                "invalid source type";
        }

        if (!isValidVariableType(
                definition.varType))
        {
            return
                "invalid variable type";
        }

        // ----------------------------------------------------
        // NUMERIC PARAMETERS
        // ----------------------------------------------------

        if (!isfinite(
                definition.minDelta))
        {
            return
                "minDelta is not finite";
        }

        if (definition.minDelta < 0.0f)
        {
            return
                "minDelta is negative";
        }

        if (!isfinite(
                definition.scale))
        {
            return
                "scale is not finite";
        }

        if (definition.scale == 0.0f)
        {
            return
                "scale is zero";
        }

        if (
            definition.bitIndex < -1 ||
            definition.bitIndex > 31
        )
        {
            return
                "bitIndex outside -1..31";
        }

        // ----------------------------------------------------
        // SOURCE-SPECIFIC VALIDATION
        // ----------------------------------------------------

        switch (definition.sourceType)
        {
            // =================================================
            // NONE
            //
            // Ammesso normalmente per valori ricevuti dal peer:
            // ModuleToFrontend o Bidirectional.
            // =================================================

            case AEEVarSourceType::None:
            {
                /*
                * SourceType::None è valido per:
                *
                * - variabili di sistema prive di sorgente locale;
                * - definizioni importate da uno schema remoto;
                * - variabili FrontendToModule ricevute dal peer;
                * - variabili ModuleToFrontend prodotte dal peer;
                * - variabili Bidirectional gestite dal protocollo.
                *
                * La direzione non determina, da sola, la validità della
                * sorgente None. Il significato dipende dal ruolo locale
                * master/slave e dall'origine statica o remota dello schema.
                */

                if (
                    definition.fnFloat ||
                    definition.fnInt ||
                    definition.fnBool
                )
                {
                    return
                        "None source has function callback";
                }

                if (definition.bufferArea != -1)
                {
                    return
                        "None source has buffer area";
                }

                if (definition.bitIndex != -1)
                {
                    return
                        "None source has bit index";
                }

                break;
            }

            // =================================================
            // BUFFER AREA
            // =================================================

            case AEEVarSourceType::BufferArea:
            {
                if (definition.bufferArea < 0)
                {
                    return
                        "BufferArea source has invalid area";
                }

                if (
                    definition.fnFloat ||
                    definition.fnInt ||
                    definition.fnBool
                )
                {
                    return
                        "BufferArea source has function callback";
                }

                if (
                    definition.bitIndex >= 0 &&
                    definition.varType !=
                        AEEVarType::BOOL
                )
                {
                    return
                        "bit-indexed BufferArea must be BOOL";
                }

                if (
                    definition.direction ==
                        AEEDirection::ModuleToFrontend
                )
                {
                    return
                        "ModuleToFrontend variable cannot use local BufferArea";
                }

                break;
            }

            // =================================================
            // GENERIC SENSOR
            // =================================================

            case AEEVarSourceType::GenericSensor:
            {
                if (
                    definition.fnFloat ||
                    definition.fnInt ||
                    definition.fnBool
                )
                {
                    return
                        "GenericSensor source has function callback";
                }

                if (
                    definition.direction ==
                        AEEDirection::ModuleToFrontend
                )
                {
                    return
                        "ModuleToFrontend variable cannot use local GenericSensor";
                }

                if (
                    definition.varType ==
                        AEEVarType::STRING
                )
                {
                    return
                        "GenericSensor STRING is unsupported";
                }

                break;
            }

            // =================================================
            // FUNCTION
            // =================================================

            case AEEVarSourceType::Function:
            {
                if (
                    definition.direction ==
                        AEEDirection::ModuleToFrontend
                )
                {
                    return
                        "ModuleToFrontend variable cannot use local Function";
                }

                const unsigned callbackCount =
                    (
                        definition.fnFloat
                            ? 1U
                            : 0U
                    ) +
                    (
                        definition.fnInt
                            ? 1U
                            : 0U
                    ) +
                    (
                        definition.fnBool
                            ? 1U
                            : 0U
                    );

                if (callbackCount == 0)
                {
                    return
                        "Function source has no callback";
                }

                if (callbackCount > 1)
                {
                    return
                        "Function source has multiple callbacks";
                }

                switch (definition.varType)
                {
                    case AEEVarType::FLOAT:
                    {
                        if (!definition.fnFloat)
                        {
                            return
                                "FLOAT Function requires fnFloat";
                        }

                        break;
                    }

                    case AEEVarType::INT:
                    {
                        if (!definition.fnInt)
                        {
                            return
                                "INT Function requires fnInt";
                        }

                        break;
                    }

                    case AEEVarType::BOOL:
                    {
                        if (!definition.fnBool)
                        {
                            return
                                "BOOL Function requires fnBool";
                        }

                        break;
                    }

                    case AEEVarType::STRING:
                    {
                        return
                            "STRING Function is unsupported";
                    }

                    default:
                    {
                        return
                            "invalid Function variable type";
                    }
                }

                break;
            }

            default:
            {
                return
                    "invalid source type";
            }
        }

        return
            nullptr;
    }

public:

    std::vector<AEEVariableBase*>
        vars;

    // ========================================================
    // COPY / MOVE POLICY
    // ========================================================

    AEEManagement(
        const AEEManagement&) =
            delete;

    AEEManagement& operator=(
        const AEEManagement&) =
            delete;

    AEEManagement(
        AEEManagement&&) =
            delete;

    AEEManagement& operator=(
        AEEManagement&&) =
            delete;

    // ========================================================
    // COMPLETE DEFINITIONS VALIDATION
    // ========================================================

    static ValidationResult ValidateDefinitions(
        const AEEVarDef* definitions,
        size_t count)
    {
        ValidationResult result;

        if (!definitions)
        {
            result.reason =
                "definitions pointer is null";

            return result;
        }

        if (count == 0)
        {
            result.reason =
                "definition count is zero";

            return result;
        }

        for (
            size_t index = 0;
            index < count;
            ++index
        )
        {
            const AEEVarDef& definition =
                definitions[index];

            const char* validationError =
                validateDefinition(
                    definition
                );

            if (validationError)
            {
                result.index =
                    index;

                result.reason =
                    validationError;

                return result;
            }

            // ------------------------------------------------
            // DUPLICATE NAME
            // ------------------------------------------------

            for (
                size_t previousIndex = 0;
                previousIndex < index;
                ++previousIndex
            )
            {
                if (
                    definitions[
                        previousIndex
                    ].name ==
                    definition.name
                )
                {
                    result.index =
                        index;

                    result.reason =
                        "duplicate variable name";

                    return result;
                }
            }
        }

        result.valid =
            true;

        result.reason =
            nullptr;

        return result;
    }

    // ========================================================
    // VALIDATION LOG
    // ========================================================

    static bool ValidateAndLog(
        const AEEVarDef* definitions,
        size_t count,
        const char* context = nullptr)
    {
        const ValidationResult result =
            ValidateDefinitions(
                definitions,
                count
            );

        if (result.valid)
        {
            LOG_IF(
                "AEE::VALIDATION",
                "Definitions valid: "
                "context=%s count=%u",
                context
                    ? context
                    : "AEE",
                static_cast<unsigned>(
                    count
                )
            );

            return true;
        }

        const char* variableName =
            "<unavailable>";

        if (
            definitions &&
            result.index < count
        )
        {
            variableName =
                definitions[
                    result.index
                ].name.c_str();
        }

        LOG_EF(
            "AEE::VALIDATION",
            "Definition invalid: "
            "context=%s index=%u name=%s "
            "reason=%s count=%u",
            context
                ? context
                : "AEE",
            static_cast<unsigned>(
                result.index
            ),
            variableName,
            result.reason
                ? result.reason
                : "unknown",
            static_cast<unsigned>(
                count
            )
        );

        return false;
    }

    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    AEEManagement(
        const AEEVarDef* definitions,
        size_t count)
    {
        _valid =
            false;

        if (
            !ValidateAndLog(
                definitions,
                count,
                "AEEManagement"
            )
        )
        {
            return;
        }

        vars.reserve(
            count
        );

        for (
            size_t index = 0;
            index < count;
            ++index
        )
        {
            const AEEVarDef& definition =
                definitions[index];

            AEEVariableBase* variable =
                nullptr;

            switch (definition.varType)
            {
                case AEEVarType::BOOL:
                {
                    variable =
                        new AEEVariable<bool>(
                            definition
                        );

                    break;
                }

                case AEEVarType::INT:
                {
                    variable =
                        new AEEVariable<int>(
                            definition
                        );

                    break;
                }

                case AEEVarType::FLOAT:
                {
                    variable =
                        new AEEVariable<float>(
                            definition
                        );

                    break;
                }

                case AEEVarType::STRING:
                {
                    variable =
                        new AEEVariable<String>(
                            definition
                        );

                    break;
                }

                default:
                {
                    LOG_EF(
                        "AEE::MANAGEMENT",
                        "Unsupported variable type: "
                        "index=%u name=%s",
                        static_cast<unsigned>(
                            index
                        ),
                        definition.name.c_str()
                    );

                    clear();

                    return;
                }
            }

            if (!variable)
            {
                LOG_EF(
                    "AEE::MANAGEMENT",
                    "Variable allocation failed: "
                    "index=%u name=%s",
                    static_cast<unsigned>(
                        index
                    ),
                    definition.name.c_str()
                );

                clear();

                return;
            }

            vars.push_back(
                variable
            );
        }

        _valid =
            vars.size() == count;

        if (!_valid)
        {
            LOG_EF(
                "AEE::MANAGEMENT",
                "Variable count mismatch: "
                "expected=%u created=%u",
                static_cast<unsigned>(
                    count
                ),
                static_cast<unsigned>(
                    vars.size()
                )
            );

            clear();

            return;
        }

        LOG_IF(
            "AEE::MANAGEMENT",
            "Management initialized: vars=%u",
            static_cast<unsigned>(
                vars.size()
            )
        );
    }

    // ========================================================
    // DESTRUCTOR
    // ========================================================

    ~AEEManagement()
    {
        clear();
    }

    // ========================================================
    // CLEAR
    // ========================================================

    void clear()
    {
        for (
            AEEVariableBase* variable :
            vars
        )
        {
            if (!variable)
            {
                continue;
            }

            variable->owner =
                nullptr;

            delete variable;
        }

        vars.clear();

        _valid =
            false;
    }

    // ========================================================
    // STATE
    // ========================================================

    bool isValid() const
    {
        return
            _valid;
    }

    size_t size() const
    {
        return
            vars.size();
    }

    // ========================================================
    // REGISTER ALL
    // ========================================================

    bool registerAll(
        AEERegistry& registry)
    {
        if (!_valid)
        {
            LOG_EF(
                "AEE::MANAGEMENT",
                "Registration rejected: "
                "management invalid"
            );

            return false;
        }

        for (
            AEEVariableBase* variable :
            vars
        )
        {
            if (!variable)
            {
                LOG_EF(
                    "AEE::MANAGEMENT",
                    "Registration failed: null variable"
                );

                return false;
            }

            variable->owner =
                &registry;

            registry.add(
                variable
            );
        }

        LOG_IF(
            "AEE::MANAGEMENT",
            "Variables registered: count=%u",
            static_cast<unsigned>(
                vars.size()
            )
        );

        return true;
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

    // ============================================================
    // SERIALIZE CHANGED JSON
    //
    // La capacità viene stimata in funzione delle variabili
    // effettivamente trasmissibili.
    //
    // La funzione restituisce String vuota in caso di:
    //
    // - nessuna variabile trasmissibile;
    // - overflow ArduinoJson;
    // - serializzazione fallita.
    //
    // Quando viene restituita String vuota, AEEProtocolEngine non
    // deve cancellare le variabili changed.
    // ============================================================

    String serializeChangedJSON(
        AEERegistry& registry,
        uint32_t sequence)
    {
        size_t eligibleCount =
            0;

        size_t estimatedCapacity =
            JSON_OBJECT_SIZE(
                1
            ) +
            64;

        // ========================================================
        // CAPACITY ESTIMATION
        // ========================================================

        registry.forEachChanged(
            [this,
            &eligibleCount,
            &estimatedCapacity](
                AEEVariableBase* variable)
            {
                if (!variable)
                {
                    return;
                }

                const AEEDirection direction =
                    variable->def.direction;

                const bool shouldSend =
                    (
                        isMaster &&
                        (
                            direction ==
                                AEEDirection::FrontendToModule ||
                            direction ==
                                AEEDirection::Bidirectional
                        )
                    ) ||
                    (
                        !isMaster &&
                        (
                            direction ==
                                AEEDirection::ModuleToFrontend ||
                            direction ==
                                AEEDirection::Bidirectional
                        )
                    );

                if (!shouldSend)
                {
                    return;
                }

                ++eligibleCount;

                estimatedCapacity +=
                    variable->def.name.length() +
                    48;
            }
        );

        if (eligibleCount == 0)
        {
            return
                String();
        }

        estimatedCapacity +=
            JSON_OBJECT_SIZE(
                eligibleCount + 1
            );

        static constexpr size_t
            MAX_AEE_DOCUMENT_CAPACITY =
                8192;

        if (
            estimatedCapacity >
            MAX_AEE_DOCUMENT_CAPACITY
        )
        {
            LOG_EF(
                "BRIDGE::AEE::TX",
                "Payload AEE stimato troppo grande: "
                "eligible=%u capacity=%u maximum=%u",
                static_cast<unsigned>(
                    eligibleCount
                ),
                static_cast<unsigned>(
                    estimatedCapacity
                ),
                static_cast<unsigned>(
                    MAX_AEE_DOCUMENT_CAPACITY
                )
            );

            return
                String();
        }

        // ========================================================
        // DOCUMENT BUILD
        // ========================================================

        DynamicJsonDocument document(
            estimatedCapacity
        );

        registry.forEachChanged(
            [this,
            &document](
                AEEVariableBase* variable)
            {
                if (!variable)
                {
                    return;
                }

                const AEEDirection direction =
                    variable->def.direction;

                const bool shouldSend =
                    (
                        isMaster &&
                        (
                            direction ==
                                AEEDirection::FrontendToModule ||
                            direction ==
                                AEEDirection::Bidirectional
                        )
                    ) ||
                    (
                        !isMaster &&
                        (
                            direction ==
                                AEEDirection::ModuleToFrontend ||
                            direction ==
                                AEEDirection::Bidirectional
                        )
                    );

                if (!shouldSend)
                {
                    return;
                }

                variable->toJson(
                    document
                );
            }
        );

        document["seq"] =
            sequence;

        if (document.overflowed())
        {
            LOG_EF(
                "BRIDGE::AEE::TX",
                "Overflow serializzazione AEE: "
                "eligible=%u capacity=%u",
                static_cast<unsigned>(
                    eligibleCount
                ),
                static_cast<unsigned>(
                    estimatedCapacity
                )
            );

            return
                String();
        }

        // ========================================================
        // SERIALIZATION
        // ========================================================

        String output;

        const size_t requiredLength =
            measureJson(
                document
            );

        if (
            !output.reserve(
                requiredLength + 1
            )
        )
        {
            LOG_EF(
                "BRIDGE::AEE::TX",
                "Allocazione payload AEE fallita: bytes=%u",
                static_cast<unsigned>(
                    requiredLength + 1
                )
            );

            return
                String();
        }

        const size_t serializedLength =
            serializeJson(
                document,
                output
            );

        if (
            serializedLength == 0 ||
            output.length() == 0
        )
        {
            LOG_EF(
                "BRIDGE::AEE::TX",
                "Serializzazione AEE fallita: "
                "eligible=%u",
                static_cast<unsigned>(
                    eligibleCount
                )
            );

            return
                String();
        }

        return
            output;
    }
    
    bool parseJSON(JsonDocument& doc, AEERegistry& reg)
    {
        for (auto kv : doc.as<JsonObject>())
        {
            const char* key = kv.key().c_str();

            if (strcmp(key, "seq") == 0)
                continue;

            AEEVariableBase* v = reg.find(key);

            if (!v)
                continue;

            AEEDirection dir = v->def.direction;

            const bool canApply =
                (isMaster &&
                    (dir == AEEDirection::ModuleToFrontend ||
                    dir == AEEDirection::Bidirectional))
                ||
                (!isMaster &&
                    (dir == AEEDirection::FrontendToModule ||
                    dir == AEEDirection::Bidirectional));

            if (canApply)
            {
                v->fromJson(kv.value());

                // SOLO SLAVE deve azzerare changed.
                // MASTER deve mantenere changed.
                if (!isMaster)
                    v->clearChanged();
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
    // BUILD MANAGEMENT FROM SCHEMA
    //
    // Costruisce un AEEManagement da uno schema completo.
    //
    // La capacità deve essere coerente con il limite massimo dello
    // schema ricostruito dal Bridge multiframe.
    // ============================================================

    AEEManagement* BuildManagementFromSchema(
        const String& json,
        AEERegistry& registry)
    {
        if (json.length() == 0)
        {
            LOG_EF(
                "AEE::SCHEMA",
                "Schema vuoto"
            );

            return nullptr;
        }

        static constexpr size_t
            MAX_SCHEMA_DOCUMENT_CAPACITY =
                8192;

        if (
            json.length() >=
            MAX_SCHEMA_DOCUMENT_CAPACITY
        )
        {
            LOG_EF(
                "AEE::SCHEMA",
                "Schema troppo grande: "
                "bytes=%u maximum=%u",
                static_cast<unsigned>(
                    json.length()
                ),
                static_cast<unsigned>(
                    MAX_SCHEMA_DOCUMENT_CAPACITY - 1
                )
            );

            return nullptr;
        }

        DynamicJsonDocument document(
            MAX_SCHEMA_DOCUMENT_CAPACITY
        );

        const DeserializationError error =
            deserializeJson(
                document,
                json
            );

        if (error)
        {
            LOG_EF(
                "AEE::SCHEMA",
                "Deserializzazione schema fallita: %s",
                error.c_str()
            );

            return nullptr;
        }

        if (document.overflowed())
        {
            LOG_EF(
                "AEE::SCHEMA",
                "Overflow documento schema"
            );

            return nullptr;
        }

        if (
            !document.containsKey(
                "schema"
            )
        )
        {
            LOG_EF(
                "AEE::SCHEMA",
                "Campo schema assente"
            );

            return nullptr;
        }

        JsonArray array =
            document[
                "schema"
            ].as<JsonArray>();

        if (array.isNull() ||
            array.size() == 0)
        {
            LOG_EF(
                "AEE::SCHEMA",
                "Array schema vuoto o non valido"
            );

            return nullptr;
        }

        size_t definitionCount =
            0;

        AEEVarDef* definitions =
            ImportSchema(
                array,
                definitionCount
            );

        if (!definitions ||
            definitionCount == 0)
        {
            delete[] definitions;

            LOG_EF(
                "AEE::SCHEMA",
                "Import definizioni fallito"
            );

            return nullptr;
        }

        AEEManagement* management =
            new AEEManagement(
                definitions,
                definitionCount
            );

        delete[] definitions;

        definitions =
            nullptr;

        if (!management)
        {
            LOG_EF(
                "AEE::SCHEMA",
                "Allocazione AEEManagement fallita"
            );

            return nullptr;
        }

        management->registerAll(
            registry
        );

        LOG_IF(
            "AEE::SCHEMA",
            "Management costruito: vars=%u",
            static_cast<unsigned>(
                registry.size()
            )
        );

        return
            management;
    }

};

// ============================================================
// HELPER CAST
// ============================================================

template<typename T>
AEEVariable<T>* as(
    AEEVariableBase* variable)
{
    if (!variable)
    {
        return nullptr;
    }

    if constexpr (
        std::is_same<T, bool>::value
    )
    {
        return
            variable->def.varType ==
                AEEVarType::BOOL
                    ? static_cast<
                          AEEVariable<bool>*
                      >(
                          variable
                      )
                    : nullptr;
    }

    if constexpr (
        std::is_same<T, int>::value
    )
    {
        return
            variable->def.varType ==
                AEEVarType::INT
                    ? static_cast<
                          AEEVariable<int>*
                      >(
                          variable
                      )
                    : nullptr;
    }

    if constexpr (
        std::is_same<T, float>::value
    )
    {
        return
            variable->def.varType ==
                AEEVarType::FLOAT
                    ? static_cast<
                          AEEVariable<float>*
                      >(
                          variable
                      )
                    : nullptr;
    }

    if constexpr (
        std::is_same<T, String>::value
    )
    {
        return
            variable->def.varType ==
                AEEVarType::STRING
                    ? static_cast<
                          AEEVariable<String>*
                      >(
                          variable
                      )
                    : nullptr;
    }

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
           def.name.c_str(),
           String(newVal).c_str());
           
    this->set(newVal, millis());
    return true;
}

template<>
inline bool AEEVariable<String>::fromJson(const JsonVariant& v)
{
    if (!v.is<const char*>())
        return false;

    const char* incoming = v.as<const char*>();

    if (!incoming)
        return false;

    if (this->get() == incoming)
        return false;

    LOG_IF(
        "SLAVE::AEE::APPLY",
        "Applying %s = %s",
        def.name.c_str(),
        incoming
    );

    set(
        String(incoming),
        millis()
    );

    return true;
}

template<>
inline void AEEVariable<String>::toJson(
    JsonDocument& document) const
{
    document[
        def.name.c_str()
    ] =
        value;
}

#endif
