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

#include "DMTransport.hpp"


#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class DMBridge;   // forward
class BridgeProtocol {
public:
    static bool handleIncoming(
        JsonDocument& doc,
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

    
    static void send(
        AEERegistry& reg,
        AckState& state,
        AEEProtocol& proto,
        SendCallback sendFn)
    {
        const uint32_t seq = state.nextSeq();

        String out = proto.serializeChangedJSON(reg, seq);

        if (out.length() == 0)
            return;

        LOG_IF(
            "BRIDGE::AEE::TX",
            "Sending AEE update: %s",
            out.c_str()
        );

        sendFn(out);

        state.lastSentSeq = seq;
        state.lastSendTime = millis();

        reg.clearAllChanged();
    }

    static void onPacket(
        JsonDocument& doc,
        AckState& state)
    {
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
    bool lastPeerOnline = false;

    bool helloSent = false;
    unsigned long helloTime = 0;
    static constexpr unsigned long HELLO_TIMEOUT = 8000;
    bool schemaRequested = false;
    bool schemaReceived  = false;

    IBridgePacer* pacer = nullptr;

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

            // indice del frame corrente
            int index = 0;

            // frame per cui stiamo aspettando ACK (-1 = nessuno)
            int waitingAckFor = -1;

            // timestamp ultimo invio
            unsigned long lastSend = 0;
            unsigned long startTime = 0;

            // vettore dei frame JSON pronti da inviare
            std::vector<String> frames;

            void reset() {
                active = false;
                index = 0;
                waitingAckFor = -1;
                lastSend = 0;
                startTime = 0;
                frames.clear();
            }
        } tx;


        // RX (SLAVE)
        struct RxState {
            bool active = false;
            uint16_t expected = 0;
            uint16_t received = 0;

            char buffer[8192];
            uint16_t offset = 0;

            unsigned long startTs = 0;

            void reset() {
                active = false;
                expected = 0;
                received = 0;
                offset = 0;
                startTs = 0;
                buffer[0] = 0;
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
            const unsigned long now = millis();

            // reset stato TX
            tx.reset();
            tx.frames.clear();
            tx.index = 0;
            tx.active = true;

            // Timestamp dell'inizio della TX completa.
            tx.startTime = now;

            size_t total = (fullSchema.length() + MAX_CHUNK - 1) / MAX_CHUNK;
            tx.frames.reserve(total);   // 🔥 evita realloc

            for (size_t i = 0; i < total; i++) {
                size_t start = i * MAX_CHUNK;
                size_t len   = min(MAX_CHUNK, fullSchema.length() - start);

                // 🔥 buffer statico → zero allocazioni
                char chunkBuf[MAX_CHUNK + 1];
                memcpy(chunkBuf, fullSchema.c_str() + start, len);
                chunkBuf[len] = 0;

                StaticJsonDocument<256> doc;
                doc["schema_part"] = (int)i;
                doc["total"]       = (int)total;
                doc["data"]        = chunkBuf;

                String out;
                serializeJson(doc, out);
                tx.frames.push_back(out);
            }

            LOG_IF("BRIDGE::SCHEMA",
                "TX multiframe avviato (%u frames)",
                tx.frames.size());
        }

        // ------------------------------------------------------------
        // MASTER: invio frame + gestione ACK
        // ------------------------------------------------------------
        void handleTx(unsigned long now) {
            if (!parent->isMaster) return;
            if (!tx.active) return;

            // completato
            if (tx.index >= tx.frames.size()) {
                LOG_IF("BRIDGE::SCHEMA", "TX multiframe COMPLETATO");
                tx.active = false;
                return;
            }

            // attesa ACK
            if (tx.waitingAckFor >= 0) {

                // 🔥 timeout dinamico
                unsigned long ackTimeout =
                    parent->peer.isOnline() ? 400 : 900;

                if (now - tx.lastSend > ackTimeout) {
                    transport->send(tx.frames[tx.index]);
                    tx.lastSend = now;

                    LOG_WF("BRIDGE::SCHEMA",
                        "ACK timeout frame %d → ritrasmissione",
                        tx.waitingAckFor);
                }
                return;
            }

            // 🔥 pacing dinamico
            unsigned long interval =
                parent->peer.isOnline() ? 10 : 25;

            if (now - tx.lastSend < interval)
                return;

            // invia frame corrente
            String& frame = tx.frames[tx.index];
            transport->send(frame);
            tx.lastSend = now;

            // 🔥 niente più fid → usa schema_part
            StaticJsonDocument<64> doc;
            deserializeJson(doc, frame);
            tx.waitingAckFor = doc["schema_part"];

            // 🔥 log rate-limit
            static unsigned long lastLog = 0;
            if (now - lastLog > 200) {
                LOG_IF("BRIDGE::SCHEMA::TX",
                    "Frame %u inviato (part=%d)",
                    tx.index + 1,
                    tx.waitingAckFor);
                lastLog = now;
            }
        }

        // ------------------------------------------------------------
        // SLAVE: ricezione frame + ACK
        // ------------------------------------------------------------
        bool handleRx(const JsonDocument& doc, unsigned long now) {

            // ------------------------------------------------------------
            // ACK schema (SLAVE → MASTER)
            // ------------------------------------------------------------
            if (doc.containsKey("schema_ack")) {
                int ack = doc["schema_ack"];

                if (parent->isMaster && tx.waitingAckFor == ack) {
                    tx.waitingAckFor = -1;
                    tx.index++;
                }
                return true;
            }

            // ------------------------------------------------------------
            // Non è un frame schema
            // ------------------------------------------------------------
            if (!doc.containsKey("schema_part"))
                return false;

            // ------------------------------------------------------------
            // MASTER non riceve mai schema_part
            // ------------------------------------------------------------
            if (parent->isMaster)
                return true;

            int part  = doc["schema_part"];
            int total = doc["total"];
            const char* data = doc["data"];

            // ------------------------------------------------------------
            // Nuovo frame 0 mentre RX è attivo → IGNORA, NON RESETTA
            // ------------------------------------------------------------
            if (part == 0 && rx.active && rx.received > 0) {
                LOG_WF("BRIDGE::SCHEMA",
                    "New frame 0 received while RX in progress → ignored");
                return true;
            }

            // ------------------------------------------------------------
            // Avvio sessione RX
            // ------------------------------------------------------------
            if (!rx.active) {

                if (part != 0) {
                    LOG_WF("BRIDGE::SCHEMA",
                        "First frame must be 0, got %d → ignoring", part);
                    return true;
                }

                rx.active   = true;
                rx.expected = total;
                rx.received = 0;
                rx.offset   = 0;
                rx.startTs  = now;

                LOG_IF("BRIDGE::SCHEMA",
                    "RX multiframe started (%d parts)", total);
            }

            // ------------------------------------------------------------
            // Frame fuori ordine → IGNORA, NON RESETTA
            // ------------------------------------------------------------
            if (part != rx.received) {
                LOG_WF("BRIDGE::SCHEMA",
                    "Out-of-order frame: expected %u, got %u → ignored",
                    rx.received, part);
                return true;
            }

            // ------------------------------------------------------------
            // Append chunk (buffer statico, zero allocazioni)
            // ------------------------------------------------------------
            size_t len = strlen(data);

            if (rx.offset + len >= sizeof(rx.buffer)) {
                LOG_WF("BRIDGE::SCHEMA",
                    "RX buffer overflow → reset");
                rx.reset();
                return true;
            }

            memcpy(rx.buffer + rx.offset, data, len);
            rx.offset += len;

            rx.received++;

            // ------------------------------------------------------------
            // ACK frame
            // ------------------------------------------------------------
            StaticJsonDocument<64> ack;
            ack["schema_ack"] = part;

            String out;
            serializeJson(ack, out);
            transport->send(out);

            // ------------------------------------------------------------
            // Timeout dinamico
            // ------------------------------------------------------------
            unsigned long timeout =
                parent->peer.isOnline() ? 15000 : 35000;

            if (now - rx.startTs > timeout) {
                LOG_WF("BRIDGE::SCHEMA",
                    "RX multiframe timeout → reset");
                rx.reset();
                return true;
            }

            // ------------------------------------------------------------
            // Completato
            // ------------------------------------------------------------
            if (rx.received == rx.expected) {

                LOG_IF("BRIDGE::SCHEMA",
                    "RX multiframe COMPLETE (len=%u)", rx.offset);

                rx.buffer[rx.offset] = 0;

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
    void setPacer(IBridgePacer& p)
    {
        pacer = &p;
    }

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


    virtual void loop(unsigned long now)
    {
        if (!transport || !reg)
            return;

        // ============================================================
        // RX / STATE MACHINE
        //
        // DEVONO essere sempre eseguiti, indipendentemente
        // dall'arbitraggio TX.
        // ============================================================
        transport->loop(now);
        processIncoming(now);
        runStateMachine(now);
        handleResend(now);

        // ============================================================
        // NETWORK ARBITRATION
        // ============================================================
        bool acquired = true;

        if (pacer)
            acquired = pacer->acquire(now);

        // ============================================================
        // TX
        //
        // Il Bridge trasmette solo se possiede la rete.
        // ============================================================
        if (acquired) {
            schema.handleTx(now);

            // ========================================================
            // RELEASE IMMEDIATO
            // ========================================================
            if (pacer)
                pacer->release(now);
        }

        // ============================================================
        // LOG PEER ONLINE/OFFLINE
        // ============================================================
        const bool online = peer.isOnline();

        if (online != lastPeerOnline) {
            lastPeerOnline = online;
        }
    }

    void sendRaw(const String& payload) {
        if (!transport) return;

        transport->send(payload);
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
                const bool hasChanges = reg->hasChanges();

                if (hasChanges) {
                    LOG_IF("BRIDGE::AEE::PENDING",
                        "AEE pending → hasChanges=1 (now=%lu)", now);
                }

                if (hasChanges && (now - lastAEEtx > AEE_TX_INTERVAL)) {
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
    void processIncoming(unsigned long now)
    {
        String msg;

        while (transport->receive(msg))
        {
            if (msg.length() < 2)
                continue;

            peer.onPacket(now);

            // ------------------------------------------------------------
            // FAST PATH: ping (no JSON parsing)
            // ------------------------------------------------------------
            if (msg.startsWith("{\"ping\""))
                continue;

            // ------------------------------------------------------------
            // FAST PATH: ack (no JSON parsing)
            // ------------------------------------------------------------
            if (msg.startsWith("{\"ack\""))
            {
                StaticJsonDocument<64> ackDoc;
                if (!deserializeJson(ackDoc, msg))
                    AEEProtocolEngine::onPacket(ackDoc, ack);
                continue;
            }

            // ------------------------------------------------------------
            // FAST PATH: schema ACK (no JSON parsing)
            // ------------------------------------------------------------
            if (msg.startsWith("{\"schema_ack\""))
            {
                StaticJsonDocument<64> ackDoc;
                if (!deserializeJson(ackDoc, msg))
                    schema.handleRx(ackDoc, now);
                continue;
            }

            // ------------------------------------------------------------
            // RICONOSCIMENTO FRAME SCHEMA (schema_part)
            // ------------------------------------------------------------
            const bool isSchemaFrame = msg.startsWith("{\"schema_part\"");

            // ------------------------------------------------------------
            // VALIDAZIONE JSON (solo non-schema)
            // ------------------------------------------------------------
            if (!isSchemaFrame)
            {
                if (msg[0] != '{' || msg[msg.length() - 1] != '}')
                {
                    LOG_WF("BRIDGE::DROP",
                        "DROP: invalid JSON boundaries: %s",
                        msg.c_str());
                    continue;
                }
            }

            // ------------------------------------------------------------
            // FRAME SCHEMA (schema_part)
            // ------------------------------------------------------------
            if (isSchemaFrame)
            {
                StaticJsonDocument<512> schemaDoc;
                auto err = deserializeJson(schemaDoc, msg);

                if (err)
                {
                    LOG_WF("BRIDGE::SCHEMA",
                        "deserializeJson FALLITA su frame schema: %s",
                        err.c_str());
                    continue;
                }

                if (schema.handleRx(schemaDoc, now))
                    continue;

                continue;
            }

            // ------------------------------------------------------------
            // JSON NORMALE (AEE, request schema, ecc.)
            // ------------------------------------------------------------
            StaticJsonDocument<2048> doc;
            auto err = deserializeJson(doc, msg);

            if (err)
            {
                LOG_WF("SLAVE::AEE",
                    "deserializeJson FALLITA: %s",
                    err.c_str());
                continue;
            }

            // ------------------------------------------------------------
            // Schema eventualmente ricevuto
            // ------------------------------------------------------------
            if (schema.handleRx(doc, now))
                continue;

            // ------------------------------------------------------------
            // Richiesta schema
            // ------------------------------------------------------------
            if (doc.containsKey("request") &&
                doc["request"] == "schema")
            {
                if (isMaster)
                    onSchemaRequest();
                continue;
            }

            // ------------------------------------------------------------
            // AEE
            // ------------------------------------------------------------
            BridgeProtocol::handleIncoming(
                doc,
                *reg,
                *transport,
                aee,
                this
            );

            // ------------------------------------------------------------
            // ACK
            // ------------------------------------------------------------
            AEEProtocolEngine::onPacket(doc, ack);
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
    JsonDocument& doc,
    AEERegistry& reg,
    ICommandTransport& transport,
    AEEProtocol& proto,
    DMBridge* parent
) {
    // Il JSON è già stato deserializzato da processIncoming().
    // NON effettuare un secondo deserializeJson().

    bool ok = proto.parseJSON(doc, reg);

    if (!ok)
        return false;

    const unsigned long now = millis();

    reg.forEachChanged([&](AEEVariableBase* v)
    {
        parent->onAEEVariableChanged(v, now);

        if (parent->onFrontendAEEChange)
            parent->onFrontendAEEChange(v, now);

        v->lastChange = 0;
    });

    // 🔥 pulizia lista changed
    reg.clearAllChanged();


    if (doc.containsKey("seq"))
    {
        const uint32_t seq = doc["seq"];

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
class DMMasterBridge : public DMBridge
{
public:

    DMMasterBridge() {}

protected:

    void onSchemaRequest() override
    {
        static String cachedSchema;
        static unsigned long lastBuild = 0;

        static constexpr unsigned long CACHE_TIME = 10000;
        static constexpr unsigned long TX_TIMEOUT  = 5000;

        const unsigned long now = millis();

        schemaRequested = true;

        // ============================================================
        // TX SCHEMA GIÀ ATTIVA
        // ============================================================

        if (schema.tx.active)
        {
            const unsigned long elapsed =
                static_cast<unsigned long>(
                    now - schema.tx.startTime
                );

            // --------------------------------------------------------
            // TX ancora valida:
            // non interrompiamo il trasferimento.
            // --------------------------------------------------------

            if (elapsed < TX_TIMEOUT)
            {
                LOG_IF(
                    "MASTER::AEE",
                    "Schema request while TX active "
                    "(%lu ms) → richiesta ignorata",
                    elapsed
                );

                return;
            }

            // --------------------------------------------------------
            // TX bloccata:
            // reset e ripartenza.
            // --------------------------------------------------------

            LOG_WF(
                "MASTER::AEE",
                "Schema TX timeout (%lu ms) "
                "→ abort & restart",
                elapsed
            );

            schema.abortTx();
        }

        // ============================================================
        // CACHE SCHEMA
        // ============================================================

        if (
            cachedSchema.length() == 0 ||
            static_cast<unsigned long>(
                now - lastBuild
            ) > CACHE_TIME
        )
        {
            DynamicJsonDocument doc(4096);

            JsonArray arr =
                doc.createNestedArray("schema");

            aee.ExportSchema(*reg, arr);

            cachedSchema = "";

            serializeJson(
                doc,
                cachedSchema
            );

            lastBuild = now;

            LOG_IF(
                "MASTER::AEE",
                "Schema ricostruito (%u vars)",
                reg->size()
            );
        }
        else
        {
            LOG_IF(
                "MASTER::AEE",
                "Schema inviato (cached)"
            );
        }

        // ============================================================
        // START TX
        // ============================================================

        schema.startTx(cachedSchema);
    }
};

#endif
