#ifndef DMINTERPRETER_HPP
#define DMINTERPRETER_HPP

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


// Single-file header-only JSON-driven interpreter for AsyncScheduler.
// Integrates with DomoManager, AEERegistry, BridgeEngine and project APIs.
// - No filesystem dependency: load JSON from const char*, Stream, or PROGMEM.
// - Extensible: register primitives, conditions and onComplete callbacks.
// - Stateful primitives allocate a small state buffer automatically.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <string>
#include <functional>
#include <vector>
#include <memory>

#include "DM.hpp"
#include "DMAEE.hpp"
#include "DMBridge.hpp"
#include "DMWebAPI.hpp"
#include "DMLogger.hpp"
#include "DMMQTTEngine.hpp"

// If GenericSensor is defined in dmplc.h (as you indicated), do not redefine it here.
// This header uses GenericSensor if available.

class DmInterpreter {
public:
    // Primitive callable signature: returns true when step completed.
    using PrimitiveFn = std::function<bool(DomoManager* dm, void* state)>;
    using PrimitiveFactory = std::function<PrimitiveFn(const JsonObject& args, DomoManager* dm, AEERegistry* aee)>;

    // Condition callable signature: returns boolean (for branch/skip)
    using ConditionFn = std::function<bool(DomoManager* dm)>;
    using ConditionFactory = std::function<ConditionFn(const JsonObject& args, DomoManager* dm, AEERegistry* aee)>;

    // onComplete callback
    using OnCompleteFn = std::function<void(DomoManager* dm)>;

    // Public API - registration
    static void registerPrimitive(const char* name, PrimitiveFactory factory) {
        s_primitiveFactories[String(name)] = factory;
    }
    static void registerCondition(const char* name, ConditionFactory factory) {
        s_conditionFactories[String(name)] = factory;
    }
    static void registerOnComplete(const char* name, OnCompleteFn cb) {
        s_onComplete[String(name)] = cb;
    }

    // Register a set of default primitives and example conditions/onComplete.
    // Call after DomoManager and AEERegistry are available.
    static void registerDefaults(DomoManager* dm, AEERegistry* aee) {
        // WRITE_AREA
        registerPrimitive("WRITE_AREA", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            int area = args["area"] | -1;
            int value = args["value"] | 0;
            return [area, value](DomoManager* dm, void* state)->bool {
                if (!dm) return true;
                if (area < 0) return true;
                dm->getBuffer().WriteElement(area, ToPanel, value, dm->getTimeManager().nowMs());
                LOG_DF("DmInterpreter","WRITE_AREA area=%d value=%d", area, value);
                return true;
            };
        });

        // LOG
        registerPrimitive("LOG", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            const char* msg = args["msg"] | "";
            String copy = msg;
            return [copy](DomoManager* dm, void* state)->bool {
                LOG_IF("DmInterpreter","%s", copy.c_str());
                return true;
            };
        });

        // VAR_SET
        registerPrimitive("VAR_SET", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            const char* var = args["var"] | nullptr;
            JsonVariant v = args["value"];
            String varCopy = var ? String(var) : String();
            return [varCopy, v](DomoManager* dm, void* state)->bool {
                if (v.is<int>()) JobVarStore::set(varCopy, JobVar::fromInt(v.as<int>()));
                else if (v.is<double>()) JobVarStore::set(varCopy, JobVar::fromFloat((float)v.as<double>()));
                else if (v.is<bool>()) JobVarStore::set(varCopy, JobVar::fromBool(v.as<bool>()));
                else if (v.is<const char*>()) JobVarStore::set(varCopy, JobVar::fromString(String(v.as<const char*>())));
                return true;
            };
        });

        // VAR_GET -> write to buffer
        registerPrimitive("VAR_GET", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            const char* var = args["var"] | nullptr;
            int targetArea = args["targetArea"] | -1;
            String varCopy = var ? String(var) : String();
            return [varCopy, targetArea](DomoManager* dm, void* state)->bool {
                if (!dm) return true;
                if (!JobVarStore::exists(varCopy)) return true;
                JobVar v = JobVarStore::get(varCopy);
                if (targetArea < 0) return true;
                if (v.isInt()) dm->getBuffer().WriteElement(targetArea, ToPanel, (int)v.iVal, dm->getTimeManager().nowMs());
                else if (v.isFloat()) dm->getBuffer().WriteElement(targetArea, ToPanel, (int)v.fVal, dm->getTimeManager().nowMs());
                else if (v.isBool()) dm->getBuffer().WriteElement(targetArea, ToPanel, v.bVal ? 1 : 0, dm->getTimeManager().nowMs());
                else if (v.isString()) dm->getBuffer().WriteElement(targetArea, ToPanel, v.sVal.toInt(), dm->getTimeManager().nowMs());
                return true;
            };
        });

        // WAIT_MS (stateful)
        registerPrimitive("WAIT_MS", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            unsigned long ms = args["ms"] | 0;
            return [ms](DomoManager* dm, void* state)->bool {
                if (!state) return true;
                unsigned long* p = static_cast<unsigned long*>(state);
                if (*p == 0) *p = millis();
                if (millis() - *p >= ms) { *p = 0; return true; }
                return false;
            };
        });

        // WAIT_COND (stateful) - uses registered condition factories
        registerPrimitive("WAIT_COND", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            const char* condName = args["cond"] | nullptr;
            unsigned long timeout = args["timeoutMs"] | 0;
            String condCopy = condName ? String(condName) : String();
            return [condCopy, timeout](DomoManager* dm, void* state)->bool {
                if (!state) return true;
                unsigned long* p = static_cast<unsigned long*>(state);
                if (*p == 0) *p = millis();
                auto it = s_conditionFactories.find(condCopy);
                if (it != s_conditionFactories.end()) {
                    auto condFn = it->second(JsonObject(), dm, nullptr);
                    if (condFn(dm)) { *p = 0; return true; }
                } else {
                    LOG_WF("DmInterpreter","WAIT_COND: condition not found: %s", condCopy.c_str());
                    *p = 0;
                    return true;
                }
                if (timeout > 0 && (millis() - *p >= timeout)) { LOG_WF("DmInterpreter","WAIT_COND timeout %s", condCopy.c_str()); *p = 0; return true; }
                return false;
            };
        });

        // AEE_READ
        registerPrimitive("AEE_READ", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            const char* name = args["name"] | nullptr;
            const char* var = args["var"] | nullptr;
            String nameCopy = name ? String(name) : String();
            String varCopy = var ? String(var) : String();
            return [nameCopy, varCopy, a](DomoManager* dm, void* state)->bool {
                if (!a) { LOG_WF("DmInterpreter","AEE_READ: registry null"); return true; }
                AEEVariableBase* v = a->find(nameCopy);
                if (!v) { LOG_WF("DmInterpreter","AEE_READ: not found %s", nameCopy.c_str()); return true; }
                switch (v->def.varType) {
                    case AEEVarType::FLOAT: if (auto* vf = as<float>(v)) JobVarStore::set(varCopy, JobVar::fromFloat(vf->get())); break;
                    case AEEVarType::INT:   if (auto* vi = as<int>(v)) JobVarStore::set(varCopy, JobVar::fromInt(vi->get())); break;
                    case AEEVarType::BOOL:  if (auto* vb = as<bool>(v)) JobVarStore::set(varCopy, JobVar::fromBool(vb->get())); break;
                    case AEEVarType::STRING:if (auto* vs = as<String>(v)) JobVarStore::set(varCopy, JobVar::fromString(vs->get())); break;
                    default: break;
                }
                return true;
            };
        });

        // AEE_WRITE
        registerPrimitive("AEE_WRITE", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            const char* name = args["name"] | nullptr;
            JsonVariant val = args["value"];
            String nameCopy = name ? String(name) : String();
            return [nameCopy, val, a](DomoManager* dm, void* state)->bool {
                if (!a) { LOG_WF("DmInterpreter","AEE_WRITE: registry null"); return true; }
                AEEVariableBase* base = a->find(nameCopy);
                if (!base) { LOG_WF("DmInterpreter","AEE_WRITE: not found %s", nameCopy.c_str()); return true; }
                switch (base->def.varType) {
                    case AEEVarType::FLOAT: if (auto* vf = as<float>(base)) vf->set((float)val.as<double>()); break;
                    case AEEVarType::INT:   if (auto* vi = as<int>(base)) vi->set((int)val.as<int>()); break;
                    case AEEVarType::BOOL:  if (auto* vb = as<bool>(base)) vb->set(val.as<bool>()); break;
                    case AEEVarType::STRING:if (auto* vs = as<String>(base)) vs->set(String(val.as<const char*>())); break;
                    default: break;
                }
                return true;
            };
        });

        // AEE_SUBSCRIBE (stateful)
        registerPrimitive("AEE_SUBSCRIBE", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            const char* name = args["name"] | nullptr;
            JsonVariant match = args["matchValue"];
            unsigned long timeout = args["timeoutMs"] | 0;
            String nameCopy = name ? String(name) : String();
            return [nameCopy, match, timeout, a](DomoManager* dm, void* state)->bool {
                if (!a) { LOG_WF("DmInterpreter","AEE_SUBSCRIBE: registry null"); return true; }
                unsigned long* p = static_cast<unsigned long*>(state);
                if (!p) return true;
                if (*p == 0) *p = millis();
                AEEVariableBase* base = a->find(nameCopy);
                if (!base) { LOG_WF("DmInterpreter","AEE_SUBSCRIBE: not found %s", nameCopy.c_str()); *p = 0; return true; }
                bool matched = false;
                switch (base->def.varType) {
                    case AEEVarType::FLOAT: if (auto* vf = as<float>(base)) if (!match.isNull()) matched = (vf->get() == (float)match.as<double>()); break;
                    case AEEVarType::INT:   if (auto* vi = as<int>(base)) if (!match.isNull()) matched = (vi->get() == match.as<int>()); break;
                    case AEEVarType::BOOL:  if (auto* vb = as<bool>(base)) if (!match.isNull()) matched = (vb->get() == match.as<bool>()); break;
                    case AEEVarType::STRING:if (auto* vs = as<String>(base)) if (!match.isNull()) matched = (vs->get() == String(match.as<const char*>())); break;
                    default: break;
                }
                if (matched) { *p = 0; return true; }
                if (timeout > 0 && (millis() - *p >= timeout)) { LOG_WF("DmInterpreter","AEE_SUBSCRIBE timeout %s", nameCopy.c_str()); *p = 0; return true; }
                return false;
            };
        });

        // BRIDGE_SEND
        registerPrimitive("BRIDGE_SEND", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            const char* payload = args["payload"] | nullptr;
            String payloadCopy = payload ? String(payload) : String();
            return [payloadCopy](DomoManager* dm, void* state)->bool {
                BridgeEngine::SendRaw(payloadCopy);
                LOG_DF("DmInterpreter","BRIDGE_SEND payload=%s", payloadCopy.c_str());
                return true;
            };
        });

        // Example condition: isHot
        registerCondition("isHot", [=](const JsonObject& args, DomoManager* d, AEERegistry* a){
            return [d](DomoManager* dm)->bool {
                BufferSourceInfo info;
                if (dm->getBuffer().GetData(33, Field, info)) return info.value > 35;
                return analogRead(A0) > 600;
            };
        });

        // Example onComplete
        registerOnComplete("jobDone", [=](DomoManager* dm){
            LOG_IF("DmInterpreter","Job completed (onComplete)");
        });
    }

    // Load JSON from const char*
    static bool loadFromString(const char* json, AsyncScheduler& scheduler, DomoManager* dm, AEERegistry* aee) {
        if (!json) return false;
        size_t needed = measureJson(json) + 1024;
        DynamicJsonDocument doc(needed);
        auto err = deserializeJson(doc, json);
        if (err) {
            LOG_WF("DmInterpreter","JSON parse error: %s", err.c_str());
            return false;
        }
        return buildJobsFromJson(doc, scheduler, dm, aee);
    }

    // Load JSON from Stream (Serial, client, etc.)
    static bool loadFromStream(Stream& s, AsyncScheduler& scheduler, DomoManager* dm, AEERegistry* aee, size_t maxDocSize = 24*1024) {
        std::vector<char> buf;
        while (s.available()) {
            buf.push_back((char)s.read());
            if (buf.size() >= maxDocSize) break;
        }
        if (buf.empty()) {
            LOG_WF("DmInterpreter","Stream empty");
            return false;
        }
        buf.push_back('\0');
        DynamicJsonDocument doc(maxDocSize);
        auto err = deserializeJson(doc, buf.data());
        if (err) {
            LOG_WF("DmInterpreter","JSON parse error: %s", err.c_str());
            return false;
        }
        return buildJobsFromJson(doc, scheduler, dm, aee);
    }

    // Load JSON stored in PROGMEM
    static bool loadFromProgmem(const char* progmemJson, AsyncScheduler& scheduler, DomoManager* dm, AEERegistry* aee) {
#if defined(ARDUINO_ARCH_AVR) || defined(ESP8266) || defined(ESP32)
        size_t len = strlen_P(progmemJson);
        std::unique_ptr<char[]> buf(new char[len + 1]);
        strcpy_P(buf.get(), progmemJson);
        return loadFromString(buf.get(), scheduler, dm, aee);
#else
        return loadFromString(progmemJson, scheduler, dm, aee);
#endif
    }

    // Build jobs from parsed JSON document
    static bool buildJobsFromJson(DynamicJsonDocument& doc, AsyncScheduler& scheduler, DomoManager* dm, AEERegistry* aee) {
        JsonArray jobs = doc["jobs"].as<JsonArray>();
        if (!jobs) {
            LOG_WF("DmInterpreter","No jobs array in JSON");
            return false;
        }

        // clear previous instances and vars
        for (auto &kv : s_instances) {
            freeStepInstanceState(kv.second);
        }
        s_instances.clear();
        JobVarStore::clear();

        for (JsonObject jobJ : jobs) {
            AsyncScheduler::Job job;
            job.priority = jobJ["priority"] | 0;

            // onComplete mapping (store name in job.name for later dispatch if needed)
            const char* onCompleteName = jobJ["onComplete"] | nullptr;
            if (onCompleteName) {
                String oc = onCompleteName;
                // store as job.name suffix (not ideal but allows later lookup)
                // AsyncScheduler::Job::onComplete is C-style pointer; we leave placeholder.
                // If user wants onComplete invoked, they can register a primitive as last step.
                (void)oc;
            }

            JsonArray steps = jobJ["steps"].as<JsonArray>();
            for (JsonObject s : steps) {
                AsyncScheduler::Step step;
                const char* type = s["type"] | "normal";
                const char* humanDesc = s["description"] | "";
                step.delayAfterMs = s["delayAfterMs"] | 0;

                // create unique id and store it in step.description for mapping
                String id = makeUniqueId();
                step.description = id;

                if (strcmp(type, "branch") == 0) {
                    step.type = AsyncScheduler::BRANCH_STEP;
                    const char* condName = s["condition"] | nullptr;
                    if (condName) {
                        String condStr = condName;
                        auto it = s_conditionFactories.find(condStr);
                        if (it != s_conditionFactories.end()) {
                            JsonObject args = s["args"].as<JsonObject>();
                            ConditionFn condFn = it->second(args, dm, aee);
                            StepInstance inst;
                            inst.id = id;
                            inst.humanDesc = humanDesc;
                            inst.condition = condFn;
                            s_instances[id] = inst;
                            step.condition = &DmInterpreter::conditionDispatcher;
                        } else {
                            LOG_WF("DmInterpreter","Condition factory not found: %s", condName);
                            step.condition = nullptr;
                        }
                    } else {
                        step.condition = nullptr;
                    }
                    step.thenStep = s["then"] | -1;
                    step.elseStep = s["else"] | -1;
                } else {
                    // NORMAL_STEP / primitive
                    step.type = AsyncScheduler::NORMAL_STEP;
                    const char* fnName = s["fn"] | nullptr;
                    JsonObject args = s["args"].as<JsonObject>();

                    if (fnName) {
                        String fnStr = fnName;
                        auto it = s_primitiveFactories.find(fnStr);
                        if (it != s_primitiveFactories.end()) {
                            PrimitiveFn prim = it->second(args, dm, aee);

                            // allocate small state for stateful primitives heuristically
                            void* statePtr = nullptr;
                            size_t stateSize = 0;
                            if (fnStr.indexOf("WAIT") >= 0 || fnStr.indexOf("SUBSCRIBE") >= 0) {
                                stateSize = sizeof(unsigned long);
                                statePtr = calloc(1, stateSize);
                            }

                            StepInstance inst;
                            inst.id = id;
                            inst.humanDesc = humanDesc;
                            inst.primitive = prim;
                            inst.statePtr = statePtr;
                            inst.stateSize = stateSize;
                            {
                                String tmp; serializeJson(args, tmp);
                                inst.argsJson = tmp;
                            }
                            s_instances[id] = inst;

                            // set step.fnc to dispatcher (C-style pointer)
                            step.fnc = &DmInterpreter::stepDispatcher;

                            // skipIf optional
                            const char* skipName = s["skipIf"] | nullptr;
                            if (skipName) {
                                String skipStr = skipName;
                                auto it2 = s_conditionFactories.find(skipStr);
                                if (it2 != s_conditionFactories.end()) {
                                    ConditionFn skipFn = it2->second(JsonObject(), dm, aee);
                                    // store skip condition in same instance
                                    s_instances[id].condition = skipFn;
                                    step.skipIf = &DmInterpreter::conditionDispatcher;
                                } else {
                                    LOG_WF("DmInterpreter","skipIf not registered: %s", skipName);
                                }
                            }
                        } else {
                            LOG_WF("DmInterpreter","Primitive not registered: %s", fnName);
                            step.fnc = [](void* ctx)->bool { return true; };
                        }
                    } else {
                        // no fn -> noop
                        step.fnc = [](void* ctx)->bool { return true; };
                    }
                }

                job.steps.push_back(step);
            }

            const char* title = jobJ["title"] | "unnamed";
            scheduler.addJob(job, title);
            LOG_IF("DmInterpreter","Registered job: %s", title);
        }

        return true;
    }

private:
    // StepInstance holds runtime state and callables for a step
    struct StepInstance {
        String id;
        String humanDesc;
        PrimitiveFn primitive;
        ConditionFn condition;
        void* statePtr = nullptr;
        size_t stateSize = 0;
        String argsJson;
    };

    // Minimal variant storage for job-local variables
    class JobVar {
    public:
        enum class Type { NONE, INT, FLOAT, BOOL, STRING };
        Type type = Type::NONE;
        long iVal = 0;
        float fVal = 0.0f;
        bool bVal = false;
        String sVal;

        static JobVar fromInt(int v)    { JobVar x; x.type = Type::INT; x.iVal = v; return x; }
        static JobVar fromFloat(float v){ JobVar x; x.type = Type::FLOAT; x.fVal = v; return x; }
        static JobVar fromBool(bool v)  { JobVar x; x.type = Type::BOOL; x.bVal = v; return x; }
        static JobVar fromString(const String& v){ JobVar x; x.type = Type::STRING; x.sVal = v; return x; }

        bool isInt() const { return type == Type::INT; }
        bool isFloat() const { return type == Type::FLOAT; }
        bool isBool() const { return type == Type::BOOL; }
        bool isString() const { return type == Type::STRING; }
    };

    // JobVarStore (shared across interpreter)
    class JobVarStore {
    private:
        static inline std::map<String, JobVar> store;
    public:
        static void set(const String& k, const JobVar& v) { store[k] = v; }
        static bool exists(const String& k) { return store.find(k) != store.end(); }
        static JobVar get(const String& k) { return store[k]; }
        static void remove(const String& k) { store.erase(k); }
        static void clear() { store.clear(); }
    };

    // Registries and instances
    static inline std::map<String, PrimitiveFactory> s_primitiveFactories;
    static inline std::map<String, ConditionFactory> s_conditionFactories;
    static inline std::map<String, OnCompleteFn> s_onComplete;
    static inline std::map<String, StepInstance> s_instances;

    // Dispatcher functions (C-style) used by AsyncScheduler
    static bool stepDispatcher(void* ctx) {
        DomoManager* dm = static_cast<DomoManager*>(ctx);
        if (!dm) return true;
        StepInstance* inst = findInstanceForCurrentStep(dm);
        if (!inst) return true;
        bool done = true;
        if (inst->primitive) done = inst->primitive(dm, inst->statePtr);
        if (done) freeStepInstanceState(*inst);
        return done;
    }

    static bool conditionDispatcher(void* ctx) {
        DomoManager* dm = static_cast<DomoManager*>(ctx);
        if (!dm) return false;
        StepInstance* inst = findInstanceForCurrentStep(dm);
        if (!inst) return false;
        if (inst->condition) return inst->condition(dm);
        return false;
    }

    // Helpers
    static String makeUniqueId() {
        static unsigned long counter = 0;
        counter++;
        char buf[32];
        snprintf(buf, sizeof(buf), "DI#%lu", counter);
        return String(buf);
    }

    static StepInstance* findInstanceForCurrentStep(DomoManager* dm) {
        if (!dm) return nullptr;
        AsyncScheduler& sched = dm->getScheduler();
        const auto& jobs = sched.getJobs();
        for (size_t j = 0; j < jobs.size(); ++j) {
            const AsyncScheduler::Job& job = jobs[j];
            if (!job.active || job.cancelled) continue;
            int idx = job.currentStep;
            if (idx < 0 || idx >= (int)job.steps.size()) continue;
            const AsyncScheduler::Step& step = job.steps[idx];
            String id = step.description;
            if (id.length() == 0) continue;
            auto it = s_instances.find(id);
            if (it != s_instances.end()) return &it->second;
        }
        return nullptr;
    }

    static void freeStepInstanceState(StepInstance& inst) {
        if (inst.statePtr && inst.stateSize > 0) {
            free(inst.statePtr);
            inst.statePtr = nullptr;
            inst.stateSize = 0;
        }
    }

    // expose JobVarStore for external use if needed
public:
    // External accessors for job variables (optional)
    static void setVar(const String& k, int v) { JobVarStore::set(k, JobVar::fromInt(v)); }
    static void setVar(const String& k, float v) { JobVarStore::set(k, JobVar::fromFloat(v)); }
    static void setVar(const String& k, bool v) { JobVarStore::set(k, JobVar::fromBool(v)); }
    static void setVar(const String& k, const String& v) { JobVarStore::set(k, JobVar::fromString(v)); }
    static bool varExists(const String& k) { return JobVarStore::exists(k); }
    static JobVar varGet(const String& k) { return JobVarStore::get(k); }
};

#endif
