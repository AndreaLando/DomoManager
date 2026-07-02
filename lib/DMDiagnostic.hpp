#ifndef DMDiagnostic_HPP
#define DMDiagnostic_HPP

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

#include "DM.hpp"
 
        
class OwnerManagerDiagnostic {
public:
    /* ============================================================
       1. STATO GENERALE
       ============================================================ */
    static void ReportStatus(const OwnerManager& om, unsigned long now) {
        Serial.println("\n===== OWNER-MANAGER STATUS =====");

        Serial.print("Current mode: ");
        Serial.println(om.getModeName());

        Serial.print("Can modify settings: ");
        Serial.println(om.canModifySettings() ? "YES" : "NO");

        Serial.print("Can control all devices: ");
        Serial.println(om.canControlAllDevices() ? "YES" : "NO");

        Serial.print("Can access logs: ");
        Serial.println(om.canAccessLogs() ? "YES" : "NO");

        Serial.print("Can use automation: ");
        Serial.println(om.canUseAutomation() ? "YES" : "NO");

        if (strcmp(om.getModeName(), "GUEST") == 0) {
            Serial.print("Guest expires in: ");
            Serial.print(om.getGuestExpireAt() > now ? om.getGuestExpireAt() - now : 0);
            Serial.println(" ms");
        }
    }

    /* ============================================================
       2. IOT REQUESTS
       ============================================================ */
    static void ReportIOT(const OwnerManager& om) {
        Serial.println("\n===== OWNER-MANAGER IOT =====");

        Serial.print("Pending IOT Guest Request: ");
        Serial.println(om.hasPendingIOTRequest() ? "YES" : "NO");

        if (om.hasPendingIOTRequest()) {
            Serial.print("Requested duration: ");
            Serial.print(om.getPendingIOTDuration());
            Serial.println(" ms");
        }
    }

    /* ============================================================
       3. CALLBACK DIAGNOSTIC
       ============================================================ */
    static void ReportCallback(const OwnerManager& om) {
        Serial.println("\n===== OWNER-MANAGER CALLBACK =====");

        Serial.print("Callback registered: ");
        Serial.println(om.hasCallback() ? "YES" : "NO");
    }

    /* ============================================================
       4. FULL REPORT
       ============================================================ */
    static void FullReport(const OwnerManager& om, unsigned long now) {
        Serial.println("\n====================================");
        Serial.println("      OWNER-MANAGER FULL REPORT     ");
        Serial.println("====================================");

        ReportStatus(om, now);
        ReportIOT(om);
        ReportCallback(om);

        Serial.println("====================================\n");
    }
};

class HotStandbyDiagnostic {
public:

    /* ============================================================
       1. STATO GENERALE
       ============================================================ */
    static void ReportStatus(HotStandbyManager& hs) {
        Serial.println("\n===== HOT-STANDBY STATUS =====");

        Serial.print("Role: ");
        Serial.println(hs.isMaster ? "MASTER" : "SLAVE");

        Serial.print("Last heartbeat sent: ");
        Serial.print(hs.getLastHeartbeat());
        Serial.println(" ms ago");

        Serial.print("Last peer heartbeat: ");
        Serial.print(hs.getLastPeerHeartbeat());
        Serial.println(" ms ago");

        Serial.print("Heartbeat timeout: ");
        Serial.print(hs.getHeartbeatTimeout());
        Serial.println(" ms");

        Serial.print("StateSync interval: ");
        Serial.print(hs.getStateSyncInterval());
        Serial.println(" ms");

        Serial.print("ProcessData interval: ");
        Serial.print(hs.getProcessDataInterval());
        Serial.println(" ms");

        Serial.print("TimestampSync interval: ");
        Serial.print(hs.getTimestampSyncInterval());
        Serial.println(" ms");
    }

    /* ============================================================
       2. STATE SYNC
       ============================================================ */
    static void ReportStateSync(HotStandbyManager& hs) {
        Serial.println("\n===== HOT-STANDBY STATE SYNC =====");

        Serial.print("Last StateSync sent: ");
        Serial.print(hs.getLastStateSync());
        Serial.println(" ms ago");

        Serial.print("StateSync size: ");
        Serial.print(hs.getStateDataLen());
        Serial.println(" bytes");

        if (hs.getStateDataLen() == 0)
            Serial.println("WARNING: No state data being replicated.");
    }

    /* ============================================================
       3. PROCESS DATA
       ============================================================ */
    static void ReportProcessData(HotStandbyManager& hs) {
        Serial.println("\n===== HOT-STANDBY PROCESS DATA =====");

        Serial.print("Last ProcessData sent: ");
        Serial.print(hs.getLastProcessDataSync());
        Serial.println(" ms ago");

        Serial.print("ProcessData size: ");
        Serial.print(hs.getProcessDataLen());
        Serial.println(" bytes");

        if (hs.getProcessDataLen() == 0)
            Serial.println("WARNING: No process data being replicated.");
    }

    /* ============================================================
       4. TIMESTAMP SYNC
       ============================================================ */
    static void ReportTimestampSync(HotStandbyManager& hs) {
        Serial.println("\n===== HOT-STANDBY TIMESTAMP SYNC =====");

        Serial.print("Last TimestampSync: ");
        Serial.print(hs.getLastTimestampSync());
        Serial.println(" ms ago");

        Serial.print("Epoch offset: ");
        Serial.println(hs.getEpochOffset());
    }

    /* ============================================================
       5. RS485 DIAGNOSTIC
       ============================================================ */
    static void ReportRS485(HotStandbyManager& hs) {
        Serial.println("\n===== HOT-STANDBY RS485 =====");

        Serial.print("Is master: ");
        Serial.println(hs.isMaster ? "YES" : "NO");

        Serial.print("Awaiting ACK: ");
        Serial.println(hs.getNode().isAwaitingAck() ? "YES" : "NO");

        Serial.print("Retry count: ");
        Serial.println(hs.getNode().getRetryCount());

        Serial.print("Last send time: ");
        Serial.print(hs.getNode().getLastSendTime());
        Serial.println(" ms ago");
    }

    /* ============================================================
       6. FULL REPORT
       ============================================================ */
    static void FullReport(HotStandbyManager& hs) {
        #if HOTSTANDBY_ENABLED
            ReportStatus(hs);
            ReportStateSync(hs);
            ReportProcessData(hs);
            ReportTimestampSync(hs);
            ReportRS485(hs);
        #else
            Serial.println("Hot-standby disabilitato (HOTSTANDBY_ENABLED=0)");
        #endif
        
    }
};

class Diagnostic {
public:
    // ============================================================
    // CALLBACK FRONTEND DIAGNOSTIC
    // ============================================================
    static std::function<void()>& frontendDiagnosticCallback() {
        static std::function<void()> cb = nullptr;
        return cb;
    }

    static void RunFrontendDiagnostics() {
        auto& cb = frontendDiagnosticCallback();
        if (cb) cb();
    }

    static void ReportAutomationEngine(DomoManager& manager, unsigned long now ) {
        Serial.println("\n===== AUTOMATION ENGINE DIAGNOSTIC =====");

        AutomationEngine& engine = manager.getAutomation();
        
        // ---------------------------------------------------------
        // 1. SCENE (solo informazioni, niente warning qui)
        // ---------------------------------------------------------
        Serial.print("Total scenes: ");
        Serial.println(engine.getSceneCount());

        for (uint8_t i = 0; i < engine.getSceneCount(); i++) {
            auto* s = engine.getScenePtr(i);

            Serial.println("------------------------------");
            Serial.print("Scene #"); Serial.print(i);
            Serial.print("  Name: "); Serial.println(s->name);

            Serial.print("Actions: ");
            Serial.println(s->actionCount);

            bool referenced = engine.isSceneReferenced(s->name);
            Serial.print("Referenced: ");
            Serial.println(referenced ? "YES" : "NO");
        }

        // ---------------------------------------------------------
        // 2. RULES
        // ---------------------------------------------------------
        Serial.print("\nTotal rules: ");
        Serial.println(engine.getRuleCount());

        for (uint8_t i = 0; i < engine.getRuleCount(); i++) {
            auto& r = engine.getRule(i);

            Serial.println("------------------------------");
            Serial.print("Rule #"); Serial.println(i);

            Serial.print("Conditions: ");
            Serial.println(r.conditionCount);

            Serial.print("Then actions: ");
            Serial.println(r.actionThenCount);

            Serial.print("Else actions: ");
            Serial.println(r.actionElseCount);

            Serial.print("Logic: ");
            Serial.println(r.useAndLogic ? "AND" : "OR");

            Serial.print("Last result: ");
            if (!r.hasLastResult) Serial.println("N/A");
            else Serial.println(r.lastResult ? "TRUE" : "FALSE");
        }

        // ---------------------------------------------------------
        // 3. SCHEDULED RULES
        // ---------------------------------------------------------
        Serial.print("\nScheduled rules: ");
        Serial.println(engine.getScheduledCount());

        for (uint8_t i = 0; i < engine.getScheduledCount(); i++) {
            auto* sr = engine.getScheduledPtr(i);

            Serial.println("------------------------------");
            Serial.print("Scheduled #"); Serial.println(i);

            Serial.print("Type: ");
            switch (sr->type) {
                case AutomationEngine::ScheduledRule::EVERY_INTERVAL: Serial.println("EVERY_INTERVAL"); break;
                case AutomationEngine::ScheduledRule::EVERY_DAY_AT: Serial.println("EVERY_DAY_AT"); break;
                case AutomationEngine::ScheduledRule::SEQUENCE_AT_TIME: Serial.println("SEQUENCE_AT_TIME"); break;
                case AutomationEngine::ScheduledRule::SEQUENCE_EVERY_INTERVAL: Serial.println("SEQUENCE_EVERY_INTERVAL"); break;
                case AutomationEngine::ScheduledRule::SEQUENCE_WITH_TIMEOUT: Serial.println("SEQUENCE_WITH_TIMEOUT"); break;
            }

            if (sr->scene) {
                Serial.print("Scene: ");
                Serial.println(sr->scene->name);
            }

            if (sr->action) {
                Serial.print("Action target: ");
                Serial.println(sr->action->targetArea);
            }

            Serial.print("Interval: ");
            Serial.println(sr->intervalMs);

            Serial.print("Last run: ");
            Serial.println(sr->lastRun);

            Serial.print("Condition: ");
            Serial.println(sr->hasCondition ? "YES" : "NO");

            Serial.print("Fallback: ");
            Serial.println(sr->hasFallback ? "YES" : "NO");
        }

        // ---------------------------------------------------------
        // 4. DYNAMIC AUTOMATION
        // ---------------------------------------------------------
        Serial.print("\nDynamic automations: ");
        Serial.println(engine.getDynamicCount());

        for (uint8_t i = 0; i < engine.getDynamicCount(); i++) {
            auto* d = engine.getDynamicPtr(i);

            Serial.println("------------------------------");
            Serial.print("Dynamic #"); Serial.println(i);

            Serial.print("Interval: ");
            Serial.println(d->interval);

            Serial.print("Last run: ");
            Serial.println(d->lastRun);

            if (now - d->lastRun > d->interval * 3)
                Serial.println(" - WARNING: Dynamic automation delayed.");
        }

        // ---------------------------------------------------------
        // 5. LAST SCENE VALUES
        // ---------------------------------------------------------
        Serial.println("\nLast scene values:");
        for (auto& p : engine.getLastSceneValues()) {
            Serial.print("Area "); Serial.print(p.first);
            Serial.print(" = "); Serial.println(p.second);
        }

        // ---------------------------------------------------------
        // 6. ORPHAN SCENES (solo qui!)
        // ---------------------------------------------------------
        Serial.println("\n===== ORPHAN SCENES =====");

        bool orphanFound = false;

        for (uint8_t i = 0; i < engine.getSceneCount(); i++) {
            auto* s = engine.getScenePtr(i);

            bool hasActions = (s->actionCount > 0);
            bool referenced = engine.isSceneReferenced(s->name);

            bool scheduledRef = false;
            for (uint8_t j = 0; j < engine.getScheduledCount(); j++) {
                if (engine.getScheduledPtr(j)->scene == s) {
                    scheduledRef = true;
                    break;
                }
            }

            bool orphan = (!hasActions) || (!referenced && !scheduledRef);

            if (!orphan) continue;

            orphanFound = true;

            Serial.println("------------------------------");
            Serial.print("Scene: "); Serial.println(s->name);

            if (!hasActions)
                Serial.println(" - No actions.");

            if (!referenced)
                Serial.println(" - Not referenced.");

            if (!scheduledRef)
                Serial.println(" - Not used in any ScheduledRule.");
        }

        if (!orphanFound)
            Serial.println("No orphan scenes detected.");

        Serial.println("===== END AUTOMATION ENGINE DIAGNOSTIC =====\n");
    }

    static void ReportIneffectiveRules(DomoManager& manager) {
        auto& eng = manager.getAutomation();

        Serial.println("\n===== REGOLE SENZA EFFETTO =====");

        for (int i = 0; i < eng.ruleCount; i++) {
            auto& r = eng.getRule(i);

            bool writes = false;

            for (int a = 0; a < r.actionThenCount; a++)
                if (r.actionsThen[a].targetArea >= 0)
                    writes = true;

            for (int a = 0; a < r.actionElseCount; a++)
                if (r.actionsElse[a].targetArea >= 0)
                    writes = true;

            if (!writes) {
                Serial.print("Regola ");
                Serial.print(i);
                Serial.println(" → NON scrive mai nulla");
            }
        }
    }

    static void ReportScheduler(DomoManager& manager, unsigned long now) {
        Serial.println("\n===== SCHEDULER DIAGNOSTIC =====");

        auto& sched = manager.getScheduler();
        const auto& jobs = sched.getJobs();

        Serial.print("Total jobs: ");
        Serial.println(jobs.size());

        if (jobs.empty()) {
            Serial.println("No jobs registered.");
            Serial.println("===== END SCHEDULER DIAGNOSTIC =====\n");
            return;
        }

        for (size_t i = 0; i < jobs.size(); i++) {
            const auto& job = jobs[i];

            Serial.println("------------------------------");
            Serial.print("Job #");
            Serial.println(i);
            Serial.print("  Name: ");
            Serial.println(job.name);

            Serial.print("Active: ");
            Serial.println(job.active ? "YES" : "NO");

            Serial.print("Cancelled: ");
            Serial.println(job.cancelled ? "YES" : "NO");

            Serial.print("Current step: ");
            Serial.print(job.currentStep);
            Serial.print(" / ");
            Serial.println(job.steps.size());

            if (job.currentStep < job.steps.size()) {
                const auto& step = job.steps[job.currentStep];

                Serial.print("Step type: ");
                Serial.println(step.type == AsyncScheduler::NORMAL_STEP ? "NORMAL" : "BRANCH");

                Serial.print("Description: ");
                Serial.println(step.description);

                Serial.print("Delay after step: ");
                Serial.print(step.delayAfterMs);
                Serial.println(" ms");
            }

            Serial.print("Priority: ");
            Serial.println(job.priority);

            Serial.print("Priority weight: ");
            Serial.println(job.priorityWeight);

            Serial.print("Next run time: ");
            Serial.println(job.nextRunTime);

            // ---- Diagnostica di blocco ----
            if (job.active && now > job.nextRunTime + 5000) {
                Serial.println("WARNING: Job appears blocked (nextRunTime overdue).");
            }

            // ---- Diagnostica zombie ----
            if (!job.active && job.currentStep == 0 && !job.cancelled) {
                Serial.println("WARNING: Job never started.");
            }
        }

        Serial.println("===== END SCHEDULER DIAGNOSTIC =====\n");
    }

    static void ReportSplitAnalysis(DomoManager& manager, unsigned long now ) {
        Serial.println("\n===== SPLIT ANALYSIS =====");

        Buffer& buffer = manager.getBuffer();
        const auto& splits = manager.getLogic().getSplits().getAll();
        
        Serial.print("Total splits: ");
        Serial.println(splits.size());

        std::unordered_map<int, std::vector<int>> outAreaOwners;

        for (size_t i = 0; i < splits.size(); i++) {
            const auto& item = splits[i];
            const auto& s = item.split;

            Serial.println("------------------------------");
            Serial.print("Split #"); Serial.println(i);

            Serial.print("Main area: ");
            Serial.println(s.mainArea);

            Serial.print("Out areas: ");
            for (int a : s.outAreas) {
                Serial.print(a);
                Serial.print(" ");
                outAreaOwners[a].push_back(i);
            }
            Serial.println();

            Serial.print("Max time: ");
            Serial.println(s.maxTime);

            Serial.print("Running: ");
            Serial.println(item.running ? "YES" : "NO");

            // --- TIMER ---
            if (item.running && s.maxTime > 0) {
                float elapsed = item.timer.ET();  // <-- CORRETTO
                float remaining = (elapsed >= s.maxTime) ? 0 : (s.maxTime - elapsed);

                Serial.print("Elapsed: ");
                Serial.print(elapsed);
                Serial.println(" ms");

                Serial.print("Remaining: ");
                Serial.print(remaining);
                Serial.println(" ms");

                if (remaining == 0)
                    Serial.println(" - WARNING: Timer expired.");
            }

            // --- MAIN AREA ---
            BufferSourceInfo infoMain;
            if (buffer.GetData(s.mainArea, Field, infoMain)) {
                Serial.print("Main value: ");
                Serial.println(infoMain.value);

                Serial.print("Main age: ");
                Serial.print(now - infoMain.time);
                Serial.println(" ms");

                if (s.maxTime > 0 && now - infoMain.time > s.maxTime)
                    Serial.println(" - WARNING: Main area timeout exceeded.");
            } else {
                Serial.println(" - ERROR: Main area has no Field value.");
            }

            // --- OUT AREAS ---
            for (int out : s.outAreas) {
                BufferSourceInfo infoOut;
                if (!buffer.GetData(out, Field, infoOut)) {
                    Serial.print(" - ERROR: Out area ");
                    Serial.print(out);
                    Serial.println(" has no Field value.");
                    continue;
                }

                Serial.print("Out ");
                Serial.print(out);
                Serial.print(" value: ");
                Serial.println(infoOut.value);

                if (buffer.GetData(s.mainArea, Field, infoMain)) {
                    if (infoMain.value != infoOut.value)
                        Serial.println(" - WARNING: Inconsistent split (main != out).");
                }

                if (s.maxTime > 0 && now - infoOut.time > s.maxTime)
                    Serial.println(" - WARNING: Out area timeout exceeded.");

                if (buffer.IsVirtual(out))
                    Serial.println(" - NOTE: Out area is virtual.");
            }
        }

        // --- CONFLITTI ---
        Serial.println("\n===== SPLIT CONFLICTS =====");

        bool conflictFound = false;
        for (auto& kv : outAreaOwners) {
            if (kv.second.size() > 1) {
                conflictFound = true;
                Serial.print("Out area ");
                Serial.print(kv.first);
                Serial.print(" controlled by splits: ");
                for (int idx : kv.second) {
                    Serial.print(idx);
                    Serial.print(" ");
                }
                Serial.println();
            }
        }

        if (!conflictFound)
            Serial.println("No conflicts detected.");

        Serial.println("===== END SPLIT ANALYSIS =====\n");
    }

    static void ReportMissingBufferDefinitions(DomoManager& dm);
    static void ReportDeviceErrors(DomoManager& dm);
    static void ReportWatchdogs(DomoManager& dm);        
    static void ReportDeviceProfiles(DomoManager& dm);   
    static void ReportRTC(DomoManager& dm);              
    static void ReportDeviceAreas(DomoManager& dm);
    
    static void FullReport(DomoManager& manager, const DiagnosticConfig & cfg) {
        unsigned long now = manager.getTimeManager().nowMs();

        if (cfg.reportScheduler)
            ReportScheduler(manager, now);

        if (cfg.reportSplitAnalysis)
            ReportSplitAnalysis(manager, now);

        if (cfg.reportAutomationEngine)
            ReportAutomationEngine(manager, now);

        auto& buf = manager.getBuffer();
        if (cfg.reportVirtualAreas)
            Buffer::Diagnostics::ReportVirtualAreas(buf);

        if (cfg.reportMissingBufferDefs)
            ReportMissingBufferDefinitions(manager);

        if (cfg.reportNeverInitialized)
            Buffer::Diagnostics::ReportNeverInitialized(buf);

        if (cfg.reportMultipleInitialized)
            Buffer::Diagnostics::ReportMultipleInitialized(buf);
        
        if (cfg.reportAutomationConfig)
            AutomationBuilder::ReportAutomationConfig(manager.getAutomationBuilderConfig());

        if (cfg.reportDeviceAreas)
            ReportDeviceAreas(manager);

        if (cfg.reportIneffectiveRules)
            ReportIneffectiveRules(manager);

        if (cfg.reportDeviceErrors)
            ReportDeviceErrors(manager);

        if (cfg.reportWatchdogs)
            ReportWatchdogs(manager);

        if (cfg.reportDeviceProfiles)
            ReportDeviceProfiles(manager);

        if (cfg.reportRTC)
            ReportRTC(manager);

        #if HOTSTANDBY_ENABLED
            if (cfg.reportHotStandby)
                HotStandbyDiagnostic::FullReport(manager.getHotStandby());
        #else
            Serial.println("Hot-standby disabilitato (HOTSTANDBY_ENABLED=0)");
        #endif

        if (cfg.reportLogBuffer)
            Logger::ReportLogBuffer(Serial);

    }

    static void SimpleReport(DomoManager& manager) {
        auto& buf = manager.getBuffer();
        ReportDeviceAreas(manager);
        Buffer::Diagnostics::ReportMultipleInitialized(buf);       
    }
};


void Diagnostic::ReportMissingBufferDefinitions(DomoManager& dm)
{
    auto& buf  = dm.getBuffer();
    auto& devs = dm.devices().getDevices();

    Serial.println("\n===== AREE MAPPATE MA NON DEFINITE =====");

    for (auto& dev : devs) {
        int chs = dev.GetChannelsSize();
        for (int ch = 0; ch < chs; ch++) {
            auto info = dev.GetChannelInfo(ch);
            for (int i = 0; i < info.items; i++) {
                int area = dev.GetArea(ch, i);
                if (!buf.Exists(area)) {
                    Serial.print("Area ");
                    Serial.print(area);
                    Serial.print(" → MAPPATA da ");
                    Serial.print(dev.GetName());
                    Serial.println(" ma NON definita");
                }
            }
        }
    }
}

void Diagnostic::ReportDeviceErrors(DomoManager& dm)
{
    auto& devs = dm.devices().getDevices();

    Serial.println("\n===== DEVICE ERROR REPORT =====");

    unsigned long mask = 0;
    int index = 0;
    int totalErrors = 0;
    int totalParked = 0;

    for (auto& dev : devs) {

        auto& err = dev.GetError();

        // 🔥 1. Device in PARKING → NON mostrarlo negli errori
        if (err.IsVisibleParked()) {
            totalParked++;

            Serial.print("Device ");
            Serial.print(index);
            Serial.print(" → PARKED | ");
            Serial.print(dev.GetName());
            Serial.print(" | IP=");
            Serial.print(dev.GetIp()[0]); Serial.print(".");
            Serial.print(dev.GetIp()[1]); Serial.print(".");
            Serial.print(dev.GetIp()[2]); Serial.print(".");
            Serial.print(dev.GetIp()[3]);
            Serial.print(" | Addr=");
            Serial.println(dev.GetDeviceAddress());

            index++;
            continue;   // 🔥 NON entra nella sezione ERROR
        }

        // 🔥 2. Device in errore normale
        if (err.IsVisibleError()) {
            bitSet(mask, index);
            totalErrors++;

            Serial.print("Device ");
            Serial.print(index);
            Serial.print(" → ERROR | ");
            Serial.print(dev.GetName());
            Serial.print(" | IP=");
            Serial.print(dev.GetIp()[0]); Serial.print(".");
            Serial.print(dev.GetIp()[1]); Serial.print(".");
            Serial.print(dev.GetIp()[2]); Serial.print(".");
            Serial.print(dev.GetIp()[3]);
            Serial.print(" | Addr=");
            Serial.println(dev.GetDeviceAddress());
        }

        index++;
    }

    // 🔥 riepilogo
    if (totalErrors == 0 && totalParked == 0) {
        Serial.println("Nessun device in errore.");
    }

    Serial.print("Error bitmask = 0x");
    Serial.println(mask, HEX);

    Serial.print("Parked devices: ");
    Serial.println(totalParked);

    Serial.println("===== END DEVICE ERROR REPORT =====\n");
}


void Diagnostic::ReportWatchdogs(DomoManager& manager)
{
    const auto& t = manager.getTimings();

    Serial.println("\n===== WATCHDOG REPORT =====");

    auto print = [](const Watchdog::ExecTiming& e) {
        Serial.print(e.name);
        Serial.print(" → last=");
        Serial.print(e.last);
        Serial.print(" ms   avg=");
        Serial.print(e.avg_fp >> 8);
        Serial.print(" ms   maxSpike=");
        Serial.print(e.maxSpike);
        Serial.print(" ms   spikes=");
        Serial.println(e.spikeCount);
    };

    print(t.somethingChanged);
    print(t.route);
    print(t.activityLoop);
    print(t.updateCycle);

    Serial.println("===== END WATCHDOG REPORT =====\n");
}

void Diagnostic::ReportDeviceProfiles(DomoManager& manager)
{
    auto list = manager.getProfiles().list();

    Serial.println("\n===== DEVICE PROFILES =====");
    for (auto name : list)
        Serial.println(name);
}

void Diagnostic::ReportRTC(DomoManager& manager)
{
    Serial.println("\n===== RTC DIAGNOSTIC =====");

    TimeManager& tm = manager.getTimeManager();

    struct tm now;
    bool ok = tm.getDateTime(now);

    if (!ok) {
        Serial.println("RTC ERROR: impossibile leggere data/ora.");
        Serial.println("Possibili cause:");
        Serial.println(" - RTC non inizializzato");
        Serial.println(" - nessuna sincronizzazione TIME::<epoch>");
        Serial.println(" - nodo master non ancora attivo");
        Serial.println("=====================================\n");
        return;
    }

    Serial.print("Current time: ");
    Serial.print(now.tm_mday); Serial.print("/");
    Serial.print(now.tm_mon + 1); Serial.print("/");
    Serial.print(now.tm_year + 1900); Serial.print(" ");
    Serial.print(now.tm_hour); Serial.print(":");
    Serial.print(now.tm_min); Serial.print(":");
    Serial.println(now.tm_sec);

    if (now.tm_year + 1900 < 2020)
        Serial.println("WARNING: year < 2020 → tempo probabilmente non sincronizzato.");

    Serial.print("Time valid: ");
    Serial.println(tm.isRTCValid() ? "YES" : "NO");

    uint32_t epoch = tm.getEpoch();
    Serial.print("Epoch: ");
    Serial.println(epoch);

    static uint32_t prevEpoch = 0;
    if (prevEpoch != 0) {
        long drift = (long)epoch - (long)prevEpoch;
        if (abs(drift) > 2) {
            Serial.print("WARNING: drift detected: ");
            Serial.print(drift);
            Serial.println(" seconds");
        }
    }
    prevEpoch = epoch;

    Serial.println("===== END RTC DIAGNOSTIC =====\n");
}

void Diagnostic::ReportDeviceAreas(DomoManager& dm)
{
    Serial.println("\n===== DEVICE → BUFFER AREA MAP =====");

    auto& devices = dm.devices().getDevices();
    auto& buffer  = dm.getBuffer();
    auto& L       = dm.getLogic();
    auto& toggles = L.getToggles();
    auto& splits  = L.getSplits();
    auto& routes  = L.getRoutes();

    for (auto& dev : devices) {
        Serial.println("----------------------------------------");
        Serial.print("DEVICE: ");
        Serial.println(dev.GetName());

        Serial.print("  IP: ");
        auto ip = dev.GetIp();
        Serial.print(ip[0]); Serial.print(".");
        Serial.print(ip[1]); Serial.print(".");
        Serial.print(ip[2]); Serial.print(".");
        Serial.println(ip[3]);

        Serial.print("  Modbus ID: ");
        Serial.println(dev.GetDeviceAddress());

        size_t chCount = dev.GetChannelsSize();

        // Mappa per rilevare duplicati
        std::unordered_map<int,int> areaCount;

        // Prima passata: conta le aree
        for (size_t chIdx = 0; chIdx < chCount; ++chIdx)
        {
            auto ch = dev.GetChannelInfo(chIdx);
            for (int item = 0; item < ch.items; ++item)
            {
                int area = dev.GetArea(chIdx, item);
                areaCount[area]++;
            }
        }

        // Seconda passata: stampa dettagli
        for (size_t chIdx = 0; chIdx < chCount; ++chIdx)
        {
            auto ch = dev.GetChannelInfo(chIdx);

            const char* typeName =
                (ch.type == GenericPrgDevice::AI) ? "AI" :
                (ch.type == GenericPrgDevice::AO) ? "AO" :
                (ch.type == GenericPrgDevice::DI) ? "DI" :
                (ch.type == GenericPrgDevice::DO) ? "DO" : "??";

            Serial.print("  Channel ");
            Serial.print(chIdx);
            Serial.print(" (");
            Serial.print(typeName);
            Serial.println("):");

            for (int item = 0; item < ch.items; ++item)
            {
                int area = dev.GetArea(chIdx, item);

                Serial.print("    • Area ");
                Serial.print(area);
                Serial.print("  (");
                Serial.print(buffer.GetName(area));
                Serial.print(")");

                // 🔥 evidenziazione aree riservate
                if (area <= dm.devices().AREA_LAST_RESERVED)
                    Serial.print("  [RISERVATA]");

                // 🔥 evidenziazione aree duplicate
                if (areaCount[area] > 1)
                    Serial.print("  [DUPLICATA]");

                // 🔥 toggle / split / route
                if (toggles.get(area)) Serial.print("  [TOGGLE]");
                if (splits.exist(area)) Serial.print("  [SPLIT]");
                if (routes.hasRoute(area)) Serial.print("  [ROUTE]");

                Serial.println();
            }
        }
    }
}

// ======================================================
// IMPLEMENTAZIONE DiagnosticWatchAreas
// ======================================================

inline void DiagnosticWatchAreas::enableArea(int area, bool en) {
    for (auto& w : watched)
        if (w.area == area)
            w.enabled = en;
}

inline void DiagnosticWatchAreas::onValueChanged(DomoManager& manager, int area, long newValue) {

    OwnerManager& om = manager.getOwner();

    // Se i log sono già in pausa → non fare nulla
    if (om.areLogsPaused())
        return;

    for (auto& w : watched) {
        if (!w.enabled || w.area != area) continue;

        if (w.lastValue != LONG_MIN && w.lastValue != newValue) {

            // 🔥 Pausa log tramite OwnerManager
            om.pauseLogs();
            autoPaused = true;

            Serial.println("\n===== AUTO DIAGNOSTIC TRIGGERED =====");
            Serial.print("Area cambiata: ");
            Serial.println(area);

            printAll(manager);
        }

        w.lastValue = newValue;
    }
}

void DiagnosticWatchAreas::onButtonPressed(DomoManager& manager, int mode, const DiagnosticConfig & cfg) {

    OwnerManager& om = manager.getOwner();

    // Se i log sono in pausa → riattivali
    if (om.areLogsPaused()) {
        om.resumeLogs();
        autoPaused = false;
        Serial.println("\n*** LOG RIABILITATI ***");
        return;
    }

    // Altrimenti → pausa i log
    om.pauseLogs();
    autoPaused = true;
    Serial.println("\n*** LOG IN PAUSA ***");

    if(mode==1) {
        Serial.println("\n******** FULL DIAGNOSTIC ********");
        Diagnostic::FullReport(manager, cfg);
        printAll(manager);

        // 🔥 AGGIUNTA: diagnostica Frontend
        Diagnostic::RunFrontendDiagnostics();
    } else {
        Serial.println("\n******** BASE CONFIGURATION ********");
        Diagnostic::SimpleReport(manager);
    }
}

inline void DiagnosticWatchAreas::printAll(DomoManager& manager) {
    auto& buf = manager.getBuffer();

    Serial.println("\n===== WATCHED AREAS =====");

    for (auto& w : watched) {
        BufferSourceInfo info;
        bool ok = buf.GetData(w.area, Field, info);

        Serial.print("Area ");
        Serial.print(w.area);
        Serial.print(" (");
        Serial.print(buf.GetName(w.area));
        Serial.print(") = ");

        if (ok) Serial.println(info.value);
        else Serial.println("N/A");
    }

    Serial.println("===== END WATCHED AREAS =====\n");
}


#endif
