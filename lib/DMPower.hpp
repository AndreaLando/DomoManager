#ifndef DMPower_HPP
#define DMPower_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑03‑26
   Note:
                    • Release iniziale

   ============================================================================ */



#include <Arduino.h>
#include <math.h>

#include "DMBaseClassCore.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class PowerSupervisor
{
public:

    // ============================================================
    // NOMI ALLARMI
    // ============================================================

    static constexpr const char* ALARM_MAIN_POWER =
        "MainPower";

    static constexpr const char* ALARM_24V_OK =
        "24V_OK";

    static constexpr const char* ALARM_FAULT =
        "Fault";

    static constexpr const char* ALARM_BATTERY_MODE =
        "BatteryMode";

    static constexpr long INVALID_VALUE =
        -2147483647L;

    // ============================================================
    // STATO TENSIONE PRINCIPALE
    // ============================================================

    /**
     * @brief Stato classificato della tensione principale.
     *
     * I nomi UNDER_VOLTAGE e OVER_VOLTAGE evitano il conflitto
     * con le macro Arduino LOW e HIGH.
     */
    enum class MainPowerState
    {
        INVALID,
        NORMAL,
        UNDER_VOLTAGE,
        OVER_VOLTAGE
    };

    // ============================================================
    // RISULTATI LETTURA
    // ============================================================

    /**
     * @brief Valore della tensione principale.
     *
     * @property value Valore acquisito in volt.
     * @property valid Indica se la lettura è valida.
     */
    struct PowerValue
    {
        float value = 0.0f;
        bool valid = false;
    };

    /**
     * @brief Risultato validabile di una lettura booleana.
     */
    struct BoolValue
    {
        bool value = false;
        bool valid = false;
    };

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================

    /**
     * @brief Configurazione del supervisore.
     *
     * I nomi dei reader sono mantenuti compatibili con la vecchia
     * versione di PowerSupervisor.
     */
    struct Config
    {
        struct Thresholds
        {
            float mainPowerLowTrip = 210.0f;
            float mainPowerLowReset = 215.0f;

            float mainPowerHighTrip = 250.0f;
            float mainPowerHighReset = 245.0f;
        };

        std::function<PowerValue()> readMainPower;
        std::function<bool()> read24vOk;
        std::function<bool()> readFault;
        std::function<bool()> readBatteryMode;

        /*
         * Campi legacy.
         *
         * Se vengono modificati, normalizeConfig() li trasferisce
         * nelle soglie Trip quando le soglie strutturate sono ancora
         * ai valori predefiniti.
         */
        float mainPowerLow = 210.0f;
        float mainPowerHigh = 250.0f;

        Thresholds thresholds;
    };

    // ============================================================
    // EVENTO ALLARME
    // ============================================================

    /**
     * @brief Evento strutturato generato dal supervisore.
     */
    struct AlarmEvent
    {
        String name;

        bool active = false;
        bool valid = false;

        float value = 0.0f;

        unsigned long timestamp = 0;
    };

    /**
     * @brief Callback legacy.
     *
     * Per compatibilità con il codice esistente, gli eventi analogici
     * vengono convertiti in long. Il rientro viene notificato con zero.
     */
    using AlarmCallback =
        std::function<void(
            const String& name,
            long value)>;

    /**
     * @brief Callback completa per eventi di allarme.
     */
    using AlarmEventCallback =
        std::function<void(
            const AlarmEvent& event)>;

    // ============================================================
    // STRUTTURE RUNTIME COMUNI
    // ============================================================

    /**
     * @brief Stato runtime di un ingresso digitale.
     */
    struct DigitalRuntime
    {
        bool value = false;
        bool valid = false;
        bool initialized = false;

        unsigned long lastChange = 0;
    };

    /**
     * @brief Stato runtime della tensione principale.
     */
    struct MainPowerRuntime
    {
        float value = 0.0f;

        bool valid = false;
        bool initialized = false;

        MainPowerState state =
            MainPowerState::INVALID;

        unsigned long lastChange = 0;
    };

    /**
     * @brief Stato runtime globale del supervisore.
     */
    struct Runtime
    {
        MainPowerRuntime mainPower;

        DigitalRuntime power24vOk;
        DigitalRuntime fault;
        DigitalRuntime batteryMode;

        unsigned long lastUpdate = 0;
    };

    // ============================================================
    // SEGNALI EXTRA
    // ============================================================

    struct Signal
    {
        struct Identity
        {
            String name;
        };

        struct Reader
        {
            std::function<long()> readFn;
        };

        struct Runtime
        {
            long lastValue =
                INVALID_VALUE;

            bool valid = false;
            bool initialized = false;

            unsigned long lastChange = 0;
        };

        Identity identity;
        Reader reader;
        Runtime runtime;
    };

    // ============================================================
    // SNAPSHOT
    // ============================================================

    struct Snapshot
    {
        Runtime runtime;

        size_t extraSignalCount = 0;
        size_t activeAlarmCount = 0;
    };

private:

    // ============================================================
    // STATO INTERNO DEGLI ALLARMI
    // ============================================================

    struct AlarmState
    {
        String name;

        bool active = false;
        bool initialized = false;

        float lastValue = 0.0f;

        unsigned long lastChange = 0;
    };

    // ============================================================
    // CALLBACK INTERNE
    // ============================================================

    struct Callbacks
    {
        AlarmCallback legacy = nullptr;

        AlarmEventCallback event = nullptr;
    };

    // ============================================================
    // STORAGE INTERNO
    // ============================================================

    struct Storage
    {
        std::vector<Signal> extraSignals;

        std::vector<AlarmState> alarms;
    };

    Config config;

    Runtime runtime;

    Callbacks callbacks;

    Storage storage;

    /**
    * @brief Verifica se un allarme è attualmente attivo.
    *
    * @param name Nome dell'allarme.
    *
    * @return true se l'allarme esiste ed è attivo.
    */
    bool isAlarmActive(
        const String& name) const
    {
        for (const auto& alarm :
            storage.alarms)
        {
            if (alarm.name == name)
            {
                return
                    alarm.initialized &&
                    alarm.active;
            }
        }

        return false;
    }

public:

    // ============================================================
    // COSTRUTTORE
    // ============================================================

    /**
     * @brief Costruisce il supervisore.
     *
     * @param value Configurazione iniziale.
     */
    explicit PowerSupervisor(
        const Config& value)
        : config(value)
    {
        normalizeConfig();
    }

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================

    /**
     * @brief Sostituisce la configurazione corrente.
     *
     * @param value Nuova configurazione.
     */
    void setConfig(
        const Config& value)
    {
        config = value;

        normalizeConfig();
    }

    /**
     * @brief Restituisce la configurazione corrente.
     */
    const Config& getConfig() const
    {
        return config;
    }

    // ============================================================
    // CALLBACK
    // ============================================================

    /**
     * @brief Registra la callback legacy.
     *
     * @param callback Callback compatibile con la precedente API.
     */
    void onAlarm(
        AlarmCallback callback)
    {
        callbacks.legacy =
            callback;
    }

    /**
     * @brief Registra la callback strutturata.
     *
     * @param callback Callback che riceve AlarmEvent.
     */
    void onAlarmEvent(
        AlarmEventCallback callback)
    {
        callbacks.event =
            callback;
    }

    // ============================================================
    // SETUP
    // ============================================================

    /**
     * @brief Reinizializza lo stato runtime.
     *
     * I segnali configurati vengono conservati, mentre valori,
     * stati e memoria degli allarmi vengono azzerati.
     */
    void setup()
    {
        runtime =
            Runtime{};

        storage.alarms.clear();

        for (auto& signal :
             storage.extraSignals)
        {
            signal.runtime =
                Signal::Runtime{};
        }
    }

    // ============================================================
    // LOOP
    // ============================================================

    /**
     * @brief Aggiorna tutte le letture del supervisore.
     *
     * @param now Timestamp corrente ottenuto tramite millis().
     */
    void loop(
        unsigned long now)
    {
        runtime.lastUpdate =
            now;

        updateMainPower(now);

        updatePower24v(now);

        updateFault(now);

        updateBatteryMode(now);

        updateExtraSignals(now);
    }

    // ============================================================
    // SEGNALI EXTRA
    // ============================================================

    /**
     * @brief Registra un nuovo segnale extra.
     *
     * Ogni variazione del valore genera una notifica.
     *
     * @param name Nome univoco del segnale.
     * @param readFn Funzione di lettura.
     *
     * @return true se il segnale è stato aggiunto.
     */
    bool addExtraSignal(
        const String& name,
        std::function<long()> readFn)
    {
        if (name.length() == 0)
            return false;

        if (!readFn)
            return false;

        if (findExtraSignal(name) != nullptr)
            return false;

        Signal signal;

        signal.identity.name =
            name;

        signal.reader.readFn =
            readFn;

        storage.extraSignals.push_back(
            signal);

        return true;
    }

    /**
     * @brief Restituisce il numero di segnali extra.
     */
    size_t getExtraSignalCount() const
    {
        return
            storage.extraSignals.size();
    }

    /**
     * @brief Restituisce un segnale extra per indice.
     *
     * @param index Indice del segnale.
     *
     * @return Puntatore al segnale oppure nullptr.
     */
    const Signal* getExtraSignal(
        size_t index) const
    {
        if (index >=
            storage.extraSignals.size())
        {
            return nullptr;
        }

        return
            &storage.extraSignals[index];
    }

    /**
     * @brief Restituisce un segnale extra per nome.
     *
     * @param name Nome del segnale.
     *
     * @return Puntatore al segnale oppure nullptr.
     */
    const Signal* getExtraSignal(
        const String& name) const
    {
        for (const auto& signal :
             storage.extraSignals)
        {
            if (signal.identity.name ==
                name)
            {
                return &signal;
            }
        }

        return nullptr;
    }

    // ============================================================
    // GETTER MAIN POWER
    // ============================================================

    float getMainPower() const
    {
        return runtime.mainPower.value;
    }

    bool isMainPowerValid() const
    {
        return runtime.mainPower.valid;
    }

    MainPowerState getMainPowerState() const
    {
        return runtime.mainPower.state;
    }

    // ============================================================
    // GETTER DIGITALI
    // ============================================================

    bool is24vOk() const
    {
        return
            runtime.power24vOk.valid &&
            runtime.power24vOk.value;
    }

    bool is24vValid() const
    {
        return
            runtime.power24vOk.valid;
    }

    bool isFault() const
    {
        return
            runtime.fault.valid &&
            runtime.fault.value;
    }

    bool isFaultValid() const
    {
        return
            runtime.fault.valid;
    }

    bool isBatteryMode() const
    {
        return
            runtime.batteryMode.valid &&
            runtime.batteryMode.value;
    }

    bool isBatteryModeValid() const
    {
        return
            runtime.batteryMode.valid;
    }

    // ============================================================
    // SNAPSHOT
    // ============================================================

    /**
     * @brief Restituisce una fotografia dello stato corrente.
     */
    Snapshot getSnapshot() const
    {
        Snapshot snapshot;

        snapshot.runtime =
            runtime;

        snapshot.extraSignalCount =
            storage.extraSignals.size();

        for (const auto& alarm :
             storage.alarms)
        {
            if (alarm.active)
            {
                ++snapshot.activeAlarmCount;
            }
        }

        return snapshot;
    }

    // ============================================================
    // DIAGNOSTICA
    // ============================================================

    class Diagnostic
    {
    public:

        /**
         * @brief Stampa il report completo del supervisore.
         *
         * @param supervisor Supervisore da analizzare.
         */
        static void Report(
            const PowerSupervisor& supervisor)
        {
            const Snapshot snapshot =
                supervisor.getSnapshot();

            Serial.println(
                "\n===== POWER SUPERVISOR =====");

            // ----------------------------------------------------
            // MAIN POWER
            // ----------------------------------------------------

            Serial.print("MainPower: ");

            if (snapshot.runtime.mainPower.valid)
            {
                Serial.print(
                    snapshot.runtime.mainPower.value);

                Serial.print(" V");
            }
            else
            {
                Serial.print("INVALID");
            }

            Serial.print("  State: ");

            printMainPowerState(
                snapshot.runtime.mainPower.state);

            // ----------------------------------------------------
            // 24 V
            // ----------------------------------------------------

            Serial.print("24V OK: ");

            printDigitalRuntime(
                snapshot.runtime.power24vOk);

            // ----------------------------------------------------
            // FAULT
            // ----------------------------------------------------

            Serial.print("Fault: ");

            printDigitalRuntime(
                snapshot.runtime.fault);

            // ----------------------------------------------------
            // BATTERY MODE
            // ----------------------------------------------------

            Serial.print("Battery Mode: ");

            printDigitalRuntime(
                snapshot.runtime.batteryMode);

            // ----------------------------------------------------
            // EXTRA SIGNAL
            // ----------------------------------------------------

            Serial.print("Extra signals: ");

            Serial.println(
                snapshot.extraSignalCount);

            for (const auto& signal :
                 supervisor.storage.extraSignals)
            {
                Serial.print(" - ");

                Serial.print(
                    signal.identity.name);

                Serial.print(" = ");

                if (signal.runtime.valid)
                {
                    Serial.print(
                        signal.runtime.lastValue);
                }
                else
                {
                    Serial.print("INVALID");
                }

                Serial.print("  LastChange=");

                Serial.println(
                    signal.runtime.lastChange);
            }

            // ----------------------------------------------------
            // ALLARMI
            // ----------------------------------------------------

            Serial.print("Active alarms: ");

            Serial.println(
                snapshot.activeAlarmCount);

            for (const auto& alarm :
                 supervisor.storage.alarms)
            {
                Serial.print(" - ");

                Serial.print(alarm.name);

                Serial.print(" = ");

                Serial.print(
                    alarm.active
                        ? "ACTIVE"
                        : "CLEARED");

                Serial.print("  Value=");

                Serial.print(
                    alarm.lastValue);

                Serial.print("  LastChange=");

                Serial.println(
                    alarm.lastChange);
            }

            Serial.print("Last update: ");

            Serial.println(
                snapshot.runtime.lastUpdate);

            Serial.println(
                "===== END POWER SUPERVISOR =====\n");
        }

    private:

        /**
         * @brief Stampa lo stato classificato della rete.
         */
        static void printMainPowerState(
            MainPowerState state)
        {
            switch (state)
            {
                case MainPowerState::NORMAL:

                    Serial.println("NORMAL");
                    break;

                case MainPowerState::UNDER_VOLTAGE:

                    Serial.println(
                        "UNDER_VOLTAGE");
                    break;

                case MainPowerState::OVER_VOLTAGE:

                    Serial.println(
                        "OVER_VOLTAGE");
                    break;

                case MainPowerState::INVALID:
                default:

                    Serial.println("INVALID");
                    break;
            }
        }

        /**
         * @brief Stampa un ingresso digitale validabile.
         */
        static void printDigitalRuntime(
            const DigitalRuntime& input)
        {
            if (!input.valid)
            {
                Serial.println("INVALID");

                return;
            }

            Serial.println(
                input.value
                    ? "YES"
                    : "NO");
        }
    };

private:

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================

    /**
     * @brief Normalizza le soglie configurate.
     */
    void normalizeConfig()
    {
        /*
         * Compatibilità con i campi legacy.
         */
        if (isfinite(config.mainPowerLow))
        {
            config.thresholds.mainPowerLowTrip =
                config.mainPowerLow;
        }

        if (isfinite(config.mainPowerHigh))
        {
            config.thresholds.mainPowerHighTrip =
                config.mainPowerHigh;
        }

        auto& thresholds =
            config.thresholds;

        if (!isfinite(
                thresholds.mainPowerLowTrip))
        {
            thresholds.mainPowerLowTrip =
                210.0f;
        }

        if (!isfinite(
                thresholds.mainPowerLowReset))
        {
            thresholds.mainPowerLowReset =
                215.0f;
        }

        if (!isfinite(
                thresholds.mainPowerHighTrip))
        {
            thresholds.mainPowerHighTrip =
                250.0f;
        }

        if (!isfinite(
                thresholds.mainPowerHighReset))
        {
            thresholds.mainPowerHighReset =
                245.0f;
        }

        if (thresholds.mainPowerLowTrip >=
            thresholds.mainPowerHighTrip)
        {
            thresholds.mainPowerLowTrip =
                210.0f;

            thresholds.mainPowerLowReset =
                215.0f;

            thresholds.mainPowerHighTrip =
                250.0f;

            thresholds.mainPowerHighReset =
                245.0f;
        }

        if (thresholds.mainPowerLowReset <
            thresholds.mainPowerLowTrip)
        {
            thresholds.mainPowerLowReset =
                thresholds.mainPowerLowTrip;
        }

        if (thresholds.mainPowerLowReset >=
            thresholds.mainPowerHighTrip)
        {
            thresholds.mainPowerLowReset =
                thresholds.mainPowerLowTrip;
        }

        if (thresholds.mainPowerHighReset >
            thresholds.mainPowerHighTrip)
        {
            thresholds.mainPowerHighReset =
                thresholds.mainPowerHighTrip;
        }

        if (thresholds.mainPowerHighReset <=
            thresholds.mainPowerLowTrip)
        {
            thresholds.mainPowerHighReset =
                thresholds.mainPowerHighTrip;
        }
    }

    // ============================================================
    // LETTURE
    // ============================================================

    /**
     * @brief Legge la tensione principale.
     *
     * @return Lettura validata.
     */
    PowerValue readMainPowerValue() const
    {
        if (!config.readMainPower)
        {
            return PowerValue{
                0.0f,
                false
            };
        }

        const PowerValue value =
            config.readMainPower();

        if (!value.valid ||
            !isfinite(value.value))
        {
            return PowerValue{
                0.0f,
                false
            };
        }

        return value;
    }

    /**
     * @brief Esegue una lettura booleana.
     *
     * Una callback assente produce valid=false.
     */
    static BoolValue readBooleanValue(
        const std::function<bool()>& reader)
    {
        if (!reader)
        {
            return BoolValue{
                false,
                false
            };
        }

        return BoolValue{
            reader(),
            true
        };
    }

    /**
     * @brief Legge un segnale extra.
     *
     * @param signal Segnale da leggere.
     * @param output Valore acquisito.
     *
     * @return true se il valore è valido.
     */
    static bool readExtraSignalValue(
        const Signal& signal,
        long& output)
    {
        if (!signal.reader.readFn)
            return false;

        output =
            signal.reader.readFn();

        return
            output != INVALID_VALUE;
    }

    // ============================================================
    // MAIN POWER
    // ============================================================

    /**
     * @brief Aggiorna tensione e stato della rete principale.
     *
     * @param now Timestamp corrente.
     */
    void updateMainPower(
        unsigned long now)
    {
        const PowerValue value =
            readMainPowerValue();

        runtime.mainPower.valid =
            value.valid;

        if (!value.valid)
        {
            runtime.mainPower.state =
                MainPowerState::INVALID;

            LOG_IF(
                "PowerSupervisor",
                "MainPower invalid");

            return;
        }

        runtime.mainPower.value =
            value.value;

        LOG_DF(
            "PowerSupervisor",
            "MainPower=%.2f",
            static_cast<double>(
                value.value));

        const MainPowerState previousState =
            runtime.mainPower.state;

        const MainPowerState newState =
            classifyMainPower(
                value.value,
                previousState);

        runtime.mainPower.state =
            newState;

        const bool newAlarm =
            isMainPowerAlarmState(
                newState);

        if (!runtime.mainPower.initialized)
        {
            runtime.mainPower.initialized =
                true;

            runtime.mainPower.lastChange =
                now;

            if (newAlarm)
            {
                setAlarm(
                    ALARM_MAIN_POWER,
                    true,
                    value.value,
                    true,
                    now);
            }

            return;
        }

        if (previousState ==
            newState)
        {
            return;
        }

        runtime.mainPower.lastChange =
            now;

        const bool previousAlarm =
            isAlarmActive(
                ALARM_MAIN_POWER);

        if (!previousAlarm &&
            newAlarm)
        {
            setAlarm(
                ALARM_MAIN_POWER,
                true,
                value.value,
                true,
                now);

            return;
        }

        if (previousAlarm &&
            !newAlarm)
        {
            setAlarm(
                ALARM_MAIN_POWER,
                false,
                value.value,
                true,
                now);

            return;
        }

        if (previousAlarm &&
            newAlarm)
        {
            setAlarm(
                ALARM_MAIN_POWER,
                true,
                value.value,
                true,
                now,
                true);
        }
    }

    /**
     * @brief Verifica se lo stato rappresenta un'anomalia.
     */
    static bool isMainPowerAlarmState(
        MainPowerState state)
    {
        return
            state ==
                MainPowerState::UNDER_VOLTAGE ||
            state ==
                MainPowerState::OVER_VOLTAGE;
    }

    /**
     * @brief Classifica la tensione applicando isteresi.
     *
     * @param voltage Tensione misurata.
     * @param previousState Stato precedente.
     *
     * @return Nuovo stato classificato.
     */
    MainPowerState classifyMainPower(
        float voltage,
        MainPowerState previousState) const
    {
        const auto& thresholds =
            config.thresholds;

        switch (previousState)
        {
            case MainPowerState::UNDER_VOLTAGE:

                if (voltage <
                    thresholds.mainPowerLowReset)
                {
                    return
                        MainPowerState::UNDER_VOLTAGE;
                }

                if (voltage >
                    thresholds.mainPowerHighTrip)
                {
                    return
                        MainPowerState::OVER_VOLTAGE;
                }

                return
                    MainPowerState::NORMAL;

            case MainPowerState::OVER_VOLTAGE:

                if (voltage >
                    thresholds.mainPowerHighReset)
                {
                    return
                        MainPowerState::OVER_VOLTAGE;
                }

                if (voltage <
                    thresholds.mainPowerLowTrip)
                {
                    return
                        MainPowerState::UNDER_VOLTAGE;
                }

                return
                    MainPowerState::NORMAL;

            case MainPowerState::NORMAL:
            case MainPowerState::INVALID:
            default:

                if (voltage <
                    thresholds.mainPowerLowTrip)
                {
                    return
                        MainPowerState::UNDER_VOLTAGE;
                }

                if (voltage >
                    thresholds.mainPowerHighTrip)
                {
                    return
                        MainPowerState::OVER_VOLTAGE;
                }

                return
                    MainPowerState::NORMAL;
        }
    }

    // ============================================================
    // INPUT DIGITALI
    // ============================================================

    /**
     * @brief Aggiorna lo stato 24V_OK.
     *
     * L'allarme è attivo quando il valore è false.
     */
    void updatePower24v(
        unsigned long now)
    {
        updateDigitalInput(
            runtime.power24vOk,
            readBooleanValue(
                config.read24vOk),
            ALARM_24V_OK,
            true,
            now);
    }

    /**
     * @brief Aggiorna l'ingresso fault.
     *
     * L'allarme è attivo quando il valore è true.
     */
    void updateFault(
        unsigned long now)
    {
        updateDigitalInput(
            runtime.fault,
            readBooleanValue(
                config.readFault),
            ALARM_FAULT,
            false,
            now);
    }

    /**
     * @brief Aggiorna lo stato battery mode.
     *
     * L'allarme è attivo quando il valore è true.
     */
    void updateBatteryMode(
        unsigned long now)
    {
        updateDigitalInput(
            runtime.batteryMode,
            readBooleanValue(
                config.readBatteryMode),
            ALARM_BATTERY_MODE,
            false,
            now);
    }

    /**
     * @brief Aggiorna un ingresso digitale.
     *
     * @param input Stato runtime da aggiornare.
     * @param value Nuovo valore validabile.
     * @param alarmName Nome dell'allarme.
     * @param activeWhenFalse Se true, false rappresenta l'allarme.
     * @param now Timestamp corrente.
     */
    void updateDigitalInput(
        DigitalRuntime& input,
        const BoolValue& value,
        const String& alarmName,
        bool activeWhenFalse,
        unsigned long now)
    {
        input.valid =
            value.valid;

        if (!value.valid)
            return;

        const bool alarmActive =
            activeWhenFalse
                ? !value.value
                : value.value;

        if (!input.initialized)
        {
            input.value =
                value.value;

            input.initialized =
                true;

            input.lastChange =
                now;

            if (alarmActive)
            {
                setAlarm(
                    alarmName,
                    true,
                    value.value ? 1.0f : 0.0f,
                    true,
                    now);
            }

            return;
        }

        if (input.value ==
            value.value)
        {
            return;
        }

        input.value =
            value.value;

        input.lastChange =
            now;

        setAlarm(
            alarmName,
            alarmActive,
            value.value ? 1.0f : 0.0f,
            true,
            now);
    }

    // ============================================================
    // EXTRA SIGNAL
    // ============================================================

    /**
     * @brief Aggiorna tutti i segnali extra.
     */
    void updateExtraSignals(
        unsigned long now)
    {
        for (auto& signal :
             storage.extraSignals)
        {
            long value =
                INVALID_VALUE;

            const bool valid =
                readExtraSignalValue(
                    signal,
                    value);

            signal.runtime.valid =
                valid;

            if (!valid)
                continue;

            if (!signal.runtime.initialized)
            {
                signal.runtime.initialized =
                    true;

                signal.runtime.lastValue =
                    value;

                signal.runtime.lastChange =
                    now;

                continue;
            }

            if (signal.runtime.lastValue ==
                value)
            {
                continue;
            }

            signal.runtime.lastValue =
                value;

            signal.runtime.lastChange =
                now;

            emitValueChange(
                signal.identity.name,
                value,
                now);
        }
    }

    // ============================================================
    // ALLARMI
    // ============================================================

    /**
     * @brief Imposta lo stato di un allarme.
     *
     * @param name Nome dell'allarme.
     * @param active Nuovo stato.
     * @param value Valore associato.
     * @param valid Validità del valore.
     * @param now Timestamp corrente.
     * @param forceNotify Forza la notifica anche senza variazione.
     */
    void setAlarm(
        const String& name,
        bool active,
        float value,
        bool valid,
        unsigned long now,
        bool forceNotify = false)
    {
        AlarmState* alarm =
            findAlarm(name);

        if (!alarm)
        {
            AlarmState newAlarm;

            newAlarm.name =
                name;

            newAlarm.active =
                active;

            newAlarm.initialized =
                true;

            newAlarm.lastValue =
                value;

            newAlarm.lastChange =
                now;

            storage.alarms.push_back(
                newAlarm);

            if (active ||
                forceNotify)
            {
                emitAlarmEvent(
                    name,
                    active,
                    value,
                    valid,
                    now);
            }

            return;
        }

        const bool stateChanged =
            !alarm->initialized ||
            alarm->active != active;

        alarm->lastValue =
            value;

        if (!stateChanged &&
            !forceNotify)
        {
            return;
        }

        alarm->initialized =
            true;

        alarm->active =
            active;

        alarm->lastChange =
            now;

        emitAlarmEvent(
            name,
            active,
            value,
            valid,
            now);
    }

    /**
     * @brief Invia un evento di allarme alle callback registrate.
     */
    void emitAlarmEvent(
        const String& name,
        bool active,
        float value,
        bool valid,
        unsigned long now)
    {
        LOG_DF(
            "PowerSupervisor",
            "Alarm %s active=%d value=%.2f",
            name.c_str(),
            active ? 1 : 0,
            static_cast<double>(value));

        if (callbacks.event)
        {
            AlarmEvent event;

            event.name =
                name;

            event.active =
                active;

            event.valid =
                valid;

            event.value =
                value;

            event.timestamp =
                now;

            callbacks.event(
                event);
        }

        if (callbacks.legacy)
        {
            const long legacyValue =
                active
                    ? static_cast<long>(value)
                    : 0L;

            callbacks.legacy(
                name,
                legacyValue);
        }
    }

    /**
     * @brief Invia la variazione di un segnale extra.
     */
    void emitValueChange(
        const String& name,
        long value,
        unsigned long now)
    {
        if (callbacks.event)
        {
            AlarmEvent event;

            event.name =
                name;

            event.active =
                true;

            event.valid =
                true;

            event.value =
                static_cast<float>(value);

            event.timestamp =
                now;

            callbacks.event(
                event);
        }

        if (callbacks.legacy)
        {
            callbacks.legacy(
                name,
                value);
        }
    }

    // ============================================================
    // RICERCA
    // ============================================================

    /**
     * @brief Cerca un allarme per nome.
     */
    AlarmState* findAlarm(
        const String& name)
    {
        for (auto& alarm :
             storage.alarms)
        {
            if (alarm.name == name)
                return &alarm;
        }

        return nullptr;
    }

    /**
     * @brief Cerca un segnale extra per nome.
     */
    Signal* findExtraSignal(
        const String& name)
    {
        for (auto& signal :
             storage.extraSignals)
        {
            if (signal.identity.name ==
                name)
            {
                return &signal;
            }
        }

        return nullptr;
    }
};


class LoadManager
{
public:

    // ============================================================
    // CALLBACK
    // ============================================================

    using LoadCb =
        std::function<void(
            const String& name,
            bool state)>;

    using SuggestionCb =
        std::function<void(
            const String& action,
            int severity,
            const String& reason)>;

    // ============================================================
    // CONFIGURAZIONE GENERALE
    // ============================================================

    /**
    * @brief Configurazione generale del LoadManager.
    */
    struct Config
    {
        float hysteresisOn = 200.0f;
        float hysteresisOff = 200.0f;

        float forecastPeak = 3000.0f;

        /**
        * @brief Tempo massimo concesso per ricevere il feedback.
        *
        * Un valore pari a zero disabilita il timeout.
        */
        unsigned long commandTimeoutMs = 30000UL;

        /**
        * @brief Ritardo base applicato dopo un timeout.
        */
        unsigned long commandRetryBackoffMs = 15000UL;

        /**
        * @brief Numero massimo di timeout consecutivi.
        *
        * Un valore pari a zero indica nessun limite.
        */
        unsigned int commandMaxRetries = 3U;
    };

    // ============================================================
    // STRUTTURE COMUNI
    // ============================================================
    /**
    * @brief Motivo del distacco gestito.
    */
    enum class ShedReason : uint8_t
    {
        NONE = 0,

        /**
        * Superamento del limite dinamico.
        */
        SOFT_LIMIT,

        /**
        * Superamento dell'hard limit.
        */
        HARD_LIMIT,

        /**
        * Distacco richiesto esplicitamente.
        */
        EXTERNAL_REQUEST
    };

    /**
    * @brief Stato runtime del distacco di un carico.
    */
    struct ShedState
    {
        /**
        * @brief Distacco richiesto ma non ancora confermato.
        */
        bool pending = false;

        /**
        * @brief Carico effettivamente distaccato dal sistema.
        */
        bool active = false;

        /**
        * @brief Motivo del distacco.
        */
        ShedReason reason =
            ShedReason::NONE;

        /**
        * @brief Timestamp della richiesta di distacco.
        */
        unsigned long requestTime = 0UL;

        /**
        * @brief Timestamp della conferma del distacco.
        */
        unsigned long confirmedTime = 0UL;
    };

    /**
     * @brief Configurazione temporale comune dei carichi.
     */
    struct Timing
    {
        unsigned long minOnMs = 0;
        unsigned long minOffMs = 0;
    };

    /**
     * @brief Stato runtime comune dei carichi.
     */
    struct Runtime
    {
        bool enabled = true;
        bool state = false;

        unsigned long lastChange = 0;
    };

    /**
    * @brief Stato runtime di un comando verso un carico.
    */
    struct Command
    {
        /**
        * @brief Indica che la richiesta attende conferma.
        */
        bool pending = false;

        /**
        * @brief Stato richiesto al carico.
        */
        bool requestedState = false;

        /**
        * @brief Timestamp di emissione della richiesta.
        */
        unsigned long requestTime = 0UL;

        /**
        * @brief Timestamp dell'ultimo timeout.
        */
        unsigned long lastTimeout = 0UL;

        /**
        * @brief Primo timestamp in cui è consentito un nuovo tentativo.
        */
        unsigned long retryAllowedAt = 0UL;

        /**
        * @brief Numero di timeout consecutivi.
        */
        unsigned int retryCount = 0U;

        /**
        * @brief Indica che è stato raggiunto il limite dei tentativi.
        */
        bool retryBlocked = false;
    };

    /**
     * @brief Statistiche usate dal sistema di autotuning.
     */
    struct Statistics
    {
        unsigned int cyclesCount = 0;

        unsigned long lastCycleTimestamp = 0;

        float avgCycleTimeSec = 0.0f;
    };

    // ============================================================
    // CARICO ELETTRICO
    // ============================================================

    struct Load
    {
        /**
         * @brief Identità del carico.
         */
        struct Identity
        {
            String name;
        };

        /**
         * @brief Configurazione elettrica del carico.
         */
        struct Configuration
        {
            float nominalPower = 0.0f;

            int priority = 0;
        };

        Identity identity;
        Configuration config;

        Timing timing;
        Runtime runtime;
        Command command;
        Statistics stats;
        ShedState shed;
    };
    
    // ============================================================
    // SNAPSHOT
    // ============================================================

    struct Snapshot
    {
        size_t totalLoads = 0;
        size_t activeLoads = 0;

        size_t pendingCommands = 0;

        float totalNominalPower = 0.0f;
        float activeNominalPower = 0.0f;

        float hysteresisOn = 0.0f;
        float hysteresisOff = 0.0f;
    };

    /**
    * @brief Richiede il distacco del carico sacrificabile.
    *
    * Viene selezionato un solo carico:
    * - abilitato;
    * - acceso;
    * - senza comando pendente;
    * - con tempo minimo ON rispettato;
    * - con priorità minore.
    *
    * @param severity Severità della suggestion.
    * @param reason Motivazione del distacco.
    * @param now Timestamp corrente.
    * @param enforceMinimumTime Se true, rispetta il tempo minimo ON.
    *
    * @return true se è stato richiesto il distacco di un carico.
    */
    bool requestLowestPriorityLoadOff(
        int severity,
        const String& reason,
        unsigned long now,
        bool enforceMinimumTime = true,
        bool ignoreRetryState = false,
        ShedReason shedReason =
            ShedReason::HARD_LIMIT)
    {
        Load* selected =
            nullptr;

        for (auto& load :
            storage.loads)
        {
            if (!load.runtime.enabled)
                continue;

            if (!load.runtime.state)
                continue;

            if (load.command.pending)
                continue;

            if (!ignoreRetryState &&
                !isCommandRetryAllowed(
                    load.command,
                    now))
            {
                continue;
            }

            if (enforceMinimumTime &&
                !canChange(
                    now,
                    load.runtime.lastChange,
                    load.timing.minOnMs))
            {
                continue;
            }

            if (selected == nullptr ||
                load.config.priority <
                    selected->config.priority)
            {
                selected =
                    &load;
            }
        }

        if (selected == nullptr)
            return false;

        return requestLoadState(
            selected->identity.name,
            false,
            severity,
            reason,
            now,
            enforceMinimumTime,
            ignoreRetryState,
            shedReason);
    }
private:

    // ============================================================
    // STORAGE INTERNO
    // ============================================================

    struct Storage
    {
        std::vector<Load> loads;
    };

    // ============================================================
    // CALLBACK INTERNE
    // ============================================================

    struct Callbacks
    {
        LoadCb onLoadChange = nullptr;
        SuggestionCb onSuggestion = nullptr;
    };

    Config config;
    Storage storage;
    Callbacks callbacks;

    /**
    * @brief Processa il timeout di un carico elettrico.
    */
    void processLoadCommandTimeout(
        Load& load,
        unsigned long now)
    {
        const bool wasPending =
            load.command.pending;

        const bool requestedState =
            load.command.requestedState;

        const bool timedOut =
            processCommandTimeout(
                load.command,
                now);

        if (timedOut &&
            wasPending &&
            !requestedState)
        {
            load.shed.pending =
                false;
        }
    }
public:

    // ============================================================
    // CONFIGURAZIONE CALLBACK
    // ============================================================

    /**
     * @brief Configura le callback del manager.
     *
     * @param cbLoad Callback invocata alla conferma di un cambio
     *               dello stato reale di un carico.
     * @param cbSuggest Callback invocata quando il manager genera
     *                  una nuova richiesta di attivazione o distacco.
     */
    void setCallbacks(
        LoadCb cbLoad,
        SuggestionCb cbSuggest)
    {
        callbacks.onLoadChange = cbLoad;
        callbacks.onSuggestion = cbSuggest;
    }

    // ============================================================
    // CONFIGURAZIONE GENERALE
    // ============================================================

    /**
     * @brief Applica l'intera configurazione del manager.
     *
     * I valori non validi vengono normalizzati.
     *
     * @param value Nuova configurazione.
     */
    void setConfig(const Config& value)
    {
        config = value;

        normalizeConfig();
    }

    /**
     * @brief Restituisce la configurazione corrente.
     *
     * @return Riferimento costante alla configurazione.
     */
    const Config& getConfig() const
    {
        return config;
    }

    /**
     * @brief Imposta le isteresi di attacco e distacco.
     *
     * @param hysteresisOnValue Margine richiesto per l'attacco.
     * @param hysteresisOffValue Margine richiesto per il distacco.
     */
    void setHysteresis(
        float hysteresisOnValue,
        float hysteresisOffValue)
    {
        if (isfinite(hysteresisOnValue) &&
            hysteresisOnValue >= 0.0f)
        {
            config.hysteresisOn =
                hysteresisOnValue;
        }

        if (isfinite(hysteresisOffValue) &&
            hysteresisOffValue >= 0.0f)
        {
            config.hysteresisOff =
                hysteresisOffValue;
        }
    }

    /**
     * @brief Restituisce l'isteresi di attacco.
     */
    float getHysteresisOn() const
    {
        return config.hysteresisOn;
    }

    /**
     * @brief Restituisce l'isteresi di distacco.
     */
    float getHysteresisOff() const
    {
        return config.hysteresisOff;
    }

    /**
     * @brief Imposta il picco di forecast utilizzato dal controllo termico.
     *
     * @param watt Valore di picco in watt.
     */
    void setForecastPeak(float watt)
    {
        if (isfinite(watt) &&
            watt > 0.0f)
        {
            config.forecastPeak = watt;
        }
    }

    /**
     * @brief Imposta il timeout dei comandi pendenti.
     *
     * Un valore pari a zero disabilita la scadenza automatica.
     *
     * @param timeoutMs Timeout in millisecondi.
     */
    void setCommandTimeout(
        unsigned long timeoutMs)
    {
        config.commandTimeoutMs =
            timeoutMs;
    }

    // ============================================================
    // REGISTRAZIONE CARICHI
    // ============================================================

    /**
     * @brief Registra un nuovo carico elettrico.
     *
     * @param name Nome univoco del carico.
     * @param nominalPower Potenza nominale positiva in watt.
     * @param minOnSec Permanenza minima nello stato ON.
     * @param minOffSec Permanenza minima nello stato OFF.
     * @param priority Priorità del carico. Un valore maggiore indica
     *                 una priorità maggiore.
     *
     * @return true se il carico è stato aggiunto.
     */
    bool addLoad(
        const String& name,
        float nominalPower,
        unsigned long minOnSec = 5,
        unsigned long minOffSec = 5,
        int priority = 0)
    {
        if (name.length() == 0)
            return false;

        if (!isfinite(nominalPower) ||
            nominalPower <= 0.0f)
        {
            return false;
        }

        if (findLoad(name) != nullptr)
            return false;

        Load load;

        load.identity.name = name;

        load.config.nominalPower =
            nominalPower;

        load.config.priority =
            priority;

        load.timing.minOnMs =
            secondsToMilliseconds(minOnSec);

        load.timing.minOffMs =
            secondsToMilliseconds(minOffSec);

        storage.loads.push_back(load);

        return true;
    }
    
    // ============================================================
    // FEEDBACK STATO REALE
    // ============================================================

    /**
     * @brief Aggiorna lo stato reale di un carico elettrico.
     *
     * Deve essere richiamata quando il frontend riceve il feedback
     * effettivo dall'attuatore KNX, Modbus, relè o altra interfaccia.
     *
     * La funzione:
     * - conferma o annulla il comando pendente;
     * - aggiorna l'ultimo cambio di stato;
     * - aggiorna le statistiche dei cicli;
     * - invoca la callback di cambio stato.
     *
     * @param name Nome del carico.
     * @param newState Stato reale ricevuto.
     * @param now Timestamp corrente in millisecondi.
     *
     * @return false se il carico non esiste.
     */
    /**
    * @brief Aggiorna lo stato reale di un carico elettrico.
    *
    * La funzione conferma anche lo stato del distacco gestito.
    */
    bool updateLoadState(
        const String& name,
        bool newState,
        unsigned long now)
    {
        Load* load =
            findLoad(name);

        if (load == nullptr)
            return false;

        /*
        * Salva il comando prima del reset.
        */
        const bool hadPendingCommand =
            load->command.pending;

        const bool requestedState =
            load->command.requestedState;

        const bool shedWasPending =
            load->shed.pending;

        resetCommandRetries(
            load->command);

        /*
        * Conferma del distacco.
        */
        if (hadPendingCommand &&
            !requestedState &&
            !newState &&
            shedWasPending)
        {
            load->shed.pending =
                false;

            load->shed.active =
                true;

            load->shed.confirmedTime =
                now;
        }

        /*
        * Conferma riattivazione.
        */
        if (newState)
        {
            load->shed.pending =
                false;

            load->shed.active =
                false;

            load->shed.reason =
                ShedReason::NONE;

            load->shed.requestTime =
                0UL;

            load->shed.confirmedTime =
                0UL;
        }

        if (load->runtime.state ==
            newState)
        {
            return true;
        }

        updateLoadStatistics(
            *load,
            newState,
            now);

        load->runtime.state =
            newState;

        load->runtime.lastChange =
            now;

        if (callbacks.onLoadChange)
        {
            callbacks.onLoadChange(
                load->identity.name,
                newState);
        }

        return true;
    }

    
    // ============================================================
    // GESTIONE CARICHI ELETTRICI
    // ============================================================

    /**
     * @brief Valuta attacco o distacco dei carichi elettrici.
     *
     * L'algoritmo produce al massimo una suggestion per chiamata.
     *
     * Per l'attacco seleziona il carico spento con priorità maggiore
     * compatibile con il margine disponibile.
     *
     * Per il distacco seleziona il carico acceso con priorità minore.
     *
     * @param netWithBattery Potenza netta della rete. L'importazione
     *                       deve essere rappresentata con segno positivo.
     * @param dynamicLimit Limite dinamico corrente.
     * @param now Timestamp corrente in millisecondi.
     */
    void updateLoads(
        float netWithBattery,
        float dynamicLimit,
        unsigned long now)
    {
        processTimeouts(now);

        if (!isfinite(netWithBattery) ||
            !isfinite(dynamicLimit) ||
            dynamicLimit <= 0.0f)
        {
            return;
        }

        if (netWithBattery <=
            dynamicLimit - config.hysteresisOn)
        {
            Load* candidate =
                selectLoadToEnable(
                    netWithBattery,
                    dynamicLimit,
                    now);

            if (candidate)
            {
                requestLoadState(
                    candidate->identity.name,
                    false,
                    2,
                    "superamento limite",
                    now,
                    true,
                    false,
                    ShedReason::SOFT_LIMIT);

                return;
            }
        }

        if (netWithBattery >=
            dynamicLimit + config.hysteresisOff)
        {
            Load* candidate =
                selectLoadToDisable(now);

            if (candidate)
            {
                createCommand(
                    candidate->command,
                    false,
                    now);

                suggest(
                    "stacca:" +
                        candidate->identity.name,
                    2,
                    "superamento limite");

                return;
            }
        }
    } 

    // ============================================================
    // ACCESSORI
    // ============================================================

    /**
     * @brief Restituisce i carichi elettrici in sola lettura.
     */
    const std::vector<Load>& getLoads() const
    {
        return storage.loads;
    }
   
    /**
     * @brief Restituisce accesso mutabile ai carichi elettrici.
     *
     * Necessario per la compatibilità con AutoTuneManager e con
     * l'orchestratore esistente.
     */
    std::vector<Load>& getLoadsMutable()
    {
        return storage.loads;
    }
    
    /**
    * @brief Restituisce il numero di comandi elettrici pendenti.
    */
    size_t getPendingCount() const
    {
        size_t count =
            0U;

        for (const auto& load :
            storage.loads)
        {
            if (load.command.pending)
            {
                ++count;
            }
        }

        return count;
    }

    /**
    * @brief Genera la word di stato degli stacchi dei carichi.
    *
    * Per ciascun carico elettrico vengono impiegati due bit:
    *
    * bit index * 2     = richiesta di distacco pendente;
    * bit index * 2 + 1 = distacco confermato e attivo.
    *
    * Sono rappresentabili fino a otto carichi.
    *
    * @return Word di stato degli stacchi.
    */
    uint16_t getLoadShedStatusWord() const
    {
        uint16_t statusWord =
            0U;

        const size_t count =
            storage.loads.size() < 8U
                ? storage.loads.size()
                : 8U;

        for (size_t index = 0U;
            index < count;
            ++index)
        {
            const Load& load =
                storage.loads[index];

            const uint8_t pendingBit =
                static_cast<uint8_t>(
                    index * 2U);

            const uint8_t activeBit =
                static_cast<uint8_t>(
                    pendingBit + 1U);

            if (load.shed.pending)
            {
                statusWord |=
                    static_cast<uint16_t>(
                        1UL << pendingBit);
            }

            if (load.shed.active)
            {
                statusWord |=
                    static_cast<uint16_t>(
                        1UL << activeBit);
            }
        }

        return statusWord;
    }

    /**
    * @brief Genera lo snapshot aggregato dei carichi elettrici.
    */
    Snapshot getSnapshot() const
    {
        Snapshot snapshot;

        snapshot.totalLoads =
            storage.loads.size();

        snapshot.hysteresisOn =
            config.hysteresisOn;

        snapshot.hysteresisOff =
            config.hysteresisOff;

        for (const auto& load :
            storage.loads)
        {
            snapshot.totalNominalPower +=
                load.config.nominalPower;

            if (load.runtime.state)
            {
                ++snapshot.activeLoads;

                snapshot.activeNominalPower +=
                    load.config.nominalPower;
            }

            if (load.command.pending)
            {
                ++snapshot.pendingCommands;
            }
        }

        return snapshot;
    }

    /**
    * @brief Configura il ritardo tra due tentativi.
    *
    * @param backoffMs Ritardo base espresso in millisecondi.
    */
    void setCommandRetryBackoff(
        unsigned long backoffMs)
    {
        config.commandRetryBackoffMs =
            backoffMs;
    }

    /**
    * @brief Configura il numero massimo di timeout consecutivi.
    *
    * @param maximumRetries Numero massimo. Zero significa illimitato.
    */
    void setCommandMaxRetries(
        unsigned int maximumRetries)
    {
        config.commandMaxRetries =
            maximumRetries;
    }

    /**
    * @brief Riabilita i tentativi per un carico elettrico.
    *
    * La funzione azzera contatori, timeout e blocco retry senza
    * modificare lo stato reale del carico.
    *
    * @param name Nome del carico.
    *
    * @return false se il carico non esiste.
    */
    bool resetLoadCommandRetries(
        const String& name)
    {
        Load* load =
            findLoad(name);

        if (load == nullptr)
            return false;

        resetCommandRetries(
            load->command);

        return true;
    }

    /**
    * @brief Azzera pending, timeout, backoff e retry
    * di tutti i carichi elettrici.
    */
    void resetAllCommandRetries()
    {
        for (auto& load :
            storage.loads)
        {
            resetCommandRetries(
                load.command);
        }
    }

    bool requestLoadState(
        const String& name,
        bool requestedState,
        int severity,
        const String& reason,
        unsigned long now,
        bool enforceMinimumTime = true,
        bool ignoreRetryState = false,
        ShedReason shedReason = ShedReason::EXTERNAL_REQUEST)
    {
        Load* load =
            findLoad(name);

        if (load == nullptr)
            return false;

        if (!load->runtime.enabled)
            return false;

        if (load->command.pending)
            return false;

        if (!ignoreRetryState &&
            !isCommandRetryAllowed(
                load->command,
                now))
        {
            return false;
        }

        if (load->runtime.state ==
            requestedState)
        {
            return false;
        }

        if (enforceMinimumTime)
        {
            const unsigned long minimumTime =
                requestedState
                    ? load->timing.minOffMs
                    : load->timing.minOnMs;

            if (!canChange(
                    now,
                    load->runtime.lastChange,
                    minimumTime))
            {
                return false;
            }
        }

        createCommand(
            load->command,
            requestedState,
            now);

        /*
        * Se viene richiesto OFF, registra lo stacco pendente.
        */
        if (!requestedState)
        {
            load->shed.pending =
                true;

            load->shed.reason =
                shedReason;

            load->shed.requestTime =
                now;
        }
        else
        {
            /*
            * La richiesta ON rappresenta il ripristino del carico.
            * shed.active verrà cancellato alla conferma dello stato ON.
            */
            load->shed.pending =
                false;
        }

        const String action =
            String(
                requestedState
                    ? "attacca:"
                    : "stacca:") +
            load->identity.name;

        suggest(
            action,
            severity,
            reason);

        return true;
    }

private:

    // ============================================================
    // VALIDAZIONE CONFIGURAZIONE
    // ============================================================

    /**
    * @brief Normalizza i parametri generali.
    */
    void normalizeConfig()
    {
        if (!isfinite(config.hysteresisOn) ||
            config.hysteresisOn < 0.0f)
        {
            config.hysteresisOn =
                200.0f;
        }

        if (!isfinite(config.hysteresisOff) ||
            config.hysteresisOff < 0.0f)
        {
            config.hysteresisOff =
                200.0f;
        }

        if (!isfinite(config.forecastPeak) ||
            config.forecastPeak <= 0.0f)
        {
            config.forecastPeak =
                3000.0f;
        }

        /*
        * commandTimeoutMs può essere zero:
        * in quel caso il timeout è disabilitato.
        *
        * commandRetryBackoffMs può essere zero:
        * in quel caso non viene applicato alcun ritardo.
        *
        * commandMaxRetries può essere zero:
        * in quel caso il numero di tentativi è illimitato.
        */
    }


    // ============================================================
    // RICERCA
    // ============================================================

    /**
     * @brief Cerca un carico elettrico per nome.
     */
    Load* findLoad(const String& name)
    {
        for (auto& load : storage.loads)
        {
            if (load.identity.name == name)
                return &load;
        }

        return nullptr;
    }


    // ============================================================
    // SELEZIONE CARICHI
    // ============================================================

    /**
     * @brief Seleziona il carico da attivare.
     *
     * Viene scelto il carico compatibile con il margine e avente
     * priorità maggiore.
     */
    Load* selectLoadToEnable(
        float netWithBattery,
        float dynamicLimit,
        unsigned long now)
    {
        Load* selected = nullptr;

        for (auto& load : storage.loads)
        {
            if (!load.runtime.enabled ||
                load.runtime.state ||
                load.command.pending)
            {
                continue;
            }

            if (!isCommandRetryAllowed(
                    load.command,
                    now))
            {
                continue;
            }

            if (!canChange(
                    now,
                    load.runtime.lastChange,
                    load.timing.minOffMs))
            {
                continue;
            }

            const float projectedPower =
                netWithBattery +
                load.config.nominalPower;

            if (projectedPower >
                dynamicLimit -
                config.hysteresisOn)
            {
                continue;
            }

            if (!selected ||
                load.config.priority >
                    selected->config.priority)
            {
                selected = &load;
            }
        }

        return selected;
    }

    /**
     * @brief Seleziona il carico da disattivare.
     *
     * Viene scelto il carico acceso avente priorità minore.
     */
    Load* selectLoadToDisable(
        unsigned long now)
    {
        Load* selected = nullptr;

        for (auto& load : storage.loads)
        {
            if (!load.runtime.enabled ||
                !load.runtime.state ||
                load.command.pending)
            {
                continue;
            }

            if (!isCommandRetryAllowed(
                    load.command,
                    now))
            {
                continue;
            }

            if (!canChange(
                    now,
                    load.runtime.lastChange,
                    load.timing.minOnMs))
            {
                continue;
            }

            if (!selected ||
                load.config.priority <
                    selected->config.priority)
            {
                selected = &load;
            }
        }

        return selected;
    }

    // ============================================================
    // COMANDI
    // ============================================================

    /**
    * @brief Registra un nuovo comando pendente.
    *
    * La funzione non modifica il contatore dei retry. Il contatore
    * viene incrementato solamente quando un comando scade.
    *
    * @param command Stato runtime del comando.
    * @param requestedState Stato richiesto.
    * @param now Timestamp corrente.
    */
    static void createCommand(
        Command& command,
        bool requestedState,
        unsigned long now)
    {
        command.pending =
            true;

        command.requestedState =
            requestedState;

        command.requestTime =
            now;
    }

    /**
    * @brief Cancella solamente la richiesta pendente.
    *
    * Le informazioni relative ai retry vengono conservate.
    *
    * @param command Comando da cancellare.
    */
    static void clearCommand(
        Command& command)
    {
        command.pending =
            false;

        command.requestedState =
            false;

        command.requestTime =
            0UL;
    }

    /**
    * @brief Calcola il backoff del prossimo tentativo.
    *
    * @param retryCount Numero di timeout consecutivi.
    *
    * @return Intervallo di backoff in millisecondi.
    */
    unsigned long calculateRetryBackoff(
        unsigned int retryCount) const
    {
        if (config.commandRetryBackoffMs == 0UL ||
            retryCount == 0U)
        {
            return 0UL;
        }

        const unsigned long maximumMultiplier =
            0xFFFFFFFFUL /
            config.commandRetryBackoffMs;

        if (static_cast<unsigned long>(
                retryCount) >
            maximumMultiplier)
        {
            return 0xFFFFFFFFUL;
        }

        return
            config.commandRetryBackoffMs *
            static_cast<unsigned long>(
                retryCount);
    }

    /**
    * @brief Verifica se un nuovo comando può essere emesso.
    *
    * La sottrazione signed permette di confrontare timestamp basati
    * su millis() mantenendo il comportamento corretto al rollover,
    * purché gli intervalli siano inferiori a metà del range uint32_t.
    *
    * @param command Stato del comando.
    * @param now Timestamp corrente.
    *
    * @return true se il comando può essere ritentato.
    */
    static bool isCommandRetryAllowed(
        const Command& command,
        unsigned long now)
    {
        if (command.pending)
            return false;

        if (command.retryBlocked)
            return false;

        if (command.retryAllowedAt == 0UL)
            return true;

        return
            static_cast<long>(
                now -
                command.retryAllowedAt) >= 0L;
    }

    /**
    * @brief Azzera completamente lo stato retry di un comando.
    *
    * Viene utilizzata quando:
    * - arriva il feedback reale;
    * - il reset viene richiesto esplicitamente;
    * - il carico viene riabilitato manualmente.
    *
    * @param command Comando da reinizializzare.
    */
    static void resetCommandRetries(
        Command& command)
    {
        command.pending =
            false;

        command.requestedState =
            false;

        command.requestTime =
            0UL;

        command.lastTimeout =
            0UL;

        command.retryAllowedAt =
            0UL;

        command.retryCount =
            0U;

        command.retryBlocked =
            false;
    }

    /**
    * @brief Elabora i timeout dei comandi elettrici pendenti.
    *
    * @param now Timestamp corrente.
    */
    void processTimeouts(
        unsigned long now)
    {
        if (config.commandTimeoutMs == 0UL)
            return;

        for (auto& load :
            storage.loads)
        {
            processLoadCommandTimeout(
                load,
                now);
        }
    }

    /**
    * @brief Verifica la scadenza di un comando pendente.
    *
    * Quando il comando supera il timeout:
    * - cancella lo stato pending;
    * - registra il timestamp del timeout;
    * - incrementa il numero di tentativi falliti;
    * - applica il backoff;
    * - blocca nuovi tentativi al raggiungimento del limite.
    *
    * @param command Comando da verificare.
    * @param now Timestamp corrente.
    *
    * @return true se il comando è scaduto.
    */
    bool processCommandTimeout(
        Command& command,
        unsigned long now)
    {
        if (!command.pending)
            return false;

        if (config.commandTimeoutMs == 0UL)
            return false;

        if (static_cast<unsigned long>(
                now - command.requestTime) <
            config.commandTimeoutMs)
        {
            return false;
        }

        command.pending =
            false;

        command.requestedState =
            false;

        command.requestTime =
            0UL;

        command.lastTimeout =
            now;

        if (command.retryCount <
            0xFFFFU)
        {
            ++command.retryCount;
        }

        /*
        * commandMaxRetries == 0 significa tentativi illimitati.
        */
        if (config.commandMaxRetries > 0U &&
            command.retryCount >=
                config.commandMaxRetries)
        {
            command.retryBlocked =
                true;

            command.retryAllowedAt =
                0UL;

            return true;
        }

        const unsigned long backoff =
            calculateRetryBackoff(
                command.retryCount);

        command.retryAllowedAt =
            now + backoff;

        return true;
    }

    // ============================================================
    // STATISTICHE
    // ============================================================

    /**
     * @brief Aggiorna le statistiche di commutazione.
     *
     * Un ciclo viene conteggiato alla transizione ON -> OFF.
     */
    static void updateLoadStatistics(
        Load& load,
        bool newState,
        unsigned long now)
    {
        if (!load.runtime.state ||
            newState)
        {
            return;
        }

        const unsigned long cycleMs =
            static_cast<unsigned long>(
                now -
                load.runtime.lastChange);

        const float cycleTimeSec =
            static_cast<float>(cycleMs) /
            1000.0f;

        ++load.stats.cyclesCount;

        load.stats.lastCycleTimestamp =
            now;

        if (load.stats.avgCycleTimeSec <=
            0.0f)
        {
            load.stats.avgCycleTimeSec =
                cycleTimeSec;

            return;
        }

        const float previousCount =
            static_cast<float>(
                load.stats.cyclesCount - 1);

        load.stats.avgCycleTimeSec =
            ((load.stats.avgCycleTimeSec *
              previousCount) +
             cycleTimeSec) /
            static_cast<float>(
                load.stats.cyclesCount);
    }


    // ============================================================
    // HELPER
    // ============================================================

    /**
     * @brief Verifica il rispetto del tempo minimo di permanenza.
     *
     * La sottrazione unsigned mantiene il comportamento corretto
     * anche in presenza del rollover di millis().
     */
    static bool canChange(
        unsigned long now,
        unsigned long lastChange,
        unsigned long minimumTime)
    {
        return
            static_cast<unsigned long>(
                now - lastChange) >=
            minimumTime;
    }

    /**
     * @brief Converte secondi in millisecondi con saturazione.
     */
    static unsigned long
    secondsToMilliseconds(
        unsigned long seconds)
    {
        const unsigned long maximumSeconds =
            0xFFFFFFFFUL / 1000UL;

        if (seconds > maximumSeconds)
            return 0xFFFFFFFFUL;

        return seconds * 1000UL;
    }

    /**
     * @brief Invia una suggestion al frontend.
     */
    void suggest(
        const String& action,
        int severity,
        const String& reason)
    {
        if (callbacks.onSuggestion)
        {
            callbacks.onSuggestion(
                action,
                severity,
                reason);
        }
    }
};


class LimitManager
{
public:

    // ============================================================
    // STATO
    // ============================================================

    /**
     * @brief Stato operativo rispetto ai limiti di potenza.
     */
    enum class State
    {
        NORMAL,
        WARNING,
        EXCEEDED,
        HARD_EXCEEDED
    };

    // ============================================================
    // CALLBACK
    // ============================================================

    using LimitCb =
        std::function<void(
            float netPower,
            float limit)>;

    using HardLimitCb =
        std::function<void(
            float netPower,
            float limit)>;

    /**
     * @brief Evento strutturato generato a ogni transizione.
     */
    struct StateEvent
    {
        State previousState =
            State::NORMAL;

        State currentState =
            State::NORMAL;

        float netPower = 0.0f;

        float effectiveLimit = 0.0f;

        float hardLimit = 0.0f;

        unsigned long timestamp = 0;
    };

    using StateCallback =
        std::function<void(
            const StateEvent& event)>;

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================

    struct Config
    {
        /**
         * @brief Limite nominale di rete.
         */
        float gridLimit = 3000.0f;

        /**
         * @brief Limite assoluto di protezione.
         *
         * Un valore pari a zero disabilita l'hard limit.
         */
        float hardLimit = 0.0f;

        /**
         * @brief Percentuale del limite dinamico che attiva WARNING.
         */
        float warningPct = 0.90f;

        /**
         * @brief Isteresi usata nel rientro da WARNING.
         */
        float warningResetHysteresisWatt =
            100.0f;

        /**
         * @brief Isteresi usata nel rientro da EXCEEDED.
         */
        float exceededResetHysteresisWatt =
            100.0f;

        /**
         * @brief Isteresi usata nel rientro da HARD_EXCEEDED.
         */
        float hardResetHysteresisWatt =
            150.0f;
    };

    // ============================================================
    // RUNTIME
    // ============================================================

    struct Runtime
    {
        State state =
            State::NORMAL;

        float lastNetPower = 0.0f;

        float lastDynamicLimit = 0.0f;

        unsigned long lastCheck = 0;

        unsigned long lastTransition = 0;

        unsigned long transitionCount = 0;
    };

    // ============================================================
    // SNAPSHOT
    // ============================================================

    struct Snapshot
    {
        Config config;

        Runtime runtime;
    };

private:

    // ============================================================
    // CALLBACK INTERNE
    // ============================================================

    struct Callbacks
    {
        LimitCb onWarn = nullptr;

        LimitCb onExceed = nullptr;

        HardLimitCb onHardLimit = nullptr;

        StateCallback onStateChanged = nullptr;
    };

    Config config;

    Runtime runtime;

    Callbacks callbacks;

public:

    // ============================================================
    // CONFIGURAZIONE CALLBACK
    // ============================================================

    /**
     * @brief Registra le callback legacy di warning e superamento.
     *
     * @param warnCb Callback di ingresso nello stato WARNING.
     * @param exceedCb Callback di ingresso nello stato EXCEEDED.
     */
    void setCallbacks(
        LimitCb warnCb,
        LimitCb exceedCb)
    {
        callbacks.onWarn =
            warnCb;

        callbacks.onExceed =
            exceedCb;
    }

    /**
     * @brief Registra la callback dell'hard limit.
     *
     * @param callback Callback di ingresso nello stato HARD_EXCEEDED.
     */
    void setHardLimitCallback(
        HardLimitCb callback)
    {
        callbacks.onHardLimit =
            callback;
    }

    /**
     * @brief Registra la callback strutturata delle transizioni.
     *
     * A differenza delle callback legacy, viene invocata anche
     * durante il rientro verso WARNING oppure NORMAL.
     *
     * @param callback Callback che riceve StateEvent.
     */
    void setStateCallback(
        StateCallback callback)
    {
        callbacks.onStateChanged =
            callback;
    }

    // ============================================================
    // CONFIGURAZIONE COMPLETA
    // ============================================================

    /**
     * @brief Applica la configurazione completa.
     *
     * I valori non validi vengono normalizzati.
     *
     * @param value Nuova configurazione.
     */
    void setConfig(
        const Config& value)
    {
        config = value;

        normalizeConfig();
    }

    /**
     * @brief Restituisce la configurazione corrente.
     */
    const Config& getConfig() const
    {
        return config;
    }

    // ============================================================
    // LIMITI
    // ============================================================

    /**
     * @brief Imposta il limite nominale.
     *
     * @param limitWatt Limite positivo espresso in watt.
     *
     * @return true se il valore è stato applicato.
     */
    bool setLimit(
        float limitWatt)
    {
        if (!isfinite(limitWatt) ||
            limitWatt <= 0.0f)
        {
            return false;
        }

        config.gridLimit =
            limitWatt;

        return true;
    }

    /**
     * @brief Imposta l'hard limit.
     *
     * Un valore pari a zero disabilita l'hard limit.
     *
     * @param limitWatt Hard limit espresso in watt.
     *
     * @return true se il valore è stato applicato.
     */
    bool setHardLimit(
        float limitWatt)
    {
        if (!isfinite(limitWatt) ||
            limitWatt < 0.0f)
        {
            return false;
        }

        config.hardLimit =
            limitWatt;

        return true;
    }

    /**
     * @brief Imposta la percentuale di warning.
     *
     * @param percentage Valore compreso tra zero e uno.
     *
     * @return true se il valore è stato applicato.
     */
    bool setWarningThreshold(
        float percentage)
    {
        if (!isfinite(percentage) ||
            percentage <= 0.0f ||
            percentage >= 1.0f)
        {
            return false;
        }

        config.warningPct =
            percentage;

        return true;
    }

    /**
     * @brief Configura le isteresi di rientro.
     *
     * @param warningWatt Isteresi dello stato WARNING.
     * @param exceededWatt Isteresi dello stato EXCEEDED.
     * @param hardWatt Isteresi dello stato HARD_EXCEEDED.
     *
     * @return true se tutti i valori sono validi.
     */
    bool setResetHysteresis(
        float warningWatt,
        float exceededWatt,
        float hardWatt)
    {
        if (!isfinite(warningWatt) ||
            !isfinite(exceededWatt) ||
            !isfinite(hardWatt))
        {
            return false;
        }

        if (warningWatt < 0.0f ||
            exceededWatt < 0.0f ||
            hardWatt < 0.0f)
        {
            return false;
        }

        config.warningResetHysteresisWatt =
            warningWatt;

        config.exceededResetHysteresisWatt =
            exceededWatt;

        config.hardResetHysteresisWatt =
            hardWatt;

        return true;
    }

    // ============================================================
    // ACCESSORI
    // ============================================================

    float getLimit() const
    {
        return config.gridLimit;
    }

    float getHardLimit() const
    {
        return config.hardLimit;
    }

    float getWarningThreshold() const
    {
        return config.warningPct;
    }

    State getState() const
    {
        return runtime.state;
    }

    const Runtime& getRuntime() const
    {
        return runtime;
    }

    Snapshot getSnapshot() const
    {
        Snapshot snapshot;

        snapshot.config =
            config;

        snapshot.runtime =
            runtime;

        return snapshot;
    }

    // ============================================================
    // RESET
    // ============================================================

    /**
     * @brief Reinizializza lo stato runtime.
     *
     * La configurazione e le callback vengono conservate.
     */
    void reset(
        unsigned long now = 0)
    {
        runtime =
            Runtime{};

        runtime.lastCheck =
            now;

        runtime.lastTransition =
            now;
    }

    // ============================================================
    // VERIFICA LIMITI
    // ============================================================

    /**
     * @brief Verifica la potenza rispetto ai limiti configurati.
     *
     * Applica isteresi sulle transizioni di rientro:
     *
     * - HARD_EXCEEDED viene mantenuto fino a quando la potenza
     *   scende sotto hardLimit - hardResetHysteresisWatt;
     * - EXCEEDED viene mantenuto fino a quando la potenza scende
     *   sotto dynamicLimit - exceededResetHysteresisWatt;
     * - WARNING viene mantenuto fino a quando la potenza scende
     *   sotto warningLimit - warningResetHysteresisWatt.
     *
     * @param netPower Potenza netta della rete.
     * @param dynamicLimit Limite operativo corrente.
     * @param now Timestamp corrente.
     *
     * @return true se è avvenuta una transizione di stato.
     */
    bool check(
        float netPower,
        float dynamicLimit,
        unsigned long now)
    {
        if (!isfinite(netPower) ||
            !isfinite(dynamicLimit) ||
            dynamicLimit <= 0.0f)
        {
            return false;
        }

        runtime.lastNetPower =
            netPower;

        runtime.lastDynamicLimit =
            dynamicLimit;

        runtime.lastCheck =
            now;

        const State previousState =
            runtime.state;

        const State newState =
            classifyState(
                netPower,
                dynamicLimit,
                previousState);

        if (newState ==
            previousState)
        {
            return false;
        }

        runtime.state =
            newState;

        runtime.lastTransition =
            now;

        ++runtime.transitionCount;

        notifyTransition(
            previousState,
            newState,
            netPower,
            dynamicLimit,
            now);

        return true;
    }

    /**
     * @brief Overload compatibile con la precedente API.
     *
     * Usa millis() come timestamp.
     */
    bool check(
        float netPower,
        float dynamicLimit)
    {
        return check(
            netPower,
            dynamicLimit,
            millis());
    }

    // ============================================================
    // DIAGNOSTICA
    // ============================================================

    class Diagnostic
    {
    public:

        static void Report(
            const LimitManager& manager)
        {
            const Snapshot snapshot =
                manager.getSnapshot();

            Serial.println(
                "\n===== LIMIT MANAGER =====");

            Serial.print("Grid limit: ");
            Serial.print(
                snapshot.config.gridLimit);
            Serial.println(" W");

            Serial.print("Hard limit: ");
            Serial.print(
                snapshot.config.hardLimit);
            Serial.println(" W");

            Serial.print("Warning threshold: ");
            Serial.print(
                snapshot.config.warningPct *
                100.0f);
            Serial.println(" %");

            Serial.print("Warning reset hysteresis: ");
            Serial.print(
                snapshot.config
                    .warningResetHysteresisWatt);
            Serial.println(" W");

            Serial.print("Exceeded reset hysteresis: ");
            Serial.print(
                snapshot.config
                    .exceededResetHysteresisWatt);
            Serial.println(" W");

            Serial.print("Hard reset hysteresis: ");
            Serial.print(
                snapshot.config
                    .hardResetHysteresisWatt);
            Serial.println(" W");

            Serial.print("Last net power: ");
            Serial.print(
                snapshot.runtime.lastNetPower);
            Serial.println(" W");

            Serial.print("Last dynamic limit: ");
            Serial.print(
                snapshot.runtime.lastDynamicLimit);
            Serial.println(" W");

            Serial.print("State: ");

            printState(
                snapshot.runtime.state);

            Serial.print("Last check: ");
            Serial.println(
                snapshot.runtime.lastCheck);

            Serial.print("Last transition: ");
            Serial.println(
                snapshot.runtime.lastTransition);

            Serial.print("Transition count: ");
            Serial.println(
                snapshot.runtime.transitionCount);

            Serial.println(
                "===== END LIMIT MANAGER =====\n");
        }

    private:

        static void printState(
            State state)
        {
            switch (state)
            {
                case State::NORMAL:
                    Serial.println("NORMAL");
                    break;

                case State::WARNING:
                    Serial.println("WARNING");
                    break;

                case State::EXCEEDED:
                    Serial.println("EXCEEDED");
                    break;

                case State::HARD_EXCEEDED:
                    Serial.println(
                        "HARD_EXCEEDED");
                    break;

                default:
                    Serial.println("UNKNOWN");
                    break;
            }
        }
    };

private:

    // ============================================================
    // NORMALIZZAZIONE
    // ============================================================

    /**
     * @brief Normalizza la configurazione interna.
     */
    void normalizeConfig()
    {
        if (!isfinite(config.gridLimit) ||
            config.gridLimit <= 0.0f)
        {
            config.gridLimit =
                3000.0f;
        }

        if (!isfinite(config.hardLimit) ||
            config.hardLimit < 0.0f)
        {
            config.hardLimit =
                0.0f;
        }

        if (!isfinite(config.warningPct) ||
            config.warningPct <= 0.0f ||
            config.warningPct >= 1.0f)
        {
            config.warningPct =
                0.90f;
        }

        normalizeNonNegative(
            config.warningResetHysteresisWatt,
            100.0f);

        normalizeNonNegative(
            config.exceededResetHysteresisWatt,
            100.0f);

        normalizeNonNegative(
            config.hardResetHysteresisWatt,
            150.0f);
    }

    /**
     * @brief Normalizza un valore float non negativo.
     */
    static void normalizeNonNegative(
        float& value,
        float fallback)
    {
        if (!isfinite(value) ||
            value < 0.0f)
        {
            value =
                fallback;
        }
    }

    // ============================================================
    // CLASSIFICAZIONE
    // ============================================================

    /**
     * @brief Classifica lo stato applicando isteresi di rientro.
     */
    State classifyState(
        float netPower,
        float dynamicLimit,
        State previousState) const
    {
        const float warningLimit =
            dynamicLimit *
            config.warningPct;

        /*
         * Le soglie di ingresso vengono sempre valutate dalla più
         * grave alla meno grave.
         */
        if (config.hardLimit > 0.0f &&
            netPower >= config.hardLimit)
        {
            return
                State::HARD_EXCEEDED;
        }

        /*
         * Permanenza nello stato HARD_EXCEEDED.
         */
        if (previousState ==
                State::HARD_EXCEEDED &&
            config.hardLimit > 0.0f &&
            netPower >=
                subtractFloorZero(
                    config.hardLimit,
                    config.hardResetHysteresisWatt))
        {
            return
                State::HARD_EXCEEDED;
        }

        if (netPower >= dynamicLimit)
        {
            return
                State::EXCEEDED;
        }

        /*
         * Permanenza nello stato EXCEEDED.
         */
        if (previousState ==
                State::EXCEEDED &&
            netPower >=
                subtractFloorZero(
                    dynamicLimit,
                    config.exceededResetHysteresisWatt))
        {
            return
                State::EXCEEDED;
        }

        if (netPower >= warningLimit)
        {
            return
                State::WARNING;
        }

        /*
         * Permanenza nello stato WARNING.
         */
        if (previousState ==
                State::WARNING &&
            netPower >=
                subtractFloorZero(
                    warningLimit,
                    config.warningResetHysteresisWatt))
        {
            return
                State::WARNING;
        }

        return
            State::NORMAL;
    }

    // ============================================================
    // CALLBACK
    // ============================================================

    /**
     * @brief Notifica una transizione di stato.
     */
    void notifyTransition(
        State previousState,
        State currentState,
        float netPower,
        float dynamicLimit,
        unsigned long now)
    {
        /*
         * Callback strutturata: riceve tutte le transizioni,
         * compresi i rientri.
         */
        if (callbacks.onStateChanged)
        {
            StateEvent event;

            event.previousState =
                previousState;

            event.currentState =
                currentState;

            event.netPower =
                netPower;

            event.effectiveLimit =
                dynamicLimit;

            event.hardLimit =
                config.hardLimit;

            event.timestamp =
                now;

            callbacks.onStateChanged(
                event);
        }

        /*
         * Callback legacy: vengono invocate solamente quando si
         * entra nello stato corrispondente.
         */
        switch (currentState)
        {
            case State::WARNING:

                if (callbacks.onWarn)
                {
                    callbacks.onWarn(
                        netPower,
                        dynamicLimit);
                }

                break;

            case State::EXCEEDED:

                if (callbacks.onExceed)
                {
                    callbacks.onExceed(
                        netPower,
                        dynamicLimit);
                }

                break;

            case State::HARD_EXCEEDED:

                if (callbacks.onHardLimit)
                {
                    callbacks.onHardLimit(
                        netPower,
                        config.hardLimit);
                }

                break;

            case State::NORMAL:
            default:
                break;
        }
    }

    // ============================================================
    // HELPER
    // ============================================================

    /**
     * @brief Sottrae un'isteresi evitando risultati negativi.
     */
    static float subtractFloorZero(
        float value,
        float amount)
    {
        const float result =
            value - amount;

        return
            result > 0.0f
                ? result
                : 0.0f;
    }
};

class AutoTuneManager
{
public:

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================

    /**
     * @brief Configurazione completa dell'algoritmo di autotuning.
     */
    struct Config
    {
        /**
         * @brief Percentuale di variazione applicata a ogni tuning.
         *
         * Esempio:
         * 0.05f corrisponde al 5%.
         */
        float tuneStepPercent = 0.05f;

        /**
         * @brief Limiti applicabili alle isteresi.
         */
        struct HysteresisLimits
        {
            float minimum = 50.0f;
            float maximum = 2000.0f;
        };

        /**
         * @brief Limiti applicabili ai tempi minimi dei carichi.
         *
         * I valori sono espressi in secondi.
         */
        struct TimingLimits
        {
            unsigned long minOnMinimumSec = 5UL;
            unsigned long minOnMaximumSec = 3600UL;

            unsigned long minOffMinimumSec = 5UL;
            unsigned long minOffMaximumSec = 3600UL;
        };

        /**
         * @brief Soglie utilizzate per classificare le commutazioni.
         */
        struct CycleThresholds
        {
            /**
             * Numero medio di cicli oltre il quale viene aumentata
             * l'isteresi generale.
             */
            float increaseHysteresisAbove = 3.0f;

            /**
             * Numero medio di cicli sotto il quale può essere ridotta
             * l'isteresi generale.
             *
             * La riduzione viene effettuata solamente quando esistono
             * statistiche valide.
             */
            float decreaseHysteresisBelow = 1.0f;

            /**
             * Numero minimo di cicli individuali necessario per
             * aumentare i tempi minimi di un carico.
             */
            unsigned int fastCycleMinimumCount = 3U;

            /**
             * Durata media massima che identifica cicli rapidi.
             */
            float fastCycleMaximumSec = 120.0f;

            /**
             * Durata media minima che identifica cicli molto lunghi.
             */
            float longCycleMinimumSec = 600.0f;
        };

        HysteresisLimits hysteresis;
        TimingLimits timing;
        CycleThresholds cycles;
    };

    // ============================================================
    // STATO OPERATIVO
    // ============================================================

    /**
     * @brief Stato corrente dei parametri regolati.
     */
    struct State
    {
        float hysteresisOn = 200.0f;
        float hysteresisOff = 200.0f;

        unsigned long executionCount = 0UL;
        unsigned long lastExecutionTimestamp = 0UL;

        bool enabled = true;
    };

    // ============================================================
    // STATISTICHE ULTIMA ESECUZIONE
    // ============================================================

    /**
     * @brief Statistiche prodotte dall'ultima esecuzione.
     */
    struct Statistics
    {
        /**
         * Media dei cicli dei soli carichi con statistiche valide.
         */
        float averageCycles = 0.0f;

        /**
         * Numero totale di carichi ricevuti.
         */
        size_t totalLoads = 0U;

        /**
         * Numero di carichi abilitati.
         */
        size_t enabledLoads = 0U;

        /**
         * Numero di carichi che hanno prodotto almeno un ciclo.
         */
        size_t validLoads = 0U;

        /**
         * Numero di carichi i cui tempi minimi sono stati aumentati.
         */
        size_t increasedTimingLoads = 0U;

        /**
         * Numero di carichi i cui tempi minimi sono stati ridotti.
         */
        size_t decreasedTimingLoads = 0U;

        bool hysteresisIncreased = false;
        bool hysteresisDecreased = false;

        /**
         * Indica se l'esecuzione ha trovato dati utilizzabili.
         */
        bool dataAvailable = false;
    };

    // ============================================================
    // SNAPSHOT
    // ============================================================

    /**
     * @brief Fotografia completa dell'AutoTuneManager.
     */
    struct Snapshot
    {
        Config config;
        State state;
        Statistics statistics;
    };

    // ============================================================
    // CALLBACK
    // ============================================================

    /**
     * @brief Callback invocata al completamento di un tuning valido.
     */
    using TuneCallback =
        std::function<void(
            const Snapshot& snapshot)>;

private:

    // ============================================================
    // DATI INTERNI
    // ============================================================

    Config config;
    State state;
    Statistics statistics;

    TuneCallback onTune = nullptr;

public:

    // ============================================================
    // COSTRUTTORE
    // ============================================================

    /**
     * @brief Costruisce AutoTuneManager con i valori predefiniti.
     */
    AutoTuneManager() = default;

    // ============================================================
    // CONFIGURAZIONE STRUTTURATA
    // ============================================================

    /**
     * @brief Applica la configurazione completa.
     *
     * I valori non validi vengono normalizzati.
     *
     * @param value Nuova configurazione.
     */
    void setConfig(
        const Config& value)
    {
        config = value;

        normalizeConfig();

        normalizeState();
    }

    /**
     * @brief Restituisce la configurazione corrente.
     */
    const Config& getConfig() const
    {
        return config;
    }

    // ============================================================
    // CONFIGURAZIONE LEGACY
    // ============================================================

    /**
     * @brief Configura l'autotuning mantenendo l'API precedente.
     *
     * @param hOn Isteresi iniziale per l'attivazione.
     * @param hOff Isteresi iniziale per il distacco.
     * @param stepPct Percentuale di regolazione per esecuzione.
     * @param hMin Isteresi minima.
     * @param hMax Isteresi massima.
     * @param minOnMin Tempo ON minimo assoluto, espresso in secondi.
     * @param minOnMax Tempo ON massimo assoluto, espresso in secondi.
     * @param minOffMin Tempo OFF minimo assoluto, espresso in secondi.
     * @param minOffMax Tempo OFF massimo assoluto, espresso in secondi.
     */
    void configure(
        float hOn,
        float hOff,
        float stepPct = 0.05f,
        float hMin = 50.0f,
        float hMax = 2000.0f,
        unsigned long minOnMin = 5UL,
        unsigned long minOnMax = 3600UL,
        unsigned long minOffMin = 5UL,
        unsigned long minOffMax = 3600UL)
    {
        state.hysteresisOn =
            hOn;

        state.hysteresisOff =
            hOff;

        config.tuneStepPercent =
            stepPct;

        config.hysteresis.minimum =
            hMin;

        config.hysteresis.maximum =
            hMax;

        config.timing.minOnMinimumSec =
            minOnMin;

        config.timing.minOnMaximumSec =
            minOnMax;

        config.timing.minOffMinimumSec =
            minOffMin;

        config.timing.minOffMaximumSec =
            minOffMax;

        normalizeConfig();

        normalizeState();
    }

    // ============================================================
    // ABILITAZIONE
    // ============================================================

    /**
     * @brief Abilita o disabilita l'algoritmo.
     *
     * @param value true per abilitare l'autotuning.
     */
    void setEnabled(
        bool value)
    {
        state.enabled =
            value;
    }

    /**
     * @brief Indica se l'autotuning è abilitato.
     */
    bool isEnabled() const
    {
        return state.enabled;
    }

    // ============================================================
    // CALLBACK
    // ============================================================

    /**
     * @brief Registra la callback di completamento tuning.
     *
     * La callback viene invocata solamente quando sono disponibili
     * statistiche valide.
     */
    void setOnTune(
        TuneCallback callback)
    {
        onTune =
            callback;
    }

    // ============================================================
    // AGGIORNAMENTO
    // ============================================================

    /**
     * @brief Esegue l'autotuning usando le statistiche dei carichi.
     *
     * La funzione considera solamente:
     * - carichi abilitati;
     * - carichi che hanno completato almeno un ciclo;
     * - statistiche con durata media finita e positiva.
     *
     * Se nessun carico contiene statistiche valide, nessun parametro
     * viene modificato e le statistiche dei carichi non vengono
     * cancellate.
     *
     * Al termine di un'elaborazione valida vengono azzerati solamente
     * cyclesCount e avgCycleTimeSec. lastCycleTimestamp viene conservato.
     *
     * @param loads Vettore mutabile dei carichi.
     *
     * @return true se sono stati elaborati dati validi.
     */
    bool update(
        std::vector<LoadManager::Load>& loads)
    {
        return update(
            loads,
            millis());
    }

    /**
     * @brief Esegue l'autotuning con timestamp esplicito.
     *
     * Questa variante evita chiamate interne a millis() quando il
     * chiamante possiede già il timestamp corrente.
     *
     * @param loads Vettore mutabile dei carichi.
     * @param now Timestamp corrente.
     *
     * @return true se sono stati elaborati dati validi.
     */
    bool update(
        std::vector<LoadManager::Load>& loads,
        unsigned long now)
    {
        resetExecutionStatistics();

        statistics.totalLoads =
            loads.size();

        state.lastExecutionTimestamp =
            now;

        ++state.executionCount;

        if (!state.enabled)
        {
            return false;
        }

        // --------------------------------------------------------
        // 1. RACCOLTA STATISTICHE VALIDE
        // --------------------------------------------------------

        float totalCycles =
            0.0f;

        for (const auto& load : loads)
        {
            if (!load.runtime.enabled)
                continue;

            ++statistics.enabledLoads;

            if (!hasValidStatistics(load))
                continue;

            totalCycles +=
                static_cast<float>(
                    load.stats.cyclesCount);

            ++statistics.validLoads;
        }

        /*
         * Nessun dato utilizzabile.
         *
         * Non vengono modificati:
         * - isteresi;
         * - tempi minimi;
         * - statistiche dei carichi.
         */
        if (statistics.validLoads == 0U)
        {
            return false;
        }

        statistics.averageCycles =
            totalCycles /
            static_cast<float>(
                statistics.validLoads);

        statistics.dataAvailable =
            true;

        // --------------------------------------------------------
        // 2. REGOLAZIONE ISTERESI
        // --------------------------------------------------------

        tuneHysteresis();

        // --------------------------------------------------------
        // 3. REGOLAZIONE TEMPI DEI CARICHI
        // --------------------------------------------------------

        for (auto& load : loads)
        {
            if (!load.runtime.enabled)
                continue;

            if (!hasValidStatistics(load))
                continue;

            tuneLoadTiming(load);
        }

        // --------------------------------------------------------
        // 4. CONSUMO DELLA FINESTRA STATISTICA
        // --------------------------------------------------------

        for (auto& load : loads)
        {
            if (!load.runtime.enabled)
                continue;

            if (!hasValidStatistics(load))
                continue;

            resetLoadWindow(load);
        }

        // --------------------------------------------------------
        // 5. CALLBACK
        // --------------------------------------------------------

        if (onTune)
        {
            onTune(
                getSnapshot());
        }

        return true;
    }

    // ============================================================
    // ACCESSORI
    // ============================================================

    /**
     * @brief Restituisce l'isteresi corrente di attivazione.
     */
    float getHystOn() const
    {
        return
            state.hysteresisOn;
    }

    /**
     * @brief Restituisce l'isteresi corrente di distacco.
     */
    float getHystOff() const
    {
        return
            state.hysteresisOff;
    }

    /**
     * @brief Imposta direttamente le isteresi correnti.
     *
     * I valori vengono limitati al range configurato.
     */
    void setHysteresis(
        float hysteresisOn,
        float hysteresisOff)
    {
        if (isfinite(hysteresisOn))
        {
            state.hysteresisOn =
                clampFloat(
                    hysteresisOn,
                    config.hysteresis.minimum,
                    config.hysteresis.maximum);
        }

        if (isfinite(hysteresisOff))
        {
            state.hysteresisOff =
                clampFloat(
                    hysteresisOff,
                    config.hysteresis.minimum,
                    config.hysteresis.maximum);
        }
    }

    /**
     * @brief Restituisce lo stato runtime corrente.
     */
    const State& getState() const
    {
        return state;
    }

    /**
     * @brief Restituisce le statistiche dell'ultima esecuzione.
     */
    const Statistics& getStatistics() const
    {
        return statistics;
    }

    /**
     * @brief Restituisce uno snapshot completo.
     */
    Snapshot getSnapshot() const
    {
        Snapshot snapshot;

        snapshot.config =
            config;

        snapshot.state =
            state;

        snapshot.statistics =
            statistics;

        return snapshot;
    }

    // ============================================================
    // DIAGNOSTICA
    // ============================================================

    class Diagnostic
    {
    public:

        /**
         * @brief Stampa il report completo dell'AutoTuneManager.
         *
         * @param manager Manager da analizzare.
         */
        static void Report(
            const AutoTuneManager& manager)
        {
            const Snapshot snapshot =
                manager.getSnapshot();

            Serial.println(
                "\n===== AUTO TUNE MANAGER =====");

            Serial.print("Enabled: ");
            Serial.println(
                snapshot.state.enabled
                    ? "YES"
                    : "NO");

            Serial.print("Hysteresis ON: ");
            Serial.println(
                snapshot.state.hysteresisOn);

            Serial.print("Hysteresis OFF: ");
            Serial.println(
                snapshot.state.hysteresisOff);

            Serial.print("Step: ");
            Serial.print(
                snapshot.config.tuneStepPercent *
                100.0f);
            Serial.println(" %");

            Serial.print("Hysteresis range: ");
            Serial.print(
                snapshot.config.hysteresis.minimum);
            Serial.print(" - ");
            Serial.println(
                snapshot.config.hysteresis.maximum);

            Serial.print("Executions: ");
            Serial.println(
                snapshot.state.executionCount);

            Serial.print("Last execution: ");
            Serial.println(
                snapshot.state.lastExecutionTimestamp);

            Serial.print("Total loads: ");
            Serial.println(
                snapshot.statistics.totalLoads);

            Serial.print("Enabled loads: ");
            Serial.println(
                snapshot.statistics.enabledLoads);

            Serial.print("Valid loads: ");
            Serial.println(
                snapshot.statistics.validLoads);

            Serial.print("Data available: ");
            Serial.println(
                snapshot.statistics.dataAvailable
                    ? "YES"
                    : "NO");

            Serial.print("Average cycles: ");
            Serial.println(
                snapshot.statistics.averageCycles);

            Serial.print("Hysteresis increased: ");
            Serial.println(
                snapshot.statistics.hysteresisIncreased
                    ? "YES"
                    : "NO");

            Serial.print("Hysteresis decreased: ");
            Serial.println(
                snapshot.statistics.hysteresisDecreased
                    ? "YES"
                    : "NO");

            Serial.print("Timing increased loads: ");
            Serial.println(
                snapshot.statistics.increasedTimingLoads);

            Serial.print("Timing decreased loads: ");
            Serial.println(
                snapshot.statistics.decreasedTimingLoads);

            Serial.println(
                "===== END AUTO TUNE MANAGER =====\n");
        }
    };

private:

    // ============================================================
    // VALIDAZIONE CONFIGURAZIONE
    // ============================================================

    /**
     * @brief Normalizza la configurazione.
     */
    void normalizeConfig()
    {
        // --------------------------------------------------------
        // STEP
        // --------------------------------------------------------

        if (!isfinite(
                config.tuneStepPercent) ||
            config.tuneStepPercent <= 0.0f)
        {
            config.tuneStepPercent =
                0.05f;
        }

        if (config.tuneStepPercent > 1.0f)
        {
            config.tuneStepPercent =
                1.0f;
        }

        // --------------------------------------------------------
        // ISTERESI
        // --------------------------------------------------------

        if (!isfinite(
                config.hysteresis.minimum) ||
            config.hysteresis.minimum < 0.0f)
        {
            config.hysteresis.minimum =
                50.0f;
        }

        if (!isfinite(
                config.hysteresis.maximum) ||
            config.hysteresis.maximum <= 0.0f)
        {
            config.hysteresis.maximum =
                2000.0f;
        }

        if (config.hysteresis.minimum >
            config.hysteresis.maximum)
        {
            const float temporary =
                config.hysteresis.minimum;

            config.hysteresis.minimum =
                config.hysteresis.maximum;

            config.hysteresis.maximum =
                temporary;
        }

        // --------------------------------------------------------
        // TEMPI MINIMI ON
        // --------------------------------------------------------

        if (config.timing.minOnMinimumSec >
            config.timing.minOnMaximumSec)
        {
            const unsigned long temporary =
                config.timing.minOnMinimumSec;

            config.timing.minOnMinimumSec =
                config.timing.minOnMaximumSec;

            config.timing.minOnMaximumSec =
                temporary;
        }

        // --------------------------------------------------------
        // TEMPI MINIMI OFF
        // --------------------------------------------------------

        if (config.timing.minOffMinimumSec >
            config.timing.minOffMaximumSec)
        {
            const unsigned long temporary =
                config.timing.minOffMinimumSec;

            config.timing.minOffMinimumSec =
                config.timing.minOffMaximumSec;

            config.timing.minOffMaximumSec =
                temporary;
        }

        // --------------------------------------------------------
        // SOGLIE CICLI
        // --------------------------------------------------------

        if (!isfinite(
                config.cycles.increaseHysteresisAbove) ||
            config.cycles.increaseHysteresisAbove < 0.0f)
        {
            config.cycles.increaseHysteresisAbove =
                3.0f;
        }

        if (!isfinite(
                config.cycles.decreaseHysteresisBelow) ||
            config.cycles.decreaseHysteresisBelow < 0.0f)
        {
            config.cycles.decreaseHysteresisBelow =
                1.0f;
        }

        if (config.cycles.decreaseHysteresisBelow >
            config.cycles.increaseHysteresisAbove)
        {
            config.cycles.decreaseHysteresisBelow =
                config.cycles.increaseHysteresisAbove;
        }

        if (config.cycles.fastCycleMinimumCount == 0U)
        {
            config.cycles.fastCycleMinimumCount =
                1U;
        }

        if (!isfinite(
                config.cycles.fastCycleMaximumSec) ||
            config.cycles.fastCycleMaximumSec <= 0.0f)
        {
            config.cycles.fastCycleMaximumSec =
                120.0f;
        }

        if (!isfinite(
                config.cycles.longCycleMinimumSec) ||
            config.cycles.longCycleMinimumSec <=
                config.cycles.fastCycleMaximumSec)
        {
            config.cycles.longCycleMinimumSec =
                600.0f;
        }
    }

    /**
     * @brief Limita lo stato corrente al range configurato.
     */
    void normalizeState()
    {
        if (!isfinite(
                state.hysteresisOn))
        {
            state.hysteresisOn =
                200.0f;
        }

        if (!isfinite(
                state.hysteresisOff))
        {
            state.hysteresisOff =
                200.0f;
        }

        state.hysteresisOn =
            clampFloat(
                state.hysteresisOn,
                config.hysteresis.minimum,
                config.hysteresis.maximum);

        state.hysteresisOff =
            clampFloat(
                state.hysteresisOff,
                config.hysteresis.minimum,
                config.hysteresis.maximum);
    }

    // ============================================================
    // STATISTICHE
    // ============================================================

    /**
     * @brief Reinizializza le statistiche dell'esecuzione.
     */
    void resetExecutionStatistics()
    {
        statistics =
            Statistics{};
    }

    /**
     * @brief Verifica se un carico contiene statistiche utilizzabili.
     */
    static bool hasValidStatistics(
        const LoadManager::Load& load)
    {
        if (load.stats.cyclesCount == 0U)
            return false;

        if (!isfinite(
                load.stats.avgCycleTimeSec))
        {
            return false;
        }

        if (load.stats.avgCycleTimeSec <= 0.0f)
            return false;

        return true;
    }

    /**
     * @brief Azzera la finestra statistica consumata.
     *
     * lastCycleTimestamp non viene modificato perché rappresenta
     * l'ultimo evento reale di commutazione.
     */
    static void resetLoadWindow(
        LoadManager::Load& load)
    {
        load.stats.cyclesCount =
            0U;

        load.stats.avgCycleTimeSec =
            0.0f;
    }

    // ============================================================
    // REGOLAZIONE ISTERESI
    // ============================================================

    /**
     * @brief Regola le isteresi usando la media dei cicli.
     */
    void tuneHysteresis()
    {
        const float step =
            config.tuneStepPercent;

        if (statistics.averageCycles >
            config.cycles.increaseHysteresisAbove)
        {
            const float deltaOn =
                state.hysteresisOn *
                step;

            const float deltaOff =
                state.hysteresisOff *
                step;

            state.hysteresisOn =
                clampFloat(
                    state.hysteresisOn +
                        deltaOn,
                    config.hysteresis.minimum,
                    config.hysteresis.maximum);

            state.hysteresisOff =
                clampFloat(
                    state.hysteresisOff +
                        deltaOff,
                    config.hysteresis.minimum,
                    config.hysteresis.maximum);

            statistics.hysteresisIncreased =
                true;

            return;
        }

        if (statistics.averageCycles <
            config.cycles.decreaseHysteresisBelow)
        {
            const float deltaOn =
                state.hysteresisOn *
                step;

            const float deltaOff =
                state.hysteresisOff *
                step;

            state.hysteresisOn =
                clampFloat(
                    state.hysteresisOn -
                        deltaOn,
                    config.hysteresis.minimum,
                    config.hysteresis.maximum);

            state.hysteresisOff =
                clampFloat(
                    state.hysteresisOff -
                        deltaOff,
                    config.hysteresis.minimum,
                    config.hysteresis.maximum);

            statistics.hysteresisDecreased =
                true;
        }
    }

    // ============================================================
    // REGOLAZIONE TEMPI
    // ============================================================

    /**
     * @brief Regola i tempi minimi di un singolo carico.
     *
     * Cicli frequenti e brevi aumentano i tempi minimi.
     * Cicli validi e molto lunghi consentono una riduzione graduale.
     */
    void tuneLoadTiming(
        LoadManager::Load& load)
    {
        const bool fastCycling =
            load.stats.cyclesCount >=
                config.cycles.fastCycleMinimumCount &&
            load.stats.avgCycleTimeSec <
                config.cycles.fastCycleMaximumSec;

        const bool longCycling =
            load.stats.cyclesCount > 0U &&
            load.stats.avgCycleTimeSec >
                config.cycles.longCycleMinimumSec;

        if (fastCycling)
        {
            increaseLoadTiming(load);

            ++statistics.increasedTimingLoads;

            return;
        }

        if (longCycling)
        {
            decreaseLoadTiming(load);

            ++statistics.decreasedTimingLoads;
        }
    }

    /**
     * @brief Aumenta i tempi minimi del carico.
     */
    void increaseLoadTiming(
        LoadManager::Load& load)
    {
        const unsigned long minOnMaximumMs =
            secondsToMilliseconds(
                config.timing.minOnMaximumSec);

        const unsigned long minOffMaximumMs =
            secondsToMilliseconds(
                config.timing.minOffMaximumSec);

        load.timing.minOnMs =
            increaseUnsignedLong(
                load.timing.minOnMs,
                config.tuneStepPercent,
                minOnMaximumMs);

        load.timing.minOffMs =
            increaseUnsignedLong(
                load.timing.minOffMs,
                config.tuneStepPercent,
                minOffMaximumMs);
    }

    /**
     * @brief Riduce i tempi minimi del carico.
     */
    void decreaseLoadTiming(
        LoadManager::Load& load)
    {
        const unsigned long minOnMinimumMs =
            secondsToMilliseconds(
                config.timing.minOnMinimumSec);

        const unsigned long minOffMinimumMs =
            secondsToMilliseconds(
                config.timing.minOffMinimumSec);

        load.timing.minOnMs =
            decreaseUnsignedLong(
                load.timing.minOnMs,
                config.tuneStepPercent,
                minOnMinimumMs);

        load.timing.minOffMs =
            decreaseUnsignedLong(
                load.timing.minOffMs,
                config.tuneStepPercent,
                minOffMinimumMs);
    }

    // ============================================================
    // HELPER NUMERICI
    // ============================================================

    /**
     * @brief Limita un valore float all'intervallo specificato.
     */
    static float clampFloat(
        float value,
        float minimum,
        float maximum)
    {
        if (value < minimum)
            return minimum;

        if (value > maximum)
            return maximum;

        return value;
    }

    /**
     * @brief Converte secondi in millisecondi con saturazione.
     */
    static unsigned long secondsToMilliseconds(
        unsigned long seconds)
    {
        const unsigned long maximumSeconds =
            0xFFFFFFFFUL /
            1000UL;

        if (seconds >
            maximumSeconds)
        {
            return
                0xFFFFFFFFUL;
        }

        return
            seconds *
            1000UL;
    }

    /**
     * @brief Aumenta un valore unsigned long evitando overflow.
     */
    static unsigned long increaseUnsignedLong(
        unsigned long value,
        float percentage,
        unsigned long maximum)
    {
        if (value >= maximum)
            return maximum;

        const double factor =
            1.0 +
            static_cast<double>(
                percentage);

        const double calculated =
            static_cast<double>(value) *
            factor;

        if (calculated >=
            static_cast<double>(maximum))
        {
            return maximum;
        }

        if (calculated >=
            static_cast<double>(
                0xFFFFFFFFUL))
        {
            return maximum;
        }

        unsigned long result =
            static_cast<unsigned long>(
                calculated);

        /*
         * Garantisce almeno un millisecondo di variazione quando il
         * valore non ha ancora raggiunto il massimo.
         */
        if (result <= value)
        {
            result =
                value + 1UL;
        }

        if (result > maximum)
        {
            result =
                maximum;
        }

        return result;
    }

    /**
     * @brief Riduce un valore unsigned long rispettando il minimo.
     */
    static unsigned long decreaseUnsignedLong(
        unsigned long value,
        float percentage,
        unsigned long minimum)
    {
        if (value <= minimum)
            return minimum;

        const double factor =
            1.0 -
            static_cast<double>(
                percentage);

        const double calculated =
            static_cast<double>(value) *
            factor;

        if (calculated <=
            static_cast<double>(minimum))
        {
            return minimum;
        }

        unsigned long result =
            static_cast<unsigned long>(
                calculated);

        /*
         * Garantisce almeno un millisecondo di variazione quando il
         * valore non ha ancora raggiunto il minimo.
         */
        if (result >= value)
        {
            result =
                value - 1UL;
        }

        if (result < minimum)
        {
            result =
                minimum;
        }

        return result;
    }
};

class DiagnosticManager
{
public:

    // ============================================================
    // REPORT RIEPILOGO LOAD MANAGER
    // ============================================================

    /**
     * @brief Stampa lo snapshot aggregato del LoadManager.
     *
     * Il report include:
     * - numero totale di carichi;
     * - numero di carichi attivi;
     * - numero di carichi termici;
     * - numero di comandi pendenti;
     * - potenza nominale installata;
     * - potenza nominale attualmente attiva;
     * - isteresi corrente.
     *
     * @param manager LoadManager da analizzare.
     */
    static void ReportLoadManagerSummary(
        const LoadManager& manager)
    {
        const LoadManager::Snapshot snapshot =
            manager.getSnapshot();

        const LoadManager::Config& config =
            manager.getConfig();

        Serial.println(
            "\n===== LOAD MANAGER SUMMARY =====");

        Serial.print("Total electrical loads: ");
        Serial.println(
            snapshot.totalLoads);

        Serial.print("Active electrical loads: ");
        Serial.println(
            snapshot.activeLoads);

        Serial.print("Pending commands: ");
        Serial.println(
            snapshot.pendingCommands);

        Serial.print("Total nominal power: ");
        Serial.print(
            snapshot.totalNominalPower);
        Serial.println(" W");

        Serial.print("Active nominal power: ");
        Serial.print(
            snapshot.activeNominalPower);
        Serial.println(" W");

        Serial.print("Hysteresis ON: ");
        Serial.print(
            snapshot.hysteresisOn);
        Serial.println(" W");

        Serial.print("Hysteresis OFF: ");
        Serial.print(
            snapshot.hysteresisOff);
        Serial.println(" W");

        Serial.print("Forecast peak: ");
        Serial.print(
            config.forecastPeak);
        Serial.println(" W");

        Serial.print("Command timeout: ");
        Serial.print(
            config.commandTimeoutMs);
        Serial.println(" ms");

        Serial.print("Retry backoff base: ");
        Serial.print(
            config.commandRetryBackoffMs);
        Serial.println(" ms");

        Serial.print("Maximum retries: ");

        if (config.commandMaxRetries == 0U)
        {
            Serial.println("UNLIMITED");
        }
        else
        {
            Serial.println(
                config.commandMaxRetries);
        }

        Serial.println(
            "===== END LOAD MANAGER SUMMARY =====");
    }

    // ============================================================
    // REPORT CARICHI ELETTRICI
    // ============================================================

    /**
     * @brief Stampa il report completo dei carichi elettrici.
     *
     * Per ogni carico vengono mostrati:
     * - identità e configurazione;
     * - stato runtime;
     * - tempi minimi;
     * - comando pendente;
     * - timeout e retry;
     * - statistiche di commutazione.
     *
     * @param manager LoadManager da analizzare.
     */
    static void ReportLoads(
        const LoadManager& manager)
    {
        Serial.println(
            "\n===== ELECTRICAL LOADS =====");

        const auto& loads =
            manager.getLoads();

        if (loads.empty())
        {
            Serial.println(
                "No electrical loads defined.");

            Serial.println(
                "===== END ELECTRICAL LOADS =====");

            return;
        }

        for (size_t index = 0;
             index < loads.size();
             ++index)
        {
            const auto& load =
                loads[index];

            Serial.println(
                "------------------------------------");

            Serial.print("Load #");
            Serial.println(index);

            // ----------------------------------------------------
            // IDENTITA' E CONFIGURAZIONE
            // ----------------------------------------------------

            Serial.print("Name: ");
            Serial.println(
                load.identity.name);

            Serial.print("Nominal power: ");
            Serial.print(
                load.config.nominalPower);
            Serial.println(" W");

            Serial.print("Priority: ");
            Serial.println(
                load.config.priority);

            // ----------------------------------------------------
            // RUNTIME
            // ----------------------------------------------------

            Serial.print("Enabled: ");
            printBoolean(
                load.runtime.enabled);

            Serial.print("State: ");
            printOnOff(
                load.runtime.state);

            Serial.print("Last state change: ");
            Serial.print(
                load.runtime.lastChange);
            Serial.println(" ms");

            // ----------------------------------------------------
            // TIMING
            // ----------------------------------------------------

            Serial.print("Minimum ON time: ");
            Serial.print(
                load.timing.minOnMs);
            Serial.println(" ms");

            Serial.print("Minimum OFF time: ");
            Serial.print(
                load.timing.minOffMs);
            Serial.println(" ms");

            // ----------------------------------------------------
            // COMANDO
            // ----------------------------------------------------

            ReportCommand(
                load.command);

            // ----------------------------------------------------
            // STATISTICHE
            // ----------------------------------------------------

            Serial.print("Cycles in current window: ");
            Serial.println(
                load.stats.cyclesCount);

            Serial.print("Average cycle time: ");
            Serial.print(
                load.stats.avgCycleTimeSec);
            Serial.println(" s");

            Serial.print("Last cycle timestamp: ");
            Serial.print(
                load.stats.lastCycleTimestamp);
            Serial.println(" ms");
        }

        Serial.println(
            "===== END ELECTRICAL LOADS =====");
    }

    // ============================================================
    // REPORT CARICHI TERMICI
    // ============================================================

   
    // ============================================================
    // REPORT LIMIT MANAGER
    // ============================================================

    /**
     * @brief Stampa la diagnostica del LimitManager.
     *
     * @param manager LimitManager da analizzare.
     */
    static void ReportLimits(
        const LimitManager& manager)
    {
        const LimitManager::Snapshot snapshot =
            manager.getSnapshot();

        Serial.println(
            "\n===== LIMIT MANAGER =====");

        Serial.print("Grid limit: ");
        Serial.print(
            snapshot.config.gridLimit);
        Serial.println(" W");

        Serial.print("Hard limit: ");

        if (snapshot.config.hardLimit > 0.0f)
        {
            Serial.print(
                snapshot.config.hardLimit);
            Serial.println(" W");
        }
        else
        {
            Serial.println("DISABLED");
        }

        Serial.print("Warning threshold: ");
        Serial.print(
            snapshot.config.warningPct *
            100.0f);
        Serial.println(" %");

        Serial.print("Warning reset hysteresis: ");
        Serial.print(
            snapshot.config
                .warningResetHysteresisWatt);
        Serial.println(" W");

        Serial.print("Exceeded reset hysteresis: ");
        Serial.print(
            snapshot.config
                .exceededResetHysteresisWatt);
        Serial.println(" W");

        Serial.print("Hard-limit reset hysteresis: ");
        Serial.print(
            snapshot.config
                .hardResetHysteresisWatt);
        Serial.println(" W");

        Serial.print("Current state: ");
        printLimitState(
            snapshot.runtime.state);

        Serial.print("Last net power: ");
        Serial.print(
            snapshot.runtime.lastNetPower);
        Serial.println(" W");

        Serial.print("Last dynamic limit: ");
        Serial.print(
            snapshot.runtime.lastDynamicLimit);
        Serial.println(" W");

        Serial.print("Last check: ");
        Serial.print(
            snapshot.runtime.lastCheck);
        Serial.println(" ms");

        Serial.print("Last transition: ");
        Serial.print(
            snapshot.runtime.lastTransition);
        Serial.println(" ms");

        Serial.print("Transition count: ");
        Serial.println(
            snapshot.runtime.transitionCount);

        Serial.println(
            "===== END LIMIT MANAGER =====");
    }

    // ============================================================
    // REPORT AUTOTUNE
    // ============================================================

    /**
     * @brief Stampa la diagnostica completa dell'AutoTuneManager.
     *
     * @param manager AutoTuneManager da analizzare.
     */
    static void ReportAutoTune(
        const AutoTuneManager& manager)
    {
        const AutoTuneManager::Snapshot snapshot =
            manager.getSnapshot();

        Serial.println(
            "\n===== AUTO TUNE MANAGER =====");

        Serial.print("Enabled: ");
        printBoolean(
            snapshot.state.enabled);

        Serial.print("Hysteresis ON: ");
        Serial.print(
            snapshot.state.hysteresisOn);
        Serial.println(" W");

        Serial.print("Hysteresis OFF: ");
        Serial.print(
            snapshot.state.hysteresisOff);
        Serial.println(" W");

        Serial.print("Tune step: ");
        Serial.print(
            snapshot.config.tuneStepPercent *
            100.0f);
        Serial.println(" %");

        Serial.print("Hysteresis range: ");
        Serial.print(
            snapshot.config.hysteresis.minimum);

        Serial.print(" - ");

        Serial.print(
            snapshot.config.hysteresis.maximum);
        Serial.println(" W");

        Serial.print("Minimum ON range: ");
        Serial.print(
            snapshot.config.timing
                .minOnMinimumSec);

        Serial.print(" - ");

        Serial.print(
            snapshot.config.timing
                .minOnMaximumSec);
        Serial.println(" s");

        Serial.print("Minimum OFF range: ");
        Serial.print(
            snapshot.config.timing
                .minOffMinimumSec);

        Serial.print(" - ");

        Serial.print(
            snapshot.config.timing
                .minOffMaximumSec);
        Serial.println(" s");

        Serial.print("Executions: ");
        Serial.println(
            snapshot.state.executionCount);

        Serial.print("Last execution: ");
        Serial.print(
            snapshot.state.lastExecutionTimestamp);
        Serial.println(" ms");

        Serial.print("Total loads: ");
        Serial.println(
            snapshot.statistics.totalLoads);

        Serial.print("Enabled loads: ");
        Serial.println(
            snapshot.statistics.enabledLoads);

        Serial.print("Loads with valid statistics: ");
        Serial.println(
            snapshot.statistics.validLoads);

        Serial.print("Data available: ");
        printBoolean(
            snapshot.statistics.dataAvailable);

        Serial.print("Average cycles: ");
        Serial.println(
            snapshot.statistics.averageCycles);

        Serial.print("Hysteresis increased: ");
        printBoolean(
            snapshot.statistics
                .hysteresisIncreased);

        Serial.print("Hysteresis decreased: ");
        printBoolean(
            snapshot.statistics
                .hysteresisDecreased);

        Serial.print("Loads with timing increased: ");
        Serial.println(
            snapshot.statistics
                .increasedTimingLoads);

        Serial.print("Loads with timing decreased: ");
        Serial.println(
            snapshot.statistics
                .decreasedTimingLoads);

        Serial.println(
            "===== END AUTO TUNE MANAGER =====");
    }

    // ============================================================
    // REPORT COMPLETO COMPATIBILE
    // ============================================================

    /**
     * @brief Stampa il report del solo LoadManager.
     *
     * Mantiene compatibilità con la chiamata esistente:
     *
     * DiagnosticManager::FullReport(loadManager);
     *
     * @param loadManager LoadManager da analizzare.
     */
    static void FullReport(
        const LoadManager& loadManager)
    {
        Serial.println(
            "\n========================================");

        Serial.println(
            "          POWER SYSTEM DIAGNOSTIC");

        Serial.println(
            "========================================");

        ReportLoadManagerSummary(
            loadManager);

        ReportLoads(
            loadManager);

        Serial.println(
            "\n========================================");

        Serial.println(
            "        END POWER SYSTEM DIAGNOSTIC");

        Serial.println(
            "========================================\n");
    }

    // ============================================================
    // REPORT COMPLETO ESTESO
    // ============================================================

    /**
     * @brief Stampa il report diagnostico completo del sottosistema.
     *
     * Include:
     * - LoadManager;
     * - carichi elettrici;
     * - carichi termici;
     * - LimitManager;
     * - AutoTuneManager.
     *
     * @param loadManager LoadManager da analizzare.
     * @param limitManager LimitManager da analizzare.
     * @param autoTuneManager AutoTuneManager da analizzare.
     */
    static void FullReport(
        const LoadManager& loadManager,
        const LimitManager& limitManager,
        const AutoTuneManager& autoTuneManager)
    {
        Serial.println(
            "\n========================================");

        Serial.println(
            "       FULL POWER SYSTEM DIAGNOSTIC");

        Serial.println(
            "========================================");

        ReportLoadManagerSummary(
            loadManager);

        ReportLoads(
            loadManager);

        ReportLimits(
            limitManager);

        ReportAutoTune(
            autoTuneManager);

        Serial.println(
            "\n========================================");

        Serial.println(
            "     END FULL POWER SYSTEM DIAGNOSTIC");

        Serial.println(
            "========================================\n");
    }

private:

    // ============================================================
    // REPORT COMANDO
    // ============================================================

    /**
     * @brief Stampa lo stato completo di un comando.
     *
     * @param command Comando da analizzare.
     */
    static void ReportCommand(
        const LoadManager::Command& command)
    {
        Serial.println("Command:");

        Serial.print("  Pending: ");
        printBoolean(
            command.pending);

        Serial.print("  Requested state: ");

        if (command.pending)
        {
            printOnOff(
                command.requestedState);
        }
        else
        {
            Serial.println("NONE");
        }

        Serial.print("  Request time: ");
        Serial.print(
            command.requestTime);
        Serial.println(" ms");

        Serial.print("  Last timeout: ");
        Serial.print(
            command.lastTimeout);
        Serial.println(" ms");

        Serial.print("  Retry allowed at: ");
        Serial.print(
            command.retryAllowedAt);
        Serial.println(" ms");

        Serial.print("  Retry count: ");
        Serial.println(
            command.retryCount);

        Serial.print("  Retry blocked: ");
        printBoolean(
            command.retryBlocked);
    }

    // ============================================================
    // HELPER DI STAMPA
    // ============================================================

    /**
     * @brief Stampa YES oppure NO.
     */
    static void printBoolean(
        bool value)
    {
        Serial.println(
            value
                ? "YES"
                : "NO");
    }

    /**
     * @brief Stampa ON oppure OFF.
     */
    static void printOnOff(
        bool state)
    {
        Serial.println(
            state
                ? "ON"
                : "OFF");
    }

    /**
     * @brief Stampa lo stato del LimitManager.
     */
    static void printLimitState(
        LimitManager::State state)
    {
        switch (state)
        {
            case LimitManager::State::NORMAL:

                Serial.println("NORMAL");
                break;

            case LimitManager::State::WARNING:

                Serial.println("WARNING");
                break;

            case LimitManager::State::EXCEEDED:

                Serial.println("EXCEEDED");
                break;

            case LimitManager::State::HARD_EXCEEDED:

                Serial.println(
                    "HARD_EXCEEDED");
                break;

            default:

                Serial.println("UNKNOWN");
                break;
        }
    }
};

class PowerManager
{
public:

    // ============================================================
    // CONFIG
    // ============================================================

    struct Config
    {
        bool autoTune = false;

        unsigned long tuneIntervalMs =
            60000;
    };

    // ============================================================
    // MODALITA'
    // ============================================================

    enum class OptimizationMode
    {
        MASSIMO_AUTOCONSUMO,
        RISPARMIO_ECONOMICO,
        MASSIMO_COMFORT,
        PROTEZIONE_RETE,
        BILANCIATO
    };

private:

    // ============================================================
    // CALLBACK
    // ============================================================

    LoadManager::LoadCb
        onLoadChange = nullptr;

    LimitManager::LimitCb
        onLimitWarning = nullptr;

    LimitManager::LimitCb
        onLimitExceeded = nullptr;

    std::function<void(
        int,
        const String&)>
        onError = nullptr;

    LoadManager::SuggestionCb
        onSuggestion = nullptr;

    // ============================================================
    // MANAGER
    // ============================================================

    LoadManager loads;

    LimitManager limits;

    AutoTuneManager autotune;

    // ============================================================
    // CONFIG
    // ============================================================

    Config cfg;

    // ============================================================
    // RUNTIME
    // ============================================================

    struct Runtime
    {
        bool initialized = false;

        unsigned long lastTuneMs = 0;

        float lastDynamicLimit = 0.0f;
    };

    Runtime runtime;

    // ============================================================
    // STATO
    // ============================================================

    float gridPower = 0.0f;

    OptimizationMode mode =
        OptimizationMode::BILANCIATO;

public:

    // ============================================================
    // COSTRUTTORE
    // ============================================================

    explicit PowerManager(
        float gridLimitWatt)
    {
        limits.setLimit(
            gridLimitWatt);
    }

    // ============================================================
    // ERRORI
    // ============================================================

private:

    void reportError(
        int code,
        const String& message)
    {
        if (onError)
        {
            onError(
                code,
                message);
        }
    }

public:

    // ============================================================
    // ACCESSO AI MANAGER
    // ============================================================

    LoadManager&
    loadManager()
    {
        return loads;
    }

    const LoadManager&
    loadManager() const
    {
        return loads;
    }

    LimitManager&
    limitManager()
    {
        return limits;
    }

    const LimitManager&
    limitManager() const
    {
        return limits;
    }

    AutoTuneManager&
    autoTuneManager()
    {
        return autotune;
    }

    const AutoTuneManager&
    autoTuneManager() const
    {
        return autotune;
    }

    // ============================================================
    // CONFIGURAZIONE
    // ============================================================

    bool setGridPower(
        float watt)
    {
        if (!isfinite(watt))
        {
            reportError(
                2001,
                "GridPower non valida");

            return false;
        }

        gridPower = watt;

        return true;
    }

    float getGridPower() const
    {
        return gridPower;
    }

    void setConfig(
        const Config& value)
    {
        cfg = value;

        if (cfg.tuneIntervalMs == 0)
        {
            cfg.autoTune = false;
        }

        runtime.lastTuneMs = 0;
        runtime.initialized = false;
    }

    const Config&
    getConfig() const
    {
        return cfg;
    }

    // ============================================================
    // HARD LIMIT
    // ============================================================

    void setHardLimit(
        float watt)
    {
        limits.setHardLimit(
            watt);
    }

    float getHardLimit() const
    {
        return
            limits.getHardLimit();
    }

    // ============================================================
    // MODALITA'
    // ============================================================

    void setOptimizationMode(
        OptimizationMode value)
    {
        mode = value;
    }

    OptimizationMode
    getOptimizationMode() const
    {
        return mode;
    }

    // ============================================================
    // CALLBACK API
    // ============================================================

    void setOnLoadChange(
        LoadManager::LoadCb cb)
    {
        onLoadChange = cb;

        loads.setCallbacks(
            onLoadChange,
            onSuggestion);
    }

    void setOnLimitWarning(
        LimitManager::LimitCb cb)
    {
        onLimitWarning = cb;

        limits.setCallbacks(
            onLimitWarning,
            onLimitExceeded);
    }

    void setOnLimitExceeded(
        LimitManager::LimitCb cb)
    {
        onLimitExceeded = cb;

        limits.setCallbacks(
            onLimitWarning,
            onLimitExceeded);
    }

    void setOnError(
        std::function<void(
            int,
            const String&)> cb)
    {
        onError = cb;
    }

    void setOnSuggestion(
        LoadManager::SuggestionCb cb)
    {
        onSuggestion = cb;

        loads.setCallbacks(
            onLoadChange,
            onSuggestion);
    }

    void fireSuggestion(
        const String& name,
        int severity,
        const String& reason)
    {
        if (onSuggestion)
        {
            onSuggestion(
                name,
                severity,
                reason);
        }
    }

    /**
    * @brief Aggiorna il sistema di gestione della potenza.
    *
    * La funzione:
    * - valida la potenza di rete;
    * - calcola il limite dinamico in base alla modalità operativa;
    * - limita il valore operativo rispetto all'hard limit;
    * - aggiorna la macchina a stati dei limiti;
    * - esegue l'autotuning alla scadenza dell'intervallo;
    * - applica le nuove isteresi solo se il tuning ha elaborato dati validi;
    * - aggiorna i carichi termici;
    * - aggiorna i carichi elettrici.
    *
    * @param month Mese corrente, riservato a strategie future.
    * @param hour Ora corrente, riservata a strategie future.
    * @param minute Minuto corrente, riservato a strategie future.
    * @param now Timestamp corrente ottenuto tramite millis().
    */
    void update(
        int month,
        int hour,
        int minute,
        unsigned long now)
    {
        // Parametri attualmente non utilizzati.
        (void)month;
        (void)hour;
        (void)minute;

        // ============================================================
        // 1. VALIDAZIONE POTENZA DI RETE
        // ============================================================

        if (!isfinite(gridPower))
        {
            reportError(
                2002,
                "GridPower non finita");

            return;
        }

        /*
        * La potenza di scambio con la rete incorpora già eventuali
        * contributi di produzione e accumulo.
        *
        * Convenzione:
        * - valore positivo: prelievo dalla rete;
        * - valore negativo: immissione in rete.
        */
        const float netWithBattery =
            gridPower;

        // ============================================================
        // 2. CALCOLO LIMITE DINAMICO
        // ============================================================

        float dynamicLimit =
            applyOptimizationMode(
                limits.getLimit());

        if (!isfinite(dynamicLimit) ||
            dynamicLimit <= 0.0f)
        {
            reportError(
                2003,
                "DynamicLimit non valida");

            return;
        }

        // ============================================================
        // 3. APPLICAZIONE HARD LIMIT
        // ============================================================

        const float hardLimit =
            limits.getHardLimit();

        if (isfinite(hardLimit) &&
            hardLimit > 0.0f &&
            dynamicLimit > hardLimit)
        {
            dynamicLimit =
                hardLimit;
        }

        runtime.lastDynamicLimit =
            dynamicLimit;

        // ============================================================
        // 4. CONTROLLO LIMITI
        // ============================================================

        limits.check(
            netWithBattery,
            dynamicLimit);

        // ============================================================
        // 5. AUTOTUNING TEMPORIZZATO
        // ============================================================

        if (cfg.autoTune &&
            cfg.tuneIntervalMs > 0UL)
        {
            /*
            * Alla prima esecuzione viene inizializzata esclusivamente
            * la finestra temporale.
            *
            * Il tuning non viene eseguito immediatamente perché non
            * sono ancora disponibili statistiche significative.
            */
            if (!runtime.initialized)
            {
                runtime.lastTuneMs =
                    now;

                runtime.initialized =
                    true;
            }
            else
            {
                const bool intervalExpired =
                    static_cast<unsigned long>(
                        now -
                        runtime.lastTuneMs) >=
                    cfg.tuneIntervalMs;

                if (intervalExpired)
                {
                    const bool tuningApplied =
                        autotune.update(
                            loads.getLoadsMutable(),
                            now);

                    /*
                    * Le isteresi vengono aggiornate solamente quando
                    * AutoTuneManager ha trovato ed elaborato dati validi.
                    */
                    if (tuningApplied)
                    {
                        loads.setHysteresis(
                            autotune.getHystOn(),
                            autotune.getHystOff());
                    }

                    /*
                    * La finestra temporale viene comunque riavviata.
                    *
                    * Se non erano disponibili statistiche, i dati dei
                    * carichi non vengono cancellati da AutoTuneManager
                    * e potranno essere usati alla finestra successiva.
                    */
                    runtime.lastTuneMs =
                        now;
                }
            }
        }
        else
        {
            /*
            * Quando l'autotuning è disabilitato, il runtime viene
            * predisposto per iniziare una nuova finestra temporale
            * qualora venga successivamente riabilitato.
            *
            * Le isteresi configurate direttamente nel LoadManager
            * non vengono sovrascritte.
            */
            runtime.initialized =
                false;

            runtime.lastTuneMs =
                now;
        }


        // ============================================================
        // 7. CONTROLLO CARICHI ELETTRICI
        // ============================================================

        loads.updateLoads(
            netWithBattery,
            dynamicLimit,
            now);
    }

    // ============================================================
    // DIAGNOSTICA
    // ============================================================

    class Diagnostic
    {
    public:

        /**
        * @brief Stampa il report completo del PowerManager.
        *
        * Include carichi, limiti, pending, timeout, retry e autotuning.
        */
        static void FullReport(
            const PowerManager& manager)
        {
            DiagnosticManager::FullReport(
                manager.loadManager(),
                manager.limitManager(),
                manager.autoTuneManager());
        }
    };

private:

    // ============================================================
    // ALGORITMO OTTIMIZZAZIONE
    // ============================================================

    float applyOptimizationMode(
        float limit)
    {
        float out = limit;

        switch (mode)
        {
            case OptimizationMode::MASSIMO_AUTOCONSUMO:

                out *= 1.2f;
                break;

            case OptimizationMode::RISPARMIO_ECONOMICO:

                out *= 0.8f;
                break;

            case OptimizationMode::MASSIMO_COMFORT:

                out += 500.0f;
                break;

            case OptimizationMode::PROTEZIONE_RETE:

                out *= 0.6f;
                break;

            case OptimizationMode::BILANCIATO:
            default:
                break;
        }

        return out;
    }
};

#endif
