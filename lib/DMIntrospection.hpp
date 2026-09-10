
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
        POWER,
        AEE,
        EXTERNAL
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

        const DomoManagerConfig* domo = nullptr;


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
        // WEB API
        // --------------------------------------------------------------------

        const FrontendConfig::webApiDeviceMessaging*
            webApi = nullptr;
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


        // --------------------------------------------------------------------
        // RIFERIMENTI ORIGINALI
        // --------------------------------------------------------------------

        const DomoManagerBufferEngine::AreaConfig*
            areaCfg = nullptr;

        const DomoManagerConfig::Devices::Device*
            device = nullptr;

        int deviceIndex = -1;


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
        // INTEGRAZIONI
        // --------------------------------------------------------------------

        bool hasAEE = false;
        bool hasMQTT = false;
        bool hasWebAPI = false;


        // --------------------------------------------------------------------
        // ALTRE SORGENTI SEMANTICHE
        // --------------------------------------------------------------------

        bool fromSecurity = false;
        bool fromHVAC = false;
        bool fromAverage = false;
        bool fromPower = false;


        // --------------------------------------------------------------------
        // SCALING
        // --------------------------------------------------------------------

        float scale = 1.0f;


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

        const DeviceMessageProfile::InToRegistryMap*
            webApiMap = nullptr;
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
   
    void build(const Context& ctx)
    {
        clear();

        _ctx = ctx;

        if (!_ctx.domo)
            return;


        // --------------------------------------------------------------------
        // CORE DOMOMANAGER
        // --------------------------------------------------------------------

        inspectAreas(
            _ctx.domo->areas
        );

        inspectDevices(
            _ctx.domo->devices
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


        finalize();
    }


    // ========================================================================
    // BUILD MINIMO
    // ========================================================================
    //
    // Utilizzato attualmente da DMRuntime:
    //
    //     introspection.build(config);
    //
    // ========================================================================

    void build(const DomoManagerConfig& cfg)
    {
        clear();

        _ctx = Context{};

        _ctx.domo = &cfg;


        inspectAreas(
            cfg.areas
        );

        inspectDevices(
            cfg.devices
        );

        inspectRoutes(
            cfg.routes
        );

        inspectToggles(
            cfg.toggles
        );

        inspectSplits(
            cfg.splits
        );


        finalize();
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
    // DEVICES
    // ========================================================================
    //
    // ATTENZIONE:
    //
    // Non inseriamo DEVICE_AREA nelle relations.
    //
    // L'associazione area -> device è già contenuta in Entity.
    //
    // ========================================================================

    void inspectDevices(
        const DomoManagerConfig::Devices& cfg)
    {
        for (size_t deviceIndex = 0;
             deviceIndex < cfg.list.size();
             deviceIndex++)
        {
            const auto& device =
                cfg.list[deviceIndex];


            for (int area :
                 device.areas)
            {
                Entity& entity =
                    ensureEntity(area);


                if (!entity.device)
                {
                    entity.device =
                        &device;

                    entity.deviceIndex =
                        static_cast<int>(
                            deviceIndex
                        );
                }
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
        // DynamicAutomation contiene void* userCtx.
        //
        // Non è sicuro fare RTTI/cast qui senza introdurre
        // una API esplicita nell'AutomationEngine.
        //
        // Per ora la indicizziamo solamente.
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

            Automation info;

            info.type =
                AutomationType::RULE;

            info.name =
                nullptr;

            info.valid =
                dynamic->callback ||
                dynamic->callbackCtx;

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
                    def.name;

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
                        EntityOrigin::EXTERNAL;


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


    // ========================================================================
    // WEB API
    // ========================================================================

    void inspectWebAPI(
        const FrontendConfig::webApiDeviceMessaging& webApi)
    {
        if (!webApi.groups)
            return;


        if (!_ctx.aeeRegistry)
            return;


        for (size_t groupIndex = 0;
             groupIndex < webApi.groupCount;
             groupIndex++)
        {
            const auto& group =
                webApi.groups[groupIndex];


            if (!group.profiles)
                continue;


            for (size_t profileIndex = 0;
                 profileIndex < group.profileCount;
                 profileIndex++)
            {
                const DeviceMessageProfile& profile =
                    group.profiles[profileIndex];


                for (size_t mapIndex = 0;
                     mapIndex < profile.regMapCount;
                     mapIndex++)
                {
                    const auto& map =
                        profile.regMap[mapIndex];


                    AEEVariableBase* aee =
                        _ctx.aeeRegistry->find(
                            String(map.regKey)
                        );


                    if (!aee)
                        continue;


                    const int area =
                        resolveAEEArea(
                            aee->def
                        );


                    if (area < 0)
                        continue;


                    Entity& entity =
                        ensureEntity(area);


                    entity.hasWebAPI =
                        true;


                    entity.origin =
                        EntityOrigin::EXTERNAL;


                    Relation relation;

                    relation.type =
                        RelationType::WEBAPI_AEE;

                    relation.sourceArea =
                        area;

                    relation.targetArea =
                        area;

                    relation.name =
                        profile.name;

                    relation.aee =
                        aee;

                    relation.webApiProfile =
                        &profile;

                    relation.webApiMap =
                        &map;


                    _relations.push_back(
                        relation
                    );
                }
            }
        }
    }


    // ========================================================================
    // FINALIZE
    // ========================================================================

    void finalize()
    {
        for (auto& entity :
             _entities)
        {
            //
            // L'area è comparsa solo perché usata
            // come target/relazione ma non ha una
            // descrizione AreaConfig.
            //
            if (!entity.areaCfg)
            {
                entity.implementationOnly =
                    true;
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
            //
            // Se vuoi ancora meno rumore:
            //
            // le aree completamente interne possono
            // essere omesse dal report.
            //
            if (entity.implementationOnly)
                continue;


            String line;

            line.reserve(180);


            line += "AREA ";
            line += String(entity.area);


            const char* name =
                getAreaName(entity.area);


            if (name && name[0] != '\0')
            {
                line += " | ";
                line += name;
            }


            if (entity.device)
            {
                line += " | DEVICE=";
                line += entity.device->name;
            }


            if (entity.kind !=
                EntityKind::UNKNOWN)
            {
                line += " | KIND=";

                line +=
                    entityKindToString(
                        entity.kind
                    );
            }


            if (entity.writable)
                line += " | WRITABLE";


            if (entity.isRouteTrigger)
                line += " | ROUTE";


            if (entity.isToggleSource)
                line += " | TOGGLE";


            if (entity.isSplitMain)
                line += " | SPLIT";


            if (entity.isRouteTarget)
                line += " | ROUTE_TARGET";


            if (entity.isToggleTarget)
                line += " | TOGGLE_TARGET";


            if (entity.isSplitTarget)
                line += " | SPLIT_TARGET";


            if (entity.isAutomationInput)
                line += " | AUTO_IN";


            if (entity.isAutomationOutput)
                line += " | AUTO_OUT";


            if (entity.hasAEE)
                line += " | AEE";


            if (entity.hasMQTT)
                line += " | MQTT";


            if (entity.hasWebAPI)
                line += " | WEBAPI";


            if (entity.fromSecurity)
                line += " | SECURITY";


            if (entity.fromHVAC)
                line += " | HVAC";


            if (entity.fromAverage)
                line += " | AVERAGE";


            if (entity.fromPower)
                line += " | POWER";


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

                    if (relation.webApiMap)
                    {
                        line += " | IN=";
                        line +=
                            relation.webApiMap->inKey;

                        line += " -> ";
                        line +=
                            relation.webApiMap->regKey;
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