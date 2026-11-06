#ifndef DMAccessControl_HPP
#define DMAccessControl_HPP

#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <functional>

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

struct AccessReader;


// ============================================================================
// ACCESS READER DEFINITION
//
// Definizione logica e immutabile del protocollo lettore.
//
// Piu lettori fisici possono condividere la stessa definizione.
//
// Esempi:
//
//   FanvilWebAPI::readerDefinition()
//   X3WebAPI::readerDefinition()
//
// Non contiene:
//
//   - indirizzo IP
//   - codice tessera
//   - area DomoManager
//   - stato runtime
// ============================================================================

struct AccessReaderDef
{
    // ========================================================================
    // CALLBACK TYPES
    // ========================================================================

    using ConfigValidator =
        bool (*)(
            const AccessReader& reader,
            String& error
        );

    using DiagnosticCallback =
        void (*)(
            const AccessReader& reader
        );


    // ========================================================================
    // IDENTITY
    // ========================================================================

    const char* id =
        nullptr;

    const char* name =
        nullptr;


    // ========================================================================
    // CAPABILITIES
    // ========================================================================

    bool supportsServerPush =
        false;

    bool supportsOnlineValidation =
        false;

    bool supportsPolling =
        false;

    bool supportsRemoteRelay =
        false;


    // ========================================================================
    // NETWORK DEFAULTS
    // ========================================================================

    uint16_t defaultPort =
        80;


    // ========================================================================
    // CONFIGURATION VALIDATION
    // ========================================================================

    ConfigValidator validateConfig =
        nullptr;


    // ========================================================================
    // DIAGNOSTICS
    // ========================================================================

    DiagnosticCallback diagnostic =
        nullptr;
};


// ============================================================================
// ACCESS READER
//
// Configurazione di un singolo lettore fisico.
//
// validCardCode:
//   codice valido solamente per questo lettore.
//
// Il codice comune a tutti i lettori viene configurato in:
//
//   AccessControlService::Config::commonCardCode
// ============================================================================

struct AccessReader
{
    // ========================================================================
    // IDENTITY
    // ========================================================================

    const char* name =
        nullptr;


    // ========================================================================
    // PROTOCOL DEFINITION
    // ========================================================================

    const AccessReaderDef* def =
        nullptr;


    // ========================================================================
    // NETWORK ADDRESS
    //
    // Può contenere:
    //
    //   - MAC address;
    //   - indirizzo IP;
    //   - identificativo di rete specifico del protocollo.
    //
    // nullptr:
    //   nessun indirizzo richiesto per la risoluzione.
    // ========================================================================

    const char* readerAddress =
        nullptr;


    // ========================================================================
    // CARD CODE READER
    //
    // Restituisce il codice autorizzato corrente.
    //
    // Può:
    //
    //   - restituire un valore fisso;
    //   - leggere il buffer DomoManager;
    //   - interrogare qualsiasi altra sorgente.
    //
    // nullptr:
    //   nessun codice specifico associato al lettore.
    // ========================================================================

    using CardCodeReader =
        String (*)();

    CardCodeReader readCardCode =
        nullptr;


    // ========================================================================
    // RUNTIME CONFIGURATION
    // ========================================================================

    bool enabled =
        true;


    /*
     * Indice logico del varco o della porta.
     */
    int doorIndex =
        -1;


    /*
     * Area DomoManager associata all'accesso.
     *
     * Se valorizzata, la callback frontend può attivare
     * un impulso locale tramite PulseManager.
     *
     * -1:
     *   nessuna area associata.
     */
    int area =
        -1;


    // ========================================================================
    // AUTHORIZED OUTBOUND MESSAGE
    //
    // Chiave del messaggio WebAPI da inviare dopo
    // un accesso autorizzato.
    //
    // Il valore deve corrispondere a una chiave presente nel
    // DeviceMessageProfile associato al WebApiDevice che possiede
    // lo stesso nome del reader.
    //
    // Esempi:
    //
    //   "RELAY_OPEN"
    //   "DOOR_OPEN"
    //   "UNLOCK"
    //
    // nullptr oppure stringa vuota:
    //   nessun comando WebAPI in uscita.
    //
    // La presenza di questo campo richiede che il client WebAPI
    // sia abilitato e correttamente inizializzato.
    // ========================================================================

    const char* authorizedMessageKey =
        nullptr;
};


// ============================================================================
// ACCESS CONTROL SERVICE
//
// Servizio istanziabile per la gestione centralizzata degli accessi.
//
// Responsabilita:
//
//   - validazione configurazione allo startup
//   - identificazione del lettore
//   - codice comune a tutti i lettori
//   - codice specifico del singolo lettore
//   - validatore applicativo aggiuntivo
//   - callback per ogni evento
//   - callback per gli accessi autorizzati
//
// Non contiene:
//
//   - parsing HTTP Fanvil
//   - parsing HTTP X2/X3
//   - trasporto HTTP
//   - gestione diretta dei rele
//
// Le classi specifiche di protocollo convertono il proprio messaggio in:
//
//   AccessControlService::Request
//
// Nessun membro runtime e statico.
// L'istanza viene posseduta dal frontend.
// ============================================================================

class AccessControlService
{
public:

    // ========================================================================
    // RESULT
    // ========================================================================

    enum class Result : uint8_t
    {
        INVALID_REQUEST = 0,

        SERVICE_NOT_INITIALIZED,

        READER_NOT_FOUND,

        READER_DISABLED,

        READER_DEFINITION_MISMATCH,

        CARD_EMPTY,

        CARD_REJECTED,

        CARD_ACCEPTED_COMMON,

        CARD_ACCEPTED_READER,

        CARD_ACCEPTED_EXTERNAL
    };


    // ========================================================================
    // NORMALIZED ACCESS REQUEST
    //
    // Formato comune prodotto dai diversi protocolli.
    // ========================================================================

    struct Request
    {
        // --------------------------------------------------------------------
        // READER
        // --------------------------------------------------------------------

        /*
         * Puntatore diretto opzionale.
         *
         * Se valorizzato, evita la ricerca per IP, nome o definizione.
         */
        const AccessReader* reader =
            nullptr;

        /*
         * Definizione attesa dal protocollo chiamante.
         */
        const AccessReaderDef* readerDef =
            nullptr;

        /*
         * Nome del lettore o identificativo del terminale.
         */
        String readerName;

        /*
         * Indirizzo IP remoto in formato testuale.
         */
        String readerAddress;


        // --------------------------------------------------------------------
        // CARD
        // --------------------------------------------------------------------

        String cardCode;

        String personName;


        // --------------------------------------------------------------------
        // TRANSACTION
        // --------------------------------------------------------------------

        String direction;

        String door;

        String timestamp;

        String rawPayload;
    };


    // ========================================================================
    // ACCESS DECISION
    // ========================================================================

    struct Decision
    {
        Result result =
            Result::INVALID_REQUEST;

        bool authorized =
            false;

        /*
         * true quando e stato utilizzato il codice comune.
         */
        bool commonCode =
            false;

        const AccessReader* reader =
            nullptr;

        const AccessReaderDef* readerDef =
            nullptr;

        String cardCode;

        String message;

        /*
         * Informazioni che il protocollo specifico puo usare
         * per costruire la propria risposta.
         */
        int relayIndex =
            0;

        int relayTime =
            0;
    };


    // ========================================================================
    // CALLBACKS
    // ========================================================================

    /*
     * Validatore applicativo aggiuntivo.
     *
     * Viene eseguito solamente se la carta non coincide con:
     *
     *   - codice comune
     *   - codice specifico del lettore
     */
    using ExternalValidator =
        std::function<bool(
            const Request& request,
            const AccessReader& reader
        )>;

    /*
     * Notifica ogni decisione.
     */
    using EventCallback =
        std::function<void(
            const Request& request,
            const Decision& decision
        )>;

    /*
     * Notifica solamente gli accessi autorizzati.
     */
    using AuthorizedCallback =
        std::function<void(
            const Request& request,
            const Decision& decision
        )>;


    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    struct Config
    {
        // --------------------------------------------------------------------
        // READERS
        // --------------------------------------------------------------------

        const AccessReader* readers;

        size_t readerCount;


        // --------------------------------------------------------------------
        // COMMON CARD
        // --------------------------------------------------------------------

        const char* commonCardCode;


        // --------------------------------------------------------------------
        // DEFAULT OUTPUT
        // --------------------------------------------------------------------

        int defaultRelayIndex;

        int defaultRelayTime;


        // --------------------------------------------------------------------
        // CALLBACKS
        // --------------------------------------------------------------------

        ExternalValidator validator;

        EventCallback onEvent;

        AuthorizedCallback onAuthorized;


        // --------------------------------------------------------------------
        // CONSTRUCTOR
        //
        // Costruttore esplicito per evitare problemi del toolchain ESP32
        // con inizializzatori dei membri di strutture annidate.
        // --------------------------------------------------------------------

        Config()
            : readers(nullptr),
              readerCount(0),
              commonCardCode(nullptr),
              defaultRelayIndex(1),
              defaultRelayTime(10),
              validator(nullptr),
              onEvent(nullptr),
              onAuthorized(nullptr)
        {
        }
    };

    bool ResolveIncomingReader(
        const AccessReaderDef* readerDef,
        const String& readerName,
        const String& readerAddress,
        const AccessReader*& resolvedReader) const
    {
        resolvedReader =
            nullptr;

        if (
            !readerDef ||
            !_configuration.readers
        )
        {
            LOG_WF(
                "ACCESS",
                "Incoming request without reader definition"
            );

            return false;
        }

        // ====================================================
        // ADDRESS VALIDATION STATUS
        //
        // Il controllo indirizzo viene abilitato quando
        // almeno un lettore della stessa definizione ha
        // readerAddress configurato.
        // ====================================================

        bool addressValidationEnabled =
            false;

        for (
            size_t readerIndex = 0;
            readerIndex < _configuration.readerCount;
            ++readerIndex
        )
        {
            const AccessReader& reader =
                _configuration.readers[
                    readerIndex
                ];

            if (
                reader.enabled &&
                reader.def == readerDef &&
                hasText(
                    reader.readerAddress
                )
            )
            {
                addressValidationEnabled =
                    true;

                break;
            }
        }

        // ====================================================
        // LEGACY RESOLUTION
        //
        // Nessun indirizzo configurato per questa definizione:
        // prova nome e successivamente definizione univoca.
        // ====================================================

        if (!addressValidationEnabled)
        {
            if (readerName.length() > 0)
            {
                resolvedReader =
                    findReaderByName(
                        readerName.c_str()
                    );

                /*
                * Il metodo pubblico findReaderByName() non
                * filtra per definizione, quindi verifichiamo
                * esplicitamente il risultato.
                */
                if (
                    resolvedReader &&
                    resolvedReader->def != readerDef
                )
                {
                    resolvedReader =
                        nullptr;
                }
            }

            if (!resolvedReader)
            {
                resolvedReader =
                    findUniqueReaderByDefinition(
                        readerDef
                    );
            }

            return
                resolvedReader != nullptr;
        }

        // ====================================================
        // ADDRESS REQUIRED
        // ====================================================

        if (
            normalizeReaderAddress(
                readerAddress
            ).length() == 0
        )
        {
            LOG_WF(
                "ACCESS",
                "Missing reader address: "
                "protocol=%s reader=%s",
                readerDef->id
                    ? readerDef->id
                    : "",
                readerName.c_str()
            );

            return false;
        }

        // ====================================================
        // ADDRESS LOOKUP
        // ====================================================

        resolvedReader =
            findReaderByAddress(
                readerDef,
                readerAddress
            );

        if (!resolvedReader)
        {
            LOG_WF(
                "ACCESS",
                "Unknown reader address: "
                "protocol=%s reader=%s address=%s",
                readerDef->id
                    ? readerDef->id
                    : "",
                readerName.c_str(),
                readerAddress.c_str()
            );

            return false;
        }

        return true;
    }

private:
    static String normalizeCardCodeForComparison(
        const String& input)
    {
        String result =
            normalizeCode(
                input
            );

        if (result.length() == 0)
        {
            return "";
        }

        size_t firstSignificantIndex =
            0;

        /*
        * Mantiene almeno una cifra:
        *
        *   000000 -> 0
        */
        while (
            firstSignificantIndex <
                result.length() - 1 &&
            result[
                firstSignificantIndex
            ] == '0'
        )
        {
            ++firstSignificantIndex;
        }

        if (firstSignificantIndex > 0)
        {
            result =
                result.substring(
                    firstSignificantIndex
                );
        }

        return result;
    }


    static String normalizeCardCodeForComparison(
        const char* input)
    {
        if (!input)
        {
            return "";
        }

        return normalizeCardCodeForComparison(
            String(
                input
            )
        );
    }

    static bool resolveReaderCardCode(
        const AccessReader& reader,
        String& output)
    {
        output =
            "";

        if (!reader.readCardCode)
        {
            return false;
        }

        output =
            reader.readCardCode();

        output =
            normalizeCode(
                output
            );

        return
            output.length() > 0;
    }

    static String normalizeReaderAddress(
        String address)
    {
        address.trim();

        if (address.length() == 0)
        {
            return "";
        }

        /*
        * Rimuove gli spazi interni.
        */
        address.replace(
            " ",
            ""
        );

        /*
        * Normalizzazione MAC:
        *
        *   00-04-24-D2-CA-13-BF-AA
        *
        * diventa:
        *
        *   00:04:24:D2:CA:13:BF:AA
        *
        * Un IPv4 rimane invariato.
        */
        address.replace(
            "-",
            ":"
        );

        address.toUpperCase();

        return address;
    }

    const AccessReader* findReaderByAddress(
        const AccessReaderDef* readerDef,
        const String& receivedAddress) const
    {
        if (
            !readerDef ||
            !_configuration.readers
        )
        {
            return nullptr;
        }

        const String normalizedReceivedAddress =
            normalizeReaderAddress(
                receivedAddress
            );

        /*
        * Nessun indirizzo ricevuto.
        *
        * Non viene effettuato un match automatico con i
        * lettori che hanno readerAddress non configurato.
        */
        if (normalizedReceivedAddress.length() == 0)
        {
            return nullptr;
        }

        for (
            size_t readerIndex = 0;
            readerIndex < _configuration.readerCount;
            ++readerIndex
        )
        {
            const AccessReader& reader =
                _configuration.readers[
                    readerIndex
                ];

            if (!reader.enabled)
                continue;

            if (reader.def != readerDef)
                continue;

            /*
            * nullptr o stringa vuota:
            * controllo indirizzo disabilitato.
            *
            * Il lettore non partecipa alla ricerca esplicita
            * per indirizzo.
            */
            if (
                !reader.readerAddress ||
                reader.readerAddress[0] == '\0'
            )
            {
                continue;
            }

            const String normalizedConfiguredAddress =
                normalizeReaderAddress(
                    String(
                        reader.readerAddress
                    )
                );

            if (normalizedConfiguredAddress.length() == 0)
                continue;

            if (
                normalizedConfiguredAddress ==
                normalizedReceivedAddress
            )
            {
                return
                    &reader;
            }
        }

        return nullptr;
    }

    // ========================================================================
    // RUNTIME CONFIGURATION
    // ========================================================================

    Config _configuration;


    // ========================================================================
    // RUNTIME STATE
    // ========================================================================

    bool _initialized =
        false;

    size_t _validReaderCount =
        0;

    size_t _enabledReaderCount =
        0;


public:

    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    AccessControlService() = default;


    // ========================================================================
    // COPY
    //
    // Il servizio contiene std::function e riferimenti a configurazioni
    // esterne. Evitiamo copie accidentali.
    // ========================================================================

    AccessControlService(
        const AccessControlService&) = delete;

    AccessControlService& operator=(
        const AccessControlService&) = delete;


    // ========================================================================
    // SETUP
    //
    // La configurazione viene accettata solamente se tutti i lettori
    // risultano validi.
    // ========================================================================

    bool setup(
        const Config& config)
    {
        reset();

        _configuration =
            config;

        // ====================================================
        // BASE CONFIGURATION
        // ====================================================

        if (
            !_configuration.readers ||
            _configuration.readerCount == 0
        )
        {
            LOG_EF(
                "ACCESS",
                "No access readers configured"
            );

            return false;
        }

        if (
            _configuration.defaultRelayIndex < 0
        )
        {
            LOG_EF(
                "ACCESS",
                "Invalid default relay index: %d",
                _configuration.defaultRelayIndex
            );

            return false;
        }

        if (
            _configuration.defaultRelayTime < 0
        )
        {
            LOG_EF(
                "ACCESS",
                "Invalid default relay time: %d",
                _configuration.defaultRelayTime
            );

            return false;
        }

        // ====================================================
        // COMMON CARD VALIDATION
        // ====================================================

        if (
            hasText(
                _configuration.commonCardCode
            ) &&
            normalizeCode(
                _configuration.commonCardCode
            ).length() == 0
        )
        {
            LOG_EF(
                "ACCESS",
                "Invalid common card code"
            );

            return false;
        }

        // ====================================================
        // READERS VALIDATION
        // ====================================================

        bool configurationValid =
            true;

        for (
            size_t i = 0;
            i < _configuration.readerCount;
            ++i
        )
        {
            const AccessReader& reader =
                _configuration.readers[i];

            if (
                !validateReader(
                    reader,
                    i
                )
            )
            {
                configurationValid =
                    false;

                continue;
            }

            ++_validReaderCount;

            if (reader.enabled)
                ++_enabledReaderCount;

            logReader(
                reader,
                i
            );

            if (
                reader.def &&
                reader.def->diagnostic
            )
            {
                reader.def->diagnostic(
                    reader
                );
            }
        }

        // ====================================================
        // FINAL VALIDATION
        // ====================================================

        if (!configurationValid)
        {
            LOG_EF(
                "ACCESS",
                "Access configuration rejected"
            );

            resetRuntimeCounters();

            return false;
        }

        if (_validReaderCount == 0)
        {
            LOG_EF(
                "ACCESS",
                "No valid access readers found"
            );

            resetRuntimeCounters();

            return false;
        }

        if (_enabledReaderCount == 0)
        {
            LOG_WF(
                "ACCESS",
                "All access readers are disabled"
            );
        }

        _initialized =
            true;

        LOG_IF(
            "ACCESS",
            "Access control initialized: "
            "readers=%u enabled=%u commonCard=%s",
            static_cast<unsigned>(
                _validReaderCount
            ),
            static_cast<unsigned>(
                _enabledReaderCount
            ),
            hasText(
                _configuration.commonCardCode
            )
                ? "CONFIGURED"
                : "NOT_CONFIGURED"
        );

        return true;
    }


    // ========================================================================
    // RESET
    // ========================================================================

    void reset()
    {
        _configuration =
            Config{};

        _initialized =
            false;

        resetRuntimeCounters();
    }


    // ========================================================================
    // VALIDATE ACCESS REQUEST
    // ========================================================================

    Decision validate(
        const Request& request)
    {
        Decision decision;

        /*
        * Codice normalizzato per log, callback e diagnostica.
        *
        * Gli zeri iniziali vengono mantenuti:
        *
        *   000859 -> 000859
        */
        decision.cardCode =
            normalizeCode(
                request.cardCode
            );

        decision.readerDef =
            request.readerDef;

        // ====================================================
        // SERVICE STATUS
        // ====================================================

        if (!_initialized)
        {
            decision.result =
                Result::SERVICE_NOT_INITIALIZED;

            decision.message =
                "Access service unavailable";

            notify(
                request,
                decision
            );

            return decision;
        }

        // ====================================================
        // CARD CODE
        // ====================================================

        if (
            decision.cardCode.length() == 0
        )
        {
            decision.result =
                Result::CARD_EMPTY;

            decision.message =
                "Invalid card";

            notify(
                request,
                decision
            );

            return decision;
        }

        /*
        * Forma utilizzata esclusivamente per i confronti.
        *
        * Esempi:
        *
        *   000859 -> 859
        *   859    -> 859
        *   000000 -> 0
        *
        * decision.cardCode conserva invece il valore 000859.
        */
        const String comparableCardCode =
            normalizeCardCodeForComparison(
                decision.cardCode
            );

        // ====================================================
        // READER LOOKUP
        // ====================================================

        const AccessReader* reader =
            request.reader;

        if (!reader)
        {
            reader =
                findReader(
                    request.readerDef,
                    request.readerAddress,
                    request.readerName
                );
        }

        if (!reader)
        {
            decision.result =
                Result::READER_NOT_FOUND;

            decision.message =
                "Reader not found";

            notify(
                request,
                decision
            );

            return decision;
        }

        decision.reader =
            reader;

        decision.readerDef =
            reader->def;

        // ====================================================
        // DEFINITION MATCH
        // ====================================================

        if (
            request.readerDef &&
            reader->def != request.readerDef
        )
        {
            decision.result =
                Result::READER_DEFINITION_MISMATCH;

            decision.message =
                "Reader definition mismatch";

            notify(
                request,
                decision
            );

            return decision;
        }

        // ====================================================
        // READER STATUS
        // ====================================================

        if (!reader->enabled)
        {
            decision.result =
                Result::READER_DISABLED;

            decision.message =
                "Reader disabled";

            notify(
                request,
                decision
            );

            return decision;
        }

        // ====================================================
        // COMMON CARD
        //
        // Il confronto ignora gli zeri iniziali.
        // ====================================================

        if (
            hasText(
                _configuration.commonCardCode
            )
        )
        {
            const String comparableCommonCardCode =
                normalizeCardCodeForComparison(
                    _configuration.commonCardCode
                );

            if (
                comparableCommonCardCode.length() > 0 &&
                comparableCardCode ==
                    comparableCommonCardCode
            )
            {
                authorize(
                    request,
                    decision,
                    Result::CARD_ACCEPTED_COMMON,
                    true,
                    "Common card accepted"
                );

                return decision;
            }
        }

        // ====================================================
        // READER-SPECIFIC CARD
        //
        // readCardCode può:
        //
        //   - restituire un codice fisso
        //   - leggere getValueFast()
        //   - leggere un database
        //   - usare qualsiasi altra sorgente
        //
        // Il valore restituito può non contenere gli zeri
        // iniziali.
        //
        // Esempio:
        //
        //   codice ricevuto: 000859
        //   valore buffer:   859
        //
        // Entrambi vengono confrontati come "859".
        // ====================================================

        if (reader->readCardCode)
        {
            const String expectedCardCode =
                normalizeCode(
                    reader->readCardCode()
                );

            const String comparableExpectedCardCode =
                normalizeCardCodeForComparison(
                    expectedCardCode
                );

            if (
                comparableExpectedCardCode.length() > 0 &&
                comparableCardCode ==
                    comparableExpectedCardCode
            )
            {
                authorize(
                    request,
                    decision,
                    Result::CARD_ACCEPTED_READER,
                    false,
                    "Reader card accepted"
                );

                return decision;
            }
        }

        // ====================================================
        // EXTERNAL VALIDATOR
        // ====================================================

        if (
            _configuration.validator &&
            _configuration.validator(
                request,
                *reader
            )
        )
        {
            authorize(
                request,
                decision,
                Result::CARD_ACCEPTED_EXTERNAL,
                false,
                "External validation accepted"
            );

            return decision;
        }

        // ====================================================
        // REJECTED
        // ====================================================

        decision.result =
            Result::CARD_REJECTED;

        decision.authorized =
            false;

        decision.commonCode =
            false;

        decision.message =
            "Card rejected";

        decision.relayIndex =
            0;

        decision.relayTime =
            0;

        notify(
            request,
            decision
        );

        return decision;
    }


    // ========================================================================
    // FIND READER BY NAME
    // ========================================================================

    const AccessReader* findReaderByName(
        const char* name) const
    {
        if (
            !_initialized ||
            !hasText(name)
        )
        {
            return nullptr;
        }

        for (
            size_t i = 0;
            i < _configuration.readerCount;
            ++i
        )
        {
            const AccessReader& reader =
                _configuration.readers[i];

            if (
                reader.name &&
                strcmp(
                    reader.name,
                    name
                ) == 0
            )
            {
                return &reader;
            }
        }

        return nullptr;
    }
    

    // ========================================================================
    // FIND UNIQUE READER BY DEFINITION
    //
    // Restituisce nullptr se piu lettori condividono la stessa definizione.
    // ========================================================================

    const AccessReader*
    findUniqueReaderByDefinition(
        const AccessReaderDef* def) const
    {
        if (
            !_initialized ||
            !def
        )
        {
            return nullptr;
        }

        const AccessReader* match =
            nullptr;

        for (
            size_t i = 0;
            i < _configuration.readerCount;
            ++i
        )
        {
            const AccessReader& reader =
                _configuration.readers[i];

            if (
                !reader.enabled ||
                reader.def != def
            )
            {
                continue;
            }

            if (match)
            {
                return nullptr;
            }

            match =
                &reader;
        }

        return match;
    }


    // ========================================================================
    // STATE ACCESSORS
    // ========================================================================

    bool isInitialized() const
    {
        return _initialized;
    }


    size_t readerCount() const
    {
        return
            _configuration.readers
                ? _configuration.readerCount
                : 0;
    }


    size_t validReaderCount() const
    {
        return _validReaderCount;
    }


    size_t enabledReaderCount() const
    {
        return _enabledReaderCount;
    }


    const AccessReader* getReader(
        size_t index) const
    {
        if (
            !_configuration.readers ||
            index >= _configuration.readerCount
        )
        {
            return nullptr;
        }

        return
            &_configuration.readers[index];
    }


    const Config& getConfig() const
    {
        return _configuration;
    }


    // ========================================================================
    // RESULT STRING
    //
    // Utility stateless, puo rimanere statica.
    // ========================================================================

    static const char* ResultToString(
        Result result)
    {
        switch (result)
        {
            case Result::INVALID_REQUEST:
                return "INVALID_REQUEST";

            case Result::SERVICE_NOT_INITIALIZED:
                return "SERVICE_NOT_INITIALIZED";

            case Result::READER_NOT_FOUND:
                return "READER_NOT_FOUND";

            case Result::READER_DISABLED:
                return "READER_DISABLED";

            case Result::READER_DEFINITION_MISMATCH:
                return "READER_DEFINITION_MISMATCH";

            case Result::CARD_EMPTY:
                return "CARD_EMPTY";

            case Result::CARD_REJECTED:
                return "CARD_REJECTED";

            case Result::CARD_ACCEPTED_COMMON:
                return "CARD_ACCEPTED_COMMON";

            case Result::CARD_ACCEPTED_READER:
                return "CARD_ACCEPTED_READER";

            case Result::CARD_ACCEPTED_EXTERNAL:
                return "CARD_ACCEPTED_EXTERNAL";
        }

        return "UNKNOWN";
    }


private:

    // ========================================================================
    // VALIDATE READER
    // ========================================================================

    bool validateReader(
        const AccessReader& reader,
        size_t readerIndex) const
    {
        bool valid =
            true;

        // ====================================================
        // NAME
        // ====================================================

        if (!hasText(reader.name))
        {
            LOG_EF(
                "ACCESS",
                "Reader[%u] has no name",
                static_cast<unsigned>(
                    readerIndex
                )
            );

            valid =
                false;
        }
        else if (
            hasDuplicateName(
                readerIndex
            )
        )
        {
            LOG_EF(
                "ACCESS",
                "Duplicate reader name: '%s'",
                reader.name
            );

            valid =
                false;
        }

        // ====================================================
        // DEFINITION
        // ====================================================

        if (!reader.def)
        {
            LOG_EF(
                "ACCESS",
                "Reader[%u] '%s' has no definition",
                static_cast<unsigned>(
                    readerIndex
                ),
                reader.name
                    ? reader.name
                    : ""
            );

            valid =
                false;
        }
        else
        {
            if (!hasText(reader.def->id))
            {
                LOG_EF(
                    "ACCESS",
                    "Reader '%s' definition has no id",
                    reader.name
                        ? reader.name
                        : ""
                );

                valid =
                    false;
            }

            if (!hasText(reader.def->name))
            {
                LOG_EF(
                    "ACCESS",
                    "Reader '%s' definition has no name",
                    reader.name
                        ? reader.name
                        : ""
                );

                valid =
                    false;
            }
        }

        // ====================================================
        // READER ADDRESS
        //
        // readerAddress può essere:
        //
        //   - nullptr o vuoto: controllo disabilitato
        //   - indirizzo IPv4
        //   - indirizzo MAC
        // ====================================================

        if (
            hasText(
                reader.readerAddress
            )
        )
        {
            const String normalizedAddress =
                normalizeReaderAddress(
                    String(
                        reader.readerAddress
                    )
                );

            if (normalizedAddress.length() == 0)
            {
                LOG_EF(
                    "ACCESS",
                    "Reader '%s' has invalid address",
                    reader.name
                        ? reader.name
                        : ""
                );

                valid =
                    false;
            }
            else if (
                hasDuplicateAddress(
                    readerIndex
                )
            )
            {
                LOG_EF(
                    "ACCESS",
                    "Duplicate reader address: "
                    "reader='%s' address=%s",
                    reader.name
                        ? reader.name
                        : "",
                    reader.readerAddress
                );

                valid =
                    false;
            }
        }

        // ====================================================
        // CARD CODE READER
        //
        // readCardCode è opzionale.
        //
        // Non viene eseguita durante il setup perché la sorgente
        // runtime, per esempio DomoManager o il buffer, potrebbe
        // non essere ancora inizializzata.
        // ====================================================

        if (!reader.readCardCode)
        {
            LOG_DF(
                "ACCESS",
                "Reader '%s' has no card code reader",
                reader.name
                    ? reader.name
                    : ""
            );
        }

        // ====================================================
        // DEFINITION-SPECIFIC VALIDATION
        // ====================================================

        if (
            reader.def &&
            reader.def->validateConfig
        )
        {
            String error;

            if (
                !reader.def->validateConfig(
                    reader,
                    error
                )
            )
            {
                LOG_EF(
                    "ACCESS",
                    "Reader '%s' configuration invalid: %s",
                    reader.name
                        ? reader.name
                        : "",
                    error.length() > 0
                        ? error.c_str()
                        : "definition validation failed"
                );

                valid =
                    false;
            }
        }

        return valid;
    }



    // ========================================================================
    // INTERNAL READER LOOKUP
    //
    // Priorita:
    //
    //   1. indirizzo IP
    //   2. nome
    //   3. definizione, solamente se univoca
    // ========================================================================

    const AccessReader* findReader(
        const AccessReaderDef* def,
        const String& address,
        const String& name) const
    {
        if (
            !_initialized ||
            !_configuration.readers
        )
        {
            return nullptr;
        }

        const AccessReader* resolvedReader =
            nullptr;

        if (
            ResolveIncomingReader(
                def,
                name,
                address,
                resolvedReader
            )
        )
        {
            return resolvedReader;
        }

        return nullptr;
    }


    // ========================================================================
    // AUTHORIZE
    // ========================================================================

    void authorize(
        const Request& request,
        Decision& decision,
        Result result,
        bool commonCode,
        const char* message)
    {
        decision.result =
            result;

        decision.authorized =
            true;

        decision.commonCode =
            commonCode;

        decision.message =
            message
                ? message
                : "Access accepted";

        decision.relayIndex =
            _configuration.defaultRelayIndex;

        decision.relayTime =
            _configuration.defaultRelayTime;

        notify(
            request,
            decision
        );

        if (_configuration.onAuthorized)
        {
            _configuration.onAuthorized(
                request,
                decision
            );
        }
    }


    // ========================================================================
    // NOTIFY
    // ========================================================================

    void notify(
        const Request& request,
        const Decision& decision)
    {
        const AccessReaderDef* definition =
            decision.readerDef
                ? decision.readerDef
                : request.readerDef;

        const char* readerName =
            (
                decision.reader &&
                decision.reader->name
            )
                ? decision.reader->name
                : request.readerName.c_str();

        LOG_DF(
            "ACCESS",
            "reader=%s def=%s card=%s "
            "authorized=%u common=%u result=%s",
            readerName
                ? readerName
                : "",
            (
                definition &&
                definition->id
            )
                ? definition->id
                : "",
            decision.cardCode.c_str(),
            decision.authorized
                ? 1U
                : 0U,
            decision.commonCode
                ? 1U
                : 0U,
            ResultToString(
                decision.result
            )
        );

        if (_configuration.onEvent)
        {
            _configuration.onEvent(
                request,
                decision
            );
        }
    }


    // ========================================================================
    // READER LOG
    // ========================================================================

    void logReader(
        const AccessReader& reader,
        size_t index) const
    {
        const char* readerName =
            reader.name
                ? reader.name
                : "";

        const char* definitionId =
            (
                reader.def &&
                reader.def->id
            )
                ? reader.def->id
                : "";

        const char* configuredAddress =
            hasText(
                reader.readerAddress
            )
                ? reader.readerAddress
                : "ANY";

        const char* addressValidation =
            hasText(
                reader.readerAddress
            )
                ? "ENABLED"
                : "DISABLED";

        const char* cardReaderStatus =
            reader.readCardCode
                ? "CONFIGURED"
                : "NOT_CONFIGURED";

        LOG_DF(
            "ACCESS",
            "Reader[%u] name=%s def=%s "
            "address=%s addressCheck=%s "
            "enabled=%u door=%d area=%d "
            "cardReader=%s",
            static_cast<unsigned>(
                index
            ),
            readerName,
            definitionId,
            configuredAddress,
            addressValidation,
            reader.enabled
                ? 1U
                : 0U,
            reader.doorIndex,
            reader.area,
            cardReaderStatus
        );
    }


    // ========================================================================
    // DUPLICATE NAME
    // ========================================================================

    bool hasDuplicateName(
        size_t currentIndex) const
    {
        if (
            !_configuration.readers ||
            currentIndex >=
                _configuration.readerCount
        )
        {
            return false;
        }

        const AccessReader& current =
            _configuration.readers[
                currentIndex
            ];

        if (!hasText(current.name))
            return false;

        for (
            size_t i = 0;
            i < _configuration.readerCount;
            ++i
        )
        {
            if (i == currentIndex)
                continue;

            const AccessReader& other =
                _configuration.readers[i];

            if (
                other.name &&
                strcmp(
                    current.name,
                    other.name
                ) == 0
            )
            {
                return true;
            }
        }

        return false;
    }


    // ========================================================================
    // DUPLICATE ADDRESS
    // ========================================================================

    bool hasDuplicateAddress(
        size_t currentIndex) const
    {
        if (
            !_configuration.readers ||
            currentIndex >=
                _configuration.readerCount
        )
        {
            return false;
        }

        const AccessReader& current =
            _configuration.readers[
                currentIndex
            ];

        /*
        * readerAddress assente:
        * validazione indirizzo disabilitata.
        */
        if (
            !hasText(
                current.readerAddress
            )
        )
        {
            return false;
        }

        const String currentAddress =
            normalizeReaderAddress(
                String(
                    current.readerAddress
                )
            );

        if (currentAddress.length() == 0)
            return false;

        for (
            size_t otherIndex = 0;
            otherIndex < _configuration.readerCount;
            ++otherIndex
        )
        {
            if (otherIndex == currentIndex)
                continue;

            const AccessReader& other =
                _configuration.readers[
                    otherIndex
                ];

            if (
                other.def != current.def ||
                !hasText(
                    other.readerAddress
                )
            )
            {
                continue;
            }

            const String otherAddress =
                normalizeReaderAddress(
                    String(
                        other.readerAddress
                    )
                );

            if (
                currentAddress ==
                otherAddress
            )
            {
                return true;
            }
        }

        return false;
    }


    // ========================================================================
    // CARD NORMALIZATION
    //
    // Non vengono rimossi gli zeri iniziali.
    // ========================================================================

    static String normalizeCode(
        const String& input)
    {
        String result =
            input;

        result.trim();

        result.replace(
            " ",
            ""
        );

        result.toUpperCase();

        return result;
    }


    static String normalizeCode(
        const char* input)
    {
        if (!input)
            return "";

        return normalizeCode(
            String(input)
        );
    }


    // ========================================================================
    // TEXT VALIDATION
    // ========================================================================

    static bool hasText(
        const char* value)
    {
        return
            value &&
            value[0] != '\0';
    }


    // ========================================================================
    // RESET COUNTERS
    // ========================================================================

    void resetRuntimeCounters()
    {
        _validReaderCount =
            0;

        _enabledReaderCount =
            0;
    }
};

#endif