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
            if (!waitingAck())
            {
                return false;
            }

            if (lastSendTime == 0)
            {
                return false;
            }

            /*
            * Il cast unsigned mantiene il controllo compatibile con
            * il rollover di millis().
            *
            * now e lastSendTime devono comunque appartenere allo
            * stesso riferimento temporale del ciclo.
            */
            const unsigned long elapsed =
                static_cast<unsigned long>(
                    now - lastSendTime
                );

            return
                elapsed >= timeoutMs;
        }

        bool commitAck(
            uint32_t sequence)
        {
            if (!hasPendingPayload())
            {
                return false;
            }

            if (
                sequence !=
                lastSentSeq
            )
            {
                LOG_DF(
                    "BRIDGE::AEE::RX",
                    "ACK ignorato: received=%lu expected=%lu",
                    static_cast<unsigned long>(
                        sequence
                    ),
                    static_cast<unsigned long>(
                        lastSentSeq
                    )
                );

                return false;
            }

            lastAckSeq =
                sequence;

            pendingPayload =
                "";

            return true;
        }
    };

    // ========================================================
    // NEW SEND
    //
    // Crea un nuovo messaggio AEE solamente quando non esiste
    // già un payload in attesa di ACK.
    //
    // Il timestamp viene ricevuto dal loop chiamante, così invio
    // e controllo timeout utilizzano lo stesso riferimento
    // temporale.
    //
    // Questo evita l'underflow:
    //     now - lastSendTime
    // quando lastSendTime viene acquisito con un millis()
    // successivo al valore now del ciclo corrente.
    // ========================================================

    static bool send(
        AEERegistry& registry,
        AckState& state,
        AEEProtocol& protocol,
        unsigned long now,
        const SendCallback& sendFunction)
    {
        // ----------------------------------------------------
        // PENDING ACK
        //
        // Non generare un nuovo sequence e un nuovo payload
        // mentre il precedente è ancora in attesa di ACK.
        // ----------------------------------------------------

        if (state.waitingAck())
        {
            return false;
        }

        // ----------------------------------------------------
        // SEQUENCE
        // ----------------------------------------------------

        const uint32_t sequence =
            state.nextSeq();

        // ----------------------------------------------------
        // SERIALIZATION
        // ----------------------------------------------------

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

        // ----------------------------------------------------
        // ACK STATE
        //
        // Lo stato deve essere aggiornato prima dell'invio.
        //
        // In caso di elaborazione immediata nello stesso ciclo,
        // checkResend() troverà lastSendTime uguale a now.
        // ----------------------------------------------------

        state.pendingPayload =
            output;

        state.lastSentSeq =
            sequence;

        state.lastSendTime =
            now;

        // ----------------------------------------------------
        // SEND
        // ----------------------------------------------------

        LOG_IF(
            "BRIDGE::AEE::TX",
            "Sending AEE update: seq=%lu payload=%s",
            static_cast<unsigned long>(
                sequence
            ),
            state.pendingPayload.c_str()
        );

        sendFunction(
            state.pendingPayload
        );

        // ----------------------------------------------------
        // REGISTRY COMMIT
        //
        // Il payload completo rimane conservato in AckState per
        // eventuali ritrasmissioni con lo stesso sequence.
        // ----------------------------------------------------

        registry.clearAllChanged();

        return true;
    }

    // ========================================================
    // ACK RECEIVE
    // ========================================================

    static bool onPacket(
        JsonDocument& document,
        AckState& state)
    {
        if (
            !document.containsKey(
                "ack"
            )
        )
        {
            return false;
        }

        const uint32_t sequence =
            document["ack"];

        const bool accepted =
            state.commitAck(
                sequence
            );

        if (accepted)
        {
            LOG_DF(
                "BRIDGE::AEE::RX",
                "ACK ricevuto: seq=%lu",
                static_cast<unsigned long>(
                    sequence
                )
            );
        }

        return
            accepted;
    }

    // ========================================================
    // RESEND
    //
    // Ritrasmette esattamente:
    //
    // - lo stesso payload;
    // - lo stesso sequence.
    //
    // Non modifica il registry e non genera una nuova sequenza.
    // ========================================================

    static bool checkResend(
        AckState& state,
        unsigned long now,
        const SendCallback& sendFunction)
    {
        static constexpr unsigned long
            RESEND_TIMEOUT =
                1500UL;

        if (
            !state.ackTimeout(
                now,
                RESEND_TIMEOUT
            )
        )
        {
            return false;
        }

        LOG_WF(
            "BRIDGE::AEE::TX",
            "ACK timeout seq=%lu elapsed=%lu ms "
            "-> resend stesso payload",
            static_cast<unsigned long>(
                state.lastSentSeq
            ),
            static_cast<unsigned long>(
                now -
                state.lastSendTime
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
public:
    // ========================================================
    // INITIAL SNAPSHOT CALLBACK
    //
    // Permette al composition layer di definire quali variabili
    // devono entrare nello snapshot iniziale.
    //
    // Il callback deve:
    //
    // - esaminare il registry ricevuto;
    // - chiamare forceChanged(now) sulle variabili da includere;
    // - restituire il numero di variabili selezionate.
    //
    // Se il callback non è configurato, il comportamento resta
    // quello storico:
    //
    //     registry.forceAllChanged(now)
    //
    // Il valore di ritorno può essere zero. Uno snapshot vuoto
    // è valido quando nessuna area Buffer è mai variata.
    // ========================================================

    using InitialSnapshotCallback =
        std::function<
            size_t(
                AEERegistry& registry,
                unsigned long now
            )
        >;

protected:
    // ========================================================
    // SESSION STATE
    // ========================================================

    struct SessionState
    {
        bool helloSent =
            false;

        unsigned long helloTime =
            0;

        bool schemaRequested =
            false;

        bool schemaReceived =
            false;

        bool initialSnapshotPending =
            false;

        bool initialSnapshotSent =
            false;

        void reset()
        {
            helloSent =
                false;

            helloTime =
                0;

            schemaRequested =
                false;

            schemaReceived =
                false;

            initialSnapshotPending =
                false;

            initialSnapshotSent =
                false;
        }

        void armInitialSnapshot()
        {
            schemaRequested =
                true;

            initialSnapshotPending =
                true;

            initialSnapshotSent =
                false;
        }

        void completeInitialSnapshot()
        {
            schemaRequested =
                false;

            initialSnapshotPending =
                false;

            initialSnapshotSent =
                true;
        }

        void resetSnapshot()
        {
            initialSnapshotPending =
                false;

            initialSnapshotSent =
                false;
        }
    };

    // ========================================================
    // TIMING STATE
    // ========================================================

    struct TimingState
    {
        unsigned long lastOnline =
            0;

        unsigned long lastPing =
            0;

        unsigned long lastAEEtx =
            0;

        unsigned long lastSchemaRequest =
            0;

        unsigned long lastSchemaTxLog =
            0;

        void reset()
        {
            lastOnline =
                0;

            lastPing =
                0;

            lastAEEtx =
                0;

            lastSchemaRequest =
                0;

            lastSchemaTxLog =
                0;
        }

        void resetDataExchange()
        {
            lastOnline =
                0;

            lastPing =
                0;

            lastAEEtx =
                0;
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

    InitialSnapshotCallback
        initialSnapshotCallback =
            nullptr;

    static constexpr unsigned long
        HELLO_TIMEOUT =
            8000UL;

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
            bool active =
                false;

            size_t index =
                0;

            int waitingAckFor =
                -1;

            unsigned long lastSend =
                0;

            unsigned long startTime =
                0;

            std::vector<String>
                frames;

            void reset()
            {
                active =
                    false;

                index =
                    0;

                waitingAckFor =
                    -1;

                lastSend =
                    0;

                startTime =
                    0;

                frames.clear();
            }
        };

        // ====================================================
        // RX STATE
        // ====================================================

        struct RxState
        {
            bool active =
                false;

            uint16_t expected =
                0;

            uint16_t received =
                0;

            char buffer[8192];

            uint16_t offset =
                0;

            unsigned long startTs =
                0;

            void reset()
            {
                active =
                    false;

                expected =
                    0;

                received =
                    0;

                offset =
                    0;

                startTs =
                    0;

                buffer[0] =
                    '\0';
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
            return
                tx.active;
        }

        bool isRxActive() const
        {
            return
                rx.active;
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
            {
                return;
            }

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

        void sendAck(int part)
        {
            if (!transport)
                return;

            StaticJsonDocument<64> document;
            document["schema_ack"] = part;

            char buffer[64];
            const size_t length =
                serializeJson(
                    document,
                    buffer,
                    sizeof(buffer)
                );

            if (length == 0 ||
                length >= sizeof(buffer))
            {
                LOG_EF(
                    "BRIDGE::SCHEMA",
                    "Serializzazione schema ACK fallita: part=%d",
                    part
                );

                return;
            }

            buffer[length] = '\0';

            String output;
            if (!output.reserve(length + 1U))
            {
                LOG_EF(
                    "BRIDGE::SCHEMA",
                    "Allocazione schema ACK fallita: bytes=%u",
                    static_cast<unsigned>(length + 1U)
                );

                return;
            }

            output = buffer;
            transport->send(output);
        }

        // ====================================================
        // START TX
        //
        // Suddivide lo schema completo in frame JSON.
        //
        // La serializzazione utilizza un buffer char temporaneo
        // per evitare il writer ArduinoJson diretto su String.
        // ====================================================

        void startTx(const String& fullSchema)
        {
            static constexpr size_t MAX_CHUNK = 160U;
            static constexpr size_t FRAME_DOCUMENT_CAPACITY = 256U;
            static constexpr size_t FRAME_BUFFER_SIZE = 256U;

            tx.reset();

            const size_t schemaLength =
                fullSchema.length();

            if (schemaLength == 0U)
            {
                LOG_WF(
                    "BRIDGE::SCHEMA",
                    "TX non avviata: schema vuoto"
                );

                return;
            }

            const size_t total =
                (
                    schemaLength +
                    MAX_CHUNK -
                    1U
                ) /
                MAX_CHUNK;

            if (total == 0U ||
                total > static_cast<size_t>(UINT16_MAX))
            {
                LOG_EF(
                    "BRIDGE::SCHEMA",
                    "TX non avviata: numero frame non valido=%u",
                    static_cast<unsigned>(total)
                );

                return;
            }

            tx.frames.reserve(total);

            for (size_t index = 0;
                index < total;
                ++index)
            {
                const size_t start =
                    index * MAX_CHUNK;

                if (start >= schemaLength)
                {
                    LOG_EF(
                        "BRIDGE::SCHEMA",
                        "Offset frame non valido: "
                        "index=%u start=%u schema=%u",
                        static_cast<unsigned>(index),
                        static_cast<unsigned>(start),
                        static_cast<unsigned>(schemaLength)
                    );

                    tx.reset();
                    return;
                }

                const size_t remaining =
                    schemaLength - start;

                const size_t chunkLength =
                    min(
                        MAX_CHUNK,
                        remaining
                    );

                if (chunkLength == 0U ||
                    chunkLength > MAX_CHUNK)
                {
                    LOG_EF(
                        "BRIDGE::SCHEMA",
                        "Lunghezza chunk non valida: "
                        "index=%u length=%u",
                        static_cast<unsigned>(index),
                        static_cast<unsigned>(chunkLength)
                    );

                    tx.reset();
                    return;
                }

                char chunkBuffer[MAX_CHUNK + 1U];

                memcpy(
                    chunkBuffer,
                    fullSchema.c_str() + start,
                    chunkLength
                );

                chunkBuffer[chunkLength] = '\0';

                StaticJsonDocument<
                    FRAME_DOCUMENT_CAPACITY
                > document;

                document["schema_part"] =
                    static_cast<unsigned>(index);

                document["total"] =
                    static_cast<unsigned>(total);

                document["data"] =
                    chunkBuffer;

                if (document.overflowed())
                {
                    LOG_EF(
                        "BRIDGE::SCHEMA",
                        "Documento frame overflow: "
                        "index=%u chunk=%u capacity=%u",
                        static_cast<unsigned>(index),
                        static_cast<unsigned>(chunkLength),
                        static_cast<unsigned>(
                            FRAME_DOCUMENT_CAPACITY
                        )
                    );

                    tx.reset();
                    return;
                }

                const size_t requiredLength =
                    measureJson(document);

                if (requiredLength == 0U ||
                    requiredLength >= FRAME_BUFFER_SIZE)
                {
                    LOG_EF(
                        "BRIDGE::SCHEMA",
                        "Dimensione frame non valida: "
                        "index=%u required=%u maximum=%u",
                        static_cast<unsigned>(index),
                        static_cast<unsigned>(requiredLength),
                        static_cast<unsigned>(
                            FRAME_BUFFER_SIZE - 1U
                        )
                    );

                    tx.reset();
                    return;
                }

                char frameBuffer[FRAME_BUFFER_SIZE];
                frameBuffer[0] = '\0';

                const size_t serializedLength =
                    serializeJson(
                        document,
                        frameBuffer,
                        sizeof(frameBuffer)
                    );

                if (serializedLength == 0U ||
                    serializedLength != requiredLength ||
                    serializedLength >= sizeof(frameBuffer))
                {
                    LOG_EF(
                        "BRIDGE::SCHEMA",
                        "Serializzazione frame fallita: "
                        "index=%u serialized=%u required=%u",
                        static_cast<unsigned>(index),
                        static_cast<unsigned>(serializedLength),
                        static_cast<unsigned>(requiredLength)
                    );

                    tx.reset();
                    return;
                }

                frameBuffer[serializedLength] = '\0';

                String output;

                if (!output.reserve(serializedLength + 1U))
                {
                    LOG_EF(
                        "BRIDGE::SCHEMA",
                        "Allocazione String frame fallita: "
                        "index=%u bytes=%u",
                        static_cast<unsigned>(index),
                        static_cast<unsigned>(
                            serializedLength + 1U
                        )
                    );

                    tx.reset();
                    return;
                }

                output = frameBuffer;

                if (output.length() != serializedLength)
                {
                    LOG_EF(
                        "BRIDGE::SCHEMA",
                        "Lunghezza frame incoerente: "
                        "index=%u serialized=%u string=%u",
                        static_cast<unsigned>(index),
                        static_cast<unsigned>(serializedLength),
                        static_cast<unsigned>(output.length())
                    );

                    tx.reset();
                    return;
                }

                tx.frames.push_back(
                    std::move(output)
                );
            }

            if (tx.frames.size() != total)
            {
                LOG_EF(
                    "BRIDGE::SCHEMA",
                    "Conteggio frame incoerente: "
                    "expected=%u actual=%u",
                    static_cast<unsigned>(total),
                    static_cast<unsigned>(tx.frames.size())
                );

                tx.reset();
                return;
            }

            /*
            * La trasmissione diventa attiva soltanto quando
            * tutti i frame sono stati costruiti correttamente.
            */
            tx.active = true;
            tx.index = 0;
            tx.waitingAckFor = -1;
            tx.lastSend = 0;
            tx.startTime = millis();

            LOG_IF(
                "BRIDGE::SCHEMA",
                "TX multiframe avviato: "
                "schemaBytes=%u frames=%u",
                static_cast<unsigned>(schemaLength),
                static_cast<unsigned>(tx.frames.size())
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
            {
                return;
            }

            if (!tx.active)
            {
                return;
            }

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

                tx.active =
                    false;

                tx.waitingAckFor =
                    -1;

                return;
            }

            // ------------------------------------------------
            // ATTESA ACK
            // ------------------------------------------------

            if (tx.waitingAckFor >= 0)
            {
                const unsigned long ackTimeout =
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

                    ++tx.index;
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
             * Il master non deve ricevere schema_part.
             * Il frame viene comunque considerato gestito.
             */
            if (parent->isMaster)
            {
                return true;
            }

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

                rx.active =
                    true;

                rx.expected =
                    static_cast<uint16_t>(
                        total
                    );

                rx.received =
                    0;

                rx.offset =
                    0;

                rx.startTs =
                    now;

                LOG_IF(
                    "BRIDGE::SCHEMA",
                    "RX multiframe started (%d parts)",
                    total
                );
            }

            // ------------------------------------------------
            // TOTAL COHERENCE
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
                    static_cast<unsigned>(
                        rx.expected
                    ),
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
                    static_cast<unsigned>(
                        rx.received
                    ),
                    part
                );

                if (rx.received > 0)
                {
                    sendAck(
                        static_cast<int>(
                            rx.received - 1
                        )
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

            ++rx.received;

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
                    static_cast<unsigned>(
                        rx.offset
                    )
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
    DMBridge() =
        default;

    virtual ~DMBridge() =
        default;

    DMBridge(
        const DMBridge&) =
            delete;

    DMBridge& operator=(
        const DMBridge&) =
            delete;

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
    // INITIAL SNAPSHOT CALLBACK
    //
    // Il callback non viene cancellato da init().
    //
    // Questo consente al BridgeEngine di configurarlo prima o
    // dopo l'inizializzazione del bridge.
    // ========================================================

    void setInitialSnapshotCallback(
        InitialSnapshotCallback callback)
    {
        initialSnapshotCallback =
            std::move(
                callback
            );
    }

    void clearInitialSnapshotCallback()
    {
        initialSnapshotCallback =
            nullptr;
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

        lastPeerOnline =
            false;

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

        transport->loop(
            now
        );

        processIncoming(
            now
        );

        peer.checkOffline(
            now,
            60000UL
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
        return
            reg;
    }

    const AEERegistry* getRegistry() const
    {
        return
            reg;
    }

    BridgePeerMonitor& getPeer()
    {
        return
            peer;
    }

    const BridgePeerMonitor& getPeer() const
    {
        return
            peer;
    }

    BridgeState getState() const
    {
        return
            state;
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
        {
            return;
        }

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
    // PREPARE INITIAL SNAPSHOT
    //
    // Il metodo centralizza la selezione dello snapshot.
    //
    // Static AEE:
    //     forceAllChanged().
    //
    // Knob/Introspection:
    //     callback del channel, filtrata con HasEverChanged().
    //
    // Uno snapshot vuoto è considerato completato.
    // ========================================================

    size_t prepareInitialSnapshot(
        unsigned long now)
    {
        if (!reg)
        {
            return 0;
        }

        /*
         * Elimina eventuali changed residui prima di costruire
         * la nuova fotografia iniziale.
         *
         * Questo evita che aggiornamenti precedenti vengano
         * confusi con la selezione dello snapshot.
         */
        reg->clearAllChanged();

        if (initialSnapshotCallback)
        {
            return
                initialSnapshotCallback(
                    *reg,
                    now
                );
        }

        reg->forceAllChanged(
            now
        );

        return
            reg->size();
    }

    // ========================================================
    // STATE MACHINE
    //
    // Il traffico AEE ordinario e i ping vengono sospesi
    // durante la trasmissione o ricezione dello schema.
    // ========================================================

    void runStateMachine(unsigned long now)
    {
        switch (state)
        {
            case BridgeState::DISCONNECTED:
            {
                setState(
                    BridgeState::CONNECTING
                );

                break;
            }

            case BridgeState::CONNECTING:
            {
                if (isMaster)
                {
                    session.schemaReceived =
                        false;

                    if (session.schemaRequested &&
                        peer.isOnline() &&
                        !schema.isTxActive())
                    {
                        setState(
                            BridgeState::READY
                        );
                    }
                }
                else if (!session.helloSent)
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

                break;
            }

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

            case BridgeState::WAIT_SCHEMA:
            {
                if (!isMaster)
                {
                    if (session.schemaReceived)
                    {
                        setState(
                            BridgeState::ALIGNING
                        );
                    }
                    else if (!schema.isRxActive() &&
                            now - session.helloTime >
                                HELLO_TIMEOUT)
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

            case BridgeState::ALIGNING:
            {
                if (!isMaster &&
                    !session.schemaReceived)
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

            case BridgeState::READY:
            {
                if (!isMaster &&
                    !session.schemaReceived)
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

            case BridgeState::DATA_EXCHANGE:
            {
                static constexpr unsigned long
                    PING_INTERVAL =
                        20000UL;

                static constexpr unsigned long
                    AEE_TX_INTERVAL =
                        1000UL;

                static constexpr unsigned long
                    OFFLINE_TIMEOUT =
                        PING_INTERVAL * 3UL;

                const bool schemaBusy =
                    schema.isTxActive() ||
                    schema.isRxActive();

                // ------------------------------------------------
                // INITIAL SNAPSHOT
                //
                // Lo snapshot viene preparato soltanto dopo il
                // completamento della trasmissione dello schema.
                // ------------------------------------------------

                if (isMaster &&
                    session.schemaRequested &&
                    session.initialSnapshotPending &&
                    !session.initialSnapshotSent &&
                    !schemaBusy)
                {
                    LOG_IF(
                        "MASTER::AEE",
                        "Preparazione snapshot iniziale: vars=%u",
                        static_cast<unsigned>(
                            reg->size()
                        )
                    );

                    const size_t selectedCount =
                        prepareInitialSnapshot(
                            now
                        );

                    /*
                    * La preparazione è completata anche quando
                    * selectedCount è zero.
                    */
                    session.completeInitialSnapshot();

                    LOG_IF(
                        "MASTER::AEE",
                        "Snapshot iniziale preparato: "
                        "selected=%u pending=%u",
                        static_cast<unsigned>(
                            selectedCount
                        ),
                        reg->hasChanges()
                            ? 1U
                            : 0U
                    );
                }

                // ------------------------------------------------
                // AEE TX
                //
                // Nessun nuovo payload AEE viene generato durante
                // la trasmissione o ricezione dello schema.
                // ------------------------------------------------

                if (!schemaBusy &&
                    reg->hasChanges() &&
                    now - timing.lastAEEtx > AEE_TX_INTERVAL)
                {
                    const bool sent =
                        AEEProtocolEngine::send(
                            *reg,
                            ack,
                            aee,
                            now,
                            [this](const String& output)
                            {
                                if (transport)
                                    transport->send(output);
                            }
                        );

                    if (sent)
                        timing.lastAEEtx = now;
                }

                // ------------------------------------------------
                // PING
                //
                // Il ping viene sospeso durante lo schema per non
                // intercalare traffico nel trasferimento multiframe.
                // ------------------------------------------------

                if (!schemaBusy &&
                    now - timing.lastPing >
                        PING_INTERVAL)
                {
                    transport->send(
                        "{\"ping\":1}"
                    );

                    timing.lastPing =
                        now;
                }

                // ------------------------------------------------
                // ONLINE / OFFLINE
                // ------------------------------------------------

                if (peer.isOnline())
                {
                    timing.lastOnline =
                        now;
                }
                else if (timing.lastOnline != 0 &&
                        now - timing.lastOnline >
                            OFFLINE_TIMEOUT)
                {
                    if (!isMaster)
                    {
                        LOG_IF(
                            "BRIDGE",
                            "SLAVE: master offline "
                            "-> restart handshake"
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
                        "MASTER: peer offline "
                        "-> restart handshake"
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
        {
            return;
        }

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

        if (
            newState ==
            BridgeState::DATA_EXCHANGE
        )
        {
            timing.resetDataExchange();
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

        if (!transport)
            return;

        const unsigned long now =
            millis();

        if (timing.lastSchemaRequest != 0 &&
            now - timing.lastSchemaRequest <
                SCHEMA_REQUEST_MIN_INTERVAL)
        {
            return;
        }

        StaticJsonDocument<64> document;
        document["request"] = "schema";

        char buffer[64];

        const size_t length =
            serializeJson(
                document,
                buffer,
                sizeof(buffer)
            );

        if (length == 0 ||
            length >= sizeof(buffer))
        {
            LOG_EF(
                "BRIDGE",
                "Serializzazione richiesta schema fallita"
            );

            return;
        }

        buffer[length] = '\0';

        String output;

        if (!output.reserve(length + 1U))
        {
            LOG_EF(
                "BRIDGE",
                "Allocazione richiesta schema fallita: bytes=%u",
                static_cast<unsigned>(length + 1U)
            );

            return;
        }

        output = buffer;

        timing.lastSchemaRequest = now;
        transport->send(output);

        LOG_I(
            "BRIDGE",
            "Richiesta schema inviata"
        );
    }


    // ========================================================
    // PROCESS INCOMING
    //
    // Ogni pacchetto viene:
    //
    //   1. validato dimensionalmente;
    //   2. verificato nei delimitatori;
    //   3. deserializzato una sola volta;
    //   4. classificato in base alle chiavi JSON;
    //   5. inoltrato al relativo handler.
    //
    // Il peer viene considerato attivo soltanto dopo la
    // ricezione di un oggetto JSON formalmente valido.
    // ========================================================

    void processIncoming(unsigned long now)
    {
        if (!transport ||
            !reg)
        {
            return;
        }

        static constexpr size_t
            MAX_INCOMING_MESSAGE_SIZE =
                4096U;

        String message;

        while (transport->receive(message))
        {
            const size_t messageLength =
                message.length();

            if (messageLength < 2U)
            {
                LOG_WF(
                    "BRIDGE::DROP",
                    "Pacchetto troppo corto: bytes=%u",
                    static_cast<unsigned>(messageLength)
                );

                continue;
            }

            if (messageLength >
                MAX_INCOMING_MESSAGE_SIZE)
            {
                LOG_EF(
                    "BRIDGE::DROP",
                    "Pacchetto troppo grande: "
                    "bytes=%u maximum=%u",
                    static_cast<unsigned>(messageLength),
                    static_cast<unsigned>(
                        MAX_INCOMING_MESSAGE_SIZE
                    )
                );

                continue;
            }

            if (message.charAt(0) != '{' ||
                message.charAt(messageLength - 1U) != '}')
            {
                LOG_WF(
                    "BRIDGE::DROP",
                    "Pacchetto non JSON: "
                    "bytes=%u first=0x%02X last=0x%02X",
                    static_cast<unsigned>(messageLength),
                    static_cast<unsigned>(
                        static_cast<uint8_t>(
                            message.charAt(0)
                        )
                    ),
                    static_cast<unsigned>(
                        static_cast<uint8_t>(
                            message.charAt(
                                messageLength - 1U
                            )
                        )
                    )
                );

                continue;
            }

            size_t documentCapacity =
                messageLength * 2U + 256U;

            documentCapacity =
                max(
                    documentCapacity,
                    static_cast<size_t>(512U)
                );

            documentCapacity =
                min(
                    documentCapacity,
                    static_cast<size_t>(8192U)
                );

            DynamicJsonDocument document(
                documentCapacity
            );

            const DeserializationError error =
                deserializeJson(
                    document,
                    message
                );

            if (error)
            {
                LOG_WF(
                    "BRIDGE::DROP",
                    "Deserializzazione fallita: "
                    "bytes=%u error=%s",
                    static_cast<unsigned>(messageLength),
                    error.c_str()
                );

                continue;
            }

            if (document.overflowed())
            {
                LOG_EF(
                    "BRIDGE::DROP",
                    "JSON overflow: "
                    "bytes=%u capacity=%u",
                    static_cast<unsigned>(messageLength),
                    static_cast<unsigned>(
                        documentCapacity
                    )
                );

                continue;
            }

            if (!document.is<JsonObject>())
            {
                LOG_WF(
                    "BRIDGE::DROP",
                    "Root JSON non object"
                );

                continue;
            }

            /*
            * Un pacchetto formalmente valido costituisce
            * attività del peer.
            */
            peer.onPacket(now);

            // ----------------------------------------------------
            // PING
            // ----------------------------------------------------

            if (document.containsKey("ping"))
                continue;

            // ----------------------------------------------------
            // AEE ACK
            // ----------------------------------------------------

            if (document.containsKey("ack"))
            {
                AEEProtocolEngine::onPacket(
                    document,
                    ack
                );

                continue;
            }

            // ----------------------------------------------------
            // SCHEMA ACK / SCHEMA FRAME
            // ----------------------------------------------------

            if (document.containsKey("schema_ack") ||
                document.containsKey("schema_part"))
            {
                schema.handleRx(
                    document,
                    now
                );

                continue;
            }

            // ----------------------------------------------------
            // SCHEMA REQUEST
            // ----------------------------------------------------

            if (document.containsKey("request"))
            {
                const char* request =
                    document["request"];

                if (request &&
                    strcmp(request, "schema") == 0)
                {
                    if (isMaster)
                        onSchemaRequest();

                    continue;
                }

                LOG_WF(
                    "BRIDGE::DROP",
                    "Richiesta sconosciuta: %s",
                    request
                        ? request
                        : "<null>"
                );

                continue;
            }

            // ----------------------------------------------------
            // NORMAL AEE MESSAGE
            // ----------------------------------------------------

            BridgeProtocol::handleIncoming(
                document,
                *reg,
                *transport,
                aee,
                this
            );
        }
    }

    // ========================================================
    // HANDLE RESEND
    //
    // Il resend AEE viene sospeso durante handshake e
    // trasferimento schema TX/RX.
    // ========================================================

    void handleResend(unsigned long now)
    {
        if (!transport)
            return;

        if (isHandshakeActive() ||
            schema.isTxActive() ||
            schema.isRxActive())
        {
            return;
        }

        AEEProtocolEngine::checkResend(
            ack,
            now,
            [this](const String& output)
            {
                if (transport)
                    transport->send(output);
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

    if (!protocol.parseJSON(
            document,
            registry))
    {
        return false;
    }

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
    i aggiornamenti ricevuti sono stati consumati
     * dalle callback applicative.
     */
    registry.clearAllChanged();

    if (!document.containsKey("seq"))
        return true;

    const uint32_t sequence =
        document["seq"];

    StaticJsonDocument<64> ackDocument;
    ackDocument["ack"] = sequence;

    char buffer[64];

    const size_t length =
        serializeJson(
            ackDocument,
            buffer,
            sizeof(buffer)
        );

    if (length == 0 ||
        length >= sizeof(buffer))
    {
        LOG_EF(
            "BRIDGE::AEE::RX",
            "Serializzazione ACK fallita: seq=%lu",
            static_cast<unsigned long>(
                sequence
            )
        );

        return false;
    }

    buffer[length] = '\0';

    String output;

    if (!output.reserve(length + 1U))
    {
        LOG_EF(
            "BRIDGE::AEE::RX",
            "Allocazione ACK fallita: "
            "seq=%lu bytes=%u",
            static_cast<unsigned long>(
                sequence
            ),
            static_cast<unsigned>(
                length + 1U
            )
        );

        return false;
    }

    output = buffer;

    transport.send(
        output
    );

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
    // CONSTANTS
    // ========================================================

    static constexpr unsigned long
        SCHEMA_CACHE_TIME =
            10000UL;

    static constexpr unsigned long
        SCHEMA_TX_TIMEOUT =
            5000UL;

    /*
     * Deve essere coerente con:
     *
     *     SchemaMultiframe::RxState::buffer[8192]
     *
     * Viene lasciato un byte disponibile per il terminatore.
     */
    static constexpr size_t
        MAX_SCHEMA_PAYLOAD_SIZE =
            8191;

    /*
     * Capacità del documento usato per costruire lo schema.
     *
     * Il registry Knob può contenere molte più variabili rispetto
     * al registry AEE statico.
     */
    static constexpr size_t
        SCHEMA_DOCUMENT_CAPACITY =
            8192;

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
            payload =
                "";

            lastBuild =
                0;
        }

        bool empty() const
        {
            return
                payload.length() ==
                0;
        }

        bool expired(
            unsigned long now,
            unsigned long timeout) const
        {
            return
                empty() ||
                static_cast<unsigned long>(
                    now - lastBuild
                ) >
                timeout;
        }
    };

    SchemaCache schemaCache;

public:
    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    DMMasterBridge() =
        default;

    // ========================================================
    // DESTRUCTOR
    // ========================================================

    ~DMMasterBridge() override =
        default;

    // ========================================================
    // INVALIDATE SCHEMA CACHE
    //
    // Deve essere chiamata quando il registry viene ricostruito
    // dopo l'inizializzazione del bridge.
    //
    // Nel setup normale non è necessaria, perché il registry
    // viene costruito prima di BridgeEngine::init().
    // ========================================================

    void invalidateSchemaCache()
    {
        schemaCache.clear();

        LOG_DF(
            "MASTER::AEE",
            "Schema cache invalidata"
        );
    }

    // ========================================================
    // HAS CACHED SCHEMA
    // ========================================================

    bool hasCachedSchema() const
    {
        return
            !schemaCache.empty();
    }

    // ========================================================
    // CACHED SCHEMA SIZE
    // ========================================================

    size_t getCachedSchemaSize() const
    {
        return
            schemaCache.payload.length();
    }

protected:
    // ========================================================
    // BUILD SCHEMA CACHE
    //
    // Lo schema viene serializzato in un buffer char
    // dimensionato tramite measureJson().
    //
    // Si evita il writer ArduinoJson diretto verso String,
    // che sulla piattaforma ha causato StoreProhibited.
    // ========================================================

    bool buildSchemaCache(unsigned long now)
    {
        if (!reg)
        {
            LOG_EF(
                "MASTER::AEE",
                "Build schema senza registry"
            );

            return false;
        }

        const size_t registrySize =
            reg->size();

        if (registrySize == 0)
        {
            LOG_EF(
                "MASTER::AEE",
                "Build schema con registry vuoto"
            );

            return false;
        }

        DynamicJsonDocument document(
            SCHEMA_DOCUMENT_CAPACITY
        );

        JsonArray schemaArray =
            document.createNestedArray("schema");

        if (schemaArray.isNull())
        {
            LOG_EF(
                "MASTER::AEE",
                "Creazione array schema fallita"
            );

            return false;
        }

        aee.ExportSchema(
            *reg,
            schemaArray
        );

        if (document.overflowed())
        {
            LOG_EF(
                "MASTER::AEE",
                "Schema JSON overflow: "
                "vars=%u capacity=%u",
                static_cast<unsigned>(registrySize),
                static_cast<unsigned>(
                    SCHEMA_DOCUMENT_CAPACITY
                )
            );

            schemaCache.clear();
            return false;
        }

        const size_t requiredLength =
            measureJson(document);

        if (requiredLength == 0U ||
            requiredLength >
                MAX_SCHEMA_PAYLOAD_SIZE)
        {
            LOG_EF(
                "MASTER::AEE",
                "Dimensione schema non valida: "
                "bytes=%u maximum=%u vars=%u",
                static_cast<unsigned>(requiredLength),
                static_cast<unsigned>(
                    MAX_SCHEMA_PAYLOAD_SIZE
                ),
                static_cast<unsigned>(registrySize)
            );

            schemaCache.clear();
            return false;
        }

        const size_t bufferSize =
            requiredLength + 1U;

        char* serializationBuffer =
            new char[bufferSize];

        if (!serializationBuffer)
        {
            LOG_EF(
                "MASTER::AEE",
                "Allocazione buffer schema fallita: bytes=%u",
                static_cast<unsigned>(bufferSize)
            );

            schemaCache.clear();
            return false;
        }

        serializationBuffer[0] = '\0';

        const size_t serializedLength =
            serializeJson(
                document,
                serializationBuffer,
                bufferSize
            );

        if (serializedLength == 0U ||
            serializedLength != requiredLength ||
            serializedLength >= bufferSize)
        {
            LOG_EF(
                "MASTER::AEE",
                "Serializzazione schema fallita: "
                "serialized=%u required=%u",
                static_cast<unsigned>(serializedLength),
                static_cast<unsigned>(requiredLength)
            );

            delete[] serializationBuffer;
            schemaCache.clear();

            return false;
        }

        serializationBuffer[serializedLength] =
            '\0';

        String newPayload;

        if (!newPayload.reserve(
                serializedLength + 1U))
        {
            LOG_EF(
                "MASTER::AEE",
                "Allocazione String schema fallita: bytes=%u",
                static_cast<unsigned>(
                    serializedLength + 1U
                )
            );

            delete[] serializationBuffer;
            schemaCache.clear();

            return false;
        }

        newPayload = serializationBuffer;

        delete[] serializationBuffer;
        serializationBuffer = nullptr;

        if (newPayload.length() !=
            serializedLength)
        {
            LOG_EF(
                "MASTER::AEE",
                "Lunghezza schema incoerente: "
                "serialized=%u string=%u",
                static_cast<unsigned>(serializedLength),
                static_cast<unsigned>(
                    newPayload.length()
                )
            );

            schemaCache.clear();
            return false;
        }

        schemaCache.payload =
            std::move(newPayload);

        schemaCache.lastBuild =
            now;

        LOG_IF(
            "MASTER::AEE",
            "Schema ricostruito: "
            "vars=%u items=%u bytes=%u",
            static_cast<unsigned>(registrySize),
            static_cast<unsigned>(schemaArray.size()),
            static_cast<unsigned>(
                schemaCache.payload.length()
            )
        );

        return true;
    }

    // ========================================================
    // SCHEMA REQUEST
    // ========================================================

    void onSchemaRequest() override
    {
        const unsigned long now =
            millis();

        if (!reg)
        {
            LOG_EF(
                "MASTER::AEE",
                "Schema request senza registry"
            );

            return;
        }

        const size_t registrySize =
            reg->size();

        if (registrySize == 0)
        {
            LOG_EF(
                "MASTER::AEE",
                "Schema request con registry vuoto"
            );

            return;
        }

        // ----------------------------------------------------
        // SCHEMA TX GIÀ ATTIVA
        // ----------------------------------------------------

        if (schema.tx.active)
        {
            const unsigned long elapsed =
                static_cast<unsigned long>(
                    now - schema.tx.startTime
                );

            if (elapsed < SCHEMA_TX_TIMEOUT)
            {
                LOG_IF(
                    "MASTER::AEE",
                    "Schema request durante TX attiva "
                    "(%lu ms) -> ignorata",
                    elapsed
                );

                return;
            }

            LOG_WF(
                "MASTER::AEE",
                "Schema TX timeout (%lu ms) "
                "-> abort e restart",
                elapsed
            );

            schema.abortTx();
        }

        // ----------------------------------------------------
        // CACHE BUILD
        // ----------------------------------------------------

        if (schemaCache.expired(
                now,
                SCHEMA_CACHE_TIME))
        {
            if (!buildSchemaCache(now))
            {
                LOG_EF(
                    "MASTER::AEE",
                    "Schema request annullata: build fallita"
                );

                return;
            }
        }
        else
        {
            LOG_IF(
                "MASTER::AEE",
                "Schema inviato dalla cache: "
                "vars=%u bytes=%u age=%lu",
                static_cast<unsigned>(registrySize),
                static_cast<unsigned>(
                    schemaCache.payload.length()
                ),
                static_cast<unsigned long>(
                    now - schemaCache.lastBuild
                )
            );
        }

        if (schemaCache.empty())
        {
            LOG_EF(
                "MASTER::AEE",
                "Schema request annullata: cache vuota"
            );

            return;
        }

        /*
        * Ogni richiesta valida riapre la fase di allineamento.
        * Lo snapshot verrà preparato dopo il completamento
        * della trasmissione multiframe.
        */
        session.armInitialSnapshot();

        schema.startTx(
            schemaCache.payload
        );

        if (!schema.isTxActive())
        {
            session.schemaRequested = false;
            session.resetSnapshot();

            LOG_EF(
                "MASTER::AEE",
                "Avvio schema TX fallito"
            );

            return;
        }

        LOG_IF(
            "MASTER::AEE",
            "Schema TX avviata: vars=%u bytes=%u",
            static_cast<unsigned>(registrySize),
            static_cast<unsigned>(
                schemaCache.payload.length()
            )
        );
    }
};

#endif