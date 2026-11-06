
#ifndef DMFrontendOrchestrators_HPP
#define DMFrontendOrchestrators_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑05‑24
   Note:
                    • Nessuna

   ============================================================================ */


#include <Arduino.h>

#include "DMSetup.hpp"
#include "DMDeclares.h"
#include "DMIntrospection.hpp"
#include "DMHomeAssistant.hpp"
#include "DMSolarInverter.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================================
// PULSE MANAGER
//
// Adattatore generico tra:
//
//   PulseOutputManager
//       e
//   DomoManager::forceEvent()
//
// Ogni impulso è configurato con:
//
//   - area di uscita
//   - durata in millisecondi
//   - sorgente evento
//
// Non usa delay().
//
// Se Trigger() viene richiamato mentre l'impulso è attivo,
// il timeout viene riavviato.
// ============================================================================
class PulseManager
{
private:

    // ========================================================================
    // PULSE CALLBACK
    //
    // active = true:
    //      attiva l'area configurata.
    //
    // active = false:
    //      disattiva l'area configurata.
    // ========================================================================

    static void OnPulseChanged(
        const PulseOutputManager::Pulse& pulse,
        bool active)
    {
        if (!DomoManager::instance)
        {
            LOG_EF(
                "PULSE",
                "DomoManager unavailable"
            );

            return;
        }

        const long outputValue =
            active
                ? 1L
                : 0L;

        DomoManager::instance->forceInternalEvent(
            pulse.area,
            outputValue
        );

        LOG_IF(
            "PULSE",
            "Pulse %s: area=%d value=%ld duration=%lu",
            active
                ? "START"
                : "STOP",
            pulse.area,
            outputValue,
            pulse.pulseTime
        );
    }


    // ========================================================================
    // INTERNAL MANAGER
    // ========================================================================

    static PulseOutputManager& Manager()
    {
        static PulseOutputManager
            manager(
                OnPulseChanged
            );

        return manager;
    }


public:

    // ========================================================================
    // ADD
    //
    // Registra un'area come uscita impulsiva.
    //
    // outputArea:
    //   area buffer pilotata.
    //
    // pulseTimeMs:
    //   durata dell'impulso.
    //
    // source:
    //   sorgente passata a forceEvent().
    // ========================================================================

    static bool Add(
        int outputArea,
        unsigned long pulseTimeMs)
    {
        return Manager().add(
            outputArea,
            pulseTimeMs
        );
    }


    // ========================================================================
    // TRIGGER
    //
    // Avvia l'impulso usando millis().
    //
    // Se l'impulso è già attivo, riavvia il timeout.
    // ========================================================================

    static bool Trigger(
        int outputArea)
    {
        const bool started =
            Manager().start(
                outputArea
            );

        if (!started)
        {
            LOG_WF(
                "PULSE",
                "Unable to trigger output: area=%d",
                outputArea
            );

            return false;
        }

        return true;
    }


    // ========================================================================
    // TRIGGER WITH TIMESTAMP
    // ========================================================================

    static bool Trigger(
        int outputArea,
        unsigned long now)
    {
        const bool started =
            Manager().start(
                outputArea,
                now
            );

        if (!started)
        {
            LOG_WF(
                "PULSE",
                "Unable to trigger output: "
                "area=%d now=%lu",
                outputArea,
                now
            );

            return false;
        }

        return true;
    }


    // ========================================================================
    // STOP
    //
    // Termina anticipatamente l'impulso e porta l'area a zero.
    // ========================================================================

    static bool Stop(
        int outputArea)
    {
        const bool stopped =
            Manager().stop(
                outputArea
            );

        if (!stopped)
        {
            LOG_WF(
                "PULSE",
                "Unable to stop output: area=%d",
                outputArea
            );

            return false;
        }

        return true;
    }


    // ========================================================================
    // EXISTS
    // ========================================================================

    static bool Exists(
        int outputArea)
    {
        return
            Manager().exists(
                outputArea
            );
    }


    // ========================================================================
    // STATUS
    // ========================================================================

    static bool IsRunning(
        int outputArea)
    {
        return
            Manager().isRunning(
                outputArea
            );
    }


    static bool HasActivePulses()
    {
        return
            Manager().hasActivePulses();
    }


    // ========================================================================
    // LOOP
    //
    // Deve essere chiamato frequentemente dal loop principale.
    // ========================================================================

    static void Loop(
        unsigned long now)
    {
        if (
            !Manager()
                .hasActivePulses()
        )
        {
            return;
        }

        Manager().update(
            now
        );
    }


    static void Loop()
    {
        Loop(
            millis()
        );
    }
};

/* ============================================================================
   
   Descrizione:
                    Astrazione minimale del trasporto MQTT utilizzata
                    dal sottosistema Home Assistant.

                    Il modulo fornisce un wrapper indipendente
                    dall'implementazione MQTT sottostante e permette
                    al runtime Home Assistant di operare senza
                    dipendere direttamente da MQTTEngine.

                    Le operazioni supportate sono:

                      • Verifica stato connessione
                      • Pubblicazione messaggi MQTT
                      • Sottoscrizione topic MQTT

   Obiettivi:
                    • Isolare il layer Home Assistant dal layer MQTT
                    • Consentire integrazioni MQTT differenti
                    • Ridurre l'accoppiamento tra moduli
                    • Facilitare test e simulazioni

   Architettura:
                    DomoManager
                           │
                           ▼
                    DMHAOrchestrator
                           │
                           ▼
                    HAMQTTTransport
                           │
                           ▼
                       MQTTEngine

   Dipendenze:
                    • Nessuna dipendenza diretta da MQTTEngine
                    • Nessuna dipendenza da Home Assistant
                    • Compatibile con qualsiasi broker MQTT

   Note:
                    • Struttura POD lightweight
                    • Nessuna allocazione dinamica
                    • Nessuna ownership delle callback
                    • Thread-safe finché il layer MQTT sottostante
                      è thread-safe
                    • Utilizzabile come adapter verso qualsiasi
                      implementazione MQTT

   ============================================================================
 */
struct HAMQTTTransport
{
    using ConnectedCallback = bool (*)(void* context);

    using PublishCallback = bool (*)(
        void* context,
        const char* topic,
        const char* payload,
        bool retain
    );

    using SubscribeCallback = bool (*)(
        void* context,
        const char* topic
    );

    void* context = nullptr;
    ConnectedCallback connected = nullptr;
    PublishCallback publish = nullptr;
    SubscribeCallback subscribe = nullptr;

    bool valid() const
    {
        return
            connected != nullptr &&
            publish != nullptr &&
            subscribe != nullptr;
    }

    bool isConnected() const
    {
        return
            connected &&
            connected(context);
    }

    bool publishMessage(
        const char* topic,
        const char* payload,
        bool retain = false) const
    {
        if (!publish ||
            !topic || topic[0] == '\0' ||
            !payload)
        {
            return false;
        }

        return publish(
            context,
            topic,
            payload,
            retain
        );
    }

    bool subscribeTopic(const char* topic) const
    {
        if (!subscribe ||
            !topic || topic[0] == '\0')
        {
            return false;
        }

        return subscribe(
            context,
            topic
        );
    }
};

// ============================================================
// HOME ASSISTANT ORCHESTRATOR
// ============================================================

class DMHAOrchestrator
{
public:
    // ========================================================================
    // MQTT INBOUND CALLBACK
    //
    // Notifica al frontend il messaggio MQTT già normalizzato.
    //
    // Utilizzato per:
    //
    //   - HOME_ASSISTANT_AREA;
    //   - HOME_ASSISTANT_RUNTIME.
    // ========================================================================

   using InboundMessageCallback =
    void (*)(
        const MQTTInboundMessage& message
    );

    // ========================================================================
    // AREA COMMAND CALLBACK
    //
    // Applica al frontend i comandi numerici destinati alle aree Buffer.
    // ========================================================================

    using CommandCallback = void (*)(
        void* context,
        uint8_t originClientIndex,
        int area,
        long value
    );

    struct Config
    {
        HAMQTTTransport transport;

        const FrontendConfig::MQTT::Client*
            mqttClientConfig =
                nullptr;

        DomoManager*
            manager =
                nullptr;

        const DomoIntrospection*
            introspection =
                nullptr;

        void*
            commandContext =
                nullptr;

        CommandCallback
            commandCallback =
                nullptr;

        InboundMessageCallback
            inboundMessageCallback =
                nullptr;
    };

private:
    static constexpr uint16_t MAX_PENDING_STATES = 128;

    struct PendingState
    {
        int area = -1;
        long value = 0;
        bool used = false;
    };

    // ========================================================================
    // PARSED COMMAND
    //
    // Rappresentazione normalizzata di un comando ricevuto da Home Assistant.
    //
    // AREA:
    //   domomanager/area/<area>/set
    //
    // RUNTIME:
    //   domomanager/runtime/<id>/set
    // ========================================================================

    struct ParsedCommand
    {
        enum class Target : uint8_t
        {
            INVALID = 0,
            AREA,
            RUNTIME
        };

        Target target =
            Target::INVALID;

        uint8_t clientIndex =
            0;

        int area =
            -1;

        DMRuntimeStore::Handle runtimeHandle =
            DMRuntimeStore::INVALID_HANDLE;

        const DMRuntimeStore::Entry* runtimeEntry =
            nullptr;

        const char* topic =
            nullptr;

        const char* payload =
            nullptr;

        size_t payloadLength =
            0;

        bool valid() const
        {
            switch (target)
            {
                case Target::AREA:
                    return area >= 0;

                case Target::RUNTIME:
                    return
                        runtimeHandle !=
                            DMRuntimeStore::INVALID_HANDLE &&
                        runtimeEntry != nullptr;

                case Target::INVALID:
                default:
                    return false;
            }
        }
    };

    // ========================================================================
    // COMMAND TOPICS
    //
    // Raggruppa i topic MQTT utilizzati per ricevere i comandi Home Assistant.
    //
    // area:
    //   domomanager/area/+/set
    //
    // runtime:
    //   domomanager/runtime/+/set
    // ========================================================================

    struct CommandTopics
    {
        String area;
        String runtime;

        void clear()
        {
            area = "";
            runtime = "";
        }

        bool valid() const
        {
            return
                area.length() > 0 &&
                runtime.length() > 0;
        }
    };

    Config _config;

    DMHomeAssistant _homeAssistant;

    bool _initialized = false;
    bool _wasConnected = false;
    bool _availabilityPublished = false;
    bool _discoveryPublished = false;
    bool _initialStatesPublished = false;

    CommandTopics _commandTopics;

    PendingState _pendingStates[MAX_PENDING_STATES];
    uint16_t _pendingStateCount = 0;

public:
    DMHAOrchestrator() = default;

    DMHAOrchestrator(const DMHAOrchestrator&) = delete;
    DMHAOrchestrator& operator=(const DMHAOrchestrator&) = delete;

    bool setup(const Config& config)
    {
        reset();

        _config = config;

        if (!_config.transport.valid())
        {
            LOG_EF(
                "HA_ORCHESTRATOR",
                "Setup fallito: transport non valido"
            );
            return false;
        }

        if (!_config.mqttClientConfig)
        {
            LOG_EF(
                "HA_ORCHESTRATOR",
                "Setup fallito: mqttClientConfig NULL"
            );
            return false;
        }

        if (!_config.manager)
        {
            LOG_EF(
                "HA_ORCHESTRATOR",
                "Setup fallito: manager NULL"
            );
            return false;
        }

        if (!_config.introspection)
        {
            LOG_EF(
                "HA_ORCHESTRATOR",
                "Setup fallito: introspection NULL"
            );
            return false;
        }

        const DMHomeAssistant::Config discoveryConfig =
            DMHomeAssistant::MakeConfig(
                *_config.mqttClientConfig
            );

        _homeAssistant.setPublisher(
            DiscoveryPublishCallback,
            this
        );

        _homeAssistant.setConfig(
            discoveryConfig
        );

        _commandTopics.area =
            _homeAssistant.commandSubscriptionTopic();

        _commandTopics.runtime =
            _homeAssistant.runtimeCommandSubscriptionTopic();

        if (!_commandTopics.valid())
        {
            LOG_EF(
                "HA_ORCHESTRATOR",
                "Setup fallito: command topics non validi"
            );

            return false;
        }

        clearPendingStates();
        resetConnectionState();
        _initialized = true;

        LOG_IF(
            "HA_ORCHESTRATOR",
            "Setup completato: entities=%u areaTopic=%s runtimeTopic=%s",
            static_cast<unsigned>(
                _config.introspection->entityCount()
            ),
            _commandTopics.area.c_str(),
            _commandTopics.runtime.c_str()
        );

        return true;
    }

    void reset()
    {
        _config = Config{};
        _initialized = false;
        _commandTopics.clear();

        resetConnectionState();
        clearPendingStates();
    }

    bool initialized() const
    {
        return _initialized;
    }

    bool connected() const
    {
        return
            _initialized &&
            _config.transport.isConnected();
    }

    void loop(unsigned long now)
    {
        (void)now;

        if (!_initialized ||
            !_config.introspection)
        {
            return;
        }

        if (!ensureConnected())
            return;

        if (!ensureSubscription())
            return;

        if (!ensureAvailability())
            return;

        if (!ensureDiscovery())
            return;

        publishInitialSnapshot();
        publishPendingStates();
    }


    void publishEvent(
        int area,
        long value)
    {
        if (!_initialized ||
            area < 0)
        {
            return;
        }

        enqueueEntityState(
            area,
            value
        );
    }

    void onRawMessage(
        uint8_t clientIndex,
        const char* topic,
        const char* payload,
        size_t length)
    {
        if (!_initialized)
            return;

        processCommand(
            clientIndex,
            topic,
            payload,
            length
        );
    }

private:
       static bool DiscoveryPublishCallback(
        void* context,
        const char* topic,
        const char* payload,
        bool retain)
    {
        DMHAOrchestrator* self =
            static_cast<DMHAOrchestrator*>(context);

        if (!self)
            return false;

        return self->_config.transport.publishMessage(
            topic,
            payload,
            retain
        );
    }

    void resetConnectionState()
    {
        _wasConnected = false;
        _availabilityPublished = false;
        _discoveryPublished = false;
        _initialStatesPublished = false;
    }

    bool ensureDiscovery()
    {
        if (_discoveryPublished)
        {
            return true;
        }

        if (!_config.introspection)
        {
            return false;
        }

        const DMHomeAssistant::Result result =
            _homeAssistant.publishAll(
                *_config.introspection
            );

        LOG_IF(
            "HA_ORCHESTRATOR",
            "Discovery: examined=%u published=%u "
            "hidden=%u unknown=%u status=%u "
            "disabled=%u failed=%u",
            static_cast<unsigned>(
                result.examined
            ),
            static_cast<unsigned>(
                result.published
            ),
            static_cast<unsigned>(
                result.skippedHidden
            ),
            static_cast<unsigned>(
                result.skippedUnknown
            ),
            static_cast<unsigned>(
                result.skippedStatus
            ),
            static_cast<unsigned>(
                result.skippedDisabled
            ),
            static_cast<unsigned>(
                result.failed
            )
        );

        if (!result.ok())
        {
            return false;
        }

        _discoveryPublished =
            true;

        return true;
    }

    bool ensureAvailability()
    {
        if (_availabilityPublished)
        {
            return true;
        }

        if (!_homeAssistant.publishAvailability(
                true))
        {
            return false;
        }

        _availabilityPublished = true;

        LOG_IF(
            "HA_ORCHESTRATOR",
            "Availability pubblicata: online"
        );

        return true;
    }

    bool ensureSubscription()
    {
        if (_wasConnected)
            return true;

        _availabilityPublished =
            false;

        _discoveryPublished =
            false;

        _initialStatesPublished =
            false;

        // ========================================================
        // BUFFER AREA COMMANDS
        // ========================================================

        if (!_config.transport.subscribeTopic(
                _commandTopics.area.c_str()))
        {
            LOG_EF(
                "HA_ORCHESTRATOR",
                "Subscription area fallita: %s",
                _commandTopics.area.c_str()
            );

            return false;
        }

        LOG_IF(
            "HA_ORCHESTRATOR",
            "Subscription area completata: %s",
            _commandTopics.area.c_str()
        );

        // ========================================================
        // RUNTIME STORE COMMANDS
        // ========================================================

        if (!_config.transport.subscribeTopic(
                _commandTopics.runtime.c_str()))
        {
            LOG_EF(
                "HA_ORCHESTRATOR",
                "Subscription runtime fallita: %s",
                _commandTopics.runtime.c_str()
            );

            return false;
        }

        LOG_IF(
            "HA_ORCHESTRATOR",
            "Subscription runtime completata: %s",
            _commandTopics.runtime.c_str()
        );

        _wasConnected =
            true;

        return true;
    }


    bool ensureConnected()
    {
        const bool mqttConnected =
            _config.transport.isConnected();

        if (mqttConnected)
        {
            return true;
        }

        if (_wasConnected)
        {
            LOG_WF(
                "HA_ORCHESTRATOR",
                "Client Home Assistant disconnesso"
            );
        }

        resetConnectionState();

        return false;
    }

    int findPendingState(int area) const
    {
        if (area < 0)
            return -1;

        for (uint16_t i = 0;
             i < _pendingStateCount;
             ++i)
        {
            const PendingState& pending =
                _pendingStates[i];

            if (pending.used &&
                pending.area == area)
            {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    bool enqueueState(int area, long value)
    {
        if (area < 0)
            return false;

        const int existingIndex =
            findPendingState(area);

        if (existingIndex >= 0)
        {
            PendingState& pending =
                _pendingStates[
                    static_cast<uint16_t>(existingIndex)
                ];

            pending.value = value;
            pending.used = true;
            return true;
        }

        if (_pendingStateCount >= MAX_PENDING_STATES)
            return false;

        PendingState& pending =
            _pendingStates[_pendingStateCount++];

        pending.area = area;
        pending.value = value;
        pending.used = true;

        return true;
    }

    void clearPendingStates()
    {
        for (uint16_t i = 0;
             i < _pendingStateCount;
             ++i)
        {
            _pendingStates[i] = PendingState{};
        }

        _pendingStateCount = 0;
    }

    bool entitySupportsState(
        const DomoIntrospection::Entity& entity) const
    {
        if (entity.implementationOnly ||
            !entity.readable)
        {
            return false;
        }

        switch (entity.kind)
        {
            case DomoIntrospection::EntityKind::SENSOR:
            case DomoIntrospection::EntityKind::BINARY_SENSOR:
            case DomoIntrospection::EntityKind::SWITCH:
            case DomoIntrospection::EntityKind::LIGHT:
            case DomoIntrospection::EntityKind::COVER:
            case DomoIntrospection::EntityKind::NUMBER:
            case DomoIntrospection::EntityKind::TEXT:
                return true;

            case DomoIntrospection::EntityKind::CLIMATE:
            case DomoIntrospection::EntityKind::BUTTON:
            case DomoIntrospection::EntityKind::STATUS:
            case DomoIntrospection::EntityKind::UNKNOWN:
            default:
                return false;
        }
    }

    void enqueueEntityState(int area, long value)
    {
        if (area < 0 ||
            !_config.introspection)
        {
            return;
        }

        const DomoIntrospection::Entity* entity =
            _config.introspection->findEntity(area);

        if (!entity ||
            !entitySupportsState(*entity))
        {
            return;
        }

        if (!enqueueState(area, value))
        {
            LOG_WF(
                "HA_ORCHESTRATOR",
                "Coda stati piena: area=%d value=%ld pending=%u",
                area,
                value,
                static_cast<unsigned>(_pendingStateCount)
            );
        }
    }

    // ========================================================================
    // PUBLISH INITIAL SNAPSHOT
    //
    // Pubblica lo stato iniziale di tutte le entity leggibili.
    //
    // AREA ENTITY:
    //
    //   Buffer
    //     -> PendingState
    //     -> publishPendingStates()
    //
    // RUNTIME ENTITY:
    //
    //   RuntimeStore
    //     -> publishRuntimeState()
    //
    // Le entity RuntimeStore non utilizzano la coda PendingState perché non
    // possiedono un'area Buffer valida.
    // ========================================================================

    void publishInitialSnapshot()
    {
        if (_initialStatesPublished ||
            !_availabilityPublished ||
            !_discoveryPublished ||
            !_config.manager ||
            !_config.introspection)
        {
            return;
        }

        auto& buffer =
            _config.manager->getBuffer();

        size_t areaEligibleCount =
            0;

        size_t areaQueuedCount =
            0;

        size_t runtimeEligibleCount =
            0;

        size_t runtimePublishedCount =
            0;

        size_t failedCount =
            0;

        for (size_t i = 0;
            i < _config.introspection->entityCount();
            ++i)
        {
            const DomoIntrospection::Entity* entity =
                _config.introspection->getEntity(
                    i
                );

            if (!entity ||
                entity->implementationOnly ||
                !entity->readable)
            {
                continue;
            }

            // ========================================================
            // RUNTIME STORE ENTITY
            // ========================================================

            if (entity->origin ==
                DomoIntrospection::EntityOrigin::RUNTIME)
            {
                if (entity->runtimeHandle ==
                    DMRuntimeStore::INVALID_HANDLE)
                {
                    ++failedCount;

                    LOG_WF(
                        "HA_ORCHESTRATOR",
                        "Snapshot runtime con handle non valido"
                    );

                    continue;
                }

                ++runtimeEligibleCount;

                if (publishRuntimeState(
                        entity->runtimeHandle))
                {
                    ++runtimePublishedCount;
                }
                else
                {
                    ++failedCount;

                    LOG_WF(
                        "HA_ORCHESTRATOR",
                        "Snapshot runtime fallito: "
                        "handle=%u id=%s",
                        static_cast<unsigned>(
                            entity->runtimeHandle
                        ),
                        entity->runtimeEntry &&
                        entity->runtimeEntry->id
                            ? entity->runtimeEntry->id
                            : "?"
                    );
                }

                continue;
            }

            // ========================================================
            // BUFFER AREA ENTITY
            // ========================================================

            if (entity->area < 0 ||
                !entitySupportsState(
                    *entity))
            {
                continue;
            }

            ++areaEligibleCount;

            const long currentValue =
                buffer.getValueFast(
                    entity->area
                );

            if (enqueueState(
                    entity->area,
                    currentValue))
            {
                ++areaQueuedCount;
            }
            else
            {
                ++failedCount;

                LOG_WF(
                    "HA_ORCHESTRATOR",
                    "Snapshot area non accodato: "
                    "area=%d value=%ld",
                    entity->area,
                    currentValue
                );
            }
        }

        /*
        * Lo snapshot è completato solo se:
        *
        *   - tutte le aree sono state accodate;
        *   - tutti i RuntimeTag sono stati pubblicati;
        *   - non si è verificato alcun errore.
        */

        if (failedCount == 0)
        {
            _initialStatesPublished =
                true;
        }

        LOG_IF(
            "HA_ORCHESTRATOR",
            "Snapshot: "
            "areaEligible=%u areaQueued=%u "
            "runtimeEligible=%u runtimePublished=%u "
            "failed=%u pending=%u completed=%u",
            static_cast<unsigned>(
                areaEligibleCount
            ),
            static_cast<unsigned>(
                areaQueuedCount
            ),
            static_cast<unsigned>(
                runtimeEligibleCount
            ),
            static_cast<unsigned>(
                runtimePublishedCount
            ),
            static_cast<unsigned>(
                failedCount
            ),
            static_cast<unsigned>(
                _pendingStateCount
            ),
            _initialStatesPublished
                ? 1U
                : 0U
        );
    }

    void publishPendingStates()
    {
        if (!_availabilityPublished ||
            !_discoveryPublished ||
            !_config.introspection ||
            _pendingStateCount == 0)
        {
            return;
        }

        uint16_t writePosition = 0;

        for (uint16_t i = 0;
             i < _pendingStateCount;
             ++i)
        {
            const PendingState pending =
                _pendingStates[i];

            if (!pending.used ||
                pending.area < 0)
            {
                continue;
            }

            const DomoIntrospection::Entity* entity =
                _config.introspection->findEntity(
                    pending.area
                );

            if (!entity ||
                !entitySupportsState(*entity))
            {
                continue;
            }

            bool published = false;

            switch (entity->kind)
            {
                case DomoIntrospection::EntityKind::SENSOR:
                case DomoIntrospection::EntityKind::NUMBER:
                {
                    const float scaledValue =
                        static_cast<float>(pending.value) *
                        entity->scale;

                    published = _homeAssistant.publishState(
                        *entity,
                        scaledValue,
                        3
                    );
                    break;
                }

                case DomoIntrospection::EntityKind::BINARY_SENSOR:
                case DomoIntrospection::EntityKind::SWITCH:
                case DomoIntrospection::EntityKind::LIGHT:
                {
                    published = _homeAssistant.publishBooleanState(
                        *entity,
                        pending.value != 0
                    );
                    break;
                }

                case DomoIntrospection::EntityKind::COVER:
                {
                    published = _homeAssistant.publishState(
                        *entity,
                        pending.value != 0
                            ? "open"
                            : "closed"
                    );
                    break;
                }

                case DomoIntrospection::EntityKind::CLIMATE:
                case DomoIntrospection::EntityKind::BUTTON:
                case DomoIntrospection::EntityKind::STATUS:
                case DomoIntrospection::EntityKind::UNKNOWN:
                default:
                {
                    published = true;
                    break;
                }
            }

            if (!published)
            {
                _pendingStates[writePosition++] = pending;
            }
        }

        for (uint16_t i = writePosition;
             i < _pendingStateCount;
             ++i)
        {
            _pendingStates[i] = PendingState{};
        }

        _pendingStateCount = writePosition;
    }

    // ========================================================================
    // RUNTIME VALUE TYPE
    // ========================================================================

    static MQTTInboundMessage::ValueType runtimeValueType(
        DMRuntimeStore::Type type)
    {
        using ValueType =
            MQTTInboundMessage::ValueType;

        switch (type)
        {
            case DMRuntimeStore::Type::BOOL:
                return ValueType::BOOLEAN;

            case DMRuntimeStore::Type::INT32:
                return ValueType::INTEGER;

            case DMRuntimeStore::Type::FLOAT:
                return ValueType::FLOAT;

            case DMRuntimeStore::Type::STRING32:
            case DMRuntimeStore::Type::STRING64:
                return ValueType::TEXT;

            case DMRuntimeStore::Type::JSON128:
            case DMRuntimeStore::Type::JSON256:
                return ValueType::JSON;
        }

        return ValueType::NONE;
    }


    // ========================================================================
    // NOTIFY INBOUND MESSAGE
    //
    // Inoltra al frontend un messaggio MQTT normalizzato.
    //
    // La callback viene eseguita sincronicamente.
    // I puntatori topic/payload non devono essere conservati senza copia.
    // ========================================================================

    void notifyInboundMessage(
        const MQTTInboundMessage& message)
    {
        if (!_config.inboundMessageCallback)
            return;

        if (!message.valid())
        {
            LOG_WF(
                "HA_COMMAND",
                "Messaggio inbound non valido: "
                "client=%u origin=%u",
                static_cast<unsigned>(
                    message.clientIndex
                ),
                static_cast<unsigned>(
                    message.origin
                )
            );

            return;
        }

        _config.inboundMessageCallback(
            message
        );
    }

    // ========================================================================
    // PROCESS COMMAND
    //
    // Analizza il topic e instrada il comando verso:
    //
    //   - Buffer, per i comandi area;
    //   - RuntimeStore, per le variabili runtime.
    //
    // Non esegue conversioni prima di aver identificato il target.
    // ========================================================================

    void processCommand(
        uint8_t clientIndex,
        const char* topic,
        const char* payload,
        size_t length)
    {
        ParsedCommand command;

        if (!parseCommand(
                clientIndex,
                topic,
                payload,
                length,
                command))
        {
            LOG_WF(
                "HA_COMMAND",
                "Topic o payload non valido: client=%u topic=%s",
                static_cast<unsigned>(
                    clientIndex
                ),
                topic
                    ? topic
                    : "?"
            );

            return;
        }

        switch (command.target)
        {
            case ParsedCommand::Target::AREA:
            {
                processAreaCommand(
                    command
                );

                return;
            }

            case ParsedCommand::Target::RUNTIME:
            {
                processRuntimeCommand(
                    command
                );

                return;
            }

            case ParsedCommand::Target::INVALID:
            default:
            {
                LOG_WF(
                    "HA_COMMAND",
                    "Target comando non valido: client=%u topic=%s",
                    static_cast<unsigned>(
                        clientIndex
                    ),
                    topic
                        ? topic
                        : "?"
                );

                return;
            }
        }
    }

    // ========================================================================
    // PROCESS AREA COMMAND
    //
    // Gestisce esclusivamente:
    //
    //   domomanager/area/<area>/set
    //
    // Il comando viene validato tramite Introspection e poi inoltrato alla
    // callback numerica già esistente.
    // ========================================================================

    void processAreaCommand(
        const ParsedCommand& command)
    {
        if (!_config.introspection)
            return;

        if (command.area < 0)
            return;

        const DomoIntrospection::Entity* entity =
            _config.introspection->findEntity(
                command.area
            );

        if (!entity)
        {
            LOG_WF(
                "HA_COMMAND",
                "Entity area non trovata: area=%d",
                command.area
            );

            return;
        }

        if (entity->implementationOnly ||
            !entity->writable)
        {
            LOG_WF(
                "HA_COMMAND",
                "Entity area non comandabile: area=%d",
                command.area
            );

            return;
        }

        long value =
            0;

        if (!parseCommandPayload(
                *entity,
                command.payload,
                command.payloadLength,
                value))
        {
            LOG_WF(
                "HA_COMMAND",
                "Payload area non valido: area=%d kind=%u",
                command.area,
                static_cast<unsigned>(
                    entity->kind
                )
            );

            return;
        }

        if (!_config.commandCallback)
        {
            LOG_WF(
                "HA_COMMAND",
                "Command callback assente: area=%d",
                command.area
            );

            return;
        }

        _config.commandCallback(
            _config.commandContext,
            command.clientIndex,
            command.area,
            value
        );

        // ========================================================
        // NORMALIZED MQTT NOTIFICATION
        // ========================================================

        MQTTInboundMessage message;

        message.origin =
            MQTTInboundMessage::Origin::
                HOME_ASSISTANT_AREA;

        message.valueType =
            MQTTInboundMessage::ValueType::
                INTEGER;

        message.clientIndex =
            command.clientIndex;

        message.backend =
            FrontendConfig::MQTT::Client::Backend::
                HOME_ASSISTANT;

        message.topic =
            command.topic;

        message.payload =
            command.payload;

        message.payloadLength =
            command.payloadLength;

        message.area =
            command.area;

        message.integerValue =
            value;

        notifyInboundMessage(
            message
        );

        LOG_IF(
            "HA_COMMAND",
            "Comando area validato: "
            "client=%u area=%d value=%ld",
            static_cast<unsigned>(
                command.clientIndex
            ),
            command.area,
            value
        );
    }

    // ========================================================================
    // FIND RUNTIME ENTITY
    //
    // Cerca nell'Introspection la entity associata a un Handle RuntimeStore.
    //
    // La ricerca è O(N), ma viene eseguita solamente in seguito a una scrittura
    // runtime proveniente da Home Assistant.
    //
    // Non viene utilizzata nel normale ciclo realtime.
    // ========================================================================

    const DomoIntrospection::Entity*
    findRuntimeEntity(
        DMRuntimeStore::Handle handle) const
    {
        if (!_config.introspection ||
            handle ==
                DMRuntimeStore::INVALID_HANDLE)
        {
            return nullptr;
        }

        for (size_t i = 0;
            i < _config.introspection->entityCount();
            ++i)
        {
            const DomoIntrospection::Entity* entity =
                _config.introspection->getEntity(
                    i
                );

            if (!entity)
                continue;

            if (entity->origin !=
                DomoIntrospection::EntityOrigin::RUNTIME)
            {
                continue;
            }

            if (entity->runtimeHandle !=
                handle)
            {
                continue;
            }

            return entity;
        }

        return nullptr;
    }

    // ========================================================================
    // PUBLISH RUNTIME STATE
    //
    // Pubblica il valore corrente di una Entry RuntimeStore sul relativo
    // state_topic Home Assistant.
    //
    // Topic:
    //
    //   <prefix>/runtime/<id>/state
    //
    // Il tipo del payload viene ricavato dal tipo reale della Entry.
    // ========================================================================

    bool publishRuntimeState(
        DMRuntimeStore::Handle handle)
    {
        if (!_config.manager ||
            !_config.introspection ||
            handle ==
                DMRuntimeStore::INVALID_HANDLE)
        {
            return false;
        }

        auto& store =
            _config.manager->getRuntimeStore();

        const DMRuntimeStore::Entry* entry =
            store.getEntry(
                handle
            );

        if (!entry ||
            !entry->used ||
            !entry->initialized ||
            !entry->publish ||
            !entry->readable())
        {
            return false;
        }

        const DomoIntrospection::Entity* entity =
            findRuntimeEntity(
                handle
            );

        if (!entity ||
            entity->implementationOnly ||
            !entity->readable)
        {
            return false;
        }

        bool published =
            false;

        switch (entry->type)
        {
            // ====================================================
            // BOOL
            // ====================================================

            case DMRuntimeStore::Type::BOOL:
            {
                published =
                    _homeAssistant.publishBooleanState(
                        *entity,
                        store.getBool(
                            handle,
                            false
                        )
                    );

                break;
            }

            // ====================================================
            // INT32
            // ====================================================

            case DMRuntimeStore::Type::INT32:
            {
                published =
                    _homeAssistant.publishState(
                        *entity,
                        static_cast<long>(
                            store.getInt32(
                                handle,
                                0
                            )
                        )
                    );

                break;
            }

            // ====================================================
            // FLOAT
            // ====================================================

            case DMRuntimeStore::Type::FLOAT:
            {
                published =
                    _homeAssistant.publishState(
                        *entity,
                        store.getFloat(
                            handle,
                            0.0f
                        ),
                        entry->decimals
                    );

                break;
            }

            // ====================================================
            // STRING
            // ====================================================

            case DMRuntimeStore::Type::STRING32:
            case DMRuntimeStore::Type::STRING64:
            {
                published =
                    _homeAssistant.publishState(
                        *entity,
                        store.getString(
                            handle,
                            ""
                        )
                    );

                break;
            }

            // ====================================================
            // JSON
            // ====================================================

            case DMRuntimeStore::Type::JSON128:
            case DMRuntimeStore::Type::JSON256:
            {
                published =
                    _homeAssistant.publishState(
                        *entity,
                        store.getJson(
                            handle,
                            "{}"
                        )
                    );

                break;
            }
        }

        if (!published)
        {
            LOG_WF(
                "HA_RUNTIME",
                "Pubblicazione stato fallita: "
                "handle=%u id=%s type=%s",
                static_cast<unsigned>(
                    handle
                ),
                entry->id
                    ? entry->id
                    : "?",
                DMRuntimeStore::typeToString(
                    entry->type
                )
            );

            return false;
        }

        LOG_IF(
            "HA_RUNTIME",
            "Stato pubblicato: "
            "handle=%u id=%s topic=%s",
            static_cast<unsigned>(
                handle
            ),
            entry->id
                ? entry->id
                : "?",
            _homeAssistant
                .stateTopic(
                    *entity
                )
                .c_str()
        );

        return true;
    }

    // ========================================================================
    // PROCESS RUNTIME COMMAND
    //
    // Gestisce esclusivamente:
    //
    //   domomanager/runtime/<id>/set
    //
    // La conversione viene eseguita in base al tipo reale della Entry:
    //
    //   BOOL       -> setBool()
    //   INT32      -> setInt32()
    //   FLOAT      -> setFloat()
    //   STRING     -> setString()
    //   JSON       -> setJson()
    //
    // RuntimeStore verifica autonomamente:
    //
    //   - validità Handle;
    //   - tipo;
    //   - accesso READ_ONLY / WRITE_ONLY / READ_WRITE;
    //   - overflow stringa;
    //   - NaN / INF;
    //   - integrità memoria.
    // ========================================================================

    void processRuntimeCommand(
        const ParsedCommand& command)
    {
        if (!_config.manager ||
            !command.runtimeEntry ||
            command.runtimeHandle ==
                DMRuntimeStore::INVALID_HANDLE)
        {
            return;
        }

        const DMRuntimeStore::Entry& entry =
            *command.runtimeEntry;

        if (!entry.writable())
        {
            LOG_WF(
                "HA_COMMAND",
                "Runtime tag non comandabile: id=%s access=%s",
                entry.id
                    ? entry.id
                    : "?",
                DMRuntimeStore::accessToString(
                    entry.access
                )
            );

            return;
        }

        /*
        * I callback MQTT forniscono lunghezza esplicita.
        *
        * Non possiamo presumere che payload sia terminato da '\0'.
        * La copia locale rende sicure strcmp(), strtof() e RuntimeStore.
        */
        char valueBuffer[
            DMRuntimeStore::MAX_ID_LENGTH > 256
                ? DMRuntimeStore::MAX_ID_LENGTH
                : 256
        ];

        if (!command.payload ||
            command.payloadLength == 0)
        {
            LOG_WF(
                "HA_COMMAND",
                "Payload runtime vuoto: id=%s",
                entry.id
                    ? entry.id
                    : "?"
            );

            return;
        }

        if (command.payloadLength >=
            sizeof(valueBuffer))
        {
            LOG_WF(
                "HA_COMMAND",
                "Payload runtime troppo lungo: id=%s bytes=%u max=%u",
                entry.id
                    ? entry.id
                    : "?",
                static_cast<unsigned>(
                    command.payloadLength
                ),
                static_cast<unsigned>(
                    sizeof(valueBuffer) - 1
                )
            );

            return;
        }

        memcpy(
            valueBuffer,
            command.payload,
            command.payloadLength
        );

        valueBuffer[
            command.payloadLength
        ] = '\0';

        auto& store =
            _config.manager->getRuntimeStore();

        DMRuntimeStore::WriteResult result =
            DMRuntimeStore::WriteResult::INVALID_TYPE;

        const unsigned long now =
            millis();

        switch (entry.type)
        {
            case DMRuntimeStore::Type::BOOL:
            {
                bool value;

                if (strcasecmp(
                        valueBuffer,
                        "ON") == 0 ||
                    strcasecmp(
                        valueBuffer,
                        "TRUE") == 0 ||
                    strcmp(
                        valueBuffer,
                        "1") == 0)
                {
                    value =
                        true;
                }
                else if (
                    strcasecmp(
                        valueBuffer,
                        "OFF") == 0 ||
                    strcasecmp(
                        valueBuffer,
                        "FALSE") == 0 ||
                    strcmp(
                        valueBuffer,
                        "0") == 0)
                {
                    value =
                        false;
                }
                else
                {
                    LOG_WF(
                        "HA_COMMAND",
                        "Payload BOOL non valido: id=%s",
                        entry.id
                            ? entry.id
                            : "?"
                    );

                    return;
                }

                result =
                    store.setBool(
                        command.runtimeHandle,
                        value,
                        DMRuntimeStore::Source::
                            HOME_ASSISTANT,
                        now
                    );

                break;
            }

            case DMRuntimeStore::Type::INT32:
            {
                char* endPointer =
                    nullptr;

                const long parsed =
                    strtol(
                        valueBuffer,
                        &endPointer,
                        10
                    );

                if (endPointer ==
                        valueBuffer ||
                    *endPointer != '\0' ||
                    parsed < INT32_MIN ||
                    parsed > INT32_MAX)
                {
                    LOG_WF(
                        "HA_COMMAND",
                        "Payload INT32 non valido: id=%s",
                        entry.id
                            ? entry.id
                            : "?"
                    );

                    return;
                }

                result =
                    store.setInt32(
                        command.runtimeHandle,
                        static_cast<int32_t>(
                            parsed
                        ),
                        DMRuntimeStore::Source::
                            HOME_ASSISTANT,
                        now
                    );

                break;
            }

            case DMRuntimeStore::Type::FLOAT:
            {
                char* endPointer =
                    nullptr;

                const float parsed =
                    strtof(
                        valueBuffer,
                        &endPointer
                    );

                if (endPointer ==
                        valueBuffer ||
                    *endPointer != '\0' ||
                    !isfinite(parsed))
                {
                    LOG_WF(
                        "HA_COMMAND",
                        "Payload FLOAT non valido: id=%s",
                        entry.id
                            ? entry.id
                            : "?"
                    );

                    return;
                }

                result =
                    store.setFloat(
                        command.runtimeHandle,
                        parsed,
                        DMRuntimeStore::Source::
                            HOME_ASSISTANT,
                        now
                    );

                break;
            }

            case DMRuntimeStore::Type::STRING32:
            case DMRuntimeStore::Type::STRING64:
            {
                result =
                    store.setString(
                        command.runtimeHandle,
                        valueBuffer,
                        DMRuntimeStore::Source::
                            HOME_ASSISTANT,
                        now
                    );

                break;
            }

            case DMRuntimeStore::Type::JSON128:
            case DMRuntimeStore::Type::JSON256:
            {
                result =
                    store.setJson(
                        command.runtimeHandle,
                        valueBuffer,
                        DMRuntimeStore::Source::
                            HOME_ASSISTANT,
                        now
                    );

                break;
            }
        }

        switch (result)
        {
            case DMRuntimeStore::WriteResult::CHANGED:
            {
                const DMRuntimeStore::Entry* updatedEntry =
                    store.getEntry(
                        command.runtimeHandle
                    );

                LOG_IF(
                    "HA_COMMAND",
                    "Runtime aggiornato: "
                    "client=%u id=%s handle=%u",
                    static_cast<unsigned>(
                        command.clientIndex
                    ),
                    updatedEntry &&
                    updatedEntry->id
                        ? updatedEntry->id
                        : "?",
                    static_cast<unsigned>(
                        command.runtimeHandle
                    )
                );

                if (!updatedEntry)
                    break;

                // ====================================================
                // NORMALIZED MQTT NOTIFICATION
                // ====================================================

                MQTTInboundMessage message;

                message.origin =
                    MQTTInboundMessage::Origin::
                        HOME_ASSISTANT_RUNTIME;

                message.valueType =
                    runtimeValueType(
                        updatedEntry->type
                    );

                message.clientIndex =
                    command.clientIndex;

                message.backend =
                    FrontendConfig::MQTT::Client::Backend::
                        HOME_ASSISTANT;

                message.topic =
                    command.topic;

                message.payload =
                    valueBuffer;

                message.payloadLength =
                    command.payloadLength;

                message.runtimeHandle =
                    static_cast<uint8_t>(
                        command.runtimeHandle
                    );

                message.runtimeId =
                    updatedEntry->id;

                message.runtimeType =
                    static_cast<uint8_t>(
                        updatedEntry->type
                    );

                message.runtimeSensitive =
                    updatedEntry->sensitive;

                switch (updatedEntry->type)
                {
                    case DMRuntimeStore::Type::BOOL:
                    {
                        message.booleanValue =
                            store.getBool(
                                command.runtimeHandle,
                                false
                            );

                        message.integerValue =
                            message.booleanValue
                                ? 1L
                                : 0L;

                        break;
                    }

                    case DMRuntimeStore::Type::INT32:
                    {
                        message.integerValue =
                            static_cast<long>(
                                store.getInt32(
                                    command.runtimeHandle,
                                    0
                                )
                            );

                        break;
                    }

                    case DMRuntimeStore::Type::FLOAT:
                    {
                        message.floatValue =
                            store.getFloat(
                                command.runtimeHandle,
                                0.0f
                            );

                        break;
                    }

                    case DMRuntimeStore::Type::STRING32:
                    case DMRuntimeStore::Type::STRING64:
                    case DMRuntimeStore::Type::JSON128:
                    case DMRuntimeStore::Type::JSON256:
                    {
                        break;
                    }
                }

                notifyInboundMessage(
                    message
                );

                // ====================================================
                // HOME ASSISTANT STATE FEEDBACK
                //
                // Pubblica sullo state_topic il valore realmente presente
                // nel RuntimeStore dopo la scrittura.
                // ====================================================

                if (!publishRuntimeState(
                        command.runtimeHandle))
                {
                    LOG_WF(
                        "HA_COMMAND",
                        "Runtime aggiornato ma stato non pubblicato: "
                        "id=%s handle=%u",
                        updatedEntry->id
                            ? updatedEntry->id
                            : "?",
                        static_cast<unsigned>(
                            command.runtimeHandle
                        )
                    );
                }

                break;
            }

            case DMRuntimeStore::WriteResult::UNCHANGED:
            {
                LOG_DF(
                    "HA_COMMAND",
                    "Runtime invariato: id=%s",
                    entry.id
                        ? entry.id
                        : "?"
                );

                break;
            }

            default:
            {
                LOG_WF(
                    "HA_COMMAND",
                    "Scrittura RuntimeStore fallita: id=%s result=%u",
                    entry.id
                        ? entry.id
                        : "?",
                    static_cast<unsigned>(
                        result
                    )
                );

                break;
            }
        }
    }

    // ========================================================================
    // PARSE RUNTIME COMMAND TOPIC
    //
    // Esempio:
    //
    //   domomanager/runtime/access.room01.card_code/set
    //
    // Output:
    //
    //   access.room01.card_code
    // ========================================================================

    bool parseRuntimeCommandTopic(
        const char* topic,
        char* runtimeId,
        size_t runtimeIdSize) const
    {
        if (!topic ||
            !runtimeId ||
            runtimeIdSize == 0 ||
            _commandTopics.runtime.length() == 0)
        {
            return false;
        }

        runtimeId[0] =
            '\0';

        const char* pattern =
            _commandTopics.runtime.c_str();

        const char* wildcard =
            strchr(
                pattern,
                '+'
            );

        if (!wildcard)
            return false;

        const size_t prefixLength =
            static_cast<size_t>(
                wildcard - pattern
            );

        const char* suffix =
            wildcard + 1;

        const size_t topicLength =
            strlen(topic);

        const size_t suffixLength =
            strlen(suffix);

        if (topicLength <
            prefixLength + suffixLength + 1)
        {
            return false;
        }

        if (strncmp(
                topic,
                pattern,
                prefixLength) != 0)
        {
            return false;
        }

        const size_t idLength =
            topicLength -
            prefixLength -
            suffixLength;

        if (idLength == 0 ||
            idLength >= runtimeIdSize)
        {
            return false;
        }

        const char* idStart =
            topic + prefixLength;

        if (strcmp(
                idStart + idLength,
                suffix) != 0)
        {
            return false;
        }

        memcpy(
            runtimeId,
            idStart,
            idLength
        );

        runtimeId[idLength] =
            '\0';

        return true;
    }

    bool parseCommandTopic(
        const char* topic,
        int& area) const
    {
        area =
            -1;

        if (!topic ||
            _commandTopics.area.length() == 0)
        {
            return false;
        }

        const char* pattern =
            _commandTopics.area.c_str();

        const char* wildcard =
            strchr(
                pattern,
                '+'
            );

        if (!wildcard)
            return false;

        const size_t prefixLength =
            static_cast<size_t>(
                wildcard - pattern
            );

        const char* suffix =
            wildcard + 1;

        if (strncmp(
                topic,
                pattern,
                prefixLength) != 0)
        {
            return false;
        }

        const char* areaStart =
            topic + prefixLength;

        char* endPointer =
            nullptr;

        const long parsedArea =
            strtol(
                areaStart,
                &endPointer,
                10
            );

        if (endPointer == areaStart ||
            strcmp(
                endPointer,
                suffix) != 0 ||
            parsedArea < 0 ||
            parsedArea > 32767)
        {
            return false;
        }

        area =
            static_cast<int>(
                parsedArea
            );

        return true;
    }

    // ========================================================================
    // PARSE COMMAND
    //
    // Riconosce automaticamente:
    //
    //   - comandi Buffer basati su area;
    //   - comandi RuntimeStore basati su ID.
    //
    // Non modifica né Buffer né RuntimeStore.
    // ========================================================================

    bool parseCommand(
        uint8_t clientIndex,
        const char* topic,
        const char* payload,
        size_t length,
        ParsedCommand& command)
    {
        command =
            ParsedCommand{};

        command.clientIndex =
            clientIndex;

        command.topic =
            topic;

        command.payload =
            payload;

        command.payloadLength =
            length;

        if (!topic ||
            !payload ||
            length == 0)
        {
            return false;
        }

        // ========================================================
        // AREA
        // ========================================================

        int area =
            -1;

        if (parseCommandTopic(
                topic,
                area))
        {
            command.target =
                ParsedCommand::Target::AREA;

            command.area =
                area;

            return true;
        }

        // ========================================================
        // RUNTIME STORE
        // ========================================================

        char runtimeId[
            DMRuntimeStore::MAX_ID_LENGTH
        ];

        if (!parseRuntimeCommandTopic(
                topic,
                runtimeId,
                sizeof(runtimeId)))
        {
            return false;
        }

        if (!_config.manager)
            return false;

        auto& store =
            _config.manager->getRuntimeStore();

        const DMRuntimeStore::Handle handle =
            store.find(
                runtimeId
            );

        if (handle ==
            DMRuntimeStore::INVALID_HANDLE)
        {
            LOG_WF(
                "HA_COMMAND",
                "Runtime tag non trovata: id=%s",
                runtimeId
            );

            return false;
        }

        const DMRuntimeStore::Entry* entry =
            store.getEntry(
                handle
            );

        if (!entry)
            return false;

        command.target =
            ParsedCommand::Target::RUNTIME;

        command.runtimeHandle =
            handle;

        command.runtimeEntry =
            entry;

        return true;
    }

    bool parseCommandPayload(
        const DomoIntrospection::Entity& entity,
        const char* payload,
        size_t length,
        long& value) const
    {
        value = 0;

        if (!payload || length == 0)
            return false;

        switch (entity.kind)
        {
            case DomoIntrospection::EntityKind::SWITCH:
            case DomoIntrospection::EntityKind::LIGHT:
            {
                if (strcasecmp(payload, "ON") == 0 ||
                    strcasecmp(payload, "TRUE") == 0 ||
                    strcmp(payload, "1") == 0)
                {
                    value = 1;
                    return true;
                }

                if (strcasecmp(payload, "OFF") == 0 ||
                    strcasecmp(payload, "FALSE") == 0 ||
                    strcmp(payload, "0") == 0)
                {
                    value = 0;
                    return true;
                }

                return false;
            }

            case DomoIntrospection::EntityKind::BUTTON:
            {
                if (strcasecmp(payload, "PRESS") == 0 ||
                    strcmp(payload, "1") == 0)
                {
                    value = 1;
                    return true;
                }

                return false;
            }

            case DomoIntrospection::EntityKind::NUMBER:
            {
                char* endPointer = nullptr;
                const float parsed = strtof(
                    payload,
                    &endPointer
                );

                if (endPointer == payload ||
                    *endPointer != '\0')
                {
                    return false;
                }

                value = entity.scale == 0.0f
                    ? static_cast<long>(parsed)
                    : static_cast<long>(parsed / entity.scale);

                return true;
            }

            case DomoIntrospection::EntityKind::TEXT:
            case DomoIntrospection::EntityKind::COVER:
            case DomoIntrospection::EntityKind::SENSOR:
            case DomoIntrospection::EntityKind::BINARY_SENSOR:
            case DomoIntrospection::EntityKind::CLIMATE:
            case DomoIntrospection::EntityKind::STATUS:
            case DomoIntrospection::EntityKind::UNKNOWN:
            default:
                return false;
        }
    }
};

class PowerSupervisorOrchestrator {
private:
    // ------------------------------------------------------------
    //  CONFIGURAZIONE DINAMICA (copiata dal frontend)
    // ------------------------------------------------------------
    static inline FrontendConfig::PowerSupervisor cfg;

    // ------------------------------------------------------------
    //  LETTURE SENSORI BASE (motore puro)
    // ------------------------------------------------------------
    static PowerSupervisor::PowerValue readMainPower() {
        auto& buf = DomoManager::instance->getBuffer();
        float v = GenericSensor::read(cfg.mainPower, buf);
        if (v <= 0)
            return {0.0f, false};
        return {v, true};
    }

    static bool readI24vOk() {
        auto& buf = DomoManager::instance->getBuffer();
        return GenericSensor::read(cfg.i24vOk, buf) != 0;
    }

    static bool readFault() {
        auto& buf = DomoManager::instance->getBuffer();
        return GenericSensor::read(cfg.fault, buf) != 0;
    }

    static bool readBattery() {
        auto& buf = DomoManager::instance->getBuffer();
        return GenericSensor::read(cfg.battery, buf) != 0;
    }

    // ------------------------------------------------------------
    //  ISTANZA SUPERVISORE
    // ------------------------------------------------------------
    static inline PowerSupervisor* instance = nullptr;

public:
    // Callback definita dal frontend
    using AlarmCallback = void (*)(const String&, long);

    // ------------------------------------------------------------
    //  SETUP (motore puro)
    // ------------------------------------------------------------
    static void Setup(const FrontendConfig::PowerSupervisor& cfgIn,
                      AlarmCallback cb)
    {
        cfg = cfgIn;

        if (instance)
            delete instance;

        // Costruzione configurazione base del supervisore
        PowerSupervisor::Config psCfg = {
            readMainPower,
            readI24vOk,
            readFault,
            readBattery
        };

        psCfg.mainPowerLow  = cfg.mainPowerLow;
        psCfg.mainPowerHigh = cfg.mainPowerHigh;

        instance = new PowerSupervisor(psCfg);

        // La callback viene dal frontend
        instance->onAlarm(cb);

        instance->setup();

        LOG_IF("PowerSupervisorEngine", "PowerSupervisor initialized");
    }

    // ------------------------------------------------------------
    //  LOOP
    // ------------------------------------------------------------
    static void Loop(unsigned long now) {
        if (instance)
            instance->loop(now);
    }

    // ------------------------------------------------------------
    //  ACCESSOR
    // ------------------------------------------------------------
    static PowerSupervisor& Get() {
        return *instance;
    }
};


class WeatherOrchestrator {
private:
    static inline WeatherStation* instance = nullptr;

public:
    using EventCallback = void (*)(WeatherEvent);
    using AlarmCallback = void (*)(const WeatherAlarm*, int);

    static void Setup(const FrontendConfig::Weather& cfg,
                      EventCallback evCb,
                      AlarmCallback alCb)
    {
        if (instance)
            delete instance;

        instance = new WeatherStation(cfg.config);

        instance->setEventCallback(evCb);
        instance->setAlarmCallback(alCb);

        LOG_IF("METEO", "Weather initialized (dynamic config)");
    }

    static void Loop(unsigned long now) {
        if (instance)
            instance->update();
    }

    static WeatherStation& Get() {
        return *instance;
    }
};


class PowerOrchestrator {
private:
    static inline PowerManager* instance = nullptr;
    static inline int loadShedStatusArea =  -1;

    // Costruisce o restituisce l’istanza del PowerManager
    static PowerManager& pm(int fallbackSoft = 3000,
                            const FrontendConfig::Power* cfg = nullptr)
    {
        if (!instance) {
    
            float gridLimit = cfg ? cfg->limitSoft
                                  : static_cast<float>(fallbackSoft);

            instance = new PowerManager(gridLimit);
        }
        return *instance;
    }

    /**
    * @brief Pubblica nel sistema la word di stato degli stacchi.
    *
    * La word viene inviata all'area configurata tramite un evento
    * interno, così attraversa il normale flusso EventManager/Buffer
    * ed è disponibile per HMI, MQTT, AEE e altri consumer.
    *
    * @param manager PowerManager da cui leggere la word.
    * @param now Timestamp corrente, riservato alla diagnostica.
    */
    static void publishLoadShedStatus(
        PowerManager& manager,
        unsigned long now)
    {
        if (loadShedStatusArea < 0)
            return;

        DomoManager* domoManager =
            DomoManager::instance;

        if (domoManager == nullptr)
            return;

        const uint16_t statusWord =
            manager
                .loadManager()
                .getLoadShedStatusWord();

        domoManager->forceInternalEvent(
            loadShedStatusArea,
            static_cast<long>(
                statusWord));

        LOG_DF(
            "POWER",
            "Load shed status area=%d word=0x%04X value=%u",
            loadShedStatusArea,
            static_cast<unsigned int>(
                statusWord),
            static_cast<unsigned int>(
                statusWord));

        (void)now;
    }

public:
    // Tipi callback forniti dal frontend
    using LoadChangeCb = void (*)(const String&, bool);
    using WarningCb    = void (*)(float, float);
    using ExceededCb   = void (*)(float, float);
    using ErrorCb      = void (*)(int, const String&);
    using SuggestionCb = void (*)(const String&, int, const String&);

    // ============================================================
    // SETUP (motore puro, backend modulare)
    // ============================================================
    /**
    * @brief Configura e inizializza il PowerOrchestrator.
    *
    * La funzione:
    * - inizializza PowerManager;
    * - configura soft limit e hard limit;
    * - registra le callback frontend;
    * - configura l'autotuning;
    * - registra i carichi elettrici;
    * - registra il carico termico;
    * - installa la gestione di emergenza per l'hard limit.
    *
    * @param cfg Configurazione frontend della gestione potenza.
    * @param fallbackSoft Limite soft utilizzato come fallback.
    * @param fallbackHard Limite hard utilizzato quando quello configurato
    *                     non è valido o non è definito.
    * @param onLoad Callback di variazione dello stato reale dei carichi.
    * @param onWarn Callback di avvicinamento al limite.
    * @param onExceeded Callback di superamento del limite soft.
    * @param onErr Callback di errore.
    * @param onSug Callback di suggestion.
    */
    static void Setup(
        const FrontendConfig::Power& cfg,
        int fallbackSoft,
        int fallbackHard,
        LoadChangeCb onLoad,
        WarningCb onWarn,
        ExceededCb onExceeded,
        ErrorCb onErr,
        SuggestionCb onSug)
    {
        loadShedStatusArea =
            cfg.loadShedStatusArea;

        // ============================================================
        // 1. POWER MANAGER
        // ============================================================

        auto& powerManager =
            pm(fallbackSoft);

        PowerManager* manager =
            &powerManager;

        // ============================================================
        // 2. HARD LIMIT
        // ============================================================

        const float configuredHardLimit =
            cfg.limitHard > 0
                ? static_cast<float>(cfg.limitHard)
                : static_cast<float>(fallbackHard);

        powerManager
            .limitManager()
            .setHardLimit(configuredHardLimit);

        /**
        * La callback viene memorizzata nel LimitManager e può essere
        * eseguita dopo il ritorno da Setup().
        *
        * Per questo motivo non deve acquisire per riferimento le
        * variabili locali di Setup(). Il puntatore al PowerManager
        * viene acquisito per valore.
        */
        powerManager
        .limitManager()
        .setHardLimitCallback(
            [manager](
                float netPower,
                float hardLimit)
            {
                if (manager == nullptr)
                    return;

                const unsigned long now =
                    millis();

                auto& loadManager =
                    manager->loadManager();

                /*
                * Primo tentativo:
                * distacco rispettando il tempo minimo ON.
                */
                bool requested =
                    loadManager
                        .requestLowestPriorityLoadOff(
                            3,
                            "hard limit",
                            now,
                            true);

                /*
                * Secondo tentativo:
                * se nessun carico può essere distaccato rispettando
                * minOnMs, l'hard limit forza il distacco.
                */
                if (!requested)
                {
                    requested =
                        loadManager
                            .requestLowestPriorityLoadOff(
                                3,
                                "hard limit forced",
                                now,
                                false);
                }

                /*
                * I parametri sono disponibili per diagnostica futura,
                * ma al momento la selezione usa lo stato dei carichi.
                */
                (void)netPower;
                (void)hardLimit;
            });

        // ============================================================
        // 3. CONFIGURAZIONE POWER MANAGER
        // ============================================================

        PowerManager::Config powerConfig;

        powerConfig.autoTune =
            cfg.autoTune;

        powerConfig.tuneIntervalMs =
            cfg.tuneIntervalMs;

        powerManager.setConfig(
            powerConfig);

        // ============================================================
        // 4. CALLBACK FRONTEND
        // ============================================================

        powerManager.setOnLoadChange(
            onLoad);

        powerManager.setOnLimitWarning(
            onWarn);

        powerManager.setOnLimitExceeded(
            onExceeded);

        powerManager.setOnError(
            onErr);

        powerManager.setOnSuggestion(
            onSug);

        // ============================================================
        // 5. CARICHI ELETTRICI
        // ============================================================

        auto& lm =
            powerManager.loadManager();

        for (size_t index = 0;
            index < cfg.loadCount;
            ++index)
        {
            const auto& loadConfig =
                cfg.loads[index];

            const bool added =
                lm.addLoad(
                    loadConfig.name,
                    loadConfig.watt,
                    loadConfig.minOn,
                    loadConfig.minOff,
                    loadConfig.priority);

            if (!added)
            {
                if (onErr)
                {
                    onErr(
                        1001,
                        String("Impossibile aggiungere il carico: ")
                        +
                        loadConfig.name);
                }

                continue;
            }
        }


        // ============================================================
        // 7. DIAGNOSTICA
        // ============================================================

        LOG_IF(
            "POWER",
            "PowerOrchestrator initialized "
            "(modular PowerManager)");
    }

    //======================================================================
    // LOOP
    //----------------------------------------------------------------------
    // Aggiorna il controllo energetico e la gestione dei carichi.
    //
    // Parametri:
    //
    //   now
    //       Timestamp corrente ottenuto da millis().
    //
    //   gridPower
    //       Potenza di scambio con la rete.
    //
    //       Convenzione:
    //         valore positivo = prelievo dalla rete
    //         valore negativo = immissione in rete
    //
    //   meanTemperature
    //       Temperatura ambiente media utilizzata dal controllo dei
    //       carichi termici.
    //
    // La produzione FV e lo stato batteria non vengono ricalcolati qui.
    // Sono già incorporati nella potenza di scambio misurata dalla rete.
    //
    // Il controllo termico riceve forecastW = 0 perché il precedente
    // modello previsionale apparteneva a PVStringManager, ora rimosso.
    //======================================================================

    static void Loop(
        unsigned long now,
        int gridPower)
    {
        auto& powerManager =
            pm();

        powerManager.setGridPower(
            static_cast<float>(
                gridPower));

        powerManager.update(
            0,
            0,
            0,
            now);

        publishLoadShedStatus(
            powerManager,
            now);
    }


    // ============================================================
    // ACCESSOR
    // ============================================================
    static PowerManager& Get() {
        return pm();
    }
};

class SolarOrchestrator
{
public:

    // ========================================================
    // PUBLIC TYPES
    // ========================================================

    using Snapshot =
        ISolarInverter::Snapshot;

    using Diagnostics =
        ISolarInverter::Diagnostics;

    using SnapshotChangedCallback =
        void (*)(
            uint8_t orchestratorId,
            const Snapshot& snapshot
        );

    using FrameReceivedCallback =
        void (*)(
            uint8_t orchestratorId,
            const Diagnostics::Frame& frame
        );

    using CommandStateCallback =
        void (*)(
            uint8_t orchestratorId,
            const Diagnostics::Command& command
        );

    // ========================================================
    // RECOVERY STATE
    // ========================================================

    struct RecoveryState
    {
        /*
        * Prossimo istante in cui tentare una nuova
        * inizializzazione.
        */
        unsigned long
            nextInitializationAttempt;

        /*
        * Istante in cui l'ultima inizializzazione
        * è stata completata.
        *
        * Serve a rilevare il caso in cui il transport
        * viene inizializzato ma non arriva mai la prima
        * risposta valida dal device.
        */
        unsigned long
            initializationCompletedAt;

        /*
        * Istante dell'ultima comunicazione realmente
        * completata con successo.
        *
        * Rimane zero fino alla prima risposta valida.
        */
        unsigned long
            lastSuccessfulCommunication;

        uint32_t
            initializationAttempts;

        uint32_t
            communicationRecoveries;

        RecoveryState()
            : nextInitializationAttempt(0),
            initializationCompletedAt(0),
            lastSuccessfulCommunication(0),
            initializationAttempts(0),
            communicationRecoveries(0)
        {
        }
    };

    // ========================================================
    // RUNTIME STATE
    // ========================================================

    struct RuntimeState
    {
        bool initialized;
        bool running;
        bool connected;

        unsigned long lastLoop;

        Snapshot snapshot;
        RecoveryState recovery;

        RuntimeState()
            : initialized(false),
              running(false),
              connected(false),
              lastLoop(0),
              snapshot(),
              recovery()
        {
        }
    };

private:

    // ========================================================
    // IDENTITY
    // ========================================================

    uint8_t
        orchestratorId;

    uint8_t
        eventSource;

    // ========================================================
    // SHARED REFERENCES
    //
    // Riferimenti non posseduti dall'orchestratore.
    // Devono rimanere validi per tutto il suo ciclo di vita.
    // ========================================================

    DomoManager*
        manager;

    DMNetworkManager*
        networkManager;

    // ========================================================
    // TCP CLIENT
    //
    // Il client viene assegnato dal SolarEngine.
    //
    // SolarOrchestrator non dipende da FrontendNetwork e può
    // quindi essere dichiarato in DMFrontendOrchestrators.hpp.
    //
    // Per un inverter seriale questo puntatore può essere null.
    // ========================================================

    DMPlatform::Network::TCPClient*
        tcpClient;

    // ========================================================
    // CONFIGURATION
    //
    // Ogni orchestratore gestisce un solo inverter.
    // ========================================================

    const FrontendConfig::Solar*
        configuration;

    // ========================================================
    // NETWORK REGISTRATION
    // ========================================================

    DMNetworkManager::ProtocolId
        protocolId;

    SocketManager::OwnerId
        socketOwner;

    // ========================================================
    // COMMUNICATION OBJECTS
    //
    // Ownership:
    //
    //   SolarOrchestrator
    //       -> ISolarInverter
    //       -> ISolarTransport
    //
    // L'inverter deve essere eliminato prima del transport.
    // ========================================================

    ISolarTransport*
        transport;

    ISolarInverter*
        inverter;

    // ========================================================
    // RUNTIME
    // ========================================================

    RuntimeState
        runtime;

    // ========================================================
    // FRONTEND CALLBACKS
    // ========================================================

    SnapshotChangedCallback
        snapshotChangedCallback;

    FrameReceivedCallback
        frameReceivedCallback;

    CommandStateCallback
        commandStateCallback;

    // ========================================================
    // CONSTANTS
    // ========================================================

    static constexpr unsigned long
        DEFAULT_RETRY_INTERVAL_MS =
            60000UL;

    static constexpr unsigned long
        COMMUNICATION_STALE_MS =
            30000UL;

public:

    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    SolarOrchestrator()
        : orchestratorId(0),
          eventSource(255),
          manager(nullptr),
          networkManager(nullptr),
          tcpClient(nullptr),
          configuration(nullptr),
          protocolId(-1),
          socketOwner(-1),
          transport(nullptr),
          inverter(nullptr),
          runtime(),
          snapshotChangedCallback(nullptr),
          frameReceivedCallback(nullptr),
          commandStateCallback(nullptr)
    {
    }

    // ========================================================
    // DESTRUCTOR
    // ========================================================

    ~SolarOrchestrator()
    {
        Cleanup();
    }

    // ========================================================
    // COPY DISABLED
    //
    // La classe possiede inverter e transport allocati
    // dinamicamente. La copia produrrebbe doppie distruzioni.
    // ========================================================

    SolarOrchestrator(
        const SolarOrchestrator&) =
            delete;

    SolarOrchestrator&
    operator=(
        const SolarOrchestrator&) =
            delete;

private:

    // ========================================================
    // SAFE NUMERIC CONVERSION
    // ========================================================

    static bool IsFinite(
        float value)
    {
        return
            !std::isnan(value) &&
            !std::isinf(value);
    }

    static long ScaleFloat(
        float value,
        float factor)
    {
        if (!IsFinite(value))
            return 0;

        return static_cast<long>(
            value * factor
        );
    }

    // ========================================================
    // DESTROY INVERTER
    // ========================================================

    void DestroyInverter()
    {
        if (!inverter)
            return;

        inverter->end();

        delete inverter;

        inverter =
            nullptr;
    }

    // ========================================================
    // DESTROY TRANSPORT
    // ========================================================

    void DestroyTransport()
    {
        if (!transport)
            return;

        delete transport;

        transport =
            nullptr;
    }

    // ========================================================
    // DESTROY COMMUNICATION OBJECTS
    //
    // Conserva:
    //
    //   - configurazione;
    //   - identificatore;
    //   - event source;
    //   - riferimenti esterni;
    //   - callback frontend;
    //   - contatori recovery.
    // ========================================================

    void DestroyCommunicationObjects()
    {
        DestroyInverter();
        DestroyTransport();

        runtime.initialized =
            false;

        runtime.connected =
            false;
    }

    // ========================================================
    // COMPLETE CLEANUP
    //
    // Azzera l'intera istanza ma non modifica le callback.
    //
    // Le callback vengono conservate durante un eventuale
    // nuovo Setup della stessa istanza.
    // ========================================================

    void Cleanup()
    {
        DestroyCommunicationObjects();

        runtime =
            RuntimeState();

        orchestratorId =
            0;

        eventSource =
            255;

        manager =
            nullptr;

        networkManager =
            nullptr;

        tcpClient =
            nullptr;

        configuration =
            nullptr;

        protocolId =
            -1;

        socketOwner =
            -1;
    }

    // ========================================================
    // CREATE TRANSPORT
    // ========================================================

    bool CreateTransport()
    {
        if (!configuration)
        {
            LOG_EF(
                "SOLAR",
                "Orchestrator %u configuration unavailable",
                orchestratorId
            );

            return false;
        }

        switch (configuration->transport)
        {
            // ------------------------------------------------
            // SERIAL PORT
            // ------------------------------------------------

            case FrontendConfig::Solar::
                TransportType::SERIAL_PORT:
            {
                if (!configuration->serial.port)
                {
                    LOG_EF(
                        "SOLAR",
                        "Orchestrator %u serial port unavailable",
                        orchestratorId
                    );

                    return false;
                }

                transport =
                    new SerialSolarTransport(
                        *configuration->serial.port,
                        configuration->serial.baudrate
                    );

                break;
            }

            // ------------------------------------------------
            // TCP CLIENT
            // ------------------------------------------------

            case FrontendConfig::Solar::
                TransportType::TCP_CLIENT:
            {
                if (!tcpClient)
                {
                    LOG_EF(
                        "SOLAR",
                        "Orchestrator %u TCP client unavailable",
                        orchestratorId
                    );

                    return false;
                }

                if (configuration->tcp.port == 0)
                {
                    LOG_EF(
                        "SOLAR",
                        "Orchestrator %u TCP port invalid",
                        orchestratorId
                    );

                    return false;
                }

                transport =
                    new TcpSolarTransport(
                        *tcpClient,
                        configuration->tcp.host,
                        configuration->tcp.port,
                        configuration->tcp.timeoutMs
                    );

                break;
            }

            // ------------------------------------------------
            // UDP CLIENT
            // ------------------------------------------------

            case FrontendConfig::Solar::
                TransportType::UDP_CLIENT:
            {
                LOG_WF(
                    "SOLAR",
                    "Orchestrator %u UDP transport not implemented",
                    orchestratorId
                );

                return false;
            }

            default:
            {
                LOG_EF(
                    "SOLAR",
                    "Orchestrator %u unknown transport type",
                    orchestratorId
                );

                return false;
            }
        }

        if (!transport)
        {
            LOG_EF(
                "SOLAR",
                "Orchestrator %u transport allocation failed",
                orchestratorId
            );

            return false;
        }

        return true;
    }

    // ========================================================
    // CREATE INVERTER
    //
    // ISolarInverter utilizza std::function.
    //
    // Le lambda catturano this e instradano le callback verso
    // la corretta istanza SolarOrchestrator.
    // ========================================================

    bool CreateInverter()
    {
        if (!transport)
        {
            LOG_EF(
                "SOLAR",
                "Orchestrator %u transport unavailable",
                orchestratorId
            );

            return false;
        }

        inverter =
            new QA6248Inverter(
                *transport,
                configuration
                    ? configuration->queryTimeoutMs
                    : 3000UL
            );

        if (!inverter)
        {
            LOG_EF(
                "SOLAR",
                "Orchestrator %u inverter allocation failed",
                orchestratorId
            );

            return false;
        }

        inverter->setSnapshotChangedCallback(
            [this](
                const Snapshot& oldSnapshot,
                const Snapshot& newSnapshot)
            {
                OnSnapshotChanged(
                    oldSnapshot,
                    newSnapshot
                );
            }
        );

        inverter->setFrameCallback(
            [this](
                const Diagnostics::Frame& frame)
            {
                OnFrameReceived(
                    frame
                );
            }
        );

        inverter->setCommandStateCallback(
            [this](
                const Diagnostics::Command& command)
            {
                OnCommandStateChanged(
                    command
                );
            }
        );

        return true;
    }

    // ========================================================
    // STATUS WORD
    // ========================================================

    static uint16_t BuildStatusWord(
        const Snapshot& snapshot)
    {
        uint16_t status =
            0;

        const auto& primary =
            snapshot.system.status.primary;

        const auto& extended =
            snapshot.system.status.extended;

        const auto& battery =
            snapshot.battery.status;

        if (primary.outputActive)
            status |= (1U << 0);

        if (primary.batteryLowAlarm)
            status |= (1U << 1);

        if (primary.batteryDisconnected)
            status |= (1U << 2);

        if (primary.overload)
            status |= (1U << 3);

        if (primary.overTemperature)
            status |= (1U << 4);

        if (primary.configurationChanged)
            status |= (1U << 5);

        if (primary.inverterOn)
            status |= (1U << 6);

        if (primary.pvPowerLow)
            status |= (1U << 7);

        if (primary.inputVoltageHigh)
            status |= (1U << 8);

        if (primary.batteryVoltageHigh)
            status |= (1U << 9);

        if (primary.fanFault)
            status |= (1U << 10);

        if (extended.dustproofInstalled)
            status |= (1U << 11);

        if (extended.switchOn)
            status |= (1U << 12);

        if (battery.chargeActive)
            status |= (1U << 13);

        if (battery.sccCharging)
            status |= (1U << 14);

        if (battery.acCharging)
            status |= (1U << 15);

        return status;
    }

    // ========================================================
    // PUBLISH FLOAT
    // ========================================================

    void PublishFloat(
        int area,
        float value,
        float factor)
    {
        if (!manager)
            return;

        if (!configuration)
            return;

        if (!configuration->publishEvents)
            return;

        if (area < 0)
            return;

        if (!IsFinite(value))
            return;

        manager->forceEvent(
            area,
            ScaleFloat(
                value,
                factor
            ),
            eventSource
        );
    }

    // ========================================================
    // PUBLISH AREA
    // ========================================================

    void PublishArea(
        int area,
        long value)
    {
        if (!manager)
            return;

        if (!configuration)
            return;

        if (!configuration->publishEvents)
            return;

        if (area < 0)
            return;

        manager->forceEvent(
            area,
            value,
            eventSource
        );
    }

    // ========================================================
    // ROUTE SNAPSHOT TO DOMOMANAGER
    // ========================================================

    void RouteSnapshot(
        const Snapshot& snapshot)
    {
        if (!configuration)
            return;

        const FrontendConfig::Solar::Areas& areas =
            configuration->areas;

        // ----------------------------------------------------
        // PV
        // ----------------------------------------------------

        if (snapshot.pv.valid)
        {
            PublishFloat(
                areas.pvVoltage,
                snapshot.pv.electrical.voltage,
                10.0f
            );

            PublishFloat(
                areas.pvCurrent,
                snapshot.pv.electrical.current,
                10.0f
            );

            PublishFloat(
                areas.pvPower,
                snapshot.pv.electrical.power,
                1.0f
            );
        }

        // ----------------------------------------------------
        // BATTERY
        // ----------------------------------------------------

        if (snapshot.battery.valid)
        {
            PublishFloat(
                areas.batteryVoltage,
                snapshot.battery.electrical.voltage,
                10.0f
            );

            if (snapshot.battery.capacity.soc >= 0)
            {
                PublishArea(
                    areas.batterySoc,
                    snapshot.battery.capacity.soc
                );
            }
        }

        // ----------------------------------------------------
        // GRID
        // ----------------------------------------------------

        if (snapshot.grid.valid)
        {
            PublishFloat(
                areas.gridVoltage,
                snapshot.grid.electrical.voltage,
                10.0f
            );

            PublishFloat(
                areas.gridPower,
                snapshot.grid.electrical.power,
                1.0f
            );
        }

        // ----------------------------------------------------
        // OUTPUT
        // ----------------------------------------------------

        if (snapshot.output.valid)
        {
            PublishFloat(
                areas.outputPower,
                snapshot.output.electrical.activePower,
                1.0f
            );

            PublishArea(
                areas.loadPercent,
                snapshot.output.load.percent
            );
        }

        // ----------------------------------------------------
        // INTERNAL
        // ----------------------------------------------------

        if (snapshot.internal.valid)
        {
            PublishFloat(
                areas.inverterTemperature,
                snapshot.internal.thermal
                    .heatsinkTemperature,
                10.0f
            );
        }

        // ----------------------------------------------------
        // STATUS
        // ----------------------------------------------------

        PublishArea(
            areas.status,
            BuildStatusWord(snapshot)
        );
    }

    // ========================================================
    // INTERNAL SNAPSHOT CALLBACK
    // ========================================================

    void OnSnapshotChanged(
        const Snapshot& oldSnapshot,
        const Snapshot& newSnapshot)
    {
        (void)oldSnapshot;

        runtime.snapshot =
            newSnapshot;

        runtime.connected =
            newSnapshot.system.connected;

        RouteSnapshot(
            newSnapshot
        );

        if (snapshotChangedCallback)
        {
            snapshotChangedCallback(
                orchestratorId,
                newSnapshot
            );
        }
    }

    // ========================================================
    // INTERNAL FRAME CALLBACK
    // ========================================================

    void OnFrameReceived(
        const Diagnostics::Frame& frame)
    {
        if (frameReceivedCallback)
        {
            frameReceivedCallback(
                orchestratorId,
                frame
            );
        }
    }

    // ========================================================
    // INTERNAL COMMAND CALLBACK
    // ========================================================

    void OnCommandStateChanged(
        const Diagnostics::Command& command)
    {
        if (commandStateCallback)
        {
            commandStateCallback(
                orchestratorId,
                command
            );
        }
    }

    // ========================================================
    // RETRY SCHEDULING
    // ========================================================

    void ScheduleRetry(
        unsigned long now)
    {
        static constexpr unsigned long
            MINIMUM_RETRY_INTERVAL_MS =
                30000UL;

        unsigned long retryInterval =
            DEFAULT_RETRY_INTERVAL_MS;

        if (configuration &&
            configuration->reconnectMs > 0)
        {
            retryInterval =
                configuration->reconnectMs;
        }

        if (retryInterval <
            MINIMUM_RETRY_INTERVAL_MS)
        {
            retryInterval =
                MINIMUM_RETRY_INTERVAL_MS;
        }

        runtime.recovery
            .nextInitializationAttempt =
                now + retryInterval;

        LOG_DF(
            "SOLAR",
            "[%u] retry scheduled in %lu ms",
            static_cast<unsigned>(
                orchestratorId
            ),
            retryInterval
        );
    }

    // ========================================================
    // TRY INITIALIZE
    // ========================================================

    bool TryInitialize(
        unsigned long now)
    {
        LOG_DF(
            "SOLAR",
            "[%u] init step 1: cleanup",
            static_cast<unsigned>(
                orchestratorId
            )
        );

        DestroyCommunicationObjects();

        runtime.recovery
            .initializationAttempts++;

        runtime.recovery
            .initializationCompletedAt =
                0;

        runtime.recovery
            .lastSuccessfulCommunication =
                0;

        LOG_DF(
            "SOLAR",
            "[%u] init step 2: create transport",
            static_cast<unsigned>(
                orchestratorId
            )
        );

        if (!CreateTransport())
        {
            LOG_EF(
                "SOLAR",
                "[%u] transport creation failed",
                static_cast<unsigned>(
                    orchestratorId
                )
            );

            ScheduleRetry(
                millis()
            );

            return false;
        }

        LOG_DF(
            "SOLAR",
            "[%u] init step 3: create inverter",
            static_cast<unsigned>(
                orchestratorId
            )
        );

        if (!CreateInverter())
        {
            LOG_EF(
                "SOLAR",
                "[%u] inverter creation failed",
                static_cast<unsigned>(
                    orchestratorId
                )
            );

            DestroyCommunicationObjects();

            ScheduleRetry(
                millis()
            );

            return false;
        }

        LOG_DF(
            "SOLAR",
            "[%u] init step 4: inverter begin",
            static_cast<unsigned>(
                orchestratorId
            )
        );

        const unsigned long beginStartedAt =
            millis();

        const bool beginResult =
            inverter->begin(
                beginStartedAt
            );

        const unsigned long beginCompletedAt =
            millis();

        const unsigned long beginDuration =
            beginCompletedAt -
            beginStartedAt;

        if (!beginResult)
        {
            LOG_WF(
                "SOLAR",
                "[%u] inverter begin failed "
                "duration=%lu ms",
                static_cast<unsigned>(
                    orchestratorId
                ),
                beginDuration
            );

            DestroyCommunicationObjects();

            runtime.recovery
                .initializationCompletedAt =
                    0;

            runtime.recovery
                .lastSuccessfulCommunication =
                    0;

            ScheduleRetry(
                beginCompletedAt
            );

            return false;
        }

        LOG_DF(
            "SOLAR",
            "[%u] init step 5: acquire state "
            "duration=%lu ms",
            static_cast<unsigned>(
                orchestratorId
            ),
            beginDuration
        );

        /*
        * L'infrastruttura è inizializzata, ma non abbiamo ancora
        * la prova di una comunicazione valida con l'inverter.
        */
        runtime.initialized =
            true;

        runtime.connected =
            false;

        runtime.snapshot =
            inverter->snapshot();

        runtime.lastLoop =
            beginCompletedAt;

        /*
        * Indica l'inizio della finestra entro la quale deve arrivare
        * la prima risposta valida.
        */
        runtime.recovery
            .initializationCompletedAt =
                beginCompletedAt;

        /*
        * Viene valorizzato esclusivamente dopo un refresh valido.
        */
        runtime.recovery
            .lastSuccessfulCommunication =
                0;

        runtime.recovery
            .nextInitializationAttempt =
                0;

        LOG_IF(
            "SOLAR",
            "[%u] initialization completed, "
            "waiting first valid communication",
            static_cast<unsigned>(
                orchestratorId
            )
        );

        return true;
    }

public:

    // ========================================================
    // SETUP
    //
    // Configura un singolo inverter.
    //
    // assignedTcpClient:
    //
    //   SERIAL_PORT -> nullptr ammesso
    //   TCP_CLIENT  -> puntatore obbligatorio
    //
    // SolarEngine assegna un client differente a ciascun
    // orchestratore TCP.
    // ========================================================

    bool Setup(
        uint8_t newOrchestratorId,
        DomoManager& dm,
        DMNetworkManager& netManager,
        DMPlatform::Network::TCPClient* assignedTcpClient,
        const FrontendConfig::Solar& cfg,
        DMNetworkManager::ProtocolId solarProtocolId,
        uint8_t solarEventSource)
    {
        Cleanup();

        orchestratorId =
            newOrchestratorId;

        eventSource =
            solarEventSource;

        manager =
            &dm;

        networkManager =
            &netManager;

        tcpClient =
            assignedTcpClient;

        configuration =
            &cfg;

        protocolId =
            solarProtocolId;

        socketOwner =
            protocolId >= 0
                ? networkManager
                    ->getProtocol(protocolId)
                    .socketOwner
                : -1;

        runtime =
            RuntimeState();

        if (!configuration->enabled)
        {
            LOG_IF(
                "SOLAR",
                "Orchestrator %u disabled",
                static_cast<unsigned>(
                    orchestratorId
                )
            );

            return true;
        }

        if (
            configuration->transport ==
                FrontendConfig::Solar::
                    TransportType::TCP_CLIENT &&
            !tcpClient)
        {
            LOG_EF(
                "SOLAR",
                "Orchestrator %u TCP selected "
                "without assigned client",
                static_cast<unsigned>(
                    orchestratorId
                )
            );

            return false;
        }

        runtime.running =
            true;

        runtime.initialized =
            false;

        runtime.connected =
            false;

        runtime.lastLoop =
            0;

        runtime.recovery =
            RecoveryState();

        /*
        * L'inizializzazione effettiva viene rinviata al Loop.
        *
        * Non eseguiamo qui:
        *
        *   CreateTransport()
        *   CreateInverter()
        *   inverter->begin()
        *   capability discovery
        *
        * Questo evita di aggiungere l'intero stack del protocollo
        * alla catena già profonda del setup generale del frontend.
        */
        runtime.recovery
            .nextInitializationAttempt =
                millis();

        LOG_IF(
            "SOLAR",
            "Orchestrator %u configured, "
            "initialization scheduled",
            static_cast<unsigned>(
                orchestratorId
            )
        );

        return true;
    }

    // ========================================================
    // LOOP
    //
    // Gestisce la macchina a stati del singolo inverter:
    //
    //   1. attesa inizializzazione/retry;
    //   2. polling non bloccante del protocollo;
    //   3. acquisizione della prima comunicazione valida;
    //   4. controllo comunicazione stale;
    //   5. distruzione e pianificazione riconnessione.
    //
    // runtime.connected rappresenta la comunicazione valida
    // con il protocollo, non la sola connessione TCP.
    // ========================================================

    void Loop(
        unsigned long now)
    {
        // ----------------------------------------------------
        // ENGINE NOT RUNNING
        // ----------------------------------------------------

        if (!runtime.running)
            return;

        // ----------------------------------------------------
        // INVALID OR DISABLED CONFIGURATION
        // ----------------------------------------------------

        if (!configuration ||
            !configuration->enabled)
        {
            return;
        }

        // ====================================================
        // INITIALIZATION / RETRY
        // ====================================================

        if (!runtime.initialized)
        {
            const unsigned long retryAt =
                runtime.recovery
                    .nextInitializationAttempt;

            /*
            * Il confronto con sottrazione signed è rollover-safe
            * purché gli intervalli siano inferiori a LONG_MAX.
            */
            const bool retryExpired =
                retryAt == 0 ||
                static_cast<long>(
                    now - retryAt
                ) >= 0;

            if (!retryExpired)
                return;

            /*
            * Viene azzerato prima del tentativo.
            *
            * Se TryInitialize() fallisce, ScheduleRetry()
            * programmerà il tentativo successivo.
            */
            runtime.recovery
                .nextInitializationAttempt =
                    0;

            LOG_DF(
                "SOLAR",
                "Orchestrator %u initialization started",
                static_cast<unsigned>(
                    orchestratorId
                )
            );

            TryInitialize(
                now
            );

            /*
            * Il tentativo di inizializzazione può distruggere o
            * ricreare transport e inverter. Il giro termina qui.
            */
            return;
        }

        // ====================================================
        // RUNTIME SAFETY
        // ====================================================

        if (!inverter)
        {
            LOG_WF(
                "SOLAR",
                "Orchestrator %u inverter unavailable",
                static_cast<unsigned>(
                    orchestratorId
                )
            );

            runtime.connected =
                false;

            DestroyCommunicationObjects();

            runtime.recovery
                .initializationCompletedAt =
                    0;

            ScheduleRetry(
                millis()
            );

            return;
        }

        // ====================================================
        // NON-BLOCKING PROTOCOL REFRESH
        // ====================================================

        const bool refreshed =
            inverter->refresh();

        /*
        * refresh() dovrebbe essere non bloccante, ma utilizziamo
        * comunque un timestamp aggiornato dopo la chiamata.
        */
        const unsigned long currentNow =
            millis();

        runtime.snapshot =
            inverter->snapshot();

        runtime.lastLoop =
            currentNow;

        // ====================================================
        // VALID COMMUNICATION
        // ====================================================

        if (refreshed)
        {
            /*
            * Solo una transazione completata con successo rende
            * valida la comunicazione a livello protocollo.
            *
            * Non viene utilizzato inverter->connected(), perché
            * nel transport TCP indica soltanto socket connessa.
            */
            runtime.connected =
                true;

            runtime.recovery
                .lastSuccessfulCommunication =
                    currentNow;

            /*
            * La prima comunicazione valida è arrivata.
            * La finestra iniziale non serve più.
            */
            runtime.recovery
                .initializationCompletedAt =
                    0;

            return;
        }

        // ====================================================
        // COMMUNICATION WATCHDOG
        // ====================================================

        const unsigned long initializationAt =
            runtime.recovery
                .initializationCompletedAt;

        const unsigned long lastSuccess =
            runtime.recovery
                .lastSuccessfulCommunication;

        // ----------------------------------------------------
        // FIRST COMMUNICATION TIMEOUT
        //
        // L'infrastruttura è stata inizializzata, ma non è mai
        // stata ricevuta una risposta valida dal protocollo.
        // ----------------------------------------------------

        const bool firstCommunicationTimeout =
            lastSuccess == 0 &&
            initializationAt != 0 &&
            static_cast<unsigned long>(
                currentNow - initializationAt
            ) >= COMMUNICATION_STALE_MS;

        // ----------------------------------------------------
        // ESTABLISHED COMMUNICATION TIMEOUT
        //
        // In precedenza era stata ricevuta almeno una risposta
        // valida, ma la comunicazione è diventata stale.
        // ----------------------------------------------------

        const bool establishedCommunicationTimeout =
            lastSuccess != 0 &&
            static_cast<unsigned long>(
                currentNow - lastSuccess
            ) >= COMMUNICATION_STALE_MS;

        /*
        * La comunicazione è ancora nella finestra ammessa.
        */
        if (!firstCommunicationTimeout &&
            !establishedCommunicationTimeout)
        {
            return;
        }

        // ====================================================
        // COMMUNICATION LOST
        // ====================================================

        runtime.connected =
            false;

        runtime.recovery
            .communicationRecoveries++;

        if (firstCommunicationTimeout)
        {
            LOG_WF(
                "SOLAR",
                "Orchestrator %u first communication timeout",
                static_cast<unsigned>(
                    orchestratorId
                )
            );
        }
        else
        {
            LOG_WF(
                "SOLAR",
                "Orchestrator %u communication lost",
                static_cast<unsigned>(
                    orchestratorId
                )
            );
        }

        /*
        * Elimina prima l'inverter e successivamente il transport.
        *
        * DestroyCommunicationObjects() imposta inoltre:
        *
        *   initialized = false
        *   connected   = false
        */
        DestroyCommunicationObjects();

        /*
        * Azzera entrambi i riferimenti temporali della sessione
        * appena terminata.
        */
        runtime.recovery
            .initializationCompletedAt =
                0;

        runtime.recovery
            .lastSuccessfulCommunication =
                0;

        /*
        * Usa il timestamp aggiornato dopo refresh(), non il valore
        * ricevuto come parametro all'inizio del giro.
        */
        ScheduleRetry(
            currentNow
        );
    }

    // ========================================================
    // DIAGNOSTIC REPORT
    // ========================================================

    void DiagnosticReport() const
    {
        Serial.println();

        Serial.print(
            "===== SOLAR ORCHESTRATOR "
        );

        Serial.print(
            orchestratorId
        );

        Serial.println(
            " ====="
        );

        Serial.print("Configured: ");
        Serial.println(
            configuration
                ? "YES"
                : "NO"
        );

        Serial.print("Enabled: ");
        Serial.println(
            IsEnabled()
                ? "YES"
                : "NO"
        );

        Serial.print("Running: ");
        Serial.println(
            runtime.running
                ? "YES"
                : "NO"
        );

        Serial.print("Initialized: ");
        Serial.println(
            runtime.initialized
                ? "YES"
                : "NO"
        );

        Serial.print("Connected: ");
        Serial.println(
            runtime.connected
                ? "YES"
                : "NO"
        );

        Serial.print("Transport: ");
        Serial.println(
            configuration
                ? static_cast<unsigned>(
                    configuration->transport
                )
                : 0U
        );

        Serial.print("Protocol ID: ");
        Serial.println(
            static_cast<int>(
                protocolId
            )
        );

        Serial.print("Socket owner: ");
        Serial.println(
            static_cast<int>(
                socketOwner
            )
        );

        Serial.print("Initialization attempts: ");
        Serial.println(
            runtime.recovery
                .initializationAttempts
        );

        Serial.print("Communication recoveries: ");
        Serial.println(
            runtime.recovery
                .communicationRecoveries
        );

        Serial.print("Last successful communication: ");
        Serial.println(
            runtime.recovery
                .lastSuccessfulCommunication
        );

        Serial.print("Next initialization attempt: ");
        Serial.println(
            runtime.recovery
                .nextInitializationAttempt
        );

        if (!inverter)
        {
            Serial.println(
                "Inverter: NOT AVAILABLE"
            );

            Serial.println(
                "===== END SOLAR ORCHESTRATOR ====="
            );

            return;
        }

        const Snapshot& snapshot =
            inverter->snapshot();

        const Diagnostics& diagnostics =
            inverter->diagnostics();

        // ----------------------------------------------------
        // INVERTER
        // ----------------------------------------------------

        Serial.println();
        Serial.println(
            "----- INVERTER -----"
        );

        Serial.print("Protocol: ");
        Serial.println(
            inverter->protocolName()
        );

        Serial.print("Model: ");
        Serial.println(
            inverter->modelName()
        );

        Serial.print("Mode: ");
        Serial.println(
            snapshot.system.operating.modeCode
                ? snapshot.system.operating.modeCode
                : '-'
        );

        Serial.print("Error code: ");
        Serial.println(
            snapshot.system.operating.errorCode
        );

        // ----------------------------------------------------
        // COMMUNICATION
        // ----------------------------------------------------

        Serial.println();
        Serial.println(
            "----- COMMUNICATION -----"
        );

        Serial.print("Queries: ");
        Serial.println(
            diagnostics.totalQueries
        );

        Serial.print("Success: ");
        Serial.println(
            diagnostics.successfulQueries
        );

        Serial.print("Failed: ");
        Serial.println(
            diagnostics.failedQueries
        );

        Serial.print("NAK: ");
        Serial.println(
            diagnostics.nakResponses
        );

        Serial.print("Timeout: ");
        Serial.println(
            diagnostics.timeoutResponses
        );

        Serial.print("Transport errors: ");
        Serial.println(
            diagnostics.transportErrors
        );

        Serial.print("Invalid responses: ");
        Serial.println(
            diagnostics.invalidResponses
        );

        Serial.print("Parse errors: ");
        Serial.println(
            diagnostics.parseErrors
        );

        Serial.print("Supported commands: ");
        Serial.println(
            diagnostics.supportedCommands
        );

        Serial.print("Unsupported commands: ");
        Serial.println(
            diagnostics.unsupportedCommands
        );

        // ----------------------------------------------------
        // PV
        // ----------------------------------------------------

        Serial.println();
        Serial.println(
            "----- PV -----"
        );

        Serial.print("Valid: ");
        Serial.println(
            snapshot.pv.valid
                ? "YES"
                : "NO"
        );

        Serial.print("Voltage: ");
        Serial.println(
            snapshot.pv.electrical.voltage,
            1
        );

        Serial.print("Current: ");
        Serial.println(
            snapshot.pv.electrical.current,
            1
        );

        Serial.print("Power: ");
        Serial.println(
            snapshot.pv.electrical.power,
            1
        );

        // ----------------------------------------------------
        // BATTERY
        // ----------------------------------------------------

        Serial.println();
        Serial.println(
            "----- BATTERY -----"
        );

        Serial.print("Valid: ");
        Serial.println(
            snapshot.battery.valid
                ? "YES"
                : "NO"
        );

        Serial.print("Connected: ");
        Serial.println(
            snapshot.battery.connected
                ? "YES"
                : "NO"
        );

        Serial.print("Voltage: ");
        Serial.println(
            snapshot.battery.electrical.voltage,
            1
        );

        Serial.print("Current: ");
        Serial.println(
            snapshot.battery.electrical.current,
            1
        );

        Serial.print("SOC: ");
        Serial.println(
            snapshot.battery.capacity.soc
        );

        // ----------------------------------------------------
        // GRID
        // ----------------------------------------------------

        Serial.println();
        Serial.println(
            "----- GRID -----"
        );

        Serial.print("Valid: ");
        Serial.println(
            snapshot.grid.valid
                ? "YES"
                : "NO"
        );

        Serial.print("Available: ");
        Serial.println(
            snapshot.grid.available
                ? "YES"
                : "NO"
        );

        Serial.print("Voltage: ");
        Serial.println(
            snapshot.grid.electrical.voltage,
            1
        );

        Serial.print("Frequency: ");
        Serial.println(
            snapshot.grid.electrical.frequency,
            1
        );

        Serial.print("Power: ");
        Serial.println(
            snapshot.grid.electrical.power,
            1
        );

        // ----------------------------------------------------
        // OUTPUT
        // ----------------------------------------------------

        Serial.println();
        Serial.println(
            "----- OUTPUT -----"
        );

        Serial.print("Valid: ");
        Serial.println(
            snapshot.output.valid
                ? "YES"
                : "NO"
        );

        Serial.print("Voltage: ");
        Serial.println(
            snapshot.output.electrical.voltage,
            1
        );

        Serial.print("Frequency: ");
        Serial.println(
            snapshot.output.electrical.frequency,
            1
        );

        Serial.print("Active power: ");
        Serial.println(
            snapshot.output.electrical.activePower,
            1
        );

        Serial.print("Load: ");
        Serial.print(
            snapshot.output.load.percent
        );
        Serial.println("%");

        // ----------------------------------------------------
        // INTERNAL
        // ----------------------------------------------------

        Serial.println();
        Serial.println(
            "----- INTERNAL -----"
        );

        Serial.print("Valid: ");
        Serial.println(
            snapshot.internal.valid
                ? "YES"
                : "NO"
        );

        Serial.print("Heatsink temperature: ");
        Serial.println(
            snapshot.internal.thermal
                .heatsinkTemperature,
            1
        );

        Serial.print("Status word: ");
        Serial.println(
            BuildStatusWord(snapshot)
        );

        Serial.println(
            "===== END SOLAR ORCHESTRATOR ====="
        );
    }

    // ========================================================
    // SHUTDOWN
    // ========================================================

    void Shutdown()
    {
        Cleanup();

        snapshotChangedCallback =
            nullptr;

        frameReceivedCallback =
            nullptr;

        commandStateCallback =
            nullptr;
    }

    // ========================================================
    // STATUS
    // ========================================================

    uint8_t GetId() const
    {
        return orchestratorId;
    }

    bool IsConfigured() const
    {
        return
            configuration != nullptr;
    }

    bool IsEnabled() const
    {
        return
            configuration &&
            configuration->enabled;
    }

    bool IsInitialized() const
    {
        return
            runtime.initialized;
    }

    bool IsRunning() const
    {
        return
            runtime.running;
    }

    bool IsConnected() const
    {
        return
            runtime.connected;
    }

    // ========================================================
    // ACCESSORS
    // ========================================================

    const FrontendConfig::Solar*
    GetConfiguration() const
    {
        return configuration;
    }

    const RuntimeState&
    GetRuntime() const
    {
        return runtime;
    }

    ISolarTransport*
    GetTransport()
    {
        return transport;
    }

    const ISolarTransport*
    GetTransport() const
    {
        return transport;
    }

    ISolarInverter*
    GetInstance()
    {
        return inverter;
    }

    const ISolarInverter*
    GetInstance() const
    {
        return inverter;
    }

    const Snapshot&
    GetSnapshot() const
    {
        static const Snapshot
            emptySnapshot {};

        return inverter
            ? inverter->snapshot()
            : emptySnapshot;
    }

    const Diagnostics&
    GetDiagnostics() const
    {
        static const Diagnostics
            emptyDiagnostics {};

        return inverter
            ? inverter->diagnostics()
            : emptyDiagnostics;
    }

    // ========================================================
    // RAW COMMAND
    // ========================================================

    bool SendRawCommand(
        const String& command,
        String& response)
    {
        if (!runtime.running)
            return false;

        if (!runtime.initialized)
            return false;

        if (!inverter)
            return false;

        return inverter->rawCommand(
            command,
            response
        );
    }

    // ========================================================
    // FRONTEND CALLBACK SETUP
    // ========================================================

    void SetSnapshotChangedCallback(
        SnapshotChangedCallback callback)
    {
        snapshotChangedCallback =
            callback;
    }

    void SetFrameReceivedCallback(
        FrameReceivedCallback callback)
    {
        frameReceivedCallback =
            callback;
    }

    void SetCommandStateCallback(
        CommandStateCallback callback)
    {
        commandStateCallback =
            callback;
    }
};

// ============================================================
//  TASK ENGINE (BACKEND / ORCHESTRATOR)
// ============================================================

class TaskEngineOrchestrator
{
public:

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================

    using TaskFn = void (*)(DomoManager&, unsigned long);

    static constexpr uint16_t STORAGE_MAX_TASKS = 32;

    // Limite dinamico:
    //
    //   1..2 task  -> 1 task/ciclo
    //   3..4       -> 2
    //   5..6       -> 3
    //   7..8       -> 4
    //   >8        -> massimo 4
    //
    // In questo modo la crescita dei task non porta ad una
    // crescita indiscriminata del lavoro eseguito nello stesso
    // passaggio.
    static constexpr uint16_t MIN_TASKS_PER_CYCLE = 1;
    static constexpr uint16_t MAX_TASKS_PER_CYCLE = 3;

    struct Task
    {
        TaskFn   fn       = nullptr;
        uint32_t interval = 0;
        uint32_t lastRun  = 0;
        bool     enabled  = false;
    };

private:

    static inline bool frontendCycleDone = false;

    /*
     * Mantengo il vector perché il tuo codice attuale lo usa già.
     *
     * IMPORTANTE:
     * - nessuna push_back durante Loop()
     * - AddTask() viene usato in Setup()
     * - il Loop() non alloca memoria
     */
    static inline std::vector<Task> tasks;

    static inline const FrontendConfig* cfg = nullptr;

    // Tie-break rotazionale tra task con la stessa urgenza.
    static inline uint16_t rrCursor = 0;

public:

    // ============================================================
    // SETUP
    // ============================================================

    static void Setup(const FrontendConfig& c)
    {
        cfg = &c;

        Clear();

        // Evita riallocazioni durante AddTask().
        if (tasks.capacity() < STORAGE_MAX_TASKS)
            tasks.reserve(STORAGE_MAX_TASKS);
    }

    // ============================================================
    // ADD TASK
    // ============================================================

    static void AddTask(
        TaskFn fn,
        uint32_t intervalMs,
        bool enabled)
    {
        if (!fn)
        {
            LOG_WF(
                "TASK-LOOP",
                "AddTask: callback nullo"
            );

            return;
        }

        if (tasks.size() >= STORAGE_MAX_TASKS)
        {
            LOG_WF(
                "TASK-LOOP",
                "Numero massimo task superato: %u/%u",
                static_cast<unsigned>(tasks.size()),
                static_cast<unsigned>(STORAGE_MAX_TASKS)
            );

            return;
        }

        tasks.push_back({
            fn,
            intervalMs,
            0,
            enabled
        });
    }

    // ============================================================
    // CLEAR
    // ============================================================

    static void Clear()
    {
        tasks.clear();

        frontendCycleDone = false;
        rrCursor = 0;
    }

    // ============================================================
    // LOOP
    // ============================================================

    static void Loop(
        DomoManager& manager,
        unsigned long now)
    {
        frontendCycleDone = false;

        const uint16_t count =
            static_cast<uint16_t>(tasks.size());

        if (count == 0)
        {
            frontendCycleDone = true;
            return;
        }

        /*
         * Numero massimo di task da eseguire in questo passaggio.
         *
         * Nessuna funzione costosa: semplice aritmetica.
         */
        uint16_t maxTasks =
            static_cast<uint16_t>((count + 1u) >> 1);

        if (maxTasks < MIN_TASKS_PER_CYCLE)
            maxTasks = MIN_TASKS_PER_CYCLE;

        if (maxTasks > MAX_TASKS_PER_CYCLE)
            maxTasks = MAX_TASKS_PER_CYCLE;

        if (maxTasks > count)
            maxTasks = count;

        uint16_t executed = 0;

        /*
         * Per evitare di rieseguire lo stesso task nel medesimo
         * Loop(), manteniamo una piccola bitmap locale.
         *
         * 32 task massimi -> uint32_t.
         *
         * Nessuna allocazione.
         */
        uint32_t selectedMask = 0;

        // ========================================================
        // SELEZIONE TASK PIÙ URGENTI
        // ========================================================

        while (executed < maxTasks)
        {
            int bestIndex = -1;
            uint32_t bestLateness = 0;
            uint16_t bestDistance = UINT16_MAX;

            /*
             * Scansione completa dei task.
             *
             * Il costo massimo è:
             *
             *   MAX_TASKS_PER_CYCLE * taskCount
             *
             * Con 4 * 8 = 32 confronti nel tuo caso.
             * È trascurabile rispetto al lavoro dei task reali.
             */
            for (uint16_t i = 0; i < count; ++i)
            {
                // Già selezionato in questo passaggio.
                if (selectedMask & (1UL << i))
                    continue;

                const Task& t = tasks[i];

                if (!t.enabled || !t.fn)
                    continue;

                /*
                 * unsigned subtraction:
                 * corretta anche in caso di rollover di millis().
                 */
                const uint32_t elapsed =
                    static_cast<uint32_t>(now - t.lastRun);

                /*
                 * Task non ancora scaduto.
                 */
                if (elapsed < t.interval)
                    continue;

                /*
                 * Urgenza = quanto siamo oltre la scadenza.
                 *
                 * interval 0:
                 * task sempre pronto.
                 */
                const uint32_t lateness =
                    elapsed - t.interval;

                /*
                 * A parità di lateness usiamo il cursore
                 * rotazionale.
                 */
                uint16_t distance;

                if (i >= rrCursor)
                    distance = i - rrCursor;
                else
                    distance =
                        static_cast<uint16_t>(
                            count - rrCursor + i
                        );

                if (
                    bestIndex < 0 ||
                    lateness > bestLateness ||
                    (
                        lateness == bestLateness &&
                        distance < bestDistance
                    )
                )
                {
                    bestIndex = i;
                    bestLateness = lateness;
                    bestDistance = distance;
                }
            }

            // Nessun task pronto.
            if (bestIndex < 0)
                break;

            Task& task = tasks[bestIndex];

            /*
             * IMPORTANTISSIMO:
             *
             * aggiorniamo lastRun PRIMA della callback.
             *
             * Così se il task genera un evento che porta
             * nuovamente dentro il sistema, non viene considerato
             * immediatamente nuovamente scaduto.
             */
            task.lastRun = static_cast<uint32_t>(now);

            selectedMask |=
                (1UL << bestIndex);

            ++executed;

            /*
             * Esecuzione vera.
             */
            task.fn(manager, now);

            /*
             * Il prossimo tie-break parte dal task successivo.
             *
             * Questo evita starvation in caso di pari priorità/
             * pari lateness.
             */
            rrCursor =
                static_cast<uint16_t>(
                    bestIndex + 1
                );

            if (rrCursor >= count)
                rrCursor = 0;
        }

        frontendCycleDone = true;
    }

    // ============================================================
    // DIAGNOSTICA / ACCESSO
    // ============================================================

    static bool hasFrontendCycleCompleted()
    {
        return frontendCycleDone;
    }

    static void resetFrontendCycleFlag()
    {
        frontendCycleDone = false;
    }

    static const FrontendConfig& getCfg()
    {
        return *cfg;
    }

    static uint16_t getTaskCount()
    {
        return static_cast<uint16_t>(tasks.size());
    }

    static uint16_t getMaxTasksPerCycle()
    {
        const uint16_t count =
            static_cast<uint16_t>(tasks.size());

        if (count == 0)
            return 0;

        uint16_t result =
            static_cast<uint16_t>((count + 1u) >> 1);

        if (result < MIN_TASKS_PER_CYCLE)
            result = MIN_TASKS_PER_CYCLE;

        if (result > MAX_TASKS_PER_CYCLE)
            result = MAX_TASKS_PER_CYCLE;

        if (result > count)
            result = count;

        return result;
    }
};

#endif
