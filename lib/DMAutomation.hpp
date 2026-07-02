#ifndef DMAUTOMATION_HPP
#define DMAUTOMATION_HPP

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


#include "DMBuffers.h"
#include "DMBaseClass.hpp"
#include "DMFncs.hpp"

// =====================================================
//  AUTOMATION ENGINE — Dichiarazioni
// =====================================================

class AutomationEngine {
private:
    bool startupMode = true;   // ⭐ parte attivo
    bool firstUpdate = true;
public:
    class BuiltinScenes {
    public:
        enum class Type {
            NoAction,
            COUNT   
        };

        // -------------------------
        // ENUM → STRING
        // -------------------------
        static const char* toString(Type t) {
            switch (t) {
                case Type::NoAction: return "NoAction";
                default:             return "";
            }
        }

        // -------------------------
        // STRING → ENUM
        // -------------------------
        static Type fromString(const char* s) {
            if (!s || !*s)
                return Type::NoAction;   // default richiesto

            // Ciclo dinamico: da 1 a COUNT-1
            for (uint8_t i = 0; i < static_cast<uint8_t>(Type::COUNT); i++) {
                Type t = static_cast<Type>(i);
                if (strcmp(s, toString(t)) == 0)
                    return t;
            }

            return Type::NoAction;       // default richiesto
        }

        // -------------------------
        // È una scena built‑in?
        // -------------------------
        static bool isBuiltin(const char* s) {
            if (!s || !*s) return false;

            // Ciclo dinamico: da 1 a COUNT-1
            for (uint8_t i = 0; i < static_cast<uint8_t>(Type::COUNT); i++) {
                Type t = static_cast<Type>(i);
                if (strcmp(s, toString(t)) == 0)
                    return true;
            }
            return false;
        }
    };


    friend class Diagnostic;

    struct SchedulerContext {
        AutomationEngine* engine;
        Buffer* buffer;
        AverageCalculator * medie;
        TimeManager* time;
    };

    using Fn    = void (*)(SchedulerContext*, unsigned long);
    using FnCtx = void (*)(SchedulerContext*, unsigned long, void*);

    // --- Condition ---
    class Condition {
    public:
        enum class CompareOp { GT, LT, GE, LE, EQ, NE, BETWEEN };

        int area;
        BufferFlagType bufferType = Field;   
        CompareOp op;
        long threshold1;
        long threshold2;

        Condition();
        Condition(int area, CompareOp c, long t1, long t2 = 0);

        bool evaluate(AutomationEngine* engine) const;
    };

    // --- Action ---
    class Action {
    public:
        int targetArea;
        long value;

        Action() : targetArea(0), value(0) {}
        Action(int area, long v) : targetArea(area), value(v) {}

        void execute(AutomationEngine* engine, unsigned long now) const;
    };

    // --- Rule ---
    class Rule {
            
    public:
        static const uint8_t MAX_CONDITIONS = 8;
        static const uint8_t MAX_ACTIONS = 8;

        Condition conditions[MAX_CONDITIONS];
        Action actionsThen[MAX_ACTIONS];
        Action actionsElse[MAX_ACTIONS];

        uint8_t conditionCount;
        uint8_t actionThenCount;
        uint8_t actionElseCount;

        bool useAndLogic;
        bool lastResult;
        bool hasLastResult;

        Rule();

        void addCondition(const Condition &c);
        void addThenAction(const Action &a);
        void addElseAction(const Action &a);

        bool evaluate(AutomationEngine* engine);
        void execute(AutomationEngine* engine, unsigned long now, bool edgeOnly);
    };

    // --- ActionSequence ---
    class ActionSequence {
    public:
        struct Step {
            Action action;
            unsigned long delay;
        };

        static const uint8_t MAX_STEPS = 10;

        Step steps[MAX_STEPS];
        uint8_t stepCount;

        bool running;
        int index;
        unsigned long lastTime;
        unsigned long startTime;

        ActionSequence() : stepCount(0), running(false), index(-1) {}

        void addStep(const Action& a, unsigned long d);
        void start(unsigned long now);
        void update(AutomationEngine* engine, unsigned long now);
        bool isRunning() const { return running; }
        unsigned long getStartTime() const { return startTime; }
        void stop() { running = false; index = -1; }
    };

    // --- Scene ---
    class Scene {
    public:
        String  name;
        static const uint8_t MAX_ACTIONS = 16;
        Action actions[MAX_ACTIONS];
        uint8_t actionCount;

        // Costruttore di default
        Scene()
            : name(""), actionCount(0)
        {}

        // Costruttore con nome
        Scene(const char* n)
            : name(n), actionCount(0)
        {}

        void addAction(const Action &a) {
            if (actionCount < MAX_ACTIONS)
                actions[actionCount++] = a;
        }

        void activate(AutomationEngine* engine, unsigned long now) const {
            for (uint8_t i = 0; i < actionCount; i++)
                actions[i].execute(engine, now);
        }
    };

    // =====================================================
    //  SCHEDULED RULES (mancavano)
    // =====================================================
    class ScheduledRule {
    public:
        enum Type {
            EVERY_INTERVAL,
            EVERY_DAY_AT,
            SEQUENCE_AT_TIME,
            SEQUENCE_EVERY_INTERVAL,
            SEQUENCE_WITH_TIMEOUT
        };


        Type type;

        ActionSequence sequence;

        bool hasCondition = false;
        Condition condition;

        unsigned long intervalMs = 0;
        unsigned long timeoutMs = 0;
        unsigned long lastRun = 0;

        bool hasFallback = false;
        Action fallbackAction;

        Scene* scene = nullptr;   // scena associata (se esiste)    
        Action* action = nullptr; // azione singola associata (se esiste)
    };

    // =====================================================
    //  DYNAMIC AUTOMATION (mancava la classe!)
    // =====================================================
    class DynamicAutomation {
    public:
        Fn callback = nullptr;
        FnCtx callbackCtx = nullptr;
        void* userCtx = nullptr;

        unsigned long interval;
        unsigned long lastRun;

        inline bool isReady(unsigned long now) const {
            return (now - lastRun) >= interval;
        }

        inline void run(AutomationEngine::SchedulerContext* ctx, unsigned long now) {
            lastRun = now;
            if (callback)
                callback(ctx, now);
            else if (callbackCtx)
                callbackCtx(ctx, now, userCtx);
        }

        DynamicAutomation(Fn fn, unsigned long ms)
            : callback(fn), interval(ms), lastRun(0) {}

        DynamicAutomation(FnCtx fn, void* ctx, unsigned long ms)
            : callbackCtx(fn), userCtx(ctx), interval(ms), lastRun(0) {}
    };


    // =====================================================
    //  API PUBBLICA
    // =====================================================

    void attachScheduler(AsyncScheduler* s,
                         Buffer* b,
                         AverageCalculator * m,
                         TimeManager* tm);

    void update(unsigned long now);

    Scene* addScene(const char *name);
    Scene* findScene(const char *name);
    void triggerScene(const char *name, unsigned long now);

    ScheduledRule* addDailyRule(int hour, int minute, Scene* scene) {
        if (scheduledCount >= MAX_SCHEDULED) return nullptr;

        ScheduledRule* r = new ScheduledRule();
        r->type = ScheduledRule::EVERY_DAY_AT;
        r->intervalMs = hour * 60 + minute; // memorizziamo HH*60+MM
        r->scene = scene;

        scheduled[scheduledCount++] = r;
        return r;
    }

    // === Diagnostic API ===
    uint8_t getSceneCount() const { return sceneCount; }
    Scene* getScenePtr(uint8_t index) {
        if (index >= sceneCount) return nullptr;
        return &scenes[index];
    }

    uint8_t getRuleCount() const { return ruleCount; }
    Rule& getRule(uint8_t index) {
        return rules[index];
    }

    uint8_t getScheduledCount() const { return scheduledCount; }
    ScheduledRule* getScheduledPtr(uint8_t index) {
        if (index >= scheduledCount) return nullptr;
        return scheduled[index];
    }

    uint8_t getDynamicCount() const { return dynamicCount; }

    DynamicAutomation* getDynamicPtr(uint8_t index) {
        if (index >= dynamicCount) return nullptr;
        return dynamic[index];
    }

    const std::vector<std::pair<int,long>>& getLastSceneValues() const {
        return lastSceneValues;
    }

    void addDynamicAutomation(unsigned long interval, Fn fn) {
        if (dynamicCount >= MAX_DYNAMIC) {
            LOG_EF("AutomationEngine",
                "Overflow DynamicAutomation: impossibile aggiungere (MAX_DYNAMIC=%d)",
                MAX_DYNAMIC);
            return;
        }

        dynamic[dynamicCount] = new DynamicAutomation(fn, interval);

        LOG_DF("AutomationEngine",
            "DynamicAutomation aggiunta [%d]: interval=%lu",
            dynamicCount, interval);

        dynamicCount++;
    }

    void addDynamicAutomation(unsigned long interval, FnCtx fn, void* ctx) {
        if (dynamicCount >= MAX_DYNAMIC) {
            LOG_EF("AutomationEngine",
                "Overflow DynamicAutomation: impossibile aggiungere (MAX_DYNAMIC=%d)",
                MAX_DYNAMIC);
            return;
        }

        dynamic[dynamicCount] = new DynamicAutomation(fn, ctx, interval);

        LOG_DF("AutomationEngine",
            "DynamicAutomation aggiunta [%d]: interval=%lu",
            dynamicCount, interval);

        dynamicCount++;
    }


    // Se vuoi tracciare quali scene sono state usate
    bool isSceneReferenced(const String& name) {
        // Per ora ritorna sempre true
        // oppure implementa un tracking reale
        return true;
    }

private:
    static const size_t MAX_SCENE_TRACK = 64;
    static const uint8_t MAX_SCHEDULED = 10;
    static const uint8_t MAX_DYNAMIC = 10;
     static const uint8_t MAX_SCENES = 20;
    static const uint8_t MAX_RULES = 20;

    SchedulerContext schedCtx;

    // =====================================================
    //  MEMBRI CONDIVISI (mancavano)
    // =====================================================
    Buffer* sharedBuffer = nullptr;
    AverageCalculator * sharedMedie = nullptr;
    AsyncScheduler* scheduler = nullptr;
    
    ScheduledRule* scheduled[MAX_SCHEDULED] = { nullptr };
    uint8_t scheduledCount = 0;
    
    DynamicAutomation* dynamic[MAX_DYNAMIC] = { nullptr };
    uint8_t dynamicCount = 0;

    // =====================================================
    //  SCENE STORAGE
    // =====================================================
   
    Scene scenes[MAX_SCENES];
    uint8_t sceneCount = 0;

    // =====================================================
    //  RULE STORAGE
    // =====================================================
    
    Rule rules[MAX_RULES];
    uint8_t ruleCount = 0;

    // =====================================================
    //  SCENE VALUE TRACKING (già presente)
    // =====================================================
    std::vector<std::pair<int, long>> lastSceneValues;

    bool getLastValue(int area, long &out) {
        for (auto &p : lastSceneValues) {
            if (p.first == area) {
                out = p.second;
                return true;
            }
        }
        return false;
    }

    void setLastValue(int area, long value) {
        for (auto &p : lastSceneValues) {
            if (p.first == area) {
                p.second = value;
                return;
            }
        }

        // Se non esiste, aggiungila
        if (lastSceneValues.size() >= MAX_SCENE_TRACK)
            lastSceneValues.erase(lastSceneValues.begin()); // rimuovi il più vecchio

        lastSceneValues.push_back({area, value});
    }

    // =====================================================
    //  markSceneReferenced (mancava)
    // =====================================================
    void markSceneReferenced(const char* name) {
        // Se non ti serve, può restare vuoto
    }

};

// =====================================================
//  AUTOMATION ENGINE IMPLEMENTATION
// =====================================================

// --- Action ---
void AutomationEngine::Action::execute(AutomationEngine* engine, unsigned long now) const {
    // Scrive nel buffer il valore dell'azione
    engine->sharedBuffer->WriteElement(targetArea, Field, value, now);
}

// --- Condition ---
AutomationEngine::Condition::Condition()
    : area(-1),
    bufferType(Field),
      op(CompareOp::EQ),
      threshold1(0),
      threshold2(0)
{
}

AutomationEngine::Condition::Condition(int area, CompareOp c, long t1, long t2)
    : area(area),
    bufferType(Field),
      op(c),
      threshold1(t1),
      threshold2(t2)
{
}

bool AutomationEngine::Condition::evaluate(AutomationEngine* engine) const {
    BufferSourceInfo info;

    // 🔥 Legge il valore corretto: area + type
    if (!engine->sharedBuffer->GetData(area, bufferType, info)) {
        LOG_WF("AutomationEngine",
            "Condizione su area %d (type=%d) ignorata: valore non inizializzato",
            area, bufferType);
        return false; // comportamento coerente, ma ora diagnostico
    }

    long value = info.value;

    switch (op) {
        case CompareOp::GT:      return value >  threshold1;
        case CompareOp::LT:      return value <  threshold1;
        case CompareOp::GE:      return value >= threshold1;
        case CompareOp::LE:      return value <= threshold1;
        case CompareOp::EQ:      return value == threshold1;
        case CompareOp::NE:      return value != threshold1;
        // BETWEEN inclusivo [threshold1, threshold2]
        case CompareOp::BETWEEN: return value >= threshold1 && value <= threshold2;
    }
    return false;
}

// --- Rule ---
AutomationEngine::Rule::Rule()
    : conditionCount(0),
      actionThenCount(0),
      actionElseCount(0),
      useAndLogic(true),
      lastResult(false),
      hasLastResult(false)
{
    
}

void AutomationEngine::Rule::addCondition(const Condition &c) {
    if (conditionCount < MAX_CONDITIONS)
        conditions[conditionCount++] = c;
}

void AutomationEngine::Rule::addThenAction(const Action &a) {
    if (actionThenCount < MAX_ACTIONS)
        actionsThen[actionThenCount++] = a;
}

void AutomationEngine::Rule::addElseAction(const Action &a) {
    if (actionElseCount < MAX_ACTIONS)
        actionsElse[actionElseCount++] = a;
}

bool AutomationEngine::Rule::evaluate(AutomationEngine* engine) {
    if (conditionCount == 0)
        return false;

    if (useAndLogic) {
        // AND logic
        for (uint8_t i = 0; i < conditionCount; i++) {
            if (!conditions[i].evaluate(engine))
                return false;
        }
        return true;
    } else {
        // OR logic
        for (uint8_t i = 0; i < conditionCount; i++) {
            if (conditions[i].evaluate(engine))
                return true;
        }
        return false;
    }
}

void AutomationEngine::Rule::execute(AutomationEngine* engine,
                                     unsigned long now,
                                     bool edgeOnly)
{
    if (conditionCount == 0) {
        LOG_WF("AutomationEngine", "Regola %p senza condizioni ignorata", this);
        return;
    }

    bool result = evaluate(engine);

    bool same = hasLastResult && (result == lastResult);
    lastResult = result;
    hasLastResult = true;

    // 🔥 LOG PRIMA ESECUZIONE
    if (!edgeOnly && !same) {
        LOG_IFH("AutomationEngine::Rule",
               "Prima esecuzione → risultato=%s",
               result ? "TRUE" : "FALSE");
    }

    if (edgeOnly && same)
        return;

    // 🔥 LOG SCENA SCELTA (anche se non cambia nulla)
    if (!same) {
        if (result) {
            LOG_IF("AutomationEngine::Rule",
                   "Eseguo scena THEN (startup o variazione)");
        } else {
            LOG_IF("AutomationEngine::Rule",
                   "Eseguo scena ELSE (startup o variazione)");
        }
    }

    if (result) {
        for (uint8_t i = 0; i < actionThenCount; i++)
            actionsThen[i].execute(engine, now);
    } else {
        for (uint8_t i = 0; i < actionElseCount; i++)
            actionsElse[i].execute(engine, now);
    }
}



void AutomationEngine::attachScheduler(AsyncScheduler* s,
                                       Buffer* b,
                                       AverageCalculator * m,
                                       TimeManager* tm)
{
    scheduler = s;

    // 🔥 SALVIAMO I PUNTATORI CONDIVISI
    sharedBuffer = b;
    sharedMedie  = m;

    schedCtx.engine = this;
    schedCtx.buffer = b;
    schedCtx.medie  = m;
    schedCtx.time    = tm;

    if (scheduler)
        scheduler->setContext(&schedCtx);
}



void AutomationEngine::update(unsigned long now) {
    bool edgeOnly = !firstUpdate;

    // ⭐ Attivo solo al primo update
    startupMode = firstUpdate;

    // --- regole standard (nel tuo caso ruleCount=0) ---
    for (uint8_t i = 0; i < ruleCount; ++i) {
        rules[i].execute(this, now, edgeOnly);
    }

    // --- scheduled rules ---
    for (uint8_t i = 0; i < scheduledCount; ++i) {
        ScheduledRule* r = scheduled[i];
        if (!r) continue;

        auto& seq = r->sequence;

        switch (r->type) {

            // ============================================================
            // 1. EVERY_INTERVAL — esecuzione periodica
            // ============================================================
            case ScheduledRule::EVERY_INTERVAL: {
                unsigned long elapsed = now - r->lastRun;

                if (elapsed >= r->intervalMs) {
                    if (!r->hasCondition || r->condition.evaluate(this)) {
                        if (r->scene)      r->scene->activate(this, now);
                        else if (r->action) r->action->execute(this, now);
                    }
                    r->lastRun = now;
                }
                break;
            }

            // ============================================================
            // 2. EVERY_DAY_AT — esecuzione a un orario preciso
            // ============================================================
            case ScheduledRule::EVERY_DAY_AT: {
                struct tm t;
                if (!schedCtx.time->getDateTime(t)) {
                    // 🔥 Diagnostica chiara: l’RTC non è valido
                    LOG_WF("AutomationEngine", "RTC non valido — regole temporali sospese");

                    // 🔥 Non aggiornare lastRun, così la regola recupera appena l’RTC torna valido
                    continue;
                }

                // Orario corrente in minuti
                int nowMin = t.tm_hour * 60 + t.tm_min;

                // Orario target in minuti
                int targetMin = r->intervalMs; // HH*60 + MM

                // Esegui solo una volta al minuto corretto
                if (nowMin == targetMin) {

                    // Evita doppie esecuzioni
                    if (now - r->lastRun > 60000) {

                        // 🔥 Esegui solo se la condizione è vera
                        if (!r->hasCondition || r->condition.evaluate(this)) {

                            if (r->scene)
                                r->scene->activate(this, now);
                            else if (r->action)
                                r->action->execute(this, now);

                            // 🔥 Aggiorna lastRun SOLO se la regola è stata eseguita
                            r->lastRun = now;
                        }
                    }
                }
                break;
            }


            // ============================================================
            // 3. SEQUENCE_AT_TIME — avvia una sequenza a un orario preciso
            // ============================================================
            case ScheduledRule::SEQUENCE_AT_TIME: {
                struct tm t;
                if (!schedCtx.time->getDateTime(t)) {
                    // 🔥 Diagnostica chiara: l’RTC non è valido
                    LOG_WF("AutomationEngine", "RTC non valido — regole temporali sospese");

                    // 🔥 Non aggiornare lastRun, così la regola recupera appena l’RTC torna valido
                    continue;
                }

                int nowMin = t.tm_hour * 60 + t.tm_min;
                int targetMin = r->intervalMs;

                if (nowMin == targetMin) {
                    if (!seq.isRunning() && now - r->lastRun > 60000) {
                        if (!r->hasCondition || r->condition.evaluate(this))
                            seq.start(now);

                        r->lastRun = now;
                    }
                }

                if (seq.isRunning())
                    seq.update(this, now);

                break;
            }

            // ============================================================
            // 4. SEQUENCE_EVERY_INTERVAL — sequenza periodica
            // ============================================================
            case ScheduledRule::SEQUENCE_EVERY_INTERVAL: {
                unsigned long elapsed = now - r->lastRun;

                // Avvia solo se NON sta già girando
                if (!seq.isRunning()) {
                    if (elapsed >= r->intervalMs) {
                        if (!r->hasCondition || r->condition.evaluate(this)) {
                            seq.start(now);
                            r->lastRun = now;   // <-- aggiorno SOLO quando parte davvero
                        }
                    }
                }

                // Aggiorna la sequenza se attiva
                if (seq.isRunning())
                    seq.update(this, now);

                break;
            }

            // ============================================================
            // 5. SEQUENCE_WITH_TIMEOUT — sequenza con timeout
            // ============================================================
            case ScheduledRule::SEQUENCE_WITH_TIMEOUT: {
                if (!seq.isRunning())
                    break;

                seq.update(this, now);

                if (now - seq.getStartTime() > r->timeoutMs) {
                    if (r->hasFallback)
                        r->fallbackAction.execute(this, now);
                    seq.stop();
                }

                break;
            }
        }
    }

    // --- dynamic automations ---
    for (uint8_t i = 0; i < dynamicCount; i++) {
        DynamicAutomation* d = dynamic[i];
        if (d->isReady(now) || startupMode) { // ⭐ esegui SEMPRE allo startup
            unsigned long before = now;
            d->run(&schedCtx, now);
            unsigned long exec = schedCtx.time->nowMs() - before;

            if (exec > 10) { // soglia configurabile
                LOG_WF("AutomationEngine", 
                    "DynamicAutomation lenta: %lums (interval=%lu)", 
                    exec, d->interval);
            }
        }
    }
    firstUpdate = false;   // ⭐ disattiva startupMode dal prossimo ciclo
}



// =====================================================
//  ACTION SEQUENCE
// =====================================================

void AutomationEngine::ActionSequence::addStep(const Action& a, unsigned long d) {
    if (stepCount >= MAX_STEPS) {
        LOG_EF("AutomationEngine::ActionSequence",
               "Overflow step: impossibile aggiungere step (MAX_STEPS=%d)",
               MAX_STEPS);
        return;
    }

    steps[stepCount].action = a;
    steps[stepCount].delay  = d;
    stepCount++;
}


void AutomationEngine::ActionSequence::start(unsigned long now) {
    if (stepCount == 0) {
        running = false;
        index   = -1;
        return;
    }

    running   = true;
    index     = 0;
    lastTime  = now;
    startTime = now;
}

void AutomationEngine::ActionSequence::update(AutomationEngine* engine, unsigned long now) {
    if (!running || index < 0 || index >= stepCount)
        return;

    if ((long)(now - lastTime) >= (long)steps[index].delay) {
        steps[index].action.execute(engine, now);
        // Evita drift: avanza lastTime di delay, non lo riassegna a now
        lastTime += steps[index].delay;
        index++;

        if (index >= stepCount) {
            running = false;
            index = -1;
        }
    }
}



AutomationEngine::Scene* AutomationEngine::addScene(const char *name) {
    if (sceneCount >= MAX_SCENES) {
        LOG_EF("AutomationEngine", 
               "Impossibile aggiungere scena '%s': limite MAX_SCENES raggiunto (%d)", 
               name, MAX_SCENES);
        return nullptr;
    }

    scenes[sceneCount] = Scene(name);
    LOG_DF("AutomationEngine", 
           "Scene caricata [%d]: %s", 
           sceneCount, name);

    return &scenes[sceneCount++];
}


AutomationEngine::Scene* AutomationEngine::findScene(const char *name) 
{
    if (!name || name[0] == '\0') {
        LOG_EF("AutomationEngine::findScene",
               "Richiesta scena NON valida (stringa vuota)");
        return nullptr;
    }

    for (uint8_t i = 0; i < sceneCount; i++) {

        const char* stored = scenes[i].name.c_str();

        // Nome scena non inizializzato → log dedicato
        if (!stored || stored[0] == '\0') {
            LOG_WF("AutomationEngine::findScene",
                   "Scena[%u] ha nome VUOTO o NON inizializzato", i);
            continue;
        }

        // Match immediato
        if (scenes[i].name.equals(name)) {
            return &scenes[i];
        }
    }

    // ❌ Scena non trovata
    LOG_EF("AutomationEngine::findScene",
           "Scena NON trovata: '%s'", name);

    return nullptr;
}



void AutomationEngine::triggerScene(const char *name, unsigned long now) {
    // ⭐ GESTIONE SPECIALE: NoAction → nessuna scena, nessun log, nessun errore
    if (!name || name[0] == '\0' || strcmp(name, BuiltinScenes::toString( BuiltinScenes::Type::NoAction ) ) == 0) {
        return;
    }
    
    Scene* s = findScene(name);
    if (!s) {
        LOG_EF("AutomationEngine::triggerScene",
               "Impossibile eseguire scena '%s' (non trovata)", name);
        return;
    }

    bool anyChange = false;

    for (uint8_t i = 0; i < s->actionCount; i++) {
        const Action& a = s->actions[i];
        int area = a.targetArea;

        long oldValue;
        bool hasOld = getLastValue(area, oldValue);

        long before = sharedBuffer->getValueFast(area);

        // ⭐ FORZA l’esecuzione allo startup
        if (!startupMode && before == a.value)
            continue;

        a.execute(this, now);

        long after = sharedBuffer->getValueFast(area);

        if (!hasOld || oldValue != after || startupMode) {
            setLastValue(area, after);
            anyChange = true;
        }
    }

    // ⭐ allo startup logga sempre la scena
    if (!anyChange && !startupMode)
        return;

    LOG_IF("AutomationEngine::triggerScene",
           startupMode ? "FORCED scene: %s" : "Executing scene: %s",
           name);

    for (uint8_t i = 0; i < s->actionCount; i++) {
        const Action& a = s->actions[i];
        LOG_DF("AutomationEngine::triggerScene",
               " → Action targetArea=%d value=%ld",
               a.targetArea, a.value);
    }
}


#endif
