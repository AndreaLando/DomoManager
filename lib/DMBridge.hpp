#ifndef DMBRIDGE_HPP
#define DMBRIDGE_HPP

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it

   Versione modulo: 1.1.0
   Ultima modifica: 2026-11-08

   Note:
                    - Stato Bridge reso per istanza.
                    - Snapshot iniziale riarmabile.
                    - Cache schema per istanza.
                    - Resend AEE con stesso payload e stesso sequence.
                    - Gestione ACK duplicati schema.

   ============================================================================ */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>

#include "DMTransport.hpp"
#include "DMAEECore.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


class DMBridge;


// ============================================================
// BRIDGE PROTOCOL
// ============================================================

class BridgeProtocol
{
public:
    static bool handleIncoming(
        JsonDocument& document,
        AEERegistry& registry,
        ICommandTransport& transport,
        AEEProtocol& protocol,
        DMBridge* parent
    );
};


// ============================================================
// PEER MONITOR
// ============================================================

class BridgePeerMonitor
{
private:
    unsigned long lastActivity = 0;
    bool online = false;

public:
    void reset()
    {
        lastActivity = 0;
        online = false;
    }

    void onPacket(
        unsigned long now)
    {
        lastActivity = now;

        if (!online)
        {
            online = true;

            LOG_IF(
                "BRIDGE",
                "Peer ONLINE"
            );
        }
    }

    void checkOffline(
        unsigned long now,
        unsigned long timeoutMs = 120000UL)
    {
        if (
            online &&
            now - lastActivity > timeoutMs
        )
        {
            online = false;

            LOG_IF(
                "BRIDGE",
                "Peer OFFLINE"
            );
        }
    }

    bool isOnline() const
    {
        return online;
    }

    unsigned long getLastActivity() const
    {
        return lastActivity;
    }
};


// ============================================================
// AEE PROTOCOL ENGINE
// ============================================================

class AEEProtocolEngine
{
public:
    using SendCallback =
        std::function<void(const String&)>;

    // ========================================================
    // ACK STATE
    // ========================================================

    struct AckState
    {
        uint32_t seqCounter = 0;

        uint32_t lastSentSeq = 0;
        uint32_t lastAckSeq = 0;

        unsigned long lastSendTime = 0;

        /*
         * Il payload deve essere conservato dopo l'invio.
         *
         * In caso di timeout deve essere ritrasmesso esattamente
         * lo stesso contenuto e con lo stesso seq.
         */
        String pendingPayload;

        void reset()
        {
            seqCounter = 0;

            lastSentSeq = 0;
            lastAckSeq = 0;

            lastSendTime = 0;

            pendingPayload = "";
        }

        uint32_t nextSeq()
        {
            return ++seqCounter;
        }

        bool hasPendingPayload() const
        {
            return pendingPayload.length() > 0;
        }

        bool ackOk() const
        {
            if (!hasPendingPayload())
                return true;

            return
                lastAckSeq ==
                lastSentSeq;
        }

        bool waitingAck() const
        {
            return
                hasPendingPayload() &&
                !ackOk();
        }

        bool ackTimeout(
            unsigned long now,
            unsigned long timeoutMs) const
        {
            return
                waitingAck() &&
                now - lastSendTime >
                    timeoutMs;
        }

        void commitAck(
            uint32_t sequence)
        {
            lastAckSeq = sequence;

            if (
                lastAckSeq ==
                lastSentSeq
            )
            {
                pendingPayload = "";
            }
        }
    };

    // ========================================================
    // NEW SEND
    // ========================================================

    static bool send(
        AEERegistry& registry,
        AckState& state,
        AEEProtocol& protocol,
        const SendCallback& sendFunction)
    {
        /*
         * Non generare un secondo pacchetto mentre il precedente
         * è ancora in attesa di ACK.
         */
        if (state.waitingAck())
            return false;

        const uint32_t sequence =
            state.nextSeq();

        String output =
            protocol.serializeChangedJSON(
                registry,
                sequence
            );

        if (output.length() == 0)
        {
            LOG_WF(
                "BRIDGE::AEE::TX",
                "Payload AEE vuoto: seq=%lu pending=%u",
                static_cast<unsigned long>(
                    sequence
                ),
                registry.hasChanges()
                    ? 1U
                    : 0U
            );

            return false;
        }

        LOG_IF(
            "BRIDGE::AEE::TX",
            "Sending AEE update: %s",
            output.c_str()
        );

        /*
         * Memorizza prima di inviare.
         *
         * In questo modo, anche se subito dopo cambia il registry,
         * il resend rimane deterministico.
         */
        state.pendingPayload =
            output;

        state.lastSentSeq =
            sequence;

        state.lastSendTime =
            millis();

        sendFunction(
            state.pendingPayload
        );

        /*
         * Il payload è ora posseduto da AckState.
         * Il registry può quindi essere pulito.
         */
        registry.clearAllChanged();

        return true;
    }

    // ========================================================
    // ACK RECEIVE
    // ========================================================

    static void onPacket(
        JsonDocument& document,
        AckState& state)
    {
        if (!document.containsKey("ack"))
            return;

        const uint32_t sequence =
            document["ack"];

        state.commitAck(
            sequence
        );
    }

    // ========================================================
    // RESEND
    // ========================================================

    static bool checkResend(
        AckState& state,
        unsigned long now,
        const SendCallback& sendFunction)
    {
        static constexpr unsigned long
            RESEND_TIMEOUT = 120UL;

        static constexpr unsigned long
            MIN_INTERVAL = 40UL;

        if (!state.hasPendingPayload())
            return false;

        if (
            !state.ackTimeout(
                now,
                RESEND_TIMEOUT
            )
        )
        {
            return false;
        }

        if (
            now - state.lastSendTime <
            MIN_INTERVAL
        )
        {
            return false;
        }

        LOG_WF(
            "BRIDGE::AEE::TX",
            "ACK timeout seq=%lu -> resend stesso payload",
            static_cast<unsigned long>(
                state.lastSentSeq
            )
        );

        sendFunction(
            state.pendingPayload
        );

        state.lastSendTime =
            now;

        return true;
    }
};


// ============================================================
// BRIDGE STATE
// ============================================================

enum class BridgeState
{
    DISCONNECTED,
    CONNECTING,
    REQUEST_SCHEMA,
    WAIT_SCHEMA,
    ALIGNING,
    READY,
    DATA_EXCHANGE
};


// ============================================================
// DMBridge
// ============================================================

class DMBridge
{
protected:
    // ========================================================
    // SESSION STATE
    // ========================================================

    struct SessionState
    {
        bool helloSent = false;
        unsigned long helloTime = 0;

        bool schemaRequested = false;
        bool schemaReceived = false;

        bool initialSnapshotPending = false;
        bool initialSnapshotSent = false;

        void reset()
        {
            helloSent = false;
            helloTime = 0;

            schemaRequested = false;
            schemaReceived = false;

            initialSnapshotPending = false;
            initialSnapshotSent = false;
        }

        void armInitialSnapshot()
        {
            schemaRequested = true;

            initialSnapshotPending = true;
            initialSnapshotSent = false;
        }

        void completeInitialSnapshot()
        {
            schemaRequested = false;

            initialSnapshotPending = false;
            initialSnapshotSent = true;
        }

        void resetSnapshot()
        {
            initialSnapshotPending = false;
            initialSnapshotSent = false;
        }
    };


    // ========================================================
    // TIMING STATE
    // ========================================================

    struct TimingState
    {
        unsigned long lastOnline = 0;
        unsigned long lastPing = 0;
        unsigned long lastAEEtx = 0;
        unsigned long lastSchemaRequest = 0;
        unsigned long lastSchemaTxLog = 0;

        void reset()
        {
            lastOnline = 0;
            lastPing = 0;
            lastAEEtx = 0;
            lastSchemaRequest = 0;
            lastSchemaTxLog = 0;
        }
    };


    // ========================================================
    // CORE
    // ========================================================

    AEEProtocol aee;

    ICommandTransport* transport =
        nullptr;

    AEERegistry* reg =
        nullptr;

    bool isMaster =
        true;

    BridgeState state =
        BridgeState::DISCONNECTED;

    BridgePeerMonitor peer;

    AEEProtocolEngine::AckState ack;

    SessionState session;
    TimingState timing;

    bool lastPeerOnline =
        false;

    IBridgePacer* pacer =
        nullptr;

    static constexpr unsigned long
        HELLO_TIMEOUT = 8000UL;


    // ========================================================
    // SCHEMA MULTIFRAME
    // ========================================================

    class SchemaMultiframe
    {
    public:
        // ====================================================
        // TX STATE
        // ====================================================

        struct TxState
        {
            bool active = false;

            size_t index = 0;

            int waitingAckFor = -1;

            unsigned long lastSend = 0;
            unsigned long startTime = 0;

            std::vector<String> frames;

            void reset()
            {
                active = false;

                index = 0;

                waitingAckFor = -1;

                lastSend = 0;
                startTime = 0;

                frames.clear();
            }
        };


        // ====================================================
        // RX STATE
        // ====================================================

        struct RxState
        {
            bool active = false;

            uint16_t expected = 0;
            uint16_t received = 0;

            char buffer[8192];

            uint16_t offset = 0;

            unsigned long startTs = 0;

            void reset()
            {
                active = false;

                expected = 0;
                received = 0;

                offset = 0;

                startTs = 0;

                buffer[0] = '\0';
            }
        };


        DMBridge* parent =
            nullptr;

        ICommandTransport* transport =
            nullptr;

        TxState tx;
        RxState rx;


        // ====================================================
        // INIT
        // ====================================================

        void init(
            DMBridge* bridge,
            ICommandTransport* commandTransport)
        {
            parent =
                bridge;

            transport =
                commandTransport;
        }


        // ====================================================
        // STATUS
        // ====================================================

        bool isTxActive() const
        {
            return tx.active;
        }

        bool isRxActive() const
        {
            return rx.active;
        }


        // ====================================================
        // RESET
        // ====================================================

        void reset()
        {
            tx.reset();
            rx.reset();
        }

        void abortTx()
        {
            if (!tx.active)
                return;

            LOG_WF(
                "BRIDGE::SCHEMA",
                "Abort TX multiframe -> reset"
            );

            tx.reset();
        }

        void restartTx(
            const String& fullSchema)
        {
            tx.reset();

            startTx(
                fullSchema
            );
        }


        // ====================================================
        // SEND SCHEMA ACK
        // ====================================================

        void sendAck(
            int part)
        {
            if (!transport)
                return;

            StaticJsonDocument<64> document;

            document["schema_ack"] =
                part;

            String output;

            serializeJson(
                document,
                output
            );

            transport->send(
                output
            );
        }


        // ====================================================
        // START TX
        // ====================================================

        void startTx(
            const String& fullSchema)
        {
            static constexpr size_t
                MAX_CHUNK = 160;

            const unsigned long now =
                millis();

            tx.reset();

            tx.active = true;
            tx.startTime = now;

            const size_t total =
                (
                    fullSchema.length() +
                    MAX_CHUNK -
                    1
                ) /
                MAX_CHUNK;

            tx.frames.reserve(
                total
            );

            for (
                size_t index = 0;
                index < total;
                ++index
            )
            {
                const size_t start =
                    index *
                    MAX_CHUNK;

                const size_t remaining =
                    fullSchema.length() -
                    start;

                const size_t length =
                    min(
                        MAX_CHUNK,
                        remaining
                    );

                char chunkBuffer[
                    MAX_CHUNK + 1
                ];

                memcpy(
                    chunkBuffer,
                    fullSchema.c_str() +
                        start,
                    length
                );

                chunkBuffer[length] =
                    '\0';

                StaticJsonDocument<256>
                    document;

                document["schema_part"] =
                    static_cast<int>(
                        index
                    );

                document["total"] =
                    static_cast<int>(
                        total
                    );

                document["data"] =
                    chunkBuffer;

                String output;

                serializeJson(
                    document,
                    output
                );

                tx.frames.push_back(
                    output
                );
            }

            LOG_IF(
                "BRIDGE::SCHEMA",
                "TX multiframe avviato (%u frames)",
                static_cast<unsigned>(
                    tx.frames.size()
                )
            );
        }


        // ====================================================
        // HANDLE TX
        // ====================================================

        void handleTx(
            unsigned long now)
        {
            if (!parent ||
                !transport)
            {
                return;
            }

            if (!parent->isMaster)
                return;

            if (!tx.active)
                return;

            // ------------------------------------------------
            // COMPLETATO
            // ------------------------------------------------

            if (
                tx.index >=
                tx.frames.size()
            )
            {
                LOG_IF(
                    "BRIDGE::SCHEMA",
                    "TX multiframe COMPLETATO"
                );

                tx.active = false;
                tx.waitingAckFor = -1;

                return;
            }

            // ------------------------------------------------
            // ATTESA ACK
            // ------------------------------------------------

            if (tx.waitingAckFor >= 0)
            {
                const unsigned long
                    ackTimeout =
                        parent
                            ->peer
                            .isOnline()
                                ? 400UL
                                : 900UL;

                if (
                    now - tx.lastSend >
                    ackTimeout
                )
                {
                    transport->send(
                        tx.frames[
                            tx.index
                        ]
                    );

                    tx.lastSend =
                        now;

                    LOG_WF(
                        "BRIDGE::SCHEMA",
                        "ACK timeout frame %d -> ritrasmissione",
                        tx.waitingAckFor
                    );
                }

                return;
            }

            // ------------------------------------------------
            // PACING
            // ------------------------------------------------

            const unsigned long interval =
                parent
                    ->peer
                    .isOnline()
                        ? 10UL
                        : 25UL;

            if (
                now - tx.lastSend <
                interval
            )
            {
                return;
            }

            // ------------------------------------------------
            // SEND CURRENT FRAME
            // ------------------------------------------------

            String& frame =
                tx.frames[
                    tx.index
                ];

            transport->send(
                frame
            );

            tx.lastSend =
                now;

            StaticJsonDocument<64>
                document;

            const DeserializationError error =
                deserializeJson(
                    document,
                    frame
                );

            if (error)
            {
                LOG_WF(
                    "BRIDGE::SCHEMA",
                    "Errore lettura schema_part TX: %s",
                    error.c_str()
                );

                tx.reset();

                return;
            }

            tx.waitingAckFor =
                document["schema_part"];

            if (
                now -
                    parent
                        ->timing
                        .lastSchemaTxLog >
                200UL
            )
            {
                LOG_IF(
                    "BRIDGE::SCHEMA::TX",
                    "Frame %u inviato (part=%d)",
                    static_cast<unsigned>(
                        tx.index + 1
                    ),
                    tx.waitingAckFor
                );

                parent
                    ->timing
                    .lastSchemaTxLog =
                        now;
            }
        }


        // ====================================================
        // HANDLE RX
        // ====================================================

        bool handleRx(
            const JsonDocument& document,
            unsigned long now)
        {
            if (!parent ||
                !transport)
            {
                return false;
            }

            // ------------------------------------------------
            // SCHEMA ACK
            // ------------------------------------------------

            if (
                document.containsKey(
                    "schema_ack"
                )
            )
            {
                const int acknowledgedPart =
                    document["schema_ack"];

                if (
                    parent->isMaster &&
                    tx.waitingAckFor ==
                        acknowledgedPart
                )
                {
                    tx.waitingAckFor =
                        -1;

                    tx.index++;
                }

                return true;
            }

            // ------------------------------------------------
            // NON È UNO SCHEMA FRAME
            // ------------------------------------------------

            if (
                !document.containsKey(
                    "schema_part"
                )
            )
            {
                return false;
            }

            /*
             * Il Master non deve ricevere schema_part.
             * Consideriamo comunque il frame gestito.
             */
            if (parent->isMaster)
                return true;

            const int part =
                document["schema_part"];

            const int total =
                document["total"];

            const char* data =
                document["data"];

            if (
                part < 0 ||
                total <= 0 ||
                !data
            )
            {
                LOG_WF(
                    "BRIDGE::SCHEMA",
                    "Frame schema non valido"
                );

                return true;
            }

            // ------------------------------------------------
            // FRAME DUPLICATO
            // ------------------------------------------------

            if (
                rx.active &&
                part <
                    static_cast<int>(
                        rx.received
                    )
            )
            {
                sendAck(
                    part
                );

                LOG_DF(
                    "BRIDGE::SCHEMA",
                    "Duplicate frame %d -> ACK repeated",
                    part
                );

                return true;
            }

            // ------------------------------------------------
            // AVVIO RX
            // ------------------------------------------------

            if (!rx.active)
            {
                if (part != 0)
                {
                    LOG_WF(
                        "BRIDGE::SCHEMA",
                        "First frame must be 0, got %d -> ignored",
                        part
                    );

                    return true;
                }

                rx.active = true;

                rx.expected =
                    static_cast<uint16_t>(
                        total
                    );

                rx.received = 0;
                rx.offset = 0;

                rx.startTs = now;

                LOG_IF(
                    "BRIDGE::SCHEMA",
                    "RX multiframe started (%d parts)",
                    total
                );
            }

            // ------------------------------------------------
            // TOTAL COERENCE
            // ------------------------------------------------

            if (
                total !=
                static_cast<int>(
                    rx.expected
                )
            )
            {
                LOG_WF(
                    "BRIDGE::SCHEMA",
                    "Schema total mismatch: expected=%u got=%d",
                    rx.expected,
                    total
                );

                return true;
            }

            // ------------------------------------------------
            // FRAME FUORI ORDINE
            // ------------------------------------------------

            if (
                part !=
                static_cast<int>(
                    rx.received
                )
            )
            {
                LOG_WF(
                    "BRIDGE::SCHEMA",
                    "Out-of-order frame: expected=%u got=%d -> ignored",
                    rx.received,
                    part
                );

                if (rx.received > 0)
                {
                    sendAck(
                        rx.received - 1
                    );
                }

                return true;
            }

            // ------------------------------------------------
            // APPEND DATA
            // ------------------------------------------------

            const size_t length =
                strlen(
                    data
                );

            if (
                rx.offset + length >=
                sizeof(rx.buffer)
            )
            {
                LOG_WF(
                    "BRIDGE::SCHEMA",
                    "RX buffer overflow -> reset"
                );

                rx.reset();

                return true;
            }

            memcpy(
                rx.buffer +
                    rx.offset,
                data,
                length
            );

            rx.offset +=
                static_cast<uint16_t>(
                    length
                );

            rx.received++;

            sendAck(
                part
            );

            // ------------------------------------------------
            // TIMEOUT
            // ------------------------------------------------

            const unsigned long timeout =
                parent
                    ->peer
                    .isOnline()
                        ? 15000UL
                        : 35000UL;

            if (
                now - rx.startTs >
                timeout
            )
            {
                LOG_WF(
                    "BRIDGE::SCHEMA",
                    "RX multiframe timeout -> reset"
                );

                rx.reset();

                return true;
            }

            // ------------------------------------------------
            // COMPLETE
            // ------------------------------------------------

            if (
                rx.received ==
                rx.expected
            )
            {
                rx.buffer[
                    rx.offset
                ] = '\0';

                LOG_IF(
                    "BRIDGE::SCHEMA",
                    "RX multiframe COMPLETE (len=%u)",
                    rx.offset
                );

                StaticJsonDocument<8192>
                    schemaDocument;

                const DeserializationError error =
                    deserializeJson(
                        schemaDocument,
                        rx.buffer
                    );

                if (error)
                {
                    LOG_WF(
                        "BRIDGE::SCHEMA",
                        "Multiframe JSON error: %s",
                        error.c_str()
                    );

                    rx.reset();

                    return true;
                }

                JsonArray schemaArray =
                    schemaDocument[
                        "schema"
                    ].as<JsonArray>();

                parent->onSchemaReceived(
                    schemaArray
                );

                parent
                    ->session
                    .schemaReceived =
                        true;

                parent->setState(
                    BridgeState::ALIGNING
                );

                rx.reset();
            }

            return true;
        }


        // ====================================================
        // EXTERNAL RX TIMEOUT CHECK
        // ====================================================

        void checkRxTimeout(
            unsigned long now)
        {
            if (!rx.active ||
                !parent)
            {
                return;
            }

            const unsigned long timeout =
                parent
                    ->peer
                    .isOnline()
                        ? 15000UL
                        : 35000UL;

            if (
                now - rx.startTs <=
                timeout
            )
            {
                return;
            }

            LOG_WF(
                "BRIDGE::SCHEMA",
                "RX multiframe timeout esterno -> reset"
            );

            rx.reset();
        }
    };


    SchemaMultiframe schema;


public:
    DMBridge() = default;

    virtual ~DMBridge() = default;

    DMBridge(
        const DMBridge&) = delete;

    DMBridge& operator=(
        const DMBridge&) = delete;


    // ========================================================
    // PACER
    // ========================================================

    void setPacer(
        IBridgePacer& bridgePacer)
    {
        pacer =
            &bridgePacer;
    }


    // ========================================================
    // INIT
    // ========================================================

    void init(
        ICommandTransport* commandTransport,
        AEERegistry& registry)
    {
        init(
            commandTransport,
            registry,
            true
        );
    }

    void init(
        ICommandTransport* commandTransport,
        AEERegistry& registry,
        bool master)
    {
        isMaster =
            master;

        transport =
            commandTransport;

        reg =
            &registry;

        state =
            BridgeState::DISCONNECTED;

        peer.reset();
        ack.reset();

        session.reset();
        timing.reset();

        schema.reset();

        aee =
            AEEProtocol(
                master
            );

        schema.init(
            this,
            transport
        );

        if (!transport)
        {
            LOG_EF(
                "BRIDGE",
                "Transport NULL"
            );

            return;
        }

        transport->begin();

        setState(
            BridgeState::CONNECTING
        );

        LOG_IF(
            "BRIDGE",
            "Bridge avviato come %s",
            isMaster
                ? "MASTER"
                : "SLAVE"
        );
    }


    // ========================================================
    // CALLBACK
    // ========================================================

    virtual void onAEEVariableChanged(
        AEEVariableBase* variable,
        unsigned long now)
    {
        (void)variable;
        (void)now;
    }

    std::function<
        void(
            AEEVariableBase*,
            unsigned long
        )
    > onFrontendAEEChange;


    // ========================================================
    // LOOP
    // ========================================================

    virtual void loop(
        unsigned long now)
    {
        if (!transport ||
            !reg)
        {
            return;
        }

        // RX deve essere sempre processato.
        transport->loop(
            now
        );

        processIncoming(
            now
        );

        schema.checkRxTimeout(
            now
        );

        runStateMachine(
            now
        );

        handleResend(
            now
        );

        bool acquired =
            true;

        if (pacer)
        {
            acquired =
                pacer->acquire(
                    now
                );
        }

        if (acquired)
        {
            schema.handleTx(
                now
            );

            if (pacer)
            {
                pacer->release(
                    now
                );
            }
        }

        const bool online =
            peer.isOnline();

        if (
            online !=
            lastPeerOnline
        )
        {
            lastPeerOnline =
                online;
        }
    }


    // ========================================================
    // ACCESSORS
    // ========================================================

    AEERegistry* getRegistry()
    {
        return reg;
    }

    BridgePeerMonitor& getPeer()
    {
        return peer;
    }

    BridgeState getState() const
    {
        return state;
    }

    bool isHandshakeActive() const
    {
        return
            state ==
                BridgeState::CONNECTING ||
            state ==
                BridgeState::REQUEST_SCHEMA ||
            state ==
                BridgeState::WAIT_SCHEMA ||
            state ==
                BridgeState::ALIGNING;
    }

    bool isReadyForData() const
    {
        return
            state ==
                BridgeState::READY ||
            state ==
                BridgeState::DATA_EXCHANGE;
    }


    // ========================================================
    // RAW SEND
    // ========================================================

    void sendRaw(
        const String& payload)
    {
        if (!transport)
            return;

        transport->send(
            payload
        );
    }


protected:
    // ========================================================
    // DERIVED CALLBACKS
    // ========================================================

    virtual void onSchemaRequest()
    {
    }

    virtual void onSchemaReceived(
        const JsonArray& schemaArray)
    {
        (void)schemaArray;
    }

    virtual void onAligned()
    {
    }


    // ========================================================
    // STATE MACHINE
    // ========================================================

    void runStateMachine(
        unsigned long now)
    {
        switch (state)
        {
            // =================================================
            // DISCONNECTED
            // =================================================

            case BridgeState::DISCONNECTED:
            {
                setState(
                    BridgeState::CONNECTING
                );

                break;
            }

            // =================================================
            // CONNECTING
            // =================================================

            case BridgeState::CONNECTING:
            {
                if (isMaster)
                {
                    session.schemaReceived =
                        false;

                    if (
                        session.schemaRequested &&
                        peer.isOnline() &&
                        !schema.isTxActive()
                    )
                    {
                        setState(
                            BridgeState::READY
                        );
                    }
                }
                else
                {
                    if (!session.helloSent)
                    {
                        sendSchemaRequest();

                        session.helloSent =
                            true;

                        session.helloTime =
                            now;

                        setState(
                            BridgeState::WAIT_SCHEMA
                        );
                    }
                }

                break;
            }

            // =================================================
            // REQUEST SCHEMA
            // =================================================

            case BridgeState::REQUEST_SCHEMA:
            {
                if (!isMaster)
                {
                    sendSchemaRequest();

                    setState(
                        BridgeState::WAIT_SCHEMA
                    );
                }

                break;
            }

            // =================================================
            // WAIT SCHEMA
            // =================================================

            case BridgeState::WAIT_SCHEMA:
            {
                if (!isMaster)
                {
                    if (
                        session.schemaReceived
                    )
                    {
                        setState(
                            BridgeState::ALIGNING
                        );
                    }
                    else if (
                        !schema.isRxActive() &&
                        now -
                            session.helloTime >
                            HELLO_TIMEOUT
                    )
                    {
                        LOG_IF(
                            "BRIDGE",
                            "Timeout schema -> retry"
                        );

                        session.helloSent =
                            false;

                        setState(
                            BridgeState::CONNECTING
                        );
                    }
                }

                break;
            }

            // =================================================
            // ALIGNING
            // =================================================

            case BridgeState::ALIGNING:
            {
                if (
                    !isMaster &&
                    !session.schemaReceived
                )
                {
                    LOG_WF(
                        "BRIDGE",
                        "ALIGNING senza schema -> CONNECTING"
                    );

                    setState(
                        BridgeState::CONNECTING
                    );

                    break;
                }

                onAligned();

                setState(
                    BridgeState::READY
                );

                break;
            }

            // =================================================
            // READY
            // =================================================

            case BridgeState::READY:
            {
                if (
                    !isMaster &&
                    !session.schemaReceived
                )
                {
                    LOG_WF(
                        "BRIDGE",
                        "READY senza schema -> CONNECTING"
                    );

                    setState(
                        BridgeState::CONNECTING
                    );

                    break;
                }

                setState(
                    BridgeState::DATA_EXCHANGE
                );

                break;
            }

            // =================================================
            // DATA EXCHANGE
            // =================================================

            case BridgeState::DATA_EXCHANGE:
            {
                static constexpr unsigned long
                    PING_INTERVAL = 20000UL;

                static constexpr unsigned long
                    AEE_TX_INTERVAL = 1000UL;

                static constexpr unsigned long
                    OFFLINE_TIMEOUT =
                        PING_INTERVAL * 3UL;

                // ---------------------------------------------
                // INITIAL SNAPSHOT
                // ---------------------------------------------

                if (
                    isMaster &&
                    session.schemaRequested &&
                    session.initialSnapshotPending &&
                    !session.initialSnapshotSent &&
                    !schema.isTxActive()
                )
                {
                    LOG_IF(
                        "MASTER::AEE",
                        "Preparazione snapshot iniziale: vars=%u",
                        static_cast<unsigned>(
                            reg->size()
                        )
                    );

                    reg->forceAllChanged(
                        now
                    );

                    if (reg->hasChanges())
                    {
                        session
                            .completeInitialSnapshot();

                        LOG_IF(
                            "MASTER::AEE",
                            "Snapshot iniziale preparato"
                        );
                    }
                    else
                    {
                        LOG_WF(
                            "MASTER::AEE",
                            "Snapshot non preparato: nessuna variabile pending"
                        );
                    }
                }

                // ---------------------------------------------
                // AEE TX
                // ---------------------------------------------

                if (
                    reg->hasChanges() &&
                    now -
                        timing.lastAEEtx >
                        AEE_TX_INTERVAL
                )
                {
                    const bool sent =
                        AEEProtocolEngine::send(
                            *reg,
                            ack,
                            aee,
                            [&](const String& output)
                            {
                                transport->send(
                                    output
                                );
                            }
                        );

                    if (sent)
                    {
                        timing.lastAEEtx =
                            now;
                    }
                }

                // ---------------------------------------------
                // PING
                // ---------------------------------------------

                if (
                    now -
                        timing.lastPing >
                        PING_INTERVAL
                )
                {
                    transport->send(
                        "{\"ping\":1}"
                    );

                    timing.lastPing =
                        now;
                }

                // ---------------------------------------------
                // ONLINE / OFFLINE
                // ---------------------------------------------

                if (peer.isOnline())
                {
                    timing.lastOnline =
                        now;
                }
                else if (
                    now -
                        timing.lastOnline >
                        OFFLINE_TIMEOUT
                )
                {
                    if (!isMaster)
                    {
                        LOG_IF(
                            "BRIDGE",
                            "SLAVE: master offline -> restart handshake"
                        );

                        session.schemaReceived =
                            false;

                        session.resetSnapshot();

                        schema.rx.reset();

                        setState(
                            BridgeState::CONNECTING
                        );

                        return;
                    }

                    LOG_IF(
                        "BRIDGE",
                        "MASTER: peer offline -> restart handshake"
                    );

                    session.schemaRequested =
                        false;

                    session.resetSnapshot();

                    setState(
                        BridgeState::CONNECTING
                    );
                }

                break;
            }
        }
    }


    // ========================================================
    // STATE CHANGE
    // ========================================================

    void setState(
        BridgeState newState)
    {
        if (state == newState)
            return;

        state =
            newState;

        if (
            !isMaster &&
            newState ==
                BridgeState::CONNECTING
        )
        {
            session.helloSent =
                false;

            session.helloTime =
                0;
        }
    }


    // ========================================================
    // SEND SCHEMA REQUEST
    // ========================================================

    void sendSchemaRequest()
    {
        static constexpr unsigned long
            SCHEMA_REQUEST_MIN_INTERVAL =
                3000UL;

        const unsigned long now =
            millis();

        if (
            timing.lastSchemaRequest != 0 &&
            now -
                timing.lastSchemaRequest <
                SCHEMA_REQUEST_MIN_INTERVAL
        )
        {
            return;
        }

        timing.lastSchemaRequest =
            now;

        StaticJsonDocument<64>
            document;

        document["request"] =
            "schema";

        String output;

        serializeJson(
            document,
            output
        );

        transport->send(
            output
        );

        LOG_I(
            "BRIDGE",
            "Richiesta schema inviata"
        );
    }


    // ========================================================
    // PROCESS INCOMING
    // ========================================================

    void processIncoming(
        unsigned long now)
    {
        String message;

        while (
            transport->receive(
                message
            )
        )
        {
            if (
                message.length() <
                2
            )
            {
                continue;
            }

            peer.onPacket(
                now
            );

            // ---------------------------------------------
            // PING
            // ---------------------------------------------

            if (
                message.startsWith(
                    "{\"ping\""
                )
            )
            {
                continue;
            }

            // ---------------------------------------------
            // AEE ACK
            // ---------------------------------------------

            if (
                message.startsWith(
                    "{\"ack\""
                )
            )
            {
                StaticJsonDocument<64>
                    ackDocument;

                if (
                    !deserializeJson(
                        ackDocument,
                        message
                    )
                )
                {
                    AEEProtocolEngine::onPacket(
                        ackDocument,
                        ack
                    );
                }

                continue;
            }

            // ---------------------------------------------
            // SCHEMA ACK
            // ---------------------------------------------

            if (
                message.startsWith(
                    "{\"schema_ack\""
                )
            )
            {
                StaticJsonDocument<64>
                    schemaAckDocument;

                if (
                    !deserializeJson(
                        schemaAckDocument,
                        message
                    )
                )
                {
                    schema.handleRx(
                        schemaAckDocument,
                        now
                    );
                }

                continue;
            }

            // ---------------------------------------------
            // SCHEMA FRAME
            // ---------------------------------------------

            const bool isSchemaFrame =
                message.startsWith(
                    "{\"schema_part\""
                );

            if (isSchemaFrame)
            {
                StaticJsonDocument<512>
                    schemaDocument;

                const DeserializationError error =
                    deserializeJson(
                        schemaDocument,
                        message
                    );

                if (error)
                {
                    LOG_WF(
                        "BRIDGE::SCHEMA",
                        "deserializeJson fallita: %s",
                        error.c_str()
                    );

                    continue;
                }

                schema.handleRx(
                    schemaDocument,
                    now
                );

                continue;
            }

            // ---------------------------------------------
            // JSON BOUNDARIES
            // ---------------------------------------------

            if (
                message[0] != '{' ||
                message[
                    message.length() - 1
                ] != '}'
            )
            {
                LOG_WF(
                    "BRIDGE::DROP",
                    "JSON non valido: %s",
                    message.c_str()
                );

                continue;
            }

            // ---------------------------------------------
            // NORMAL JSON
            // ---------------------------------------------

            StaticJsonDocument<2048>
                document;

            const DeserializationError error =
                deserializeJson(
                    document,
                    message
                );

            if (error)
            {
                LOG_WF(
                    "BRIDGE::AEE",
                    "deserializeJson fallita: %s",
                    error.c_str()
                );

                continue;
            }

            if (
                schema.handleRx(
                    document,
                    now
                )
            )
            {
                continue;
            }

            // ---------------------------------------------
            // SCHEMA REQUEST
            // ---------------------------------------------

            if (
                document.containsKey(
                    "request"
                ) &&
                document["request"] ==
                    "schema"
            )
            {
                if (isMaster)
                {
                    onSchemaRequest();
                }

                continue;
            }

            // ---------------------------------------------
            // AEE
            // ---------------------------------------------

            BridgeProtocol::handleIncoming(
                document,
                *reg,
                *transport,
                aee,
                this
            );

            AEEProtocolEngine::onPacket(
                document,
                ack
            );
        }
    }


    // ========================================================
    // HANDLE RESEND
    // ========================================================

    void handleResend(
        unsigned long now)
    {
        if (!transport)
            return;

        AEEProtocolEngine::checkResend(
            ack,
            now,
            [&](const String& output)
            {
                transport->send(
                    output
                );
            }
        );
    }
};


// ============================================================
// BRIDGE PROTOCOL IMPLEMENTATION
// ============================================================

inline bool BridgeProtocol::handleIncoming(
    JsonDocument& document,
    AEERegistry& registry,
    ICommandTransport& transport,
    AEEProtocol& protocol,
    DMBridge* parent)
{
    if (!parent)
        return false;

    const bool parsed =
        protocol.parseJSON(
            document,
            registry
        );

    if (!parsed)
        return false;

    const unsigned long now =
        millis();

    registry.forEachChanged(
        [&](AEEVariableBase* variable)
        {
            if (!variable)
                return;

            parent->onAEEVariableChanged(
                variable,
                now
            );

            if (parent->onFrontendAEEChange)
            {
                parent->onFrontendAEEChange(
                    variable,
                    now
                );
            }

            variable->lastChange = 0;
        }
    );

    /*
     * Gli aggiornamenti ricevuti sono stati consumati
     * dalle callback applicative.
     */
    registry.clearAllChanged();

    if (
        document.containsKey(
            "seq"
        )
    )
    {
        const uint32_t sequence =
            document["seq"];

        StaticJsonDocument<64>
            ackDocument;

        ackDocument["ack"] =
            sequence;

        String output;

        serializeJson(
            ackDocument,
            output
        );

        transport.send(
            output
        );
    }

    return true;
}


// ============================================================
// MASTER BRIDGE
// ============================================================

class DMMasterBridge
    : public DMBridge
{
private:
    // ========================================================
    // SCHEMA CACHE
    // ========================================================

    struct SchemaCache
    {
        String payload;

        unsigned long lastBuild =
            0;

        void clear()
        {
            payload = "";
            lastBuild = 0;
        }

        bool expired(
            unsigned long now,
            unsigned long timeout) const
        {
            return
                payload.length() == 0 ||
                now - lastBuild >
                    timeout;
        }
    };

    SchemaCache schemaCache;


public:
    DMMasterBridge() = default;

    ~DMMasterBridge() override =
        default;


protected:
    // ========================================================
    // SCHEMA REQUEST
    // ========================================================

    void onSchemaRequest() override
    {
        static constexpr unsigned long
            CACHE_TIME = 10000UL;

        static constexpr unsigned long
            TX_TIMEOUT = 5000UL;

        const unsigned long now =
            millis();

        /*
         * Ogni nuova richiesta schema deve riarmare
         * lo snapshot iniziale per questa istanza.
         */
        session.armInitialSnapshot();

        // ----------------------------------------------------
        // SCHEMA TX GIÀ ATTIVA
        // ----------------------------------------------------

        if (schema.tx.active)
        {
            const unsigned long elapsed =
                now -
                schema.tx.startTime;

            if (
                elapsed <
                TX_TIMEOUT
            )
            {
                LOG_IF(
                    "MASTER::AEE",
                    "Schema request durante TX attiva (%lu ms) -> ignorata",
                    elapsed
                );

                return;
            }

            LOG_WF(
                "MASTER::AEE",
                "Schema TX timeout (%lu ms) -> abort",
                elapsed
            );

            schema.abortTx();
        }

        // ----------------------------------------------------
        // REGISTRY VALIDATION
        // ----------------------------------------------------

        if (!reg)
        {
            LOG_EF(
                "MASTER::AEE",
                "Schema request senza registry"
            );

            return;
        }

        // ----------------------------------------------------
        // CACHE BUILD
        // ----------------------------------------------------

        if (
            schemaCache.expired(
                now,
                CACHE_TIME
            )
        )
        {
            DynamicJsonDocument document(
                4096
            );

            JsonArray schemaArray =
                document.createNestedArray(
                    "schema"
                );

            aee.ExportSchema(
                *reg,
                schemaArray
            );

            schemaCache.payload =
                "";

            serializeJson(
                document,
                schemaCache.payload
            );

            schemaCache.lastBuild =
                now;

            LOG_IF(
                "MASTER::AEE",
                "Schema ricostruito (%u vars)",
                static_cast<unsigned>(
                    reg->size()
                )
            );
        }
        else
        {
            LOG_IF(
                "MASTER::AEE",
                "Schema inviato dalla cache"
            );
        }

        // ----------------------------------------------------
        // START TX
        // ----------------------------------------------------

        schema.startTx(
            schemaCache.payload
        );
    }
};

#endif