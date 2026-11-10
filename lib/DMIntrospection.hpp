
// ============================================================================
// DMIntrospection.hpp
// ============================================================================
//
// INTROSPECTION DEL SISTEMA DOMOMANAGER
//
// Principio:
//   DomoManager possiede la configurazione.
//   DomoIntrospection NON duplica la configurazione.
//
//   Costruisce solamente:
//
//      - viste delle aree
//      - associazioni area <-> device
//      - relazioni route / toggle / split
//      - riferimenti alle automation
//      - correlazioni AEE
//      - correlazioni MQTT
//      - correlazioni WebAPI
//      - stato di validazione
//
// Utilizzo previsto:
//
//      DMRuntime
//          |
//          +--> DomoIntrospection
//                    |
//                    +--> Diagnostics
//                    +--> MQTT / HA
//                    +--> AEE
//                    +--> WebAPI
//                    +--> HMI
//
// ============================================================================

#pragma once


class DomoIntrospection
{
public:
    // ========================================================================
    // PHYSICAL I/O TYPE
    // ========================================================================
    //
    // Descrive il ruolo fisico dell'area all'interno del profilo device.
    //
    // Il valore viene ricavato strutturalmente da:
    //
    //     DeviceConfig.profile
    //         -> DeviceProfiles
    //             -> GenericPrgDeviceChannel.type
    //
    // Non viene dedotto:
    //
    //     - dalla label
    //     - dal numero area
    //     - dai prefissi testuali "DI:" / "DO:"
    //     - dal tipo semantico Home Assistant
    //
    // ========================================================================

    enum class IOType
    {
        UNKNOWN,

        DIGITAL_INPUT,
        DIGITAL_OUTPUT,

        ANALOG_INPUT,
        ANALOG_OUTPUT,

        VIRTUAL
    };

    enum class EntityGroup : uint8_t
    {
        ROOT = 0,
        PHYSICAL_DEVICE,
        SECURITY,
        SOLAR
    };

    // ========================================================================
    // ENTITY KIND
    // ========================================================================

    enum class EntityKind
    {
        UNKNOWN,

        SENSOR,
        BINARY_SENSOR,
        SWITCH,
        LIGHT,
        COVER,
        CLIMATE,
        BUTTON,
        NUMBER,
        TEXT,
        STATUS
    };


    // ========================================================================
    // ENTITY ORIGIN
    // ========================================================================

    enum class EntityOrigin
    {
        AREA,
        DERIVED,
        SECURITY,
        HVAC,
        AVERAGE,
        SOLAR,
        POWER,
        AEE,
        RUNTIME,
        EXTERNAL_SOURCE
    };


    // ========================================================================
    // RELATION TYPE
    // ========================================================================

    enum class RelationType
    {
        ROUTE,
        TOGGLE,
        SPLIT,

        AUTOMATION_INPUT,
        AUTOMATION_OUTPUT,

        AEE_AREA,
        MQTT_AREA,
        WEBAPI_AEE
    };


    // ========================================================================
    // AUTOMATION TYPE
    // ========================================================================

    enum class AutomationType
    {
        SCENE,
        RULE,
        SEQUENCE
    };


    // ========================================================================
    // CONTEXT
    // ========================================================================
    //
    // Tutti i puntatori sono riferimenti agli oggetti già esistenti.
    //
    // Non vengono copiati dati di configurazione.
    //
    // ========================================================================

    struct Context
    {
        // --------------------------------------------------------------------
        // DOMOMANAGER
        // --------------------------------------------------------------------

        const DomoManagerConfig*
            domo = nullptr;


        // --------------------------------------------------------------------
        // DEVICE PROFILES
        // --------------------------------------------------------------------
        //
        // Deve puntare alla stessa istanza utilizzata dal runtime Modbus.
        //
        // In questo modo l'introspection vede anche gli eventuali profili
        // custom registrati durante l'inizializzazione.
        //
        // --------------------------------------------------------------------

        const DeviceProfiles*
            deviceProfiles = nullptr;


        // --------------------------------------------------------------------
        // AUTOMATION
        // --------------------------------------------------------------------

        AutomationEngine*
            automationEngine = nullptr;


        // --------------------------------------------------------------------
        // AEE
        // --------------------------------------------------------------------
        //
        // NON const: WebAPI e altri consumer possono utilizzare find().
        //
        // --------------------------------------------------------------------

        AEERegistry*
            aeeRegistry = nullptr;


        // --------------------------------------------------------------------
        // RUNTIME
        // --------------------------------------------------------------------

        Buffer*
            buffer = nullptr;

        DMRuntimeStore*
            runtimeStore = nullptr;

        AverageCalculator*
            averages = nullptr;

        TimeManager*
            time = nullptr;


        // --------------------------------------------------------------------
        // MQTT
        // --------------------------------------------------------------------

        const FrontendConfig::MQTT*
            mqtt = nullptr;


        // --------------------------------------------------------------------
        // OTHERS
        // --------------------------------------------------------------------

        const FrontendConfig::webApiDeviceMessaging*
            webApi = nullptr;

        const FrontendConfig::HVAC* hvac = nullptr;

        const FrontendConfig::Averages*
            averagesConfig = nullptr;

        const FrontendConfig::Solar*
            solar = nullptr;

    };


    // ========================================================================
    // ENTITY
    // ========================================================================
    //
    // Vista semantica di un'area.
    //
    // I dati originali restano in AreaConfig / DeviceConfig.
    //
    // ========================================================================
    struct ClimateInfo
    {
        int setpointArea = -1;

        int temperatureArea = -1;

        int humidityArea = -1;

        int fanCoilId = -1;
    };

    struct NumberConfig
    {
        enum class Mode
        {
            AUTO,
            BOX,
            SLIDER
        };

        bool enabled = false;

        float minimumValue = 0.0f;
        float maximumValue = 100.0f;

        float stepValue = 1.0f;

        float scale = 1.0f;

        uint8_t displayDecimals = 0;

        Mode mode = Mode::BOX;

        bool valid() const
        {
            return
                maximumValue >= minimumValue &&
                stepValue > 0.0f &&
                scale > 0.0f;
        }
    };

    struct Entity
    {
        // --------------------------------------------------------------------
        // IDENTITÀ
        // --------------------------------------------------------------------

        int area = -1;

        EntityKind kind =
            EntityKind::UNKNOWN;

        EntityOrigin origin =
            EntityOrigin::AREA;

        EntityGroup group =
            EntityGroup::ROOT;

        // --------------------------------------------------------------------
        // TIPO FISICO
        // --------------------------------------------------------------------
        //
        // Indica il ruolo fisico del canale da cui deriva l'area:
        //
        //   DI -> DIGITAL_INPUT
        //   DO -> DIGITAL_OUTPUT
        //   AI -> ANALOG_INPUT
        //   AO -> ANALOG_OUTPUT
        //
        // Le aree senza device fisico possono essere marcate VIRTUAL.
        //
        // UNKNOWN indica che l'introspection non possiede ancora elementi
        // sufficienti per determinare il tipo fisico.
        //
        // --------------------------------------------------------------------

        IOType ioType =
            IOType::UNKNOWN;


        // --------------------------------------------------------------------
        // RIFERIMENTI ORIGINALI
        // --------------------------------------------------------------------

        const DomoManagerBufferEngine::AreaConfig*
            areaCfg = nullptr;

        const DomoManagerConfig::Devices::Device*
            device = nullptr;

        int deviceIndex = -1;

        DMRuntimeStore::Handle
            runtimeHandle =
                DMRuntimeStore::INVALID_HANDLE;

        const DMRuntimeStore::Entry*
            runtimeEntry =
                nullptr;

        // --------------------------------------------------------------------
        // POSIZIONE NEL DEVICE
        // --------------------------------------------------------------------
        //
        // Indice dell'area all'interno dell'array areas[] del device.
        //
        // Esempio:
        //
        //   MA01_AXCX4020
        //
        //   areas[0..3] -> DI
        //   areas[4..5] -> DO
        //
        // Serve per correlare deterministicamente l'area con il profilo.
        //
        // --------------------------------------------------------------------

        int deviceAreaIndex = -1;


        // --------------------------------------------------------------------
        // SEMANTICA
        // --------------------------------------------------------------------

        const char* unit = nullptr;

        const char* deviceClass = nullptr;

        const char* stateClass = nullptr;


        // --------------------------------------------------------------------
        // ACCESSO
        // --------------------------------------------------------------------

        bool readable = true;

        bool writable = false;


        // --------------------------------------------------------------------
        // VISIBILITÀ
        // --------------------------------------------------------------------

        bool implementationOnly = false;


        // --------------------------------------------------------------------
        // RELAZIONI
        // --------------------------------------------------------------------

        bool isRouteTrigger = false;
        bool isRouteTarget = false;

        bool isToggleSource = false;
        bool isToggleTarget = false;

        bool isSplitMain = false;
        bool isSplitTarget = false;

        bool isDerivedOutput = false;

        bool isAutomationInput = false;
        bool isAutomationOutput = false;


        // --------------------------------------------------------------------
        // RUOLO STRUTTURALE
        // --------------------------------------------------------------------

        bool isTechnicalState = false;


        // --------------------------------------------------------------------
        // INTEGRAZIONI
        // --------------------------------------------------------------------

        bool hasAEE = false;
        bool hasMQTT = false;
        bool hasWebAPI = false;


        // --------------------------------------------------------------------
        // SORGENTI SPECIALISTICHE
        // --------------------------------------------------------------------

        bool fromSecurity = false;
        bool fromHVAC = false;
        bool fromAverage = false;
        bool fromPower = false;
        bool fromSolar = false;

        // --------------------------------------------------------------------
        // SCALING
        // --------------------------------------------------------------------

        float scale = 1.0f;
        NumberConfig number;


        ClimateInfo climate;

        // --------------------------------------------------------------------
        // HELPERS
        // --------------------------------------------------------------------

        bool hasAreaConfig() const
        {
            return areaCfg != nullptr;
        }


        bool hasDevice() const
        {
            return device != nullptr;
        }


        bool isInput() const
        {
            return
                ioType == IOType::DIGITAL_INPUT ||
                ioType == IOType::ANALOG_INPUT;
        }


        bool isOutput() const
        {
            return
                ioType == IOType::DIGITAL_OUTPUT ||
                ioType == IOType::ANALOG_OUTPUT;
        }


        bool isDigitalInput() const
        {
            return
                ioType == IOType::DIGITAL_INPUT;
        }


        bool isDigitalOutput() const
        {
            return
                ioType == IOType::DIGITAL_OUTPUT;
        }


        bool isAnalogInput() const
        {
            return
                ioType == IOType::ANALOG_INPUT;
        }


        bool isAnalogOutput() const
        {
            return
                ioType == IOType::ANALOG_OUTPUT;
        }


        bool isVirtual() const
        {
            return
                ioType == IOType::VIRTUAL;
        }
    };

    // ========================================================================
    // RELATION
    // ========================================================================

    struct Relation
    {
        RelationType type =
            RelationType::ROUTE;


        // --------------------------------------------------------------------
        // AREE
        // --------------------------------------------------------------------

        int sourceArea = -1;

        int targetArea = -1;


        // --------------------------------------------------------------------
        // DESCRIZIONE
        // --------------------------------------------------------------------

        const char* name = nullptr;


        // --------------------------------------------------------------------
        // ROUTE
        // --------------------------------------------------------------------

        int value = 0;


        // --------------------------------------------------------------------
        // GENERIC INDEX
        // --------------------------------------------------------------------

        int index = -1;


        // --------------------------------------------------------------------
        // SPLIT / SEQUENCE
        // --------------------------------------------------------------------

        unsigned long duration = 0;


        // --------------------------------------------------------------------
        // AEE
        // --------------------------------------------------------------------

        AEEVariableBase* aee = nullptr;


        // --------------------------------------------------------------------
        // MQTT
        // --------------------------------------------------------------------

        const FrontendConfig::MQTT::Device*
            mqttDevice = nullptr;

        const FrontendConfig::MQTT::Mapping*
            mqttMapping = nullptr;


        // --------------------------------------------------------------------
        // WEB API
        // --------------------------------------------------------------------

        const DeviceMessageProfile*
            webApiProfile = nullptr;

        const WebApiDevice*
            webApiDevice = nullptr;
    };


    // ========================================================================
    // AUTOMATION
    // ========================================================================

    struct Automation
    {
        AutomationType type =
            AutomationType::RULE;


        // Nome:
        //
        // Scene / Rule hanno nome.
        // Sequence attualmente non ha nome.
        //
        const char* name = nullptr;


        // Riferimenti originali
        const AutomationBuilder::SceneCfg*
            scene = nullptr;

        const AutomationBuilder::RuleCfg*
            rule = nullptr;

        const AutomationBuilder::SequenceCfg*
            sequence = nullptr;


        size_t inputCount = 0;

        size_t outputCount = 0;

        bool valid = true;
    };


    // ========================================================================
    // VALIDATION
    // ========================================================================

    struct Validation
    {
        size_t entities = 0;

        size_t describedEntities = 0;

        size_t hardwareOnlyEntities = 0;

        size_t implementationOnlyEntities = 0;

        size_t writableEntities = 0;


        size_t routeRelations = 0;
        size_t toggleRelations = 0;
        size_t splitRelations = 0;

        size_t automationInputRelations = 0;
        size_t automationOutputRelations = 0;

        size_t aeeBindings = 0;
        size_t mqttBindings = 0;
        size_t webApiBindings = 0;


        size_t unknownEntities = 0;


        size_t invalidRouteRelations = 0;
        size_t invalidToggleRelations = 0;
        size_t invalidSplitRelations = 0;

        size_t invalidAutomationAreas = 0;
        size_t invalidAutomationScenes = 0;

        size_t invalidMQTTMappings = 0;
        size_t invalidWebAPIMappings = 0;


        bool ok() const
        {
            return
                invalidRouteRelations == 0 &&
                invalidToggleRelations == 0 &&
                invalidSplitRelations == 0 &&
                invalidAutomationAreas == 0 &&
                invalidAutomationScenes == 0 &&
                invalidMQTTMappings == 0 &&
                invalidWebAPIMappings == 0;
        }
    };


    // ========================================================================
    // COSTRUTTORE
    // ========================================================================

    DomoIntrospection() = default;


    // ========================================================================
    // BUILD COMPLETO
    // ========================================================================
    //
    // Unico percorso consentito per costruire DomoIntrospection.
    //
    // Dipendenze obbligatorie:
    //
    //   - DomoManagerConfig
    //   - DeviceProfiles
    //
    // Dipendenze opzionali:
    //
    //   - AutomationEngine
    //   - AEERegistry
    //   - Buffer
    //   - AverageCalculator
    //   - TimeManager
    //   - MQTT
    //   - WebAPI
    //
    // ========================================================================

    void build(
        const Context& ctx)
    {
        // --------------------------------------------------------------------
        // RESET
        // --------------------------------------------------------------------

        clear();

        _ctx =
            ctx;


        // --------------------------------------------------------------------
        // REQUIRED: DOMOMANAGER CONFIG
        // --------------------------------------------------------------------

        if (!_ctx.domo)
        {
            LOG_E(
                "DomoIntrospection",
                "Build FULL fallita: domo == nullptr"
            );

            return;
        }


        // --------------------------------------------------------------------
        // REQUIRED: DEVICE PROFILES
        // --------------------------------------------------------------------

        if (!_ctx.deviceProfiles)
        {
            LOG_E(
                "DomoIntrospection",
                "Build FULL fallita: deviceProfiles == nullptr"
            );

            return;
        }


        LOG_IF(
            "DomoIntrospection",
            "Build FULL avviata: devices=%u",
            static_cast<unsigned>(
                _ctx.domo->devices.list.size()
            )
        );


        // --------------------------------------------------------------------
        // CORE DOMOMANAGER
        // --------------------------------------------------------------------

        inspectAreas(
            _ctx.domo->areas
        );

        inspectDevices(
            _ctx.domo->devices,
            *_ctx.deviceProfiles
        );

        inspectRoutes(
            _ctx.domo->routes
        );

        inspectToggles(
            _ctx.domo->toggles
        );

        inspectSplits(
            _ctx.domo->splits
        );


        // --------------------------------------------------------------------
        // AUTOMATION
        // --------------------------------------------------------------------

        if (_ctx.automationEngine)
        {
            inspectAutomationEngine(
                *_ctx.automationEngine
            );
        }


        // --------------------------------------------------------------------
        // AEE
        // --------------------------------------------------------------------

        if (_ctx.aeeRegistry)
        {
            inspectAEE(
                *_ctx.aeeRegistry
            );
        }


        // --------------------------------------------------------------------
        // MQTT
        // --------------------------------------------------------------------

        if (_ctx.mqtt)
        {
            inspectMQTT(
                *_ctx.mqtt
            );
        }


        // --------------------------------------------------------------------
        // WEB API
        // --------------------------------------------------------------------

        if (_ctx.webApi)
        {
            inspectWebAPI(
                *_ctx.webApi
            );
        }


        // --------------------------------------------------------------------
        // HVAC
        // --------------------------------------------------------------------

        if (_ctx.hvac)
        {
            inspectHVAC(
                *_ctx.hvac
            );
        }

        if (_ctx.averagesConfig)
        {
            inspectAverages(
                *_ctx.averagesConfig
            );
        }

        if (_ctx.solar)
        {
            inspectSolar(
                *_ctx.solar
            );
        }

        if (_ctx.runtimeStore)
        {
            inspectRuntimeStore();
        }

        // --------------------------------------------------------------------
        // FINALIZE
        // --------------------------------------------------------------------

        finalize();


        // --------------------------------------------------------------------
        // IO SUMMARY
        // --------------------------------------------------------------------

        size_t digitalInputs = 0;
        size_t digitalOutputs = 0;
        size_t analogInputs = 0;
        size_t analogOutputs = 0;
        size_t virtualAreas = 0;
        size_t unknownIO = 0;


        for (const Entity& entity :
            _entities)
        {
            switch (entity.ioType)
            {
                case IOType::DIGITAL_INPUT:
                    ++digitalInputs;
                    break;

                case IOType::DIGITAL_OUTPUT:
                    ++digitalOutputs;
                    break;

                case IOType::ANALOG_INPUT:
                    ++analogInputs;
                    break;

                case IOType::ANALOG_OUTPUT:
                    ++analogOutputs;
                    break;

                case IOType::VIRTUAL:
                    ++virtualAreas;
                    break;

                case IOType::UNKNOWN:
                default:
                    ++unknownIO;
                    break;
            }
        }


        LOG_IF(
            "DomoIntrospection",
            "Build FULL completata: "
            "entities=%u DI=%u DO=%u AI=%u AO=%u "
            "virtual=%u unknownIO=%u",
            static_cast<unsigned>(
                _entities.size()
            ),
            static_cast<unsigned>(
                digitalInputs
            ),
            static_cast<unsigned>(
                digitalOutputs
            ),
            static_cast<unsigned>(
                analogInputs
            ),
            static_cast<unsigned>(
                analogOutputs
            ),
            static_cast<unsigned>(
                virtualAreas
            ),
            static_cast<unsigned>(
                unknownIO
            )
        );
    }
    

    // ========================================================================
    // CLEAR
    // ========================================================================

    void clear()
    {
        _ctx = Context{};

        _entities.clear();
        _relations.clear();
        _automations.clear();
    }


    // ========================================================================
    // ACCESSORS
    // ========================================================================

    size_t entityCount() const
    {
        return _entities.size();
    }


    size_t relationCount() const
    {
        return _relations.size();
    }


    size_t automationCount() const
    {
        return _automations.size();
    }


    const Entity* getEntity(
        size_t index) const
    {
        if (index >= _entities.size())
            return nullptr;

        return &_entities[index];
    }


    Entity* getEntity(
        size_t index)
    {
        if (index >= _entities.size())
            return nullptr;

        return &_entities[index];
    }


    const Relation* getRelation(
        size_t index) const
    {
        if (index >= _relations.size())
            return nullptr;

        return &_relations[index];
    }


    const Automation* getAutomation(
        size_t index) const
    {
        if (index >= _automations.size())
            return nullptr;

        return &_automations[index];
    }


    // ========================================================================
    // FIND ENTITY
    // ========================================================================

    Entity* findEntity(
        int area)
    {
        for (auto& e : _entities)
        {
            if (e.area == area)
                return &e;
        }

        return nullptr;
    }


    const Entity* findEntity(
        int area) const
    {
        for (const auto& e : _entities)
        {
            if (e.area == area)
                return &e;
        }

        return nullptr;
    }


    // ========================================================================
    // AREA NAME
    // ========================================================================

    const char* getAreaName(
        int area) const
    {
        const Entity* entity =
            findEntity(area);

        // ----------------------------------------------------
        // HVAC virtual entities
        // ----------------------------------------------------

        if (entity &&
            entity->origin ==
                EntityOrigin::HVAC)
        {
            const int index =
                (-1000 - area);

            if (_ctx.hvac &&
                index >= 0 &&
                static_cast<size_t>(index) <
                    _ctx.hvac->zoneCount)
            {
                return
                    _ctx.hvac->zones[index].name;
            }
        }

        // ----------------------------------------------------
        // Normal areas
        // ----------------------------------------------------

        if (entity &&
            entity->areaCfg &&
            entity->areaCfg->label.length() > 0)
        {
            return entity->areaCfg->label.c_str();
        }

        return getRegistryAreaName(area);
    }


    // ========================================================================
    // ITERATORS
    // ========================================================================

    template<typename F>
    void forEachEntity(F fn) const
    {
        for (const auto& entity : _entities)
            fn(entity);
    }


    template<typename F>
    void forEachRelation(F fn) const
    {
        for (const auto& relation : _relations)
            fn(relation);
    }


    template<typename F>
    void forEachAutomation(F fn) const
    {
        for (const auto& automation : _automations)
            fn(automation);
    }


    // ========================================================================
    // VALIDATION
    // ========================================================================

    Validation validate() const
    {
        Validation result;


        // --------------------------------------------------------------------
        // ENTITIES
        // --------------------------------------------------------------------

        result.entities =
            _entities.size();


        for (const auto& entity :
             _entities)
        {
            if (entity.hasAreaConfig())
                result.describedEntities++;
            else
                result.hardwareOnlyEntities++;


            if (entity.implementationOnly)
                result.implementationOnlyEntities++;


            if (entity.writable)
                result.writableEntities++;


            if (entity.kind ==
                EntityKind::UNKNOWN)
            {
                result.unknownEntities++;
            }
        }


        // --------------------------------------------------------------------
        // RELATIONS
        // --------------------------------------------------------------------

        for (const auto& relation :
             _relations)
        {
            switch (relation.type)
            {
                case RelationType::ROUTE:

                    result.routeRelations++;

                    if (!findEntity(
                            relation.sourceArea))
                    {
                        result.invalidRouteRelations++;
                    }

                    if (!findEntity(
                            relation.targetArea))
                    {
                        result.invalidRouteRelations++;
                    }

                    break;


                case RelationType::TOGGLE:

                    result.toggleRelations++;

                    if (!findEntity(
                            relation.sourceArea))
                    {
                        result.invalidToggleRelations++;
                    }

                    if (!findEntity(
                            relation.targetArea))
                    {
                        result.invalidToggleRelations++;
                    }

                    break;


                case RelationType::SPLIT:

                    result.splitRelations++;

                    if (!findEntity(
                            relation.sourceArea))
                    {
                        result.invalidSplitRelations++;
                    }

                    if (!findEntity(
                            relation.targetArea))
                    {
                        result.invalidSplitRelations++;
                    }

                    break;


                case RelationType::AUTOMATION_INPUT:

                    result.automationInputRelations++;

                    if (!findEntity(
                            relation.targetArea))
                    {
                        result.invalidAutomationAreas++;
                    }

                    break;


                case RelationType::AUTOMATION_OUTPUT:

                    result.automationOutputRelations++;

                    if (!findEntity(
                            relation.targetArea))
                    {
                        result.invalidAutomationAreas++;
                    }

                    break;


                case RelationType::AEE_AREA:

                    result.aeeBindings++;

                    break;


                case RelationType::MQTT_AREA:

                    result.mqttBindings++;

                    if (relation.targetArea < 0)
                        result.invalidMQTTMappings++;

                    break;


                case RelationType::WEBAPI_AEE:

                    result.webApiBindings++;

                    if (!relation.aee)
                        result.invalidWebAPIMappings++;

                    break;
            }
        }


        // --------------------------------------------------------------------
        // AUTOMATIONS
        // --------------------------------------------------------------------

        for (const auto& automation :
             _automations)
        {
            if (!automation.valid)
                result.invalidAutomationScenes++;
        }


        return result;
    }


    // ========================================================================
    // REPORT
    // ========================================================================

    void report() const
    {
        Serial.println();
        Serial.println(
            "=================================================="
        );

        Serial.println(
            "             DOMO INTROSPECTION"
        );

        Serial.println(
            "=================================================="
        );


        reportSummary();

        reportEntities();

        reportRelations();

        reportAutomations();

        reportValidation();


        Serial.println(
            "=================================================="
        );

        Serial.println();
    }


private:
    
    // ========================================================================
    // STORAGE
    // ========================================================================

    Context _ctx;

    std::vector<Entity>
        _entities;

    std::vector<Relation>
        _relations;

    std::vector<Automation>
        _automations;


    // ========================================================================
    // ENTITY CREATION
    // ========================================================================

    Entity& ensureEntity(
        int area)
    {
        for (auto& entity :
             _entities)
        {
            if (entity.area == area)
                return entity;
        }


        Entity entity;

        entity.area = area;

        _entities.push_back(
            entity
        );

        return _entities.back();
    }


    // ========================================================================
    // AREAS
    // ========================================================================

    void inspectAreas(
        const DomoManagerBufferEngine::AreasConfig& cfg)
    {
        for (const auto& area :
             cfg.list)
        {
            Entity& entity =
                ensureEntity(area.area);

            entity.areaCfg =
                &area;
        }
    }

    // ========================================================================
    // MAP DEVICE CHANNEL TYPE
    // ========================================================================
    //
    // Converte il tipo fisico definito nel DeviceProfiles nel tipo I/O
    // indipendente utilizzato da DomoIntrospection.
    //
    // Il parametro viene ricevuto come int per mantenere compatibilità con
    // enum non scoped ed evitare dipendenze dal nome concreto del tipo enum.
    //
    // ========================================================================

    static IOType mapChannelType(
        int channelType)
    {
        switch (channelType)
        {
            case GenericPrgDevice::DI:
            {
                return
                    IOType::DIGITAL_INPUT;
            }

            case GenericPrgDevice::DO:
            {
                return
                    IOType::DIGITAL_OUTPUT;
            }

            case GenericPrgDevice::AI:
            {
                return
                    IOType::ANALOG_INPUT;
            }

            case GenericPrgDevice::AO:
            {
                return
                    IOType::ANALOG_OUTPUT;
            }

            default:
            {
                return
                    IOType::UNKNOWN;
            }
        }
    }

    

    // ========================================================================
    // INSPECT DEVICES
    // ========================================================================
    //
    // Associa ciascuna area al relativo device e determina il tipo fisico
    // utilizzando il profilo registrato in DeviceProfiles.
    //
    // La correlazione è posizionale:
    //
    //     device.areas[]
    //
    // viene associato alla sequenza ottenuta espandendo:
    //
    //     profile[].items
    //
    // Esempio:
    //
    //     MA01_AXCX4020:
    //
    //         DI items=4
    //         DO items=2
    //
    //     device.areas:
    //
    //         15, 16, 17, 18, 19, 20
    //
    // Risultato:
    //
    //         15 -> DIGITAL_INPUT
    //         16 -> DIGITAL_INPUT
    //         17 -> DIGITAL_INPUT
    //         18 -> DIGITAL_INPUT
    //         19 -> DIGITAL_OUTPUT
    //         20 -> DIGITAL_OUTPUT
    //
    // startingAddr e ItemsPerCall riguardano il protocollo Modbus e non
    // modificano la corrispondenza posizionale con device.areas[].
    //
    // Non vengono utilizzati:
    //
    //     - label delle aree
    //     - prefissi testuali DI: / DO:
    //     - numeri area cablati
    //     - nomi specifici dei device
    //
    // ========================================================================

    void inspectDevices(
        const DomoManagerConfig::Devices& cfg,
        const DeviceProfiles& deviceProfiles)
    {
        for (size_t deviceIndex = 0;
            deviceIndex < cfg.list.size();
            ++deviceIndex)
        {
            const DomoManagerConfig::Devices::Device& device =
                cfg.list[deviceIndex];


            // ================================================================
            // PROFILE LOOKUP
            // ================================================================

            const std::vector<DeviceProfiles::Channel>* profile =
                deviceProfiles.get(
                    device.profile.c_str()
                );


            // ================================================================
            // PROFILE NOT FOUND
            // ================================================================

            if (!profile)
            {
                LOG_EF(
                    "DomoIntrospection",
                    "Profilo non trovato: "
                    "device='%s' profile='%s'",
                    device.name.c_str(),
                    device.profile.c_str()
                );

                /*
                * Le aree vengono comunque associate al device,
                * mantenendo IOType::UNKNOWN.
                */

                for (size_t areaIndex = 0;
                    areaIndex < device.areas.size();
                    ++areaIndex)
                {
                    const int area =
                        device.areas[areaIndex];

                    if (area < 0)
                        continue;


                    Entity& entity =
                        ensureEntity(
                            area
                        );


                    if (!entity.device)
                    {
                        entity.device =
                            &device;

                        entity.deviceIndex =
                            static_cast<int>(
                                deviceIndex
                            );

                        entity.deviceAreaIndex =
                            static_cast<int>(
                                areaIndex
                            );
                    }


                    if (entity.ioType ==
                        IOType::UNKNOWN)
                    {
                        entity.ioType =
                            IOType::UNKNOWN;
                    }
                }

                continue;
            }


            // ================================================================
            // EXPANDED AREA INDEX
            // ================================================================
            //
            // Posizione corrente dentro device.areas[].
            //
            // ================================================================

            size_t deviceAreaIndex =
                0;


            // ================================================================
            // PROFILE CHANNELS
            // ================================================================

            for (size_t channelIndex = 0;
                channelIndex < profile->size();
                ++channelIndex)
            {
                const DeviceProfiles::Channel& channel =
                    (*profile)[channelIndex];


                // ------------------------------------------------------------
                // PHYSICAL TYPE
                // ------------------------------------------------------------

                const IOType channelIOType =
                    mapChannelType(
                        static_cast<int>(
                            channel.type
                        )
                    );


                // ------------------------------------------------------------
                // CHANNEL ITEM COUNT
                // ------------------------------------------------------------

                if (channel.items <= 0)
                {
                    LOG_WF(
                        "DomoIntrospection",
                        "Canale profilo senza elementi: "
                        "device='%s' profile='%s' "
                        "channel=%u items=%d",
                        device.name.c_str(),
                        device.profile.c_str(),
                        static_cast<unsigned>(
                            channelIndex
                        ),
                        channel.items
                    );

                    continue;
                }


                const size_t channelItems =
                    static_cast<size_t>(
                        channel.items
                    );


                // ------------------------------------------------------------
                // EXPAND CHANNEL ITEMS
                // ------------------------------------------------------------

                for (size_t itemIndex = 0;
                    itemIndex < channelItems;
                    ++itemIndex)
                {
                    // --------------------------------------------------------
                    // DEVICE AREAS EXHAUSTED
                    // --------------------------------------------------------

                    if (deviceAreaIndex >=
                        device.areas.size())
                    {
                        LOG_WF(
                            "DomoIntrospection",
                            "Profilo con più elementi delle aree: "
                            "device='%s' profile='%s' "
                            "channel=%u item=%u "
                            "expandedIndex=%u areas=%u",
                            device.name.c_str(),
                            device.profile.c_str(),
                            static_cast<unsigned>(
                                channelIndex
                            ),
                            static_cast<unsigned>(
                                itemIndex
                            ),
                            static_cast<unsigned>(
                                deviceAreaIndex
                            ),
                            static_cast<unsigned>(
                                device.areas.size()
                            )
                        );

                        break;
                    }


                    // --------------------------------------------------------
                    // AREA
                    // --------------------------------------------------------

                    const int area =
                        device.areas[
                            deviceAreaIndex
                        ];


                    if (area >= 0)
                    {
                        Entity& entity =
                            ensureEntity(
                                area
                            );


                        // ====================================================
                        // DEVICE ASSOCIATION
                        // ====================================================

                        if (!entity.device)
                        {
                            entity.device =
                                &device;

                            entity.deviceIndex =
                                static_cast<int>(
                                    deviceIndex
                                );

                            entity.deviceAreaIndex =
                                static_cast<int>(
                                    deviceAreaIndex
                                );
                        }
                        else if (entity.device !=
                                &device)
                        {
                            LOG_WF(
                                "DomoIntrospection",
                                "Area associata a più device: "
                                "area=%d current='%s' new='%s'",
                                area,
                                entity.device->name.c_str(),
                                device.name.c_str()
                            );
                        }


                        // ====================================================
                        // PHYSICAL I/O TYPE
                        // ====================================================

                        if (entity.ioType ==
                            IOType::UNKNOWN)
                        {
                            entity.ioType =
                                channelIOType;
                        }
                        else if (entity.ioType !=
                                channelIOType)
                        {
                            LOG_WF(
                                "DomoIntrospection",
                                "Conflitto IOType: "
                                "area=%d device='%s' "
                                "profile='%s' "
                                "existing=%u new=%u",
                                area,
                                device.name.c_str(),
                                device.profile.c_str(),
                                static_cast<unsigned>(
                                    entity.ioType
                                ),
                                static_cast<unsigned>(
                                    channelIOType
                                )
                            );
                        }
                    }


                    ++deviceAreaIndex;
                }


                /*
                * Tutte le aree del device sono state associate.
                */

                if (deviceAreaIndex >=
                    device.areas.size())
                {
                    break;
                }
            }


            // ================================================================
            // AREAS NOT COVERED BY PROFILE
            // ================================================================
            //
            // Il device contiene più aree rispetto alla somma di profile.items.
            //
            // Le aree residue vengono associate al device, ma mantengono
            // IOType::UNKNOWN.
            //
            // ================================================================

            while (deviceAreaIndex <
                device.areas.size())
            {
                const int area =
                    device.areas[
                        deviceAreaIndex
                    ];


                if (area >= 0)
                {
                    Entity& entity =
                        ensureEntity(
                            area
                        );


                    if (!entity.device)
                    {
                        entity.device =
                            &device;

                        entity.deviceIndex =
                            static_cast<int>(
                                deviceIndex
                            );

                        entity.deviceAreaIndex =
                            static_cast<int>(
                                deviceAreaIndex
                            );
                    }


                    if (entity.ioType ==
                        IOType::UNKNOWN)
                    {
                        entity.ioType =
                            IOType::UNKNOWN;
                    }


                    LOG_WF(
                        "DomoIntrospection",
                        "Area senza elemento nel profilo: "
                        "area=%d device='%s' "
                        "profile='%s' areaIndex=%u",
                        area,
                        device.name.c_str(),
                        device.profile.c_str(),
                        static_cast<unsigned>(
                            deviceAreaIndex
                        )
                    );
                }


                ++deviceAreaIndex;
            }
        }
    }

    


    // ========================================================================
    // ROUTES
    // ========================================================================

    void inspectRoutes(
        const DomoManagerRouteEngine::RoutesConfig& cfg)
    {
        for (const auto& route :
             cfg.list)
        {
            Entity& trigger =
                ensureEntity(
                    route.triggerArea
                );


            trigger.isRouteTrigger =
                true;

            trigger.writable =
                true;


            for (const auto& routeCase :
                 route.cases)
            {
                for (const auto& action :
                     routeCase.actions)
                {
                    Entity& target =
                        ensureEntity(
                            action.targetArea
                        );


                    target.isRouteTarget =
                        true;

                    target.isDerivedOutput =
                        true;


                    Relation relation;

                    relation.type =
                        RelationType::ROUTE;

                    relation.sourceArea =
                        route.triggerArea;

                    relation.targetArea =
                        action.targetArea;

                    relation.name =
                        route.name.c_str();

                    relation.value =
                        routeCase.triggerValue;


                    _relations.push_back(
                        relation
                    );
                }
            }
        }
    }


    // ========================================================================
    // TOGGLES
    // ========================================================================

    void inspectToggles(
        const DomoManagerToggleEngine::TogglesConfig& cfg)
    {
        for (const auto& toggle :
             cfg.list)
        {
            Entity& source =
                ensureEntity(
                    toggle.areaRead
                );


            source.isToggleSource =
                true;

            source.writable =
                true;


            for (size_t i = 0;
                 i < toggle.forwards.size();
                 i++)
            {
                const int targetArea =
                    toggle.forwards[i];


                Entity& target =
                    ensureEntity(
                        targetArea
                    );


                target.isToggleTarget =
                    true;

                target.isDerivedOutput =
                    true;


                Relation relation;

                relation.type =
                    RelationType::TOGGLE;

                relation.sourceArea =
                    toggle.areaRead;

                relation.targetArea =
                    targetArea;

                relation.name =
                    "toggle";

                relation.index =
                    static_cast<int>(i);


                _relations.push_back(
                    relation
                );
            }
        }
    }


    // ========================================================================
    // SPLITS
    // ========================================================================

    void inspectSplits(
        const DomoManagerSplitEngine::SplitsConfig& cfg)
    {
        for (const auto& split :
             cfg.list)
        {
            Entity& main =
                ensureEntity(
                    split.mainArea
                );


            main.isSplitMain =
                true;

            main.writable =
                true;


            for (size_t i = 0;
                 i < split.outAreas.size();
                 i++)
            {
                const int targetArea =
                    split.outAreas[i];


                Entity& target =
                    ensureEntity(
                        targetArea
                    );


                target.isSplitTarget =
                    true;

                target.isDerivedOutput =
                    true;


                Relation relation;

                relation.type =
                    RelationType::SPLIT;

                relation.sourceArea =
                    split.mainArea;

                relation.targetArea =
                    targetArea;

                relation.name =
                    "split";

                relation.index =
                    static_cast<int>(i);

                relation.duration =
                    split.maxTime;


                _relations.push_back(
                    relation
                );
            }
        }
    }


    // ========================================================================
    // AUTOMATION
    // ========================================================================
     void inspectAutomationEngine(
        AutomationEngine& engine)
    {
        // --------------------------------------------------------
        // SCENES
        // --------------------------------------------------------

        for (uint8_t i = 0;
            i < engine.getSceneCount();
            i++)
        {
            AutomationEngine::Scene* scene =
                engine.getScenePtr(i);

            if (!scene)
                continue;

            Automation info;

            info.type =
                AutomationType::SCENE;

            info.name =
                scene->name.c_str();

            info.inputCount = 0;

            info.outputCount =
                scene->actionCount;

            for (uint8_t a = 0;
                a < scene->actionCount;
                a++)
            {
                const auto& action =
                    scene->actions[a];

                Entity& entity =
                    ensureEntity(
                        action.targetArea
                    );

                entity.isAutomationOutput =
                    true;

                Relation relation;

                relation.type =
                    RelationType::AUTOMATION_OUTPUT;

                relation.targetArea =
                    action.targetArea;

                relation.name =
                    scene->name.c_str();

                relation.value =
                    action.value;

                _relations.push_back(
                    relation
                );
            }

            _automations.push_back(
                info
            );
        }


        // --------------------------------------------------------
        // STATIC RULES
        // --------------------------------------------------------

        for (uint8_t i = 0;
            i < engine.getRuleCount();
            i++)
        {
            AutomationEngine::Rule& rule =
                engine.getRule(i);

            Automation info;

            info.type =
                AutomationType::RULE;

            info.name =
                nullptr;

            info.inputCount =
                rule.conditionCount;

            info.outputCount =
                rule.actionThenCount +
                rule.actionElseCount;


            // INPUT
            for (uint8_t c = 0;
                c < rule.conditionCount;
                c++)
            {
                const auto& condition =
                    rule.conditions[c];

                if (condition.area < 0)
                    continue;

                Entity& entity =
                    ensureEntity(
                        condition.area
                    );

                entity.isAutomationInput =
                    true;

                Relation relation;

                relation.type =
                    RelationType::AUTOMATION_INPUT;

                relation.sourceArea =
                    condition.area;

                relation.targetArea =
                    condition.area;

                relation.name =
                    "rule";

                _relations.push_back(
                    relation
                );
            }


            // THEN
            for (uint8_t a = 0;
                a < rule.actionThenCount;
                a++)
            {
                const auto& action =
                    rule.actionsThen[a];

                Entity& entity =
                    ensureEntity(
                        action.targetArea
                    );

                entity.isAutomationOutput =
                    true;


                Relation relation;

                relation.type =
                    RelationType::AUTOMATION_OUTPUT;

                relation.targetArea =
                    action.targetArea;

                relation.value =
                    action.value;

                relation.name =
                    "rule";

                _relations.push_back(
                    relation
                );
            }


            // ELSE
            for (uint8_t a = 0;
                a < rule.actionElseCount;
                a++)
            {
                const auto& action =
                    rule.actionsElse[a];

                Entity& entity =
                    ensureEntity(
                        action.targetArea
                    );

                entity.isAutomationOutput =
                    true;


                Relation relation;

                relation.type =
                    RelationType::AUTOMATION_OUTPUT;

                relation.targetArea =
                    action.targetArea;

                relation.value =
                    action.value;

                relation.name =
                    "rule";

                _relations.push_back(
                    relation
                );
            }


            _automations.push_back(
                info
            );
        }


        // --------------------------------------------------------
        // SCHEDULED
        // --------------------------------------------------------

        for (uint8_t i = 0;
            i < engine.getScheduledCount();
            i++)
        {
            AutomationEngine::ScheduledRule* scheduled =
                engine.getScheduledPtr(i);

            if (!scheduled)
                continue;

            Automation info;

            info.type =
                AutomationType::RULE;

            info.name =
                nullptr;


            if (scheduled->hasCondition &&
                scheduled->condition.area >= 0)
            {
                Entity& entity =
                    ensureEntity(
                        scheduled->condition.area
                    );

                entity.isAutomationInput =
                    true;
            }


            if (scheduled->scene)
            {
                info.name =
                    scheduled->scene->name.c_str();
            }


            _automations.push_back(
                info
            );
        }


        // --------------------------------------------------------
        // DYNAMIC
        // --------------------------------------------------------
        //
        // Le DynamicAutomation espongono una vista strutturale
        // delle aree utilizzate:
        //
        //   - inputAreas
        //   - outputAreas
        //
        // Questa vista non contiene configurazioni duplicate,
        // accessi al Buffer o identificativi cablati.
        //
        // L'introspection utilizza i metadati per costruire:
        //
        //   - AUTOMATION_INPUT
        //   - AUTOMATION_OUTPUT
        //   - isTechnicalState
        //
        // Una stessa area presente sia negli input sia negli output
        // della medesima DynamicAutomation viene considerata uno
        // stato tecnico interno.
        //
        // --------------------------------------------------------

        for (uint8_t i = 0;
            i < engine.getDynamicCount();
            i++)
        {
            AutomationEngine::DynamicAutomation* dynamic =
                engine.getDynamicPtr(i);

            if (!dynamic)
                continue;


            // ====================================================
            // AUTOMATION DESCRIPTOR
            // ====================================================

            Automation info;

            info.type =
                AutomationType::RULE;

            info.name =
                dynamic->name;

            info.inputCount =
                dynamic->inputAreaCount;

            info.outputCount =
                dynamic->outputAreaCount;

            info.valid =
                dynamic->callback ||
                dynamic->callbackCtx;


            // ====================================================
            // INPUT AREAS
            // ====================================================

            for (uint8_t inputIndex = 0;
                inputIndex < dynamic->inputAreaCount;
                inputIndex++)
            {
                const int area =
                    dynamic->inputAreas[inputIndex];

                if (area < 0)
                    continue;

                Entity& entity =
                    ensureEntity(area);

                entity.isAutomationInput = true;


                // ------------------------------------------------
                // TECHNICAL STATE
                // ------------------------------------------------
                //
                // Una stessa area utilizzata dalla medesima
                // DynamicAutomation sia come input sia come output
                // rappresenta uno stato tecnico interno.
                //
                // ------------------------------------------------

                if (dynamic->hasOutputArea(area))
                {
                    entity.isTechnicalState = true;
                }


                // ------------------------------------------------
                // INPUT RELATION
                // ------------------------------------------------

                Relation relation;

                relation.type =
                    RelationType::AUTOMATION_INPUT;

                relation.sourceArea =
                    area;

                relation.targetArea =
                    area;

                relation.name =
                    dynamic->name
                        ? dynamic->name
                        : "dynamic";

                relation.index =
                    static_cast<int>(
                        inputIndex
                    );

                _relations.push_back(
                    relation
                );
            }


            // ====================================================
            // OUTPUT AREAS
            // ====================================================

            for (uint8_t outputIndex = 0;
                outputIndex < dynamic->outputAreaCount;
                outputIndex++)
            {
                const int area =
                    dynamic->outputAreas[outputIndex];

                if (area < 0)
                    continue;

                Entity& entity =
                    ensureEntity(area);

                entity.isAutomationOutput = true;


                // ------------------------------------------------
                // TECHNICAL STATE
                // ------------------------------------------------

                if (dynamic->hasInputArea(area))
                {
                    entity.isTechnicalState = true;
                }


                // ------------------------------------------------
                // OUTPUT RELATION
                // ------------------------------------------------

                Relation relation;

                relation.type =
                    RelationType::AUTOMATION_OUTPUT;

                relation.sourceArea =
                    area;

                relation.targetArea =
                    area;

                relation.name =
                    dynamic->name
                        ? dynamic->name
                        : "dynamic";

                relation.index =
                    static_cast<int>(
                        outputIndex
                    );

                _relations.push_back(
                    relation
                );
            }


            // ====================================================
            // REGISTER AUTOMATION
            // ====================================================

            _automations.push_back(
                info
            );
        }

    }

    void inspectAutomation(
        const AutomationBuilder::AutomationConfig& cfg)
    {
        inspectAutomationScenes(cfg);

        inspectAutomationRules(cfg);

        inspectAutomationSequences(cfg);
    }


    // ========================================================================
    // AUTOMATION - SCENES
    // ========================================================================

    void inspectAutomationScenes(
        const AutomationBuilder::AutomationConfig& cfg)
    {
        for (const auto& scene :
             cfg.scenes)
        {
            Automation info;

            info.type =
                AutomationType::SCENE;

            info.name =
                scene.name.c_str();

            info.scene =
                &scene;

            info.outputCount =
                scene.actions.size();


            for (const auto& action :
                 scene.actions)
            {
                Entity& target =
                    ensureEntity(
                        action.area
                    );


                target.isAutomationOutput =
                    true;


                Relation relation;

                relation.type =
                    RelationType::AUTOMATION_OUTPUT;

                relation.sourceArea =
                    -1;

                relation.targetArea =
                    action.area;

                relation.name =
                    scene.name.c_str();

                relation.value =
                    static_cast<int>(
                        action.value
                    );


                _relations.push_back(
                    relation
                );
            }


            _automations.push_back(
                info
            );
        }
    }


    // ========================================================================
    // AUTOMATION - RULES
    // ========================================================================

    void inspectAutomationRules(
        const AutomationBuilder::AutomationConfig& cfg)
    {
        for (const auto& rule :
             cfg.rules)
        {
            Automation info;

            info.type =
                AutomationType::RULE;

            info.name =
                rule.name.c_str();

            info.rule =
                &rule;


            // ----------------------------------------------------------------
            // THRESHOLD
            // ----------------------------------------------------------------

            if (rule.type == "threshold")
            {
                inspectAutomationInput(
                    info,
                    rule.threshold.area,
                    rule.name.c_str()
                );
            }


            // ----------------------------------------------------------------
            // TREND
            // ----------------------------------------------------------------

            else if (rule.type == "trend")
            {
                inspectAutomationInput(
                    info,
                    rule.trend.area,
                    rule.name.c_str()
                );
            }


            // ----------------------------------------------------------------
            // BITMASK
            // ----------------------------------------------------------------

            else if (rule.type == "bitmask")
            {
                inspectAutomationInput(
                    info,
                    rule.bitmask.area,
                    rule.name.c_str()
                );
            }


            // ----------------------------------------------------------------
            // MULTI
            // ----------------------------------------------------------------

            else if (rule.type == "multi")
            {
                for (const auto& condition :
                     rule.multi.conditions)
                {
                    inspectAutomationInput(
                        info,
                        condition.area,
                        rule.name.c_str()
                    );
                }
            }


            // ----------------------------------------------------------------
            // COMPOSITE
            // ----------------------------------------------------------------

            else if (rule.type == "composite")
            {
                for (const auto& input :
                     rule.composite.inputs)
                {
                    inspectAutomationInput(
                        info,
                        input.area,
                        rule.name.c_str()
                    );
                }


                if (rule.composite.output.area >= 0)
                {
                    Entity& output =
                        ensureEntity(
                            rule.composite.output.area
                        );


                    output.isAutomationOutput =
                        true;


                    Relation relation;

                    relation.type =
                        RelationType::AUTOMATION_OUTPUT;

                    relation.sourceArea =
                        -1;

                    relation.targetArea =
                        rule.composite.output.area;

                    relation.name =
                        rule.name.c_str();

                    relation.index =
                        rule.composite.output.bitIndex;


                    _relations.push_back(
                        relation
                    );


                    info.outputCount++;
                }
            }


            // ----------------------------------------------------------------
            // SCENE REFERENCES
            // ----------------------------------------------------------------

            if (rule.sceneTrue.length() > 0)
            {
                if (!sceneExists(
                        cfg,
                        rule.sceneTrue))
                {
                    info.valid = false;
                }
            }


            if (rule.sceneFalse.length() > 0)
            {
                if (!sceneExists(
                        cfg,
                        rule.sceneFalse))
                {
                    info.valid = false;
                }
            }


            _automations.push_back(
                info
            );
        }
    }


    // ========================================================================
    // AUTOMATION INPUT
    // ========================================================================

    void inspectAutomationInput(
        Automation& automation,
        int area,
        const char* automationName)
    {
        if (area < 0)
        {
            automation.valid =
                false;

            return;
        }


        Entity& source =
            ensureEntity(area);


        source.isAutomationInput =
            true;


        Relation relation;

        relation.type =
            RelationType::AUTOMATION_INPUT;

        relation.sourceArea =
            area;

        relation.targetArea =
            area;

        relation.name =
            automationName;


        _relations.push_back(
            relation
        );


        automation.inputCount++;
    }


    // ========================================================================
    // AUTOMATION - SEQUENCES
    // ========================================================================

    void inspectAutomationSequences(
        const AutomationBuilder::AutomationConfig& cfg)
    {
        for (const auto& sequence :
             cfg.sequences)
        {
            Automation info;

            info.type =
                AutomationType::SEQUENCE;

            info.name =
                nullptr;

            info.sequence =
                &sequence;


            for (const auto& step :
                 sequence.steps)
            {
                Entity& target =
                    ensureEntity(
                        step.area
                    );


                target.isAutomationOutput =
                    true;


                Relation relation;

                relation.type =
                    RelationType::AUTOMATION_OUTPUT;

                relation.sourceArea =
                    -1;

                relation.targetArea =
                    step.area;

                relation.name =
                    "sequence";

                relation.value =
                    static_cast<int>(
                        step.value
                    );

                relation.duration =
                    step.delayMs;


                _relations.push_back(
                    relation
                );


                info.outputCount++;
            }


            _automations.push_back(
                info
            );
        }
    }


    // ========================================================================
    // AUTOMATION - SCENE EXISTS
    // ========================================================================

    bool sceneExists(
        const AutomationBuilder::AutomationConfig& cfg,
        const String& name) const
    {
        if (name.length() == 0)
            return true;


        if (AutomationEngine::BuiltinScenes::isBuiltin(
                name.c_str()))
        {
            return true;
        }


        for (const auto& scene :
             cfg.scenes)
        {
            if (scene.name == name)
                return true;
        }


        return false;
    }


    // ========================================================================
    // AEE
    // ========================================================================
    //
    // AEE è una proiezione di alcune variabili.
    //
    // Non crea nuove entity concettuali.
    //
    // ========================================================================

    void inspectAEE(
        AEERegistry& registry)
    {
        registry.forEach(
            [&](AEEVariableBase* variable)
            {
                if (!variable)
                    return;


                const AEEVarDef& def =
                    variable->def;


                const int area =
                    resolveAEEArea(
                        def
                    );


                if (area < 0)
                    return;


                Entity& entity =
                    ensureEntity(area);


                entity.hasAEE =
                    true;

                entity.scale *=
                    def.scale;


                Relation relation;

                relation.type =
                    RelationType::AEE_AREA;

                relation.sourceArea =
                    area;

                relation.targetArea =
                    area;

                relation.name =
                    def.name.c_str();

                relation.aee =
                    variable;


                _relations.push_back(
                    relation
                );
            }
        );
    }


    // ========================================================================
    // AEE AREA RESOLUTION
    // ========================================================================

    int resolveAEEArea(
        const AEEVarDef& def) const
    {
        if (def.sourceType ==
            AEEVarSourceType::BufferArea)
        {
            return def.bufferArea;
        }


        if (def.sourceType ==
            AEEVarSourceType::GenericSensor)
        {
            if (def.sensorCfg.type ==
                GenericSensorConfig::Type::BUFFER)
            {
                return def.sensorCfg.value;
            }
        }


        return -1;
    }


    // ========================================================================
    // MQTT
    // ========================================================================
    //
    // SOLO binding espliciti.
    //
    // Non facciamo discovery del mondo Zigbee.
    //
    // ========================================================================

    void inspectMQTT(
        const FrontendConfig::MQTT& mqtt)
    {
        if (!mqtt.clients || mqtt.clientCount == 0)
            return;


        for (size_t clientIndex = 0;
            clientIndex < mqtt.clientCount;
            ++clientIndex)
        {
            const auto& client =
                mqtt.clients[clientIndex];


            if (!client.enabled)
                continue;


            if (!client.devices || client.deviceCount == 0)
                continue;


            for (size_t deviceIndex = 0;
                deviceIndex < client.deviceCount;
                ++deviceIndex)
            {
                const auto& device =
                    client.devices[deviceIndex];


                for (size_t mappingIndex = 0;
                    mappingIndex <
                        FrontendConfig::MQTT::Device::MAX_MAPPINGS;
                    ++mappingIndex)
                {
                    const auto& mapping =
                        device.mappings[mappingIndex];


                    /*
                    * Mapping non utilizzato.
                    */
                    if (!mapping.field)
                        continue;


                    /*
                    * Mapping senza area DomoManager.
                    */
                    if (mapping.area < 0)
                        continue;


                    Entity& entity =
                        ensureEntity(
                            mapping.area
                        );


                    entity.hasMQTT =
                        true;

                    entity.origin =
                        EntityOrigin::EXTERNAL_SOURCE;


                    entity.scale *=
                        mapping.scale;


                    if (mapping.direction ==
                            FrontendConfig::MQTT::Mapping::Direction::WRITE ||
                        mapping.direction ==
                            FrontendConfig::MQTT::Mapping::Direction::READ_WRITE)
                    {
                        entity.writable =
                            true;
                    }


                    Relation relation;

                    relation.type =
                        RelationType::MQTT_AREA;

                    relation.sourceArea =
                        mapping.area;

                    relation.targetArea =
                        mapping.area;

                    relation.name =
                        device.name;

                    relation.mqttDevice =
                        &device;

                    relation.mqttMapping =
                        &mapping;


                    _relations.push_back(
                        relation
                    );
                }
            }
        }
    }

    void inspectSolar(
        const FrontendConfig::Solar& solar)
    {
        if (!solar.enabled)
            return;

        auto registerArea =
            [&](int area)
            {
                if (area < 0)
                    return;

                Entity& entity =
                    ensureEntity(area);

                entity.origin =
                    EntityOrigin::SOLAR;

                entity.fromSolar =
                    true;

                entity.readable =
                    true;
            };

        registerArea(
            solar.areas.pvVoltage);

        registerArea(
            solar.areas.pvCurrent);

        registerArea(
            solar.areas.pvPower);

        registerArea(
            solar.areas.batteryVoltage);

        registerArea(
            solar.areas.batterySoc);

        registerArea(
            solar.areas.gridVoltage);

        registerArea(
            solar.areas.gridPower);

        registerArea(
            solar.areas.outputPower);

        registerArea(
            solar.areas.loadPercent);
    }

    void inspectRuntimeStore()
    {
        if (!_ctx.runtimeStore)
            return;

        _ctx.runtimeStore->forEach(
            [&](DMRuntimeStore::Handle handle,
                const DMRuntimeStore::Entry& entry)
            {
                if (!entry.used ||
                    !entry.publish ||
                    !entry.id)
                {
                    return;
                }

                Entity entity;

                entity.area =
                    -1;

                entity.origin =
                    EntityOrigin::RUNTIME;

                entity.ioType =
                    IOType::VIRTUAL;

                entity.runtimeHandle =
                    handle;

                entity.runtimeEntry =
                    &entry;

                entity.readable =
                    entry.readable();

                entity.writable =
                    entry.writable();

                entity.implementationOnly =
                    false;

                entity.unit =
                    entry.unit;

                entity.deviceClass =
                    entry.deviceClass;

                entity.stateClass =
                    entry.stateClass;

                switch (entry.type)
                {
                    case DMRuntimeStore::Type::BOOL:
                    {
                        entity.kind =
                            entry.writable()
                                ? EntityKind::SWITCH
                                : EntityKind::BINARY_SENSOR;

                        break;
                    }

                    case DMRuntimeStore::Type::INT32:
                    case DMRuntimeStore::Type::FLOAT:
                    {
                        entity.kind =
                            entry.writable()
                                ? EntityKind::NUMBER
                                : EntityKind::SENSOR;

                        entity.number.enabled =
                            entry.writable();

                        entity.number.minimumValue =
                            entry.minimum;

                        entity.number.maximumValue =
                            entry.maximum;

                        entity.number.stepValue =
                            entry.step;

                        entity.number.displayDecimals =
                            entry.decimals;

                        break;
                    }

                    case DMRuntimeStore::Type::STRING32:
                    case DMRuntimeStore::Type::STRING64:
                    case DMRuntimeStore::Type::JSON128:
                    case DMRuntimeStore::Type::JSON256:
                    {
                        entity.kind =
                            EntityKind::TEXT;

                        break;
                    }

                    default:
                    {
                        entity.kind =
                            EntityKind::UNKNOWN;

                        break;
                    }
                }

                _entities.push_back(
                    entity
                );

                LOG_IF(
                    "INTROSPECTION",
                    "Runtime entity: handle=%u id=%s type=%s kind=%s",
                    static_cast<unsigned>(
                        handle
                    ),
                    entry.id,
                    DMRuntimeStore::typeToString(
                        entry.type
                    ),
                    entityKindToString(
                        entity.kind
                    )
                );
            }
        );
    }


    void inspectAverages(
        const FrontendConfig::Averages& averages)
    {
        if (!averages.enabled)
            return;

        if (!averages.gruppi ||
            averages.gruppiCount == 0)
        {
            return;
        }

        for (size_t i = 0;
            i < averages.gruppiCount;
            ++i)
        {
            const auto& group =
                averages.gruppi[i];

            if (group.areaOut < 0)
                continue;

            Entity& entity =
                ensureEntity(
                    group.areaOut
                );

            entity.origin =
                EntityOrigin::AVERAGE;

            entity.fromAverage =
                true;

            entity.ioType =
                IOType::VIRTUAL;

            entity.readable =
                true;

            entity.writable =
                false;

            /*
            * AverageCalculator memorizza nel Buffer:
            *
            *     valoreBuffer = valoreReale * outScale
            *
            * La pubblicazione deve quindi applicare:
            *
            *     valoreReale = valoreBuffer / outScale
            */
            entity.scale =
                group.outScale > 0.0f
                    ? 1.0f / group.outScale
                    : 1.0f;

            LOG_IF(
                "INTROSPECTION",
                "Average entity: area=%d name=%s "
                "outScale=%.3f entityScale=%.3f",
                group.areaOut,
                group.nome
                    ? group.nome
                    : "?",
                static_cast<double>(
                    group.outScale
                ),
                static_cast<double>(
                    entity.scale
                )
            );
        }
    }

    void inspectHVAC(
        const FrontendConfig::HVAC& hvac)
    {
        if (!hvac.enabled)
            return;

        if (!hvac.zones)
            return;

        for (size_t i = 0;
            i < hvac.zoneCount;
            ++i)
        {
            const auto& zone =
                hvac.zones[i];

            Entity entity;

            entity.area =
                -1000 - static_cast<int>(i);

            entity.kind =
                EntityKind::CLIMATE;

            entity.origin =
                EntityOrigin::HVAC;

            entity.fromHVAC =
                true;

            entity.ioType =
                IOType::VIRTUAL;

            entity.readable =
                true;

            entity.writable =
                true;

            entity.climate.temperatureArea =
                zone.temperatureArea;

            entity.climate.humidityArea =
                zone.humidityArea;

            entity.climate.setpointArea =
                zone.setpointArea;

            entity.climate.fanCoilId =
                zone.fanCoilId;

            _entities.push_back(
                entity
            );
        }
    }

    // ========================================================================
    // WEB API
    // ========================================================================

    void inspectWebAPI(
        const FrontendConfig::
            webApiDeviceMessaging& webApi)
    {
        // ========================================================
        // CLIENT / POLLING
        // ========================================================

        if (webApi.client.enabled)
        {
            LOG_IF(
                "INTROSPECTION",
                "WebAPI client: timeout=%lu devices=%u",
                static_cast<unsigned long>(
                    webApi.client.timeoutMs
                ),
                static_cast<unsigned>(
                    webApi.client.deviceCount
                )
            );

            if (
                !webApi.client.devices ||
                webApi.client.deviceCount == 0
            )
            {
                LOG_WF(
                    "INTROSPECTION",
                    "WebAPI client enabled without devices"
                );
            }
            else
            {
                for (
                    size_t deviceIndex = 0;
                    deviceIndex <
                        webApi.client.deviceCount;
                    ++deviceIndex
                )
                {
                    const WebApiDevice& device =
                        webApi.client.devices[
                            deviceIndex
                        ];

                    LOG_IF(
                        "INTROSPECTION",
                        "WebAPI client[%u]: "
                        "name=%s profile=%s host=%s port=%u "
                        "interval=%lu poll=%s enabled=%u",
                        static_cast<unsigned>(
                            deviceIndex
                        ),
                        device.name
                            ? device.name
                            : "",
                        (
                            device.profile &&
                            device.profile->name
                        )
                            ? device.profile->name
                            : "",
                        device.host
                            ? device.host
                            : "",
                        static_cast<unsigned>(
                            device.port
                        ),
                        static_cast<unsigned long>(
                            device.intervalMs
                        ),
                        device.pollMessageKey
                            ? device.pollMessageKey
                            : "",
                        device.enabled
                            ? 1U
                            : 0U
                    );
                }
            }
        }

        // ========================================================
        // SERVER
        //
        // Gli endpoint non appartengono più alla configurazione.
        // Vengono costruiti internamente dal WebAPIEngine.
        // ========================================================

        if (webApi.server.enabled)
        {
            LOG_IF(
                "INTROSPECTION",
                "WebAPI server: port=%u timeout=%lu",
                static_cast<unsigned>(
                    webApi.server.port
                ),
                static_cast<unsigned long>(
                    webApi.server.timeoutMs
                )
            );
        }

        // ========================================================
        // COMPLETELY DISABLED
        // ========================================================

        if (
            !webApi.client.enabled &&
            !webApi.server.enabled
        )
        {
            LOG_IF(
                "INTROSPECTION",
                "WebAPI client and server disabled"
            );
        }
    }


    // ========================================================================
    // FINALIZE
    // ========================================================================
    //
    // Completa la costruzione del modello strutturale.
    //
    // Responsabilità:
    //
    //   1. Nasconde le entity prive di AreaConfig.
    //   2. Individua gli stati tecnici delle automazioni.
    //   3. Non assegna EntityKind.
    //   4. Non interpreta label.
    //   5. Non contiene ID area, buffer o configurazioni cablate.
    //
    // La classificazione semantica definitiva viene eseguita
    // successivamente da DomoSemanticResolver.
    //
    // ========================================================================

    void finalize()
    {
        for (auto& entity :
            _entities)
        {
            // ============================================================
            // ENTITY PRIVA DI CONFIGURAZIONE AREA
            // ============================================================
            //
            // L'entity può essere stata creata indirettamente perché
            // referenziata da:
            //
            //   - device
            //   - route
            //   - toggle
            //   - split
            //   - automation
            //   - AEE
            //   - MQTT
            //   - WebAPI
            //
            // Se non esiste una AreaConfig associata, l'entity non possiede
            // una descrizione pubblica esplicita e viene considerata un
            // dettaglio implementativo.
            //
            // Questa regola non modifica:
            //
            //   - kind
            //   - readable
            //   - writable
            //   - origin
            //
            // ============================================================

            if (!entity.areaCfg)
            {
                switch (entity.origin)
                {
                    case EntityOrigin::HVAC:
                    case EntityOrigin::AVERAGE:
                    case EntityOrigin::RUNTIME:
                        break;

                    default:
                        entity.implementationOnly = true;
                        break;
                }
            }



            // ============================================================
            // STATO TECNICO DI AUTOMAZIONE
            // ============================================================
            //
            // Un'entity utilizzata contemporaneamente come ingresso e
            // uscita del grafo delle automazioni rappresenta uno stato
            // tecnico condiviso.
            //
            // Esempio:
            //
            //   automation input:
            //       area X
            //
            //   automation output:
            //       area X
            //
            // In questo caso area X è presumibilmente utilizzata come:
            //
            //   - bitmask interna
            //   - stato aggregato
            //   - memoria della regola
            //   - stato intermedio
            //
            // La rilevazione deriva solamente dalle relazioni presenti nel
            // grafo. Non vengono controllati:
            //
            //   - numeri area
            //   - nomi area
            //   - label
            //   - oggetti Buffer
            //   - costanti AREA_*
            //
            // La classificazione definitiva viene lasciata al
            // DomoSemanticResolver, che trasformerà isTechnicalState in:
            //
            //   kind               = STATUS
            //   readable           = true
            //   writable           = false
            //   implementationOnly = true
            //
            // ============================================================

            if (entity.isAutomationInput &&
                entity.isAutomationOutput)
            {
                entity.isTechnicalState = true;
            }
        }
    }


    // ========================================================================
    // AREA REGISTRY
    // ========================================================================

    const char* getRegistryAreaName(
        int area) const
    {
        for (size_t i = 0;
             i < AreaRegistry::count();
             i++)
        {
            if (AreaRegistry::getValueByIndex(i) ==
                area)
            {
                return
                    AreaRegistry::getNameByIndex(i);
            }
        }


        return nullptr;
    }


    // ========================================================================
    // REPORT SUMMARY
    // ========================================================================

    void reportSummary() const
    {
        Validation v =
            validate();


        Serial.println();
        Serial.println(
            "----- SUMMARY -----"
        );


        Serial.print(
            "Areas indexed: "
        );

        Serial.println(
            v.entities
        );


        Serial.print(
            "Described: "
        );

        Serial.println(
            v.describedEntities
        );


        Serial.print(
            "Hardware-only: "
        );

        Serial.println(
            v.hardwareOnlyEntities
        );


        Serial.print(
            "Internal: "
        );

        Serial.println(
            v.implementationOnlyEntities
        );


        Serial.print(
            "Writable: "
        );

        Serial.println(
            v.writableEntities
        );


        Serial.println();


        Serial.print(
            "Routes: "
        );

        Serial.println(
            v.routeRelations
        );


        Serial.print(
            "Toggles: "
        );

        Serial.println(
            v.toggleRelations
        );


        Serial.print(
            "Splits: "
        );

        Serial.println(
            v.splitRelations
        );


        Serial.print(
            "Automation input: "
        );

        Serial.println(
            v.automationInputRelations
        );


        Serial.print(
            "Automation output: "
        );

        Serial.println(
            v.automationOutputRelations
        );


        Serial.print(
            "AEE bindings: "
        );

        Serial.println(
            v.aeeBindings
        );


        Serial.print(
            "MQTT bindings: "
        );

        Serial.println(
            v.mqttBindings
        );


        Serial.print(
            "WebAPI bindings: "
        );

        Serial.println(
            v.webApiBindings
        );


        Serial.print(
            "Automations: "
        );

        Serial.println(
            _automations.size()
        );
    }


    // ========================================================================
    // REPORT ENTITIES
    // ========================================================================

    void reportEntities() const
    {
        Serial.println();

        Serial.println(
            "----- AREAS -----"
        );

        for (const auto& entity :
            _entities)
        {
            /*
            * Le entity completamente interne normalmente vengono omesse.
            *
            * Tuttavia uno stato tecnico viene mostrato per consentire
            * la verifica della classificazione strutturale.
            */

            if (entity.implementationOnly &&
                !entity.isTechnicalState)
            {
                continue;
            }

            String line;

            line.reserve(240);

            line += "AREA ";
            line += String(entity.area);


            // ------------------------------------------------------------
            // NOME
            // ------------------------------------------------------------

            const char* name =
                getAreaName(
                    entity.area
                );

            if (name &&
                name[0] != '\0')
            {
                line += " | ";
                line += name;
            }


            // ------------------------------------------------------------
            // DEVICE
            // ------------------------------------------------------------

            if (entity.device)
            {
                line += " | DEVICE=";
                line += entity.device->name;
            }


            // ------------------------------------------------------------
            // SEMANTICA
            // ------------------------------------------------------------

            line += " | KIND=";

            line +=
                entityKindToString(
                    entity.kind
                );


            // ------------------------------------------------------------
            // ACCESSO
            // ------------------------------------------------------------

            if (entity.readable)
            {
                line += " | READABLE";
            }

            if (entity.writable)
            {
                line += " | WRITABLE";
            }


            // ------------------------------------------------------------
            // VISIBILITÀ
            // ------------------------------------------------------------

            if (entity.implementationOnly)
            {
                line += " | HIDDEN";
            }


            // ------------------------------------------------------------
            // RUOLO STRUTTURALE
            // ------------------------------------------------------------

            if (entity.isTechnicalState)
            {
                line += " | TECHNICAL_STATE";
            }


            // ------------------------------------------------------------
            // ROUTE
            // ------------------------------------------------------------

            if (entity.isRouteTrigger)
            {
                line += " | ROUTE_SOURCE";
            }

            if (entity.isRouteTarget)
            {
                line += " | ROUTE_TARGET";
            }


            // ------------------------------------------------------------
            // TOGGLE
            // ------------------------------------------------------------

            if (entity.isToggleSource)
            {
                line += " | TOGGLE_SOURCE";
            }

            if (entity.isToggleTarget)
            {
                line += " | TOGGLE_TARGET";
            }


            // ------------------------------------------------------------
            // SPLIT
            // ------------------------------------------------------------

            if (entity.isSplitMain)
            {
                line += " | SPLIT_SOURCE";
            }

            if (entity.isSplitTarget)
            {
                line += " | SPLIT_TARGET";
            }


            // ------------------------------------------------------------
            // DERIVED
            // ------------------------------------------------------------

            if (entity.isDerivedOutput)
            {
                line += " | DERIVED_OUTPUT";
            }


            // ------------------------------------------------------------
            // AUTOMATION
            // ------------------------------------------------------------

            if (entity.isAutomationInput)
            {
                line += " | AUTO_IN";
            }

            if (entity.isAutomationOutput)
            {
                line += " | AUTO_OUT";
            }


            // ------------------------------------------------------------
            // INTEGRAZIONI
            // ------------------------------------------------------------

            if (entity.hasAEE)
            {
                line += " | AEE";
            }

            if (entity.hasMQTT)
            {
                line += " | MQTT";
            }

            if (entity.hasWebAPI)
            {
                line += " | WEBAPI";
            }


            // ------------------------------------------------------------
            // SORGENTI SPECIALISTICHE
            // ------------------------------------------------------------

            if (entity.fromSecurity)
            {
                line += " | SECURITY";
            }

            if (entity.fromHVAC)
            {
                line += " | HVAC";
            }

            if (entity.fromAverage)
            {
                line += " | AVERAGE";
            }

            if (entity.fromPower)
            {
                line += " | POWER";
            }

            if (entity.fromSolar)
            {
                line += " | SOLAR";
            }

            // ------------------------------------------------------------
            // METADATI SEMANTICI
            // ------------------------------------------------------------

            if (entity.unit &&
                entity.unit[0] != '\0')
            {
                line += " | UNIT=";
                line += entity.unit;
            }

            if (entity.deviceClass &&
                entity.deviceClass[0] != '\0')
            {
                line += " | CLASS=";
                line += entity.deviceClass;
            }

            if (entity.stateClass &&
                entity.stateClass[0] != '\0')
            {
                line += " | STATE=";
                line += entity.stateClass;
            }


            Serial.println(
                line
            );
        }
    }


    // ========================================================================
    // REPORT RELATIONS
    // ========================================================================

    void reportRelations() const
    {
        if (_relations.empty())
            return;


        Serial.println();
        Serial.println(
            "----- RELATIONS -----"
        );


        for (const auto& relation :
             _relations)
        {
            String line;

            line.reserve(170);


            line += relationTypeToString(
                relation.type
            );


            if (relation.sourceArea >= 0)
            {
                line += " | ";
                line +=
                    String(
                        relation.sourceArea
                    );
            }


            if (relation.targetArea >= 0)
            {
                line += " -> ";
                line +=
                    String(
                        relation.targetArea
                    );
            }


            if (relation.name &&
                relation.name[0] != '\0')
            {
                line += " | ";
                line += relation.name;
            }


            switch (relation.type)
            {
                case RelationType::ROUTE:

                    line += " | VALUE=";
                    line +=
                        String(
                            relation.value
                        );

                    break;


                case RelationType::SPLIT:

                    line += " | TIME=";
                    line +=
                        String(
                            relation.duration
                        );

                    break;


                case RelationType::MQTT_AREA:

                    if (relation.mqttMapping &&
                        relation.mqttMapping->field)
                    {
                        line += " | FIELD=";
                        line +=
                            relation.mqttMapping->field;
                    }

                    break;


                case RelationType::AEE_AREA:

                    if (relation.aee)
                    {
                        line += " | VAR=";
                        line +=
                            relation.aee->def.name;
                    }

                    break;


                case RelationType::WEBAPI_AEE:

                    if (relation.webApiDevice)
                    {
                        line += " | DEVICE=";

                        line +=
                            relation.webApiDevice->name
                                ? relation.webApiDevice->name
                                : "";
                    }

                    if (relation.webApiProfile)
                    {
                        line += " | PROFILE=";

                        line +=
                            relation.webApiProfile->name
                                ? relation.webApiProfile->name
                                : "";
                    }

                    break;


                default:
                    break;
            }


            Serial.println(
                line
            );
        }
    }


    // ========================================================================
    // REPORT AUTOMATIONS
    // ========================================================================

    void reportAutomations() const
    {
        if (_automations.empty())
            return;


        Serial.println();
        Serial.println(
            "----- AUTOMATIONS -----"
        );


        for (const auto& automation :
             _automations)
        {
            String line;

            line.reserve(180);


            switch (automation.type)
            {
                case AutomationType::SCENE:
                    line += "SCENE";
                    break;

                case AutomationType::RULE:
                    line += "RULE";
                    break;

                case AutomationType::SEQUENCE:
                    line += "SEQUENCE";
                    break;
            }


            if (automation.name &&
                automation.name[0] != '\0')
            {
                line += " | ";
                line += automation.name;
            }


            line += " | IN=";
            line +=
                String(
                    automation.inputCount
                );


            line += " | OUT=";
            line +=
                String(
                    automation.outputCount
                );


            if (!automation.valid)
                line += " | INVALID";


            Serial.println(
                line
            );
        }
    }


    // ========================================================================
    // REPORT VALIDATION
    // ========================================================================

    void reportValidation() const
    {
        Validation v =
            validate();


        Serial.println();
        Serial.println(
            "----- VALIDATION -----"
        );


        if (v.invalidRouteRelations)
        {
            Serial.print(
                "Route errors: "
            );

            Serial.println(
                v.invalidRouteRelations
            );
        }


        if (v.invalidToggleRelations)
        {
            Serial.print(
                "Toggle errors: "
            );

            Serial.println(
                v.invalidToggleRelations
            );
        }


        if (v.invalidSplitRelations)
        {
            Serial.print(
                "Split errors: "
            );

            Serial.println(
                v.invalidSplitRelations
            );
        }


        if (v.invalidAutomationAreas)
        {
            Serial.print(
                "Automation area errors: "
            );

            Serial.println(
                v.invalidAutomationAreas
            );
        }


        if (v.invalidAutomationScenes)
        {
            Serial.print(
                "Automation scene errors: "
            );

            Serial.println(
                v.invalidAutomationScenes
            );
        }


        if (v.invalidMQTTMappings)
        {
            Serial.print(
                "MQTT mapping errors: "
            );

            Serial.println(
                v.invalidMQTTMappings
            );
        }


        if (v.invalidWebAPIMappings)
        {
            Serial.print(
                "WebAPI mapping errors: "
            );

            Serial.println(
                v.invalidWebAPIMappings
            );
        }


        if (v.ok())
        {
            Serial.println(
                "STATUS: OK"
            );
        }
        else
        {
            Serial.println(
                "STATUS: WARNING"
            );
        }
    }


    // ========================================================================
    // STRING HELPERS
    // ========================================================================

    static const char* entityKindToString(
        EntityKind kind)
    {
        switch (kind)
        {
            case EntityKind::UNKNOWN:
                return "UNKNOWN";

            case EntityKind::SENSOR:
                return "SENSOR";

            case EntityKind::BINARY_SENSOR:
                return "BINARY_SENSOR";

            case EntityKind::SWITCH:
                return "SWITCH";

            case EntityKind::LIGHT:
                return "LIGHT";

            case EntityKind::COVER:
                return "COVER";

            case EntityKind::CLIMATE:
                return "CLIMATE";

            case EntityKind::BUTTON:
                return "BUTTON";

            case EntityKind::NUMBER:
                return "NUMBER";

            case EntityKind::TEXT:
                return "TEXT";

            case EntityKind::STATUS:
                return "STATUS";
        }

        return "UNKNOWN";
    }


    static const char* relationTypeToString(
        RelationType type)
    {
        switch (type)
        {
            case RelationType::ROUTE:
                return "ROUTE";

            case RelationType::TOGGLE:
                return "TOGGLE";

            case RelationType::SPLIT:
                return "SPLIT";

            case RelationType::AUTOMATION_INPUT:
                return "AUTO_IN";

            case RelationType::AUTOMATION_OUTPUT:
                return "AUTO_OUT";

            case RelationType::AEE_AREA:
                return "AEE";

            case RelationType::MQTT_AREA:
                return "MQTT";

            case RelationType::WEBAPI_AEE:
                return "WEBAPI";
        }

        return "UNKNOWN";
    }
};