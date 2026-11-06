
#ifndef DMOnline_HPP
#define DMOnline_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it

   Versione modulo: 1.0.0
   Ultima modifica: 2026-10-28

   Note:
                    - Client applicativo DomoManager Online
                    - Profilo HTTP JSON integrato
                    - Risoluzione placeholder
                    - Download configurazione
                    - Upload configurazione
                    - ACK applicazione configurazione
                    - Nessuna dipendenza da AccessControlService

   ============================================================================ */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

#include "DMWebAPI.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


// ============================================================================
// DOMOMANAGER ONLINE CLIENT
//
// Responsabilità:
//
//   - definire il DeviceMessageProfile DomoManager Online;
//   - individuare il WebApiDevice associato;
//   - risolvere i placeholder negli URL;
//   - inviare richieste tramite DeviceMessageEngine;
//   - riconoscere e gestire le risposte del proprio profilo;
//   - notificare il consumer attraverso callback applicative.
//
// Non gestisce:
//
//   - il trasporto HTTP;
//   - la persistenza della configurazione;
//   - la costruzione di FrontendConfig;
//   - la costruzione di DomoManagerConfig;
//   - l'interpretazione della configurazione.
//
// Queste responsabilità verranno delegate successivamente a:
//
//   DMConfigInterpreter
//   DMConfigurationManager
//
// ============================================================================
class DomoManagerOnlineClient
{
public:

    // ========================================================================
    // MESSAGE KEYS
    // ========================================================================

    static constexpr const char* MESSAGE_CONFIG_VERSION =
        "CONFIG_VERSION";

    static constexpr const char* MESSAGE_CONFIG_MANIFEST =
        "CONFIG_MANIFEST";

    static constexpr const char* MESSAGE_CONFIG_FULL =
        "CONFIG_FULL";

    static constexpr const char* MESSAGE_CONFIG_SECTION =
        "CONFIG_SECTION";

    static constexpr const char* MESSAGE_CONFIG_JOBS =
        "CONFIG_JOBS";

    static constexpr const char* MESSAGE_CONFIG_ACK =
        "CONFIG_ACK";

    static constexpr const char* MESSAGE_CONFIG_UPLOAD =
        "CONFIG_UPLOAD";


    // ========================================================================
    // CONFIG
    // ========================================================================

    struct Config
    {
        /*
         * Abilita il client DomoManager Online.
         */
        bool enabled =
            false;

        /*
         * Nome del WebApiDevice presente in WEB_API_DEVICES.
         */
        const char* deviceName =
            "DomoManagerOnline";

        /*
         * Identificativo univoco del controller sul server.
         *
         * Esempio:
         *
         *   opta-domotica
         *   casa-andrea
         *   controller-001
         */
        const char* controllerId =
            nullptr;

        /*
         * Se true, il client può accettare una risposta JSON vuota
         * per le operazioni di scrittura.
         *
         * Per ora viene mantenuto false.
         */
        bool allowEmptyWriteResponse =
            false;
    };


    // ========================================================================
    // VERSION INFO
    // ========================================================================

    struct VersionInfo
    {
        uint32_t schemaVersion =
            0;

        uint32_t revision =
            0;

        String checksum;

        bool valid =
            false;
    };


    // ========================================================================
    // CALLBACK TYPES
    // ========================================================================

    using VersionCallback =
        std::function<void(
            const VersionInfo& version)>;

    using JsonCallback =
        std::function<void(
            const char* messageKey,
            const String& rawResponse,
            const JsonDocument& document)>;

    using ErrorCallback =
        std::function<void(
            const char* messageKey,
            const String& error)>;


private:

    // ========================================================================
    // RUNTIME
    // ========================================================================

    Config _config;

    DeviceMessageEngine* _engine =
        nullptr;

    const WebApiDevice* _device =
        nullptr;

    bool _initialized =
        false;

    /*
     * Sezione utilizzata per ${section}.
     *
     * Il relativo valore viene impostato prima di CONFIG_SECTION.
     */
    String _requestedSection;


    // ========================================================================
    // CALLBACKS
    // ========================================================================

    VersionCallback _onVersion;

    JsonCallback _onManifest;

    JsonCallback _onFullConfig;

    JsonCallback _onSection;

    JsonCallback _onJobs;

    JsonCallback _onAck;

    JsonCallback _onUpload;

    ErrorCallback _onError;


public:

    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    DomoManagerOnlineClient() =
        default;


    // ========================================================================
    // COPY DISABLED
    // ========================================================================

    DomoManagerOnlineClient(
        const DomoManagerOnlineClient&) = delete;

    DomoManagerOnlineClient& operator=(
        const DomoManagerOnlineClient&) = delete;


    // ========================================================================
    // PROFILE
    // ========================================================================

    static const DeviceMessageProfile&
    Profile()
    {
        static const DeviceMessageProfile::OutMessage
            outMessages[] =
            {
                {
                    MESSAGE_CONFIG_VERSION,

                    "/api/v1/controllers/"
                    "${controllerId}/config/version",

                    "GET",

                    nullptr,

                    DeviceMessageProfile::
                        ResponseFormat::JSON
                },

                {
                    MESSAGE_CONFIG_MANIFEST,

                    "/api/v1/controllers/"
                    "${controllerId}/config/manifest",

                    "GET",

                    nullptr,

                    DeviceMessageProfile::
                        ResponseFormat::JSON
                },

                {
                    MESSAGE_CONFIG_FULL,

                    "/api/v1/controllers/"
                    "${controllerId}/config/full",

                    "GET",

                    nullptr,

                    DeviceMessageProfile::
                        ResponseFormat::JSON
                },

                {
                    MESSAGE_CONFIG_SECTION,

                    "/api/v1/controllers/"
                    "${controllerId}/config/"
                    "${section}",

                    "GET",

                    nullptr,

                    DeviceMessageProfile::
                        ResponseFormat::JSON
                },

                {
                    MESSAGE_CONFIG_JOBS,

                    "/api/v1/controllers/"
                    "${controllerId}/config/jobs",

                    "GET",

                    nullptr,

                    DeviceMessageProfile::
                        ResponseFormat::JSON
                },

                {
                    MESSAGE_CONFIG_ACK,

                    "/api/v1/controllers/"
                    "${controllerId}/config/ack",

                    "POST",

                    nullptr,

                    DeviceMessageProfile::
                        ResponseFormat::JSON
                },

                {
                    MESSAGE_CONFIG_UPLOAD,

                    "/api/v1/controllers/"
                    "${controllerId}/config",

                    "PUT",

                    nullptr,

                    DeviceMessageProfile::
                        ResponseFormat::JSON
                }
            };


        static const DeviceMessageProfile
            profile =
            {
                .profileId =
                    5000,

                .name =
                    "DomoManager_Config_API",

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

                .stateRules =
                    nullptr,

                .stateRuleCount =
                    0
            };

        return profile;
    }


    // ========================================================================
    // BEGIN
    // ========================================================================

    bool Begin(
        DeviceMessageEngine& engine,
        const Config& config)
    {
        Reset();

        _engine =
            &engine;

        _config =
            config;

        // ====================================================
        // DISABLED
        // ====================================================

        if (!_config.enabled)
        {
            LOG_IF(
                "DOMO-ONLINE",
                "DomoManager Online client disabled"
            );

            return false;
        }

        // ====================================================
        // DEVICE NAME
        // ====================================================

        if (
            !_config.deviceName ||
            _config.deviceName[0] == '\0'
        )
        {
            return Fail(
                nullptr,
                "WebAPI device name not configured"
            );
        }

        // ====================================================
        // CONTROLLER ID
        // ====================================================

        if (
            !_config.controllerId ||
            _config.controllerId[0] == '\0'
        )
        {
            return Fail(
                nullptr,
                "Controller ID not configured"
            );
        }

        if (!ValidatePathSegment(
                _config.controllerId
            ))
        {
            return Fail(
                nullptr,
                "Invalid controller ID"
            );
        }

        // ====================================================
        // DEVICE LOOKUP
        // ====================================================

        _device =
            _engine->findDevice(
                _config.deviceName
            );

        if (!_device)
        {
            return Fail(
                nullptr,
                String(
                    "WebAPI device not found: "
                ) +
                _config.deviceName
            );
        }

        // ====================================================
        // PROFILE VALIDATION
        // ====================================================

        if (
            !_device->profile ||
            _device->profile->profileId !=
                Profile().profileId
        )
        {
            return Fail(
                nullptr,
                String(
                    "Invalid profile for device: "
                ) +
                _config.deviceName
            );
        }

        _initialized =
            true;

        LOG_IF(
            "DOMO-ONLINE",
            "Initialized: device=%s controllerId=%s",
            _config.deviceName,
            _config.controllerId
        );

        return true;
    }


    // ========================================================================
    // RESET
    // ========================================================================

    void Reset()
    {
        _engine =
            nullptr;

        _device =
            nullptr;

        _initialized =
            false;

        _requestedSection =
            "";

        _config =
            Config{};
    }


    // ========================================================================
    // STATUS
    // ========================================================================

    bool isInitialized() const
    {
        return
            _initialized &&
            _engine &&
            _device;
    }


    bool isEnabled() const
    {
        return
            _config.enabled;
    }


    bool isBusy() const
    {
        return
            _engine
                ? _engine->isBusy()
                : false;
    }


    const WebApiDevice*
    device() const
    {
        return _device;
    }


    const char*
    controllerId() const
    {
        return
            _config.controllerId
                ? _config.controllerId
                : "";
    }


    // ========================================================================
    // CALLBACK REGISTRATION
    // ========================================================================

    void SetVersionCallback(
        VersionCallback callback)
    {
        _onVersion =
            callback;
    }


    void SetManifestCallback(
        JsonCallback callback)
    {
        _onManifest =
            callback;
    }


    void SetFullConfigCallback(
        JsonCallback callback)
    {
        _onFullConfig =
            callback;
    }


    void SetSectionCallback(
        JsonCallback callback)
    {
        _onSection =
            callback;
    }


    void SetJobsCallback(
        JsonCallback callback)
    {
        _onJobs =
            callback;
    }


    void SetAckCallback(
        JsonCallback callback)
    {
        _onAck =
            callback;
    }


    void SetUploadCallback(
        JsonCallback callback)
    {
        _onUpload =
            callback;
    }


    void SetErrorCallback(
        ErrorCallback callback)
    {
        _onError =
            callback;
    }


    // ========================================================================
    // PLACEHOLDER RESOLVER
    //
    // Restituisce true solamente se:
    //
    //   - il device appartiene a DomoManager Online;
    //   - il placeholder è riconosciuto.
    //
    // Il parametro output riceve il valore risolto.
    // ========================================================================

    bool ResolvePlaceholder(
        const WebApiDevice& device,
        const char* variable,
        String& output) const
    {
        output =
            "";

        if (!OwnsDevice(device))
            return false;

        if (!variable)
        {
            LOG_WF(
                "DOMO-ONLINE",
                "Null placeholder"
            );

            return true;
        }

        // ====================================================
        // CONTROLLER ID
        // ====================================================

        if (
            strcmp(
                variable,
                "controllerId"
            ) == 0
        )
        {
            output =
                controllerId();

            return true;
        }

        // ====================================================
        // SECTION
        // ====================================================

        if (
            strcmp(
                variable,
                "section"
            ) == 0
        )
        {
            output =
                _requestedSection;

            if (output.length() == 0)
            {
                LOG_EF(
                    "DOMO-ONLINE",
                    "Section placeholder requested "
                    "without configured section"
                );
            }

            return true;
        }

        LOG_WF(
            "DOMO-ONLINE",
            "Unknown placeholder: %s",
            variable
        );

        /*
         * Il device appartiene a questo client.
         * Il placeholder è però sconosciuto.
         */
        return true;
    }


    // ========================================================================
    // REQUEST VERSION
    // ========================================================================

    bool RequestVersion()
    {
        return SendMessage(
            MESSAGE_CONFIG_VERSION
        );
    }


    // ========================================================================
    // REQUEST MANIFEST
    // ========================================================================

    bool RequestManifest()
    {
        return SendMessage(
            MESSAGE_CONFIG_MANIFEST
        );
    }


    // ========================================================================
    // REQUEST FULL CONFIG
    // ========================================================================

    bool RequestFullConfig()
    {
        return SendMessage(
            MESSAGE_CONFIG_FULL
        );
    }


    // ========================================================================
    // REQUEST SECTION
    //
    // Per garantire la coerenza di ${section}, la richiesta viene rifiutata
    // se il DeviceMessageEngine sta già elaborando o accodando richieste.
    //
    // Questo evita che _requestedSection venga modificata prima della
    // costruzione effettiva dell'URL.
    // ========================================================================

    bool RequestSection(
        const char* section)
    {
        if (!EnsureReady(
                MESSAGE_CONFIG_SECTION
            ))
        {
            return false;
        }

        if (!section)
        {
            return Fail(
                MESSAGE_CONFIG_SECTION,
                "Null configuration section"
            );
        }

        String normalized =
            section;

        normalized.trim();

        if (
            normalized.length() == 0 ||
            !ValidatePathSegment(
                normalized.c_str()
            )
        )
        {
            return Fail(
                MESSAGE_CONFIG_SECTION,
                "Invalid configuration section"
            );
        }

        if (_engine->isBusy())
        {
            return Fail(
                MESSAGE_CONFIG_SECTION,
                "WebAPI engine busy"
            );
        }

        _requestedSection =
            normalized;

        const bool queued =
            _engine->Send(
                _device,
                MESSAGE_CONFIG_SECTION
            );

        if (!queued)
        {
            _requestedSection =
                "";

            return Fail(
                MESSAGE_CONFIG_SECTION,
                "Unable to queue section request"
            );
        }

        LOG_IF(
            "DOMO-ONLINE",
            "Requested configuration section=%s",
            _requestedSection.c_str()
        );

        return true;
    }


    // ========================================================================
    // REQUEST JOBS
    // ========================================================================

    bool RequestJobs()
    {
        return SendMessage(
            MESSAGE_CONFIG_JOBS
        );
    }


    // ========================================================================
    // SEND ACK
    // ========================================================================

    bool SendAck(
        uint32_t revision,
        const char* status,
        bool rebootRequired,
        const char* detail = nullptr)
    {
        if (!status)
        {
            return Fail(
                MESSAGE_CONFIG_ACK,
                "ACK status not configured"
            );
        }

        DynamicJsonDocument document(
            512
        );

        document["revision"] =
            revision;

        document["status"] =
            status;

        document["rebootRequired"] =
            rebootRequired;

        if (
            detail &&
            detail[0] != '\0'
        )
        {
            document["detail"] =
                detail;
        }

        return SendJson(
            MESSAGE_CONFIG_ACK,
            document
        );
    }


    // ========================================================================
    // UPLOAD CONFIG DOCUMENT
    // ========================================================================

    bool UploadConfig(
        const JsonDocument& document)
    {
        return SendJson(
            MESSAGE_CONFIG_UPLOAD,
            document
        );
    }


    // ========================================================================
    // UPLOAD RAW JSON
    //
    // Il JSON viene validato localmente prima dell'accodamento.
    // ========================================================================

    bool UploadConfig(
        const String& json)
    {
        if (json.length() == 0)
        {
            return Fail(
                MESSAGE_CONFIG_UPLOAD,
                "Empty configuration payload"
            );
        }

        DynamicJsonDocument validationDocument(
            json.length() + 512
        );

        const DeserializationError error =
            deserializeJson(
                validationDocument,
                json
            );

        if (error)
        {
            return Fail(
                MESSAGE_CONFIG_UPLOAD,
                String(
                    "Invalid upload JSON: "
                ) +
                error.c_str()
            );
        }

        return SendMessage(
            MESSAGE_CONFIG_UPLOAD,
            json
        );
    }


    // ========================================================================
    // HANDLE RESPONSE
    //
    // Restituisce:
    //
    //   true:
    //     la risposta appartiene a DomoManager Online;
    //
    //   false:
    //     la risposta appartiene a un altro profilo WebAPI.
    //
    // Questa funzione viene invocata dalla callback comune del
    // DeviceMessageEngine.
    // ========================================================================

    bool HandleResponse(
        const WebApiDevice& device,
        const DeviceMessageProfile::OutMessage& message,
        const String& rawResponse,
        const ParsedResponse& parsed)
    {
        if (!OwnsDevice(device))
            return false;

        const char* messageKey =
            message.key
                ? message.key
                : "";

        if (!parsed.jsonValid)
        {
            /*
             * In questa implementazione tutte le risposte del
             * profilo DomoManager Online devono essere JSON.
             */
            Fail(
                messageKey,
                "Invalid JSON response"
            );

            ClearRequestState(
                messageKey
            );

            return true;
        }

        LOG_IF(
            "DOMO-ONLINE",
            "Response: message=%s bytes=%u",
            messageKey,
            static_cast<unsigned>(
                rawResponse.length()
            )
        );

        // ====================================================
        // CONFIG VERSION
        // ====================================================

        if (
            strcmp(
                messageKey,
                MESSAGE_CONFIG_VERSION
            ) == 0
        )
        {
            VersionInfo version;

            version.schemaVersion =
                parsed.json["schemaVersion"] | 0U;

            version.revision =
                parsed.json["revision"] | 0U;

            version.checksum =
                parsed.json["checksum"] |
                "";

            version.valid =
                version.schemaVersion > 0 &&
                version.revision > 0;

            if (!version.valid)
            {
                Fail(
                    messageKey,
                    "Invalid version response"
                );

                return true;
            }

            LOG_IF(
                "DOMO-ONLINE",
                "Remote version: schema=%lu "
                "revision=%lu checksum=%s",
                static_cast<unsigned long>(
                    version.schemaVersion
                ),
                static_cast<unsigned long>(
                    version.revision
                ),
                version.checksum.c_str()
            );

            if (_onVersion)
            {
                _onVersion(
                    version
                );
            }

            return true;
        }

        // ====================================================
        // CONFIG MANIFEST
        // ====================================================

        if (
            strcmp(
                messageKey,
                MESSAGE_CONFIG_MANIFEST
            ) == 0
        )
        {
            InvokeJsonCallback(
                _onManifest,
                messageKey,
                rawResponse,
                parsed.json
            );

            return true;
        }

        // ====================================================
        // FULL CONFIG
        // ====================================================

        if (
            strcmp(
                messageKey,
                MESSAGE_CONFIG_FULL
            ) == 0
        )
        {
            InvokeJsonCallback(
                _onFullConfig,
                messageKey,
                rawResponse,
                parsed.json
            );

            return true;
        }

        // ====================================================
        // CONFIG SECTION
        // ====================================================

        if (
            strcmp(
                messageKey,
                MESSAGE_CONFIG_SECTION
            ) == 0
        )
        {
            InvokeJsonCallback(
                _onSection,
                messageKey,
                rawResponse,
                parsed.json
            );

            ClearRequestState(
                messageKey
            );

            return true;
        }

        // ====================================================
        // JOBS
        // ====================================================

        if (
            strcmp(
                messageKey,
                MESSAGE_CONFIG_JOBS
            ) == 0
        )
        {
            InvokeJsonCallback(
                _onJobs,
                messageKey,
                rawResponse,
                parsed.json
            );

            return true;
        }

        // ====================================================
        // ACK
        // ====================================================

        if (
            strcmp(
                messageKey,
                MESSAGE_CONFIG_ACK
            ) == 0
        )
        {
            InvokeJsonCallback(
                _onAck,
                messageKey,
                rawResponse,
                parsed.json
            );

            return true;
        }

        // ====================================================
        // UPLOAD
        // ====================================================

        if (
            strcmp(
                messageKey,
                MESSAGE_CONFIG_UPLOAD
            ) == 0
        )
        {
            InvokeJsonCallback(
                _onUpload,
                messageKey,
                rawResponse,
                parsed.json
            );

            return true;
        }

        Fail(
            messageKey,
            "Unknown DomoManager Online response"
        );

        return true;
    }


private:

    // ========================================================================
    // DEVICE OWNERSHIP
    // ========================================================================

    bool OwnsDevice(
        const WebApiDevice& device) const
    {
        if (
            !_initialized ||
            !_device
        )
        {
            return false;
        }

        if (&device != _device)
            return false;

        if (!device.profile)
            return false;

        return
            device.profile->profileId ==
            Profile().profileId;
    }


    // ========================================================================
    // ENSURE READY
    // ========================================================================

    bool EnsureReady(
        const char* messageKey)
    {
        if (!_config.enabled)
        {
            return Fail(
                messageKey,
                "DomoManager Online disabled"
            );
        }

        if (
            !_initialized ||
            !_engine ||
            !_device
        )
        {
            return Fail(
                messageKey,
                "DomoManager Online not initialized"
            );
        }

        return true;
    }


    // ========================================================================
    // SEND MESSAGE
    // ========================================================================

    bool SendMessage(
        const char* messageKey,
        const String& body = "")
    {
        if (!EnsureReady(
                messageKey
            ))
        {
            return false;
        }

        const bool queued =
            _engine->Send(
                _device,
                messageKey,
                body
            );

        if (!queued)
        {
            return Fail(
                messageKey,
                "Unable to queue WebAPI request"
            );
        }

        LOG_IF(
            "DOMO-ONLINE",
            "Queued request: %s",
            messageKey
                ? messageKey
                : ""
        );

        return true;
    }


    // ========================================================================
    // SEND JSON
    // ========================================================================

    bool SendJson(
        const char* messageKey,
        const JsonDocument& document)
    {
        String body;

        if (
            serializeJson(
                document,
                body
            ) == 0
        )
        {
            return Fail(
                messageKey,
                "Unable to serialize JSON payload"
            );
        }

        return SendMessage(
            messageKey,
            body
        );
    }


    // ========================================================================
    // VALIDATE PATH SEGMENT
    // ========================================================================

    static bool ValidatePathSegment(
        const char* value)
    {
        if (
            !value ||
            value[0] == '\0'
        )
        {
            return false;
        }

        for (
            const char* current = value;
            *current != '\0';
            ++current
        )
        {
            const char c =
                *current;

            const bool valid =
                isalnum(
                    static_cast<unsigned char>(c)
                ) ||
                c == '-' ||
                c == '_' ||
                c == '.';

            if (!valid)
                return false;
        }

        return true;
    }


    // ========================================================================
    // CLEAR REQUEST STATE
    // ========================================================================

    void ClearRequestState(
        const char* messageKey)
    {
        if (
            messageKey &&
            strcmp(
                messageKey,
                MESSAGE_CONFIG_SECTION
            ) == 0
        )
        {
            _requestedSection =
                "";
        }
    }


    // ========================================================================
    // INVOKE JSON CALLBACK
    // ========================================================================

    static void InvokeJsonCallback(
        const JsonCallback& callback,
        const char* messageKey,
        const String& rawResponse,
        const JsonDocument& document)
    {
        if (!callback)
            return;

        callback(
            messageKey,
            rawResponse,
            document
        );
    }


    // ========================================================================
    // FAILURE
    // ========================================================================

    bool Fail(
        const char* messageKey,
        const String& error)
    {
        LOG_EF(
            "DOMO-ONLINE",
            "message=%s error=%s",
            messageKey
                ? messageKey
                : "",
            error.c_str()
        );

        if (_onError)
        {
            _onError(
                messageKey,
                error
            );
        }

        return false;
    }
};

#endif