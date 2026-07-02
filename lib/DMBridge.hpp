#ifndef DMBridge_HPP
#define DMBridge_HPP

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

#include "DMBaseClass.hpp"
#include "DMAdapters.hpp"


#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class DMBridge;   // forward
class BridgeProtocol {
public:
    static bool handleIncoming(
        const String& msg,
        AEERegistry& reg,
        ICommandTransport& transport,
        AEEProtocol& proto,
        DMBridge* parent
    );
};


// ============================================================
//  PEER MONITOR
// ============================================================
class BridgePeerMonitor {
private:
    unsigned long lastActivity = 0;
    bool online = false;

public:
    void onPacket(unsigned long now) {
        lastActivity = now;
        if (!online) {
            online = true;
            LOG_IF("BRIDGE", "Peer ONLINE");
        }
    }

    void checkOffline(unsigned long now, unsigned long timeoutMs = 120000) {
        if (online && (now - lastActivity > timeoutMs)) {
            online = false;
            LOG_IF("BRIDGE", "Peer OFFLINE");
        }
    }

    bool isOnline() const { return online; }
};

// ============================================================
//  ACK ENGINE
// ============================================================
class AEEProtocolEngine {
public:
    using SendCallback = std::function<void(const String&)>;

    struct AckState {
        uint32_t seqCounter = 0;
        uint32_t lastSentSeq = 0;
        uint32_t lastAckSeq = 0;
        unsigned long lastSendTime = 0;

        bool ackOk() const { return lastAckSeq == lastSentSeq; }
        bool ackTimeout(unsigned long now, unsigned long timeoutMs) const {
            return !ackOk() && (now - lastSendTime > timeoutMs);
        }
        uint32_t nextSeq() { return ++seqCounter; }
    };

    static void send(AEERegistry& reg, AckState& state, AEEProtocol& proto, SendCallback sendFn) {
        String json = proto.serializeChangedJSON(reg);
        if (json.length() == 0) return;

        StaticJsonDocument<1024> doc;
        deserializeJson(doc, json);

        uint32_t seq = state.nextSeq();
        doc["seq"] = seq;

        String out;
        serializeJson(doc, out);

        // 🔵 LOG AEE TX (solo pacchetti AEE, non ping, non schema)
        LOG_IF("BRIDGE::AEE::TX", "Sending AEE update: %s", out.c_str());

        sendFn(out);

        state.lastSentSeq = seq;
        state.lastSendTime = millis();

        // 🟦 PATCH: reset dei flag changed dopo l’invio
        reg.clearAllChanged();
    }


    static void onPacket(const String& msg, AckState& state) {
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, msg)) return;

        if (doc.containsKey("ack"))
            state.lastAckSeq = doc["ack"];
    }

    static void checkResend(AEERegistry& reg,
                        AckState& state,
                        unsigned long now,
                        AEEProtocol& proto,
                        SendCallback sendFn)

    {
        const unsigned long RESEND_TIMEOUT = 120;
        const unsigned long MIN_INTERVAL   = 40;

        if (state.ackTimeout(now, RESEND_TIMEOUT) &&
            (now - state.lastSendTime > MIN_INTERVAL))
        {
            send(reg, state, proto, sendFn);
        }
    }
};

// ============================================================
//  STATE MACHINE
// ============================================================
enum class BridgeState {
    DISCONNECTED,
    CONNECTING,
    REQUEST_SCHEMA,
    WAIT_SCHEMA,
    ALIGNING,
    READY,
    DATA_EXCHANGE
};

// ============================================================
//  DMBridge (COMMON + MASTER)
// ============================================================
class DMBridge {   
protected:
    AEEProtocol aee;   // istanza
    ICommandTransport* transport = nullptr;
    AEERegistry* reg = nullptr;

    bool isMaster = true;   // MASTER by default
    BridgeState state = BridgeState::DISCONNECTED;

    BridgePeerMonitor peer;
    AEEProtocolEngine::AckState ack;

    // --- LOG RATE LIMIT ---
    unsigned long lastRxLog = 0;
    unsigned long lastTxLog = 0;
    bool lastPeerOnline = false;

    bool helloSent = false;
    unsigned long helloTime = 0;
    static constexpr unsigned long HELLO_TIMEOUT = 8000;
    bool schemaRequested = false;
    bool schemaReceived  = false;

    // ============================================================
    //  INTERNAL CLASS: SCHEMA MULTIFRAME ENGINE
    // ============================================================
    class SchemaMultiframe {
    public:
        DMBridge* parent = nullptr;
        ICommandTransport* transport = nullptr;

        // TX (MASTER)
        struct TxState {
            bool active = false;
            std::vector<String> frames;
            size_t index = 0;
            unsigned long lastSend = 0;
            const unsigned long SEND_INTERVAL = 20;
            const unsigned long ACK_TIMEOUT   = 800;
            int waitingAckFor = -1;

            // --- Retry counter (optional but recommended) ---
            int ackRetryCount = 0;

            // --- Reset TX state ---
            void reset() {
                active = false;
                frames.clear();
                index = 0;
                lastSend = 0;
                waitingAckFor = -1;
                ackRetryCount = 0;
            }
        } tx;


        // RX (SLAVE)
        struct RxState {
            bool active = false;
            String buffer;
            int expected = -1;
            int received = 0;
            unsigned long startTs = 0;
            const unsigned long TIMEOUT = 35000;

            // --- Reset RX state ---
            void reset() {
                active = false;
                buffer = "";
                expected = -1;
                received = 0;
                startTs = 0;
            }
        } rx;

        bool isTxActive() const {
            return tx.active;
        }

        bool isRxActive() const {
            return rx.active;
        }

        void restartTx(const String& fullSchema) {
            tx.reset();
            startTx(fullSchema);
        }

        void abortTx() {
            if (tx.active) {
                LOG_WF("BRIDGE::SCHEMA", "Abort TX multiframe → reset");
                tx.reset();
            }
        }

        // ------------------------------------------------------------
        // INIT
        // ------------------------------------------------------------
        void init(DMBridge* p, ICommandTransport* t) {
            parent = p;
            transport = t;
        }

        // ------------------------------------------------------------
        // MASTER: avvia invio schema multiframe
        // ------------------------------------------------------------
        void startTx(const String& fullSchema) {
            const size_t MAX_CHUNK = 160;

            // 🔥 reset completo stato TX
            tx.reset();

            tx.frames.clear();
            tx.index = 0;
            tx.active = true;

            size_t total = (fullSchema.length() + MAX_CHUNK - 1) / MAX_CHUNK;

            for (size_t i = 0; i < total; i++) {
                size_t start = i * MAX_CHUNK;
                size_t len   = min(MAX_CHUNK, fullSchema.length() - start);

                String chunk = fullSchema.substring(start, start + len);

                StaticJsonDocument<512> doc;
                doc["schema_part"] = (int)i;
                doc["total"]       = (int)total;
                doc["data"]        = chunk;
                doc["fid"]         = (int)i;

                String out;
                serializeJson(doc, out);
                tx.frames.push_back(out);
            }

            LOG_IF("BRIDGE::SCHEMA", "TX multiframe avviato (%u frames)", tx.frames.size());
        }

        // ------------------------------------------------------------
        // MASTER: invio frame + gestione ACK
        // ------------------------------------------------------------
        void handleTx(unsigned long now) {
            if (!parent->isMaster) return;
            if (!tx.active) return;

            if (tx.index >= tx.frames.size()) {
                LOG_IF("BRIDGE::SCHEMA", "TX multiframe COMPLETATO");
                tx.active = false;
                return;
            }

            // attesa ACK
            if (tx.waitingAckFor >= 0) {
                if (now - tx.lastSend > tx.ACK_TIMEOUT) {
                    //LOG_WF("BRIDGE::SCHEMA", "ACK timeout frame %d → ritrasmissione",
                    //       tx.waitingAckFor);
                    transport->send(tx.frames[tx.index]);
                    tx.lastSend = now;
                }
                return;
            }

            // pacing
            if (now - tx.lastSend < tx.SEND_INTERVAL)
                return;

            // invia frame corrente
            String& frame = tx.frames[tx.index];
            transport->send(frame);
            tx.lastSend = now;

            StaticJsonDocument<256> doc;
            deserializeJson(doc, frame);
            tx.waitingAckFor = doc["fid"];

            //LOG_IF("BRIDGE::SCHEMA::TX", "Frame %u inviato (fid=%d)",
            //       tx.index+1, tx.waitingAckFor);
        }

        // ------------------------------------------------------------
        // SLAVE: ricezione frame + ACK
        // ------------------------------------------------------------
        bool handleRx(const JsonDocument& doc, unsigned long now) {
            // --- ACK from SLAVE to MASTER ---
            if (doc.containsKey("schema_ack")) {
                int ack = doc["schema_ack"];
                if (parent->isMaster && tx.waitingAckFor == ack) {
                    //LOG_IF("BRIDGE::SCHEMA", "ACK received for frame %d", ack);
                    tx.waitingAckFor = -1;
                    tx.index++;
                }
                return true;
            }

            // --- Not a schema frame ---
            if (!doc.containsKey("schema_part"))
                return false;

            // --- MASTER never receives schema parts ---
            if (parent->isMaster)
                return true;

            int part  = doc["schema_part"];
            int total = doc["total"];
            const char* data = doc["data"];

            // --- If a new frame 0 arrives while RX is active → reset ---
            if (part == 0 && rx.active && rx.received > 0) {
                LOG_WF("BRIDGE::SCHEMA",
                    "New frame 0 received while RX in progress → resetting RX");
                rx.reset();
            }

            /// --- Start new RX session ---
            if (!rx.active) {
                // La sessione può iniziare SOLO da frame 0
                if (part != 0) {
                    LOG_WF("BRIDGE::SCHEMA",
                        "First frame must be 0, got %d → ignoring", part);
                    return true; // non resetto, semplicemente ignoro questo frame
                }

                rx.active   = true;
                rx.expected = total;
                rx.received = 0;
                rx.buffer   = "";
                rx.startTs  = now;

                LOG_IF("BRIDGE::SCHEMA", "RX multiframe started (%d parts)", total);
            }

            // --- Reject out-of-order frames ---
            if (part != rx.received) {
                LOG_WF("BRIDGE::SCHEMA",
                    "Out-of-order frame: expected %u, got %u → resetting RX",
                    rx.received, part);
                rx.reset();
                return true;
            }

            // --- Append data ---
            rx.buffer += data;
            rx.received++;

            //LOG_DF("BRIDGE::SCHEMA", "RX frame %d/%d (len=%u)",
            //    part + 1, total, rx.buffer.length());

            // --- Send ACK ---
            StaticJsonDocument<128> ack;
            ack["schema_ack"] = part;
            String out;
            serializeJson(ack, out);
            transport->send(out);

            // --- Timeout ---
            if (now - rx.startTs > rx.TIMEOUT) {
                LOG_WF("BRIDGE::SCHEMA", "RX multiframe timeout → reset");
                rx.reset();
                return true;
            }

            // --- Completed ---
            if (rx.received == rx.expected) {
                LOG_IF("BRIDGE::SCHEMA", "RX multiframe COMPLETE (len=%u)",
                    rx.buffer.length());

                StaticJsonDocument<8192> doc2;
                auto err = deserializeJson(doc2, rx.buffer);
                if (err) {
                    LOG_WF("BRIDGE::SCHEMA",
                        "Multiframe JSON error: %s", err.c_str());
                    rx.reset();
                    return true;
                }

                JsonArray arr = doc2["schema"].as<JsonArray>();
                parent->onSchemaReceived(arr);
                parent->schemaReceived = true;
                parent->setState(BridgeState::ALIGNING);

                rx.reset();
            }

            return true;
        }

    };

    SchemaMultiframe schema;

public:
    DMBridge() {}

    virtual ~DMBridge() {}

    virtual void onAEEVariableChanged(AEEVariableBase* v, unsigned long now) {}

    void init(ICommandTransport* t, AEERegistry& r) {
        init(t, r, true);   // 🔥 default: MASTER
    }

    void init(ICommandTransport* t, AEERegistry& r, bool master) {
        isMaster = master;
        transport = t;
        reg = &r;

        aee = AEEProtocol(master);   // se vuoi re-inizializzarlo qui
        schema.init(this, transport);

        transport->begin();
        setState(BridgeState::CONNECTING);
        LOG_IF("BRIDGE", "Bridge avviato come %s", isMaster ? "MASTER" : "SLAVE");
    }

    AEERegistry* getRegistry() { return reg; }

    virtual void loop(unsigned long now) {
        if (!transport || !reg) return;

        schema.handleTx(now);

        transport->loop(now);
        processIncoming(now);
        runStateMachine(now);
        handleResend(now);

        // ============================================================
        //  LOG PEER ONLINE/OFFLINE
        // ============================================================
        bool online = peer.isOnline();
        if (online != lastPeerOnline) {
            //LOG_IF("MASTER::PEER", "Peer %s", online ? "ONLINE" : "OFFLINE");
            lastPeerOnline = online;
        }
    }

    void sendRaw(const String& payload) {
        if (!transport) return;

        transport->send(payload);

        /*
        // --- LOG TX (rate-limited) ---
        if (millis() - lastTxLog > 300) {
            LOG_IF("MASTER::TX", "TX: %s", payload.c_str());
            lastTxLog = millis();
        } */
    }


    BridgePeerMonitor& getPeer() { return peer; }
    BridgeState getState() const {
        return state;
    }

    bool isHandshakeActive() const {
        return state == BridgeState::CONNECTING ||
            state == BridgeState::REQUEST_SCHEMA ||
            state == BridgeState::WAIT_SCHEMA ||
            state == BridgeState::ALIGNING;
    }

    bool isReadyForData() const {
        return state == BridgeState::READY ||
            state == BridgeState::DATA_EXCHANGE;
    }

    std::function<void(AEEVariableBase*, unsigned long)> onFrontendAEEChange;

protected:

    // ============================================================
    //  VIRTUAL CALLBACKS (SLAVE OVERRIDES)
    // ============================================================
    virtual void onSchemaRequest() {}                 // MASTER risponde
    virtual void onSchemaReceived(const JsonArray&) {}// SLAVE ricostruisce
    virtual void onAligned() {}                       // SLAVE allinea

    // ============================================================
    //  STATE MACHINE (HARD-CODED)
    // ============================================================
    void runStateMachine(unsigned long now) {

        switch (state) {
            case BridgeState::DISCONNECTED:
                setState(BridgeState::CONNECTING);
                break;

            case BridgeState::CONNECTING:
                if (isMaster) {
                    schemaReceived = false;   // 🔥 reset snapshot
                    // 🔥 MASTER diventa READY solo dopo richiesta schema
                    if (schemaRequested && peer.isOnline() && !schema.isTxActive()) {
                        setState(BridgeState::READY);
                    }

                } else {
                    if (!helloSent) {
                        sendSchemaRequest();
                        helloSent = true;
                        helloTime = now;
                        setState(BridgeState::WAIT_SCHEMA);
                    }
                }
                break;

            case BridgeState::REQUEST_SCHEMA:
                if (!isMaster) {
                    sendSchemaRequest();
                    setState(BridgeState::WAIT_SCHEMA);
                }
                break;

            case BridgeState::WAIT_SCHEMA:
                if (!isMaster) {
                    if (schemaReceived) {
                        // schema ricevuto e ricostruito → si può allineare
                        setState(BridgeState::ALIGNING);
                    } else if (!schema.isRxActive() && (now - helloTime > HELLO_TIMEOUT)) {
                        LOG_IF("BRIDGE", "Timeout schema → retry");
                        helloSent = false;
                        setState(BridgeState::CONNECTING);
                    }

                }
                break;

            case BridgeState::ALIGNING:
                if (!isMaster && !schemaReceived) {   // <--- added !isMaster
                    // safety: do not align without schema
                    LOG_WF("BRIDGE", "ALIGNING without schemaReceived → forcing CONNECTING");
                    setState(BridgeState::CONNECTING);
                    break;
                }
                onAligned();
                setState(BridgeState::READY);
                break;


            case BridgeState::READY:
                if (!isMaster && !schemaReceived) {   // <--- added !isMaster
                    LOG_WF("BRIDGE", "READY without schemaReceived → blocking DATA_EXCHANGE");
                    setState(BridgeState::CONNECTING);
                    break;
                }
                setState(BridgeState::DATA_EXCHANGE);
                break;

            case BridgeState::DATA_EXCHANGE: {
                static unsigned long lastOnline = 0;
                static unsigned long lastPing   = 0;
                static unsigned long lastAEEtx  = 0;

                const unsigned long PING_INTERVAL    = 20000;
                const unsigned long OFFLINE_TIMEOUT  = PING_INTERVAL * 3;
                const unsigned long AEE_TX_INTERVAL  = 1000;

                // 🔥 MASTER: invia snapshot iniziale SOLO al primo ingresso in DATA_EXCHANGE
                static bool initialSnapshotSent = false;
                if (isMaster && schemaRequested && !initialSnapshotSent) {
                    LOG_IF("MASTER::AEE", "Snapshot iniziale inviato dopo schema COMPLETO");

                    reg->forceAllChanged(now);
                    initialSnapshotSent = true;
                    schemaRequested = false;
                }

                // ============================================================
                // 1) AEE: invio SOLO quando serve
                // ============================================================
                if (reg->hasChanges()) {
                    LOG_IF("BRIDGE::AEE::PENDING",
                        "AEE pending → hasChanges=1 (now=%lu)", now);
                }

                if (reg->hasChanges() && (now - lastAEEtx > AEE_TX_INTERVAL)) {
                    AEEProtocolEngine::send(*reg, ack, aee, [&](const String& out){
                        //LOG_IF("BRIDGE::AEE::PAYLOAD", "%s", out.c_str());
                        transport->send(out);
                    });

                    lastAEEtx = now;
                }

                // ============================================================
                // 2) PING periodico
                // ============================================================
                if (now - lastPing > PING_INTERVAL) {
                    //LOG_IF("BRIDGE::PING::TX",
                    //    "Sending ping (now=%lu)", now);
                    transport->send("{\"ping\":1}");
                    lastPing = now;
                }

                // ============================================================
                // 3) Monitor online/offline
                // ============================================================
                if (peer.isOnline()) {
                    lastOnline = now;
                } else if (now - lastOnline > OFFLINE_TIMEOUT) {

                    if (!isMaster) {
                        LOG_IF("BRIDGE",
                            "SLAVE: master offline → restart handshake");

                        schemaReceived = false;
                        schema.rx.reset();
                        setState(BridgeState::CONNECTING);
                        return;
                    }

                    LOG_IF("BRIDGE",
                        "MASTER: peer offline → restart handshake");
                    setState(BridgeState::CONNECTING);
                }

                break;
            }

        }
    }

    void setState(BridgeState s) {
        if (state != s) {
            //LOG_IF("BRIDGE", "STATE %d → %d", (int)state, (int)s);
            state = s;

            // 🔥 reset handshake SLAVE quando si torna in CONNECTING
            if (!isMaster && s == BridgeState::CONNECTING) {
                helloSent = false;
                helloTime = 0;
            }
        }
    }

    // ============================================================
    //  SCHEMA REQUEST (SLAVE)
    // ============================================================
    void sendSchemaRequest() {
        static unsigned long lastSchemaReq = 0;
        const unsigned long SCHEMA_REQ_MIN_INTERVAL = 3000;

        unsigned long now = millis();
        if (now - lastSchemaReq < SCHEMA_REQ_MIN_INTERVAL) {
            return; // evita bombardamento
        }
        lastSchemaReq = now;

        StaticJsonDocument<64> doc;
        doc["request"] = "schema";

        String out;
        serializeJson(doc, out);
        transport->send(out);

        LOG_I("BRIDGE", "Richiesta schema inviata");
    }

    // ============================================================
    //  PACKET PROCESSING
    // ============================================================
    void processIncoming(unsigned long now) {
        String msg;    

        while (transport->receive(msg)) {
            //LOG_DF("BRIDGE::RAW::RX::processIncoming", "RX raw: %s", msg.c_str());

            if (msg.length() < 2)
                continue;

            peer.onPacket(now);

            // --- riconoscimento veloce frame schema ---
            bool isSchemaFrame =
                msg.startsWith("{\"schema_part\"") ||
                msg.startsWith("{\"schema_ack\"");

            // ------------------------------------------------------------
            //  VALIDAZIONE JSON (solo non-schema)
            // ------------------------------------------------------------
            if (!isSchemaFrame) {
                if (msg[0] != '{' || msg[msg.length()-1] != '}') {
                    LOG_WF("BRIDGE::DROP", "DROP: invalid JSON boundaries: %s", msg.c_str());

                    continue;
                }
            }

            // ------------------------------------------------------------
            //  PARSE JSON
            // ------------------------------------------------------------
            StaticJsonDocument<2048> doc;

            if (isSchemaFrame) {
                StaticJsonDocument<512> schemaDoc;
                auto err = deserializeJson(schemaDoc, msg);
                if (err) {
                    LOG_WF("BRIDGE::SCHEMA", "deserializeJson FALLITA su frame schema: %s", err.c_str());
                    continue;
                }

                if (schema.handleRx(schemaDoc, now))
                    continue;

                continue;
            }

            auto err = deserializeJson(doc, msg);
            if (err) {
                LOG_WF("SLAVE::AEE", "deserializeJson FALLITA: %s", err.c_str());
                continue;
            }

            if (schema.handleRx(doc, now))
                continue;

            // richiesta schema
            if (doc.containsKey("request") && doc["request"] == "schema") {
                if (isMaster)
                    onSchemaRequest();

                continue;
            }

            BridgeProtocol::handleIncoming(msg, *reg, *transport, aee, this);
            AEEProtocolEngine::onPacket(msg, ack);
        }
    }

    // ============================================================
    //  RESEND
    // ============================================================
    void handleResend(unsigned long now) {
        AEEProtocolEngine::checkResend(
            *reg, ack, now, aee,
            [&](const String& out){ transport->send(out); }
        );
    }
};

// ============================================================
//  BRIDGE PROTOCOL (AEE + ACK)
// ============================================================
bool BridgeProtocol::handleIncoming(
    const String& msg,
    AEERegistry& reg,
    ICommandTransport& transport,
    AEEProtocol& proto,
    DMBridge* parent
) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg)) return false;

    bool ok = proto.parseJSON(msg, reg);
    if (!ok) return false;

    unsigned long now = millis();
    reg.forEach([&](AEEVariableBase* v){
        if (v->lastChange != 0) {
            // callback interna (override)
            parent->onAEEVariableChanged(v, now);

            // callback frontend (registrata dall’utente)
            if (parent->onFrontendAEEChange)
                parent->onFrontendAEEChange(v, now);
            
            v->lastChange = 0;   // 🔥 reset dopo notifica
        }
    });

    if (doc.containsKey("seq")) {
        uint32_t seq = doc["seq"];
        StaticJsonDocument<64> ack;
        ack["ack"] = seq;

        String out;
        serializeJson(ack, out);
        transport.send(out);
    }

    return true;
}


// ============================================================
//  MASTER IMPLEMENTATION
// ============================================================
class DMMasterBridge : public DMBridge {
public:
    DMMasterBridge() {  }

protected:
    void onSchemaRequest() override {
        static String cachedSchema;
        static unsigned long lastBuild = 0;
        const unsigned long CACHE_TIME = 10000; // 10 secondi
        
        schemaRequested = true;   // 🔥 SLAVE ha chiesto lo schema

        // 🔥 Se c'è una TX attiva, abortiscila e riparti da zero
        if (schema.tx.active) {
            LOG_IF("MASTER::AEE",
                   "Schema request while TX active → abort & restart");
            schema.abortTx();
        }

        // Ricostruisci lo schema solo se necessario
        if (cachedSchema.length() == 0 || millis() - lastBuild > CACHE_TIME) {
            DynamicJsonDocument doc(4096);
            JsonArray arr = doc.createNestedArray("schema");

            aee.ExportSchema(*reg, arr);

            cachedSchema = "";
            serializeJson(doc, cachedSchema);

            lastBuild = millis();

            LOG_IF("MASTER::AEE", "Schema ricostruito (%u vars)", reg->size());
        } else {
            LOG_IF("MASTER::AEE", "Schema inviato (cached)");
        }

        //Invia schema
        schema.startTx(cachedSchema);
    }


};

#endif
