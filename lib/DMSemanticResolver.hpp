
// ============================================================================
// DMSemanticResolver.hpp
// ============================================================================
//
// RISOLUTORE SEMANTICO DOMOMANAGER
//
// Input:
//     DomoIntrospection
//
// Output:
//     EntityKind
//     unit
//     deviceClass
//     stateClass
//     writable
//     implementationOnly
//
// Principio:
//     NON duplica la configurazione.
//     NON modifica DomoManagerConfig.
//     Arricchisce solamente le Entity già indicizzate.
//
// Precedenza:
//
//     1. Security / fonti specializzate (quando disponibili)
//     2. GenericSensor
//     3. Route / Split / Toggle
//     4. label area
//     5. fallback UNKNOWN
//
// ============================================================================

#pragma once

#include "DMIntrospection.hpp"

class DomoSemanticResolver
{
public:

    // ========================================================================
    // SEMANTIC TOKENS
    // ========================================================================

    enum class SemanticToken
    {
        NONE,

        // Sensors
        TEMPERATURE,
        HUMIDITY,
        ILLUMINANCE,
        PRESSURE,
        VOLTAGE,
        CURRENT,
        POWER,
        FREQUENCY,
        CARBON_MONOXIDE,
        LEVEL,
        GENERIC_STATUS,

        // Binary sensors
        MOTION,
        TAMPER,
        SMOKE,
        FLOOD,
        DOOR,
        WINDOW,

        // Generic entities
        LIGHT,
        BUTTON,
        SWITCH_INPUT,
        COVER,
        VALVE,
        BUZZER,
        FAN,
        NUMBER,
        SOLAR_GROUP,
        SECURITY_GROUP,

        // Commands / compound states
        OPEN_CLOSE,
        ON_OFF,

        // RGBW / composite
        RED,
        GREEN,
        BLUE,
        WHITE,
        MAIN,

        // State / direction
        OPEN,
        CLOSE,

        SECURITY
        
    };


    // ========================================================================
    // DICTIONARY
    // ========================================================================

    struct DictionaryEntry
    {
        SemanticToken token;

        const char* const* words;

        size_t wordCount;

        /*
         * true  = parola intera
         * false = substring
         */
        bool wholeWord;
    };


    struct Dictionary
    {
        const DictionaryEntry* entries;

        size_t entryCount;
    };


    // ========================================================================
    // RESULT
    // ========================================================================

    struct Result
    {
        size_t total = 0;

        size_t sensors = 0;
        size_t binarySensors = 0;
        size_t switches = 0;
        size_t lights = 0;
        size_t covers = 0;
        size_t climates = 0;
        size_t buttons = 0;
        size_t numbers = 0;
        size_t texts = 0;
        size_t status = 0;

        size_t unknown = 0;

        size_t hidden = 0;

        size_t writable = 0;
    };


    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    DomoSemanticResolver()
        : _dictionary(
            &italianDictionary())
    {
    }


    explicit DomoSemanticResolver(
        const Dictionary& dictionary)
        : _dictionary(&dictionary)
    {
    }


    // ========================================================================
    // ITALIAN DICTIONARY
    // ========================================================================

    static const Dictionary& italianDictionary()
    {
        static const char* solarGroupWords[] =
        {
            "fotovoltaico",
            "inverter",
            "batteria inverter"
        };

        static const char* securityGroupWords[] =
        {
            "antifurto",
            "security",
            "allarme telecamera"
        };

        static const char* switchInputWords[] =
        {
            "interruttore",
            "int."
        };

        static const char* numberWords[] =
        {
            "setpoint",
            "soglia",
            "threshold",
            "dimmer",
            "intensita",
            "intensità",
            "timeout",
            "ritardo",
            "delay",
            "tempo",
            "durata",
            "volume",
            "velocita",
            "velocità"
        };

        static const char* levelWords[] =
        {
            "percentuale",
            "percento",
            "livello",
            "soc"
        };

        static const char* genericStatusWords[] =
        {
            "stato inverter",
            "stato comunicazione",
            "stato distacco",
            "stato antifurto",
            "allarme telecamera"
        };

        static const char* securityWords[] =
        {
            "security",
            "allarme",
            "alarm",
            "intrusione",
            "antifurto"
        };

        static const char* fanWords[] =
        {
            "estrattore",
            "fan",
            "ventola",
            "cappa"
        };

        // --------------------------------------------------------------------
        // SENSORI
        // --------------------------------------------------------------------

        static const char* temperatureWords[] =
        {
            "temperatura",
            "temp"
        };

        static const char* humidityWords[] =
        {
            "umidita",
            "umidità"
        };

        static const char* illuminanceWords[] =
        {
            "luminosita",
            "luminosità",
            "lux",
            "illuminazione"
        };

        static const char* pressureWords[] =
        {
            "pressione"
        };

        static const char* voltageWords[] =
        {
            "tensione",
            "voltaggio"
        };

        static const char* currentWords[] =
        {
            "corrente"
        };

        static const char* powerWords[] =
        {
            "potenza",
            "consumo"
        };

        static const char* frequencyWords[] =
        {
            "frequenza"
        };

        static const char* carbonMonoxideWords[] =
        {
            "co",
            "monossido",
            "monossido di carbonio"
        };


        // --------------------------------------------------------------------
        // SICUREZZA
        // --------------------------------------------------------------------

        static const char* motionWords[] =
        {
            "pir",
            "movimento"
        };

        static const char* tamperWords[] =
        {
            "tamper",
            "manomissione"
        };

        static const char* smokeWords[] =
        {
            "fumo"
        };

        static const char* floodWords[] =
        {
            "allagamento",
            "allagata",
            "allagato"
        };

        static const char* doorWords[] =
        {
            "porta"
        };

        static const char* windowWords[] =
        {
            "finestra",
            "finestre"
        };


        // --------------------------------------------------------------------
        // ATTUATORI / CONTROLLI
        // --------------------------------------------------------------------

        static const char* lightWords[] =
        {
            "luce",
            "luci",
            "led",
            "lampada",
            "plafoniera"
        };

        static const char* buttonWords[] =
        {
            "pulsante",
            "tasto"
        };

        static const char* coverWords[] =
        {
            "vasistas",
            "serranda",
            "tapparella",
            "persiana",
            "scuro",
            "scuri"
        };

        static const char* valveWords[] =
        {
            "valvola"
        };

        static const char* buzzerWords[] =
        {
            "buzzer",
            "cicalino"
        };


        // --------------------------------------------------------------------
        // COMANDI COMPOSTI
        // --------------------------------------------------------------------

        static const char* openCloseWords[] =
        {
            "open/close",
            "apertura/chiusura",
            "apri/chiudi"
        };

        static const char* onOffWords[] =
        {
            "on/off",
            "on off",
            "on-off",
            "acceso/spento",
            "accensione/spegnimento"
        };


        // --------------------------------------------------------------------
        // RGBW
        // --------------------------------------------------------------------

        static const char* redWords[] =
        {
            "rosso",
            "rossa",
            "red",
            "r"
        };

        static const char* greenWords[] =
        {
            "verde",
            "green",
            "g"
        };

        static const char* blueWords[] =
        {
            "blu",
            "blue",
            "b"
        };

        static const char* whiteWords[] =
        {
            "bianco",
            "bianca",
            "white",
            "w"
        };

        static const char* mainWords[] =
        {
            "main",
            "principale"
        };


        // --------------------------------------------------------------------
        // DIREZIONE / STATO
        // --------------------------------------------------------------------

        static const char* openWords[] =
        {
            "apertura",
            "apri",
            "aperto",
            "aperta",
            "aperti",
            "aperte"
        };

        static const char* closeWords[] =
        {
            "chiusura",
            "chiudi",
            "chiuso",
            "chiusa",
            "chiusi",
            "chiuse"
        };


        // --------------------------------------------------------------------
        // ENTRY
        // --------------------------------------------------------------------

        static const DictionaryEntry entries[] =
        {
            // Sensors
            {
                SemanticToken::TEMPERATURE,
                temperatureWords,
                sizeof(temperatureWords) /
                    sizeof(temperatureWords[0]),
                false
            },

            {
                SemanticToken::HUMIDITY,
                humidityWords,
                sizeof(humidityWords) /
                    sizeof(humidityWords[0]),
                false
            },

            {
                SemanticToken::ILLUMINANCE,
                illuminanceWords,
                sizeof(illuminanceWords) /
                    sizeof(illuminanceWords[0]),
                false
            },

            {
                SemanticToken::PRESSURE,
                pressureWords,
                sizeof(pressureWords) /
                    sizeof(pressureWords[0]),
                false
            },

            {
                SemanticToken::VOLTAGE,
                voltageWords,
                sizeof(voltageWords) /
                    sizeof(voltageWords[0]),
                false
            },

            {
                SemanticToken::CURRENT,
                currentWords,
                sizeof(currentWords) /
                    sizeof(currentWords[0]),
                false
            },

            {
                SemanticToken::POWER,
                powerWords,
                sizeof(powerWords) /
                    sizeof(powerWords[0]),
                false
            },

            {
                SemanticToken::FREQUENCY,
                frequencyWords,
                sizeof(frequencyWords) /
                    sizeof(frequencyWords[0]),
                false
            },

            {
                SemanticToken::CARBON_MONOXIDE,
                carbonMonoxideWords,
                sizeof(carbonMonoxideWords) /
                    sizeof(carbonMonoxideWords[0]),
                true
            },


            // Binary sensors

            {
                SemanticToken::MOTION,
                motionWords,
                sizeof(motionWords) /
                    sizeof(motionWords[0]),
                false
            },

            {
                SemanticToken::TAMPER,
                tamperWords,
                sizeof(tamperWords) /
                    sizeof(tamperWords[0]),
                false
            },

            {
                SemanticToken::SMOKE,
                smokeWords,
                sizeof(smokeWords) /
                    sizeof(smokeWords[0]),
                false
            },

            {
                SemanticToken::FLOOD,
                floodWords,
                sizeof(floodWords) /
                    sizeof(floodWords[0]),
                false
            },

            {
                SemanticToken::DOOR,
                doorWords,
                sizeof(doorWords) /
                    sizeof(doorWords[0]),
                false
            },

            {
                SemanticToken::WINDOW,
                windowWords,
                sizeof(windowWords) /
                    sizeof(windowWords[0]),
                false
            },


            // Actuators / controls
            {
                SemanticToken::SWITCH_INPUT,
                switchInputWords,
                sizeof(switchInputWords) /
                    sizeof(switchInputWords[0]),
                false
            },

            {
                SemanticToken::LIGHT,
                lightWords,
                sizeof(lightWords) /
                    sizeof(lightWords[0]),
                false
            },

            {
                SemanticToken::BUTTON,
                buttonWords,
                sizeof(buttonWords) /
                    sizeof(buttonWords[0]),
                false
            },

            {
                SemanticToken::COVER,
                coverWords,
                sizeof(coverWords) /
                    sizeof(coverWords[0]),
                false
            },

            {
                SemanticToken::VALVE,
                valveWords,
                sizeof(valveWords) /
                    sizeof(valveWords[0]),
                false
            },

            {
                SemanticToken::BUZZER,
                buzzerWords,
                sizeof(buzzerWords) /
                    sizeof(buzzerWords[0]),
                false
            },

            {
                SemanticToken::FAN,
                fanWords,
                sizeof(fanWords) /
                    sizeof(fanWords[0]),
                false
            },

            {
                SemanticToken::LEVEL,
                levelWords,
                sizeof(levelWords) /
                    sizeof(levelWords[0]),
                false
            },

            {
                SemanticToken::GENERIC_STATUS,
                genericStatusWords,
                sizeof(genericStatusWords) /
                    sizeof(genericStatusWords[0]),
                false
            },

            {
                SemanticToken::NUMBER,
                numberWords,
                sizeof(numberWords) /
                    sizeof(numberWords[0]),
                false
            },

            // Compound commands

            {
                SemanticToken::OPEN_CLOSE,
                openCloseWords,
                sizeof(openCloseWords) /
                    sizeof(openCloseWords[0]),
                false
            },

            {
                SemanticToken::ON_OFF,
                onOffWords,
                sizeof(onOffWords) /
                    sizeof(onOffWords[0]),
                false
            },


            // RGBW

            {
                SemanticToken::RED,
                redWords,
                sizeof(redWords) /
                    sizeof(redWords[0]),
                true
            },

            {
                SemanticToken::GREEN,
                greenWords,
                sizeof(greenWords) /
                    sizeof(greenWords[0]),
                true
            },

            {
                SemanticToken::BLUE,
                blueWords,
                sizeof(blueWords) /
                    sizeof(blueWords[0]),
                true
            },

            {
                SemanticToken::WHITE,
                whiteWords,
                sizeof(whiteWords) /
                    sizeof(whiteWords[0]),
                true
            },

            {
                SemanticToken::MAIN,
                mainWords,
                sizeof(mainWords) /
                    sizeof(mainWords[0]),
                false
            },


            // Direction / state

            {
                SemanticToken::OPEN,
                openWords,
                sizeof(openWords) /
                    sizeof(openWords[0]),
                false
            },

            {
                SemanticToken::CLOSE,
                closeWords,
                sizeof(closeWords) /
                    sizeof(closeWords[0]),
                false
            },

            {
                SemanticToken::SOLAR_GROUP,
                solarGroupWords,
                sizeof(solarGroupWords) /
                    sizeof(solarGroupWords[0]),
                false
            },

            {
                SemanticToken::SECURITY_GROUP,
                securityGroupWords,
                sizeof(securityGroupWords) /
                    sizeof(securityGroupWords[0]),
                false
            }
        };


        static const Dictionary dictionary =
        {
            entries,
            sizeof(entries) /
                sizeof(entries[0])
        };


        return dictionary;
    }


    // ========================================================================
    // RESOLVE
    // ========================================================================

    Result resolve(
        DomoIntrospection& introspection)
    {
        return resolve(
            introspection,
            *_dictionary
        );
    }


    // ========================================================================
    // RESOLVE WITH DICTIONARY
    // ========================================================================

    Result resolve(
        DomoIntrospection& introspection,
        const Dictionary& dictionary)
    {
        _result = Result{};

        _dictionary = &dictionary;


        // --------------------------------------------------------------------
        // PASS 1
        // Individual semantics
        //
        // L'accesso avviene per indice e non tramite area.
        //
        // Le Runtime Entity non possiedono un'area Buffer e utilizzano area=-1.
        // L'uso di findEntity(source.area) provocherebbe collisioni tra tutte
        // le Runtime Entity.
        // --------------------------------------------------------------------

        for (size_t i = 0;
            i < introspection.entityCount();
            ++i)
        {
            DomoIntrospection::Entity* entity =
                introspection.getEntity(i);

            if (!entity)
                continue;

            resolveEntity(
                introspection,
                *entity
            );
        }


        // --------------------------------------------------------------------
        // PASS 2
        // Derived semantics
        // --------------------------------------------------------------------

        resolveRoutes(
            introspection
        );

        resolveSplits(
            introspection
        );

        resolveToggles(
            introspection
        );

        resolveDerivedAreas(
            introspection
        );


        // --------------------------------------------------------------------
        // PASS 3
        // Result
        // --------------------------------------------------------------------

        calculateResult(
            introspection
        );


        return _result;
    }


    // ========================================================================
    // REPORT
    // ========================================================================

    void report(
        const DomoIntrospection& introspection) const
    {
        Serial.println();

        Serial.println(
            "----- SEMANTIC MODEL -----"
        );

        for (size_t i = 0;
            i < introspection.entityCount();
            ++i)
        {
            const auto* entity =
                introspection.getEntity(i);

            if (!entity)
                continue;

            if (entity->implementationOnly)
                continue;

            String line;

            line.reserve(
                256
            );

            const bool isRuntime =
                entity->origin ==
                    DomoIntrospection::
                        EntityOrigin::RUNTIME;

            // ========================================================
            // ENTITY IDENTIFIER
            // ========================================================

            if (isRuntime)
            {
                line +=
                    "RUNTIME ";

                if (entity->runtimeEntry &&
                    entity->runtimeEntry->id &&
                    entity->runtimeEntry->id[0] != '\0')
                {
                    line +=
                        entity->runtimeEntry->id;
                }
                else
                {
                    line +=
                        "HANDLE ";

                    line +=
                        String(
                            static_cast<unsigned>(
                                entity->runtimeHandle
                            )
                        );
                }
            }
            else
            {
                line +=
                    "AREA ";

                line +=
                    String(
                        entity->area
                    );
            }

            // ========================================================
            // ENTITY NAME
            // ========================================================

            const char* name =
                nullptr;

            if (isRuntime)
            {
                if (entity->runtimeEntry)
                {
                    name =
                        entity->runtimeEntry->name;

                    if (!name ||
                        name[0] == '\0')
                    {
                        name =
                            entity->runtimeEntry->id;
                    }
                }
            }
            else
            {
                name =
                    introspection.getAreaName(
                        entity->area
                    );
            }

            if (name &&
                name[0] != '\0')
            {
                line +=
                    " | ";

                line +=
                    name;
            }

            // ========================================================
            // ENTITY KIND
            // ========================================================

            line +=
                " | KIND=";

            line +=
                entityKindToString(
                    entity->kind
                );

            // ========================================================
            // ENTITY GROUP
            // ========================================================

            line +=
                " | GROUP=";

            switch (entity->group)
            {
                case DomoIntrospection::
                    EntityGroup::ROOT:
                {
                    line +=
                        "ROOT";

                    break;
                }

                case DomoIntrospection::
                    EntityGroup::PHYSICAL_DEVICE:
                {
                    line +=
                        "PHYSICAL_DEVICE";

                    break;
                }

                case DomoIntrospection::
                    EntityGroup::SECURITY:
                {
                    line +=
                        "SECURITY";

                    break;
                }

                case DomoIntrospection::
                    EntityGroup::SOLAR:
                {
                    line +=
                        "SOLAR";

                    break;
                }

                default:
                {
                    line +=
                        "UNKNOWN";

                    break;
                }
            }

            // ========================================================
            // SEMANTIC METADATA
            // ========================================================

            if (entity->unit &&
                entity->unit[0] != '\0')
            {
                line +=
                    " | UNIT=";

                line +=
                    entity->unit;
            }

            if (entity->deviceClass &&
                entity->deviceClass[0] != '\0')
            {
                line +=
                    " | CLASS=";

                line +=
                    entity->deviceClass;
            }

            if (entity->stateClass &&
                entity->stateClass[0] != '\0')
            {
                line +=
                    " | STATE=";

                line +=
                    entity->stateClass;
            }

            // ========================================================
            // ACCESS
            // ========================================================

            if (entity->writable)
            {
                line +=
                    " | WRITABLE";
            }

            Serial.println(
                line
            );
        }

        Serial.println();

        Serial.println(
            "----- SEMANTIC SUMMARY -----"
        );

        Serial.print(
            "Sensors: "
        );

        Serial.println(
            _result.sensors
        );

        Serial.print(
            "Binary sensors: "
        );

        Serial.println(
            _result.binarySensors
        );

        Serial.print(
            "Switches: "
        );

        Serial.println(
            _result.switches
        );

        Serial.print(
            "Lights: "
        );

        Serial.println(
            _result.lights
        );

        Serial.print(
            "Covers: "
        );

        Serial.println(
            _result.covers
        );

        Serial.print(
            "Climate: "
        );

        Serial.println(
            _result.climates
        );

        Serial.print(
            "Buttons: "
        );

        Serial.println(
            _result.buttons
        );

        Serial.print(
            "Numbers: "
        );

        Serial.println(
            _result.numbers
        );

        Serial.print(
            "Texts: "
        );

        Serial.println(
            _result.texts
        );

        Serial.print(
            "Status: "
        );

        Serial.println(
            _result.status
        );

        Serial.print(
            "Unknown: "
        );

        Serial.println(
            _result.unknown
        );

        Serial.print(
            "Hidden: "
        );

        Serial.println(
            _result.hidden
        );

        Serial.print(
            "Writable: "
        );

        Serial.println(
            _result.writable
        );
    }


private:

    Result _result;

    const Dictionary* _dictionary;


    // ========================================================================
    // ENTITY RESOLUTION
    // ========================================================================

    bool resolveLevel(
        const String& text,
        DomoIntrospection::Entity& entity)
    {
        if (!hasToken(
                text,
                SemanticToken::LEVEL))
        {
            return false;
        }

        setSensor(
            entity,
            "%",
            nullptr,
            "measurement"
        );

        return true;
    }

    bool resolveGenericStatus(
        const String& text,
        DomoIntrospection::Entity& entity)
    {
        if (!hasToken(
                text,
                SemanticToken::GENERIC_STATUS))
        {
            return false;
        }

        setSensor(
            entity,
            nullptr,
            nullptr,
            nullptr
        );

        return true;
    }

    void resolveEntityGroup(
        DomoIntrospection::Entity& entity)
    {
        using Group =
            DomoIntrospection::EntityGroup;

        // ============================================================
        // STRUCTURAL SOURCES
        // ============================================================

        if (entity.fromSecurity)
        {
            entity.group =
                Group::SECURITY;

            return;
        }

        if (entity.fromSolar)
        {
            entity.group =
                Group::SOLAR;

            return;
        }

        // ============================================================
        // RUNTIME STORE
        // ============================================================

        if (entity.origin ==
            DomoIntrospection::EntityOrigin::RUNTIME)
        {
            entity.group =
                Group::ROOT;

            return;
        }

        // ============================================================
        // SEMANTIC FALLBACK
        // ============================================================

        if (entity.areaCfg)
        {
            const char* label =
                entity.areaCfg->label.c_str();

            if (label &&
                label[0] != '\0')
            {
                String text(
                    label
                );

                if (hasToken(
                        text,
                        SemanticToken::SOLAR_GROUP))
                {
                    entity.group =
                        Group::SOLAR;

                    return;
                }

                if (hasToken(
                        text,
                        SemanticToken::SECURITY_GROUP))
                {
                    entity.group =
                        Group::SECURITY;

                    return;
                }
            }
        }

        // ============================================================
        // PHYSICAL DEVICE
        // ============================================================

        if (entity.device &&
            entity.deviceIndex >= 0)
        {
            entity.group =
                Group::PHYSICAL_DEVICE;

            return;
        }

        // ============================================================
        // ROOT
        // ============================================================

        entity.group =
            Group::ROOT;
    }

    void resolveEntity(
        DomoIntrospection&,
        DomoIntrospection::Entity& entity)
    {
        // ============================================================
        // DEVICE GROUP
        // ============================================================

        resolveEntityGroup(
            entity
        );

        // ============================================================
        // RUNTIME STORE
        // ============================================================

        if (entity.origin ==
            DomoIntrospection::EntityOrigin::RUNTIME)
        {
            if (!entity.runtimeEntry)
            {
                entity.kind =
                    DomoIntrospection::EntityKind::UNKNOWN;

                entity.implementationOnly =
                    true;

                return;
            }

            entity.readable =
                entity.runtimeEntry->readable();

            entity.writable =
                entity.runtimeEntry->writable();

            entity.implementationOnly =
                !entity.runtimeEntry->publish;

            return;
        }

        // ============================================================
        // AREA ENTITY
        // ============================================================

        if (!entity.areaCfg)
            return;

        const char* label =
            entity.areaCfg->label.c_str();

        if (!label ||
            label[0] == '\0')
        {
            return;
        }

        String text(
            label
        );

        // ============================================================
        // SEMANTIC PRECEDENCE
        // ============================================================

        if (resolveLevel(
                text,
                entity))
        {
            return;
        }

        if (resolveGenericStatus(
                text,
                entity))
        {
            return;
        }

        // ============================================================
        // STANDARD RESOLUTION
        // ============================================================

        resolveByLabel(
            label,
            entity
        );
    }

    // ========================================================================
    // LABEL RESOLUTION
    // ========================================================================

    void resolveByLabel(
        const char* label,
        DomoIntrospection::Entity& entity)
    {
        if (!label || !*label)
            return;


        String text(label);


        // --------------------------------------------------------------------
        // BUTTON
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::BUTTON))
        {
            setButton(entity);
            return;
        }

        if (hasToken(
                text,
                SemanticToken::SWITCH_INPUT))
        {
            setSwitch(
                entity
            );

            entity.writable = false;

            return;
        }

        // --------------------------------------------------------------------
        // NUMBER
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::NUMBER))
        {
            setNumber(
                entity
            );

            return;
        }

        // --------------------------------------------------------------------
        // TAMPER
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::TAMPER))
        {
            setBinarySensor(
                entity,
                "tamper"
            );

            return;
        }


        // --------------------------------------------------------------------
        // SMOKE
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::SMOKE))
        {
            setBinarySensor(
                entity,
                "smoke"
            );

            return;
        }


        // --------------------------------------------------------------------
        // FLOOD
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::FLOOD))
        {
            setBinarySensor(
                entity,
                "moisture"
            );

            return;
        }


        // --------------------------------------------------------------------
        // DOOR
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::DOOR))
        {
            setBinarySensor(
                entity,
                "door"
            );

            return;
        }


        // --------------------------------------------------------------------
        // WINDOW / COVER FEEDBACK
        //
        // Esempi:
        //
        //   "Vasistas dx. chiuso"
        //   "Serranda sx. aperta"
        //
        // ATTENZIONE:
        // questa regola è puramente lessicale.
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::COVER) &&
            (
                hasToken(
                    text,
                    SemanticToken::OPEN
                ) ||
                hasToken(
                    text,
                    SemanticToken::CLOSE
                )
            ))
        {
            setBinarySensor(
                entity,
                "window"
            );

            return;
        }


        // --------------------------------------------------------------------
        // MOTION
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::MOTION))
        {
            setBinarySensor(
                entity,
                "motion"
            );

            return;
        }


        // --------------------------------------------------------------------
        // CARBON MONOXIDE
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::CARBON_MONOXIDE))
        {
            setSensor(
                entity,
                "ppm",
                "carbon_monoxide",
                "measurement"
            );

            return;
        }


        // --------------------------------------------------------------------
        // TEMPERATURE
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::TEMPERATURE))
        {
            setSensor(
                entity,
                "°C",
                "temperature",
                "measurement"
            );

            return;
        }


        // --------------------------------------------------------------------
        // HUMIDITY
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::HUMIDITY))
        {
            setSensor(
                entity,
                "%",
                "humidity",
                "measurement"
            );

            return;
        }


        // --------------------------------------------------------------------
        // ILLUMINANCE
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::ILLUMINANCE))
        {
            setSensor(
                entity,
                "lx",
                "illuminance",
                "measurement"
            );

            return;
        }


        // --------------------------------------------------------------------
        // PRESSURE
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::PRESSURE))
        {
            setSensor(
                entity,
                "bar",
                "pressure",
                "measurement"
            );

            return;
        }


        // --------------------------------------------------------------------
        // VOLTAGE
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::VOLTAGE))
        {
            setSensor(
                entity,
                "V",
                "voltage",
                "measurement"
            );

            return;
        }


        // --------------------------------------------------------------------
        // CURRENT
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::CURRENT))
        {
            setSensor(
                entity,
                "A",
                "current",
                "measurement"
            );

            return;
        }


        // --------------------------------------------------------------------
        // POWER
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::POWER))
        {
            setSensor(
                entity,
                "W",
                "power",
                "measurement"
            );

            return;
        }


        // --------------------------------------------------------------------
        // FREQUENCY
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::FREQUENCY))
        {
            setSensor(
                entity,
                "Hz",
                "frequency",
                "measurement"
            );

            return;
        }


        // --------------------------------------------------------------------
        // VALVE
        // --------------------------------------------------------------------
        //
        // Senza informazione strutturale DI/DO, la label "Valvola ... aperta"
        // non consente di distinguere un feedback da un attuatore.
        //
        // Pertanto VALVE resta SWITCH.
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::VALVE))
        {
            setSwitch(entity);
            return;
        }


        // --------------------------------------------------------------------
        // BUZZER
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::BUZZER))
        {
            setSwitch(entity);
            return;
        }

        // --------------------------------------------------------------------
        // FAN / EXTRACTOR
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::FAN))
        {
            setSwitch(entity);
            return;
        }

        // --------------------------------------------------------------------
        // OPEN/CLOSE command
        //
        // Esempi:
        //
        //   "Open/close scuro"
        //   "Apertura/chiusura ..."
        //
        // Tutto passa dal dizionario.
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::OPEN_CLOSE))
        {
            setSwitch(entity);
            return;
        }


        // --------------------------------------------------------------------
        // ON/OFF command
        //
        // Esempi:
        //
        //   "On/off scuro"
        //   "Acceso/spento ..."
        //
        // Tutto passa dal dizionario.
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::ON_OFF))
        {
            setSwitch(entity);
            return;
        }


        // --------------------------------------------------------------------
        // LIGHT / LED
        // --------------------------------------------------------------------
        //
        // Una descrizione luminosa rappresenta una LIGHT solamente quando
        // l'area è associata a un'uscita fisica.
        //
        // Regole:
        //
        //     LIGHT + DIGITAL_OUTPUT
        //         -> LIGHT
        //
        //     LIGHT + ANALOG_OUTPUT
        //         -> LIGHT
        //
        //     LIGHT + DIGITAL_INPUT
        //         -> nessuna classificazione luminosa
        //
        //     LIGHT + ANALOG_INPUT
        //         -> nessuna classificazione luminosa
        //
        //     LIGHT + UNKNOWN / VIRTUAL
        //         -> nessuna classificazione automatica
        //
        // Esempi:
        //
        //     "Luce letto 1" + DO
        //         -> LIGHT
        //
        //     "Luce cucina 1" + DO
        //         -> LIGHT
        //
        //     "DI: Int. Luce bagno" + DI
        //         -> non LIGHT
        //
        // Non viene utilizzato il prefisso testuale DI: o DO:.
        // Il tipo deriva esclusivamente dal DeviceProfiles.
        //
        // --------------------------------------------------------------------

        if (hasToken(
                text,
                SemanticToken::LIGHT))
        {
            if (entity.isOutput())
            {
                setLight(
                    entity
                );

                return;
            }

            /*
            * La parola "luce" su un ingresso descrive la funzione
            * comandata, non il tipo dell'entity fisica.
            *
            * Non eseguiamo return, così eventuali regole strutturali
            * successive possono ancora classificare l'entity.
            */
        }


        // --------------------------------------------------------------------
        // Nessuna semantica da label.
        // --------------------------------------------------------------------
    }


    // ========================================================================
    // ROUTES
    // ========================================================================
    //
    // Una ROUTE viene considerata LIGHT composita quando i target
    // contengono almeno:
    //
    //   RED
    //   GREEN
    //   BLUE
    //
    // e almeno:
    //
    //   WHITE
    //   oppure MAIN
    //
    // Una volta riconosciuta la route RGBW:
    //
    //   source = LIGHT
    //   tutti i target = implementationOnly
    //
    // Non vengono interpretati per ID.
    //
    // ========================================================================

    void resolveRoutes(
        DomoIntrospection& introspection)
    {
        for (size_t i = 0;
             i < introspection.entityCount();
             i++)
        {
            DomoIntrospection::Entity* source =
                introspection.getEntity(i);

            if (!source)
                continue;


            if (!source->isRouteTrigger)
                continue;


            bool hasRed = false;
            bool hasGreen = false;
            bool hasBlue = false;
            bool hasWhite = false;
            bool hasMain = false;


            // ----------------------------------------------------------------
            // Analisi target
            // ----------------------------------------------------------------

            for (size_t r = 0;
                 r < introspection.relationCount();
                 r++)
            {
                const auto* relation =
                    introspection.getRelation(r);

                if (!relation)
                    continue;


                if (relation->type !=
                    DomoIntrospection::RelationType::ROUTE)
                    continue;


                if (relation->sourceArea !=
                    source->area)
                    continue;


                const auto* target =
                    introspection.findEntity(
                        relation->targetArea
                    );

                if (!target)
                    continue;


                if (!target->areaCfg)
                    continue;


                const char* label =
                    target->areaCfg->label.c_str();


                if (!label || !*label)
                    continue;


                String targetText(label);


                if (hasToken(
                        targetText,
                        SemanticToken::RED))
                {
                    hasRed = true;
                }


                if (hasToken(
                        targetText,
                        SemanticToken::GREEN))
                {
                    hasGreen = true;
                }


                if (hasToken(
                        targetText,
                        SemanticToken::BLUE))
                {
                    hasBlue = true;
                }


                if (hasToken(
                        targetText,
                        SemanticToken::WHITE))
                {
                    hasWhite = true;
                }


                if (hasToken(
                        targetText,
                        SemanticToken::MAIN))
                {
                    hasMain = true;
                }
            }


            const bool isRGBW =
                hasRed &&
                hasGreen &&
                hasBlue &&
                (hasWhite || hasMain);


            if (!isRGBW)
                continue;


            // ----------------------------------------------------------------
            // SOURCE SEMANTICS
            // ----------------------------------------------------------------
            //
            // La route descrive la relazione fra il comando e i canali RGBW,
            // ma non trasforma automaticamente un ingresso fisico in LIGHT.
            //
            // Solamente una sorgente fisicamente configurata come DO/AO può
            // essere pubblicata direttamente come LIGHT.
            //
            // Un ingresso DI/AI rimane un comando fisico e non rappresenta
            // l'attuatore luminoso.
            //
            // ----------------------------------------------------------------

            if (source->isOutput())
            {
                setLight(
                    *source
                );

                source->implementationOnly =
                    false;
            }


            // ----------------------------------------------------------------
            // Tutti i target sono implementation details
            // ----------------------------------------------------------------

            for (size_t r = 0;
                 r < introspection.relationCount();
                 r++)
            {
                const auto* relation =
                    introspection.getRelation(r);

                if (!relation)
                    continue;


                if (relation->type !=
                    DomoIntrospection::RelationType::ROUTE)
                    continue;


                if (relation->sourceArea !=
                    source->area)
                    continue;


                DomoIntrospection::Entity* target =
                    introspection.findEntity(
                        relation->targetArea
                    );

                if (!target)
                    continue;


                target->implementationOnly = true;
            }
        }
    }


    // ========================================================================
    // SPLITS
    // ========================================================================

    void resolveSplits(
        DomoIntrospection& introspection)
    {
        std::vector<int> sources;

        sources.reserve(
            introspection.relationCount()
        );


        // --------------------------------------------------------------------
        // Raccogli source uniche
        // --------------------------------------------------------------------

        for (size_t r = 0;
             r < introspection.relationCount();
             r++)
        {
            const auto* relation =
                introspection.getRelation(r);

            if (!relation)
                continue;


            if (relation->type !=
                DomoIntrospection::RelationType::SPLIT)
                continue;


            bool exists = false;


            for (size_t i = 0;
                 i < sources.size();
                 i++)
            {
                if (sources[i] ==
                    relation->sourceArea)
                {
                    exists = true;
                    break;
                }
            }


            if (!exists)
            {
                sources.push_back(
                    relation->sourceArea
                );
            }
        }


        std::vector<int> processed;

        processed.reserve(
            sources.size()
        );


        // --------------------------------------------------------------------
        // Componenti del grafo
        // --------------------------------------------------------------------

        for (size_t i = 0;
             i < sources.size();
             i++)
        {
            const int seed =
                sources[i];


            bool alreadyProcessed = false;


            for (size_t p = 0;
                 p < processed.size();
                 p++)
            {
                if (processed[p] == seed)
                {
                    alreadyProcessed = true;
                    break;
                }
            }


            if (alreadyProcessed)
                continue;


            std::vector<int> group;

            group.push_back(seed);


            for (size_t cursor = 0;
                 cursor < group.size();
                 cursor++)
            {
                const int current =
                    group[cursor];


                for (size_t targetRelation = 0;
                     targetRelation <
                         introspection.relationCount();
                     targetRelation++)
                {
                    const auto* relation =
                        introspection.getRelation(
                            targetRelation
                        );


                    if (!relation)
                        continue;


                    if (relation->type !=
                        DomoIntrospection::RelationType::SPLIT)
                        continue;


                    if (relation->sourceArea !=
                        current)
                        continue;


                    const int sharedTarget =
                        relation->targetArea;


                    for (size_t otherSource = 0;
                         otherSource < sources.size();
                         otherSource++)
                    {
                        const int candidate =
                            sources[otherSource];


                        bool alreadyInGroup = false;


                        for (size_t g = 0;
                             g < group.size();
                             g++)
                        {
                            if (group[g] == candidate)
                            {
                                alreadyInGroup = true;
                                break;
                            }
                        }


                        if (alreadyInGroup)
                            continue;


                        bool sharesTarget = false;


                        for (size_t candidateRelation = 0;
                             candidateRelation <
                                 introspection.relationCount();
                             candidateRelation++)
                        {
                            const auto* candidateRel =
                                introspection.getRelation(
                                    candidateRelation
                                );


                            if (!candidateRel)
                                continue;


                            if (candidateRel->type !=
                                DomoIntrospection::RelationType::SPLIT)
                                continue;


                            if (candidateRel->sourceArea !=
                                candidate)
                                continue;


                            if (candidateRel->targetArea ==
                                sharedTarget)
                            {
                                sharesTarget = true;
                                break;
                            }
                        }


                        if (sharesTarget)
                        {
                            group.push_back(
                                candidate
                            );
                        }
                    }
                }
            }


            // ----------------------------------------------------------------
            // Marca processati
            // ----------------------------------------------------------------

            for (size_t g = 0;
                 g < group.size();
                 g++)
            {
                processed.push_back(
                    group[g]
                );
            }


            // ----------------------------------------------------------------
            // Conta source compatibili con COVER
            // ----------------------------------------------------------------

            size_t coverSources = 0;


            for (size_t g = 0;
                 g < group.size();
                 g++)
            {
                const auto* entity =
                    introspection.findEntity(
                        group[g]
                    );


                if (!entity)
                    continue;


                if (!entity->areaCfg)
                    continue;


                const char* label =
                    entity->areaCfg->label.c_str();


                if (!label || !*label)
                    continue;


                String text(label);


                if (hasToken(
                        text,
                        SemanticToken::BUTTON) &&
                    hasToken(
                        text,
                        SemanticToken::COVER))
                {
                    coverSources++;
                }
            }


            if (coverSources < 2)
                continue;


            // ----------------------------------------------------------------
            // Cerca rappresentante OPEN
            // ----------------------------------------------------------------

            int canonicalArea = -1;


            for (size_t g = 0;
                 g < group.size();
                 g++)
            {
                const auto* entity =
                    introspection.findEntity(
                        group[g]
                    );


                if (!entity)
                    continue;


                if (!entity->areaCfg)
                    continue;


                const char* label =
                    entity->areaCfg->label.c_str();


                if (!label || !*label)
                    continue;


                String text(label);


                if (hasToken(
                        text,
                        SemanticToken::BUTTON) &&
                    hasToken(
                        text,
                        SemanticToken::COVER) &&
                    hasToken(
                        text,
                        SemanticToken::OPEN))
                {
                    canonicalArea =
                        entity->area;

                    break;
                }
            }


            // ----------------------------------------------------------------
            // Fallback
            // ----------------------------------------------------------------

            if (canonicalArea < 0)
            {
                for (size_t g = 0;
                     g < group.size();
                     g++)
                {
                    const auto* entity =
                        introspection.findEntity(
                            group[g]
                        );


                    if (!entity)
                        continue;


                    if (!entity->areaCfg)
                        continue;


                    const char* label =
                        entity->areaCfg->label.c_str();


                    if (!label || !*label)
                        continue;


                    String text(label);


                    if (hasToken(
                            text,
                            SemanticToken::BUTTON) &&
                        hasToken(
                            text,
                            SemanticToken::COVER))
                    {
                        canonicalArea =
                            entity->area;

                        break;
                    }
                }
            }


            if (canonicalArea < 0)
                continue;


            // ----------------------------------------------------------------
            // COVER + implementation details
            // ----------------------------------------------------------------

            for (size_t g = 0;
                 g < group.size();
                 g++)
            {
                DomoIntrospection::Entity* entity =
                    introspection.findEntity(
                        group[g]
                    );


                if (!entity)
                    continue;


                if (entity->area ==
                    canonicalArea)
                {
                    entity->kind =
                        DomoIntrospection::EntityKind::COVER;

                    entity->readable = true;
                    entity->writable = true;
                    entity->implementationOnly = false;
                }
                else
                {
                    entity->implementationOnly = true;
                }


                hideSplitTargets(
                    introspection,
                    entity->area
                );
            }
        }
    }

    // ========================================================================
    // TOGGLES
    // ========================================================================
    //
    // Un Toggle rappresenta semanticamente uno SWITCH.
    //
    // Esempio:
    //
    //     Pulsante luce bagno
    //
    // viene trasformato dal Toggle Engine in uno stato ON/OFF
    // persistente.
    //
    // Home Assistant deve quindi visualizzarlo come SWITCH e non
    // come BUTTON.
    //
    // ========================================================================

    void resolveToggles(
        DomoIntrospection& introspection)
    {
        for (size_t r = 0;
             r < introspection.relationCount();
             ++r)
        {
            const auto* relation =
                introspection.getRelation(r);

            if (!relation)
                continue;

            if (relation->type !=
                DomoIntrospection::RelationType::TOGGLE)
            {
                continue;
            }

            DomoIntrospection::Entity* source =
                introspection.findEntity(
                    relation->sourceArea
                );

            if (!source)
                continue;

            source->kind =
                DomoIntrospection::EntityKind::SWITCH;

            source->unit =
                nullptr;

            source->deviceClass =
                nullptr;

            source->stateClass =
                nullptr;

            source->readable =
                true;

            source->writable =
                true;
        }
    }

    // ========================================================================
    // DERIVED / STATUS
    // ========================================================================

    void resolveDerivedAreas(
        DomoIntrospection& introspection)
    {
        for (size_t i = 0;
            i < introspection.entityCount();
            i++)
        {
            DomoIntrospection::Entity* entity =
                introspection.getEntity(i);

            if (!entity)
                continue;


            // ============================================================
            // TECHNICAL STATE
            // ============================================================
            //
            // La entity è stata riconosciuta strutturalmente come stato
            // tecnico durante la costruzione dell'introspection.
            //
            // Esempio:
            //
            //   stessa area utilizzata contemporaneamente come input
            //   e output del grafo delle automazioni.
            //
            // Questa classificazione strutturale ha precedenza rispetto
            // alla classificazione lessicale ottenuta dalla label.
            //
            // Non vengono utilizzati:
            //
            //   - ID area specifici
            //   - nomi applicativi
            //   - riferimenti cablati ai buffer
            //
            // ============================================================

            if (entity->isTechnicalState)
            {
                entity->kind =
                    DomoIntrospection::EntityKind::STATUS;

                entity->unit = nullptr;
                entity->deviceClass = nullptr;
                entity->stateClass = nullptr;

                entity->readable = true;
                entity->writable = false;

                entity->implementationOnly = true;

                continue;
            }


            // ============================================================
            // DERIVED DETECTION
            // ============================================================

            const bool derived =
                entity->origin ==
                    DomoIntrospection::EntityOrigin::DERIVED ||
                entity->isDerivedOutput ||
                entity->isAutomationInput ||
                entity->isAutomationOutput;


            // ============================================================
            // SPECIALIZED SECURITY SOURCE
            // ============================================================
            //
            // fromSecurity indica solamente la provenienza.
            //
            // Una entity Security può già essere classificata come:
            //
            //   - BINARY_SENSOR
            //   - SENSOR
            //   - SWITCH
            //   - BUTTON
            //   - STATUS
            //
            // Una semantica già risolta non deve essere sovrascritta.
            //
            // Solamente una entity Security ancora UNKNOWN viene
            // convertita in STATUS.
            //
            // La visibilità non viene modificata dal resolver.
            //
            // ============================================================

            if (entity->fromSecurity)
            {
                if (entity->kind ==
                    DomoIntrospection::EntityKind::UNKNOWN)
                {
                    entity->kind =
                        DomoIntrospection::EntityKind::STATUS;

                    entity->unit = nullptr;
                    entity->deviceClass = nullptr;
                    entity->stateClass = nullptr;

                    entity->readable = true;
                    entity->writable = false;
                }

                continue;
            }


            // ============================================================
            // GENERIC DERIVED ENTITY
            // ============================================================

            if (!derived)
                continue;


            // ------------------------------------------------------------
            // Una semantica reale già risolta deve essere conservata.
            //
            // Esempi:
            //
            //   PIR usato come input automazione
            //       -> resta BINARY_SENSOR / motion
            //
            //   temperatura usata come input automazione
            //       -> resta SENSOR / temperature
            //
            //   switch usato come output automazione
            //       -> resta SWITCH
            // ------------------------------------------------------------

            if (entity->kind !=
                DomoIntrospection::EntityKind::UNKNOWN)
            {
                continue;
            }


            // ------------------------------------------------------------
            // Solamente una entity derivata ancora UNKNOWN diventa STATUS.
            // ------------------------------------------------------------

            entity->kind =
                DomoIntrospection::EntityKind::STATUS;

            entity->unit = nullptr;
            entity->deviceClass = nullptr;
            entity->stateClass = nullptr;

            entity->readable = true;
            entity->writable = false;

            entity->implementationOnly = true;
        }
    }
    
        // ========================================================================
    // HIDE SPLIT TARGETS
    // ========================================================================

    void hideSplitTargets(
        DomoIntrospection& introspection,
        int sourceArea)
    {
        for (size_t r = 0;
             r < introspection.relationCount();
             r++)
        {
            const auto* relation =
                introspection.getRelation(r);

            if (!relation)
                continue;

            if (relation->type !=
                DomoIntrospection::RelationType::SPLIT)
            {
                continue;
            }

            if (relation->sourceArea !=
                sourceArea)
            {
                continue;
            }

            DomoIntrospection::Entity* target =
                introspection.findEntity(
                    relation->targetArea
                );

            if (!target)
                continue;

            target->implementationOnly = true;
        }
    }


    // ========================================================================
    // TOKEN MATCH
    // ========================================================================

    bool hasToken(
        const String& text,
        SemanticToken token) const
    {
        const DictionaryEntry* entry =
            findDictionaryEntry(
                token
            );

        if (!entry)
            return false;

        String source(text);

        source.toLowerCase();

        for (size_t i = 0;
             i < entry->wordCount;
             i++)
        {
            const char* word =
                entry->words[i];

            if (!word || !*word)
                continue;

            String needle(word);

            needle.toLowerCase();

            if (entry->wholeWord)
            {
                if (containsWholeWord(
                        source,
                        needle))
                {
                    return true;
                }
            }
            else
            {
                if (source.indexOf(
                        needle) >= 0)
                {
                    return true;
                }
            }
        }

        return false;
    }


    // ========================================================================
    // DICTIONARY LOOKUP
    // ========================================================================

    const DictionaryEntry* findDictionaryEntry(
        SemanticToken token) const
    {
        if (!_dictionary)
            return nullptr;

        for (size_t i = 0;
             i < _dictionary->entryCount;
             i++)
        {
            if (_dictionary->entries[i].token ==
                token)
            {
                return
                    &_dictionary->entries[i];
            }
        }

        return nullptr;
    }


    // ========================================================================
    // WHOLE WORD
    // ========================================================================

    bool containsWholeWord(
        const String& text,
        const String& word) const
    {
        int position = 0;

        while (true)
        {
            position =
                text.indexOf(
                    word,
                    position
                );

            if (position < 0)
                return false;

            const int end =
                position +
                word.length();

            const bool leftOK =
                position == 0 ||
                !isWordChar(
                    text[position - 1]
                );

            const bool rightOK =
                end >= text.length() ||
                !isWordChar(
                    text[end]
                );

            if (leftOK && rightOK)
                return true;

            position = end;
        }
    }


    // ========================================================================
    // WORD CHARACTER
    // ========================================================================

    bool isWordChar(
        char c) const
    {
        return
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_';
    }


    // ========================================================================
    // SENSOR
    // ========================================================================

    void setSensor(
        DomoIntrospection::Entity& entity,
        const char* unit,
        const char* deviceClass,
        const char* stateClass)
    {
        entity.kind =
            DomoIntrospection::EntityKind::SENSOR;

        entity.unit =
            unit;

        entity.deviceClass =
            deviceClass;

        entity.stateClass =
            stateClass;

        entity.readable = true;
        entity.writable = false;
    }


    // ========================================================================
    // BINARY SENSOR
    // ========================================================================

    void setBinarySensor(
        DomoIntrospection::Entity& entity,
        const char* deviceClass)
    {
        entity.kind =
            DomoIntrospection::EntityKind::BINARY_SENSOR;

        entity.unit =
            nullptr;

        entity.deviceClass =
            deviceClass;

        entity.stateClass =
            nullptr;

        entity.readable = true;
        entity.writable = false;
    }

    // ========================================================================
    // LIGHT
    // ========================================================================
    //
    // Classifica una entity come sorgente luminosa.
    //
    // La funzione deve essere chiamata solamente dopo aver verificato che
    // l'entity corrisponda a un'uscita fisica:
    //
    //     DIGITAL_OUTPUT
    //     ANALOG_OUTPUT
    //
    // Una LIGHT può essere inizialmente una semplice uscita ON/OFF.
    //
    // Funzionalità avanzate come:
    //
    //     - dimmer
    //     - RGB
    //     - RGBW
    //     - temperatura colore
    //
    // potranno essere aggiunte successivamente mediante analisi strutturale.
    //
    // ========================================================================

    void setLight(
        DomoIntrospection::Entity& entity)
    {
        entity.kind =
            DomoIntrospection::EntityKind::LIGHT;

        entity.unit =
            nullptr;

        entity.deviceClass =
            nullptr;

        entity.stateClass =
            nullptr;

        entity.readable =
            true;

        entity.writable =
            true;
    }

    // ========================================================================
    // SWITCH
    // ========================================================================

    void setSwitch(
        DomoIntrospection::Entity& entity)
    {
        entity.kind =
            DomoIntrospection::EntityKind::SWITCH;

        entity.unit = nullptr;
        entity.deviceClass = nullptr;
        entity.stateClass = nullptr;

        entity.readable = true;
        entity.writable = true;
    }


    // ========================================================================
    // BUTTON
    // ========================================================================

    void setButton(
        DomoIntrospection::Entity& entity)
    {
        entity.kind =
            DomoIntrospection::EntityKind::BUTTON;

        entity.unit = nullptr;
        entity.deviceClass = nullptr;
        entity.stateClass = nullptr;

        entity.readable = true;
        entity.writable = true;
    }

    // ========================================================================
    // NUMBER
    // ========================================================================

    void setNumber(
        DomoIntrospection::Entity& entity)
    {
        entity.kind =
            DomoIntrospection::EntityKind::NUMBER;

        entity.unit =
            nullptr;

        entity.deviceClass =
            nullptr;

        entity.stateClass =
            nullptr;

        entity.readable =
            true;

        entity.writable =
            true;
    }

    // ========================================================================
    // RESULT
    // ========================================================================

    void calculateResult(
        const DomoIntrospection& introspection)
    {
        _result = Result{};

        _result.total =
            introspection.entityCount();

        for (size_t i = 0;
             i < introspection.entityCount();
             i++)
        {
            const auto* entity =
                introspection.getEntity(i);

            if (!entity)
                continue;

            if (entity->implementationOnly)
            {
                _result.hidden++;
                continue;
            }

            if (entity->writable)
                _result.writable++;

            switch (entity->kind)
            {
                case DomoIntrospection::EntityKind::SENSOR:
                    _result.sensors++;
                    break;

                case DomoIntrospection::EntityKind::BINARY_SENSOR:
                    _result.binarySensors++;
                    break;

                case DomoIntrospection::EntityKind::SWITCH:
                    _result.switches++;
                    break;

                case DomoIntrospection::EntityKind::LIGHT:
                    _result.lights++;
                    break;

                case DomoIntrospection::EntityKind::COVER:
                    _result.covers++;
                    break;

                case DomoIntrospection::EntityKind::CLIMATE:
                    _result.climates++;
                    break;

                case DomoIntrospection::EntityKind::BUTTON:
                    _result.buttons++;
                    break;

                case DomoIntrospection::EntityKind::NUMBER:
                    _result.numbers++;
                    break;

                case DomoIntrospection::EntityKind::TEXT:
                    _result.texts++;
                    break;

                case DomoIntrospection::EntityKind::STATUS:
                    _result.status++;
                    break;

                case DomoIntrospection::EntityKind::UNKNOWN:
                    _result.unknown++;
                    break;
            }
        }
    }


    // ========================================================================
    // ENTITY STRING
    // ========================================================================

    static const char* entityKindToString(
        DomoIntrospection::EntityKind kind)
    {
        switch (kind)
        {
            case DomoIntrospection::EntityKind::UNKNOWN:
                return "UNKNOWN";

            case DomoIntrospection::EntityKind::SENSOR:
                return "SENSOR";

            case DomoIntrospection::EntityKind::BINARY_SENSOR:
                return "BINARY_SENSOR";

            case DomoIntrospection::EntityKind::SWITCH:
                return "SWITCH";

            case DomoIntrospection::EntityKind::LIGHT:
                return "LIGHT";

            case DomoIntrospection::EntityKind::COVER:
                return "COVER";

            case DomoIntrospection::EntityKind::CLIMATE:
                return "CLIMATE";

            case DomoIntrospection::EntityKind::BUTTON:
                return "BUTTON";

            case DomoIntrospection::EntityKind::NUMBER:
                return "NUMBER";

            case DomoIntrospection::EntityKind::TEXT:
                return "TEXT";

            case DomoIntrospection::EntityKind::STATUS:
                return "STATUS";
        }

        return "UNKNOWN";
    }
};


