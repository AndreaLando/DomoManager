#ifndef DMAUTOMATION_HPP
#define DMAUTOMATION_HPP

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑09‑05
   Note:
                    • Nessuna

   ============================================================================ */


#include "DMBuffers.hpp"
#include "DMBaseClassUtils.hpp"

class AutomationEngine
{
private:

    bool startupMode = true;
    bool firstUpdate = true;

public:

    // ============================================================
    // BUILTIN SCENES
    // ============================================================

    class BuiltinScenes
    {
    public:

        enum class Type
        {
            NoAction,
            COUNT
        };

        static const char* toString(Type t)
        {
            switch (t)
            {
                case Type::NoAction:
                    return "NoAction";

                default:
                    return "";
            }
        }

        static Type fromString(const char* s)
        {
            if (!s || !*s)
                return Type::NoAction;

            for (uint8_t i = 0;
                 i < static_cast<uint8_t>(Type::COUNT);
                 i++)
            {
                Type t = static_cast<Type>(i);

                if (strcmp(s, toString(t)) == 0)
                    return t;
            }

            return Type::NoAction;
        }

        static bool isBuiltin(const char* s)
        {
            if (!s || !*s)
                return false;

            for (uint8_t i = 0;
                 i < static_cast<uint8_t>(Type::COUNT);
                 i++)
            {
                Type t = static_cast<Type>(i);

                if (strcmp(s, toString(t)) == 0)
                    return true;
            }

            return false;
        }
    };


    friend class Diagnostic;

    // ============================================================
    // ACTION
    // ============================================================

    class Action
    {
    public:

        int targetArea;
        long value;

        Action()
            : targetArea(0),
              value(0)
        {
        }

        Action(int area, long v)
            : targetArea(area),
              value(v)
        {
        }
    };


    // ============================================================
    // ACTION LIST
    //
    // Le actions vengono raccolte durante l'elaborazione
    // e restituite al DomoManager.
    // ============================================================

    using ActionList = std::vector<Action>;

    // ============================================================
    // SCHEDULER CONTEXT
    // ============================================================

    struct SchedulerContext
    {
        AutomationEngine* engine;
        Buffer* buffer;
        AverageCalculator* medie;
        TimeManager* time;

        ActionList* actions = nullptr;
    };


    using Fn =
        void (*)(SchedulerContext*, unsigned long);

    using FnCtx =
        void (*)(SchedulerContext*, unsigned long, void*);

    // ============================================================
    // CONDITION
    // ============================================================

    class Condition
    {
    public:

        enum class CompareOp
        {
            GT,
            LT,
            GE,
            LE,
            EQ,
            NE,
            BETWEEN
        };

        int area;
        CompareOp op;
        long threshold1;
        long threshold2;

        Condition();

        Condition(
            int area,
            CompareOp c,
            long t1,
            long t2 = 0
        );

        bool evaluate(
            AutomationEngine* engine
        ) const;
    };


    // ============================================================
    // RULE
    // ============================================================

    class Rule
    {
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

        void addCondition(
            const Condition& c
        );

        void addThenAction(
            const Action& a
        );

        void addElseAction(
            const Action& a
        );

        bool evaluate(
            AutomationEngine* engine
        );

        void execute(
            AutomationEngine* engine,
            unsigned long now,
            bool edgeOnly,
            ActionList& actions
        );
    };


    // ============================================================
    // ACTION SEQUENCE
    // ============================================================

    class ActionSequence
    {
    public:

        struct Step
        {
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

        ActionSequence()
            : stepCount(0),
              running(false),
              index(-1),
              lastTime(0),
              startTime(0)
        {
        }

        void addStep(
            const Action& a,
            unsigned long d
        );

        void start(
            unsigned long now
        );

        void update(
            AutomationEngine* engine,
            unsigned long now,
            ActionList& actions
        );

        bool isRunning() const
        {
            return running;
        }

        unsigned long getStartTime() const
        {
            return startTime;
        }

        void stop()
        {
            running = false;
            index = -1;
        }
    };


    // ============================================================
    // SCENE
    // ============================================================

    class Scene
    {
    public:

        String name;

        static const uint8_t MAX_ACTIONS = 16;

        Action actions[MAX_ACTIONS];

        uint8_t actionCount;

        Scene()
            : name(""),
              actionCount(0)
        {
        }

        Scene(const char* n)
            : name(n),
              actionCount(0)
        {
        }

        void addAction(
            const Action& a
        )
        {
            if (actionCount < MAX_ACTIONS)
                actions[actionCount++] = a;
        }

        void activate(
            AutomationEngine* engine,
            unsigned long now,
            ActionList& output
        ) const
        {
            (void)engine;
            (void)now;

            for (uint8_t i = 0;
                 i < actionCount;
                 i++)
            {
                output.push_back(actions[i]);
            }
        }
    };


    // ============================================================
    // SCHEDULED RULE
    // ============================================================

    class ScheduledRule
    {
    public:

        enum Type
        {
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

        Scene* scene = nullptr;
        Action* action = nullptr;
    };


    // ============================================================
    // DYNAMIC AUTOMATION
    // ============================================================

    class DynamicAutomation
    {
    public:

        Fn callback = nullptr;

        FnCtx callbackCtx = nullptr;

        void* userCtx = nullptr;

        unsigned long interval;
        unsigned long lastRun;

        inline bool isReady(
            unsigned long now
        ) const
        {
            return (now - lastRun) >= interval;
        }

        inline void run(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now
        )
        {
            lastRun = now;

            if (callback)
            {
                callback(ctx, now);
            }
            else if (callbackCtx)
            {
                callbackCtx(
                    ctx,
                    now,
                    userCtx
                );
            }
        }

        DynamicAutomation(
            Fn fn,
            unsigned long ms
        )
            : callback(fn),
              callbackCtx(nullptr),
              userCtx(nullptr),
              interval(ms),
              lastRun(0)
        {
        }

        DynamicAutomation(
            FnCtx fn,
            void* ctx,
            unsigned long ms
        )
            : callback(nullptr),
              callbackCtx(fn),
              userCtx(ctx),
              interval(ms),
              lastRun(0)
        {
        }
    };


    // ============================================================
    // PUBLIC API
    // ============================================================

    void attachScheduler(
        AsyncScheduler* s,
        Buffer* b,
        AverageCalculator* m,
        TimeManager* tm
    );


    // ============================================================
    // EVENT
    //
    // Restituisce tutte le Action generate dall'evento.
    // ============================================================

    ActionList onEvent(
        unsigned long now,
        int area = -1
    );


    // ============================================================
    // UPDATE
    //
    // Restituisce tutte le Action generate dal ciclo scheduler.
    // ============================================================

    ActionList update(
        unsigned long now
    );


    Scene* addScene(
        const char* name
    );


    Scene* findScene(
        const char* name
    );


    // triggerScene viene mantenuto come API pubblica.
    // Anche qui le Action vengono raccolte.
    ActionList triggerScene(
        const char* name,
        unsigned long now
    );


    // ============================================================
    // DAILY RULE
    // ============================================================

    ScheduledRule* addDailyRule(
        int hour,
        int minute,
        Scene* scene
    )
    {
        if (scheduledCount >= MAX_SCHEDULED)
            return nullptr;

        ScheduledRule* r =
            new ScheduledRule();

        r->type =
            ScheduledRule::EVERY_DAY_AT;

        // HH * 60 + MM
        r->intervalMs =
            hour * 60 + minute;

        r->scene = scene;

        scheduled[scheduledCount++] = r;

        return r;
    }


    // ============================================================
    // DIAGNOSTIC API
    // ============================================================

    uint8_t getSceneCount() const
    {
        return sceneCount;
    }


    Scene* getScenePtr(
        uint8_t index
    )
    {
        if (index >= sceneCount)
            return nullptr;

        return &scenes[index];
    }


    uint8_t getRuleCount() const
    {
        return ruleCount;
    }


    Rule& getRule(
        uint8_t index
    )
    {
        return rules[index];
    }


    uint8_t getScheduledCount() const
    {
        return scheduledCount;
    }


    ScheduledRule* getScheduledPtr(
        uint8_t index
    )
    {
        if (index >= scheduledCount)
            return nullptr;

        return scheduled[index];
    }


    uint8_t getDynamicCount() const
    {
        return dynamicCount;
    }


    DynamicAutomation* getDynamicPtr(
        uint8_t index
    )
    {
        if (index >= dynamicCount)
            return nullptr;

        return dynamic[index];
    }


    const std::vector<std::pair<int, long>>&
    getLastSceneValues() const
    {
        return lastSceneValues;
    }


    // ============================================================
    // DYNAMIC AUTOMATION
    // ============================================================

    void addDynamicAutomation(
        unsigned long interval,
        FnCtx fn,
        void* ctx
    )
    {
        if (dynamicCount >= MAX_DYNAMIC)
        {
            LOG_EF(
                "AutomationEngine",
                "Overflow DynamicAutomation: "
                "impossibile aggiungere "
                "(MAX_DYNAMIC=%d)",
                MAX_DYNAMIC
            );

            return;
        }

        dynamic[dynamicCount] =
            new DynamicAutomation(
                fn,
                ctx,
                interval
            );

        LOG_DF(
            "AutomationEngine",
            "DynamicAutomation aggiunta [%d]: interval=%lu",
            dynamicCount,
            interval
        );

        dynamicCount++;
    }


    // ============================================================
    // SCENE REFERENCE
    // ============================================================

    bool isSceneReferenced(
        const String& name
    )
    {
        (void)name;

        // Per ora ritorna sempre true.
        // Eventuale tracking reale in futuro.
        return true;
    }


private:

    // ============================================================
    // LIMITI
    // ============================================================

    static const size_t MAX_SCENE_TRACK = 64;

    static const uint8_t MAX_SCHEDULED = 10;

    static const uint8_t MAX_DYNAMIC = 10;

    static const uint8_t MAX_SCENES = 20;

    static const uint8_t MAX_RULES = 20;


    // ============================================================
    // SCHEDULER
    // ============================================================

    SchedulerContext schedCtx;

    Buffer* sharedBuffer = nullptr;

    AverageCalculator* sharedMedie = nullptr;

    AsyncScheduler* scheduler = nullptr;


    // ============================================================
    // SCHEDULED STORAGE
    // ============================================================

    ScheduledRule* scheduled[MAX_SCHEDULED] =
    {
        nullptr
    };

    uint8_t scheduledCount = 0;


    // ============================================================
    // DYNAMIC STORAGE
    // ============================================================

    DynamicAutomation* dynamic[MAX_DYNAMIC] =
    {
        nullptr
    };

    uint8_t dynamicCount = 0;


    // ============================================================
    // SCENE STORAGE
    // ============================================================

    Scene scenes[MAX_SCENES];

    uint8_t sceneCount = 0;


    // ============================================================
    // RULE STORAGE
    // ============================================================

    Rule rules[MAX_RULES];

    uint8_t ruleCount = 0;


    // ============================================================
    // SCENE VALUE TRACKING
    // ============================================================

    std::vector<std::pair<int, long>>
        lastSceneValues;


    bool getLastValue(
        int area,
        long& out
    )
    {
        for (auto& p : lastSceneValues)
        {
            if (p.first == area)
            {
                out = p.second;
                return true;
            }
        }

        return false;
    }


    void setLastValue(
        int area,
        long value
    )
    {
        for (auto& p : lastSceneValues)
        {
            if (p.first == area)
            {
                p.second = value;
                return;
            }
        }

        if (lastSceneValues.size() >= MAX_SCENE_TRACK)
        {
            lastSceneValues.erase(
                lastSceneValues.begin()
            );
        }

        lastSceneValues.push_back(
            {
                area,
                value
            }
        );
    }
};


// ============================================================
// AUTOMATION ENGINE IMPLEMENTATION
// ============================================================


// ============================================================
// CONDITION
// ============================================================

AutomationEngine::Condition::Condition()
    : area(-1),
      op(CompareOp::EQ),
      threshold1(0),
      threshold2(0)
{
}


AutomationEngine::Condition::Condition(
    int area,
    CompareOp c,
    long t1,
    long t2
)
    : area(area),
      op(c),
      threshold1(t1),
      threshold2(t2)
{
}


bool AutomationEngine::Condition::evaluate(
    AutomationEngine* engine
) const
{
    if (!engine)
        return false;

    if (!engine->sharedBuffer)
    {
        LOG_WF(
            "AutomationEngine",
            "Condition: Buffer non configurato"
        );

        return false;
    }

    BufferSourceInfo info;

    if (!engine->sharedBuffer->GetData(
            area,
            info))
    {
        LOG_WF(
            "AutomationEngine",
            "Condizione su area %d ignorata: "
            "valore non inizializzato",
            area
        );

        return false;
    }

    long value = info.value;

    switch (op)
    {
        case CompareOp::GT:
            return value > threshold1;

        case CompareOp::LT:
            return value < threshold1;

        case CompareOp::GE:
            return value >= threshold1;

        case CompareOp::LE:
            return value <= threshold1;

        case CompareOp::EQ:
            return value == threshold1;

        case CompareOp::NE:
            return value != threshold1;

        case CompareOp::BETWEEN:
            return value >= threshold1 &&
                   value <= threshold2;
    }

    return false;
}


// ============================================================
// RULE
// ============================================================

AutomationEngine::Rule::Rule()
    : conditionCount(0),
      actionThenCount(0),
      actionElseCount(0),
      useAndLogic(true),
      lastResult(false),
      hasLastResult(false)
{
}


void AutomationEngine::Rule::addCondition(
    const Condition& c
)
{
    if (conditionCount < MAX_CONDITIONS)
        conditions[conditionCount++] = c;
}


void AutomationEngine::Rule::addThenAction(
    const Action& a
)
{
    if (actionThenCount < MAX_ACTIONS)
        actionsThen[actionThenCount++] = a;
}


void AutomationEngine::Rule::addElseAction(
    const Action& a
)
{
    if (actionElseCount < MAX_ACTIONS)
        actionsElse[actionElseCount++] = a;
}


bool AutomationEngine::Rule::evaluate(
    AutomationEngine* engine
)
{
    if (conditionCount == 0)
        return false;

    if (useAndLogic)
    {
        for (uint8_t i = 0;
             i < conditionCount;
             i++)
        {
            if (!conditions[i].evaluate(engine))
                return false;
        }

        return true;
    }

    for (uint8_t i = 0;
         i < conditionCount;
         i++)
    {
        if (conditions[i].evaluate(engine))
            return true;
    }

    return false;
}


void AutomationEngine::Rule::execute(
    AutomationEngine* engine,
    unsigned long now,
    bool edgeOnly,
    ActionList& actions
)
{
    (void)now;

    if (conditionCount == 0)
    {
        LOG_WF(
            "AutomationEngine::Rule",
            "Regola %p senza condizioni ignorata",
            this
        );

        return;
    }

    bool result =
        evaluate(engine);

    bool same =
        hasLastResult &&
        (result == lastResult);

    lastResult = result;
    hasLastResult = true;


    // ============================================================
    // LOG PRIMA ESECUZIONE / VARIAZIONE
    // ============================================================

    if (!edgeOnly && !same)
    {
        LOG_IFH(
            "AutomationEngine::Rule",
            "Prima esecuzione → risultato=%s",
            result ? "TRUE" : "FALSE"
        );
    }


    // Edge only:
    // nessuna variazione = nessuna action.
    if (edgeOnly && same)
        return;


    if (!same)
    {
        if (result)
        {
            LOG_IF(
                "AutomationEngine::Rule",
                "Eseguo scena THEN "
                "(startup o variazione)"
            );
        }
        else
        {
            LOG_IF(
                "AutomationEngine::Rule",
                "Eseguo scena ELSE "
                "(startup o variazione)"
            );
        }
    }


    // ============================================================
    // RACCOLTA ACTION
    // ============================================================

    if (result)
    {
        for (uint8_t i = 0;
             i < actionThenCount;
             i++)
        {
            actions.push_back(
                actionsThen[i]
            );
        }
    }
    else
    {
        for (uint8_t i = 0;
             i < actionElseCount;
             i++)
        {
            actions.push_back(
                actionsElse[i]
            );
        }
    }
}


// ============================================================
// ATTACH SCHEDULER
// ============================================================

void AutomationEngine::attachScheduler(
    AsyncScheduler* s,
    Buffer* b,
    AverageCalculator* m,
    TimeManager* tm
)
{
    scheduler = s;

    sharedBuffer = b;
    sharedMedie = m;

    schedCtx.engine = this;
    schedCtx.buffer = b;
    schedCtx.medie = m;
    schedCtx.time = tm;

    if (scheduler)
        scheduler->setContext(
            &schedCtx
        );
}


// ============================================================
// EVENT
// ============================================================

AutomationEngine::ActionList
AutomationEngine::onEvent(
    unsigned long now,
    int area
)
{
    ActionList actions;


    for (uint8_t i = 0;
         i < ruleCount;
         i++)
    {
        Rule& rule = rules[i];


        // ========================================================
        // EVENTO SPECIFICO
        // ========================================================

        if (area >= 0)
        {
            bool interested = false;

            for (uint8_t c = 0;
                 c < rule.conditionCount;
                 c++)
            {
                if (rule.conditions[c].area == area)
                {
                    interested = true;
                    break;
                }
            }

            if (!interested)
                continue;
        }


        // ========================================================
        // STARTUP / EDGE
        // ========================================================

        rule.execute(
            this,
            now,
            !startupMode,
            actions
        );
    }


    return actions;
}


// ============================================================
// UPDATE
// ============================================================

AutomationEngine::ActionList
AutomationEngine::update(
    unsigned long now
)
{
    ActionList actions;

    // La ActionList appartiene a questa singola update().
    // Le DynamicAutomation la useranno per accumulare
    // le Action prodotte durante il ciclo.
    schedCtx.actions = &actions;

    // ============================================================
    // STARTUP STATE
    // ============================================================

    startupMode = firstUpdate;


    // ============================================================
    // STARTUP
    // ============================================================

    if (firstUpdate)
    {
        ActionList startupActions =
            onEvent(now);

        actions.insert(
            actions.end(),
            startupActions.begin(),
            startupActions.end()
        );
    }


    // ============================================================
    // SCHEDULED RULES
    // ============================================================

    for (uint8_t i = 0;
         i < scheduledCount;
         i++)
    {
        ScheduledRule* r =
            scheduled[i];

        if (!r)
            continue;


        auto& seq =
            r->sequence;


        switch (r->type)
        {

            // ====================================================
            // EVERY_INTERVAL
            // ====================================================

            case ScheduledRule::EVERY_INTERVAL:
            {
                unsigned long elapsed =
                    now - r->lastRun;


                if (elapsed >= r->intervalMs)
                {
                    if (!r->hasCondition ||
                        r->condition.evaluate(this))
                    {
                        if (r->scene)
                        {
                            r->scene->activate(
                                this,
                                now,
                                actions
                            );
                        }
                        else if (r->action)
                        {
                            actions.push_back(
                                *r->action
                            );
                        }
                    }

                    r->lastRun = now;
                }

                break;
            }


            // ====================================================
            // EVERY_DAY_AT
            // ====================================================

            case ScheduledRule::EVERY_DAY_AT:
            {
                if (!schedCtx.time)
                {
                    LOG_WF(
                        "AutomationEngine",
                        "TimeManager non configurato"
                    );

                    continue;
                }


                struct tm t;

                if (!schedCtx.time->getDateTime(t))
                {
                    LOG_WF(
                        "AutomationEngine",
                        "RTC non valido — "
                        "regole temporali sospese"
                    );

                    continue;
                }


                int nowMin =
                    t.tm_hour * 60 +
                    t.tm_min;

                int targetMin =
                    static_cast<int>(
                        r->intervalMs
                    );


                if (nowMin == targetMin)
                {
                    if (now - r->lastRun > 60000)
                    {
                        if (!r->hasCondition ||
                            r->condition.evaluate(this))
                        {
                            if (r->scene)
                            {
                                r->scene->activate(
                                    this,
                                    now,
                                    actions
                                );
                            }
                            else if (r->action)
                            {
                                actions.push_back(
                                    *r->action
                                );
                            }

                            // Aggiornato solo quando eseguita
                            r->lastRun = now;
                        }
                    }
                }

                break;
            }


            // ====================================================
            // SEQUENCE_AT_TIME
            // ====================================================

            case ScheduledRule::SEQUENCE_AT_TIME:
            {
                if (!schedCtx.time)
                {
                    LOG_WF(
                        "AutomationEngine",
                        "TimeManager non configurato"
                    );

                    continue;
                }


                struct tm t;

                if (!schedCtx.time->getDateTime(t))
                {
                    LOG_WF(
                        "AutomationEngine",
                        "RTC non valido — "
                        "regole temporali sospese"
                    );

                    continue;
                }


                int nowMin =
                    t.tm_hour * 60 +
                    t.tm_min;

                int targetMin =
                    static_cast<int>(
                        r->intervalMs
                    );


                if (nowMin == targetMin)
                {
                    if (!seq.isRunning() &&
                        now - r->lastRun > 60000)
                    {
                        if (!r->hasCondition ||
                            r->condition.evaluate(this))
                        {
                            seq.start(now);

                            r->lastRun = now;
                        }
                    }
                }


                if (seq.isRunning())
                {
                    seq.update(
                        this,
                        now,
                        actions
                    );
                }

                break;
            }


            // ====================================================
            // SEQUENCE_EVERY_INTERVAL
            // ====================================================

            case ScheduledRule::SEQUENCE_EVERY_INTERVAL:
            {
                unsigned long elapsed =
                    now - r->lastRun;


                if (!seq.isRunning())
                {
                    if (elapsed >= r->intervalMs)
                    {
                        if (!r->hasCondition ||
                            r->condition.evaluate(this))
                        {
                            seq.start(now);

                            r->lastRun = now;
                        }
                    }
                }


                if (seq.isRunning())
                {
                    seq.update(
                        this,
                        now,
                        actions
                    );
                }

                break;
            }


            // ====================================================
            // SEQUENCE_WITH_TIMEOUT
            // ====================================================

            case ScheduledRule::SEQUENCE_WITH_TIMEOUT:
            {
                if (!seq.isRunning())
                    break;


                seq.update(
                    this,
                    now,
                    actions
                );


                if (now - seq.getStartTime() >
                    r->timeoutMs)
                {
                    if (r->hasFallback)
                    {
                        actions.push_back(
                            r->fallbackAction
                        );
                    }

                    seq.stop();
                }

                break;
            }
        }
    }


    // ============================================================
    // DYNAMIC AUTOMATIONS
    // ============================================================

    for (uint8_t i = 0;
         i < dynamicCount;
         i++)
    {
        DynamicAutomation* d =
            dynamic[i];

        if (!d)
            continue;


        if (d->isReady(now) ||
            startupMode)
        {
            unsigned long before = now;

            d->run(
                &schedCtx,
                now
            );


            unsigned long exec = 0;

            if (schedCtx.time)
            {
                exec =
                    schedCtx.time->nowMs() -
                    before;
            }


            if (exec > 10)
            {
                LOG_WF(
                    "AutomationEngine",
                    "DynamicAutomation lenta: "
                    "%lums (interval=%lu)",
                    exec,
                    d->interval
                );
            }
        }
    }


    // ============================================================
    // FINE STARTUP
    // ============================================================

    firstUpdate = false;

    return actions;
}


// ============================================================
// ACTION SEQUENCE
// ============================================================

void AutomationEngine::ActionSequence::addStep(
    const Action& a,
    unsigned long d
)
{
    if (stepCount >= MAX_STEPS)
    {
        LOG_EF(
            "AutomationEngine::ActionSequence",
            "Overflow step: impossibile aggiungere "
            "step (MAX_STEPS=%d)",
            MAX_STEPS
        );

        return;
    }


    steps[stepCount].action = a;

    steps[stepCount].delay = d;

    stepCount++;
}


void AutomationEngine::ActionSequence::start(
    unsigned long now
)
{
    if (stepCount == 0)
    {
        running = false;
        index = -1;

        return;
    }


    running = true;

    index = 0;

    lastTime = now;

    startTime = now;
}


void AutomationEngine::ActionSequence::update(
    AutomationEngine* engine,
    unsigned long now,
    ActionList& actions
)
{
    if (!running ||
        index < 0 ||
        index >= stepCount)
    {
        return;
    }


    if ((long)(now - lastTime) >=
        (long)steps[index].delay)
    {
        // NON eseguiamo più l'Action.
        // La raccogliamo soltanto.
        actions.push_back(
            steps[index].action
        );


        // Evita drift
        lastTime +=
            steps[index].delay;

        index++;


        if (index >= stepCount)
        {
            running = false;

            index = -1;
        }
    }
}


// ============================================================
// ADD SCENE
// ============================================================

AutomationEngine::Scene*
AutomationEngine::addScene(
    const char* name
)
{
    if (sceneCount >= MAX_SCENES)
    {
        LOG_EF(
            "AutomationEngine",
            "Impossibile aggiungere scena '%s': "
            "limite MAX_SCENES raggiunto (%d)",
            name,
            MAX_SCENES
        );

        return nullptr;
    }


    scenes[sceneCount] =
        Scene(name);


    LOG_DF(
        "AutomationEngine",
        "Scene caricata [%d]: %s",
        sceneCount,
        name
    );


    return &scenes[sceneCount++];
}


// ============================================================
// FIND SCENE
// ============================================================

AutomationEngine::Scene*
AutomationEngine::findScene(
    const char* name
)
{
    if (!name || name[0] == '\0')
    {
        LOG_EF(
            "AutomationEngine::findScene",
            "Richiesta scena NON valida "
            "(stringa vuota)"
        );

        return nullptr;
    }


    for (uint8_t i = 0;
         i < sceneCount;
         i++)
    {
        const char* stored =
            scenes[i].name.c_str();


        if (!stored ||
            stored[0] == '\0')
        {
            LOG_WF(
                "AutomationEngine::findScene",
                "Scena[%u] ha nome VUOTO "
                "o NON inizializzato",
                i
            );

            continue;
        }


        if (scenes[i].name.equals(name))
        {
            return &scenes[i];
        }
    }


    LOG_EF(
        "AutomationEngine::findScene",
        "Scena NON trovata: '%s'",
        name
    );


    return nullptr;
}


// ============================================================
// TRIGGER SCENE
// ============================================================

AutomationEngine::ActionList
AutomationEngine::triggerScene(
    const char* name,
    unsigned long now
)
{
    ActionList actions;


    // ============================================================
    // BUILTIN NoAction
    // ============================================================

    if (!name ||
        name[0] == '\0' ||
        strcmp(
            name,
            BuiltinScenes::toString(
                BuiltinScenes::Type::NoAction
            )
        ) == 0)
    {
        return actions;
    }


    Scene* s =
        findScene(name);


    if (!s)
    {
        LOG_EF(
            "AutomationEngine::triggerScene",
            "Impossibile eseguire scena '%s' "
            "(non trovata)",
            name
        );

        return actions;
    }


    bool anyChange = false;


    // ============================================================
    // RACCOLTA ACTION
    // ============================================================

    for (uint8_t i = 0;
         i < s->actionCount;
         i++)
    {
        const Action& a =
            s->actions[i];


        long oldValue;

        bool hasOld =
            getLastValue(
                a.targetArea,
                oldValue
            );


        long before =
            sharedBuffer
                ? sharedBuffer->getValueFast(
                      a.targetArea)
                : 0;


        // Fuori startup:
        // se il Buffer ha già il valore richiesto,
        // non generiamo l'Action.
        if (!startupMode &&
            before == a.value)
        {
            continue;
        }


        // L'Action viene raccolta.
        actions.push_back(a);


        // lastSceneValues rappresenta il valore
        // richiesto da Automation.
        if (!hasOld ||
            oldValue != a.value ||
            startupMode)
        {
            setLastValue(
                a.targetArea,
                a.value
            );

            anyChange = true;
        }
    }


    // ============================================================
    // NESSUNA AZIONE
    // ============================================================

    if (!anyChange &&
        !startupMode)
    {
        return actions;
    }


    // ============================================================
    // LOG
    // ============================================================

    LOG_IF(
        "AutomationEngine::triggerScene",
        startupMode
            ? "FORCED scene: %s"
            : "Executing scene: %s",
        name
    );


    for (uint8_t i = 0;
         i < s->actionCount;
         i++)
    {
        const Action& a =
            s->actions[i];


        LOG_DF(
            "AutomationEngine::triggerScene",
            " → Action targetArea=%d value=%ld",
            a.targetArea,
            a.value
        );
    }


    return actions;
}


#endif
