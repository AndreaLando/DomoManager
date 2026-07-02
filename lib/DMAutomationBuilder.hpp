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
#include "DMFncs.hpp"
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

    struct DebounceCfg {
        int area;
        long threshold;
        unsigned long debounceMs;
        unsigned long tofMinutes;
    };

    struct BitmaskInputCfg {
        int area;
        int bitIndex;
    };

    struct CompositeInputCfg {
        String type;      // "debounce", "trend", "bitmask", ...
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

    static int resolveAreaOrLog(const char* context, const char* areaName) {
        int area = AreaRegistry::resolve(areaName);

        if (area < 0) {
            LOG_EF("AutomationBuilder",
                "%s: area '%s' NON registrata in AreaRegistry",
                context,
                areaName ? areaName : "(null)");
        }

        return area;
    }


    // ============================================================
    // PARSER JSON
    // ============================================================
    static bool parseJson(const char* json, AutomationConfig& out);

    // ============================================================
    // 2. COSTRUZIONE COMPLETA
    // ============================================================
    void build(AutomationEngine& engine,
                              Buffer& buffer,
                              AverageCalculator & medie,
                              TimeManager& time,
                              const AutomationConfig& cfg)
    {
        // 1. SCENE
        buildScenes(engine, cfg);

        // 3. REGOLE DINAMICHE
        buildDynamicRules(engine, buffer, medie, time, cfg);

        // 6. SEQUENZE
        buildSequences(engine, cfg);

        LOG_IF("AutomationBuilder",
            "Automazioni caricate: scenes=%d dynamic=%d sequences=%d",
            cfg.scenes.size(),
            engine.getDynamicCount(),
            cfg.sequences.size());

    }


private:

    // ============================================================
    // 3. SCENE
    // ============================================================
    static void buildScenes(AutomationEngine& engine, const AutomationConfig& cfg) {
        for (const auto& s : cfg.scenes) {
            auto* scene = engine.addScene(s.name.c_str());
            if (!scene) continue;

            for (const auto& a : s.actions)
                scene->addAction({ a.area, a.value });
        }
    }

    // ============================================================
    // 4. REGOLE DINAMICHE
    // ============================================================
    void buildDynamicRules(AutomationEngine& engine,
                                          Buffer& buffer,
                                          AverageCalculator & medie,
                                          TimeManager& time,
                                          const AutomationConfig& cfg)
    {
        for (const auto& r : cfg.rules) {

            if (r.type == "trend") {
                auto* rule = new TrendRule(r);
                engine.addDynamicAutomation(r.intervalMs, TrendRule::callback, rule);
            }

            else if (r.type == "threshold") {
                auto* rule = new ThresholdRule(r);
                engine.addDynamicAutomation(r.intervalMs, ThresholdRule::callback, rule);
            }

            else if (r.type == "bitmask") {
                auto* rule = new BitmaskRule(r);
                engine.addDynamicAutomation(r.intervalMs, BitmaskRule::callback, rule);
            }

            else if (r.type == "multi") {
                auto* rule = new MultiRule(r);
                engine.addDynamicAutomation(r.intervalMs, MultiRule::callback, rule);
            }

            else if (r.type == "time") {
                auto* rule = new TimeRule(r);
                engine.addDynamicAutomation(60000, TimeRule::callback, rule);
            }

            else if (r.type == "sequence") {
                auto* rule = new SequenceRule(r);
                engine.addDynamicAutomation(r.intervalMs, SequenceRule::callback, rule);
            }

            else if (r.type == "composite") {
                // Crea la nuova CompositeRule dinamica
                auto* rule = new CompositeRule(r);

                // Registra la regola nel motore
                engine.addDynamicAutomation(
                    r.intervalMs,
                    CompositeRule::callback,
                    rule
                );

                LOG_DF("AutomationBuilder",
                    "CompositeRule dinamica caricata: '%s' con %d input",
                    r.name.c_str(),
                    r.composite.inputs.size()
                );
            }


            LOG_DF("AutomationBuilder",
                "AGGIUNTA Regola '%s' type=%s inputs=%d",
                r.name.c_str(),
                r.type.c_str(),
                r.composite.inputs.size());

        }
    }

    // ============================================================
    // 7. SEQUENZE
    // ============================================================
    void buildSequences(AutomationEngine& engine,
                                       const AutomationConfig& cfg)
    {
        for (const auto& seqCfg : cfg.sequences) {
            auto* seq = new AutomationEngine::ActionSequence();

            for (const auto& step : seqCfg.steps)
                seq->addStep({ step.area, step.value }, step.delayMs);

            engine.addDynamicAutomation(
                seqCfg.intervalMs,
                [](AutomationEngine::SchedulerContext* ctx, unsigned long now, void* userCtx) {
                    auto* s = static_cast<AutomationEngine::ActionSequence*>(userCtx);
                    if (!s->isRunning())
                        s->start(now);
                    s->update(ctx->engine, now);
                },
                seq
            );
        }
    }


    // ============================================================
    // 8. REGOLE DINAMICHE (IMPLEMENTAZIONI)
    // ============================================================

    // --- TrendRule ---
    class TrendRule {
    public:
        TrendRule(const RuleCfg& cfg) : cfg(cfg) {}

        static void callback(AutomationEngine::SchedulerContext* ctx,
                             unsigned long now,
                             void* userCtx)
        {
            static_cast<TrendRule*>(userCtx)->run(ctx, now);
        }

        void run(AutomationEngine::SchedulerContext* ctx, unsigned long now) {
            auto& buffer = *ctx->buffer;
            auto& medie  = *ctx->medie;

            long value = buffer.getValueFast(cfg.trend.area, cfg.trend.scale);

            medie.addMeasurement(cfg.name, value);
            auto trend = medie.groupTrend(cfg.name);

            bool cond = (cfg.trend.trend == "rising"  && trend == Group::INCREASING && value > cfg.trend.threshold) ||
                        (cfg.trend.trend == "falling" && trend == Group::DECREASING && value < cfg.trend.threshold);

            ctx->engine->triggerScene(cond ? cfg.sceneTrue.c_str() : cfg.sceneFalse.c_str(), now);
        }

    private:
        RuleCfg cfg;
    };

    // --- ThresholdRule ---
    class ThresholdRule {
    public:
        ThresholdRule(const RuleCfg& cfg) : cfg(cfg) {}

        static void callback(AutomationEngine::SchedulerContext* ctx,
                             unsigned long now,
                             void* userCtx)
        {
            static_cast<ThresholdRule*>(userCtx)->run(ctx, now);
        }

        void run(AutomationEngine::SchedulerContext* ctx, unsigned long now) {
            auto& buffer = *ctx->buffer;
            if (cfg.threshold.area < 0) {
                LOG_WF("ThresholdRule",
                    "Regola '%s' ignorata: area non valida (%d)",
                    cfg.name.c_str(), cfg.threshold.area);
                return;
            }
            long v = buffer.getValueFast(cfg.threshold.area);

            bool cond = false;
            if (cfg.threshold.op == ">")  cond = v >  cfg.threshold.threshold;
            if (cfg.threshold.op == "<")  cond = v <  cfg.threshold.threshold;
            if (cfg.threshold.op == ">=") cond = v >= cfg.threshold.threshold;
            if (cfg.threshold.op == "<=") cond = v <= cfg.threshold.threshold;

            ctx->engine->triggerScene(cond ? cfg.sceneTrue.c_str() : cfg.sceneFalse.c_str(), now);
        }

    private:
        RuleCfg cfg;
    };

    // --- BitmaskRule ---
    class BitmaskRule {
    public:
        BitmaskRule(const RuleCfg& cfg) : cfg(cfg) {}

        static void callback(AutomationEngine::SchedulerContext* ctx,
                             unsigned long now,
                             void* userCtx)
        {
            static_cast<BitmaskRule*>(userCtx)->run(ctx, now);
        }

        void run(AutomationEngine::SchedulerContext* ctx, unsigned long now) {
            auto& buffer = *ctx->buffer;

            long bits = buffer.getValueFast(cfg.bitmask.area);
            bool cond = bitRead(bits, cfg.bitmask.bitIndex);

            ctx->engine->triggerScene(cond ? cfg.sceneTrue.c_str() : cfg.sceneFalse.c_str(), now);
        }

    private:
        RuleCfg cfg;
    };

    // --- MultiRule ---
    class MultiRule {
    public:
        MultiRule(const RuleCfg& cfg) : cfg(cfg) {}

        static void callback(AutomationEngine::SchedulerContext* ctx,
                             unsigned long now,
                             void* userCtx)
        {
            static_cast<MultiRule*>(userCtx)->run(ctx, now);
        }

        void run(AutomationEngine::SchedulerContext* ctx, unsigned long now) {
            auto& buffer = *ctx->buffer;

            bool result = (cfg.multi.logic == "AND");

            for (auto& c : cfg.multi.conditions) {
                long v = buffer.getValueFast(c.area);

                bool cond = false;
                if (c.op == ">")  cond = v >  c.threshold;
                if (c.op == "<")  cond = v <  c.threshold;
                if (c.op == ">=") cond = v >= c.threshold;
                if (c.op == "<=") cond = v <= c.threshold;

                if (cfg.multi.logic == "AND") result &= cond;
                if (cfg.multi.logic == "OR")  result |= cond;
            }

            ctx->engine->triggerScene(result ? cfg.sceneTrue.c_str() : cfg.sceneFalse.c_str(), now);
        }

    private:
        RuleCfg cfg;
    };

    // --- TimeRule ---
    class TimeRule {
    public:
        TimeRule(const RuleCfg& cfg) : cfg(cfg) {}

        static void callback(AutomationEngine::SchedulerContext* ctx,
                             unsigned long now,
                             void* userCtx)
        {
            static_cast<TimeRule*>(userCtx)->run(ctx, now);
        }

        void run(AutomationEngine::SchedulerContext* ctx, unsigned long now) {
            struct tm t;
            if (!ctx->time->getDateTime(t))
                return;

            int nowMin = t.tm_hour * 60 + t.tm_min;

            if (nowMin == cfg.threshold.threshold)
                ctx->engine->triggerScene(cfg.sceneTrue.c_str(), now);
        }

    private:
        RuleCfg cfg;
    };

    // --- SequenceRule ---
    class SequenceRule {
    public:
        SequenceRule(const RuleCfg& cfg) : cfg(cfg) {}

        static void callback(AutomationEngine::SchedulerContext* ctx,
                             unsigned long now,
                             void* userCtx)
        {
            static_cast<SequenceRule*>(userCtx)->run(ctx, now);
        }

        void run(AutomationEngine::SchedulerContext* ctx, unsigned long now) {
            if (!seq.isRunning())
                seq.start(now);

            seq.update(ctx->engine, now);
        }

    private:
        RuleCfg cfg;
        AutomationEngine::ActionSequence seq;
    };

    // ============================================================
    // 8B. NUOVO MOTORE COMPOSITE DINAMICO
    // ============================================================
    class CompositeInput {
    public:
        virtual bool evaluate(Buffer& buffer,
                            AverageCalculator& medie,
                            unsigned long now) = 0;

        virtual ~CompositeInput() {}
    };

    class DebounceInput : public CompositeInput {
        CompositeInputCfg cfg;
        FastDebounce deb;
        TOF tof;

    public:
        DebounceInput(const CompositeInputCfg& c)
            : cfg(c),
            deb(c.debounceMs),
            tof(c.tofMinutes, TimerBase::Minutes)
        {}

        bool evaluate(Buffer& buffer,
                    AverageCalculator&,
                    unsigned long now) override
        {
            long v = buffer.getValueFast(cfg.area);
            bool raw = deb.update(v > cfg.threshold);

            tof.Run(raw);
            return raw || tof.Q();
        }
    };

    class TrendInput : public CompositeInput {
        CompositeInputCfg cfg;
        TOF tof;

    public:
        TrendInput(const CompositeInputCfg& c)
            : cfg(c),
            tof(c.tofMinutes, TimerBase::Minutes)
        {}

        bool evaluate(Buffer& buffer,
                    AverageCalculator& medie,
                    unsigned long now) override
        {
            long v = buffer.getValueFast(cfg.area, cfg.scale);

            medie.addMeasurement(cfg.name, v);
            auto trend = medie.groupTrend(cfg.name);

            bool raw =
                (cfg.trend == "rising"  && trend == Group::INCREASING && v > cfg.threshold) ||
                (cfg.trend == "falling" && trend == Group::DECREASING && v < cfg.threshold);

            tof.Run(raw);
            return raw || tof.Q();
        }
    };

    class BitmaskInput : public CompositeInput {
        CompositeInputCfg cfg;

    public:
        BitmaskInput(const CompositeInputCfg& c) : cfg(c) {}

        bool evaluate(Buffer& buffer,
                    AverageCalculator&,
                    unsigned long now) override
        {
            long bits = buffer.getValueFast(cfg.area);
            return bitRead(bits, cfg.bitIndex);
        }
    };

    static CompositeInput* createInput(const CompositeInputCfg& cfg) {
        // 1. Input semplice: TRUE se area != 0
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

        // 5. Futuri moduli (threshold, hysteresis, ecc.)
        // if (cfg.type == "threshold") return new ThresholdInput(cfg);

        LOG_WF("CompositeRule", "Tipo input '%s' NON supportato", cfg.type.c_str());
        return nullptr;
    }


    class CompositeRule {
    public:
        CompositeRule(const RuleCfg& cfg)
            : cfg(cfg)
        {
            for (auto& ic : cfg.composite.inputs) {
                CompositeInput* in = createInput(ic);
                if (in) inputs.push_back(in);
            }
        }

        static void callback(AutomationEngine::SchedulerContext* ctx,
                            unsigned long now,
                            void* userCtx)
        {
            static_cast<CompositeRule*>(userCtx)->run(ctx, now);
        }

        void run(AutomationEngine::SchedulerContext* ctx, unsigned long now) {
            auto& buffer = *ctx->buffer;
            auto& medie  = *ctx->medie;

            bool result = (cfg.composite.logic == "AND");

            for (auto* in : inputs) {
                bool v = in->evaluate(buffer, medie, now);

                if (cfg.composite.logic == "AND") result &= v;
                if (cfg.composite.logic == "OR")  result |= v;
            }

            // Scrivi output bitmask
            long bits = buffer.getValueFast(cfg.composite.output.area);
            bitWrite(bits, cfg.composite.output.bitIndex, result);
            buffer.WriteElement(cfg.composite.output.area, Field, bits, now);

            // Attiva scena
            ctx->engine->triggerScene(
                result ? cfg.sceneTrue.c_str() : cfg.sceneFalse.c_str(),
                now
            );
        }

    private:
        RuleCfg cfg;
        std::vector<CompositeInput*> inputs;
    };

    class SimpleInput : public CompositeInput {
        int area;

    public:
        SimpleInput(int a) : area(a) {}

        bool evaluate(Buffer& buffer,
                    AverageCalculator&,
                    unsigned long now) override
        {
            return buffer.getValueFast(area) != 0;
        }
    };

public:

    // ============================================================
    // DUMP CONFIGURAZIONE AUTOMAZIONI
    // ============================================================
    static void ReportAutomationConfig(const AutomationConfig& cfg) {
        Serial.println("\n===== AUTOMATION CONFIG DUMP =====");

        // ------------------------------------------------------------
        // 1. SCENE
        // ------------------------------------------------------------
        Serial.print("\nScenes: ");
        Serial.println(cfg.scenes.size());

        for (const auto& s : cfg.scenes) {
            Serial.println("------------------------------");
            Serial.print("Scene: ");
            Serial.println(s.name);

            if (s.actions.empty()) {
                Serial.println(" - WARNING: Scene has no actions");
                continue;
            }

            for (const auto& a : s.actions) {
                Serial.print("   Action → area=");
                Serial.print(a.area);
                Serial.print("  value=");
                Serial.println(a.value);
            }
        }

        // ------------------------------------------------------------
        // 2. RULES
        // ------------------------------------------------------------
        Serial.print("\nRules: ");
        Serial.println(cfg.rules.size());

        for (const auto& r : cfg.rules) {
            Serial.println("------------------------------");
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
            if (r.type == "threshold") {
                Serial.print("  Threshold area=");
                Serial.print(r.threshold.area);
                Serial.print(" op=");
                Serial.print(r.threshold.op);
                Serial.print(" thr=");
                Serial.println(r.threshold.threshold);
            }

            // trend
            if (r.type == "trend") {
                Serial.print("  Trend area=");
                Serial.print(r.trend.area);
                Serial.print(" scale=");
                Serial.print(r.trend.scale);
                Serial.print(" thr=");
                Serial.print(r.trend.threshold);
                Serial.print(" trend=");
                Serial.println(r.trend.trend);
            }

            // bitmask
            if (r.type == "bitmask") {
                Serial.print("  Bitmask area=");
                Serial.print(r.bitmask.area);
                Serial.print(" bit=");
                Serial.println(r.bitmask.bitIndex);
            }

            // multi
            if (r.type == "multi") {
                Serial.print("  Multi logic=");
                Serial.println(r.multi.logic);

                for (const auto& c : r.multi.conditions) {
                    Serial.print("    Cond area=");
                    Serial.print(c.area);
                    Serial.print(" op=");
                    Serial.print(c.op);
                    Serial.print(" thr=");
                    Serial.println(c.threshold);
                }
            }
            
        }

        // ------------------------------------------------------------
        // 5. SEQUENCES
        // ------------------------------------------------------------
        Serial.print("\nSequences: ");
        Serial.println(cfg.sequences.size());

        for (const auto& s : cfg.sequences) {
            Serial.println("------------------------------");
            Serial.print("Interval: ");
            Serial.println(s.intervalMs);

            for (const auto& st : s.steps) {
                Serial.print("  Step area=");
                Serial.print(st.area);
                Serial.print(" value=");
                Serial.print(st.value);
                Serial.print(" delay=");
                Serial.println(st.delayMs);
            }
        }

        Serial.println("\n===== END AUTOMATION CONFIG DUMP =====\n");
    }

};

bool AutomationBuilder::parseJson(const char* json, AutomationConfig& out)
{
    if (!json || json[0] == '\0') {
        LOG_EF("Automation", "JSON vuoto");
        return false;
    }

    StaticJsonDocument<8192> doc;
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        LOG_EF("Automation", "Errore parsing JSON: %s", err.c_str());
        return false;
    }

    // ------------------------------------------------------------
    // SCENES
    // ------------------------------------------------------------
    if (doc.containsKey("scenes")) {
        for (JsonVariant v : doc["scenes"].as<JsonArray>()) {
            SceneCfg sc;
            sc.name = v["name"] | "";

            for (JsonVariant a : v["actions"].as<JsonArray>()) {
                SceneActionCfg act;

                const char* areaName = a["area"] | "";
                act.area = resolveAreaOrLog(sc.name.c_str(), areaName);

                if (act.area < 0)
                    continue;

                act.value = a["value"] | 0;
                sc.actions.push_back(act);
            }

            if (sc.actions.empty()) {
                LOG_WF("AutomationBuilder",
                    "Scene '%s' ignorata: nessuna action valida",
                    sc.name.c_str());
                continue;
            }

            out.scenes.push_back(sc);
        }
    }

    // ------------------------------------------------------------
    // RULES
    // ------------------------------------------------------------
    if (doc.containsKey("rules")) {
        for (JsonVariant v : doc["rules"].as<JsonArray>()) {
            RuleCfg rc;
            rc.name       = v["name"] | "";
            rc.type       = v["type"] | "";
            rc.intervalMs = v["intervalMs"] | 1000;

            // threshold
            if (v.containsKey("threshold")) {
                rc.threshold.area = AreaRegistry::resolve(v["threshold"]["area"] | "");
                rc.threshold.op        = v["threshold"]["op"] | "";
                rc.threshold.threshold = v["threshold"]["threshold"] | 0;
            }

            // trend
            if (v.containsKey("trend")) {
                rc.trend.area = AreaRegistry::resolve(v["trend"]["area"] | "");
                rc.trend.scale     = v["trend"]["scale"] | 1;
                rc.trend.threshold = v["trend"]["threshold"] | 0;
                rc.trend.trend     = v["trend"]["trend"] | "";
            }

            // bitmask
            if (v.containsKey("bitmask")) {
                rc.bitmask.area = AreaRegistry::resolve(v["bitmask"]["area"] | "");
                rc.bitmask.bitIndex = v["bitmask"]["bitIndex"] | 0;
            }

            // multi
            if (v.containsKey("multi")) {
                rc.multi.logic = v["multi"]["logic"] | "AND";
                for (JsonVariant c : v["multi"]["conditions"].as<JsonArray>()) {
                    ConditionCfg cc;
                    cc.area = AreaRegistry::resolve(c["area"] | "");

                    cc.op        = c["op"] | "";
                    cc.threshold = c["threshold"] | 0;
                    rc.multi.conditions.push_back(cc);
                }
            }

            // ------------------------------------------------------------
            // COMPOSITE (nuovo formato dinamico)
            // ------------------------------------------------------------
            if (v.containsKey("composite")) {

                JsonObject comp = v["composite"];

                // LOGICA (AND / OR)
                rc.composite.logic = comp["logic"] | "OR";

                // -------------------------
                // INPUTS DINAMICI
                // -------------------------
                if (comp.containsKey("inputs")) {
                    for (JsonVariant in : comp["inputs"].as<JsonArray>()) {

                        CompositeInputCfg ic;

                        ic.type = in["type"] | "";
                        ic.name = in["name"] | "";

                        const char* areaName = in["area"] | "";
                        ic.area = resolveAreaOrLog(rc.name.c_str(), areaName);

                        // debounce
                        ic.threshold   = in["threshold"]   | 0;
                        ic.debounceMs  = in["debounceMs"]  | 0;

                        // trend
                        ic.scale       = in["scale"]       | 1;
                        ic.trend       = in["trend"]       | "";

                        // bitmask
                        ic.bitIndex    = in["bitIndex"]    | -1;

                        // TOF
                        ic.tofMinutes  = in["tofMinutes"]  | 0;

                        rc.composite.inputs.push_back(ic);
                    }
                }

                // -------------------------
                // OUTPUT
                //-------------------------
                if (comp.containsKey("output")) {
                    JsonObject out = comp["output"];

                    const char* areaName = out["area"] | "";
                    rc.composite.output.area = resolveAreaOrLog(rc.name.c_str(), areaName);

                    rc.composite.output.bitIndex = out["bitIndex"] | -1;
                }
            }

            rc.sceneTrue  = v["sceneTrue"] | "";
            rc.sceneFalse = v["sceneFalse"] | "";

            // ------------------------------------------------------------
            // VALIDAZIONE AREE (PUNTO ESATTO DOVE VA INSERITA)
            // ------------------------------------------------------------
            if (rc.type == "composite") {
                // almeno 1 input
                if (rc.composite.inputs.empty()) {
                    LOG_EF("AutomationBuilder",
                        "Regola composite '%s' ignorata: composite senza inputs",
                        rc.name.c_str());
                    continue;
                }

                // validazione aree input
                bool invalid = false;
                for (auto& ic : rc.composite.inputs) {
                    if (ic.area < 0) invalid = true;
                }

                if (invalid) {
                    LOG_EF("AutomationBuilder",
                        "Regola composite '%s' ignorata: composite con area input non valida",
                        rc.name.c_str());
                    continue;
                }

                // validazione output
                if (rc.composite.output.area < 0) {
                    LOG_EF("AutomationBuilder",
                        "Regola composite '%s' ignorata: composite output area non valida",
                        rc.name.c_str());
                    continue;
                }
            }

            // (opzionale) validazione generale
            if (rc.type == "threshold" && rc.threshold.area < 0) continue;
            if (rc.type == "trend"     && rc.trend.area < 0) continue;
            if (rc.type == "bitmask"   && rc.bitmask.area < 0) continue;

            // ------------------------------------------------------------
            // VALIDAZIONE SCENE TRUE/FALSE (con supporto NoAction)
            // ------------------------------------------------------------
            bool sceneTrueExists = (rc.sceneTrue == AutomationEngine::BuiltinScenes::toString( AutomationEngine::BuiltinScenes::Type::NoAction ) );
            bool sceneFalseExists = (rc.sceneFalse == AutomationEngine::BuiltinScenes::toString( AutomationEngine::BuiltinScenes::Type::NoAction ) );

            // Controlla sceneTrue se non è NoAction
            if (!sceneTrueExists) {
                for (const auto& s : out.scenes)
                    if (s.name == rc.sceneTrue)
                        sceneTrueExists = true;
            }

            // Controlla sceneFalse se non è NoAction
            if (!sceneFalseExists) {
                for (const auto& s : out.scenes)
                    if (s.name == rc.sceneFalse)
                        sceneFalseExists = true;
            }

            if (!sceneTrueExists) {
                LOG_EF("AutomationBuilder",
                    "Regola scene '%s' ignorata: sceneTrue '%s' non esiste",
                    rc.name.c_str(),
                    rc.sceneTrue.c_str());
                continue;
            }

            if (!sceneFalseExists) {
                LOG_EF("AutomationBuilder",
                    "Regola scene '%s' ignorata: sceneFalse '%s' non esiste",
                    rc.name.c_str(),
                    rc.sceneFalse.c_str());
                continue;
            }


            out.rules.push_back(rc);
        }
    }

    // ------------------------------------------------------------
    // SEQUENCES
    // ------------------------------------------------------------
    if (doc.containsKey("sequences")) {
        for (JsonVariant v : doc["sequences"].as<JsonArray>()) {
            SequenceCfg sc;
            sc.intervalMs = v["intervalMs"] | 1000;

            for (JsonVariant step : v["steps"].as<JsonArray>()) {
                SequenceStepCfg st;

                // PATCH: risoluzione automatica dell'area
                String key = step["area"] | "";
                st.area = AreaRegistry::resolve(key);

                st.value   = step["value"] | 0;
                st.delayMs = step["delayMs"] | 0;

                sc.steps.push_back(st);
            }

            out.sequences.push_back(sc);
        }
    }

    LOG_D("Automation", "JSON automazioni caricato correttamente");
    return true;
}

#endif

