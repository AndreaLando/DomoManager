#ifndef DMWebAPIDefs_HPP
#define DMWebAPIDefs_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it

   Versione modulo: 3.0.0
   Ultima modifica: 2026-10-20

   Note:
                    • Interfaccia comune protocolli accesso
                    • Gestione Fanvil incapsulata
                    • Gestione AxessTMC incapsulata
                    • Validazione centralizzata AccessControlService
                    • Definizione lettore posseduta dal protocollo
                    • Profilo polling posseduto dal protocollo
                    • Endpoint HTTP posseduto dal protocollo
                    • Nessuna dipendenza da AEE

   ============================================================================ */

#include "DMWebAPI.hpp"
#include "DMAccessControl.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================================
// ACCESS WEB API ENDPOINT DEFINITION
//
// Un singolo protocollo può esporre più endpoint HTTP.
// ============================================================================

struct AccessWebAPIEndpointDef
{
    const char* name =
        nullptr;

    const char* path =
        nullptr;

    const char* method =
        nullptr;
};



// ============================================================================
// ACCESS WEB API INTERFACE
//
// Interfaccia comune per tutti i protocolli di controllo accessi:
//
//   • Fanvil
//   • AxessTMC
//   • futuri terminali
//
// Ogni implementazione fornisce:
//
//   • identificativo protocollo
//   • definizione AccessReaderDef
//   • eventuale profilo polling
//   • endpoint HTTP server
//   • gestione richiesta HTTP
//
// La validazione della tessera viene delegata a:
//
//   AccessControlService
// ============================================================================

class IAccessWebAPI
{
public:

    virtual ~IAccessWebAPI() = default;


    // ========================================================================
    // IDENTITY
    // ========================================================================

    virtual const char* protocolId() const = 0;

    virtual const char* protocolName() const = 0;


    // ========================================================================
    // ACCESS READER DEFINITION
    // ========================================================================

    virtual const AccessReaderDef&
    readerDefinition() const = 0;


    // ========================================================================
    // POLLING PROFILE
    //
    // Può restituire nullptr se il protocollo non prevede polling.
    // ========================================================================

    virtual const DeviceMessageProfile*
    pollingProfile() const = 0;


    // ========================================================================
    // SERVER ENDPOINTS
    //
    // Un protocollo può esporre uno o più endpoint.
    // ========================================================================

    virtual size_t endpointCount() const = 0;


    virtual const AccessWebAPIEndpointDef*
        endpointDefinition(size_t endpointIndex) const = 0;


    // ========================================================================
    // HTTP REQUEST
    // ========================================================================

    virtual void handleRequest(
        size_t endpointIndex,
        const HttpServerRequest& request,
        HttpServerResponse& response) = 0;
};


// ============================================================================
// ACCESS WEB API COMMON UTILITIES
// ============================================================================

class AccessWebAPIUtils
{
public:

    // ========================================================================
    // URL DECODE
    // ========================================================================

    static String UrlDecode(
        const String& input)
    {
        String output;

        output.reserve(
            input.length()
        );

        for (
            size_t i = 0;
            i < input.length();
            ++i
        )
        {
            const char current =
                input[i];

            if (current == '+')
            {
                output += ' ';
                continue;
            }

            if (
                current == '%' &&
                i + 2 < input.length()
            )
            {
                const int high =
                    HexValue(
                        input[i + 1]
                    );

                const int low =
                    HexValue(
                        input[i + 2]
                    );

                if (
                    high >= 0 &&
                    low >= 0
                )
                {
                    output +=
                        static_cast<char>(
                            (high << 4) |
                            low
                        );

                    i += 2;

                    continue;
                }
            }

            output += current;
        }

        return output;
    }


    // ========================================================================
    // CSV FIELD
    //
    // Mantiene correttamente i campi vuoti.
    // ========================================================================

    static String GetCsvField(
        const String& value,
        size_t requestedIndex)
    {
        size_t currentIndex = 0;

        int fieldStart = 0;

        for (
            int position = 0;
            position <= value.length();
            ++position
        )
        {
            const bool fieldEnd =
                position == value.length() ||
                value[position] == ',';

            if (!fieldEnd)
                continue;

            if (
                currentIndex ==
                requestedIndex
            )
            {
                String field =
                    value.substring(
                        fieldStart,
                        position
                    );

                field.trim();

                return field;
            }

            ++currentIndex;

            fieldStart =
                position + 1;
        }

        return "";
    }


    // ========================================================================
    // FIND NAMED TRANSACTION FIELD
    //
    // Funzione generica: non dipende direttamente dai tipi annidati di
    // AxessTMCWebAPI.
    //
    // Il tipo concreto viene dedotto quando la funzione viene chiamata con:
    //
    //   AxessTMCWebAPI::WebAPIConfig
    //
    // Il tipo restituito viene dedotto dagli elementi puntati da:
    //
    //   config.transactionFields
    // ========================================================================

    template<
        typename TConfig
    >
    static auto FindTransactionField(
        const TConfig& config,
        const char* fieldName)
        -> decltype(
            config.transactionFields
        )
    {
        if (
            !fieldName ||
            fieldName[0] == '\0' ||
            !config.transactionFields
        )
        {
            return nullptr;
        }

        for (
            size_t fieldIndex = 0;
            fieldIndex <
                config.transactionFieldCount;
            ++fieldIndex
        )
        {
            const auto& field =
                config.transactionFields[
                    fieldIndex
                ];

            if (
                field.name &&
                strcmp(
                    field.name,
                    fieldName
                ) == 0
            )
            {
                return
                    &field;
            }
        }

        return nullptr;
    }


    // ========================================================================
    // GET NAMED TRANSACTION FIELD
    //
    // Il tipo della configurazione viene dedotto automaticamente.
    //
    // Esempio:
    //
    //   const AxessTMCWebAPI::WebAPIConfig& config =
    //       AxessTMCWebAPI::Config();
    //
    //   AccessWebAPIUtils::GetTransactionField(
    //       config,
    //       transaction,
    //       "COD_UTENTE",
    //       output
    //   );
    // ========================================================================

    template<
        typename TConfig
    >
    static bool GetTransactionField(
        const TConfig& config,
        const String& transaction,
        const char* fieldName,
        String& output)
    {
        output =
            "";

        const auto* field =
            FindTransactionField(
                config,
                fieldName
            );

        if (!field)
        {
            return false;
        }

        output =
            GetCsvField(
                transaction,
                field->index
            );

        return true;
    }

    // ========================================================================
    // RESPONSE VALUE SANITIZER
    // ========================================================================

    static String SanitizeResponseValue(
        String value)
    {
        value.replace(
            "\r",
            " "
        );

        value.replace(
            "\n",
            " "
        );

        return value;
    }


    // ========================================================================
    // STANDARD RESPONSE
    // ========================================================================

    static void BuildTextResponse(
        HttpServerResponse& response,
        int statusCode,
        const String& body)
    {
        response.statusCode =
            statusCode;

        response.contentType =
            "text/plain";

        response.body =
            body;

        response.closeConnection =
            true;
    }
private:

    static int HexValue(
        char value)
    {
        if (
            value >= '0' &&
            value <= '9'
        )
        {
            return value - '0';
        }

        if (
            value >= 'A' &&
            value <= 'F'
        )
        {
            return value - 'A' + 10;
        }

        if (
            value >= 'a' &&
            value <= 'f'
        )
        {
            return value - 'a' + 10;
        }

        return -1;
    }
};


// ============================================================================
// FANVIL WEB API
//
// Implementazione non statica.
//
// Dipendenze:
//   • AccessControlService ricevuto nel costruttore
//
// Nessun singleton.
// Nessun AccessControlService globale.
// ============================================================================

class FanvilWebAPI :
    public IAccessWebAPI
{
public:

    // ========================================================================
    // NORMALIZED FANVIL EVENT
    // ========================================================================

    struct AccessEvent
    {
        String raw;

        String readerName;
        String readerAddress;

        String door;
        String result;
        String time;
        String personName;
        String cardCode;
        String cardType;
        String mac;

        bool valid = false;
    };


private:

    // ========================================================================
    // ACCESS CONTROL SERVICE
    // ========================================================================

    AccessControlService& _accessControl;


public:

    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    explicit FanvilWebAPI(
        AccessControlService& accessControl)
        : _accessControl(
              accessControl
          )
    {
    }


    // ========================================================================
    // COPY DISABLED
    // ========================================================================

    FanvilWebAPI(
        const FanvilWebAPI&) = delete;

    FanvilWebAPI& operator=(
        const FanvilWebAPI&) = delete;


    // ========================================================================
    // IDENTITY
    // ========================================================================

    const char* protocolId() const override
    {
        return "fanvil";
    }


    const char* protocolName() const override
    {
        return "Fanvil Access Reader";
    }


    // ========================================================================
    // ACCESS READER DEFINITION
    // ========================================================================

    const AccessReaderDef&
    readerDefinition() const override
    {
        return ReaderDefinition();
    }


    static const AccessReaderDef&
    ReaderDefinition()
    {
        static const AccessReaderDef definition =
        {
            .id =
                "fanvil",

            .name =
                "Fanvil Access Reader",

            .supportsServerPush =
                true,

            .supportsOnlineValidation =
                false,

            .supportsPolling =
                true,

            .supportsRemoteRelay =
                true,

            .defaultPort =
                80,

            .validateConfig =
                ValidateReaderConfig,

            .diagnostic =
                nullptr
        };

        return definition;
    }


    // ========================================================================
    // POLLING PROFILE
    // ========================================================================

    const DeviceMessageProfile*
    pollingProfile() const override
    {
        return &Profile();
    }


    static const DeviceMessageProfile&
    Profile()
    {
        static const DeviceMessageProfile::OutMessage
            outMessages[] =
            {
                {
                    "Incomingcall",
                    "/newcall.xml?num=${call_id}",
                    "GET",
                    nullptr
                },
                {
                    "DoorSensor1",
                    "/door1.xml?state=${state}",
                    "GET",
                    nullptr
                },
                {
                    "Tamper",
                    "/tamper.xml?mac=${mac}",
                    "GET",
                    nullptr
                },
                {
                    "KEEPALIVE",
                    "/keepalive.xml?mac=${mac}",
                    "GET",
                    nullptr
                },
                {
                    /*
                    * Attivazione uscita interna Fanvil.
                    *
                    * Equivalente a:
                    *
                    * http://admin:admin@IP_FANVIL/
                    * cgi-bin/ConfigManApp.com?
                    * egs&output1=OUT1_SOS
                    */
                    "RELAY_OPEN",
                    "/cgi-bin/ConfigManApp.com"
                    "?egs&output1=OUT1_SOS",
                    "GET",
                    nullptr,
                    DeviceMessageProfile::ResponseFormat::RAW
                }
            };


        static const DeviceMessageProfile::InField
            inFields[] =
            {
                {
                    "key",
                    "key=(.*)"
                },
                {
                    "code",
                    "code=(.*)"
                }
            };


        static const DeviceMessageProfile::CorrelationRule
            correlationRules[] =
            {
                {
                    "Incomingcall",
                    "key",
                    "optional"
                },
                {
                    "DoorSensor1",
                    "key",
                    "optional"
                }
            };


        static const DeviceMessageProfile::StateRule
            stateRules[] =
            {
                {
                    "ONLINE",
                    "timeout",
                    "OFFLINE",
                    nullptr
                },
                {
                    "OFFLINE",
                    "key",
                    "ONLINE",
                    nullptr
                },
                {
                    "ONLINE",
                    "key",
                    "ONLINE",
                    nullptr
                }
            };


        static const DeviceMessageProfile
            profile =
            {
                .profileId =
                    2001,

                .name =
                    "Fanvil_Doorphone",

                .outMessages =
                    outMessages,

                .outCount =
                    sizeof(outMessages) /
                    sizeof(outMessages[0]),

                .inFields =
                    inFields,

                .inCount =
                    sizeof(inFields) /
                    sizeof(inFields[0]),

                .rules =
                    correlationRules,

                .ruleCount =
                    sizeof(correlationRules) /
                    sizeof(correlationRules[0]),

                .stateRules =
                    stateRules,

                .stateRuleCount =
                    sizeof(stateRules) /
                    sizeof(stateRules[0])
            };


        return profile;
    }


    // ========================================================================
    // SERVER ENDPOINTS
    // ========================================================================

    enum EndpointIndex : size_t
    {
        ENDPOINT_ACCESS =
            0,

        ENDPOINT_DEVICE_LOG =
            1,

        ENDPOINT_COUNT =
            2
    };

    size_t endpointCount() const override
    {
        return
            ENDPOINT_COUNT;
    }


    const AccessWebAPIEndpointDef*
    endpointDefinition(
        size_t endpointIndex) const override
    {
        static const AccessWebAPIEndpointDef
            endpoints[] =
            {
                /*
                * Endpoint manuale/diagnostico.
                */
                {
                    "FanvilAccess",
                    "/fanvil/access",
                    "GET"
                },

                /*
                * Endpoint reale utilizzato dal Fanvil i60K
                * per il Relay Log Export.
                */
                {
                    "FanvilDeviceLog",
                    "/v1/api/device_log",
                    "POST"
                }
            };

        if (
            endpointIndex >=
            ENDPOINT_COUNT
        )
        {
            return nullptr;
        }

        return
            &endpoints[
                endpointIndex
            ];
    }


    // ========================================================================
    // HTTP REQUEST
    // ========================================================================

    void handleRequest(
        size_t endpointIndex,
        const HttpServerRequest& request,
        HttpServerResponse& response) override
    {
        switch (endpointIndex)
        {
            // ====================================================
            // MANUAL / DIAGNOSTIC ACCESS REQUEST
            // ====================================================

            case ENDPOINT_ACCESS:
            {
                HandleAccessRequest(
                    request,
                    response
                );

                return;
            }

            // ====================================================
            // FANVIL I60K DEVICE LOG
            // ====================================================

            case ENDPOINT_DEVICE_LOG:
            {
                HandleDeviceLog(
                    request,
                    response
                );

                return;
            }

            // ====================================================
            // UNKNOWN ENDPOINT
            // ====================================================

            default:
            {
                LOG_WF(
                    "FANVIL",
                    "Unknown endpoint index: %u",
                    static_cast<unsigned>(
                        endpointIndex
                    )
                );

                AccessWebAPIUtils::BuildTextResponse(
                    response,
                    404,
                    "NOT FOUND"
                );

                return;
            }
        }
    }


private:

    void HandleAccessRequest(
        const HttpServerRequest& request,
        HttpServerResponse& response)
    {
        // ====================================================
        // REQUEST PARSING
        // ====================================================

        AccessEvent event;

        if (
            !ParseRequest(
                request,
                event
            )
        )
        {
            LOG_WF(
                "FANVIL",
                "Invalid access request: "
                "method=%s path=%s query='%s' body='%s'",
                request.method.c_str(),
                request.path.c_str(),
                request.query.c_str(),
                request.body.c_str()
            );

            AccessWebAPIUtils::BuildTextResponse(
                response,
                400,
                "INVALID"
            );

            return;
        }

        // ====================================================
        // ACCESS REQUEST
        // ====================================================

        AccessControlService::Request
            accessRequest;

        accessRequest.readerDef =
            &readerDefinition();

        accessRequest.readerName =
            event.readerName;

        accessRequest.readerAddress =
            event.readerAddress;

        accessRequest.cardCode =
            event.cardCode;

        accessRequest.personName =
            event.personName;

        accessRequest.door =
            event.door;

        accessRequest.timestamp =
            event.time;

        accessRequest.rawPayload =
            event.raw;

        // ====================================================
        // ACCESS VALIDATION
        // ====================================================

        const AccessControlService::Decision
            decision =
                _accessControl.validate(
                    accessRequest
                );

        // ====================================================
        // DIAGNOSTICS
        // ====================================================

        LOG_IF(
            "FANVIL",
            "Access: reader=%s card=%s "
            "authorized=%u result=%s",
            (
                decision.reader &&
                decision.reader->name
            )
                ? decision.reader->name
                : event.readerName.c_str(),
            decision.cardCode.c_str(),
            decision.authorized
                ? 1U
                : 0U,
            AccessControlService::ResultToString(
                decision.result
            )
        );

        // ====================================================
        // HTTP RESPONSE
        //
        // L'apertura dell'uscita viene gestita dalla callback
        // AccessControlService::onAuthorized.
        // ====================================================

        AccessWebAPIUtils::BuildTextResponse(
            response,
            200,
            decision.authorized
                ? "AUTHORIZED"
                : "DENIED"
        );
    }

    void HandleDeviceLog(
        const HttpServerRequest& request,
        HttpServerResponse& response)
    {
        // ====================================================
        // BODY VALIDATION
        // ====================================================

        if (request.body.length() == 0)
        {
            LOG_WF(
                "FANVIL",
                "Device log without body"
            );

            AccessWebAPIUtils::BuildTextResponse(
                response,
                400,
                "BAD REQUEST"
            );

            return;
        }

        // ====================================================
        // JSON PARSING
        // ====================================================

        StaticJsonDocument<2048>
            document;

        const DeserializationError error =
            deserializeJson(
                document,
                request.body
            );

        if (error)
        {
            LOG_WF(
                "FANVIL",
                "Invalid device log JSON: "
                "error=%s bytes=%u",
                error.c_str(),
                static_cast<unsigned>(
                    request.body.length()
                )
            );

            AccessWebAPIUtils::BuildTextResponse(
                response,
                400,
                "BAD REQUEST"
            );

            return;
        }

        // ====================================================
        // MESSAGE TYPE
        // ====================================================

        const char* item =
            document["type"]["item"] |
            "";

        if (
            strcmp(
                item,
                "device_log"
            ) != 0
        )
        {
            LOG_WF(
                "FANVIL",
                "Unsupported device log type: '%s'",
                item
            );

            AccessWebAPIUtils::BuildTextResponse(
                response,
                400,
                "BAD REQUEST"
            );

            return;
        }

        // ====================================================
        // DATA ARRAY
        // ====================================================

        JsonArray data =
            document["data"]
                .as<JsonArray>();

        if (
            data.isNull() ||
            data.size() == 0
        )
        {
            LOG_WF(
                "FANVIL",
                "Device log without records"
            );

            AccessWebAPIUtils::BuildTextResponse(
                response,
                200,
                "OK"
            );

            return;
        }

        LOG_IF(
            "FANVIL",
            "Device log received: records=%u",
            static_cast<unsigned>(
                data.size()
            )
        );

        // ====================================================
        // PROCESS RECORDS
        // ====================================================

        for (
            JsonObject record :
            data
        )
        {
            String readerAddress =
                record["mac"] |
                "";

            String personName =
                record["userName"] |
                "";

            String extra =
                record["extra"] |
                "";

            readerAddress.trim();
            personName.trim();
            extra.trim();

            LOG_IF(
                "FANVIL",
                "Device log: mac=%s user='%s' extra='%s'",
                readerAddress.c_str(),
                personName.c_str(),
                extra.c_str()
            );

            // ====================================================
            // CARD EVENT
            //
            // Formato reale Fanvil i60K:
            //
            //   C:<cardCode>
            //
            // Esempio:
            //
            //   C:2953393440
            // ====================================================

            if (
                !extra.startsWith(
                    "C:"
                )
            )
            {
                LOG_DF(
                    "FANVIL",
                    "Ignored non-card event: extra='%s'",
                    extra.c_str()
                );

                continue;
            }

            String cardCode =
                extra.substring(
                    2
                );

            cardCode.trim();

            if (cardCode.length() == 0)
            {
                LOG_WF(
                    "FANVIL",
                    "Empty card code: extra='%s'",
                    extra.c_str()
                );

                continue;
            }

            // ====================================================
            // CARD VALIDATION
            // ====================================================

            bool validCardCode =
                true;

            for (
                size_t index = 0;
                index < cardCode.length();
                ++index
            )
            {
                if (
                    cardCode[index] < '0' ||
                    cardCode[index] > '9'
                )
                {
                    validCardCode =
                        false;

                    break;
                }
            }

            if (!validCardCode)
            {
                LOG_WF(
                    "FANVIL",
                    "Invalid card code format: '%s'",
                    cardCode.c_str()
                );

                continue;
            }

            // ====================================================
            // READER RESOLUTION
            //
            // Il MAC Fanvil viene usato come readerAddress.
            // ====================================================

            const AccessReader* resolvedReader =
                nullptr;

            if (
                !_accessControl.ResolveIncomingReader(
                    &readerDefinition(),
                    "FanvilIngresso",
                    readerAddress,
                    resolvedReader
                )
            )
            {
                LOG_WF(
                    "FANVIL",
                    "Unknown device log reader: "
                    "mac=%s card=%s",
                    readerAddress.c_str(),
                    cardCode.c_str()
                );

                continue;
            }

            // ====================================================
            // ACCESS REQUEST
            // ====================================================

            AccessControlService::Request
                accessRequest;

            accessRequest.reader =
                resolvedReader;

            accessRequest.readerDef =
                &readerDefinition();

            accessRequest.readerName =
                (
                    resolvedReader &&
                    resolvedReader->name
                )
                    ? resolvedReader->name
                    : "FanvilIngresso";

            accessRequest.readerAddress =
                readerAddress;

            accessRequest.cardCode =
                cardCode;

            accessRequest.personName =
                personName;

            accessRequest.rawPayload =
                extra;

            // ====================================================
            // ACCESS VALIDATION
            // ====================================================

            const AccessControlService::Decision
                decision =
                    _accessControl.validate(
                        accessRequest
                    );

            LOG_IF(
                "FANVIL",
                "Card event: reader=%s mac=%s "
                "card=%s authorized=%u result=%s",
                accessRequest.readerName.c_str(),
                readerAddress.c_str(),
                cardCode.c_str(),
                decision.authorized
                    ? 1U
                    : 0U,
                AccessControlService::ResultToString(
                    decision.result
                )
            );
        }

        // ====================================================
        // HTTP RESPONSE
        //
        // Il Fanvil richiede solo conferma di ricezione.
        // La decisione viene gestita internamente da
        // AccessControlService e dalle callback frontend.
        // ====================================================

        AccessWebAPIUtils::BuildTextResponse(
            response,
            200,
            "OK"
        );
    }

    // ========================================================================
    // READER CONFIG VALIDATION
    // ========================================================================

    static bool ValidateReaderConfig(
        const AccessReader& reader,
        String& error)
    {
        if (reader.doorIndex < 0)
        {
            error =
                "doorIndex not configured";

            return false;
        }

        return true;
    }


    // ========================================================================
    // REQUEST PARSER
    //
    // Supporta:
    //
    //   GET /?number=123&type=card&name=Mario
    //   GET /?code=123&door=1
    //
    //   POST:
    //   door1:success,time,name,number,type,mac
    // ========================================================================

    static bool ParseRequest(
        const HttpServerRequest& request,
        AccessEvent& event)
    {
        event =
            AccessEvent{};

        event.raw =
            request.body.length() > 0
                ? request.body
                : request.query;

        // ====================================================
        // QUERY PARAMETERS
        // ====================================================

        event.cardCode =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "number"
                )
            );

        if (event.cardCode.length() == 0)
        {
            event.cardCode =
                AccessWebAPIUtils::UrlDecode(
                    request.getQueryValue(
                        "card"
                    )
                );
        }

        if (event.cardCode.length() == 0)
        {
            event.cardCode =
                AccessWebAPIUtils::UrlDecode(
                    request.getQueryValue(
                        "code"
                    )
                );
        }

        event.personName =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "name"
                )
            );

        event.cardType =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "type"
                )
            );

        event.mac =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "mac"
                )
            );

        event.result =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "result"
                )
            );

        event.time =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "time"
                )
            );

        event.door =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "door"
                )
            );

        if (event.door.length() == 0)
        {
            event.door =
                AccessWebAPIUtils::UrlDecode(
                    request.getQueryValue(
                        "index"
                    )
                );
        }

        /*
         * Se il Fanvil invia un nome logico, lo preferiamo.
         */
        event.readerName =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "reader"
                )
            );

        if (event.readerName.length() == 0)
        {
            event.readerName =
                event.mac;
        }

        /*
         * Quando HttpServerRequest espone remoteAddress:
         *
         * event.readerAddress =
         *     request.remoteAddress.toString();
         */

        if (event.cardCode.length() > 0)
        {
            event.valid =
                true;

            return true;
        }

        // ====================================================
        // BODY
        // ====================================================

        String payload =
            request.body;

        payload.trim();

        if (payload.length() == 0)
            return false;

        /*
         * Supporta:
         *
         *   log=...
         *   data=...
         *   info=...
         */
        const int equals =
            payload.indexOf('=');

        if (equals > 0)
        {
            String key =
                payload.substring(
                    0,
                    equals
                );

            key.trim();
            key.toLowerCase();

            if (
                key == "log" ||
                key == "data" ||
                key == "info"
            )
            {
                payload =
                    payload.substring(
                        equals + 1
                    );
            }
        }

        payload =
            AccessWebAPIUtils::UrlDecode(
                payload
            );

        event.raw =
            payload;

        return ParseRelayLog(
            payload,
            event
        );
    }


    // ========================================================================
    // RELAY LOG PARSER
    //
    // Formato:
    //
    //   door$index:$result,$time,$name,$number,$type,$mac
    //
    // Esempio:
    //
    //   door1:success,20261020-143000,Mario,123456,card,AABBCC
    // ========================================================================

    static bool ParseRelayLog(
        const String& payload,
        AccessEvent& event)
    {
        const int colon =
            payload.indexOf(':');

        if (colon <= 0)
            return false;

        String doorPart =
            payload.substring(
                0,
                colon
            );

        doorPart.trim();

        if (doorPart.startsWith("door"))
        {
            event.door =
                doorPart.substring(4);
        }
        else
        {
            event.door =
                doorPart;
        }

        const String csv =
            payload.substring(
                colon + 1
            );

        event.result =
            AccessWebAPIUtils::GetCsvField(
                csv,
                0
            );

        event.time =
            AccessWebAPIUtils::GetCsvField(
                csv,
                1
            );

        event.personName =
            AccessWebAPIUtils::GetCsvField(
                csv,
                2
            );

        event.cardCode =
            AccessWebAPIUtils::GetCsvField(
                csv,
                3
            );

        event.cardType =
            AccessWebAPIUtils::GetCsvField(
                csv,
                4
            );

        event.mac =
            AccessWebAPIUtils::GetCsvField(
                csv,
                5
            );

        event.readerName =
            event.mac;

        event.valid =
            event.cardCode.length() > 0;

        return event.valid;
    }
};


// ============================================================================
// AxessTMC WEB API
//
// Implementazione non statica.
//
// Dipendenze:
//   • AccessControlService ricevuto nel costruttore
//
// Nessun singleton.
// Nessun AccessControlService globale.
// ============================================================================

class AxessTMCWebAPI :
    public IAccessWebAPI
{
public:
    // ============================================================================
    // AxessTMC TRANSACTION FIELD DEFINITION
    //
    // Associa un nome logico alla posizione del campo
    // nel record CSV inviato tramite $transaction$.
    // ============================================================================

    struct TransactionFieldDef
    {
        const char* name =
            nullptr;

        size_t index =
            0;
    };


    // ============================================================================
    // AxessTMC WEB API CONFIGURATION
    // ============================================================================

    struct WebAPIConfig
    {
        const char* transactionParameter =
            "trsn";

        const TransactionFieldDef*
            transactionFields =
                nullptr;

        size_t transactionFieldCount =
            0;

        /*
        * Campo dello schema da usare come codice personale
        * per AccessControlService.
        */
        const char* cardCodeField =
            "COD_UTENTE";
    };

    // ========================================================================
    // TRANSACTION
    // ========================================================================

    struct Transaction
    {
        String raw;

        String terminalId;
        String mac;

        String date;
        String time;

        int direction = -1;

        String reason;
        String cardCode;

        String controls;
        String result;
        String source;
        String edition;
        String gate;

        bool valid = false;
    };


private:

    // ========================================================================
    // ACCESS CONTROL SERVICE
    // ========================================================================

    AccessControlService& _accessControl;

    // ========================================================================
    // TRANSACTION SCHEMA
    // ========================================================================

    static const WebAPIConfig&
    Config()
    {
        static const TransactionFieldDef
            transactionFields[] =
            {
                {
                    "DATA",
                    0
                },
                {
                    "ORA",
                    1
                },
                {
                    "DIREZIONE",
                    2
                },
                {
                    "COD_CAUSALE",
                    3
                },
                {
                    "COD_UTENTE",
                    4
                },
                {
                    "CONTROLLI",
                    5
                },
                {
                    "ESITO",
                    6
                },
                {
                    "SORGENTE",
                    7
                },
                {
                    "EDIZIONE",
                    8
                },
                {
                    "VARCO",
                    9
                },
                {
                    "UTC",
                    10
                },
                {
                    "ORA_LEGALE",
                    11
                },
                {
                    "TRACCIATO_TESSERA",
                    12
                },
                {
                    "SLAVE_ID",
                    13
                },
                {
                    "LETTORE",
                    14
                },
                {
                    "TIPO",
                    15
                },
                {
                    "HASH_PAYLOAD",
                    16
                }
            };

        static const WebAPIConfig
            config =
            {
                .transactionParameter =
                    "trsn",

                .transactionFields =
                    transactionFields,

                .transactionFieldCount =
                    sizeof(transactionFields) /
                    sizeof(transactionFields[0]),

                .cardCodeField =
                    "COD_UTENTE"
            };

        return config;
    }
public:

    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    explicit AxessTMCWebAPI(
        AccessControlService& accessControl)
        : _accessControl(
              accessControl
          )
    {
    }


    // ========================================================================
    // COPY DISABLED
    // ========================================================================

    AxessTMCWebAPI(
        const AxessTMCWebAPI&) = delete;

    AxessTMCWebAPI& operator=(
        const AxessTMCWebAPI&) = delete;


    // ========================================================================
    // IDENTITY
    // ========================================================================

    const char* protocolId() const override
    {
        return "AxessTMC";
    }


    const char* protocolName() const override
    {
        return "AxessTMC Access Reader";
    }


    // ========================================================================
    // ACCESS READER DEFINITION
    // ========================================================================

    const AccessReaderDef&
    readerDefinition() const override
    {
        return ReaderDefinition();
    }


    static const AccessReaderDef&
    ReaderDefinition()
    {
        static const AccessReaderDef definition =
        {
            .id =
                "AxessTMC",

            .name =
                "AxessTMC Access Reader",

            .supportsServerPush =
                true,

            .supportsOnlineValidation =
                true,

            .supportsPolling =
                true,

            .supportsRemoteRelay =
                true,

            .defaultPort =
                80,

            .validateConfig =
                ValidateReaderConfig,

            .diagnostic =
                nullptr
        };

        return definition;
    }


    // ========================================================================
    // POLLING PROFILE
    // ========================================================================

    const DeviceMessageProfile*
    pollingProfile() const override
    {
        return &Profile();
    }


    static const DeviceMessageProfile&
    Profile()
    {
        static const DeviceMessageProfile::OutMessage
            outMessages[] =
            {
                {
                    "ONLINE",
                    "/online?trsn=${transaction}&id=${termid}",
                    "GET",
                    nullptr
                },
                {
                    "KEEPALIVE",
                    "/keepalive?id=${termid}",
                    "GET",
                    nullptr
                }
            };


        static const DeviceMessageProfile::InField
            inFields[] =
            {
                {
                    "screen",
                    "screen=(.*)"
                },
                {
                    "beep",
                    "beep=([0-9]+)"
                },
                {
                    "relay",
                    "relay=(.*)"
                },
                {
                    "time",
                    "time=([0-9]{6})"
                },
                {
                    "date",
                    "date=([0-9]{8})"
                },
                {
                    "keepaliveperiod",
                    "keepaliveperiod=([0-9]+)"
                },
                {
                    "cmd",
                    "cmd=(.*)"
                }
            };


        static const DeviceMessageProfile::StateRule
            stateRules[] =
            {
                {
                    "ONLINE",
                    "timeout",
                    "OFFLINE",
                    "KEEPALIVE"
                },
                {
                    "OFFLINE",
                    "keepalive_ok",
                    "ONLINE",
                    nullptr
                }
            };


        static const DeviceMessageProfile profile =
            {
                .profileId =
                    930,

                .name =
                    "X1X2_HTTP",

                .outMessages =
                    outMessages,

                .outCount =
                    sizeof(outMessages) /
                    sizeof(outMessages[0]),

                .inFields =
                    inFields,

                .inCount =
                    sizeof(inFields) /
                    sizeof(inFields[0]),

                .rules =
                    nullptr,

                .ruleCount =
                    0,

                .stateRules =
                    stateRules,

                .stateRuleCount =
                    sizeof(stateRules) /
                    sizeof(stateRules[0])
            };

        return profile;
    }


    // ========================================================================
    // SERVER ENDPOINTS
    //
    // Endpoint 0: keepalive
    // Endpoint 1: transazione online
    // ========================================================================

    enum EndpointIndex : size_t
    {
        ENDPOINT_KEEPALIVE =
            0,

        ENDPOINT_ONLINE =
            1,

        ENDPOINT_BATCH =
            2,

        ENDPOINT_COUNT =
            3
    };


    size_t endpointCount() const override
    {
        return
            ENDPOINT_COUNT;
    }


    const AccessWebAPIEndpointDef*
    endpointDefinition(
        size_t endpointIndex) const override
    {
        static const AccessWebAPIEndpointDef
            endpoints[] =
            {
                {
                    "X1X2KeepAlive",
                    "/keepalive",
                    "GET"
                },
                {
                    "X1X2Online",
                    "/online",
                    "GET"
                },
                {
                    "X1X2Batch",
                    "/batch",
                    "GET"
                }
            };

        if (
            endpointIndex >=
            ENDPOINT_COUNT
        )
        {
            return nullptr;
        }

        return
            &endpoints[
                endpointIndex
            ];
    }



    // ========================================================================
    // HTTP REQUEST
    // ========================================================================
    void handleRequest(
        size_t endpointIndex,
        const HttpServerRequest& request,
        HttpServerResponse& response) override
    {
        switch (endpointIndex)
        {
            // ====================================================
            // KEEPALIVE
            // ====================================================

            case ENDPOINT_KEEPALIVE:
            {
                HandleKeepAlive(
                    request,
                    response,
                    60
                );

                return;
            }

            // ====================================================
            // ONLINE TRANSACTION
            // ====================================================

            case ENDPOINT_ONLINE:
            {
                HandleOnlineTransaction(
                    request,
                    response
                );

                return;
            }

            // ====================================================
            // BATCH TRANSACTION
            // ====================================================

            case ENDPOINT_BATCH:
            {
                HandleBatchTransaction(
                    request,
                    response
                );

                return;
            }

            // ====================================================
            // UNKNOWN ENDPOINT
            // ====================================================

            default:
            {
                LOG_WF(
                    "AxessTMC",
                    "Unknown endpoint index: %u",
                    static_cast<unsigned>(
                        endpointIndex
                    )
                );

                AccessWebAPIUtils::BuildTextResponse(
                    response,
                    404,
                    "NOT FOUND"
                );

                return;
            }
        }
    }

    // ========================================================================
    // BATCH TRANSACTION
    //
    // Il terminale invia una transazione precedentemente memorizzata:
    //
    //   GET /batch?trsn=...&id=...&mac=...
    //
    // La risposta:
    //
    //   ack=1
    //
    // conferma che DomoManager ha ricevuto la transazione e autorizza
    // il terminale a eliminarla dalla propria memoria.
    //
    // Le transazioni batch sono storiche:
    //   - non viene ripetuta l'autorizzazione;
    //   - non viene eseguita la callback autorizzata;
    //   - non viene comandato alcun relè;
    //   - non viene generato alcun impulso.
    // ========================================================================

    void HandleBatchTransaction(
        const HttpServerRequest& request,
        HttpServerResponse& response)
    {
        // ====================================================
        // REQUEST PARAMETERS
        // ====================================================

        const String terminalId =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "id"
                )
            );

        const String readerAddress =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "mac"
                )
            );

        const String encodedTransaction =
            request.getQueryValue(
                Config().transactionParameter
            );

        const String transaction =
            AccessWebAPIUtils::UrlDecode(
                encodedTransaction
            );

        // ====================================================
        // OPTIONAL DIAGNOSTIC PARSING
        //
        // Il parsing serve esclusivamente per ottenere log
        // leggibili. L'ACK viene comunque restituito anche se
        // il tracciato storico non è completo.
        // ====================================================

        String date;
        String time;
        String cardCode;
        String gate;
        String result;

        if (transaction.length() > 0)
        {
            const WebAPIConfig& config =
                Config();

            AccessWebAPIUtils::GetTransactionField(
                config,
                transaction,
                "DATA",
                date
            );

            AccessWebAPIUtils::GetTransactionField(
                config,
                transaction,
                "ORA",
                time
            );

            AccessWebAPIUtils::GetTransactionField(
                config,
                transaction,
                config.cardCodeField,
                cardCode
            );

            AccessWebAPIUtils::GetTransactionField(
                config,
                transaction,
                "VARCO",
                gate
            );

            AccessWebAPIUtils::GetTransactionField(
                config,
                transaction,
                "ESITO",
                result
            );
        }

        // ====================================================
        // DIAGNOSTICS
        // ====================================================

        LOG_IF(
            "AxessTMC",
            "Batch received: "
            "terminal=%s address=%s "
            "card=%s date=%s time=%s "
            "gate=%s result=%s bytes=%u",
            terminalId.c_str(),
            readerAddress.c_str(),
            cardCode.c_str(),
            date.c_str(),
            time.c_str(),
            gate.c_str(),
            result.c_str(),
            static_cast<unsigned>(
                transaction.length()
            )
        );

        LOG_DF(
            "AxessTMC",
            "Batch payload: '%s'",
            transaction.c_str()
        );

        // ====================================================
        // ACKNOWLEDGEMENT
        //
        // La risposta deve essere immediata e minimale.
        // ====================================================

        AccessWebAPIUtils::BuildTextResponse(
            response,
            200,
            "ack=1\r\n"
        );

        LOG_IF(
            "AxessTMC",
            "Batch acknowledged: "
            "terminal=%s address=%s",
            terminalId.c_str(),
            readerAddress.c_str()
        );
    }

   void HandleOnlineTransaction(
        const HttpServerRequest& request,
        HttpServerResponse& response)
    {
        Transaction transaction;

        // ====================================================
        // TRANSACTION PARSING
        // ====================================================

        if (
            !ParseRequest(
                request,
                transaction
            )
        )
        {
            LOG_WF(
                "AxessTMC",
                "Invalid online request: query='%s'",
                request.query.c_str()
            );

            AccessWebAPIUtils::BuildTextResponse(
                response,
                400,
                "BAD REQUEST"
            );

            return;
        }

        // ====================================================
        // READER RESOLUTION
        //
        // transaction.mac viene utilizzato come readerAddress.
        //
        // Se nessun readerAddress è configurato per il
        // protocollo, ResolveIncomingReader() usa il fallback
        // legacy.
        //
        // Se almeno un readerAddress è configurato, il valore
        // ricevuto deve corrispondere a un lettore abilitato.
        // ====================================================

        const AccessReader* resolvedReader =
            nullptr;

        if (
            !_accessControl.ResolveIncomingReader(
                &readerDefinition(),
                transaction.terminalId,
                transaction.mac,
                resolvedReader
            )
        )
        {
            LOG_WF(
                "AxessTMC",
                "Rejected online request: "
                "terminal=%s readerAddress=%s card=%s",
                transaction.terminalId.c_str(),
                transaction.mac.c_str(),
                transaction.cardCode.c_str()
            );

            AccessWebAPIUtils::BuildTextResponse(
                response,
                400,
                "BAD REQUEST"
            );

            return;
        }

        // ====================================================
        // ACCESS REQUEST
        // ====================================================

        AccessControlService::Request
            accessRequest;

        accessRequest.readerDef =
            &readerDefinition();

        /*
        * Usa il nome logico configurato del lettore risolto.
        *
        * In assenza di indirizzi configurati può essere
        * utilizzato il terminalId come fallback.
        */
        if (
            resolvedReader &&
            resolvedReader->name
        )
        {
            accessRequest.readerName =
                resolvedReader->name;
        }
        else
        {
            accessRequest.readerName =
                transaction.terminalId;
        }

        /*
        * Può contenere un MAC oppure un IP, in base al
        * protocollo e alla configurazione del lettore.
        */
        accessRequest.readerAddress =
            transaction.mac;

        accessRequest.cardCode =
            transaction.cardCode;

        accessRequest.direction =
            String(
                transaction.direction
            );

        accessRequest.door =
            transaction.gate;

        accessRequest.timestamp =
            transaction.date +
            " " +
            transaction.time;

        accessRequest.rawPayload =
            transaction.raw;

        // ====================================================
        // ACCESS VALIDATION
        // ====================================================

        const AccessControlService::Decision
            decision =
                _accessControl.validate(
                    accessRequest
                );

        // ====================================================
        // DIAGNOSTICS
        // ====================================================

        LOG_DF(
            "AxessTMC",
            "Online transaction: "
            "terminal=%s reader=%s address=%s "
            "card=%s date=%s time=%s "
            "direction=%d gate=%s "
            "authorized=%u result=%s",
            transaction.terminalId.c_str(),
            (
                resolvedReader &&
                resolvedReader->name
            )
                ? resolvedReader->name
                : "",
            transaction.mac.c_str(),
            transaction.cardCode.c_str(),
            transaction.date.c_str(),
            transaction.time.c_str(),
            transaction.direction,
            transaction.gate.c_str(),
            decision.authorized
                ? 1U
                : 0U,
            AccessControlService::ResultToString(
                decision.result
            )
        );

        // ====================================================
        // ONLINE RESPONSE
        //
        // Autorizzato:
        //   beep=100
        //   relay=<index>,<time>
        //   save=0
        //
        // Negato:
        //   beep=3
        //   save=0
        // ====================================================

        BuildResponse(
            decision.authorized,
            decision.authorized
                ? "Accesso consentito"
                : "Accesso negato",
            decision.authorized
                ? 100
                : 3,
            decision.authorized
                ? decision.relayIndex
                : 0,
            decision.authorized
                ? decision.relayTime
                : 0,
            false,
            response
        );
    }


private:
    // ========================================================================
    // KEEP ALIVE
    // ========================================================================

    void HandleKeepAlive(
        const HttpServerRequest& request,
        HttpServerResponse& response,
        uint32_t keepAlivePeriodSeconds)
    {
        // ====================================================
        // REQUEST PARAMETERS
        // ====================================================

        const String terminalId =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "id"
                )
            );

        /*
        * Per AxessTMC il parametro mac viene utilizzato come
        * readerAddress.
        */
        const String readerAddress =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "mac"
                )
            );

        const String date =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "date"
                )
            );

        const String time =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "time"
                )
            );

        const String localTransactions =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "localtrsn"
                )
            );

        const String status =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "status"
                )
            );

        // ====================================================
        // READER RESOLUTION
        //
        // Se nessun readerAddress è configurato per il
        // protocollo, ResolveIncomingReader() usa il fallback
        // legacy.
        //
        // Se almeno un readerAddress è configurato, il valore
        // ricevuto deve corrispondere a un lettore abilitato.
        // ====================================================

        const AccessReader* resolvedReader =
            nullptr;

        if (
            !_accessControl.ResolveIncomingReader(
                &readerDefinition(),
                terminalId,
                readerAddress,
                resolvedReader
            )
        )
        {
            LOG_WF(
                "AxessTMC",
                "Rejected keepalive: "
                "terminal=%s readerAddress=%s",
                terminalId.c_str(),
                readerAddress.c_str()
            );

            AccessWebAPIUtils::BuildTextResponse(
                response,
                400,
                "BAD REQUEST"
            );

            return;
        }

        // ====================================================
        // KEEPALIVE PERIOD
        // ====================================================

        if (keepAlivePeriodSeconds == 0)
        {
            keepAlivePeriodSeconds =
                30;
        }

        // ====================================================
        // DIAGNOSTICS
        // ====================================================

        LOG_DF(
            "AxessTMC",
            "Keepalive: reader=%s id=%s "
            "address=%s date=%s time=%s "
            "localtrsn=%s status=%s period=%lu",
            (
                resolvedReader &&
                resolvedReader->name
            )
                ? resolvedReader->name
                : "",
            terminalId.c_str(),
            readerAddress.c_str(),
            date.c_str(),
            time.c_str(),
            localTransactions.c_str(),
            status.c_str(),
            static_cast<unsigned long>(
                keepAlivePeriodSeconds
            )
        );

        // ====================================================
        // RESPONSE BODY
        // ====================================================

        String body;

        body.reserve(
            80
        );

        if (date.length() == 8)
        {
            body +=
                "date=";

            body +=
                date;

            body +=
                "\r\n";
        }

        if (time.length() == 6)
        {
            body +=
                "time=";

            body +=
                time;

            body +=
                "\r\n";
        }

        body +=
            "keepaliveperiod=";

        body +=
            String(
                keepAlivePeriodSeconds
            );

        body +=
            "\r\n";

        body +=
            "save=0\r\n";

        // ====================================================
        // HTTP RESPONSE
        // ====================================================

        AccessWebAPIUtils::BuildTextResponse(
            response,
            200,
            body
        );

        LOG_DF(
            "AxessTMC",
            "Keepalive response: "
            "reader=%s id=%s address=%s "
            "period=%lu body='%s'",
            (
                resolvedReader &&
                resolvedReader->name
            )
                ? resolvedReader->name
                : "",
            terminalId.c_str(),
            readerAddress.c_str(),
            static_cast<unsigned long>(
                keepAlivePeriodSeconds
            ),
            response.body.c_str()
        );
    }

    // ========================================================================
    // READER CONFIG VALIDATION
    // ========================================================================

    static bool ValidateReaderConfig(
        const AccessReader& reader,
        String& error)
    {
        if (reader.doorIndex < 0)
        {
            error =
                "gate not configured";

            return false;
        }

        return true;
    }


    // ========================================================================
    // REQUEST PARSER
    // ========================================================================

    static bool ParseRequest(
        const HttpServerRequest& request,
        Transaction& transaction)
    {
        transaction =
            Transaction{};

        const WebAPIConfig& config =
            Config();

        const String encodedTransaction =
            request.getQueryValue(
                config.transactionParameter
            );

        if (encodedTransaction.length() == 0)
        {
            LOG_WF(
                "AxessTMC",
                "Missing transaction parameter '%s'",
                config.transactionParameter
            );

            return false;
        }

        transaction.raw =
            AccessWebAPIUtils::UrlDecode(
                encodedTransaction
            );

        transaction.terminalId =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "id"
                )
            );

        transaction.mac =
            AccessWebAPIUtils::UrlDecode(
                request.getQueryValue(
                    "mac"
                )
            );

        String direction;

        AccessWebAPIUtils::GetTransactionField(
            config,
            transaction.raw,
            "DATA",
            transaction.date
        );

        AccessWebAPIUtils::GetTransactionField(
            config,
            transaction.raw,
            "ORA",
            transaction.time
        );

        AccessWebAPIUtils::GetTransactionField(
            config,
            transaction.raw,
            "DIREZIONE",
            direction
        );

        AccessWebAPIUtils::GetTransactionField(
            config,
            transaction.raw,
            "COD_CAUSALE",
            transaction.reason
        );

        const bool cardFieldFound =
            AccessWebAPIUtils::GetTransactionField(
                config,
                transaction.raw,
                config.cardCodeField,
                transaction.cardCode
            );

        AccessWebAPIUtils::GetTransactionField(
            config,
            transaction.raw,
            "CONTROLLI",
            transaction.controls
        );

        AccessWebAPIUtils::GetTransactionField(
            config,
            transaction.raw,
            "ESITO",
            transaction.result
        );

        AccessWebAPIUtils::GetTransactionField(
            config,
            transaction.raw,
            "SORGENTE",
            transaction.source
        );

        AccessWebAPIUtils::GetTransactionField(
            config,
            transaction.raw,
            "EDIZIONE",
            transaction.edition
        );

        AccessWebAPIUtils::GetTransactionField(
            config,
            transaction.raw,
            "VARCO",
            transaction.gate
        );

        transaction.direction =
            direction.length() > 0
                ? direction.toInt()
                : -1;

        transaction.valid =
            cardFieldFound &&
            transaction.cardCode.length() > 0;

        return
            transaction.valid;
    }


    // ========================================================================
    // RESPONSE BUILDER
    // ========================================================================

    static void BuildResponse(
        bool authorized,
        const String& screenMessage,
        int beep,
        int relayIndex,
        int relayTime,
        bool save,
        HttpServerResponse& response)
    {
        response.statusCode =
            200;

        response.contentType =
            "text/plain";

        response.closeConnection =
            true;

        String body;

        body.reserve(128);

        if (screenMessage.length() > 0)
        {
            body += "screen=";

            body +=
                AccessWebAPIUtils::
                    SanitizeResponseValue(
                        screenMessage
                    );

            body += "\r\n";
        }

        if (authorized)
        {
            body += "beep=100\r\n";

            if (
                relayIndex > 0 &&
                relayTime > 0
            )
            {
                body += "relay=";

                body += String(
                    relayIndex
                );

                body += ",";

                body += String(
                    relayTime
                );

                body += "\r\n";
            }
        }
        else
        {
            body += "beep=";

            body += String(
                beep == 100
                    ? 0
                    : beep
            );

            body += "\r\n";
        }

        body += "save=";

        body +=
            save
                ? "1"
                : "0";

        body += "\r\n";

        response.body =
            body;

        LOG_DF(
            "AxessTMC",
            "HTTP response authorized=%u body='%s'",
            authorized
                ? 1U
                : 0U,
            response.body.c_str()
        );
    }
};

// ============================================================================
// REOLINK WEB API
//
// Profilo HTTP client per telecamere Reolink.
//
// La telecamera non implementa IAccessWebAPI perché:
//
//   - non è un lettore di controllo accessi;
//   - non espone endpoint verso DomoManager;
//   - viene interrogata periodicamente da DeviceMessageEngine.
//
// GetAiState restituisce lo stato dei rilevamenti:
//
//   bit 0 = persona
//   bit 1 = veicolo
//   bit 2 = animale
//
// Valori possibili:
//
//   0 = nessun allarme
//   1 = persona
//   2 = veicolo
//   3 = persona + veicolo
//   4 = animale
//   5 = persona + animale
//   6 = veicolo + animale
//   7 = persona + veicolo + animale
// ============================================================================

class ReolinkWebAPI
{
public:

    // ========================================================================
    // ALARM BITMASK
    // ========================================================================

    enum AlarmBit : uint8_t
    {
        ALARM_NONE =
            0,

        ALARM_PERSON =
            1U << 0,

        ALARM_VEHICLE =
            1U << 1,

        ALARM_ANIMAL =
            1U << 2
    };


    // ========================================================================
    // PROFILE ID
    // ========================================================================

    static constexpr int PROFILE_ID =
        3001;


    // ========================================================================
    // MESSAGE KEYS
    // ========================================================================

    static constexpr const char*
        MESSAGE_AI_STATE =
            "AI_STATE";


    // ========================================================================
    // DEVICE MESSAGE PROFILE
    // ========================================================================

    static const DeviceMessageProfile&
    Profile()
    {
        static const DeviceMessageProfile::OutMessage
            outMessages[] =
            {
                {
                    /*
                     * Stato rilevamenti AI.
                     *
                     * Risposta attesa:
                     *
                     * [
                     *   {
                     *     "cmd": "GetAiState",
                     *     "code": 0,
                     *     "value": {
                     *       "channel": 0,
                     *       "dog_cat": {
                     *         "alarm_state": 0,
                     *         "support": 1
                     *       },
                     *       "face": {
                     *         "alarm_state": 0,
                     *         "support": 0
                     *       },
                     *       "people": {
                     *         "alarm_state": 0,
                     *         "support": 1
                     *       },
                     *       "vehicle": {
                     *         "alarm_state": 0,
                     *         "support": 1
                     *       }
                     *     }
                     *   }
                     * ]
                     */
                    MESSAGE_AI_STATE,

                    "/api.cgi"
                    "?cmd=GetAiState"
                    "&user=${username}"
                    "&password=${password}",

                    "GET",

                    nullptr,

                    DeviceMessageProfile::
                        ResponseFormat::
                            JSON
                }
            };


        /*
         * Nessun InField:
         *
         * la risposta viene analizzata direttamente
         * come JSON.
         */
        static const DeviceMessageProfile
            profile =
            {
                .profileId =
                    PROFILE_ID,

                .name =
                    "Reolink_AI_Camera",

                .outMessages =
                    outMessages,

                .outCount =
                    sizeof(outMessages) /
                    sizeof(outMessages[0]),

                .inFields =
                    nullptr,

                .inCount =
                    0,

                .rules =
                    nullptr,

                .ruleCount =
                    0,

                /*
                 * Nessuna macchina a stati automatica.
                 *
                 * Un eventuale timeout non deve generare
                 * ulteriori comandi o polling supplementari.
                 */
                .stateRules =
                    nullptr,

                .stateRuleCount =
                    0
            };


        return
            profile;
    }


    // ========================================================================
    // PROFILE CHECK
    // ========================================================================

    static bool IsProfile(
        const DeviceMessageProfile* profile)
    {
        return
            profile &&
            profile->profileId ==
                PROFILE_ID;
    }


    // ========================================================================
    // MESSAGE CHECK
    // ========================================================================

    static bool IsAiStateMessage(
        const DeviceMessageProfile::OutMessage*
            message)
    {
        return
            message &&
            message->key &&
            strcmp(
                message->key,
                MESSAGE_AI_STATE
            ) == 0;
    }


    // ========================================================================
    // PARSE AI ALARM MASK
    //
    // Restituisce:
    //
    //   true
    //       Risposta valida. alarmMask contiene la bitmask.
    //
    //   false
    //       Risposta non valida oppure comando fallito.
    //
    // Sono considerati soltanto:
    //
    //   people
    //   vehicle
    //   dog_cat
    //
    // Il campo face viene ignorato.
    // ========================================================================

    static bool ParseAlarmMask(
        const JsonVariantConst& root,
        uint8_t& alarmMask,
        int& channel)
    {
        alarmMask =
            ALARM_NONE;

        channel =
            -1;


        // ====================================================
        // ROOT ARRAY
        // ====================================================

        if (
            !root.is<JsonArrayConst>()
        )
        {
            return false;
        }


        const JsonArrayConst responseArray =
            root.as<JsonArrayConst>();


        if (
            responseArray.size() == 0
        )
        {
            return false;
        }


        // ====================================================
        // RESPONSE OBJECT
        // ====================================================

        const JsonObjectConst responseObject =
            responseArray[0]
                .as<JsonObjectConst>();


        if (
            responseObject.isNull()
        )
        {
            return false;
        }


        // ====================================================
        // COMMAND
        // ====================================================

        const char* command =
            responseObject["cmd"] |
            "";


        if (
            strcmp(
                command,
                "GetAiState"
            ) != 0
        )
        {
            return false;
        }


        // ====================================================
        // RESPONSE CODE
        // ====================================================

        const int responseCode =
            responseObject["code"] |
            -1;


        if (responseCode != 0)
        {
            return false;
        }


        // ====================================================
        // VALUE OBJECT
        // ====================================================

        const JsonObjectConst value =
            responseObject["value"]
                .as<JsonObjectConst>();


        if (
            value.isNull()
        )
        {
            return false;
        }


        channel =
            value["channel"] |
            -1;


        // ====================================================
        // PERSON
        // ====================================================

        if (
            IsAlarmActive(
                value,
                "people"
            )
        )
        {
            alarmMask |=
                ALARM_PERSON;
        }


        // ====================================================
        // VEHICLE
        // ====================================================

        if (
            IsAlarmActive(
                value,
                "vehicle"
            )
        )
        {
            alarmMask |=
                ALARM_VEHICLE;
        }


        // ====================================================
        // ANIMAL
        // ====================================================

        if (
            IsAlarmActive(
                value,
                "dog_cat"
            )
        )
        {
            alarmMask |=
                ALARM_ANIMAL;
        }


        return true;
    }


    // ========================================================================
    // BIT TEST
    // ========================================================================

    static bool HasAlarm(
        uint8_t alarmMask,
        AlarmBit alarmBit)
    {
        return
            (
                alarmMask &
                static_cast<uint8_t>(
                    alarmBit
                )
            ) != 0;
    }


    // ========================================================================
    // ALARM HELPERS
    // ========================================================================

    static bool HasPerson(
        uint8_t alarmMask)
    {
        return
            HasAlarm(
                alarmMask,
                ALARM_PERSON
            );
    }


    static bool HasVehicle(
        uint8_t alarmMask)
    {
        return
            HasAlarm(
                alarmMask,
                ALARM_VEHICLE
            );
    }


    static bool HasAnimal(
        uint8_t alarmMask)
    {
        return
            HasAlarm(
                alarmMask,
                ALARM_ANIMAL
            );
    }


private:

    // ========================================================================
    // AI ALARM FIELD
    //
    // Il rilevamento viene considerato attivo soltanto quando:
    //
    //   support     != 0
    //   alarm_state != 0
    // ========================================================================

    static bool IsAlarmActive(
        const JsonObjectConst& value,
        const char* alarmName)
    {
        if (
            !alarmName ||
            alarmName[0] == '\0'
        )
        {
            return false;
        }


        const JsonObjectConst alarm =
            value[alarmName]
                .as<JsonObjectConst>();


        if (
            alarm.isNull()
        )
        {
            return false;
        }


        const bool supported =
            (
                alarm["support"] |
                0
            ) != 0;


        const bool active =
            (
                alarm["alarm_state"] |
                0
            ) != 0;


        return
            supported &&
            active;
    }
};

#endif