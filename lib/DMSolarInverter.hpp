
#pragma once

#include <Arduino.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "DMPlatformNetwork.hpp"

//======================================================================
// ISolarTransport
//----------------------------------------------------------------------
// Interfaccia astratta per il trasporto.
//
// Implementazioni previste:
//   - UART / HardwareSerial
//   - RS232
//   - TCP transparent bridge
//   - RS485
//======================================================================

class ISolarTransport
{
public:
    enum class SolarFrameReadResult : uint8_t
    {
        Pending,
        Complete,
        Timeout,
        Disconnected
    };

    virtual ~ISolarTransport() = default;

    virtual bool begin() = 0;
    virtual void end() = 0;

    virtual bool connected() const = 0;

    virtual bool write(
        const uint8_t* data,
        size_t length) = 0;

    virtual SolarFrameReadResult readFrame(
        std::vector<uint8_t>& frame,
        uint32_t timeoutMs) = 0;

    virtual void resetFrameReader() = 0;

    virtual void flushInput() = 0;
};


//======================================================================
// ISolarInverter
//======================================================================

class ISolarInverter
{
public:

    //==================================================================
    // ENUMERAZIONI
    //==================================================================

    enum class QueryResult : uint8_t
    {
        Pending,
        Success,
        Nak,
        Timeout,
        TransportError,
        InvalidResponse,
        ParseError
    };

    enum class CommandState : uint8_t
    {
        Unknown,
        Supported,
        NotSupported,
        Timeout,
        TransportError,
        InvalidResponse
    };

    enum class CommandFraming : uint8_t
    {
        CR,
        LF,
        CRLF,
        CRC_XMODEM_CR
    };

    enum class CommandCategory : uint8_t
    {
        Realtime,
        Energy,
        Configuration,
        Diagnostic,
        Unknown
    };

    //==================================================================
    // SNAPSHOT
    //==================================================================

    struct Snapshot
    {
        //==================================================================
        // PHOTOVOLTAIC
        //==================================================================

        struct PV
        {
            bool valid = false;

            struct Electrical
            {
                float voltage = NAN;
                float current = NAN;
                float power = NAN;

                /*
                * Corrente erogata dal regolatore FV
                * verso il bus batteria.
                */
                float batteryCurrent = NAN;

                /*
                * Tensione batteria misurata dal regolatore SCC.
                */
                float sccBatteryVoltage = NAN;

            } electrical;

            struct Energy
            {
                float daily = NAN;
                float monthly = NAN;
                float yearly = NAN;
                float total = NAN;

            } energy;

            struct Limits
            {
                float auxiliaryValue = NAN;
                float maxCurrent = NAN;

                uint32_t ratedPower = 0;

            } limits;

            struct Thermal
            {
                float temperature = NAN;

            } thermal;

            uint32_t updatedAt = 0;

        } pv;

        //==================================================================
        // BATTERY
        //==================================================================

        struct Battery
        {
            bool valid = false;
            bool connected = false;

            struct Electrical
            {
                float voltage = NAN;
                float current = NAN;
                float power = NAN;

                float chargingCurrent = NAN;
                float dischargeCurrent = NAN;

            } electrical;

            struct Capacity
            {
                int16_t soc = -1;

            } capacity;

            struct Metadata
            {
                uint16_t cells = 0;

            } metadata;

            struct Thermal
            {
                float temperature = NAN;

            } thermal;

            struct Status
            {
                /*
                * Campi legacy conservati per compatibilità.
                */
                uint32_t statusWord = 0;
                uint32_t warningWord = 0;
                uint32_t faultWord = 0;

                /*
                * Flag proprietari HBAT.
                *
                * Il campo HBAT[6] è una sequenza di cifre posizionali,
                * non una bitword binaria.
                */
                String rawFlags;

                uint8_t chargeSwitch = 0;
                uint8_t solarChargeSwitch = 0;
                uint8_t acChargeSwitch = 0;

                uint8_t lithiumActivateSwitch = 0;
                uint8_t lithiumActivateProcess = 0;

                uint8_t reservedFlag = 0;

                /*
                * Codice nativo:
                *
                *   0 = idle
                *   1 = discharging
                *   2 = charging
                */
                uint8_t batteryStatus = 0;

                bool chargeActive = false;
                bool sccCharging = false;
                bool acCharging = false;

            } status;

            uint32_t updatedAt = 0;

        } battery;

        //==================================================================
        // GRID
        //==================================================================

        struct Grid
        {
            bool valid = false;
            bool available = false;

            struct Electrical
            {
                float voltage = NAN;
                float frequency = NAN;

                float current = NAN;
                float power = NAN;

            } electrical;

            struct Rating
            {
                float voltage = NAN;
                float current = NAN;
                float frequency = NAN;

            } rating;

            uint32_t updatedAt = 0;

        } grid;

        //==================================================================
        // OUTPUT
        //==================================================================

        struct Output
        {
            bool valid = false;

            struct Electrical
            {
                float voltage = NAN;
                float frequency = NAN;
                float current = NAN;

                float activePower = NAN;
                float apparentPower = NAN;

            } electrical;

            struct Load
            {
                uint8_t percent = 0;

            } load;

            struct Rating
            {
                float voltage = NAN;
                float frequency = NAN;
                float current = NAN;

                uint32_t activePower = 0;
                uint32_t apparentPower = 0;

            } rating;

            uint32_t updatedAt = 0;

        } output;

        //==================================================================
        // INTERNAL MEASUREMENTS
        //==================================================================

        struct Internal
        {
            bool valid = false;

            struct Electrical
            {
                float busVoltage = NAN;

                /*
                * Componente continua rilevata sull'uscita AC.
                * Campo proprietario HOP.
                */
                int32_t outputDcComponent = 0;

                /*
                * Corrente interna dell'induttore.
                * Campo proprietario HOP.
                */
                float inductorCurrent = NAN;

            } electrical;

            struct Thermal
            {
                float heatsinkTemperature = NAN;
                float pvTemperature = NAN;
                float boostTemperature = NAN;
                float maximumTemperature = NAN;

            } thermal;

            struct Fan
            {
                uint32_t speed1 = 0;
                uint32_t speed2 = 0;

                uint8_t status1 = 0;
                uint8_t status2 = 0;

            } fan;

            struct Diagnostic
            {
                int32_t fanOffset = 0;
                uint32_t eepromVersion = 0;

                /*
                * Campi proprietari non ancora interpretati semanticamente.
                */
                String hopInternalData;
                int32_t temperatureInternalData = 0;

            } diagnostic;

            uint32_t updatedAt = 0;

        } internal;

        //==================================================================
        // ENERGY
        //==================================================================

        struct Energy
        {
            bool valid = false;

            struct Timestamp
            {
                String date;
                String time;

            } timestamp;

            struct Production
            {
                float today = NAN;
                float month = NAN;
                float year = NAN;
                float total = NAN;

                float secondaryTotal = NAN;

            } production;

            uint32_t updatedAt = 0;

        } energy;

        //==================================================================
        // SETTINGS
        //==================================================================

        struct Settings
        {
            bool valid = false;

            struct Voltage
            {
                float batteryRating = NAN;

                float bulk = NAN;
                float floating = NAN;
                float cutoff = NAN;

                float low = NAN;
                float warning = NAN;
                float restart = NAN;
                float over = NAN;

                float recharge = NAN;
                float redischarge = NAN;

            } voltage;

            struct Current
            {
                uint16_t maxPV = 0;
                uint16_t maxCharge = 0;
                uint16_t maxUtilityCharge = 0;

            } current;

            struct Rating
            {
                uint32_t inverterPower = 0;
                uint32_t pvPower = 0;

            } rating;

            struct Mode
            {
                /*
                * Codici nativi del protocollo.
                * Nessuna interpretazione semantica nel driver.
                */
                uint8_t batteryType = 0;
                uint8_t acInputRange = 0;
                uint8_t outputSourcePriority = 0;
                uint8_t chargerSourcePriority = 0;
                uint8_t machineType = 0;
                uint8_t topology = 0;
                uint8_t outputMode = 0;
                uint8_t pvOkCondition = 0;

                uint8_t parallelMaxUnits = 0;

            } mode;

            uint32_t updatedAt = 0;

        } settings;

        //==================================================================
        // SYSTEM
        //==================================================================

        struct System
        {
            bool valid = false;
            bool connected = false;

            struct Identity
            {
                String protocol;
                String protocolVersion;

                String model;
                String serialNumber;

                String firmware;
                String secondaryFirmware;

            } identity;

            struct Operating
            {
                /*
                * Codice nativo della modalità operativa.
                *
                * Per i protocolli Voltronic proviene da QMOD.
                * Per QA6248 proviene dal primo carattere del secondo
                * token della risposta HSTS.
                */
                char modeCode = '\0';

                /*
                * Codice errore nativo restituito da HSTS.
                *
                * Viene conservato come stringa perché il formato
                * potrebbe differire tra firmware.
                */
                String errorCode;

            } operating;

            struct Status
            {
                /*
                * QPIGS field 16.
                */
                uint32_t statusWord = 0;

                /*
                * QPIGS field 20.
                */
                uint32_t statusWord2 = 0;

                /*
                * QPIWS completo.
                */
                String warningBitWord;

                /*
                * Stringhe native dei flag QA6248 HSTS.
                *
                * flagsRaw contiene la concatenazione:
                *
                *   flags del secondo token dopo modeCode
                *   +
                *   eventuale terzo token
                */
                String flagsRaw;

                /*
                * Flag HSTS A..L.
                */
                struct PrimaryFlags
                {
                    bool a = false;            
                    bool outputActive = false;          // B
                    bool batteryLowAlarm = false;       // C
                    bool batteryDisconnected = false;  // D
                    bool overload = false;              // E
                    bool overTemperature = false;       // F
                    bool configurationChanged = false;  // G
                    bool inverterOn = false;            // H
                    bool pvPowerLow = false;             // I
                    bool inputVoltageHigh = false;       // J
                    bool batteryVoltageHigh = false;     // K
                    bool fanFault = false;               // L

                } primary;

                /*
                * Flag HSTS estesi.
                *
                * Il repository QA6248 associa:
                *
                *   R = dustproof installed
                *   T = switch on
                *
                * Gli altri vengono conservati per completezza.
                */
                struct ExtendedFlags
                {
                    bool m = false;
                    bool p = false;
                    bool q = false;
                    bool dustproofInstalled = false;  // R
                    bool s = false;
                    bool switchOn = false;            // T
                    bool u = false;
                    bool v = false;
                    bool w = false;
                    bool x = false;
                    bool y = false;

                } extended;

            } status;

            struct Runtime
            {
                uint32_t connectedSince = 0;
                uint32_t lastSuccessfulQuery = 0;
                uint32_t lastRefresh = 0;

            } runtime;

        } system;

        uint32_t timestamp = 0;
        uint32_t revision = 0;
    };

    //==================================================================
    // DIAGNOSTICS
    //==================================================================

    struct Diagnostics
    {
        struct Frame
        {
            uint32_t timestamp = 0;

            String command;
            String tx;
            String rx;

            QueryResult result = QueryResult::InvalidResponse;

            bool success = false;
            bool nak = false;
            bool timeout = false;

            uint32_t latencyMs = 0;
        };

        struct Command
        {
            String name;

            CommandCategory category =
                CommandCategory::Unknown;

            CommandState state =
                CommandState::Unknown;

            CommandFraming framing =
                CommandFraming::CR;

            bool enabled = true;
            bool pollable = false;
            bool discoveryEnabled = true;

            uint32_t pollIntervalMs = 0;
            uint32_t lastPollAt = 0;
            uint32_t nextPollAt = 0;

            uint32_t requests = 0;
            uint32_t successes = 0;
            uint32_t failures = 0;

            uint32_t naks = 0;
            uint32_t timeouts = 0;
            uint32_t transportErrors = 0;

            uint32_t parseErrors = 0;
            uint32_t invalidResponses = 0;

            uint32_t lastLatencyMs = 0;
            uint32_t lastSuccessAt = 0;
            uint32_t lastFailureAt = 0;

            QueryResult lastResult =
                QueryResult::InvalidResponse;

            String lastResponse;
        };

        uint32_t totalQueries = 0;
        uint32_t successfulQueries = 0;
        uint32_t failedQueries = 0;

        uint32_t nakResponses = 0;
        uint32_t timeoutResponses = 0;
        uint32_t transportErrors = 0;
        uint32_t invalidResponses = 0;
        uint32_t parseErrors = 0;

        uint32_t txFrames = 0;
        uint32_t rxFrames = 0;

        uint32_t supportedCommands = 0;
        uint32_t unsupportedCommands = 0;
        uint32_t timeoutCommands = 0;
        uint32_t invalidCommands = 0;
        uint32_t unknownCommands = 0;

        uint32_t discoveryStartedAt = 0;
        uint32_t discoveryCompletedAt = 0;

        uint32_t lastPollTimestamp = 0;
        uint32_t lastSuccessfulQuery = 0;

        static constexpr size_t HistorySize = 100;

        std::array<Frame, HistorySize> history {};
        size_t historyIndex = 0;
        size_t historyCount = 0;

        std::vector<Command> commands;
    };

    //==================================================================
    // CALLBACK
    //==================================================================

    using SnapshotChangedCallback =
        std::function<void(
            const Snapshot& oldSnapshot,
            const Snapshot& newSnapshot)>;

    using FrameCallback =
        std::function<void(
            const Diagnostics::Frame& frame)>;

    using CommandStateCallback =
        std::function<void(
            const Diagnostics::Command& command)>;

public:

    virtual ~ISolarInverter() = default;

    virtual bool begin(uint32_t now) = 0;
    virtual void end() = 0;

    virtual bool discoverCapabilities() = 0;
    virtual bool refresh() = 0;

    virtual const Snapshot&
        snapshot() const = 0;

    virtual const Diagnostics&
        diagnostics() const = 0;

    virtual bool rawCommand(
        const String& command,
        String& response) = 0;

    virtual bool commandSupported(
        const String& command) const = 0;

    virtual const Diagnostics::Command*
        findCommand(const String& command) const = 0;

    virtual void setSnapshotChangedCallback(
        SnapshotChangedCallback callback) = 0;

    virtual void setFrameCallback(
        FrameCallback callback) = 0;

    virtual void setCommandStateCallback(
        CommandStateCallback callback) = 0;

    virtual bool connected() const = 0;

    virtual const char* protocolName() const = 0;
    virtual const char* modelName() const = 0;
};

//======================================================================
// SolarInverterBase
//======================================================================

class SolarInverterBase :
    public ISolarInverter
{
protected:
    //==================================================================
    // ASYNCHRONOUS TRANSACTION STATE
    //==================================================================

    struct PendingTransaction
    {
        bool active = false;
        bool discovery = false;

        String command;
        String response;

        String txDiagnostic;

        Diagnostics::Command*
            commandInfo = nullptr;

        CommandFraming framing =
            CommandFraming::CR;

        uint32_t startedAt = 0;

        std::vector<uint8_t>
            rxFrame;

        void reset()
        {
            active =
                false;

            discovery =
                false;

            command =
                "";

            response =
                "";

            txDiagnostic =
                "";

            commandInfo =
                nullptr;

            framing =
                CommandFraming::CR;

            startedAt =
                0;

            rxFrame.clear();
        }
    };

    //==================================================================
    // ORDINARY POLLING RUNTIME
    //------------------------------------------------------------------
    // Gestisce esclusivamente il polling successivo alla discovery.
    //
    // La discovery iniziale rimane veloce e non utilizza questo pacing.
    //
    // Regole:
    //   - un solo comando alla volta;
    //   - almeno commandSpacingMs tra due comandi ordinari;
    //   - pollIntervalMs del comando deve essere comunque scaduto.
    //==================================================================

    struct PollingRuntime
    {
        const uint32_t POLLING_MS=20000UL;
        
        size_t cursor = 0;

        uint32_t nextCommandAt = 0;

        uint32_t commandSpacingMs = POLLING_MS;

        void reset()
        {
            cursor = 0;
            nextCommandAt = 0;
            commandSpacingMs = POLLING_MS;
        }

        void scheduleNext(
            uint32_t now)
        {
            nextCommandAt =
                now +
                commandSpacingMs;
        }

        bool isReady(
            uint32_t now) const
        {
            return
                static_cast<int32_t>(
                    now - nextCommandAt
                ) >= 0;
        }
    };

    //==================================================================
    // ASYNCHRONOUS DISCOVERY STATE
    //==================================================================

    struct DiscoveryRuntime
    {
        bool active = false;
        bool completed = false;
        bool anySupported = false;

        size_t cursor = 0;

        uint32_t startedAt = 0;

        void reset()
        {
            active =
                false;

            completed =
                false;

            anySupported =
                false;

            cursor =
                0;

            startedAt =
                0;
        }
    };

    struct CommandDefinition
    {
        String name;

        CommandCategory category =
            CommandCategory::Unknown;

        bool pollable = false;
        bool discoveryEnabled = true;

        uint32_t pollIntervalMs = 0;

        CommandFraming framing =
            CommandFraming::CR;
    };

    PendingTransaction
        _pendingTransaction;

    DiscoveryRuntime
        _discoveryRuntime;

    PollingRuntime
        _pollingRuntime;
public:

    explicit SolarInverterBase(
        ISolarTransport& transport,
        uint32_t queryTimeoutMs = 2000)
        : _transport(transport),
          _queryTimeoutMs(queryTimeoutMs)
    {
    }

    ~SolarInverterBase() override = default;

    //==================================================================
    // LIFECYCLE
    //==================================================================

    bool begin(
        uint32_t now) override
    {
        resetRuntimeState();

        if (!_transport.begin())
        {
            _started =
                false;

            _connected =
                false;

            return false;
        }

        _started =
            true;

        _connected =
            _transport.connected();

        _snapshot.system.identity.protocol =
            protocolName();

        _snapshot.system.identity.model =
            modelName();

        _snapshot.system.connected =
            _connected;

        _snapshot.system.valid =
            true;

        _snapshot.timestamp =
            now;

        _snapshot.system.runtime.lastRefresh =
            now;

        // ------------------------------------------------------
        // REGISTRY
        // ------------------------------------------------------

        registerProtocolCommands();

        rebuildCommandDiagnostics();

        // ------------------------------------------------------
        // ASYNCHRONOUS DISCOVERY INITIALIZATION
        // ------------------------------------------------------

        _pendingTransaction.reset();

        _discoveryRuntime.reset();

        _discoveryRuntime.active =
            true;

        _discoveryRuntime.completed =
            false;

        _discoveryRuntime.anySupported =
            false;

        _discoveryRuntime.cursor =
            0;

        _discoveryRuntime.startedAt =
            now;

        _diagnostics.discoveryStartedAt =
            now;

        _diagnostics.discoveryCompletedAt =
            0;

        /*
         * begin() conferma che il transport è stato inizializzato.
         *
         * La capability discovery viene eseguita progressivamente
         * dalle successive chiamate a refresh().
         */
        return true;
    }

    void end() override
    {
        _pendingTransaction.reset();

        _discoveryRuntime.reset();

        _transport.resetFrameReader();

        _transport.end();

        _started =
            false;

        _connected =
            false;

        _snapshot.system.connected =
            false;

        _snapshot.timestamp =
            millis();
    }

    //==================================================================
    // DISCOVERY
    //==================================================================

    bool discoverCapabilities() override
    {
        if (!_started)
            return false;

        if (_pendingTransaction.active)
            return false;

        _discoveryRuntime.reset();

        _pollingRuntime.reset();

        _discoveryRuntime.active =
            true;

        _discoveryRuntime.completed =
            false;

        _discoveryRuntime.anySupported =
            false;

        _discoveryRuntime.cursor =
            0;

        _discoveryRuntime.startedAt =
            millis();

        _diagnostics.discoveryStartedAt =
            _discoveryRuntime.startedAt;

        _diagnostics.discoveryCompletedAt =
            0;

        for (auto& command :
            _diagnostics.commands)
        {
            command.state =
                CommandState::Unknown;

            command.lastPollAt =
                0;

            command.nextPollAt =
                0;
        }

        recalculateCapabilityCounters();

        return true;
    }

    //==================================================================
    // REFRESH
    //----------------------------------------------------------------------
    // Esegue al massimo un comando per chiamata.
    //
    // La rotazione non contiene alcun comando cablato.
    // Utilizza direttamente Diagnostics::commands, costruito durante
    // registerProtocolCommands() e aggiornato dalla capability discovery.
    //
    // Sono candidati esclusivamente i comandi:
    //
    //   - abilitati;
    //   - pollable;
    //   - riconosciuti come Supported;
    //   - arrivati alla relativa scadenza.
    //
    // I comandi che durante la discovery hanno restituito NAK risultano
    // NotSupported e vengono automaticamente esclusi.
    //
    // Anche Timeout, TransportError, InvalidResponse e Unknown vengono
    // esclusi perché non sono stati validati positivamente.
    //
    // Lo Snapshot iniziale viene popolato da discoverCapabilities().
    // Le chiamate successive avanzano in round-robin, eseguendo un solo
    // comando dovuto per ogni invocazione di refresh().
    //==================================================================

    bool refresh() override
    {
        if (!_started)
            return false;

        const uint32_t now =
            millis();

        Snapshot previous =
            _snapshot;

        bool snapshotUpdated =
            false;

        // ==========================================================
        // DISCOVERY ASINCRONA VELOCE
        // ==========================================================

        if (_discoveryRuntime.active)
        {
            snapshotUpdated =
                serviceDiscovery(
                    now
                );

            _snapshot.system.connected =
                _connected;

            _snapshot.system.runtime.lastRefresh =
                now;

            _snapshot.timestamp =
                now;

            notifySnapshotIfChanged(
                previous
            );

            return snapshotUpdated;
        }

        // ==========================================================
        // TRANSAZIONE DI POLLING IN CORSO
        // ==========================================================

        if (_pendingTransaction.active)
        {
            String response;

            const QueryResult result =
                pollTransaction(
                    response
                );

            if (result == QueryResult::Pending)
            {
                /*
                * La transazione resta aperta.
                *
                * Nessun nuovo comando può essere inviato.
                */
                return false;
            }

            snapshotUpdated =
                completeTransaction(
                    result,
                    response,
                    now
                );

            _diagnostics.lastPollTimestamp =
                now;

            _snapshot.system.connected =
                _connected;

            _snapshot.system.runtime.lastRefresh =
                now;

            _snapshot.timestamp =
                now;

            if (snapshotUpdated)
            {
                _snapshot.system.runtime.lastSuccessfulQuery =
                    now;
            }

            /*
            * Qualunque sia il risultato finale:
            *
            *   Success
            *   NAK
            *   Timeout
            *   TransportError
            *   InvalidResponse
            *   ParseError
            *
            * il prossimo comando ordinario potrà partire solamente
            * dopo il pacing globale configurato.
            */
            _pollingRuntime.scheduleNext(
                now
            );

            notifySnapshotIfChanged(
                previous
            );

            return snapshotUpdated;
        }

        // ==========================================================
        // PACING GLOBALE POLLING ORDINARIO
        // ==========================================================

        if (!_pollingRuntime.isReady(
                now))
        {
            return false;
        }

        // ==========================================================
        // SELEZIONE PROSSIMO COMANDO DOVUTO
        // ==========================================================

        const size_t commandCount =
            _diagnostics.commands.size();

        if (commandCount == 0U)
            return false;

        if (
            _pollingRuntime.cursor >=
            commandCount
        )
        {
            _pollingRuntime.cursor =
                0U;
        }

        bool commandStarted =
            false;

        for (size_t scanned = 0U;
            scanned < commandCount;
            ++scanned)
        {
            const size_t commandIndex =
                (
                    _pollingRuntime.cursor +
                    scanned
                ) %
                commandCount;

            Diagnostics::Command& command =
                _diagnostics.commands[
                    commandIndex
                ];

            if (!command.enabled)
                continue;

            if (!command.pollable)
                continue;

            if (
                command.state !=
                CommandState::Supported
            )
            {
                continue;
            }

            /*
            * Oltre al pacing globale, deve essere scaduto anche
            * l'intervallo specifico del comando.
            */
            if (!isDue(
                    now,
                    command.nextPollAt))
            {
                continue;
            }

            _pollingRuntime.cursor =
                (commandIndex + 1U) %
                commandCount;

            commandStarted =
                startTransaction(
                    command,
                    false
                );

            /*
            * Un solo tentativo di avvio per chiamata.
            */
            break;
        }

        if (!commandStarted)
        {
            /*
            * Nessun comando era eleggibile oppure la trasmissione
            * non è partita.
            *
            * Evitiamo di ripetere la scansione ad ogni ciclo da 5 ms.
            * La scansione verrà ritentata dopo 10 secondi.
            */
            _pollingRuntime.scheduleNext(
                now
            );
        }

        _snapshot.system.runtime.lastRefresh =
            now;

        _snapshot.timestamp =
            now;

        return false;
    }

    //==================================================================
    // RAW COMMAND
    //==================================================================

    bool rawCommand(
        const String& command,
        String& response) override
    {
        response =
            "";

        if (!_started)
            return false;

        if (_discoveryRuntime.active)
            return false;

        if (_pendingTransaction.active)
            return false;

        Diagnostics::Command* commandInfo =
            findCommandMutable(
                command
            );

        if (!commandInfo)
            return false;

        /*
         * L'interfaccia bool originale non può rappresentare una
         * transazione asincrona Pending.
         *
         * Il comando RAW sincrono viene quindi temporaneamente
         * disabilitato fino all'introduzione di:
         *
         *   queueRawCommand()
         *   rawCommandCompleted()
         *   getRawCommandResponse()
         */
        LOG_WF(
            "SOLAR",
            "Raw synchronous command unavailable in non-blocking mode: %s",
            command.c_str()
        );

        return false;
    }

    //==================================================================
    // ACCESSO DATI
    //==================================================================

    const Snapshot&
    snapshot() const override
    {
        return _snapshot;
    }

    const Diagnostics&
    diagnostics() const override
    {
        return _diagnostics;
    }

    bool commandSupported(
        const String& command) const override
    {
        const Diagnostics::Command* info =
            findCommand(command);

        return
            info &&
            info->state ==
                CommandState::Supported;
    }

    const Diagnostics::Command*
    findCommand(
        const String& command) const override
    {
        for (const auto& entry :
             _diagnostics.commands)
        {
            if (entry.name == command)
                return &entry;
        }

        return nullptr;
    }

    //==================================================================
    // CALLBACK
    //==================================================================

    void setSnapshotChangedCallback(
        SnapshotChangedCallback callback) override
    {
        _snapshotChangedCallback =
            std::move(callback);
    }

    void setFrameCallback(
        FrameCallback callback) override
    {
        _frameCallback =
            std::move(callback);
    }

    void setCommandStateCallback(
        CommandStateCallback callback) override
    {
        _commandStateCallback =
            std::move(callback);
    }

    //==================================================================
    // STATO
    //==================================================================

    bool connected() const override
    {
        return
            _started &&
            _connected;
    }

protected:

    //==================================================================
    // API PER I DRIVER
    //==================================================================

    virtual void registerProtocolCommands() = 0;

    virtual bool parseProtocolResponse(
        const String& command,
        const String& response) = 0;

    //==================================================================
    // REGISTER COMMAND
    //----------------------------------------------------------------------
    // Registra un comando e il relativo framing.
    //
    // Framing supportati:
    //
    //   CR
    //       payload + 0x0D
    //
    //   LF
    //       payload + 0x0A
    //
    //   CRLF
    //       payload + 0x0D + 0x0A
    //
    //   CRC_XMODEM_CR
    //       payload + CRC16/XMODEM escaped + 0x0D
    //==================================================================

    void registerCommand(
        const String& name,
        CommandCategory category,
        bool pollable,
        uint32_t pollIntervalMs,
        bool discoveryEnabled = true,
        CommandFraming framing =
            CommandFraming::CR)
    {
        CommandDefinition definition;

        definition.name =
            name;

        definition.category =
            category;

        definition.pollable =
            pollable;

        definition.pollIntervalMs =
            pollIntervalMs;

        definition.discoveryEnabled =
            discoveryEnabled;

        definition.framing =
            framing;

        _commandDefinitions.push_back(
            definition
        );
    }

    bool startTransaction(
        Diagnostics::Command& command,
        bool discovery)
    {
        if (!_started)
            return false;

        if (_pendingTransaction.active)
            return false;

        const std::vector<uint8_t> txFrame =
            buildCommandFrame(
                command.name,
                command.framing
            );

        if (txFrame.empty())
            return false;

        _transport.flushInput();

        _pendingTransaction.reset();

        _pendingTransaction.active =
            true;

        _pendingTransaction.discovery =
            discovery;

        _pendingTransaction.command =
            command.name;

        _pendingTransaction.commandInfo =
            &command;

        _pendingTransaction.framing =
            command.framing;

        _pendingTransaction.startedAt =
            millis();

        _pendingTransaction.txDiagnostic =
            frameToHex(
                txFrame
            );

        command.requests++;

        command.lastPollAt =
            _pendingTransaction.startedAt;

        _diagnostics.totalQueries++;

        _diagnostics.txFrames++;

        if (!_transport.write(
                txFrame.data(),
                txFrame.size()))
        {
            const uint32_t completedAt =
                millis();

            const uint32_t latency =
                completedAt -
                _pendingTransaction.startedAt;

            registerResult(
                &command,
                QueryResult::TransportError,
                "",
                completedAt,
                latency
            );

            recordFrame(
                command.name,
                _pendingTransaction.txDiagnostic,
                "",
                QueryResult::TransportError,
                completedAt,
                latency
            );

            _connected =
                false;

            _snapshot.system.connected =
                false;

            _pendingTransaction.reset();

            return false;
        }

        return true;
    }

    bool completeTransaction(
        QueryResult result,
        const String& response,
        uint32_t now)
    {
        if (!_pendingTransaction.active)
            return false;

        Diagnostics::Command* command =
            _pendingTransaction.commandInfo;

        const String commandName =
            _pendingTransaction.command;

        const bool discovery =
            _pendingTransaction.discovery;

        bool parsed =
            false;

        if (
            result ==
            QueryResult::Success
        )
        {
            parsed =
                parseProtocolResponse(
                    commandName,
                    response
                );

            if (!parsed && command)
            {
                command->parseErrors++;

                command->lastResult =
                    QueryResult::ParseError;

                command->failures++;

                command->lastFailureAt =
                    now;

                _diagnostics.parseErrors++;
            }
        }

        if (command)
        {
            if (discovery)
            {
                command->state =
                    commandStateFromResult(
                        parsed
                            ? QueryResult::Success
                            : (
                                result ==
                                QueryResult::Success
                                    ? QueryResult::ParseError
                                    : result
                            )
                    );
            }
            else
            {
                command->lastPollAt =
                    now;

                command->nextPollAt =
                    now +
                    command->pollIntervalMs;
            }
        }

        _pendingTransaction.reset();

        return parsed;
    }

    bool serviceDiscovery(
        uint32_t now)
    {
        if (!_discoveryRuntime.active)
            return false;

        // ==========================================================
        // TRANSAZIONE DISCOVERY IN CORSO
        // ==========================================================

        if (_pendingTransaction.active)
        {
            String response;

            const QueryResult result =
                pollTransaction(
                    response
                );

            if (result == QueryResult::Pending)
                return false;

            const bool parsed =
                completeTransaction(
                    result,
                    response,
                    now
                );

            if (parsed)
            {
                _discoveryRuntime.anySupported =
                    true;
            }

            /*
            * Nessuna pausa durante la discovery.
            *
            * Alla chiamata successiva verrà immediatamente avviato
            * il comando discovery seguente.
            */
            return parsed;
        }

        // ==========================================================
        // PROSSIMO COMANDO DISCOVERY
        // ==========================================================

        while (
            _discoveryRuntime.cursor <
            _diagnostics.commands.size()
        )
        {
            Diagnostics::Command& command =
                _diagnostics.commands[
                    _discoveryRuntime.cursor++
                ];

            if (!command.enabled)
                continue;

            if (!command.discoveryEnabled)
                continue;

            startTransaction(
                command,
                true
            );

            return false;
        }

        // ==========================================================
        // DISCOVERY COMPLETATA
        // ==========================================================

        _discoveryRuntime.active =
            false;

        _discoveryRuntime.completed =
            true;

        _diagnostics.discoveryCompletedAt =
            now;

        recalculateCapabilityCounters();

        _connected =
            _discoveryRuntime.anySupported ||
            _transport.connected();

        _snapshot.system.connected =
            _connected;

        _snapshot.system.valid =
            true;

        _snapshot.timestamp =
            now;

        // ==========================================================
        // INIZIALIZZAZIONE POLLING ORDINARIO
        // ==========================================================

        _pollingRuntime.cursor =
            0U;

        /*
        * I comandi supportati e pollable diventano inizialmente
        * eleggibili.
        *
        * Il primo comando non parte però immediatamente:
        * viene rispettata la pausa globale di 10 secondi.
        */
        for (auto& command :
            _diagnostics.commands)
        {
            command.lastPollAt =
                0U;

            command.nextPollAt =
                0U;
        }

        _pollingRuntime.scheduleNext(
            now
        );

        LOG_IF(
            "SOLAR",
            "DISCOVERY DONE - ordinary polling starts in %lu ms",
            static_cast<unsigned long>(
                _pollingRuntime.commandSpacingMs
            )
        );

        return
            _discoveryRuntime.anySupported;
    }

    QueryResult pollTransaction(
        String& response)
    {
        response =
            "";

        if (!_pendingTransaction.active)
            return QueryResult::InvalidResponse;

        Diagnostics::Command* commandInfo =
            _pendingTransaction.commandInfo;

        const auto readResult =
            _transport.readFrame(
                _pendingTransaction.rxFrame,
                _queryTimeoutMs
            );

        // ------------------------------------------------------
        // ANCORA IN ATTESA
        // ------------------------------------------------------

        if (
            readResult ==
            ISolarTransport::
                SolarFrameReadResult::Pending
        )
        {
            return QueryResult::Pending;
        }

        const uint32_t completedAt =
            millis();

        const uint32_t latency =
            completedAt -
            _pendingTransaction.startedAt;

        // ------------------------------------------------------
        // TIMEOUT
        // ------------------------------------------------------

        if (
            readResult ==
            ISolarTransport::
                SolarFrameReadResult::Timeout
        )
        {
            registerResult(
                commandInfo,
                QueryResult::Timeout,
                "",
                completedAt,
                latency
            );

            recordFrame(
                _pendingTransaction.command,
                _pendingTransaction.txDiagnostic,
                "",
                QueryResult::Timeout,
                completedAt,
                latency
            );

            _connected =
                _transport.connected();

            _snapshot.system.connected =
                _connected;

            _pendingTransaction.reset();

            return QueryResult::Timeout;
        }

        // ------------------------------------------------------
        // DISCONNECTED
        // ------------------------------------------------------

        if (
            readResult ==
            ISolarTransport::
                SolarFrameReadResult::Disconnected
        )
        {
            registerResult(
                commandInfo,
                QueryResult::TransportError,
                "",
                completedAt,
                latency
            );

            recordFrame(
                _pendingTransaction.command,
                _pendingTransaction.txDiagnostic,
                "",
                QueryResult::TransportError,
                completedAt,
                latency
            );

            _connected =
                false;

            _snapshot.system.connected =
                false;

            _pendingTransaction.reset();

            return QueryResult::TransportError;
        }

        // ------------------------------------------------------
        // COMPLETE
        // ------------------------------------------------------

        _diagnostics.rxFrames++;

        const String rxDiagnostic =
            frameToHex(
                _pendingTransaction.rxFrame
            );

        if (!extractResponsePayload(
                _pendingTransaction.rxFrame,
                _pendingTransaction.framing,
                response))
        {
            registerResult(
                commandInfo,
                QueryResult::InvalidResponse,
                rxDiagnostic,
                completedAt,
                latency
            );

            recordFrame(
                _pendingTransaction.command,
                _pendingTransaction.txDiagnostic,
                rxDiagnostic,
                QueryResult::InvalidResponse,
                completedAt,
                latency
            );

            _pendingTransaction.reset();

            return QueryResult::InvalidResponse;
        }

        trimResponse(
            response
        );

        const QueryResult result =
            classifyResponse(
                response
            );

        registerResult(
            commandInfo,
            result,
            response,
            completedAt,
            latency
        );

        recordFrame(
            _pendingTransaction.command,
            _pendingTransaction.txDiagnostic,
            rxDiagnostic,
            result,
            completedAt,
            latency
        );

        if (result == QueryResult::Success)
        {
            const bool wasConnected =
                _connected;

            _connected =
                true;

            _snapshot.system.connected =
                true;

            if (!wasConnected)
            {
                _snapshot.system.runtime.connectedSince =
                    completedAt;
            }

            _snapshot.system.runtime.lastSuccessfulQuery =
                completedAt;

            _diagnostics.lastSuccessfulQuery =
                completedAt;
        }
        else if (
            result == QueryResult::Timeout ||
            result == QueryResult::TransportError)
        {
            _connected =
                _transport.connected();

            _snapshot.system.connected =
                _connected;
        }

        return result;
    }

    static std::vector<String>
    tokenize(const String& rawResponse)
    {
        std::vector<String> values;

        String response = rawResponse;

        trimResponse(response);

        if (response.startsWith("("))
            response.remove(0, 1);

        response.trim();

        int start = 0;

        while (start < response.length())
        {
            while (
                start < response.length() &&
                response[start] == ' ')
            {
                start++;
            }

            if (start >= response.length())
                break;

            int separator =
                response.indexOf(' ', start);

            if (separator < 0)
            {
                values.push_back(
                    response.substring(start));
                break;
            }

            values.push_back(
                response.substring(
                    start,
                    separator));

            start = separator + 1;
        }

        return values;
    }

    static bool parseFloatToken(
        const std::vector<String>& values,
        size_t index,
        float& output)
    {
        if (index >= values.size())
            return false;

        const String& token = values[index];

        if (token.length() == 0)
            return false;

        output = token.toFloat();

        return true;
    }

    static bool parseUIntToken(
        const std::vector<String>& values,
        size_t index,
        uint32_t& output)
    {
        if (index >= values.size())
            return false;

        const String& token = values[index];

        if (token.length() == 0)
            return false;

        output =
            static_cast<uint32_t>(
                token.toInt());

        return true;
    }

    void markSnapshotUpdated(
        uint32_t timestamp)
    {
        _snapshot.timestamp =
            timestamp;

        _snapshot.revision++;
    }

//==================================================================
// PARSE SIGNED INTEGER TOKEN
//------------------------------------------------------------------
// Converte un token numerico in int32_t.
//
// Restituisce false quando:
//   - l'indice non esiste;
//   - il token è vuoto.
//==================================================================

static bool parseIntToken(
    const std::vector<String>& values,
    size_t index,
    int32_t& output)
{
    if (index >= values.size())
        return false;

    const String& token =
        values[index];

    if (token.length() == 0)
        return false;

    output =
        static_cast<int32_t>(
            token.toInt()
        );

    return true;
}


//==================================================================
// PARSE BINARY WORD
//------------------------------------------------------------------
// Converte una stringa binaria in una bitword uint32_t.
//
// La funzione accetta esclusivamente caratteri:
//
//   '0'
//   '1'
//
// Se la stringa supera 32 bit, vengono convertiti gli ultimi
// 32 bit. Per QPIWS la stringa completa viene comunque conservata
// anche nello Snapshot come String.
//==================================================================

static bool parseBinaryWord(
    const String& value,
    uint32_t& output)
{
    if (value.length() == 0)
        return false;

    const size_t firstIndex =
        value.length() > 32
            ? value.length() - 32
            : 0;

    uint32_t result = 0;

    for (size_t index = firstIndex;
         index < value.length();
         ++index)
    {
        const char bit =
            value[index];

        if (bit != '0' &&
            bit != '1')
        {
            return false;
        }

        result <<= 1U;

        if (bit == '1')
            result |= 1U;
    }

    output =
        result;

    return true;
}


//==================================================================
// PARSE BYTE CODE
//------------------------------------------------------------------
// Converte un codice protocollo numerico in uint8_t.
//
// Conserva il valore nativo senza interpretarlo semanticamente.
//==================================================================

static bool parseByteToken(
    const std::vector<String>& values,
    size_t index,
    uint8_t& output)
{
    uint32_t value = 0;

    if (!parseUIntToken(
            values,
            index,
            value))
    {
        return false;
    }

    output =
        static_cast<uint8_t>(
            value
        );

    return true;
}

//==================================================================
// QPI
//------------------------------------------------------------------
// Device Protocol ID Inquiry.
//
// Risposta tipica:
//
//   (PI30
//
// Il prefisso "PI" identifica il campo come protocol ID.
// Il valore viene conservato integralmente in:
//
//   snapshot.system.identity.protocolVersion
//
// Aggiorna:
//
//   snapshot.system.identity
//==================================================================

bool parseQPI(
    const String& response, uint32_t now)
{
    String value =
        response;

    value.trim();

    if (value.startsWith("("))
        value.remove(0, 1);

    if (!value.startsWith("PI"))
        return false;

    if (value.length() <= 2)
        return false;

    _snapshot.system.identity.protocolVersion =
        value.substring(2);

    _snapshot.system.valid =
        true;

    markSnapshotUpdated(now);

    return true;
}

//==================================================================
// QID / QSID
//------------------------------------------------------------------
// Device Serial Number Inquiry.
//
// Risposte tipiche:
//
//   (92932307100001
//   (12345678901234567890
//
// QID viene normalmente usato per seriali standard.
// QSID viene usato da varianti con seriale più lungo.
//
// Il payload viene conservato come stringa perché:
//   - può contenere zeri iniziali;
//   - può superare la capacità di uint64_t;
//   - rappresenta un identificatore, non una quantità.
//
// Aggiorna:
//
//   snapshot.system.identity.serialNumber
//==================================================================

bool parseDeviceSerial(
    const String& response, uint32_t now)
{
    String serial =
        response;

    serial.trim();

    if (serial.startsWith("("))
        serial.remove(0, 1);

    serial.trim();

    if (serial.length() == 0)
        return false;

    _snapshot.system.identity.serialNumber =
        serial;

    _snapshot.system.valid =
        true;

    markSnapshotUpdated(now);

    return true;
}

//==================================================================
// QMN
//------------------------------------------------------------------
// Model Name Inquiry.
//
// Risposta tipica:
//
//   (VMII-6200
//
// Il valore viene conservato senza traduzioni o normalizzazioni.
//
// Può essere utilizzato dal composition root per selezionare
// la variante protocollo:
//
//   - classic
//   - VMII / MAX
//   - QA6248 proprietario
//
// Aggiorna:
//
//   snapshot.system.identity.model
//==================================================================

bool parseQMN(
    const String& response, uint32_t now)
{
    String model =
        response;

    model.trim();

    if (model.startsWith("("))
        model.remove(0, 1);

    model.trim();

    if (model.length() == 0)
        return false;

    _snapshot.system.identity.model =
        model;

    _snapshot.system.valid =
        true;

    markSnapshotUpdated(now);

    return true;
}

//==================================================================
// QMOD
//------------------------------------------------------------------
// Device Operating Mode Inquiry.
//
// Risposta tipica:
//
//   (B
//
// Codici noti:
//
//   P = Power-on
//   S = Standby
//   L = Line
//   B = Battery
//   F = Fault
//   D = Shutdown
//   H = Power-saving
//
// Il driver conserva esclusivamente il codice nativo.
//
// Aggiorna:
//
//   snapshot.system.operating.modeCode
//==================================================================

bool parseQMOD(
    const String& response, uint32_t now)
{
    String value =
        response;

    value.trim();

    if (value.startsWith("("))
        value.remove(0, 1);

    value.trim();

    if (value.length() == 0)
        return false;

    const char mode =
        value[0];

    switch (mode)
    {
        case 'P':
        case 'S':
        case 'L':
        case 'B':
        case 'F':
        case 'D':
        case 'H':
            break;

        default:
            return false;
    }

    _snapshot.system.operating.modeCode =
        mode;

    _snapshot.system.valid =
        true;

    markSnapshotUpdated(now);

    return true;
}

//==================================================================
// QVFW
//------------------------------------------------------------------
// Main CPU Firmware Version Inquiry.
//
// Risposta tipica:
//
//   (VERFW:00123.01
//
// Aggiorna:
//
//   snapshot.system.identity.firmware
//==================================================================

bool parseQVFW(
    const String& response, uint32_t now)
{
    String value =
        response;

    value.trim();

    if (value.startsWith("("))
        value.remove(0, 1);

    const int separator =
        value.indexOf(':');

    if (separator < 0)
        return false;

    const String prefix =
        value.substring(
            0,
            separator
        );

    if (prefix != "VERFW")
        return false;

    String firmware =
        value.substring(
            separator + 1
        );

    firmware.trim();

    if (firmware.length() == 0)
        return false;

    _snapshot.system.identity.firmware =
        firmware;

    _snapshot.system.valid =
        true;

    markSnapshotUpdated(now);

    return true;
}


//==================================================================
// QVFW2
//------------------------------------------------------------------
// Secondary CPU Firmware Version Inquiry.
//
// Risposta tipica:
//
//   (VERFW2:00003.25
//
// Aggiorna:
//
//   snapshot.system.identity.secondaryFirmware
//==================================================================

bool parseQVFW2(
    const String& response, uint32_t now)
{
    String value =
        response;

    value.trim();

    if (value.startsWith("("))
        value.remove(0, 1);

    const int separator =
        value.indexOf(':');

    if (separator < 0)
        return false;

    const String prefix =
        value.substring(
            0,
            separator
        );

    if (prefix != "VERFW2")
        return false;

    String firmware =
        value.substring(
            separator + 1
        );

    firmware.trim();

    if (firmware.length() == 0)
        return false;

    _snapshot.system.identity.secondaryFirmware =
        firmware;

    _snapshot.system.valid =
        true;

    markSnapshotUpdated(now);

    return true;
}

//==================================================================
// QPIRI
//------------------------------------------------------------------
// Device Rating Information Inquiry.
//
// Layout Datouboss/Voltronic classic:
//
// [0]  grid rating voltage
// [1]  grid rating current
// [2]  AC output rating voltage
// [3]  AC output rating frequency
// [4]  AC output rating current
// [5]  AC output rating apparent power
// [6]  AC output rating active power
// [7]  battery rating voltage
// [8]  battery recharge voltage
// [9]  battery under-voltage
// [10] battery bulk voltage
// [11] battery float voltage
// [12] battery type code
// [13] max utility/AC charging current
// [14] max total charging current
// [15] AC input range code
// [16] output source priority code
// [17] charger source priority code
// [18] maximum parallel units
// [19] machine type code
// [20] topology code
// [21] output mode code
// [22] battery redischarge voltage
// [23] PV OK condition code
//
// I codici vengono conservati come valori nativi.
// Nessuna traduzione Home Assistant viene eseguita nel driver.
//
// Aggiorna:
//
//   snapshot.grid.rating
//   snapshot.output.rating
//   snapshot.settings
//==================================================================

bool parseQPIRI(
    const String& response, uint32_t now)
{
    const auto fields =
        tokenize(response);

    if (fields.size() < 24)
        return false;

    Snapshot::Grid grid =
        _snapshot.grid;

    Snapshot::Output output =
        _snapshot.output;

    Snapshot::Settings settings =
        _snapshot.settings;

    uint32_t numericValue = 0;

    // ----------------------------------------------------------
    // GRID RATING
    // ----------------------------------------------------------

    if (!parseFloatToken(
            fields,
            0,
            grid.rating.voltage))
    {
        return false;
    }

    if (!parseFloatToken(
            fields,
            1,
            grid.rating.current))
    {
        return false;
    }

    // ----------------------------------------------------------
    // OUTPUT RATING
    // ----------------------------------------------------------

    if (!parseFloatToken(
            fields,
            2,
            output.rating.voltage))
    {
        return false;
    }

    if (!parseFloatToken(
            fields,
            3,
            output.rating.frequency))
    {
        return false;
    }

    if (!parseFloatToken(
            fields,
            4,
            output.rating.current))
    {
        return false;
    }

    if (parseUIntToken(
            fields,
            5,
            numericValue))
    {
        output.rating.apparentPower =
            numericValue;
    }

    if (parseUIntToken(
            fields,
            6,
            numericValue))
    {
        output.rating.activePower =
            numericValue;

        settings.rating.inverterPower =
            numericValue;
    }

    // ----------------------------------------------------------
    // BATTERY VOLTAGES
    // ----------------------------------------------------------

    parseFloatToken(
        fields,
        7,
        settings.voltage.batteryRating
    );

    parseFloatToken(
        fields,
        8,
        settings.voltage.recharge
    );

    parseFloatToken(
        fields,
        9,
        settings.voltage.low
    );

    parseFloatToken(
        fields,
        10,
        settings.voltage.bulk
    );

    parseFloatToken(
        fields,
        11,
        settings.voltage.floating
    );

    // ----------------------------------------------------------
    // PROTOCOL CODES
    // ----------------------------------------------------------

    parseByteToken(
        fields,
        12,
        settings.mode.batteryType
    );

    if (parseUIntToken(
            fields,
            13,
            numericValue))
    {
        settings.current.maxUtilityCharge =
            static_cast<uint16_t>(
                numericValue
            );
    }

    if (parseUIntToken(
            fields,
            14,
            numericValue))
    {
        settings.current.maxCharge =
            static_cast<uint16_t>(
                numericValue
            );
    }

    parseByteToken(
        fields,
        15,
        settings.mode.acInputRange
    );

    parseByteToken(
        fields,
        16,
        settings.mode.outputSourcePriority
    );

    parseByteToken(
        fields,
        17,
        settings.mode.chargerSourcePriority
    );

    parseByteToken(
        fields,
        18,
        settings.mode.parallelMaxUnits
    );

    parseByteToken(
        fields,
        19,
        settings.mode.machineType
    );

    parseByteToken(
        fields,
        20,
        settings.mode.topology
    );

    parseByteToken(
        fields,
        21,
        settings.mode.outputMode
    );

    parseFloatToken(
        fields,
        22,
        settings.voltage.redischarge
    );

    parseByteToken(
        fields,
        23,
        settings.mode.pvOkCondition
    );

    grid.updatedAt =
        millis();

    output.updatedAt =
        millis();

    settings.valid =
        true;

    settings.updatedAt =
        millis();

    _snapshot.grid =
        grid;

    _snapshot.output =
        output;

    _snapshot.settings =
        settings;

    markSnapshotUpdated(now);

    return true;
}

//==================================================================
// QPIGS
//----------------------------------------------------------------------
// Device General Status Parameters Inquiry.
//
// QPIGS restituisce le principali misure runtime dell'inverter.
//
// Frame tipico:
//
// (230.0 50.0 230.0 50.0 0600 0500 008 400
//  52.40 010 095 035 0050 120.0 52.30 0020
//  10010110 00 00 01200 110
//
// Mapping dei campi:
//
//   [0]  Tensione rete                              [V]
//   [1]  Frequenza rete                             [Hz]
//   [2]  Tensione uscita inverter                   [V]
//   [3]  Frequenza uscita inverter                  [Hz]
//   [4]  Potenza apparente uscita                   [VA]
//   [5]  Potenza attiva uscita                      [W]
//   [6]  Carico inverter                            [%]
//   [7]  Tensione bus DC interno                    [V]
//   [8]  Tensione batteria                          [V]
//   [9]  Corrente di carica batteria                [A]
//   [10] Stato di carica batteria                   [%]
//   [11] Temperatura dissipatore                    [°C]
//   [12] Corrente FV verso batteria                 [A]
//   [13] Tensione ingresso FV                       [V]
//   [14] Tensione batteria rilevata dal regolatore  [V]
//   [15] Corrente di scarica batteria               [A]
//   [16] Bitword stato principale
//   [17] Offset ventola / campo diagnostico
//   [18] Versione EEPROM
//   [19] Potenza FV                                 [W]
//   [20] Bitword stato estesa
//
// Le bitword vengono conservate integralmente:
//
//   snapshot.system.status.statusWord
//   snapshot.system.status.statusWord2
//
// Non vengono suddivise in flag booleani o in aree distinte.
//
// La corrente netta batteria viene calcolata come:
//
//   correnteNetta = correnteCarica - correnteScarica
//
// Convenzione:
//
//   valore positivo = batteria in carica
//   valore negativo = batteria in scarica
//
// Aggiorna atomicamente:
//
//   snapshot.grid
//   snapshot.output
//   snapshot.battery
//   snapshot.pv
//   snapshot.internal
//   snapshot.system
//
// Restituisce false se il frame non contiene almeno i 17 campi
// obbligatori oppure se uno dei campi strutturali non è valido.
//==================================================================

bool parseQPIGS(
    const String& response, uint32_t now)
{
    const auto fields =
        tokenize(response);

    /*
     * I campi da 17 a 20 sono opzionali in alcune varianti.
     *
     * I campi 0..16 costituiscono il frame minimo valido.
     */
    if (fields.size() < 17)
        return false;

    // ----------------------------------------------------------
    // COPIE LOCALI
    //
    // Il commit nello Snapshot avviene solamente dopo il parsing
    // dei campi obbligatori.
    // ----------------------------------------------------------

    Snapshot::Grid grid =
        _snapshot.grid;

    Snapshot::Output output =
        _snapshot.output;

    Snapshot::Battery battery =
        _snapshot.battery;

    Snapshot::PV pv =
        _snapshot.pv;

    Snapshot::Internal internal =
        _snapshot.internal;

    Snapshot::System system =
        _snapshot.system;

    uint32_t unsignedValue = 0;

    // ==========================================================
    // GRID
    // ==========================================================

    if (!parseFloatToken(
            fields,
            0,
            grid.electrical.voltage))
    {
        return false;
    }

    if (!parseFloatToken(
            fields,
            1,
            grid.electrical.frequency))
    {
        return false;
    }

    /*
     * La disponibilità rete viene derivata dalla tensione.
     *
     * La soglia evita che valori residui prossimi allo zero
     * vengano interpretati come presenza rete.
     */
    grid.available =
        std::isfinite(
            grid.electrical.voltage
        ) &&
        grid.electrical.voltage > 20.0f;

    grid.valid =
        true;

    grid.updatedAt =
        now;

    // ==========================================================
    // OUTPUT
    // ==========================================================

    if (!parseFloatToken(
            fields,
            2,
            output.electrical.voltage))
    {
        return false;
    }

    if (!parseFloatToken(
            fields,
            3,
            output.electrical.frequency))
    {
        return false;
    }

    if (!parseUIntToken(
            fields,
            4,
            unsignedValue))
    {
        return false;
    }

    output.electrical.apparentPower =
        static_cast<float>(
            unsignedValue
        );

    if (!parseUIntToken(
            fields,
            5,
            unsignedValue))
    {
        return false;
    }

    output.electrical.activePower =
        static_cast<float>(
            unsignedValue
        );

    if (!parseUIntToken(
            fields,
            6,
            unsignedValue))
    {
        return false;
    }

    output.load.percent =
        static_cast<uint8_t>(
            unsignedValue > 100U
                ? 100U
                : unsignedValue
        );

    output.valid =
        true;

    output.updatedAt =
        now;

    // ==========================================================
    // INTERNAL DC BUS
    // ==========================================================

    if (parseUIntToken(
            fields,
            7,
            unsignedValue))
    {
        internal.electrical.busVoltage =
            static_cast<float>(
                unsignedValue
            );
    }

    // ==========================================================
    // BATTERY
    // ==========================================================

    if (!parseFloatToken(
            fields,
            8,
            battery.electrical.voltage))
    {
        return false;
    }

    if (!parseFloatToken(
            fields,
            9,
            battery.electrical.chargingCurrent))
    {
        return false;
    }

    if (!parseUIntToken(
            fields,
            10,
            unsignedValue))
    {
        return false;
    }

    battery.capacity.soc =
        static_cast<int16_t>(
            unsignedValue > 100U
                ? 100U
                : unsignedValue
        );

    battery.connected =
        std::isfinite(
            battery.electrical.voltage
        ) &&
        battery.electrical.voltage > 10.0f;

    // ==========================================================
    // HEATSINK TEMPERATURE
    // ==========================================================

    if (!parseFloatToken(
            fields,
            11,
            internal.thermal.heatsinkTemperature))
    {
        return false;
    }

    // ==========================================================
    // PHOTOVOLTAIC / SCC
    // ==========================================================

    if (!parseFloatToken(
            fields,
            12,
            pv.electrical.batteryCurrent))
    {
        return false;
    }

    if (!parseFloatToken(
            fields,
            13,
            pv.electrical.voltage))
    {
        return false;
    }

    if (!parseFloatToken(
            fields,
            14,
            pv.electrical.sccBatteryVoltage))
    {
        return false;
    }

    // ==========================================================
    // BATTERY DISCHARGE
    // ==========================================================

    if (!parseFloatToken(
            fields,
            15,
            battery.electrical.dischargeCurrent))
    {
        return false;
    }

    /*
     * Corrente netta:
     *
     *   positiva = carica
     *   negativa = scarica
     */
    battery.electrical.current =
        battery.electrical.chargingCurrent -
        battery.electrical.dischargeCurrent;

    if (std::isfinite(
            battery.electrical.voltage) &&
        std::isfinite(
            battery.electrical.current))
    {
        battery.electrical.power =
            battery.electrical.voltage *
            battery.electrical.current;
    }
    else
    {
        battery.electrical.power =
            NAN;
    }

    battery.valid =
        true;

    battery.updatedAt =
        now;

    // ==========================================================
    // PRIMARY STATUS BITWORD
    // ==========================================================

    if (!parseBinaryWord(
            fields[16],
            system.status.statusWord))
    {
        return false;
    }

    // ==========================================================
    // OPTIONAL FIELD 17: FAN OFFSET
    // ==========================================================

    if (fields.size() > 17)
    {
        const String& token =
            fields[17];

        if (token.length() > 0)
        {
            internal.diagnostic.fanOffset =
                static_cast<int32_t>(
                    token.toInt()
                );
        }
    }

    // ==========================================================
    // OPTIONAL FIELD 18: EEPROM VERSION
    // ==========================================================

    if (fields.size() > 18)
    {
        if (parseUIntToken(
                fields,
                18,
                unsignedValue))
        {
            internal.diagnostic.eepromVersion =
                unsignedValue;
        }
    }

    // ==========================================================
    // OPTIONAL FIELD 19: PV OUTPUT POWER
    // ==========================================================

    if (fields.size() > 19)
    {
        if (parseFloatToken(
                fields,
                19,
                pv.electrical.power))
        {
            /*
             * Se tensione e potenza sono valide, ricava la
             * corrente PV solamente quando il frame non ne
             * fornisce già una misura diretta affidabile.
             */
            if ((!std::isfinite(
                    pv.electrical.current) ||
                 pv.electrical.current < 0.0f) &&
                std::isfinite(
                    pv.electrical.voltage) &&
                pv.electrical.voltage > 0.1f)
            {
                pv.electrical.current =
                    pv.electrical.power /
                    pv.electrical.voltage;
            }
        }
    }

    // ==========================================================
    // OPTIONAL FIELD 20: EXTENDED STATUS BITWORD
    // ==========================================================

    if (fields.size() > 20)
    {
        if (!parseBinaryWord(
                fields[20],
                system.status.statusWord2))
        {
            /*
             * Il secondo status word è opzionale.
             *
             * Un valore non valido non deve invalidare l'intero
             * frame QPIGS già correttamente acquisito.
             */
            system.status.statusWord2 =
                0;
        }
    }

    // ==========================================================
    // VALIDITY AND TIMESTAMPS
    // ==========================================================

    pv.valid =
        std::isfinite(
            pv.electrical.voltage
        );

    pv.updatedAt =
        now;

    internal.valid =
        true;

    internal.updatedAt =
        now;

    system.valid =
        true;

    // ==========================================================
    // ATOMIC COMMIT
    // ==========================================================

    _snapshot.grid =
        grid;

    _snapshot.output =
        output;

    _snapshot.battery =
        battery;

    _snapshot.pv =
        pv;

    _snapshot.internal =
        internal;

    _snapshot.system =
        system;

    markSnapshotUpdated(now);

    return true;
}

//==================================================================
// QPIWS
//------------------------------------------------------------------
// Device Warning Status Inquiry.
//
// Risposta tipica:
//
//   (0000000000000000000000000000000000
//
// La risposta rappresenta una bitword di warning/fault.
//
// La bitword viene conservata integralmente come String.
//
// Non viene effettuato lo sbittamento in:
//   - booleani nello Snapshot;
//   - aree DomoManager distinte.
//
// La decodifica può essere eseguita successivamente da:
//   - semantic resolver;
//   - diagnostica;
//   - Home Assistant;
//   - helper bit-oriented.
//
// Aggiorna:
//
//   snapshot.system.status.warningBitWord
//==================================================================

bool parseQPIWS(
    const String& response, uint32_t now)
{
    String bitWord =
        response;

    bitWord.trim();

    if (bitWord.startsWith("("))
        bitWord.remove(0, 1);

    bitWord.trim();

    if (bitWord.length() < 16)
        return false;

    for (size_t index = 0;
         index < bitWord.length();
         ++index)
    {
        if (bitWord[index] != '0' &&
            bitWord[index] != '1')
        {
            return false;
        }
    }

    _snapshot.system.status.warningBitWord =
        bitWord;

    _snapshot.system.valid =
        true;

    markSnapshotUpdated(now);

    return true;
}

//==================================================================
// QDI
//------------------------------------------------------------------
// Default Setting Value Information Inquiry.
//
// Layout usato dalle varianti Voltronic/Datouboss:
//
// [0]  AC output rating voltage
// [1]  AC output rating frequency
// [2]  max utility/AC charging current
// [3]  battery under-voltage
// [4]  battery float voltage
// [5]  battery bulk voltage
// [6]  battery recharge voltage
// [7]  max total charging current
// [8]  AC input range code
// [9]  output source priority code
// [10] charger source priority code
// [11] battery type code
// ...
// [21] output mode code
// [22] battery redischarge voltage
// [23] PV OK condition code
//
// I campi non documentati o non verificati non vengono consumati.
//
// Aggiorna:
//
//   snapshot.output.rating
//   snapshot.settings
//==================================================================

bool parseQDI(
    const String& response, uint32_t now)
{
    const auto fields =
        tokenize(response);

    if (fields.size() < 24)
        return false;

    Snapshot::Output output =
        _snapshot.output;

    Snapshot::Settings settings =
        _snapshot.settings;

    uint32_t numericValue = 0;

    if (!parseFloatToken(
            fields,
            0,
            output.rating.voltage))
    {
        return false;
    }

    if (!parseFloatToken(
            fields,
            1,
            output.rating.frequency))
    {
        return false;
    }

    if (parseUIntToken(
            fields,
            2,
            numericValue))
    {
        settings.current.maxUtilityCharge =
            static_cast<uint16_t>(
                numericValue
            );
    }

    parseFloatToken(
        fields,
        3,
        settings.voltage.low
    );

    parseFloatToken(
        fields,
        4,
        settings.voltage.floating
    );

    parseFloatToken(
        fields,
        5,
        settings.voltage.bulk
    );

    parseFloatToken(
        fields,
        6,
        settings.voltage.recharge
    );

    if (parseUIntToken(
            fields,
            7,
            numericValue))
    {
        settings.current.maxCharge =
            static_cast<uint16_t>(
                numericValue
            );
    }

    parseByteToken(
        fields,
        8,
        settings.mode.acInputRange
    );

    parseByteToken(
        fields,
        9,
        settings.mode.outputSourcePriority
    );

    parseByteToken(
        fields,
        10,
        settings.mode.chargerSourcePriority
    );

    parseByteToken(
        fields,
        11,
        settings.mode.batteryType
    );

    if (fields.size() > 21)
    {
        parseByteToken(
            fields,
            21,
            settings.mode.outputMode
        );
    }

    if (fields.size() > 22)
    {
        parseFloatToken(
            fields,
            22,
            settings.voltage.redischarge
        );
    }

    if (fields.size() > 23)
    {
        parseByteToken(
            fields,
            23,
            settings.mode.pvOkCondition
        );
    }

    output.updatedAt =
        now;

    settings.valid =
        true;

    settings.updatedAt =
        now;

    _snapshot.output =
        output;

    _snapshot.settings =
        settings;

    markSnapshotUpdated(now);

    return true;
}

private:

    //==================================================================
    // DISCOVERY HELPERS
    //==================================================================

    //==================================================================
    // REBUILD COMMAND DIAGNOSTICS
    //----------------------------------------------------------------------
    // Ricostruisce la vista diagnostica dei comandi registrati.
    //
    // Il framing viene copiato integralmente dalla definizione del comando.
    //==================================================================

    void rebuildCommandDiagnostics()
    {
        _diagnostics.commands.clear();

        for (const auto& definition :
            _commandDefinitions)
        {
            Diagnostics::Command command;

            command.name =
                definition.name;

            command.category =
                definition.category;

            command.state =
                CommandState::Unknown;

            command.framing =
                definition.framing;

            command.enabled =
                true;

            command.pollable =
                definition.pollable;

            command.discoveryEnabled =
                definition.discoveryEnabled;

            command.pollIntervalMs =
                definition.pollIntervalMs;

            command.lastPollAt =
                0;

            command.nextPollAt =
                0;

            _diagnostics.commands.push_back(
                command
            );
        }

        recalculateCapabilityCounters();
    }

    Diagnostics::Command*
    findCommandMutable(
        const String& command)
    {
        for (auto& entry :
             _diagnostics.commands)
        {
            if (entry.name == command)
                return &entry;
        }

        return nullptr;
    }

    static CommandState
    commandStateFromResult(
        QueryResult result)
    {
        switch (result)
        {
            case QueryResult::Success:
                return CommandState::Supported;

            case QueryResult::Nak:
                return CommandState::NotSupported;

            case QueryResult::Timeout:
                return CommandState::Timeout;

            case QueryResult::TransportError:
                return CommandState::TransportError;

            case QueryResult::InvalidResponse:
            case QueryResult::ParseError:
            default:
                return CommandState::InvalidResponse;
        }
    }

    void recalculateCapabilityCounters()
    {
        _diagnostics.supportedCommands = 0;
        _diagnostics.unsupportedCommands = 0;
        _diagnostics.timeoutCommands = 0;
        _diagnostics.invalidCommands = 0;
        _diagnostics.unknownCommands = 0;

        for (const auto& command :
             _diagnostics.commands)
        {
            switch (command.state)
            {
                case CommandState::Supported:
                    _diagnostics.supportedCommands++;
                    break;

                case CommandState::NotSupported:
                    _diagnostics.unsupportedCommands++;
                    break;

                case CommandState::Timeout:
                    _diagnostics.timeoutCommands++;
                    break;

                case CommandState::TransportError:
                case CommandState::InvalidResponse:
                    _diagnostics.invalidCommands++;
                    break;

                case CommandState::Unknown:
                default:
                    _diagnostics.unknownCommands++;
                    break;
            }
        }
    }

    //==================================================================
    // QUERY CLASSIFICATION
    //==================================================================

    static QueryResult classifyResponse(
        const String& response)
    {
        if (response.length() == 0)
            return QueryResult::Timeout;

        if (
            response.indexOf("NAK") >= 0 ||
            response.indexOf("(NAK") >= 0)
        {
            return QueryResult::Nak;
        }

        if (!response.startsWith("("))
            return QueryResult::InvalidResponse;

        return QueryResult::Success;
    }

    static void trimResponse(
        String& response)
    {
        response.replace("\r", "");
        response.replace("\n", "");
        response.trim();
    }

    //==================================================================
    // DIAGNOSTICA
    //==================================================================

    void registerResult(
        Diagnostics::Command* command,
        QueryResult result,
        const String& response,
        uint32_t timestamp,
        uint32_t latency)
    {
        if (command)
        {
            command->lastResult = result;
            command->lastResponse = response;
            command->lastLatencyMs = latency;

            switch (result)
            {
                case QueryResult::Success:
                    command->successes++;
                    command->lastSuccessAt =
                        timestamp;
                    break;

                case QueryResult::Nak:
                    command->failures++;
                    command->naks++;
                    command->lastFailureAt =
                        timestamp;
                    break;

                case QueryResult::Timeout:
                    command->failures++;
                    command->timeouts++;
                    command->lastFailureAt =
                        timestamp;
                    break;

                case QueryResult::ParseError:
                    command->failures++;
                    command->parseErrors++;
                    command->lastFailureAt =
                        timestamp;
                    break;

                case QueryResult::InvalidResponse:
                    command->failures++;
                    command->invalidResponses++;
                    command->lastFailureAt =
                        timestamp;
                    break;

                case QueryResult::TransportError:
                    command->failures++;
                    command->transportErrors++;

                    command->lastFailureAt =
                        timestamp;

                    break;
            }
        }

        switch (result)
        {
            case QueryResult::Success:
                _diagnostics.successfulQueries++;
                break;

            case QueryResult::Nak:
                _diagnostics.failedQueries++;
                _diagnostics.nakResponses++;
                break;

            case QueryResult::Timeout:
                _diagnostics.failedQueries++;
                _diagnostics.timeoutResponses++;
                break;

            case QueryResult::TransportError:
                _diagnostics.failedQueries++;
                _diagnostics.transportErrors++;
                break;

            case QueryResult::InvalidResponse:
                _diagnostics.failedQueries++;
                _diagnostics.invalidResponses++;
                break;

            case QueryResult::ParseError:
                _diagnostics.failedQueries++;
                _diagnostics.parseErrors++;
                break;
        }
    }

    void recordFrame(
        const String& command,
        const String& tx,
        const String& rx,
        QueryResult result,
        uint32_t timestamp,
        uint32_t latency)
    {
        Diagnostics::Frame& frame =
            _diagnostics.history[
                _diagnostics.historyIndex %
                Diagnostics::HistorySize];

        frame.timestamp = timestamp;
        frame.command = command;
        frame.tx = tx;
        frame.rx = rx;
        frame.result = result;

        frame.success =
            result == QueryResult::Success;

        frame.nak =
            result == QueryResult::Nak;

        frame.timeout =
            result == QueryResult::Timeout;

        frame.latencyMs = latency;

        _diagnostics.historyIndex =
            (_diagnostics.historyIndex + 1) %
            Diagnostics::HistorySize;

        if (
            _diagnostics.historyCount <
            Diagnostics::HistorySize)
        {
            _diagnostics.historyCount++;
        }

        if (_frameCallback)
            _frameCallback(frame);
    }

    //==================================================================
    // SNAPSHOT CHANGE DETECTION
    //==================================================================

    void notifySnapshotIfChanged(
        const Snapshot& previous)
    {
        if (!snapshotEquals(previous, _snapshot))
        {
            if (_snapshotChangedCallback)
            {
                _snapshotChangedCallback(
                    previous,
                    _snapshot);
            }
        }
    }

    static bool floatEqual(
        float a,
        float b,
        float epsilon = 0.0001f)
    {
        if (std::isnan(a) && std::isnan(b))
            return true;

        if (std::isnan(a) != std::isnan(b))
            return false;

        return fabsf(a - b) <= epsilon;
    }

    static bool snapshotEquals(
        const Snapshot& a,
        const Snapshot& b)
    {
        return

            // =====================================================
            // SYSTEM
            // =====================================================

            a.system.valid ==
                b.system.valid &&

            a.system.connected ==
                b.system.connected &&

            a.system.identity.protocol ==
                b.system.identity.protocol &&

            a.system.identity.protocolVersion ==
                b.system.identity.protocolVersion &&

            a.system.identity.model ==
                b.system.identity.model &&

            a.system.identity.serialNumber ==
                b.system.identity.serialNumber &&

            a.system.identity.firmware ==
                b.system.identity.firmware &&

            a.system.identity.secondaryFirmware ==
                b.system.identity.secondaryFirmware &&

            a.system.operating.modeCode ==
                b.system.operating.modeCode &&

            a.system.operating.errorCode ==
                b.system.operating.errorCode &&

            a.system.status.statusWord ==
                b.system.status.statusWord &&

            a.system.status.statusWord2 ==
                b.system.status.statusWord2 &&

            a.system.status.warningBitWord ==
                b.system.status.warningBitWord &&

            a.system.status.flagsRaw ==
                b.system.status.flagsRaw &&

            // =====================================================
            // PRIMARY FLAGS
            // =====================================================

            a.system.status.primary.a ==
                b.system.status.primary.a &&

            a.system.status.primary.outputActive ==
                b.system.status.primary.outputActive &&

            a.system.status.primary.batteryLowAlarm ==
                b.system.status.primary.batteryLowAlarm &&

            a.system.status.primary.batteryDisconnected ==
                b.system.status.primary.batteryDisconnected &&

            a.system.status.primary.overload ==
                b.system.status.primary.overload &&

            a.system.status.primary.overTemperature ==
                b.system.status.primary.overTemperature &&

            a.system.status.primary.configurationChanged ==
                b.system.status.primary.configurationChanged &&

            a.system.status.primary.inverterOn ==
                b.system.status.primary.inverterOn &&

            a.system.status.primary.pvPowerLow ==
                b.system.status.primary.pvPowerLow &&

            a.system.status.primary.inputVoltageHigh ==
                b.system.status.primary.inputVoltageHigh &&

            a.system.status.primary.batteryVoltageHigh ==
                b.system.status.primary.batteryVoltageHigh &&

            a.system.status.primary.fanFault ==
                b.system.status.primary.fanFault &&

            // =====================================================
            // EXTENDED FLAGS
            // =====================================================

            a.system.status.extended.m ==
                b.system.status.extended.m &&

            a.system.status.extended.p ==
                b.system.status.extended.p &&

            a.system.status.extended.q ==
                b.system.status.extended.q &&

            a.system.status.extended.dustproofInstalled ==
                b.system.status.extended.dustproofInstalled &&

            a.system.status.extended.s ==
                b.system.status.extended.s &&

            a.system.status.extended.switchOn ==
                b.system.status.extended.switchOn &&

            a.system.status.extended.u ==
                b.system.status.extended.u &&

            a.system.status.extended.v ==
                b.system.status.extended.v &&

            a.system.status.extended.w ==
                b.system.status.extended.w &&

            a.system.status.extended.x ==
                b.system.status.extended.x &&

            a.system.status.extended.y ==
                b.system.status.extended.y &&

            // =====================================================
            // PV
            // =====================================================

            a.pv.valid ==
                b.pv.valid &&

            a.pv.electrical.voltage ==
                b.pv.electrical.voltage &&

            a.pv.electrical.current ==
                b.pv.electrical.current &&

            a.pv.electrical.power ==
                b.pv.electrical.power &&

            a.pv.electrical.sccBatteryVoltage ==
                b.pv.electrical.sccBatteryVoltage &&

            a.pv.limits.ratedPower ==
                b.pv.limits.ratedPower &&

            a.pv.thermal.temperature ==
                b.pv.thermal.temperature &&

            // =====================================================
            // BATTERY
            // =====================================================

            a.battery.valid ==
                b.battery.valid &&

            a.battery.connected ==
                b.battery.connected &&

            a.battery.metadata.cells ==
                b.battery.metadata.cells &&

            a.battery.electrical.voltage ==
                b.battery.electrical.voltage &&

            a.battery.electrical.current ==
                b.battery.electrical.current &&

            a.battery.electrical.power ==
                b.battery.electrical.power &&

            a.battery.electrical.chargingCurrent ==
                b.battery.electrical.chargingCurrent &&

            a.battery.electrical.dischargeCurrent ==
                b.battery.electrical.dischargeCurrent &&

            a.battery.capacity.soc ==
                b.battery.capacity.soc &&

            a.battery.status.statusWord ==
                b.battery.status.statusWord &&

            a.battery.status.warningWord ==
                b.battery.status.warningWord &&

            a.battery.status.faultWord ==
                b.battery.status.faultWord &&

            a.battery.status.rawFlags ==
                b.battery.status.rawFlags &&

            a.battery.status.chargeSwitch ==
                b.battery.status.chargeSwitch &&

            a.battery.status.solarChargeSwitch ==
                b.battery.status.solarChargeSwitch &&

            a.battery.status.acChargeSwitch ==
                b.battery.status.acChargeSwitch &&

            a.battery.status.lithiumActivateSwitch ==
                b.battery.status.lithiumActivateSwitch &&

            a.battery.status.lithiumActivateProcess ==
                b.battery.status.lithiumActivateProcess &&

            a.battery.status.reservedFlag ==
                b.battery.status.reservedFlag &&

            a.battery.status.batteryStatus ==
                b.battery.status.batteryStatus &&

            a.battery.status.chargeActive ==
                b.battery.status.chargeActive &&

            a.battery.status.sccCharging ==
                b.battery.status.sccCharging &&

            a.battery.status.acCharging ==
                b.battery.status.acCharging &&

            // =====================================================
            // GRID
            // =====================================================

            a.grid.valid ==
                b.grid.valid &&

            a.grid.available ==
                b.grid.available &&

            a.grid.electrical.voltage ==
                b.grid.electrical.voltage &&

            a.grid.electrical.frequency ==
                b.grid.electrical.frequency &&

            // =====================================================
            // OUTPUT
            // =====================================================

            a.output.valid ==
                b.output.valid &&

            a.output.electrical.voltage ==
                b.output.electrical.voltage &&

            a.output.electrical.frequency ==
                b.output.electrical.frequency &&

            a.output.electrical.current ==
                b.output.electrical.current &&

            a.output.electrical.activePower ==
                b.output.electrical.activePower &&

            a.output.electrical.apparentPower ==
                b.output.electrical.apparentPower &&

            a.output.load.percent ==
                b.output.load.percent &&

            // =====================================================
            // INTERNAL
            // =====================================================

            a.internal.valid ==
                b.internal.valid &&

            a.internal.electrical.busVoltage ==
                b.internal.electrical.busVoltage &&

            a.internal.electrical.outputDcComponent ==
                b.internal.electrical.outputDcComponent &&

            a.internal.electrical.inductorCurrent ==
                b.internal.electrical.inductorCurrent &&

            a.internal.thermal.heatsinkTemperature ==
                b.internal.thermal.heatsinkTemperature &&

            a.internal.thermal.pvTemperature ==
                b.internal.thermal.pvTemperature &&

            a.internal.thermal.boostTemperature ==
                b.internal.thermal.boostTemperature &&

            a.internal.thermal.maximumTemperature ==
                b.internal.thermal.maximumTemperature &&

            a.internal.fan.speed1 ==
                b.internal.fan.speed1 &&

            a.internal.fan.speed2 ==
                b.internal.fan.speed2 &&

            a.internal.fan.status1 ==
                b.internal.fan.status1 &&

            a.internal.fan.status2 ==
                b.internal.fan.status2 &&

            a.internal.diagnostic.fanOffset ==
                b.internal.diagnostic.fanOffset &&

            a.internal.diagnostic.eepromVersion ==
                b.internal.diagnostic.eepromVersion &&

            a.internal.diagnostic.hopInternalData ==
                b.internal.diagnostic.hopInternalData &&

            a.internal.diagnostic.temperatureInternalData ==
                b.internal.diagnostic.temperatureInternalData;
    }

    static bool isDue(
        uint32_t now,
        uint32_t target)
    {
        return
            static_cast<int32_t>(
                now - target) >= 0;
    }

    //==================================================================
    // RESET RUNTIME STATE
    //------------------------------------------------------------------
    // Ripristina Snapshot, diagnostica, registry e cursore round-robin.
    //==================================================================

    void resetRuntimeState()
    {
        _snapshot =
            Snapshot {};

        _diagnostics =
            Diagnostics {};

        _commandDefinitions.clear();

        _connected =
            false;

        _started =
            false;

        _pendingTransaction.reset();

        _discoveryRuntime.reset();

        _pollingRuntime.reset();

        _transport.resetFrameReader();
    }


protected:

    ISolarTransport& _transport;

    Snapshot _snapshot {};
    Diagnostics _diagnostics {};

    std::vector<CommandDefinition>
        _commandDefinitions;

    SnapshotChangedCallback
        _snapshotChangedCallback;

    FrameCallback
        _frameCallback;

    CommandStateCallback
        _commandStateCallback;

    uint32_t _queryTimeoutMs = 2000;

    bool _started = false;
    bool _connected = false;

    //==================================================================
    // CRC16/XMODEM
    //----------------------------------------------------------------------
    // Calcola il CRC16/XMODEM sul payload.
    //
    // Parametri:
    //   polynomial = 0x1021
    //   initial    = 0x0000
    //   refin      = false
    //   refout     = false
    //==================================================================

    static uint16_t crc16Xmodem(
        const uint8_t* data,
        size_t length)
    {
        if (!data || length == 0)
            return 0;

        uint16_t crc =
            0x0000U;

        for (size_t index = 0;
            index < length;
            ++index)
        {
            crc ^=
                static_cast<uint16_t>(
                    data[index]
                ) << 8U;

            for (uint8_t bit = 0;
                bit < 8;
                ++bit)
            {
                if ((crc & 0x8000U) != 0U)
                {
                    crc =
                        static_cast<uint16_t>(
                            (crc << 1U) ^
                            0x1021U
                        );
                }
                else
                {
                    crc =
                        static_cast<uint16_t>(
                            crc << 1U
                        );
                }
            }
        }

        return crc;
    }


    //==================================================================
    // ESCAPE CRC BYTE
    //----------------------------------------------------------------------
    // Il protocollo Voltronic non consente che i byte CRC contengano:
    //
    //   0x28  '('
    //   0x0D  CR
    //   0x0A  LF
    //
    // Quando uno dei due byte CRC assume uno di questi valori,
    // viene incrementato di uno.
    //==================================================================

    static uint8_t escapeProtocolCrcByte(
        uint8_t value)
    {
        switch (value)
        {
            case 0x28U:
            case 0x0DU:
            case 0x0AU:
                return
                    static_cast<uint8_t>(
                        value + 1U
                    );

            default:
                return value;
        }
    }


    //==================================================================
    // APPEND PROTOCOL CRC
    //----------------------------------------------------------------------
    // Calcola il CRC16/XMODEM del contenuto corrente del frame e aggiunge
    // i due byte CRC escaped in ordine big-endian.
    //==================================================================

    static void appendProtocolCrc(
        std::vector<uint8_t>& frame)
    {
        const uint16_t crc =
            crc16Xmodem(
                frame.data(),
                frame.size()
            );

        const uint8_t crcHigh =
            escapeProtocolCrcByte(
                static_cast<uint8_t>(
                    (crc >> 8U) & 0xFFU
                )
            );

        const uint8_t crcLow =
            escapeProtocolCrcByte(
                static_cast<uint8_t>(
                    crc & 0xFFU
                )
            );

        frame.push_back(
            crcHigh
        );

        frame.push_back(
            crcLow
        );
    }


    //==================================================================
    // BUILD COMMAND FRAME
    //----------------------------------------------------------------------
    // Costruisce il frame binario completo usando il framing associato
    // alla definizione del comando.
    //==================================================================

    static std::vector<uint8_t> buildCommandFrame(
        const String& command,
        CommandFraming framing)
    {
        std::vector<uint8_t> frame;

        const bool usesCrc =
            framing ==
                CommandFraming::CRC_XMODEM_CR;

        size_t terminatorSize = 1U;

        if (framing == CommandFraming::CRLF)
            terminatorSize = 2U;

        frame.reserve(
            command.length() +
            (usesCrc ? 2U : 0U) +
            terminatorSize
        );

        for (size_t index = 0;
            index < command.length();
            ++index)
        {
            frame.push_back(
                static_cast<uint8_t>(
                    command[index]
                )
            );
        }

        if (usesCrc)
        {
            appendProtocolCrc(
                frame
            );
        }

        switch (framing)
        {
            case CommandFraming::LF:
            {
                frame.push_back(
                    0x0AU
                );

                break;
            }

            case CommandFraming::CRLF:
            {
                frame.push_back(
                    0x0DU
                );

                frame.push_back(
                    0x0AU
                );

                break;
            }

            case CommandFraming::CRC_XMODEM_CR:
            case CommandFraming::CR:
            default:
            {
                frame.push_back(
                    0x0DU
                );

                break;
            }
        }

        return frame;
    }


    //==================================================================
    // EXTRACT RESPONSE PAYLOAD
    //----------------------------------------------------------------------
    // Estrae il payload applicativo dal frame ricevuto.
    //
    // Per CRC_XMODEM_CR:
    //
    //   payload + CRC_H + CRC_L
    //
    // Per gli altri framing:
    //
    //   l'intero frame è considerato payload.
    //
    // Il transport ha già rimosso CR, LF o CRLF.
    //==================================================================

    static bool extractResponsePayload(
        const std::vector<uint8_t>& frame,
        CommandFraming framing,
        String& payload)
    {
        payload = "";

        if (frame.empty())
            return false;

        const bool usesCrc =
            framing ==
                CommandFraming::CRC_XMODEM_CR;

        size_t payloadLength =
            frame.size();

        if (usesCrc)
        {
            if (frame.size() < 3U)
                return false;

            payloadLength =
                frame.size() - 2U;

            const uint16_t calculatedCrc =
                crc16Xmodem(
                    frame.data(),
                    payloadLength
                );

            const uint8_t expectedHigh =
                escapeProtocolCrcByte(
                    static_cast<uint8_t>(
                        (calculatedCrc >> 8U) &
                        0xFFU
                    )
                );

            const uint8_t expectedLow =
                escapeProtocolCrcByte(
                    static_cast<uint8_t>(
                        calculatedCrc &
                        0xFFU
                    )
                );

            const uint8_t receivedHigh =
                frame[payloadLength];

            const uint8_t receivedLow =
                frame[payloadLength + 1U];

            if (receivedHigh != expectedHigh ||
                receivedLow != expectedLow)
            {
                return false;
            }
        }

        payload.reserve(
            payloadLength
        );

        for (size_t index = 0;
            index < payloadLength;
            ++index)
        {
            payload +=
                static_cast<char>(
                    frame[index]
                );
        }

        return
            payload.length() > 0;
    }


    //==================================================================
    // FRAME TO HEX STRING
    //----------------------------------------------------------------------
    // Converte un frame binario in una rappresentazione leggibile per
    // diagnostica.
    //
    // Esempio:
    //
    //   51 50 49 47 53 B7 A9 0D
    //==================================================================

    static String frameToHex(
        const std::vector<uint8_t>& frame)
    {
        static constexpr char Hex[] =
            "0123456789ABCDEF";

        String output;

        if (frame.empty())
            return output;

        output.reserve(
            frame.size() * 3U
        );

        for (size_t index = 0;
            index < frame.size();
            ++index)
        {
            if (index > 0)
                output += ' ';

            const uint8_t value =
                frame[index];

            output +=
                Hex[(value >> 4U) & 0x0FU];

            output +=
                Hex[value & 0x0FU];
        }

        return output;
    }
};

//======================================================================
// QA6248Inverter
//======================================================================

class QA6248Inverter :
    public SolarInverterBase
{
public:

    explicit QA6248Inverter(
        ISolarTransport& transport,
        uint32_t queryTimeoutMs = 2000)
        : SolarInverterBase(
            transport,
            queryTimeoutMs)
    {
    }

    const char*
    protocolName() const override
    {
        return "QA6248";
    }

    const char*
    modelName() const override
    {
        return "Datouboss/ZeroEra HY6248";
    }

protected:

    //==================================================================
    // REGISTER PROTOCOL COMMANDS
    //------------------------------------------------------------------
    // Registra prima i comandi standard Datouboss/Voltronic e poi
    // le estensioni proprietarie QA6248.
    //
    // Gli intervalli sono divisi per categoria:
    //
    //   realtime      2 secondi
    //   diagnostic    30-60 secondi
    //   configuration 5 minuti
    //   identity      solo discovery
    //
    // I tempi possono successivamente essere iniettati dalla
    // FrontendConfig::Solar.
    //==================================================================

    void registerProtocolCommands() override
    {
        // ==========================================================
        // VOLTRONIC / DATOUBOSS COMPATIBILITY
        //
        // Questi comandi vengono mantenuti anche quando il modello
        // QA6248 corrente restituisce NAK. Potranno essere supportati
        // da altri firmware o modelli.
        // ==========================================================

        registerCommand(
            "QPI",
            CommandCategory::Diagnostic,
            false,
            0,
            true,
            CommandFraming::CRC_XMODEM_CR
        );

        registerCommand(
            "QID",
            CommandCategory::Diagnostic,
            false,
            0,
            true,
            CommandFraming::CRC_XMODEM_CR
        );

        registerCommand(
            "QSID",
            CommandCategory::Diagnostic,
            false,
            0,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "QMN",
            CommandCategory::Diagnostic,
            false,
            0,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "QVFW",
            CommandCategory::Diagnostic,
            false,
            0,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "QVFW2",
            CommandCategory::Diagnostic,
            false,
            0,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "QMOD",
            CommandCategory::Realtime,
            true,
            5000,
            true,
            CommandFraming::CRC_XMODEM_CR
        );

        registerCommand(
            "QPIGS",
            CommandCategory::Realtime,
            true,
            2000,
            true,
            CommandFraming::CRC_XMODEM_CR
        );

        registerCommand(
            "QPIWS",
            CommandCategory::Diagnostic,
            true,
            30000,
            true,
            CommandFraming::CRC_XMODEM_CR
        );

        registerCommand(
            "QPIRI",
            CommandCategory::Configuration,
            true,
            300000,
            true,
            CommandFraming::CRC_XMODEM_CR
        );

        registerCommand(
            "QDI",
            CommandCategory::Configuration,
            false,
            0,
            true,
            CommandFraming::CR
        );

        // ==========================================================
        // QA6248 REALTIME
        //
        // I comandi H* di lettura non usano CRC:
        //
        //   command + CR
        // ==========================================================

        registerCommand(
            "HSTS",
            CommandCategory::Realtime,
            true,
            2000,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "HBAT",
            CommandCategory::Realtime,
            true,
            2000,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "HOP",
            CommandCategory::Realtime,
            true,
            2000,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "HPV",
            CommandCategory::Realtime,
            true,
            2000,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "HTEMP",
            CommandCategory::Diagnostic,
            true,
            10000,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "HGRID",
            CommandCategory::Realtime,
            true,
            2000,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "HPVB",
            CommandCategory::Realtime,
            true,
            5000,
            true,
            CommandFraming::CR
        );

        // ==========================================================
        // QA6248 ENERGY
        // ==========================================================

        registerCommand(
            "HGEN",
            CommandCategory::Energy,
            true,
            60000,
            true,
            CommandFraming::CR
        );

        // ==========================================================
        // QA6248 CONFIGURATION
        // ==========================================================

        registerCommand(
            "HEEP1",
            CommandCategory::Configuration,
            true,
            300000,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "HEEP2",
            CommandCategory::Configuration,
            true,
            300000,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "HEEP3",
            CommandCategory::Configuration,
            true,
            300000,
            true,
            CommandFraming::CR
        );

        registerCommand(
            "HEEP4",
            CommandCategory::Configuration,
            false,
            0,
            true,
            CommandFraming::CR
        );
    }

    //==================================================================
    // PROTOCOL RESPONSE DISPATCHER
    //------------------------------------------------------------------
    // Instrada ogni risposta verso il parser specifico.
    //
    // Sono supportati due insiemi:
    //
    //   QA6248 proprietario:
    //     HPV
    //     HBAT
    //     HGEN
    //     HEEP1
    //     HEEP2
    //     HEEP3
    //
    //   Voltronic/Datouboss compatibile:
    //     QPI
    //     QID
    //     QSID
    //     QMN
    //     QMOD
    //     QVFW
    //     QVFW2
    //     QPIGS
    //     QPIRI
    //     QPIWS
    //     QDI
    //
    // Un comando riconosciuto ma con payload invalido restituisce false.
    //==================================================================

    bool parseProtocolResponse(
        const String& command,
        const String& response) override
    {
        const uint32_t now =
            millis();

        // ==========================================================
        // QA6248 REALTIME
        // ==========================================================

        if (command == "HSTS")
            return parseHSTS(response, now);

        if (command == "HBAT")
            return parseHBAT(response, now);

        if (command == "HOP")
            return parseHOP(response, now);

        if (command == "HPV")
            return parseHPV(response, now);

        if (command == "HTEMP")
            return parseHTEMP(response, now);

        if (command == "HGRID")
            return parseHGRID(response, now);

        if (command == "HPVB")
            return parseHPVB(response, now);

        // ==========================================================
        // QA6248 ENERGY
        // ==========================================================

        if (command == "HGEN")
            return parseHGEN(response, now);

        // ==========================================================
        // QA6248 CONFIGURATION
        // ==========================================================

        if (command == "HEEP1")
            return parseHEEP1(response, now);

        if (command == "HEEP2")
            return parseHEEP2(response, now);

        if (command == "HEEP3")
            return parseHEEP3(response, now);

        /*
        * HEEP4 viene mantenuto nel registry per discovery e
        * compatibilità futura, ma non dispone ancora di un parser.
        */
        if (command == "HEEP4")
            return false;

        // ==========================================================
        // VOLTRONIC / DATOUBOSS IDENTITY
        // ==========================================================

        if (command == "QPI")
            return parseQPI(response, now);

        if (command == "QID" ||
            command == "QSID")
        {
            return parseDeviceSerial(
                response,
                now
            );
        }

        if (command == "QMN")
            return parseQMN(response, now);

        if (command == "QMOD")
            return parseQMOD(response, now);

        if (command == "QVFW")
            return parseQVFW(response, now);

        if (command == "QVFW2")
            return parseQVFW2(response, now);

        // ==========================================================
        // VOLTRONIC / DATOUBOSS RUNTIME AND CONFIGURATION
        // ==========================================================

        if (command == "QPIGS")
            return parseQPIGS(response, now);

        if (command == "QPIRI")
            return parseQPIRI(response, now);

        if (command == "QPIWS")
            return parseQPIWS(response, now);

        if (command == "QDI")
            return parseQDI(response, now);

        return false;
    }

private:

    //==================================================================
    // HPV
    //------------------------------------------------------------------
    // Snapshot fotovoltaico.
    //
    // Frame reale:
    //
    // (023.2 00.0 00000 00000.0 00000 0 000.0 027 08500
    //
    // Mapping:
    //
    // [0] tensione FV [V]
    // [1] corrente FV [A]
    // [2] potenza FV [W]
    // [3] energia giornaliera / contatore energia
    // [4] energia totale / contatore energia
    // [5] stato (non utilizzato)
    // [6] valore ausiliario
    // [7] corrente FV massima
    // [8] potenza FV nominale
    //
    // Aggiorna:
    //
    // snapshot.pv.electrical
    // snapshot.pv.energy
    // snapshot.pv.limits
    //
    // Inoltre sincronizza:
    //
    // snapshot.settings.rating.pvPower
    //==================================================================

    bool parseHPV(
        const String& response,
        uint32_t now)
    {
        const auto fields =
            tokenize(response);

        if (fields.size() < 3)
            return false;

        Snapshot::PV pv =
            _snapshot.pv;

        Snapshot::Settings settings =
            _snapshot.settings;

        if (!parseFloatToken(
                fields,
                0,
                pv.electrical.voltage))
        {
            return false;
        }

        if (!parseFloatToken(
                fields,
                1,
                pv.electrical.current))
        {
            return false;
        }

        if (!parseFloatToken(
                fields,
                2,
                pv.electrical.power))
        {
            return false;
        }

        /*
        * I campi successivi non fanno parte del layout minimo
        * documentato dal parser QA6248 di riferimento, ma sono
        * presenti sul firmware attualmente testato.
        */
        if (fields.size() > 3)
        {
            parseFloatToken(
                fields,
                3,
                pv.energy.daily
            );
        }

        if (fields.size() > 4)
        {
            parseFloatToken(
                fields,
                4,
                pv.energy.total
            );
        }

        if (fields.size() > 6)
        {
            parseFloatToken(
                fields,
                6,
                pv.limits.auxiliaryValue
            );
        }

        if (fields.size() > 7)
        {
            parseFloatToken(
                fields,
                7,
                pv.limits.maxCurrent
            );
        }

        if (fields.size() > 8)
        {
            uint32_t ratedPower = 0;

            if (parseUIntToken(
                    fields,
                    8,
                    ratedPower))
            {
                pv.limits.ratedPower =
                    ratedPower;

                settings.rating.pvPower =
                    ratedPower;
            }
        }

        pv.valid =
            true;

        pv.updatedAt =
            now;

        _snapshot.pv =
            pv;

        _snapshot.settings =
            settings;

        markSnapshotUpdated(now);

        return true;
    }

    //==================================================================
    // HBAT
    //------------------------------------------------------------------
    // Snapshot batteria.
    //
    // Frame reale:
    //
    // (04 001.3 000 000 00000 380 000002210200 00000000
    //
    // Mapping verificato:
    //
    // [0] status word
    // [1] tensione batteria
    // [6] warning word
    // [7] fault word
    //
    // Manteniamo le bitword native del protocollo.
    // Nessuna espansione in flag booleani.
    //
    // Aggiorna:
    //
    // snapshot.battery.electrical
    // snapshot.battery.status
    //==================================================================

    bool parseHBAT(
        const String& response,
        uint32_t now)
    {
        const auto fields =
            tokenize(response);

        /*
        * Layout QA6248:
        *
        *   [0] numero elementi/celle batteria
        *   [1] tensione batteria
        *   [2] capacità/SOC batteria
        *   [3] corrente di carica
        *   [4] corrente di scarica
        *   [5] tensione bus DC
        *   [6] sequenza flag batteria
        *   [7] campo proprietario aggiuntivo
        */
        if (fields.size() < 7)
            return false;

        Snapshot::Battery battery =
            _snapshot.battery;

        Snapshot::Internal internal =
            _snapshot.internal;

        uint32_t unsignedValue = 0;

        if (!parseUIntToken(
                fields,
                0,
                unsignedValue))
        {
            return false;
        }

        battery.metadata.cells =
            static_cast<uint16_t>(
                unsignedValue > UINT16_MAX
                    ? UINT16_MAX
                    : unsignedValue
            );

        if (!parseFloatToken(
                fields,
                1,
                battery.electrical.voltage))
        {
            return false;
        }

        if (!parseUIntToken(
                fields,
                2,
                unsignedValue))
        {
            return false;
        }

        battery.capacity.soc =
            static_cast<int16_t>(
                unsignedValue > 100U
                    ? 100U
                    : unsignedValue
            );

        if (!parseFloatToken(
                fields,
                3,
                battery.electrical.chargingCurrent))
        {
            return false;
        }

        if (!parseFloatToken(
                fields,
                4,
                battery.electrical.dischargeCurrent))
        {
            return false;
        }

        if (!parseFloatToken(
                fields,
                5,
                internal.electrical.busVoltage))
        {
            return false;
        }

        const String& rawFlags =
            fields[6];

        if (rawFlags.length() < 7)
            return false;

        auto digitAt =
            [&rawFlags](size_t index, uint8_t& output) -> bool
            {
                if (index >= rawFlags.length())
                    return false;

                const char value =
                    rawFlags[index];

                if (value < '0' ||
                    value > '9')
                {
                    return false;
                }

                output =
                    static_cast<uint8_t>(
                        value - '0'
                    );

                return true;
            };

        uint8_t chargeSwitch = 0;
        uint8_t solarChargeSwitch = 0;
        uint8_t acChargeSwitch = 0;
        uint8_t lithiumActivateSwitch = 0;
        uint8_t lithiumActivateProcess = 0;
        uint8_t reservedFlag = 0;
        uint8_t batteryStatus = 0;

        if (!digitAt(0, chargeSwitch) ||
            !digitAt(1, solarChargeSwitch) ||
            !digitAt(2, acChargeSwitch) ||
            !digitAt(3, lithiumActivateSwitch) ||
            !digitAt(4, lithiumActivateProcess) ||
            !digitAt(5, reservedFlag) ||
            !digitAt(6, batteryStatus))
        {
            return false;
        }

        battery.status.rawFlags =
            rawFlags;

        battery.status.chargeSwitch =
            chargeSwitch;

        battery.status.solarChargeSwitch =
            solarChargeSwitch;

        battery.status.acChargeSwitch =
            acChargeSwitch;

        battery.status.lithiumActivateSwitch =
            lithiumActivateSwitch;

        battery.status.lithiumActivateProcess =
            lithiumActivateProcess;

        battery.status.reservedFlag =
            reservedFlag;

        battery.status.batteryStatus =
            batteryStatus;

        battery.status.sccCharging =
            solarChargeSwitch == 1U;

        battery.status.acCharging =
            acChargeSwitch == 1U;

        battery.status.chargeActive =
            battery.status.sccCharging ||
            battery.status.acCharging;

        battery.electrical.current =
            battery.electrical.chargingCurrent -
            battery.electrical.dischargeCurrent;

        if (std::isfinite(
                battery.electrical.voltage) &&
            std::isfinite(
                battery.electrical.current))
        {
            battery.electrical.power =
                battery.electrical.voltage *
                battery.electrical.current;
        }
        else
        {
            battery.electrical.power =
                NAN;
        }

        /*
        * HSTS è autorevole per stabilire la disconnessione.
        * In sua assenza viene applicata una soglia conservativa.
        */
        if (_snapshot.system.valid &&
            _snapshot.system.status.primary.batteryDisconnected)
        {
            battery.connected =
                false;
        }
        else
        {
            battery.connected =
                std::isfinite(
                    battery.electrical.voltage
                ) &&
                battery.electrical.voltage > 10.0f;
        }

        battery.valid =
            true;

        battery.updatedAt =
            now;

        internal.valid =
            true;

        internal.updatedAt =
            now;

        _snapshot.battery =
            battery;

        _snapshot.internal =
            internal;

        markSnapshotUpdated(now);

        return true;
    }

    //==================================================================
    // HGEN
    //------------------------------------------------------------------
    // Statistiche energetiche.
    //
    // Frame reale:
    //
    // (120910 07:14 00.000 0000.0 0000.0
    //  000000000.0 000000000000
    //
    // Mapping:
    //
    // [0] data
    // [1] ora
    // [2] energia giorno
    // [3] energia mese
    // [4] energia anno
    // [5] energia totale
    // [6] contatore secondario
    //
    // Aggiorna:
    //
    // snapshot.energy.timestamp
    // snapshot.energy.production
    //==================================================================

    bool parseHGEN(
        const String& response, uint32_t now)
    {
        const auto fields =
            tokenize(response);

        if (fields.size() < 7)
            return false;

        Snapshot::Energy parsed =
            _snapshot.energy;

        parsed.timestamp.date =
            fields[0];

        parsed.timestamp.time =
            fields[1];

        parseFloatToken(
            fields,
            2,
            parsed.production.today);

        parseFloatToken(
            fields,
            3,
            parsed.production.month);

        parseFloatToken(
            fields,
            4,
            parsed.production.year);

        parseFloatToken(
            fields,
            5,
            parsed.production.total);

        parseFloatToken(
            fields,
            6,
            parsed.production.secondaryTotal);

        parsed.valid = true;
        parsed.updatedAt = now;

        _snapshot.energy = parsed;

        markSnapshotUpdated(now);

        return true;
    }

    //==================================================================
    // HEEP1
    //------------------------------------------------------------------
    // Parametri batteria.
    //
    // Frame reale:
    //
    // (0 060 030 04000100230 011 1 1 0 0 0 010 020
    //  095 050 056.4 054.0 042.0 010 000
    //
    // Mapping confermato:
    //
    // [14] bulk voltage
    // [15] float voltage
    // [16] cutoff voltage
    //
    // [1] max utility charge current
    //
    // Aggiorna:
    //
    // snapshot.settings.voltage
    // snapshot.settings.current
    //==================================================================

    bool parseHEEP1(
        const String& response, uint32_t now)
    {
        const auto fields =
            tokenize(response);

        if (fields.size() < 17)
            return false;

        Snapshot::Settings parsed =
            _snapshot.settings;

        if (!parseFloatToken(
                fields,
                14,
                parsed.voltage.bulk))
        {
            return false;
        }

        if (!parseFloatToken(
                fields,
                15,
                parsed.voltage.floating))
        {
            return false;
        }

        if (!parseFloatToken(
                fields,
                16,
                parsed.voltage.cutoff))
        {
            return false;
        }

        uint32_t current = 0;

        if (parseUIntToken(
                fields,
                1,
                current))
        {
            parsed.current.maxUtilityCharge =
                static_cast<uint16_t>(current);
        }

        parsed.valid = true;
        parsed.updatedAt = now;

        _snapshot.settings = parsed;

        markSnapshotUpdated(now);

        return true;
    }

    //==================================================================
    // HEEP2
    //------------------------------------------------------------------
    // Parametri di protezione e correnti.
    //
    // Frame reale:
    //
    // (1 044.0 020 044.0 046.0 054.0 0 058.4 060
    //  120 030 0000 0000 05 0000 52.0 95000
    //
    // Mapping:
    //
    // [1]  low voltage
    // [3]  warning voltage
    // [4]  restart voltage
    // [5]  recharge voltage
    // [7]  over voltage
    // [15] redischarge voltage
    //
    // [8]  max PV current
    // [9]  max charge current
    // [10] max utility charge current
    //
    // Aggiorna:
    //
    // snapshot.settings.voltage
    // snapshot.settings.current
    //==================================================================

    bool parseHEEP2(
        const String& response, uint32_t now)
    {
        const auto fields =
            tokenize(response);

        if (fields.size() < 17)
            return false;

        Snapshot::Settings parsed =
            _snapshot.settings;

        parseFloatToken(
            fields,
            1,
            parsed.voltage.low);

        parseFloatToken(
            fields,
            3,
            parsed.voltage.warning);

        parseFloatToken(
            fields,
            4,
            parsed.voltage.restart);

        parseFloatToken(
            fields,
            5,
            parsed.voltage.recharge);

        parseFloatToken(
            fields,
            7,
            parsed.voltage.over);

        parseFloatToken(
            fields,
            15,
            parsed.voltage.redischarge);

        uint32_t numericValue = 0;

        if (parseUIntToken(
                fields,
                8,
                numericValue))
        {
            parsed.current.maxPV =
                static_cast<uint16_t>(numericValue);
        }

        if (parseUIntToken(
                fields,
                9,
                numericValue))
        {
            parsed.current.maxCharge =
                static_cast<uint16_t>(numericValue);
        }

        if (parseUIntToken(
                fields,
                10,
                numericValue))
        {
            parsed.current.maxUtilityCharge =
                static_cast<uint16_t>(numericValue);
        }

        parsed.valid = true;
        parsed.updatedAt =now;

        _snapshot.settings = parsed;

        markSnapshotUpdated(now);

        return true;
    }

    //==================================================================
    // HEEP3
    //------------------------------------------------------------------
    // Parametri nominali inverter.
    //
    // Frame reale:
    //
    // (2048 2048 2048 0150 053.5 03000 200 1023 001
    //  04750 2043 0000 000 06200 01 0 00
    //
    // Mapping confermato:
    //
    // [13] inverter rated power [W]
    //
    // Aggiorna:
    //
    // snapshot.settings.rating
    //==================================================================

    bool parseHEEP3(
        const String& response, uint32_t now)
    {
        const auto fields =
            tokenize(response);

        if (fields.size() < 14)
            return false;

        Snapshot::Settings parsed =
            _snapshot.settings;

        uint32_t ratedPower = 0;

        if (!parseUIntToken(
                fields,
                13,
                ratedPower))
        {
            return false;
        }

        parsed.rating.inverterPower =
            ratedPower;

        parsed.valid = true;
        parsed.updatedAt = now;

        _snapshot.settings = parsed;

        markSnapshotUpdated(now);

        return true;
    }

    bool parseHSTS(
        const String& response,
        uint32_t now)
    {
        const auto fields =
            tokenize(response);

        /*
        * Formato minimo:
        *
        *   [0] error code
        *   [1] mode code + flag iniziali
        *
        * Un eventuale terzo token contiene la prosecuzione
        * della sequenza di flag.
        */
        if (fields.size() < 2)
            return false;

        const String& errorCode =
            fields[0];

        const String& modeAndFlags =
            fields[1];

        if (modeAndFlags.length() == 0)
            return false;

        String allFlags;

        if (modeAndFlags.length() > 1)
        {
            allFlags =
                modeAndFlags.substring(1);
        }

        if (fields.size() > 2)
        {
            allFlags +=
                fields[2];
        }

        Snapshot::System system =
            _snapshot.system;

        system.operating.errorCode =
            errorCode;

        system.operating.modeCode =
            modeAndFlags[0];

        system.status.flagsRaw =
            allFlags;

        auto flagAt =
            [&allFlags](size_t index) -> bool
            {
                return
                    index < allFlags.length() &&
                    allFlags[index] == '1';
            };

        /*
        * Flag primari HSTS:
        *
        *   0  A
        *   1  B output active
        *   2  C battery low alarm
        *   3  D battery disconnected
        *   4  E overload
        *   5  F overtemperature
        *   6  G configuration changed
        *   7  H inverter on
        *   8  I PV power low
        *   9  J input voltage high
        *   10 K battery voltage high
        *   11 L fan fault
        */
        system.status.primary.a =
            flagAt(0);

        system.status.primary.outputActive =
            flagAt(1);

        system.status.primary.batteryLowAlarm =
            flagAt(2);

        system.status.primary.batteryDisconnected =
            flagAt(3);

        system.status.primary.overload =
            flagAt(4);

        system.status.primary.overTemperature =
            flagAt(5);

        system.status.primary.configurationChanged =
            flagAt(6);

        system.status.primary.inverterOn =
            flagAt(7);

        system.status.primary.pvPowerLow =
            flagAt(8);

        system.status.primary.inputVoltageHigh =
            flagAt(9);

        system.status.primary.batteryVoltageHigh =
            flagAt(10);

        system.status.primary.fanFault =
            flagAt(11);

        /*
        * Flag estesi HSTS:
        *
        *   12 M
        *   13 P
        *   14 Q
        *   15 R dustproof installed
        *   16 S
        *   17 T switch on
        *   18 U
        *   19 V
        *   20 W
        *   21 X
        *   22 Y
        */
        system.status.extended.m =
            flagAt(12);

        system.status.extended.p =
            flagAt(13);

        system.status.extended.q =
            flagAt(14);

        system.status.extended.dustproofInstalled =
            flagAt(15);

        system.status.extended.s =
            flagAt(16);

        system.status.extended.switchOn =
            flagAt(17);

        system.status.extended.u =
            flagAt(18);

        system.status.extended.v =
            flagAt(19);

        system.status.extended.w =
            flagAt(20);

        system.status.extended.x =
            flagAt(21);

        system.status.extended.y =
            flagAt(22);

        system.valid =
            true;

        _snapshot.system =
            system;

        /*
        * Allinea lo stato batteria con il flag operativo HSTS.
        *
        * La misura HBAT può riportare una tensione residua anche
        * quando la batteria è fisicamente scollegata.
        */
        if (system.status.primary.batteryDisconnected)
        {
            _snapshot.battery.connected =
                false;
        }

        markSnapshotUpdated(now);

        return true;
    }

    bool parseHOP(
        const String& response,
        uint32_t now)
    {
        const auto fields =
            tokenize(response);

        /*
        * Layout QA6248 HOP:
        *
        *   [0] tensione uscita
        *   [1] frequenza uscita
        *   [2] potenza apparente
        *   [3] potenza attiva
        *   [4] carico percentuale
        *   [5] componente DC
        *   [6] campo interno proprietario
        *   [7] corrente induttore
        */
        if (fields.size() < 8)
            return false;

        Snapshot::Output output =
            _snapshot.output;

        Snapshot::Internal internal =
            _snapshot.internal;

        uint32_t unsignedValue = 0;
        int32_t signedValue = 0;

        if (!parseFloatToken(
                fields,
                0,
                output.electrical.voltage))
        {
            return false;
        }

        if (!parseFloatToken(
                fields,
                1,
                output.electrical.frequency))
        {
            return false;
        }

        if (!parseUIntToken(
                fields,
                2,
                unsignedValue))
        {
            return false;
        }

        output.electrical.apparentPower =
            static_cast<float>(
                unsignedValue
            );

        if (!parseUIntToken(
                fields,
                3,
                unsignedValue))
        {
            return false;
        }

        output.electrical.activePower =
            static_cast<float>(
                unsignedValue
            );

        if (!parseUIntToken(
                fields,
                4,
                unsignedValue))
        {
            return false;
        }

        output.load.percent =
            static_cast<uint8_t>(
                unsignedValue > 100U
                    ? 100U
                    : unsignedValue
            );

        if (!parseIntToken(
                fields,
                5,
                signedValue))
        {
            return false;
        }

        internal.electrical.outputDcComponent =
            signedValue;

        internal.diagnostic.hopInternalData =
            fields[6];

        if (!parseFloatToken(
                fields,
                7,
                internal.electrical.inductorCurrent))
        {
            return false;
        }

        if (std::isfinite(
                output.electrical.voltage) &&
            output.electrical.voltage > 1.0f &&
            std::isfinite(
                output.electrical.apparentPower))
        {
            output.electrical.current =
                output.electrical.apparentPower /
                output.electrical.voltage;
        }
        else
        {
            output.electrical.current =
                NAN;
        }

        output.valid =
            true;

        output.updatedAt =
            now;

        internal.valid =
            true;

        internal.updatedAt =
            now;

        _snapshot.output =
            output;

        _snapshot.internal =
            internal;

        markSnapshotUpdated(now);

        return true;
    }

    bool parseHGRID(
        const String& response,
        uint32_t now)
    {
        const auto fields =
            tokenize(response);

        if (fields.size() < 2)
            return false;

        Snapshot::Grid grid =
            _snapshot.grid;

        if (!parseFloatToken(
                fields,
                0,
                grid.electrical.voltage))
        {
            return false;
        }

        if (!parseFloatToken(
                fields,
                1,
                grid.electrical.frequency))
        {
            return false;
        }

        grid.available =
            std::isfinite(
                grid.electrical.voltage
            ) &&
            grid.electrical.voltage > 20.0f;

        grid.valid =
            true;

        grid.updatedAt =
            now;

        _snapshot.grid =
            grid;

        markSnapshotUpdated(now);

        return true;
    }

    bool parseHPVB(
        const String& response,
        uint32_t now)
    {
        const auto fields =
            tokenize(response);

        if (fields.empty())
            return false;

        Snapshot::PV pv =
            _snapshot.pv;

        if (!parseFloatToken(
                fields,
                0,
                pv.electrical.sccBatteryVoltage))
        {
            return false;
        }

        pv.updatedAt =
            now;

        /*
        * HPVB da solo non rende valido l'intero gruppo PV.
        * La validità del gruppo rimane legata almeno alla
        * telemetria HPV.
        */
        _snapshot.pv =
            pv;

        markSnapshotUpdated(now);

        return true;
    }

    bool parseHTEMP(
        const String& response,
        uint32_t now)
    {
        const auto fields =
            tokenize(response);

        /*
        * Layout minimo:
        *
        *   [0] temperatura PV
        *   [1] temperatura inverter/dissipatore
        *   [2] temperatura boost
        *   [3] campo interno
        *   [4] temperatura massima
        *   [5] velocità ventola 1
        *   [6] velocità ventola 2
        *
        * Opzionali:
        *
        *   [7] stato ventola 1
        *   [8] stato ventola 2
        */
        if (fields.size() < 7)
            return false;

        Snapshot::PV pv =
            _snapshot.pv;

        Snapshot::Internal internal =
            _snapshot.internal;

        uint32_t unsignedValue = 0;
        int32_t signedValue = 0;

        if (!parseFloatToken(
                fields,
                0,
                pv.thermal.temperature))
        {
            return false;
        }

        internal.thermal.pvTemperature =
            pv.thermal.temperature;

        if (!parseFloatToken(
                fields,
                1,
                internal.thermal.heatsinkTemperature))
        {
            return false;
        }

        if (!parseFloatToken(
                fields,
                2,
                internal.thermal.boostTemperature))
        {
            return false;
        }

        if (!parseIntToken(
                fields,
                3,
                signedValue))
        {
            return false;
        }

        internal.diagnostic.temperatureInternalData =
            signedValue;

        if (!parseFloatToken(
                fields,
                4,
                internal.thermal.maximumTemperature))
        {
            return false;
        }

        if (!parseUIntToken(
                fields,
                5,
                unsignedValue))
        {
            return false;
        }

        internal.fan.speed1 =
            unsignedValue;

        if (!parseUIntToken(
                fields,
                6,
                unsignedValue))
        {
            return false;
        }

        internal.fan.speed2 =
            unsignedValue;

        if (fields.size() > 7 &&
            parseUIntToken(
                fields,
                7,
                unsignedValue))
        {
            internal.fan.status1 =
                static_cast<uint8_t>(
                    unsignedValue > UINT8_MAX
                        ? UINT8_MAX
                        : unsignedValue
                );
        }

        if (fields.size() > 8 &&
            parseUIntToken(
                fields,
                8,
                unsignedValue))
        {
            internal.fan.status2 =
                static_cast<uint8_t>(
                    unsignedValue > UINT8_MAX
                        ? UINT8_MAX
                        : unsignedValue
                );
        }

        pv.updatedAt =
            now;

        internal.valid =
            true;

        internal.updatedAt =
            now;

        _snapshot.pv =
            pv;

        _snapshot.internal =
            internal;

        markSnapshotUpdated(now);

        return true;
    }
};

class SerialSolarTransport :
    public ISolarTransport
{
private:

    HardwareSerial&
        _serial;

    uint32_t
        _baudrate;

    // ========================================================
    // NON BLOCKING RX STATE
    // ========================================================

    bool
        _readActive = false;

    bool
        _previousWasCR = false;

    uint32_t
        _readStartedAt = 0;

public:

    SerialSolarTransport(
        HardwareSerial& serial,
        uint32_t baudrate)
        :
        _serial(serial),
        _baudrate(baudrate)
    {
    }

    // ========================================================
    // BEGIN
    // ========================================================

    bool begin() override
    {
        resetFrameReader();

        _serial.begin(
            _baudrate
        );

        return true;
    }

    // ========================================================
    // END
    // ========================================================

    void end() override
    {
        resetFrameReader();

        _serial.flush();
    }

    // ========================================================
    // CONNECTION STATUS
    // ========================================================

    bool connected() const override
    {
        return true;
    }

    // ========================================================
    // WRITE
    // ========================================================

    bool write(
        const uint8_t* data,
        size_t length) override
    {
        if (!data ||
            length == 0)
        {
            return false;
        }

        resetFrameReader();

        return
            _serial.write(
                data,
                length
            ) == length;
    }

    // ========================================================
    // RESET FRAME READER
    // ========================================================

    void resetFrameReader() override
    {
        _readActive =
            false;

        _previousWasCR =
            false;

        _readStartedAt =
            0;
    }

    // ========================================================
    // NON BLOCKING READ FRAME
    // ========================================================

    SolarFrameReadResult readFrame(
        std::vector<uint8_t>& frame,
        uint32_t timeoutMs) override
    {
        const uint32_t now =
            millis();

        if (!_readActive)
        {
            _readActive =
                true;

            _previousWasCR =
                false;

            _readStartedAt =
                now;

            frame.clear();
        }

        // ----------------------------------------------------
        // CONSUMA SOLO I BYTE DISPONIBILI
        // ----------------------------------------------------

        while (_serial.available() > 0)
        {
            const int raw =
                _serial.read();

            if (raw < 0)
                break;

            const uint8_t value =
                static_cast<uint8_t>(
                    raw
                );

            // --------------------------------------------
            // CR
            // --------------------------------------------

            if (value == 0x0D)
            {
                if (!frame.empty())
                {
                    resetFrameReader();

                    return
                        SolarFrameReadResult::Complete;
                }

                _previousWasCR =
                    true;

                continue;
            }

            // --------------------------------------------
            // LF
            // --------------------------------------------

            if (value == 0x0A)
            {
                if (_previousWasCR)
                {
                    _previousWasCR =
                        false;

                    continue;
                }

                if (!frame.empty())
                {
                    resetFrameReader();

                    return
                        SolarFrameReadResult::Complete;
                }

                continue;
            }

            _previousWasCR =
                false;

            frame.push_back(
                value
            );
        }

        // ----------------------------------------------------
        // TIMEOUT
        // ----------------------------------------------------

        if (
            static_cast<uint32_t>(
                now - _readStartedAt
            ) >= timeoutMs
        )
        {
            resetFrameReader();

            return
                SolarFrameReadResult::Timeout;
        }

        // ----------------------------------------------------
        // FRAME NON ANCORA COMPLETO
        // ----------------------------------------------------

        return
            SolarFrameReadResult::Pending;
    }

    // ========================================================
    // FLUSH INPUT
    // ========================================================

    void flushInput() override
    {
        resetFrameReader();

        while (_serial.available() > 0)
        {
            _serial.read();
        }
    }
};

class TcpSolarTransport :
    public ISolarTransport
{
private:

    DMPlatform::Network::TCPClient&
        _client;

    IPAddress
        _host;

    uint16_t
        _port;

    uint32_t
        _timeoutMs;

    // ========================================================
    // NON-BLOCKING FRAME READER STATE
    // ========================================================

    bool
        _readActive = false;

    bool
        _previousWasCR = false;

    uint32_t
        _readStartedAt = 0;

public:

    TcpSolarTransport(
        DMPlatform::Network::TCPClient& client,
        const IPAddress& host,
        uint16_t port,
        uint32_t timeoutMs)
        :
        _client(client),
        _host(host),
        _port(port),
        _timeoutMs(timeoutMs)
    {
    }

    // ========================================================
    // BEGIN
    // ========================================================

    bool begin() override
    {
        resetFrameReader();

        if (_client.connected())
            return true;

        return _client.connect(
            _host,
            _port
        );
    }

    // ========================================================
    // END
    // ========================================================

    void end() override
    {
        resetFrameReader();

        _client.stop();
    }

    // ========================================================
    // CONNECTION STATUS
    // ========================================================

    bool connected() const override
    {
        return
            _client.connected();
    }

    // ========================================================
    // WRITE
    // ========================================================

    bool write(
        const uint8_t* data,
        size_t length) override
    {
        if (!data ||
            length == 0)
        {
            return false;
        }

        if (!_client.connected())
            return false;

        /*
         * Ogni nuova trasmissione apre una nuova sessione
         * di ricezione.
         */
        resetFrameReader();

        return
            _client.write(
                data,
                length
            ) == length;
    }

    // ========================================================
    // RESET NON-BLOCKING FRAME READER
    // ========================================================

    void resetFrameReader() override
    {
        _readActive =
            false;

        _previousWasCR =
            false;

        _readStartedAt =
            0;
    }

    // ========================================================
    // NON-BLOCKING READ FRAME
    //
    // La funzione:
    //
    //   - non contiene while temporali;
    //   - non contiene delay();
    //   - consuma esclusivamente i byte già disponibili;
    //   - ritorna immediatamente se la risposta non è completa;
    //   - conserva internamente timestamp e stato CR/LF.
    //
    // Il vettore frame deve essere mantenuto dal chiamante tra
    // chiamate consecutive fino a Complete, Timeout o errore.
    // ========================================================

    SolarFrameReadResult readFrame(
        std::vector<uint8_t>& frame,
        uint32_t timeoutMs) override
    {
        const uint32_t now =
            millis();

        const uint32_t effectiveTimeout =
            timeoutMs > 0
                ? timeoutMs
                : _timeoutMs;

        // ----------------------------------------------------
        // START NEW READ SESSION
        // ----------------------------------------------------

        if (!_readActive)
        {
            _readActive =
                true;

            _previousWasCR =
                false;

            _readStartedAt =
                now;

            frame.clear();
        }

        // ----------------------------------------------------
        // CONNECTION LOST
        //
        // Vengono prima consumati eventuali byte residui già
        // presenti nel buffer TCP.
        // ----------------------------------------------------

        if (!_client.connected() &&
            _client.available() == 0)
        {
            resetFrameReader();

            return
                SolarFrameReadResult::Disconnected;
        }

        // ----------------------------------------------------
        // CONSUME ONLY CURRENTLY AVAILABLE BYTES
        //
        // Questo ciclo non attende nuovi byte. Consuma solo
        // quelli già presenti nel buffer del client.
        // ----------------------------------------------------

        while (_client.available() > 0)
        {
            const int raw =
                _client.read();

            if (raw < 0)
                break;

            const uint8_t value =
                static_cast<uint8_t>(
                    raw
                );

            // ------------------------------------------------
            // CARRIAGE RETURN
            // ------------------------------------------------

            if (value == 0x0DU)
            {
                if (!frame.empty())
                {
                    resetFrameReader();

                    return
                        SolarFrameReadResult::Complete;
                }

                _previousWasCR =
                    true;

                continue;
            }

            // ------------------------------------------------
            // LINE FEED
            // ------------------------------------------------

            if (value == 0x0AU)
            {
                /*
                 * LF successivo a CR già consumato.
                 */
                if (_previousWasCR)
                {
                    _previousWasCR =
                        false;

                    continue;
                }

                if (!frame.empty())
                {
                    resetFrameReader();

                    return
                        SolarFrameReadResult::Complete;
                }

                continue;
            }

            _previousWasCR =
                false;

            frame.push_back(
                value
            );
        }

        // ----------------------------------------------------
        // TIMEOUT
        // ----------------------------------------------------

        if (
            static_cast<uint32_t>(
                now - _readStartedAt
            ) >= effectiveTimeout
        )
        {
            resetFrameReader();

            return
                SolarFrameReadResult::Timeout;
        }

        // ----------------------------------------------------
        // NOT COMPLETED YET
        // ----------------------------------------------------

        return
            SolarFrameReadResult::Pending;
    }

    // ========================================================
    // FLUSH INPUT
    // ========================================================

    void flushInput() override
    {
        resetFrameReader();

        while (_client.available() > 0)
        {
            _client.read();
        }
    }
};