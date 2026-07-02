#ifndef DMHStandby_HPP
#define DMHStandby_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑06‑24
   Note:
                    • Nessuna

   ============================================================================ */

#include "DMRS485Node.hpp"
#include "DMBaseClass.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


class HotStandbyManager {
public:
    // ---------------------------------------------------------
    // CALLBACK TYPES
    // ---------------------------------------------------------
    typedef void (*RoleCallback)();
    typedef void (*PeerCallback)();
    typedef void (*StateSyncCallback)(uint8_t* data, uint8_t len);
    typedef void (*ProcessDataCallback)(uint8_t* data, uint8_t len);
    typedef void (*TimestampCallback)(uint32_t epoch);

    // ---------------------------------------------------------
    // COSTRUTTORE
    // ---------------------------------------------------------
    HotStandbyManager(bool startAsMaster, TimeManager* tm)
        : node(startAsMaster), isMaster(startAsMaster), tm(tm) {}

    // ---------------------------------------------------------
    // CALLBACK REGISTRATION
    // ---------------------------------------------------------
    void onBecomeMaster(RoleCallback cb) { cbBecomeMaster = cb; }
    void onBecomeSlave(RoleCallback cb) { cbBecomeSlave = cb; }
    void onPeerAlive(PeerCallback cb) { cbPeerAlive = cb; }
    void onPeerLost(PeerCallback cb) { cbPeerLost = cb; }
    void onStateSync(StateSyncCallback cb) { cbStateSync = cb; }
    void onProcessData(ProcessDataCallback cb) { cbProcessData = cb; }
    void onTimestampSync(TimestampCallback cb) { cbTimestampSync = cb; }

    // ---------------------------------------------------------
    // INIT
    // ---------------------------------------------------------
    void begin(unsigned long baud = 9600) {
        LOG_IF("HotStandby", "Avvio manager (isMaster=%d)", isMaster);

        node.begin(baud);

        node.onPacket(onPacketStatic);
        node.onTimeout(onTimeoutStatic);
        node.onError(onErrorStatic);

        instance = this;
        lastHeartbeat = millis();
        lastPeerHeartbeat = millis();

        LOG_IF("HotStandby", "Inizializzazione completata");
    }

    // ---------------------------------------------------------
    // LOOP NON-BLOCCANTE
    // ---------------------------------------------------------
    void poll() {
        node.poll();
        handleHeartbeat();
        handleFailover();
        handleFailback();
        handleStateSync();
        handleProcessData();
        handleTimestampSync();
    }

    // ---------------------------------------------------------
    // API PUBBLICA: replica variabili di processo
    // ---------------------------------------------------------
    void updateProcessData(uint8_t* data, uint8_t len) {
        memcpy(processData, data, len);
        processDataLen = len;
    }

    bool isMaster;

    // Accessors for diagnostics
    unsigned long getLastHeartbeat() const { return millis() - lastHeartbeat; }
    unsigned long getLastPeerHeartbeat() const { return millis() - lastPeerHeartbeat; }
    unsigned long getHeartbeatTimeout() const { return heartbeatTimeout; }

    unsigned long getLastStateSync() const { return millis() - lastStateSync; }
    unsigned long getStateSyncInterval() const { return stateSyncInterval; }
    uint8_t getStateDataLen() const { return stateDataLen; }

    unsigned long getLastProcessDataSync() const { return millis() - lastProcessDataSync; }
    unsigned long getProcessDataInterval() const { return processDataInterval; }
    uint8_t getProcessDataLen() const { return processDataLen; }

    unsigned long getLastTimestampSync() const { return millis() - lastTimestampSync; }
    unsigned long getTimestampSyncInterval() const { return timestampInterval; }
    int32_t getEpochOffset() const { return epochOffset; }

    RS485Node& getNode() { return node; }

private:
    RS485Node node;
    TimeManager* tm;

    // Heartbeat
    const uint16_t heartbeatInterval = 500;
    const uint16_t heartbeatTimeout  = 1500;
    unsigned long lastHeartbeat = 0;
    unsigned long lastPeerHeartbeat = 0;

    // State sync
    const uint16_t stateSyncInterval = 300;
    uint8_t stateData[32];
    uint8_t stateDataLen = 0;
    unsigned long lastStateSync = 0;

    // Process data replication
    const uint16_t processDataInterval = 200;
    uint8_t processData[32];
    uint8_t processDataLen = 0;
    unsigned long lastProcessDataSync = 0;

    // Timestamp sync (epoch)
    const uint16_t timestampInterval = 1000;
    unsigned long lastTimestampSync = 0;
    int32_t epochOffset = 0;

    // Callbacks
    RoleCallback cbBecomeMaster = nullptr;
    RoleCallback cbBecomeSlave = nullptr;
    PeerCallback cbPeerAlive = nullptr;
    PeerCallback cbPeerLost = nullptr;
    StateSyncCallback cbStateSync = nullptr;
    ProcessDataCallback cbProcessData = nullptr;
    TimestampCallback cbTimestampSync = nullptr;

    // Static trampoline
    static HotStandbyManager* instance;

    static void onPacketStatic(uint8_t type, uint8_t* payload, uint8_t len) {
        instance->onPacket(type, payload, len);
    }
    static void onTimeoutStatic() {
        instance->onTimeout();
    }
    static void onErrorStatic(uint8_t code) {
        instance->onError(code);
    }

    // ---------------------------------------------------------
    // PACKET HANDLING
    // ---------------------------------------------------------
    void onPacket(uint8_t type, uint8_t* payload, uint8_t len) {
        LOG_IF("HotStandby", "Pacchetto ricevuto: type=0x%02X len=%d", type, len);

        switch (type) {
        case 0x01: // HEARTBEAT
            LOG_IF("HotStandby", "Heartbeat ricevuto");
            lastPeerHeartbeat = millis();
            if (cbPeerAlive) cbPeerAlive();
            break;

        case 0x02: // STATE_SYNC
            LOG_IF("HotStandby", "StateSync ricevuto (%d bytes)", len);
            if (cbStateSync) cbStateSync(payload, len);
            break;

        case 0x03: // PROCESS_DATA
            LOG_IF("HotStandby", "ProcessData ricevuto (%d bytes)", len);
            if (cbProcessData) cbProcessData(payload, len);
            break;

        case 0x04: // TIMESTAMP_SYNC (epoch)
            LOG_IF("HotStandby", "TimestampSync ricevuto");
            handleTimestampPacket(payload, len);
            break;
        }
    }

    void onTimeout() {}
    void onError(uint8_t code) {
        LOG_WF("HotStandby", "Errore RS485: code=%d", code);
    }

    // ---------------------------------------------------------
    // HEARTBEAT
    // ---------------------------------------------------------
    void handleHeartbeat() {
        if (millis() - lastHeartbeat > heartbeatInterval) {
            uint8_t dummy = 0;
            node.sendPacket(0x01, &dummy, 0, false);
            lastHeartbeat = millis();

            LOG_IF("HotStandby", "Heartbeat inviato (isMaster=%d)", isMaster);
        }
    }

    // ---------------------------------------------------------
    // FAILOVER
    // ---------------------------------------------------------
    void handleFailover() {
        if (!isMaster && millis() - lastPeerHeartbeat > heartbeatTimeout) {
            LOG_WF("HotStandby", "Failover: master non risponde da %lu ms",
                   millis() - lastPeerHeartbeat);
            becomeMaster();
        }
    }

    // ---------------------------------------------------------
    // FAILBACK
    // ---------------------------------------------------------
    void handleFailback() {
        if (isMaster) return;

        if (millis() - lastPeerHeartbeat < heartbeatTimeout / 2) {
            LOG_IF("HotStandby", "Failback: master tornato operativo");
            becomeSlave();
        }
    }

    // ---------------------------------------------------------
    // STATE SYNC
    // ---------------------------------------------------------
    void handleStateSync() {
        if (!isMaster) return;

        if (millis() - lastStateSync > stateSyncInterval) {
            node.sendPacket(0x02, stateData, stateDataLen, false);
            lastStateSync = millis();

            LOG_IF("HotStandby", "StateSync inviato (%d bytes)", stateDataLen);
        }
    }

    // ---------------------------------------------------------
    // PROCESS DATA REPLICATION
    // ---------------------------------------------------------
    void handleProcessData() {
        if (!isMaster) return;

        if (millis() - lastProcessDataSync > processDataInterval) {
            node.sendPacket(0x03, processData, processDataLen, false);
            lastProcessDataSync = millis();

            LOG_IF("HotStandby", "ProcessData inviato (%d bytes)", processDataLen);
        }
    }

    // ---------------------------------------------------------
    // TIMESTAMP SYNC (epoch)
    // ---------------------------------------------------------
    void handleTimestampSync() {
        if (!isMaster) return;

        if (millis() - lastTimestampSync > timestampInterval) {
            uint32_t epoch = tm->getEpoch();

            uint8_t buf[4] = {
                (uint8_t)(epoch & 0xFF),
                (uint8_t)((epoch >> 8) & 0xFF),
                (uint8_t)((epoch >> 16) & 0xFF),
                (uint8_t)((epoch >> 24) & 0xFF)
            };

            node.sendPacket(0x04, buf, 4, false);
            lastTimestampSync = millis();

            LOG_IF("HotStandby", "TimestampSync inviato: epoch=%lu", epoch);
        }
    }

    void handleTimestampPacket(uint8_t* payload, uint8_t len) {
        if (len != 4) return;

        uint32_t masterEpoch =
            (uint32_t)payload[0] |
            ((uint32_t)payload[1] << 8) |
            ((uint32_t)payload[2] << 16) |
            ((uint32_t)payload[3] << 24);

        tm->updateFromEpoch(masterEpoch);

        LOG_IF("HotStandby", "RTC aggiornato da master: epoch=%lu", masterEpoch);

        if (cbTimestampSync)
            cbTimestampSync(masterEpoch);
    }

    // ---------------------------------------------------------
    // ROLE SWITCHING
    // ---------------------------------------------------------
    void becomeMaster() {
        LOG_IF("HotStandby", "Cambio ruolo: divento MASTER");

        isMaster = true;
        node.isMaster = true;

        if (cbBecomeMaster) cbBecomeMaster();
        if (cbPeerLost) cbPeerLost();
    }

    void becomeSlave() {
        LOG_IF("HotStandby", "Cambio ruolo: divento SLAVE");

        isMaster = false;
        node.isMaster = false;

        if (cbBecomeSlave) cbBecomeSlave();
    }
};

HotStandbyManager* HotStandbyManager::instance = nullptr;

#endif
