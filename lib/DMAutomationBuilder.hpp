#ifndef DMAutomationBuilder_HPP
#define DMAutomationBuilder_HPP

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑05‑11
   Note:
                    • Nessuna

   ============================================================================ */

#pragma once

#include "DMAutomation.hpp"
#include "DMLogger.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class AutomationBuilder {
public:

    // ============================================================
    // 1. STRUTTURE DATI CONFIGURAZIONE
    // ============================================================

    struct SceneActionCfg {
        int area;
        long value;
    };

    struct SceneCfg {
        String name;
        std::vector<SceneActionCfg> actions;
    };

    struct ConditionCfg {
        int area;
        String op;
        long threshold;
    };

    struct MultiRuleCfg {
        String logic; // AND / OR
        std::vector<ConditionCfg> conditions;
    };

    struct TrendCfg {
        int area;
        int scale;
        long threshold;
        String trend; // rising / falling
        unsigned long tofMinutes = 0;
    };

    struct BitmaskInputCfg {
        int area;
        int bitIndex;
    };

    struct CompositeInputCfg {
        String type;      // "simple", "debounce", "trend", "bitmask", ...
        String name;      // etichetta logica (per medie/trend/debug)

        int area = -1;

        // --- debounce ---
        long threshold = 0;
        unsigned long debounceMs = 0;

        // --- trend ---
        int scale = 1;
        String trend;     // "rising" / "falling"

        // --- bitmask ---
        int bitIndex = -1;

        // --- TOF ---
        unsigned long tofMinutes = 0;
    };

    struct CompositeOutputCfg {
        int area = -1;
        int bitIndex = -1;
    };

    struct CompositeCfg {
        String logic = "OR";   // AND / OR
        std::vector<CompositeInputCfg> inputs;
        CompositeOutputCfg output;
    };

    struct RuleCfg {
        String name;
        String type; // trend, threshold, bitmask, multi, composite, time, sequence
        unsigned long intervalMs;

        // sotto-configurazioni
        TrendCfg trend;
        MultiRuleCfg multi;
        CompositeCfg composite;
        ConditionCfg threshold;
        BitmaskInputCfg bitmask;

        String sceneTrue;
        String sceneFalse;
    };

    struct SequenceStepCfg {
        int area;
        long value;
        unsigned long delayMs;
    };

    struct SequenceCfg {
        unsigned long intervalMs;
        std::vector<SequenceStepCfg> steps;
    };

    struct AutomationConfig {
        std::vector<SceneCfg> scenes;
        std::vector<RuleCfg> rules;
        std::vector<SequenceCfg> sequences;
    };


    static int resolveAreaOrLog(
        const char* context,
        const char* areaName)
    {
        int area = AreaRegistry::resolve(areaName);

        if (area < 0)
        {
            LOG_EF(
                "AutomationBuilder",
                "%s: area '%s' NON registrata in AreaRegistry",
                context,
                areaName ? areaName : "(null)"
            );
        }

        return area;
    }


    // ============================================================
    // PARSER JSON
    // ============================================================

    static bool parseJson(
        const char* json,
        AutomationConfig& out);


    // ============================================================
    // 2. COSTRUZIONE COMPLETA
    // ============================================================

    void build(
        AutomationEngine& engine,
        const AutomationConfig& cfg)
    {
        // 1. SCENE
        buildScenes(engine, cfg);

        // 3. REGOLE DINAMICHE
        buildDynamicRules(engine, cfg);

        // 6. SEQUENZE
        buildSequences(engine, cfg);

        LOG_IF(
            "AutomationBuilder",
            "Automazioni caricate: scenes=%d dynamic=%d sequences=%d",
            cfg.scenes.size(),
            engine.getDynamicCount(),
            cfg.sequences.size()
        );
    }


private:

    // ============================================================
    // HELPER
    // ============================================================

    static inline bool CompareValue(
        long value,
        const String& op,
        long threshold)
    {
        if (op == ">")  return value > threshold;
        if (op == "<")  return value < threshold;
        if (op == ">=") return value >= threshold;
        if (op == "<=") return value <= threshold;

        return false;
    }


    /*
     * Le DynamicAutomation producono ActionList tramite
     * SchedulerContext::actions.
     *
     * Nessuna coda persistente:
     * la lista appartiene alla singola update() di AutomationEngine.
     */
    static void appendActions(
        AutomationEngine::SchedulerContext* ctx,
        const AutomationEngine::ActionList& actions)
    {
        if (!ctx)
        {
            LOG_E(
                "AutomationBuilder",
                "appendActions: ctx == nullptr"
            );
            return;
        }

        if (!ctx->actions)
        {
            LOG_E(
                "AutomationBuilder",
                "appendActions: ctx->actions == nullptr"
            );
            return;
        }

        ctx->actions->insert(
            ctx->actions->end(),
            actions.begin(),
            actions.end()
        );
    }


    static void appendScene(
        AutomationEngine::SchedulerContext* ctx,
        const char* sceneName,
        unsigned long now)
    {
        if (!ctx || !ctx->engine)
            return;

        if (!sceneName || sceneName[0] == '\0')
            return;

        const auto actions =
            ctx->engine->triggerScene(
                sceneName,
                now
            );

        appendActions(ctx, actions);
    }


    // ============================================================
    // 3. SCENE
    // ============================================================

    static void buildScenes(
        AutomationEngine& engine,
        const AutomationConfig& cfg)
    {
        for (const auto& s : cfg.scenes)
        {
            auto* scene =
                engine.addScene(s.name.c_str());

            if (!scene)
                continue;

            for (const auto& a : s.actions)
            {
                scene->addAction({
                    a.area,
                    a.value
                });
            }
        }
    }


    // ============================================================
    // 4. REGOLE DINAMICHE
    // ============================================================

    void buildDynamicRules(
        AutomationEngine& engine,
        const AutomationConfig& cfg)
    {
        for (const auto& r : cfg.rules)
        {
            if (r.type == "trend")
            {
                auto* rule = new TrendRule(r);

                engine.addDynamicAutomation(
                    r.intervalMs,
                    TrendRule::callback,
                    rule
                );
            }

            else if (r.type == "threshold")
            {
                auto* rule = new ThresholdRule(r);

                engine.addDynamicAutomation(
                    r.intervalMs,
                    ThresholdRule::callback,
                    rule
                );
            }

            else if (r.type == "bitmask")
            {
                auto* rule = new BitmaskRule(r);

                engine.addDynamicAutomation(
                    r.intervalMs,
                    BitmaskRule::callback,
                    rule
                );
            }

            else if (r.type == "multi")
            {
                auto* rule = new MultiRule(r);

                engine.addDynamicAutomation(
                    r.intervalMs,
                    MultiRule::callback,
                    rule
                );
            }

            else if (r.type == "time")
            {
                auto* rule = new TimeRule(r);

                engine.addDynamicAutomation(
                    60000,
                    TimeRule::callback,
                    rule
                );
            }

            else if (r.type == "sequence")
            {
                auto* rule = new SequenceRule(r);

                engine.addDynamicAutomation(
                    r.intervalMs,
                    SequenceRule::callback,
                    rule
                );
            }

            else if (r.type == "composite")
            {
                auto* rule = new CompositeRule(r);

                engine.addDynamicAutomation(
                    r.intervalMs,
                    CompositeRule::callback,
                    rule
                );

                LOG_DF(
                    "AutomationBuilder",
                    "CompositeRule dinamica caricata: '%s' con %d input",
                    r.name.c_str(),
                    r.composite.inputs.size()
                );
            }

            LOG_DF(
                "AutomationBuilder",
                "AGGIUNTA Regola '%s' type=%s inputs=%d",
                r.name.c_str(),
                r.type.c_str(),
                r.composite.inputs.size()
            );
        }
    }


    // ============================================================
    // 7. SEQUENZE
    // ============================================================

    void buildSequences(
        AutomationEngine& engine,
        const AutomationConfig& cfg)
    {
        for (const auto& seqCfg : cfg.sequences)
        {
            auto* seq =
                new AutomationEngine::ActionSequence();

            if (!seq)
                continue;

            for (const auto& step : seqCfg.steps)
            {
                seq->addStep(
                    {
                        step.area,
                        step.value
                    },
                    step.delayMs
                );
            }

            engine.addDynamicAutomation(
                seqCfg.intervalMs,

                [](
                    AutomationEngine::SchedulerContext* ctx,
                    unsigned long now,
                    void* userCtx)
                {
                    auto* s =
                        static_cast<
                            AutomationEngine::ActionSequence*
                        >(userCtx);

                    if (!s)
                        return;

                    if (!s->isRunning())
                        s->start(now);

                    if (!ctx)
                        return;

                    if (!ctx->actions)
                        return;

                    s->update(
                        ctx->engine,
                        now,
                        *ctx->actions
                    );
                },

                seq
            );
        }
    }


    // ============================================================
    // 8. REGOLE DINAMICHE
    // ============================================================


    // ------------------------------------------------------------
    // TrendRule
    // ------------------------------------------------------------

    class TrendRule {
    public:

        explicit TrendRule(const RuleCfg& cfg)
            : cfg(cfg)
        {
        }


        static void callback(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now,
            void* userCtx)
        {
            auto* self =
                static_cast<TrendRule*>(userCtx);

            if (!self)
                return;

            self->run(ctx, now);
        }


        void run(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now)
        {
            if (!ctx || !ctx->buffer || !ctx->medie)
                return;

            auto& buffer = *ctx->buffer;
            auto& medie  = *ctx->medie;

            long value =
                buffer.getValueFast(
                    cfg.trend.area,
                    cfg.trend.scale
                );

            medie.addMeasurement(
                cfg.name,
                value
            );

            auto trend =
                medie.groupTrend(
                    cfg.name
                );

            const bool cond =
                (
                    cfg.trend.trend == "rising" &&
                    trend == Group::INCREASING &&
                    value > cfg.trend.threshold
                )
                ||
                (
                    cfg.trend.trend == "falling" &&
                    trend == Group::DECREASING &&
                    value < cfg.trend.threshold
                );

            appendScene(
                ctx,
                cond
                    ? cfg.sceneTrue.c_str()
                    : cfg.sceneFalse.c_str(),
                now
            );
        }


    private:

        RuleCfg cfg;
    };


    // ------------------------------------------------------------
    // ThresholdRule
    // ------------------------------------------------------------

    class ThresholdRule {
    public:

        explicit ThresholdRule(const RuleCfg& cfg)
            : cfg(cfg)
        {
        }


        static void callback(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now,
            void* userCtx)
        {
            auto* self =
                static_cast<ThresholdRule*>(userCtx);

            if (!self)
                return;

            self->run(ctx, now);
        }


        void run(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now)
        {
            if (!ctx || !ctx->buffer)
                return;

            if (cfg.threshold.area < 0)
            {
                LOG_WF(
                    "ThresholdRule",
                    "Regola '%s' ignorata: area non valida (%d)",
                    cfg.name.c_str(),
                    cfg.threshold.area
                );

                return;
            }

            auto& buffer = *ctx->buffer;

            long v =
                buffer.getValueFast(
                    cfg.threshold.area
                );

            bool cond =
                CompareValue(
                    v,
                    cfg.threshold.op,
                    cfg.threshold.threshold
                );

            appendScene(
                ctx,
                cond
                    ? cfg.sceneTrue.c_str()
                    : cfg.sceneFalse.c_str(),
                now
            );
        }


    private:

        RuleCfg cfg;
    };


    // ------------------------------------------------------------
    // BitmaskRule
    // ------------------------------------------------------------

    class BitmaskRule {
    public:

        explicit BitmaskRule(const RuleCfg& cfg)
            : cfg(cfg)
        {
        }


        static void callback(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now,
            void* userCtx)
        {
            auto* self =
                static_cast<BitmaskRule*>(userCtx);

            if (!self)
                return;

            self->run(ctx, now);
        }


        void run(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now)
        {
            if (!ctx || !ctx->buffer)
                return;

            auto& buffer = *ctx->buffer;

            long bits =
                buffer.getValueFast(
                    cfg.bitmask.area
                );

            bool cond =
                bitRead(
                    bits,
                    cfg.bitmask.bitIndex
                );

            appendScene(
                ctx,
                cond
                    ? cfg.sceneTrue.c_str()
                    : cfg.sceneFalse.c_str(),
                now
            );
        }


    private:

        RuleCfg cfg;
    };


    // ------------------------------------------------------------
    // MultiRule
    // ------------------------------------------------------------

    class MultiRule {
    public:

        explicit MultiRule(const RuleCfg& cfg)
            : cfg(cfg)
        {
        }


        static void callback(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now,
            void* userCtx)
        {
            auto* self =
                static_cast<MultiRule*>(userCtx);

            if (!self)
                return;

            self->run(ctx, now);
        }


        void run(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now)
        {
            if (!ctx || !ctx->buffer)
                return;

            auto& buffer = *ctx->buffer;

            bool result =
                (cfg.multi.logic == "AND");

            for (const auto& c : cfg.multi.conditions)
            {
                long v =
                    buffer.getValueFast(
                        c.area
                    );

                bool cond =
                    CompareValue(
                        v,
                        c.op,
                        c.threshold
                    );

                if (cfg.multi.logic == "AND")
                    result &= cond;

                else if (cfg.multi.logic == "OR")
                    result |= cond;
            }

            appendScene(
                ctx,
                result
                    ? cfg.sceneTrue.c_str()
                    : cfg.sceneFalse.c_str(),
                now
            );
        }


    private:

        RuleCfg cfg;
    };


    // ------------------------------------------------------------
    // TimeRule
    // ------------------------------------------------------------

    class TimeRule {
    public:

        explicit TimeRule(const RuleCfg& cfg)
            : cfg(cfg)
        {
        }


        static void callback(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now,
            void* userCtx)
        {
            auto* self =
                static_cast<TimeRule*>(userCtx);

            if (!self)
                return;

            self->run(ctx, now);
        }


        void run(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now)
        {
            if (!ctx || !ctx->time)
                return;

            struct tm t;

            if (!ctx->time->getDateTime(t))
                return;

            int nowMin =
                t.tm_hour * 60 +
                t.tm_min;

            if (nowMin == cfg.threshold.threshold)
            {
                appendScene(
                    ctx,
                    cfg.sceneTrue.c_str(),
                    now
                );
            }
        }


    private:

        RuleCfg cfg;
    };


    // ------------------------------------------------------------
    // SequenceRule
    // ------------------------------------------------------------

    class SequenceRule {
    public:

        explicit SequenceRule(const RuleCfg& cfg)
            : cfg(cfg)
        {
        }


        static void callback(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now,
            void* userCtx)
        {
            auto* self =
                static_cast<SequenceRule*>(userCtx);

            if (!self)
                return;

            self->run(ctx, now);
        }


        void run(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now)
        {
            if (!ctx)
                return;

            if (!ctx->actions)
                return;

            if (!seq.isRunning())
                seq.start(now);

            seq.update(
                ctx->engine,
                now,
                *ctx->actions
            );
        }


    private:

        RuleCfg cfg;
        AutomationEngine::ActionSequence seq;
    };


    // ============================================================
    // 8B. COMPOSITE INPUT
    // ============================================================

    class CompositeInput {
    public:

        virtual bool evaluate(
            Buffer& buffer,
            AverageCalculator& medie,
            unsigned long now) = 0;

        virtual ~CompositeInput()
        {
        }
    };


    // ------------------------------------------------------------
    // DebounceInput
    // ------------------------------------------------------------

    class DebounceInput : public CompositeInput {
        CompositeInputCfg cfg;
        FastDebounce deb;
        TOF tof;

    public:

        explicit DebounceInput(
            const CompositeInputCfg& c)
            : cfg(c),
              deb(c.debounceMs),
              tof(
                  c.tofMinutes,
                  TimerBase::Minutes
              )
        {
        }


        bool evaluate(
            Buffer& buffer,
            AverageCalculator&,
            unsigned long now) override
        {
            long v =
                buffer.getValueFast(
                    cfg.area
                );

            bool raw =
                deb.update(
                    v > cfg.threshold
                );

            tof.Run(raw);

            return raw || tof.Q();
        }
    };


    // ------------------------------------------------------------
    // TrendInput
    // ------------------------------------------------------------

    class TrendInput : public CompositeInput {
        CompositeInputCfg cfg;
        TOF tof;

    public:

        explicit TrendInput(
            const CompositeInputCfg& c)
            : cfg(c),
              tof(
                  c.tofMinutes,
                  TimerBase::Minutes
              )
        {
        }


        bool evaluate(
            Buffer& buffer,
            AverageCalculator& medie,
            unsigned long now) override
        {
            long v =
                buffer.getValueFast(
                    cfg.area,
                    cfg.scale
                );

            medie.addMeasurement(
                cfg.name,
                v
            );

            auto trend =
                medie.groupTrend(
                    cfg.name
                );

            bool raw =
                (
                    cfg.trend == "rising" &&
                    trend == Group::INCREASING &&
                    v > cfg.threshold
                )
                ||
                (
                    cfg.trend == "falling" &&
                    trend == Group::DECREASING &&
                    v < cfg.threshold
                );

            tof.Run(raw);

            return raw || tof.Q();
        }
    };


    // ------------------------------------------------------------
    // BitmaskInput
    // ------------------------------------------------------------

    class BitmaskInput : public CompositeInput {
        CompositeInputCfg cfg;

    public:

        explicit BitmaskInput(
            const CompositeInputCfg& c)
            : cfg(c)
        {
        }


        bool evaluate(
            Buffer& buffer,
            AverageCalculator&,
            unsigned long now) override
        {
            long bits =
                buffer.getValueFast(
                    cfg.area
                );

            return bitRead(
                bits,
                cfg.bitIndex
            );
        }
    };


    // ------------------------------------------------------------
    // SimpleInput
    // ------------------------------------------------------------

    class SimpleInput : public CompositeInput {
        int area;

    public:

        explicit SimpleInput(int a)
            : area(a)
        {
        }


        bool evaluate(
            Buffer& buffer,
            AverageCalculator&,
            unsigned long now) override
        {
            return buffer.getValueFast(area) != 0;
        }
    };


    // ------------------------------------------------------------
    // createInput
    // ------------------------------------------------------------

    static CompositeInput* createInput(
        const CompositeInputCfg& cfg)
    {
        // 1. Input semplice
        if (cfg.type == "simple")
            return new SimpleInput(cfg.area);

        // 2. Debounce
        if (cfg.type == "debounce")
            return new DebounceInput(cfg);

        // 3. Trend
        if (cfg.type == "trend")
            return new TrendInput(cfg);

        // 4. Bitmask
        if (cfg.type == "bitmask")
            return new BitmaskInput(cfg);

        LOG_WF(
            "CompositeRule",
            "Tipo input '%s' NON supportato",
            cfg.type.c_str()
        );

        return nullptr;
    }


    // ============================================================
    // CompositeRule
    // ============================================================

    class CompositeRule {
    public:

        explicit CompositeRule(
            const RuleCfg& cfg)
            : cfg(cfg)
        {
            for (const auto& ic :
                 cfg.composite.inputs)
            {
                CompositeInput* in =
                    createInput(ic);

                if (in)
                    inputs.push_back(in);
            }
        }


        ~CompositeRule()
        {
            for (auto* input : inputs)
                delete input;

            inputs.clear();
        }


        static void callback(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now,
            void* userCtx)
        {
            auto* self =
                static_cast<CompositeRule*>(userCtx);

            if (!self)
                return;

            self->run(ctx, now);
        }


        void run(
            AutomationEngine::SchedulerContext* ctx,
            unsigned long now)
        {
            if (!ctx ||
                !ctx->buffer ||
                !ctx->medie)
            {
                return;
            }

            auto& buffer = *ctx->buffer;
            auto& medie  = *ctx->medie;

            bool result =
                (cfg.composite.logic == "AND");

            for (auto* in : inputs)
            {
                if (!in)
                    continue;

                bool v =
                    in->evaluate(
                        buffer,
                        medie,
                        now
                    );

                if (cfg.composite.logic == "AND")
                    result &= v;

                else if (cfg.composite.logic == "OR")
                    result |= v;
            }


            // ----------------------------------------------------
            // OUTPUT BITMASK
            //
            // NON scrive più direttamente nel Buffer.
            // Produce una Action.
            // ----------------------------------------------------

            if (cfg.composite.output.area >= 0 &&
                cfg.composite.output.bitIndex >= 0)
            {
                long bits =
                    buffer.getValueFast(
                        cfg.composite.output.area
                    );

                bitWrite(
                    bits,
                    cfg.composite.output.bitIndex,
                    result
                );

                if (ctx->actions)
                {
                    ctx->actions->push_back({
                        cfg.composite.output.area,
                        bits
                    });
                }
            }


            // ----------------------------------------------------
            // ATTIVA SCENA
            // ----------------------------------------------------

            appendScene(
                ctx,
                result
                    ? cfg.sceneTrue.c_str()
                    : cfg.sceneFalse.c_str(),
                now
            );
        }


    private:

        RuleCfg cfg;

        std::vector<CompositeInput*> inputs;
    };


public:

    // ============================================================
    // DUMP CONFIGURAZIONE AUTOMAZIONI
    // ============================================================

    static void ReportAutomationConfig(
        const AutomationConfig& cfg)
    {
        Serial.println(
            "\n===== AUTOMATION CONFIG DUMP ====="
        );


        // --------------------------------------------------------
        // 1. SCENE
        // --------------------------------------------------------

        Serial.print("\nScenes: ");
        Serial.println(cfg.scenes.size());

        for (const auto& s : cfg.scenes)
        {
            Serial.println(
                "------------------------------"
            );

            Serial.print("Scene: ");
            Serial.println(s.name);

            if (s.actions.empty())
            {
                Serial.println(
                    " - WARNING: Scene has no actions"
                );

                continue;
            }

            for (const auto& a : s.actions)
            {
                Serial.print(
                    "   Action -> area="
                );

                Serial.print(a.area);

                Serial.print(
                    "  value="
                );

                Serial.println(a.value);
            }
        }


        // --------------------------------------------------------
        // 2. RULES
        // --------------------------------------------------------

        Serial.print("\nRules: ");
        Serial.println(cfg.rules.size());

        for (const auto& r : cfg.rules)
        {
            Serial.println(
                "------------------------------"
            );

            Serial.print("Rule: ");
            Serial.println(r.name);

            Serial.print("Type: ");
            Serial.println(r.type);

            Serial.print("Interval: ");
            Serial.println(r.intervalMs);

            Serial.print("SceneTrue: ");
            Serial.println(r.sceneTrue);

            Serial.print("SceneFalse: ");
            Serial.println(r.sceneFalse);


            // threshold
            if (r.type == "threshold")
            {
                Serial.print(
                    "  Threshold area="
                );

                Serial.print(
                    r.threshold.area
                );

                Serial.print(
                    " op="
                );

                Serial.print(
                    r.threshold.op
                );

                Serial.print(
                    " thr="
                );

                Serial.println(
                    r.threshold.threshold
                );
            }


            // trend
            if (r.type == "trend")
            {
                Serial.print(
                    "  Trend area="
                );

                Serial.print(
                    r.trend.area
                );

                Serial.print(
                    " scale="
                );

                Serial.print(
                    r.trend.scale
                );

                Serial.print(
                    " thr="
                );

                Serial.print(
                    r.trend.threshold
                );

                Serial.print(
                    " trend="
                );

                Serial.println(
                    r.trend.trend
                );
            }


            // bitmask
            if (r.type == "bitmask")
            {
                Serial.print(
                    "  Bitmask area="
                );

                Serial.print(
                    r.bitmask.area
                );

                Serial.print(
                    " bit="
                );

                Serial.println(
                    r.bitmask.bitIndex
                );
            }


            // multi
            if (r.type == "multi")
            {
                Serial.print(
                    "  Multi logic="
                );

                Serial.println(
                    r.multi.logic
                );

                for (const auto& c :
                     r.multi.conditions)
                {
                    Serial.print(
                        "    Cond area="
                    );

                    Serial.print(c.area);

                    Serial.print(
                        " op="
                    );

                    Serial.print(c.op);

                    Serial.print(
                        " thr="
                    );

                    Serial.println(
                        c.threshold
                    );
                }
            }


            // composite
            if (r.type == "composite")
            {
                Serial.print(
                    "  Composite logic="
                );

                Serial.println(
                    r.composite.logic
                );

                Serial.print(
                    "  Inputs="
                );

                Serial.println(
                    r.composite.inputs.size()
                );

                for (const auto& ic :
                     r.composite.inputs)
                {
                    Serial.print(
                        "    Input type="
                    );

                    Serial.print(
                        ic.type
                    );

                    Serial.print(
                        " area="
                    );

                    Serial.print(
                        ic.area
                    );

                    Serial.print(
                        " bit="
                    );

                    Serial.print(
                        ic.bitIndex
                    );

                    Serial.print(
                        " threshold="
                    );

                    Serial.println(
                        ic.threshold
                    );
                }

                Serial.print(
                    "  Output area="
                );

                Serial.print(
                    r.composite.output.area
                );

                Serial.print(
                    " bit="
                );

                Serial.println(
                    r.composite.output.bitIndex
                );
            }
        }


        // --------------------------------------------------------
        // 5. SEQUENCES
        // --------------------------------------------------------

        Serial.print("\nSequences: ");
        Serial.println(cfg.sequences.size());

        for (const auto& s :
             cfg.sequences)
        {
            Serial.println(
                "------------------------------"
            );

            Serial.print(
                "Interval: "
            );

            Serial.println(
                s.intervalMs
            );

            for (const auto& st :
                 s.steps)
            {
                Serial.print(
                    "  Step area="
                );

                Serial.print(
                    st.area
                );

                Serial.print(
                    " value="
                );

                Serial.print(
                    st.value
                );

                Serial.print(
                    " delay="
                );

                Serial.println(
                    st.delayMs
                );
            }
        }


        Serial.println(
            "\n===== END AUTOMATION CONFIG DUMP =====\n"
        );
    }
};


// ============================================================================
// JSON PARSER
// ============================================================================

bool AutomationBuilder::parseJson(
    const char* json,
    AutomationConfig& out)
{
    if (!json || json[0] == '\0')
    {
        LOG_EF(
            "Automation",
            "JSON vuoto"
        );

        return false;
    }


    StaticJsonDocument<8192> doc;

    DeserializationError err =
        deserializeJson(
            doc,
            json
        );

    if (err)
    {
        LOG_EF(
            "Automation",
            "Errore parsing JSON: %s",
            err.c_str()
        );

        return false;
    }


    // ============================================================
    // SCENES
    // ============================================================

    if (doc.containsKey("scenes"))
    {
        for (
            JsonVariant v :
            doc["scenes"].as<JsonArray>())
        {
            SceneCfg sc;

            sc.name =
                v["name"] | "";


            for (
                JsonVariant a :
                v["actions"].as<JsonArray>())
            {
                SceneActionCfg act;

                const char* areaName =
                    a["area"] | "";

                act.area =
                    resolveAreaOrLog(
                        sc.name.c_str(),
                        areaName
                    );

                if (act.area < 0)
                    continue;

                act.value =
                    a["value"] | 0;

                sc.actions.push_back(
                    act
                );
            }


            if (sc.actions.empty())
            {
                LOG_WF(
                    "AutomationBuilder",
                    "Scene '%s' ignorata: nessuna action valida",
                    sc.name.c_str()
                );

                continue;
            }


            out.scenes.push_back(
                sc
            );
        }
    }


    // ============================================================
    // RULES
    // ============================================================

    if (doc.containsKey("rules"))
    {
        for (
            JsonVariant v :
            doc["rules"].as<JsonArray>())
        {
            RuleCfg rc;

            rc.name =
                v["name"] | "";

            rc.type =
                v["type"] | "";

            rc.intervalMs =
                v["intervalMs"] | 1000;


            // ----------------------------------------------------
            // threshold
            // ----------------------------------------------------

            if (v.containsKey("threshold"))
            {
                rc.threshold.area =
                    AreaRegistry::resolve(
                        v["threshold"]["area"] | ""
                    );

                rc.threshold.op =
                    v["threshold"]["op"] | "";

                rc.threshold.threshold =
                    v["threshold"]["threshold"] | 0;
            }


            // ----------------------------------------------------
            // trend
            // ----------------------------------------------------

            if (v.containsKey("trend"))
            {
                rc.trend.area =
                    AreaRegistry::resolve(
                        v["trend"]["area"] | ""
                    );

                rc.trend.scale =
                    v["trend"]["scale"] | 1;

                rc.trend.threshold =
                    v["trend"]["threshold"] | 0;

                rc.trend.trend =
                    v["trend"]["trend"] | "";
            }


            // ----------------------------------------------------
            // bitmask
            // ----------------------------------------------------

            if (v.containsKey("bitmask"))
            {
                rc.bitmask.area =
                    AreaRegistry::resolve(
                        v["bitmask"]["area"] | ""
                    );

                rc.bitmask.bitIndex =
                    v["bitmask"]["bitIndex"] | 0;
            }


            // ----------------------------------------------------
            // multi
            // ----------------------------------------------------

            if (v.containsKey("multi"))
            {
                rc.multi.logic =
                    v["multi"]["logic"] | "AND";


                for (
                    JsonVariant c :
                    v["multi"]["conditions"]
                        .as<JsonArray>())
                {
                    ConditionCfg cc;

                    cc.area =
                        AreaRegistry::resolve(
                            c["area"] | ""
                        );

                    cc.op =
                        c["op"] | "";

                    cc.threshold =
                        c["threshold"] | 0;

                    rc.multi.conditions.push_back(
                        cc
                    );
                }
            }


            // ----------------------------------------------------
            // COMPOSITE
            // ----------------------------------------------------

            if (v.containsKey("composite"))
            {
                JsonObject comp =
                    v["composite"];


                rc.composite.logic =
                    comp["logic"] | "OR";


                // ------------------------------------------------
                // INPUTS
                // ------------------------------------------------

                if (comp.containsKey("inputs"))
                {
                    for (
                        JsonVariant in :
                        comp["inputs"]
                            .as<JsonArray>())
                    {
                        CompositeInputCfg ic;


                        ic.type =
                            in["type"] | "";

                        ic.name =
                            in["name"] | "";


                        const char* areaName =
                            in["area"] | "";


                        ic.area =
                            resolveAreaOrLog(
                                rc.name.c_str(),
                                areaName
                            );


                        // debounce
                        ic.threshold =
                            in["threshold"] | 0;

                        ic.debounceMs =
                            in["debounceMs"] | 0;


                        // trend
                        ic.scale =
                            in["scale"] | 1;

                        ic.trend =
                            in["trend"] | "";


                        // bitmask
                        ic.bitIndex =
                            in["bitIndex"] | -1;


                        // TOF
                        ic.tofMinutes =
                            in["tofMinutes"] | 0;


                        rc.composite.inputs.push_back(
                            ic
                        );
                    }
                }


                // ------------------------------------------------
                // OUTPUT
                // ------------------------------------------------

                if (comp.containsKey("output"))
                {
                    JsonObject output =
                        comp["output"];


                    const char* areaName =
                        output["area"] | "";


                    rc.composite.output.area =
                        resolveAreaOrLog(
                            rc.name.c_str(),
                            areaName
                        );


                    rc.composite.output.bitIndex =
                        output["bitIndex"] | -1;
                }
            }


            // ----------------------------------------------------
            // SCENE TRUE / FALSE
            // ----------------------------------------------------

            rc.sceneTrue =
                v["sceneTrue"] | "";

            rc.sceneFalse =
                v["sceneFalse"] | "";


            // ====================================================
            // VALIDAZIONE COMPOSITE
            // ====================================================

            if (rc.type == "composite")
            {
                // almeno 1 input
                if (rc.composite.inputs.empty())
                {
                    LOG_EF(
                        "AutomationBuilder",
                        "Regola composite '%s' ignorata: composite senza inputs",
                        rc.name.c_str()
                    );

                    continue;
                }


                // validazione aree input
                bool invalid = false;

                for (const auto& ic :
                     rc.composite.inputs)
                {
                    if (ic.area < 0)
                        invalid = true;
                }


                if (invalid)
                {
                    LOG_EF(
                        "AutomationBuilder",
                        "Regola composite '%s' ignorata: composite con area input non valida",
                        rc.name.c_str()
                    );

                    continue;
                }


                // validazione output
                if (rc.composite.output.area < 0)
                {
                    LOG_EF(
                        "AutomationBuilder",
                        "Regola composite '%s' ignorata: composite output area non valida",
                        rc.name.c_str()
                    );

                    continue;
                }


                if (rc.composite.output.bitIndex < 0)
                {
                    LOG_EF(
                        "AutomationBuilder",
                        "Regola composite '%s' ignorata: composite output bit non valido",
                        rc.name.c_str()
                    );

                    continue;
                }
            }


            // ====================================================
            // VALIDAZIONE GENERALE
            // ====================================================

            if (
                rc.type == "threshold" &&
                rc.threshold.area < 0)
            {
                continue;
            }


            if (
                rc.type == "trend" &&
                rc.trend.area < 0)
            {
                continue;
            }


            if (
                rc.type == "bitmask" &&
                rc.bitmask.area < 0)
            {
                continue;
            }


            // ====================================================
            // VALIDAZIONE SCENE TRUE/FALSE
            // ====================================================

            const String noActionName =
                AutomationEngine::BuiltinScenes::toString(
                    AutomationEngine::BuiltinScenes::Type::NoAction
                );


            bool sceneTrueExists =
                (rc.sceneTrue == noActionName);

            bool sceneFalseExists =
                (rc.sceneFalse == noActionName);


            // sceneTrue
            if (!sceneTrueExists)
            {
                for (const auto& s :
                     out.scenes)
                {
                    if (s.name == rc.sceneTrue)
                    {
                        sceneTrueExists = true;
                        break;
                    }
                }
            }


            // sceneFalse
            if (!sceneFalseExists)
            {
                for (const auto& s :
                     out.scenes)
                {
                    if (s.name == rc.sceneFalse)
                    {
                        sceneFalseExists = true;
                        break;
                    }
                }
            }


            // ----------------------------------------------------
            // sceneTrue mancante
            // ----------------------------------------------------

            if (!sceneTrueExists)
            {
                LOG_EF(
                    "AutomationBuilder",
                    "Regola scene '%s' ignorata: sceneTrue '%s' non esiste",
                    rc.name.c_str(),
                    rc.sceneTrue.c_str()
                );

                continue;
            }


            // ----------------------------------------------------
            // sceneFalse mancante
            // ----------------------------------------------------

            if (!sceneFalseExists)
            {
                LOG_EF(
                    "AutomationBuilder",
                    "Regola scene '%s' ignorata: sceneFalse '%s' non esiste",
                    rc.name.c_str(),
                    rc.sceneFalse.c_str()
                );

                continue;
            }


            out.rules.push_back(
                rc
            );
        }
    }


    // ============================================================
    // SEQUENCES
    // ============================================================

    if (doc.containsKey("sequences"))
    {
        for (
            JsonVariant v :
            doc["sequences"].as<JsonArray>())
        {
            SequenceCfg sc;

            sc.intervalMs =
                v["intervalMs"] | 1000;


            for (
                JsonVariant step :
                v["steps"].as<JsonArray>())
            {
                SequenceStepCfg st;


                String key =
                    step["area"] | "";


                st.area =
                    AreaRegistry::resolve(
                        key
                    );


                st.value =
                    step["value"] | 0;


                st.delayMs =
                    step["delayMs"] | 0;


                if (st.area < 0)
                {
                    LOG_EF(
                        "AutomationBuilder",
                        "Sequence ignorata: area '%s' non valida",
                        key.c_str()
                    );

                    continue;
                }


                sc.steps.push_back(
                    st
                );
            }


            if (!sc.steps.empty())
            {
                out.sequences.push_back(
                    sc
                );
            }
            else
            {
                LOG_WF(
                    "AutomationBuilder",
                    "Sequence ignorata: nessuno step valido"
                );
            }
        }
    }


    LOG_D(
        "Automation",
        "JSON automazioni caricato correttamente"
    );

    return true;
}

#endif

