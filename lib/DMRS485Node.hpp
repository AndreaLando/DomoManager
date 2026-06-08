#ifndef DMRS485Node_HPP
#define DMRS485Node_HPP

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

/* ============================================================================
   RS485NODE & HOTSTANDBYMANAGER — Comunicazione RS485 e failover attivo/passivo
   ============================================================================

   Questo modulo fornisce due componenti avanzati per sistemi Opta e PLC:

       • RS485Node           → protocollo RS485 con pacchetti, ACK/NACK, retry
       • HotStandbyManager   → gestione hot‑standby master/slave con failover

   Entrambi sono progettati per funzionare in ambienti industriali, con logica
   deterministica, nessun blocco e massima affidabilità nella comunicazione.

   ---------------------------------------------------------------------------
   1. RS485NODE — Protocollo RS485 con ACK, retry e state machine RX
   ---------------------------------------------------------------------------

   RS485Node implementa un protocollo leggero e robusto basato su frame:

       START   = 0x7E
       LEN     = numero byte (TYPE + PAYLOAD)
       TYPE    = tipo pacchetto
       PAYLOAD = dati variabili
       CHKSUM  = XOR di LEN, TYPE e PAYLOAD

   Funzionalità principali:
       • sendPacket() con ACK opzionale e retry automatico
       • state machine RX non‑bloccante
       • callback per pacchetti, errori e timeout
       • gestione automatica di ACK (0xFE) e NACK (0xFF)
       • timeout configurato per ritrasmissione
       • memorizzazione ultimo pacchetto per retry

   Logica di ricezione:
       • WAIT_START → WAIT_LEN → WAIT_TYPE → WAIT_PAYLOAD → WAIT_CHKSUM
       • verifica checksum
       • dispatch al callback utente

   Utilizzo tipico:
       • comunicazione tra Opta
       • reti RS485 multi‑nodo
       • protocolli custom industriali

   ---------------------------------------------------------------------------
   2. HOTSTANDBYMANAGER — Failover attivo/passivo tra due Opta
   ---------------------------------------------------------------------------

   HotStandbyManager implementa un sistema completo di ridondanza:

       • master attivo
       • slave in standby
       • failover automatico se il master non risponde
       • failback automatico quando il master torna operativo
       • sincronizzazione stato, dati di processo e timestamp

   Componenti principali:
       • heartbeat periodico
       • timeout heartbeat per failover
       • state sync (variabili di stato)
       • process data sync (variabili di processo)
       • timestamp sync (epoch)
       • callback per cambio ruolo, peer alive/lost, sync e process data

   Pacchetti supportati:
       • 0x01 → HEARTBEAT
       • 0x02 → STATE_SYNC
       • 0x03 → PROCESS_DATA
       • 0x04 → TIMESTAMP_SYNC

   Logica failover:
       • se slave non riceve heartbeat entro heartbeatTimeout → diventa master
       • se master ritorna operativo → failback automatico

   Logica failback:
       • se slave rileva heartbeat stabile → ritorna slave

   Sincronizzazione:
       • stato replicato ogni stateSyncInterval
       • dati di processo replicati ogni processDataInterval
       • timestamp replicato ogni timestampInterval

   Obiettivo:
       • garantire continuità operativa in sistemi critici
       • evitare downtime
       • mantenere sincronizzazione tra nodi

   ---------------------------------------------------------------------------
   3. CALLBACKS
   ---------------------------------------------------------------------------

   RS485Node:
       • onPacket(type, payload, len)
       • onError(code)
       • onTimeout()

   HotStandbyManager:
       • onBecomeMaster()
       • onBecomeSlave()
       • onPeerAlive()
       • onPeerLost()
       • onStateSync(data, len)
       • onProcessData(data, len)
       • onTimestampSync(epoch)

   ---------------------------------------------------------------------------
   4. NOTE DI DESIGN
   ---------------------------------------------------------------------------

   • Nessuna operazione bloccante: tutto è non‑blocking.
   • State machine RX ottimizzata per sistemi embedded.
   • Retry e timeout integrati per affidabilità industriale.
   • Failover deterministico e prevedibile.
   • Sincronizzazione dati e timestamp integrata.
   • Nessuna allocazione dinamica ricorrente.
   • Logging dettagliato tramite Logger.

   ============================================================================ */
#include <ArduinoRS485.h>

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class RS485Node {
public:
    // ---------------------------------------------------------
    // CALLBACK TYPES
    // ---------------------------------------------------------
    typedef void (*PacketCallback)(uint8_t type, uint8_t* payload, uint8_t len);
    typedef void (*ErrorCallback)(uint8_t code);
    typedef void (*TimeoutCallback)();

    // ---------------------------------------------------------
    // COSTRUTTORE
    // ---------------------------------------------------------
    RS485Node(bool master = false)
        : isMaster(master) {}

    void begin(unsigned long baud = 9600) {
        RS485.begin(baud);
        lastSendTime = millis();
    }

    // ---------------------------------------------------------
    // REGISTRAZIONE CALLBACK
    // ---------------------------------------------------------
    void onPacket(PacketCallback cb) { packetCallback = cb; }
    void onError(ErrorCallback cb) { errorCallback = cb; }
    void onTimeout(TimeoutCallback cb) { timeoutCallback = cb; }

    // ---------------------------------------------------------
    // INVIO PACCHETTO CON ACK/RETRY
    // ---------------------------------------------------------
    void sendPacket(uint8_t type, const uint8_t* payload, uint8_t len, bool requireAck = false) {
        uint8_t totalLen = 1 + len; // TYPE + PAYLOAD
        uint8_t checksum = totalLen ^ type;
        for (uint8_t i = 0; i < len; i++) checksum ^= payload[i];

        RS485.beginTransmission();
        RS485.write(0x7E);          // START
        RS485.write(totalLen);      // LEN
        RS485.write(type);          // TYPE
        RS485.write(payload, len);  // PAYLOAD
        RS485.write(checksum);      // CHKSUM
        RS485.endTransmission();

        if (requireAck) {
            awaitingAck = true;
            retryCount = 0;
            lastSendTime = millis();
            lastTypeSent = type;
            lastPayloadLen = len;
            memcpy(lastPayload, payload, len);
        }
    }

    // ---------------------------------------------------------
    // POLL NON-BLOCCANTE (RX + TIMEOUT + RETRY)
    // ---------------------------------------------------------
    void poll() {
        handleRxStateMachine();
        handleAckTimeout();
    }

    // ---------------------------------------------------------
    // FLAG MASTER/SLAVE
    // ---------------------------------------------------------
    bool isMaster;

    bool isAwaitingAck() const { return awaitingAck; }
    uint8_t getRetryCount() const { return retryCount; }
    unsigned long getLastSendTime() const { return millis() - lastSendTime; }

private:
    // ---------------------------------------------------------
    // STATE MACHINE RX
    // ---------------------------------------------------------
    enum State {
        WAIT_START,
        WAIT_LEN,
        WAIT_TYPE,
        WAIT_PAYLOAD,
        WAIT_CHKSUM
    };

    State state = WAIT_START;

    uint8_t packetLen = 0;
    uint8_t type = 0;
    uint8_t payload[64];
    uint8_t index = 0;
    uint8_t checksum = 0;

    // ---------------------------------------------------------
    // CALLBACKS
    // ---------------------------------------------------------
    PacketCallback packetCallback = nullptr;
    ErrorCallback errorCallback = nullptr;
    TimeoutCallback timeoutCallback = nullptr;

    // ---------------------------------------------------------
    // ACK / RETRY
    // ---------------------------------------------------------
    bool awaitingAck = false;
    uint8_t retryCount = 0;
    const uint8_t maxRetries = 3;
    const uint16_t ackTimeout = 300; // ms

    unsigned long lastSendTime = 0;

    uint8_t lastTypeSent = 0;
    uint8_t lastPayload[64];
    uint8_t lastPayloadLen = 0;

    // ---------------------------------------------------------
    // STATE MACHINE IMPLEMENTATION
    // ---------------------------------------------------------
    void handleRxStateMachine() {
        while (RS485.available()) {
            uint8_t b = RS485.read();

            switch (state) {
            case WAIT_START:
                if (b == 0x7E) state = WAIT_LEN;
                break;

            case WAIT_LEN:
                packetLen = b;
                index = 0;
                checksum = b;
                state = WAIT_TYPE;
                break;

            case WAIT_TYPE:
                type = b;
                checksum ^= b;
                if (packetLen == 1)
                    state = WAIT_CHKSUM;
                else
                    state = WAIT_PAYLOAD;
                break;

            case WAIT_PAYLOAD:
                payload[index++] = b;
                checksum ^= b;
                if (index >= packetLen - 1)
                    state = WAIT_CHKSUM;
                break;

            case WAIT_CHKSUM:
                if (checksum == b) {
                    processPacket(type, payload, packetLen - 1);
                } else {
                    if (errorCallback) errorCallback(1); // checksum error
                }
                state = WAIT_START;
                return;
            }
        }
    }

    // ---------------------------------------------------------
    // GESTIONE PACCHETTO RICEVUTO
    // ---------------------------------------------------------
    void processPacket(uint8_t type, uint8_t* data, uint8_t len) {
        // Se è un ACK
        if (type == 0xFE) {
            awaitingAck = false;
            return;
        }

        // Se è un NACK
        if (type == 0xFF) {
            retrySend();
            return;
        }

        // Se richiede ACK
        if (type & 0x80) {
            sendAck();
        }

        // Callback utente
        if (packetCallback)
            packetCallback(type, data, len);
    }

    // ---------------------------------------------------------
    // ACK / NACK
    // ---------------------------------------------------------
    void sendAck() {
        uint8_t dummy = 0;
        sendPacket(0xFE, &dummy, 0, false);
    }

    void sendNack() {
        uint8_t dummy = 0;
        sendPacket(0xFF, &dummy, 0, false);
    }

    // ---------------------------------------------------------
    // TIMEOUT + RETRY
    // ---------------------------------------------------------
    void handleAckTimeout() {
        if (!awaitingAck) return;

        if (millis() - lastSendTime > ackTimeout) {
            if (retryCount < maxRetries) {
                retrySend();
            } else {
                awaitingAck = false;
                if (timeoutCallback) timeoutCallback();
            }
        }
    }

    void retrySend() {
        retryCount++;
        lastSendTime = millis();
        sendPacket(lastTypeSent, lastPayload, lastPayloadLen, true);
    }
};

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
