
#ifndef DMFrontendEngines_HPP
#define DMFrontendEngines_HPP

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

#include "DMDeclares.h"
#include "DMMQTT.hpp"
#include "DMFrontendOrchestrators.hpp"
#include "DMIntrospection.hpp"
#include "DMIntrusion.hpp"
#include "DMPlatformNetwork.hpp"
#include "DMPlatformModbus.hpp"
#include "DMOnline.hpp"

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


// ============================================================================
// FRONTEND STATUS
//
// Rappresenta lo stato tecnico comune di un frontend.
//
// La classe:
//   - non dipende da DomoManager;
//   - non conosce aree buffer;
//   - non conosce il protocollo specifico;
//   - non esegue pubblicazioni;
//   - non usa allocazione dinamica;
//   - non usa funzioni virtuali;
//   - occupa pochi byte;
//   - segnala quando la status word cambia.
//
// Ogni Engine mantiene una o più istanze FrontendStatus e traduce
// autonomamente il proprio stato runtime nei flag comuni.
//
// Layout status word:
//
//   bit 0 = Enabled
//   bit 1 = Running
//   bit 2 = Initialized
//   bit 3 = Connected
//   bit 4 = Active
//   bit 5 = Recovering
//   bit 6 = Degraded
//   bit 7 = Fault
//
//   bit 8..15 = Reserved
//
// I bit riservati non devono essere utilizzati dai singoli frontend
// senza estendere formalmente il contratto comune.
// ============================================================================

class FrontendStatus
{
public:

    // ========================================================================
    // TYPES
    // ========================================================================

    using Word =
        uint16_t;

    // ========================================================================
    // COMMON FLAGS
    //
    // bit 0..7  = stato comune dei frontend
    // bit 8..15 = riservati
    // ========================================================================

    enum class Flag : uint8_t
    {
        Enabled     = 0,
        Running     = 1,
        Initialized = 2,
        Connected   = 3,
        Active      = 4,
        Recovering  = 5,
        Degraded    = 6,
        Fault       = 7
    };

    // ========================================================================
    // MASKS
    //
    // Utilizzabili anche senza costruire un'istanza FrontendStatus.
    // ========================================================================

    static constexpr Word
        ENABLED_MASK =
            static_cast<Word>(
                1U << 0
            );

    static constexpr Word
        RUNNING_MASK =
            static_cast<Word>(
                1U << 1
            );

    static constexpr Word
        INITIALIZED_MASK =
            static_cast<Word>(
                1U << 2
            );

    static constexpr Word
        CONNECTED_MASK =
            static_cast<Word>(
                1U << 3
            );

    static constexpr Word
        ACTIVE_MASK =
            static_cast<Word>(
                1U << 4
            );

    static constexpr Word
        RECOVERING_MASK =
            static_cast<Word>(
                1U << 5
            );

    static constexpr Word
        DEGRADED_MASK =
            static_cast<Word>(
                1U << 6
            );

    static constexpr Word
        FAULT_MASK =
            static_cast<Word>(
                1U << 7
            );

    static constexpr Word
        COMMON_MASK =
            0x00FFU;

    static constexpr Word
        RESERVED_MASK =
            0xFF00U;

private:

    // ========================================================================
    // CURRENT STATUS WORD
    //
    // Non utilizzare il nome "word":
    //
    // Arduino.h definisce:
    //
    //   #define word(...) makeWord(__VA_ARGS__)
    //
    // Il nome statusWord evita il conflitto con la macro Arduino.
    // ========================================================================

    Word
        statusWord;

    // ========================================================================
    // LAST CONSUMED STATUS WORD
    //
    // Contiene l'ultima word consegnata al consumer tramite
    // ConsumeChanged().
    // ========================================================================

    Word
        consumedStatusWord;

    // ========================================================================
    // CHANGE STATE
    //
    // Indica se almeno una versione della word è già stata consumata.
    // ========================================================================

    bool
        consumed;

public:

    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    constexpr FrontendStatus()
        : statusWord(0),
          consumedStatusWord(0),
          consumed(false)
    {
    }

    // ========================================================================
    // MASK
    // ========================================================================

    static constexpr Word Mask(
        Flag flag)
    {
        return static_cast<Word>(
            1U <<
            static_cast<uint8_t>(
                flag
            )
        );
    }

    // ========================================================================
    // SANITIZE
    //
    // Conserva esclusivamente i bit comuni 0..7.
    //
    // I bit riservati 8..15 vengono sempre azzerati dalle operazioni
    // SetWord(), Reset(initialWord) e SetMasked().
    // ========================================================================

    static constexpr Word Sanitize(
        Word value)
    {
        return static_cast<Word>(
            value &
            COMMON_MASK
        );
    }

    // ========================================================================
    // RESET
    //
    // Azzera tutti i flag e forza la successiva ConsumeChanged() a
    // restituire la word corrente.
    // ========================================================================

    void Reset()
    {
        statusWord =
            0;

        consumedStatusWord =
            0;

        consumed =
            false;
    }

    // ========================================================================
    // RESET WITH INITIAL WORD
    //
    // Imposta una word iniziale, eliminando eventuali bit riservati.
    //
    // La successiva ConsumeChanged() restituisce sempre la word.
    // ========================================================================

    void Reset(
        Word initialWord)
    {
        statusWord =
            Sanitize(
                initialWord
            );

        consumedStatusWord =
            0;

        consumed =
            false;
    }

    // ========================================================================
    // SET FLAG
    //
    // Imposta o azzera un singolo flag.
    //
    // Restituisce:
    //   true  = status word modificata;
    //   false = nessuna modifica.
    // ========================================================================

    bool Set(
        Flag flag,
        bool value)
    {
        const Word mask =
            Mask(
                flag
            );

        const Word previousStatusWord =
            statusWord;

        if (value)
        {
            statusWord =
                static_cast<Word>(
                    statusWord |
                    mask
                );
        }
        else
        {
            statusWord =
                static_cast<Word>(
                    statusWord &
                    static_cast<Word>(
                        ~mask
                    )
                );
        }

        return
            statusWord !=
            previousStatusWord;
    }

    // ========================================================================
    // SET MULTIPLE FLAGS
    //
    // Aggiorna atomisticamente più flag.
    //
    // mask:
    //   identifica i bit da modificare.
    //
    // values:
    //   contiene i nuovi valori dei bit selezionati.
    //
    // Esempio:
    //
    //   SetMasked(
    //       RUNNING_MASK |
    //       INITIALIZED_MASK,
    //       RUNNING_MASK
    //   );
    //
    // Risultato:
    //   Running     = true
    //   Initialized = false
    //
    // I bit riservati vengono ignorati.
    // ========================================================================

    bool SetMasked(
        Word mask,
        Word values)
    {
        mask =
            Sanitize(
                mask
            );

        values =
            Sanitize(
                values
            );

        const Word previousStatusWord =
            statusWord;

        statusWord =
            static_cast<Word>(
                (
                    statusWord &
                    static_cast<Word>(
                        ~mask
                    )
                ) |
                (
                    values &
                    mask
                )
            );

        return
            statusWord !=
            previousStatusWord;
    }

    // ========================================================================
    // SET ENTIRE WORD
    //
    // Sostituisce integralmente lo stato comune.
    //
    // I bit riservati vengono sempre azzerati.
    // ========================================================================

    bool SetWord(
        Word value)
    {
        const Word sanitizedValue =
            Sanitize(
                value
            );

        if (
            statusWord ==
            sanitizedValue)
        {
            return false;
        }

        statusWord =
            sanitizedValue;

        return true;
    }

    // ========================================================================
    // CLEAR FLAG
    // ========================================================================

    bool Clear(
        Flag flag)
    {
        return Set(
            flag,
            false
        );
    }

    // ========================================================================
    // CLEAR ALL FLAGS
    //
    // Diversamente da Reset(), conserva lo stato di consumo.
    //
    // Se la word precedente era diversa da zero, HasChanged() rileverà
    // normalmente la variazione.
    // ========================================================================

    bool ClearAll()
    {
        return SetWord(
            0
        );
    }

    // ========================================================================
    // FLAG ACCESS
    // ========================================================================

    bool IsSet(
        Flag flag) const
    {
        return
            (
                statusWord &
                Mask(flag)
            ) != 0;
    }

    // ========================================================================
    // MASK ACCESS
    //
    // Restituisce true quando tutti i bit specificati nella mask
    // risultano impostati.
    // ========================================================================

    bool HasAll(
        Word mask) const
    {
        mask =
            Sanitize(
                mask
            );

        return
            (
                statusWord &
                mask
            ) ==
            mask;
    }

    // ========================================================================
    // ANY MASK ACCESS
    //
    // Restituisce true quando almeno uno dei bit specificati nella mask
    // risulta impostato.
    // ========================================================================

    bool HasAny(
        Word mask) const
    {
        mask =
            Sanitize(
                mask
            );

        return
            (
                statusWord &
                mask
            ) != 0;
    }

    // ========================================================================
    // WORD ACCESS
    // ========================================================================

    Word GetWord() const
    {
        return
            statusWord;
    }

    // ========================================================================
    // COMMON ACCESSORS
    // ========================================================================

    bool IsEnabled() const
    {
        return IsSet(
            Flag::Enabled
        );
    }

    bool IsRunning() const
    {
        return IsSet(
            Flag::Running
        );
    }

    bool IsInitialized() const
    {
        return IsSet(
            Flag::Initialized
        );
    }

    bool IsConnected() const
    {
        return IsSet(
            Flag::Connected
        );
    }

    bool IsActive() const
    {
        return IsSet(
            Flag::Active
        );
    }

    bool IsRecovering() const
    {
        return IsSet(
            Flag::Recovering
        );
    }

    bool IsDegraded() const
    {
        return IsSet(
            Flag::Degraded
        );
    }

    bool HasFault() const
    {
        return IsSet(
            Flag::Fault
        );
    }

    // ========================================================================
    // OPERATIONAL STATE
    //
    // Definizione comune minima:
    //
    //   Enabled
    //   Running
    //   Initialized
    //   Active
    //   not Fault
    //
    // Connected non è obbligatorio perché alcuni frontend, per esempio
    // un server HTTP, possono essere operativi senza client connessi.
    // ========================================================================

    bool IsOperational() const
    {
        return
            IsEnabled() &&
            IsRunning() &&
            IsInitialized() &&
            IsActive() &&
            !HasFault();
    }

    // ========================================================================
    // HEALTH STATE
    //
    // Indica che il frontend non è degradato e non è in fault.
    //
    // Non richiede che il frontend sia abilitato o in esecuzione.
    // ========================================================================

    bool IsHealthy() const
    {
        return
            !IsDegraded() &&
            !HasFault();
    }

    // ========================================================================
    // CHANGE DETECTION
    //
    // Restituisce true quando:
    //
    //   - la word non è mai stata consumata;
    //   - la word corrente è diversa dall'ultima word consumata.
    // ========================================================================

    bool HasChanged() const
    {
        return
            !consumed ||
            statusWord !=
                consumedStatusWord;
    }

    // ========================================================================
    // CONSUME CHANGED WORD
    //
    // Se lo stato è cambiato:
    //
    //   - copia la word corrente in output;
    //   - aggiorna consumedStatusWord;
    //   - marca la word come consumata;
    //   - restituisce true.
    //
    // Se lo stato non è cambiato:
    //
    //   - non modifica output;
    //   - restituisce false.
    //
    // Consente agli Engine di pubblicare la status word nel Buffer solo
    // quando cambia realmente.
    // ========================================================================

    bool ConsumeChanged(
        Word& output)
    {
        if (!HasChanged())
            return false;

        output =
            statusWord;

        consumedStatusWord =
            statusWord;

        consumed =
            true;

        return true;
    }

    // ========================================================================
    // PEEK LAST CONSUMED WORD
    // ========================================================================

    Word GetConsumedWord() const
    {
        return
            consumedStatusWord;
    }

    // ========================================================================
    // MARK CONSUMED
    //
    // Marca la word corrente come già consumata senza restituirla.
    //
    // Utile per inizializzare slot non utilizzati senza produrre eventi.
    // ========================================================================

    void MarkConsumed()
    {
        consumedStatusWord =
            statusWord;

        consumed =
            true;
    }

    // ========================================================================
    // FORCE CHANGED
    //
    // Forza la successiva ConsumeChanged() a restituire la word corrente,
    // anche quando non è stata modificata.
    // ========================================================================

    void ForceChanged()
    {
        consumed =
            false;
    }
};


class WatchEngine {
public:

    // ============================================================
    // 1. REGISTRAZIONE AREE DA MONITORARE
    // ============================================================
    static void attach(DomoManager& manager, const FrontendConfig::Watch& cfg) {
        if (!cfg.enabled)
            return;

        auto& diag = manager.getWatchDiag();

        for (size_t i = 0; i < cfg.count; i++) {
            diag.addArea(cfg.aree[i]);
        }

        LOG_IF("WatchEngine", "Diagnostic areas registered (%u)", cfg.count);
    }
};

class HVACEngine {
private:
    static inline DMHVAC hvac;   // wrapper ufficiale

public:

    // ========================================================================
    //  SETUP
    // ========================================================================
    static void Setup(const FrontendConfig::HVAC& cfg)
    {
        // FrontendConfig::HVAC eredita DMHVAC::HVACConfig
        hvac.setup(cfg);

        LOG_IF("HVAC", "HVAC initialized with %u zones", cfg.zoneCount);
    }

    // ========================================================================
    //  LOOP
    // ========================================================================
    static void Loop(unsigned long now,
                 const float* temperaturePerZona,
                 float tInterna,
                 float tEsterna,
                 bool finestraAperta,
                 const HeatPumpController::HVACTime& ht)
    {
        // Passiamo tutto direttamente al wrapper DMHVAC
        hvac.loop(
            now,
            ht,
            const_cast<float*>(temperaturePerZona),
            tInterna,
            tEsterna,
            finestraAperta
        );
    }


    // ========================================================================
    //  ACCESSORS
    // ========================================================================
    static HeatPumpController& GetHP() {
        return hvac.getHP();
    }

    static HeatPumpController::ZoneHVAC& GetZone(size_t i) {
        return *(hvac.getHP().getZoneList()[i]);
    }

    static size_t GetZoneCount() {
        return hvac.getHP().getZoneList().size();
    }

    static void DiagnosticReport() {
        HeatPumpController& hp = hvac.getHP();
        HeatPumpController::Diagnostic::Report(hp);
    }
};

class AEEEngine {
public:
    AEERegistry registry;
    AEEManagement* manager = nullptr;

    struct ReceiveContext {
        Buffer* buffer = nullptr;
        EventManager* events = nullptr;
        int aeeSource = -1;
    };

private:
    ReceiveContext receiveContext;

public:

    static AEEEngine& instance() {
        static AEEEngine inst;
        return inst;
    }

    void Setup(const FrontendConfig::Bridge::AEE& cfg) {
        manager = new AEEManagement(cfg.vars, cfg.count);
        manager->registerAll(registry);
    }

    static AEERegistry& getAEE() {
        return instance().registry;
    }

    static AEEManagement& getMgr() {
        return *instance().manager;
    }

    // ============================================================
    // CONTEXT RICEZIONE
    // ============================================================

    void attachReceiveContext(
        Buffer& buffer,
        EventManager& events,
        int aeeSource)
    {
        receiveContext.buffer = &buffer;
        receiveContext.events = &events;
        receiveContext.aeeSource = aeeSource;
    }

    // ============================================================
    // RICEZIONE AEE DAL PEER
    // ============================================================

    void applyReceived(
        AEEVariableBase* v,
        unsigned long now)
    {
        if (!v)
            return;

        if (!receiveContext.buffer ||
            !receiveContext.events)
            return;

        // --------------------------------------------------------
        // La variabile deve essere ricevibile dal Peer.
        // --------------------------------------------------------

        if (v->def.direction != AEEDirection::ModuleToFrontend &&
            v->def.direction != AEEDirection::Bidirectional)
        {
            return;
        }

        // ========================================================
        // 1. BUFFER AREA
        // ========================================================

        if (v->def.sourceType == AEEVarSourceType::BufferArea)
        {
            applyBufferArea(v, now);
            return;
        }

        // ========================================================
        // 2. FUNZIONE / SISTEMA
        // ========================================================

        onAEEVarChanged(v, now);
    }

private:

    // ============================================================
    // AEE BUFFER AREA → BUFFER + EVENT MANAGER
    // ============================================================

    void applyBufferArea(
        AEEVariableBase* v,
        unsigned long now)
    {
        Buffer& buffer = *receiveContext.buffer;

        const int area = v->def.bufferArea;

        if (area < 0 || area >= buffer.size())
        {
            LOG_WF(
                "AEE::RX",
                "Area non valida per '%s': %d",
                v->def.name,
                area
            );
            return;
        }

        long value = 0;

        // --------------------------------------------------------
        // BOOL
        // --------------------------------------------------------

        if (auto* b = as<bool>(v))
        {
            if (v->def.bitIndex >= 0)
            {
                // Mantieni tutti gli altri bit.
                value = buffer.getValueFast(area);

                bitWrite(
                    value,
                    v->def.bitIndex,
                    b->get()
                );
            }
            else
            {
                value = b->get() ? 1L : 0L;
            }
        }

        // --------------------------------------------------------
        // INT
        // --------------------------------------------------------

        else if (auto* i = as<int>(v))
        {
            value = static_cast<long>(
                i->get() / v->def.scale
            );
        }

        // --------------------------------------------------------
        // FLOAT
        // --------------------------------------------------------

        else if (auto* f = as<float>(v))
        {
            value = static_cast<long>(
                f->get() / v->def.scale
            );
        }

        else
        {
            LOG_WF(
                "AEE::RX",
                "Tipo non supportato per '%s'",
                v->def.name
            );
            return;
        }

        // --------------------------------------------------------
        // 1) Scrittura nel Buffer
        // --------------------------------------------------------

        if (DomoManager::instance)
        {
            DomoManager::instance->forceEvent(
                area,
                value,
                static_cast<uint8_t>(
                    receiveContext.aeeSource
                )
            );

            LOG_DF(
                "AEE::RX",
                "AEE '%s' -> Buffer area=%d value=%ld",
                v->def.name,
                area,
                value
            );
        }
    }


    // ============================================================
    // POST PROCESSOR AEE
    //
    // Qui confluisce la vecchia OnAEEVarChanged().
    // ============================================================

    void onAEEVarChanged(
        AEEVariableBase* v,
        unsigned long now)
    {
        if (!v)
            return;

        // ========================================================
        // NONE / MESSAGGI DI SISTEMA
        // ========================================================

        if (v->def.sourceType == AEEVarSourceType::None)
        {
            // ----------------------------------------------------
            // EPOCH
            // ----------------------------------------------------

            if (strcmp(
                    v->def.name,
                    "epoch") == 0)
            {
                auto* e = as<int>(v);

                if (!e)
                {
                    LOG_WF(
                        "AEE",
                        "epoch ricevuto con tipo non valido"
                    );
                    return;
                }

                int epoch = e->get();

                LOG_IF(
                    "AEE",
                    "Epoch ricevuto dal PEER: %d → applico al RTC",
                    epoch
                );

                receiveContext.buffer; // nessun uso, solo contesto AEE

                // Qui non usiamo il Buffer.
                // Serve il TimeManager del DOMO.
                if (DomoManager::instance)
                {
                    DomoManager::instance
                        ->getTimeManager()
                        .getRTC()
                        .applyEpoch(epoch);
                }

                v->clearChanged();

                return;
            }

            return;
        }

        // ========================================================
        // FUNCTION
        // ========================================================

        if (v->def.sourceType ==
            AEEVarSourceType::Function)
        {
            // Post processor applicativo.
            //
            // Qui aggiungeremo le eventuali AEE Function
            // ricevibili dal Peer.
            return;
        }
    }
};

class BridgeEngine
{
private:

    class DomoBridgePacer : public IBridgePacer
    {
    private:
        DMNetworkManager& network;
        DMNetworkManager::ProtocolId protocolId;

    public:
        DomoBridgePacer(
            DMNetworkManager& n,
            DMNetworkManager::ProtocolId id)
            : network(n),
              protocolId(id)
        {}

        bool acquire(unsigned long now) override
        {
            return network.tryAcquire(
                protocolId,
                now
            );
        }

        void release(unsigned long now) override
        {
            network.release(
                protocolId,
                now
            );
        }
    };


    DMMasterBridge bridge;
    DomoBridgePacer* pacer = nullptr;


public:

    static BridgeEngine& instance()
    {
        static BridgeEngine inst;
        return inst;
    }


    static DMMasterBridge& get()
    {
        return instance().bridge;
    }


    void setupPacer(
        DMNetworkManager& network,
        DMNetworkManager::ProtocolId protocolId)
    {
        delete pacer;

        pacer = new DomoBridgePacer(
            network,
            protocolId
        );

        bridge.setPacer(*pacer);
    }
};

class FrontendNetwork
{
public:

    static constexpr size_t MAX_MQTT_CLIENTS = 4;

    struct SolarTCP
    {
        DMPlatform::Network::TCPClient client;
    };

    struct ModbusTCP
    {
        DMPlatform::Network::TCPClient client;

        DMPlatform::Modbus::TCPClient modbus{
            client
        };
    };


    struct MQTT
    {
        DMPlatform::Network::TCPClient client;

        PubSubClient pubSub;


        MQTT()
            : client(),
              pubSub()
        {
            pubSub.setClient(
                client
            );
        }
    };


    explicit FrontendNetwork(
        size_t mqttClientCount)
    {
        _mqttClientCount =
            mqttClientCount;


        if (_mqttClientCount >
            MAX_MQTT_CLIENTS)
        {
            _mqttClientCount =
                MAX_MQTT_CLIENTS;
        }
    }


    ModbusTCP& modbusTCP()
    {
        return _modbusTCP;
    }


    MQTT& mqtt(
        size_t index)
    {
        return _mqtt[index];
    }


    size_t mqttClientCount() const
    {
        return _mqttClientCount;
    }

    SolarTCP& solar(
        size_t index)
    {
        return
            _solar[index];
    }

    const SolarTCP& solar(
        size_t index) const
    {
        return
            _solar[index];
    }
    
private:
    SolarTCP _solar [
                        FrontendConfig::
                            MAX_SOLAR_INVERTERS
                    ];

    ModbusTCP _modbusTCP;

    size_t _mqttClientCount = 0;

    MQTT _mqtt[
        MAX_MQTT_CLIENTS
    ];
};

// ============================================================
// SOLAR ENGINE
//
// Coordina il sottosistema Solar multi-inverter.
//
// Responsabilità:
//   - gestione della collezione di SolarOrchestrator;
//   - associazione tra configurazione e singolo orchestrator;
//   - assegnazione di un client TCP dedicato a ogni inverter;
//   - propagazione delle callback verso il frontend;
//   - esecuzione del loop di tutti gli inverter configurati;
//   - accesso centralizzato a stato, snapshot e diagnostica;
//   - inoltro dei comandi raw al corretto orchestrator;
//   - arresto coordinato dell'intero sottosistema.
//
// Ogni SolarOrchestrator gestisce autonomamente:
//   - creazione e distruzione del transport Serial/TCP;
//   - creazione dell'implementazione ISolarInverter;
//   - inizializzazione e capability discovery;
//   - polling del protocollo;
//   - stato runtime e connessione;
//   - retry e recovery della comunicazione;
//   - routing Snapshot verso DomoManager;
//   - diagnostica del singolo inverter.
//
// Deve essere dichiarato DOPO FrontendNetwork perché assegna
// a ogni SolarOrchestrator il relativo client TCP tramite:
//
//   network.solar(index).client
//
// SolarEngine non possiede direttamente transport o inverter:
// la loro ownership appartiene al singolo SolarOrchestrator.
// ============================================================

class SolarEngine
{
public:

    // ========================================================
    // PUBLIC TYPES
    //
    // SolarEngine espone gli stessi tipi del singolo
    // SolarOrchestrator, mantenendo stabile l'API usata
    // dal frontend.
    // ========================================================

    using Snapshot =
        SolarOrchestrator::Snapshot;

    using Diagnostics =
        SolarOrchestrator::Diagnostics;

    using RuntimeState =
        SolarOrchestrator::RuntimeState;

    using SnapshotChangedCallback =
        SolarOrchestrator::
            SnapshotChangedCallback;

    using FrameReceivedCallback =
        SolarOrchestrator::
            FrameReceivedCallback;

    using CommandStateCallback =
        SolarOrchestrator::
            CommandStateCallback;

private:

    // ========================================================
    // CONSTANTS
    // ========================================================

    static constexpr uint8_t
        MAX_ORCHESTRATORS =
            FrontendConfig::
                MAX_SOLAR_INVERTERS;

    static constexpr unsigned long
        COMMUNICATION_STALE_MS =
            30000UL;

    // ========================================================
    // SHARED REFERENCES
    // ========================================================

    static inline DomoManager*
        frontendManager =
            nullptr;

    // ========================================================
    // ORCHESTRATORS
    //
    // Ogni SolarOrchestrator gestisce un singolo inverter:
    //
    //   - transport;
    //   - protocollo;
    //   - polling;
    //   - runtime;
    //   - recovery;
    //   - routing eventi;
    //   - diagnostica.
    //
    // SolarEngine coordina esclusivamente la collezione.
    // ========================================================

    static inline SolarOrchestrator
        orchestrators[MAX_ORCHESTRATORS];

    static inline uint8_t
        orchestratorCount =
            0;

    // ========================================================
    // SLOT METADATA
    //
    // Conservati direttamente da SolarEngine per evitare
    // dipendenze da getter specifici di SolarOrchestrator.
    // ========================================================

    static inline const FrontendConfig::Solar*
        configurations[MAX_ORCHESTRATORS] {};

    static inline uint8_t
        eventSources[MAX_ORCHESTRATORS] {};

    // ========================================================
    // GENERIC FRONTEND STATUS
    //
    // Ogni orchestrator possiede una word tecnica separata
    // dallo status funzionale dell'inverter.
    // ========================================================

    static inline FrontendStatus
        frontendStatus[MAX_ORCHESTRATORS];

    // ========================================================
    // FRONTEND CALLBACKS
    //
    // Le callback vengono conservate anche prima del Setup.
    // ========================================================

    static inline SnapshotChangedCallback
        snapshotChangedCallback =
            nullptr;

    static inline FrameReceivedCallback
        frameReceivedCallback =
            nullptr;

    static inline CommandStateCallback
        commandStateCallback =
            nullptr;

private:

    // ========================================================
    // INDEX VALIDATION
    // ========================================================

    static bool IsValidId(
        uint8_t orchestratorId)
    {
        return
            orchestratorId <
                orchestratorCount &&
            orchestratorId <
                MAX_ORCHESTRATORS;
    }

    // ========================================================
    // INTERNAL ACCESSORS
    // ========================================================

    static SolarOrchestrator*
    Find(
        uint8_t orchestratorId)
    {
        if (!IsValidId(
                orchestratorId))
        {
            return nullptr;
        }

        return
            &orchestrators[
                orchestratorId
            ];
    }

    static const SolarOrchestrator*
    FindConst(
        uint8_t orchestratorId)
    {
        if (!IsValidId(
                orchestratorId))
        {
            return nullptr;
        }

        return
            &orchestrators[
                orchestratorId
            ];
    }

    // ========================================================
    // UPDATE FRONTEND STATUS
    //
    // Traduce lo stato specifico di SolarOrchestrator nella
    // word tecnica comune FrontendStatus.
    //
    // La funzione non pubblica eventi.
    // ========================================================

    static void UpdateFrontendStatus(
        uint8_t orchestratorId,
        unsigned long now)
    {
        SolarOrchestrator* orchestrator =
            Find(
                orchestratorId
            );

        if (!orchestrator)
            return;

        FrontendStatus& status =
            frontendStatus[
                orchestratorId
            ];

        const RuntimeState& runtime =
            orchestrator
                ->GetRuntime();

        const bool enabled =
            orchestrator
                ->IsEnabled();

        const bool running =
            orchestrator
                ->IsRunning();

        const bool initialized =
            orchestrator
                ->IsInitialized();

        const unsigned long lastSuccess =
            runtime.recovery
                .lastSuccessfulCommunication;

        const unsigned long nextRetry =
            runtime.recovery
                .nextInitializationAttempt;

        const bool hasSuccessfulCommunication =
            lastSuccess != 0;

        const bool communicationStale =
            hasSuccessfulCommunication &&
            static_cast<unsigned long>(
                now - lastSuccess
            ) >
            30000UL;

        /*
        * Connected significa:
        * almeno una risposta valida ricevuta
        * e non scaduta.
        */
        const bool connected =
            hasSuccessfulCommunication &&
            !communicationStale;

        /*
        * Active significa:
        * frontend realmente operativo.
        */
        const bool active =
            initialized &&
            connected;

        const bool recovering =
            enabled &&
            running &&
            !initialized &&
            nextRetry != 0;

        const bool degraded =
            enabled &&
            running &&
            initialized &&
            !active;

        const bool fault =
            enabled &&
            (
                !running ||
                (
                    !active &&
                    !recovering &&
                    !degraded
                )
            );

        status.Set(
            FrontendStatus::Flag::Enabled,
            enabled
        );

        status.Set(
            FrontendStatus::Flag::Running,
            running
        );

        status.Set(
            FrontendStatus::Flag::Initialized,
            initialized
        );

        status.Set(
            FrontendStatus::Flag::Connected,
            connected
        );

        status.Set(
            FrontendStatus::Flag::Active,
            active
        );

        status.Set(
            FrontendStatus::Flag::Recovering,
            recovering
        );

        status.Set(
            FrontendStatus::Flag::Degraded,
            degraded
        );

        status.Set(
            FrontendStatus::Flag::Fault,
            fault
        );

        LOG_DF(
            "SOLAR",
            "FrontendStatus id=%u init=%u conn=%u active=%u lastSuccess=%lu",
            static_cast<unsigned>(
                orchestratorId
            ),
            initialized,
            connected,
            active,
            lastSuccess
        );
    }

    // ========================================================
    // PUBLISH FRONTEND STATUS
    //
    // Pubblica la word solo quando cambia.
    //
    // La classe FrontendStatus mantiene internamente:
    //
    //   - word corrente;
    //   - ultima word consumata;
    //   - stato di cambiamento.
    // ========================================================

    static void PublishFrontendStatus(
        uint8_t orchestratorId,
        unsigned long now)
    {
        if (!IsValidId(
                orchestratorId))
        {
            return;
        }

        const FrontendConfig::Solar*
            configuration =
                configurations[
                    orchestratorId
                ];

        if (!configuration)
            return;

        const int area =
            configuration
                ->areas
                .frontendStatus;

        if (area < 0)
            return;

        if (!configuration->publishEvents)
            return;

        if (!frontendManager)
            return;

        UpdateFrontendStatus(
            orchestratorId,
            now
        );

        FrontendStatus::Word
            statusValue =
                0;

        if (
            !frontendStatus[
                orchestratorId
            ].ConsumeChanged(
                statusValue
            ))
        {
            return;
        }

        frontendManager
            ->forceEvent(
                area,
                static_cast<long>(
                    statusValue
                ),
                eventSources[
                    orchestratorId
                ]
            );

        LOG_DF(
            "SOLAR",
            "Orchestrator %u frontend status=0x%04X",
            static_cast<unsigned>(
                orchestratorId
            ),
            static_cast<unsigned>(
                statusValue
            )
        );
    }

public:

    // ========================================================
    // SHUTDOWN
    //
    // Arresta tutti gli orchestrator configurati.
    //
    // Le callback registrate su SolarEngine vengono conservate
    // per consentire un successivo Setup.
    // ========================================================

    static void Shutdown()
    {
        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .Shutdown();

            frontendStatus[index]
                .Reset();

            frontendStatus[index]
                .MarkConsumed();

            configurations[index] =
                nullptr;

            eventSources[index] =
                255;
        }

        orchestratorCount =
            0;

        frontendManager =
            nullptr;
    }

    // ========================================================
    // RESET
    //
    // Esegue uno shutdown completo e azzera anche le callback
    // frontend.
    // ========================================================

    static void Reset()
    {
        Shutdown();

        snapshotChangedCallback =
            nullptr;

        frameReceivedCallback =
            nullptr;

        commandStateCallback =
            nullptr;
    }

    // ========================================================
    // SETUP
    //
    // Costruisce la collezione di SolarOrchestrator.
    //
    // Ogni inverter TCP riceve il proprio client:
    //
    //   network.solar(0).client
    //   network.solar(1).client
    //   network.solar(2).client
    //   network.solar(3).client
    //
    // Gli inverter seriali non richiedono un client TCP.
    // ========================================================

    static void Setup(
        DomoManager& manager,
        DMNetworkManager& networkManager,
        FrontendNetwork& network,
        const FrontendConfig::SolarInverters&
            solarConfigurations,
        DMNetworkManager::ProtocolId protocolId,
        uint8_t firstEventSource)
    {
        Shutdown();

        frontendManager =
            &manager;

        orchestratorCount =
            solarConfigurations.count <
                MAX_ORCHESTRATORS
            ? solarConfigurations.count
            : MAX_ORCHESTRATORS;

        LOG_IF(
            "SOLAR",
            "SolarEngine setup: configured=%u activeSlots=%u",
            static_cast<unsigned>(
                solarConfigurations.count
            ),
            static_cast<unsigned>(
                orchestratorCount
            )
        );

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            const FrontendConfig::Solar&
                configuration =
                    solarConfigurations
                        .inverter[index];

            SolarOrchestrator& orchestrator =
                orchestrators[index];

            FrontendStatus& status =
                frontendStatus[index];

            // ------------------------------------------------
            // SLOT METADATA
            // ------------------------------------------------

            configurations[index] =
                &configuration;

            eventSources[index] =
                static_cast<uint8_t>(
                    firstEventSource +
                    index
                );

            // ------------------------------------------------
            // RESET FRONTEND STATUS
            //
            // Reset forza la prima pubblicazione.
            // ------------------------------------------------

            status.Reset();

            // ------------------------------------------------
            // TCP CLIENT ASSIGNMENT
            // ------------------------------------------------

            DMPlatform::Network::TCPClient*
                assignedTcpClient =
                    nullptr;

            if (
                configuration.transport ==
                FrontendConfig::Solar::
                    TransportType::TCP_CLIENT)
            {
                assignedTcpClient =
                    &network
                        .solar(index)
                        .client;
            }

            // ------------------------------------------------
            // FRONTEND CALLBACKS
            // ------------------------------------------------

            orchestrator
                .SetSnapshotChangedCallback(
                    snapshotChangedCallback
                );

            orchestrator
                .SetFrameReceivedCallback(
                    frameReceivedCallback
                );

            orchestrator
                .SetCommandStateCallback(
                    commandStateCallback
                );

            // ------------------------------------------------
            // SINGLE ORCHESTRATOR SETUP
            // ------------------------------------------------

            const bool configured =
                orchestrator.Setup(
                    index,
                    manager,
                    networkManager,
                    assignedTcpClient,
                    configuration,
                    protocolId,
                    eventSources[index]
                );

            const unsigned long now =
                millis();

            UpdateFrontendStatus(
                index,
                now
            );

            if (!configured)
            {
                status.Set(
                    FrontendStatus::Flag::Fault,
                    true
                );

                status.Set(
                    FrontendStatus::Flag::Recovering,
                    false
                );

                LOG_EF(
                    "SOLAR",
                    "Orchestrator %u setup failed",
                    static_cast<unsigned>(
                        index
                    )
                );
            }
            else
            {
                LOG_IF(
                    "SOLAR",
                    "Orchestrator %u configured: "
                    "enabled=%u transport=%u status=0x%04X",
                    static_cast<unsigned>(
                        index
                    ),
                    configuration.enabled
                        ? 1U
                        : 0U,
                    static_cast<unsigned>(
                        configuration.transport
                    ),
                    static_cast<unsigned>(
                        status.GetWord()
                    )
                );
            }

            PublishFrontendStatus(
                index,
                now
            );
        }

        // ----------------------------------------------------
        // RESET UNUSED SLOTS
        // ----------------------------------------------------

        for (
            uint8_t index = orchestratorCount;
            index < MAX_ORCHESTRATORS;
            ++index)
        {
            configurations[index] =
                nullptr;

            eventSources[index] =
                255;

            frontendStatus[index]
                .Reset();

            frontendStatus[index]
                .MarkConsumed();
        }
    }

    // ========================================================
    // LOOP
    //
    // Esegue il loop di tutti gli orchestrator configurati.
    //
    // Dopo ogni ciclo aggiorna e pubblica la word tecnica
    // soltanto quando cambia.
    // ========================================================

    static void Loop(
        unsigned long now)
    {
        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .Loop(now);

            PublishFrontendStatus(
                index,
                now
            );
        }
    }

    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    static void DiagnosticReport()
    {
        Serial.println();
        Serial.println(
            "===== SOLAR ENGINE ====="
        );

        Serial.print(
            "Orchestrator count: "
        );

        Serial.println(
            orchestratorCount
        );

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            Serial.print(
                "Frontend status ["
            );

            Serial.print(
                index
            );

            Serial.print(
                "]: 0x"
            );

            Serial.println(
                frontendStatus[index]
                    .GetWord(),
                HEX
            );

            orchestrators[index]
                .DiagnosticReport();
        }

        Serial.println(
            "===== END SOLAR ENGINE ====="
        );
    }

    static void DiagnosticReport(
        uint8_t orchestratorId)
    {
        SolarOrchestrator* orchestrator =
            Find(
                orchestratorId
            );

        if (!orchestrator)
        {
            Serial.print(
                "Solar orchestrator not found: "
            );

            Serial.println(
                orchestratorId
            );

            return;
        }

        Serial.print(
            "Frontend status: 0x"
        );

        Serial.println(
            frontendStatus[
                orchestratorId
            ].GetWord(),
            HEX
        );

        orchestrator
            ->DiagnosticReport();
    }

    // ========================================================
    // COUNT
    // ========================================================

    static uint8_t GetEngineCount()
    {
        return
            orchestratorCount;
    }

    static uint8_t GetOrchestratorCount()
    {
        return
            orchestratorCount;
    }

    // ========================================================
    // ORCHESTRATOR ACCESS
    // ========================================================

    static SolarOrchestrator*
    Get(
        uint8_t orchestratorId)
    {
        return
            Find(
                orchestratorId
            );
    }

    static const SolarOrchestrator*
    GetConst(
        uint8_t orchestratorId)
    {
        return
            FindConst(
                orchestratorId
            );
    }

    // ========================================================
    // FRONTEND STATUS ACCESS
    // ========================================================

    static const FrontendStatus*
    GetFrontendStatus(
        uint8_t orchestratorId = 0)
    {
        if (!IsValidId(
                orchestratorId))
        {
            return nullptr;
        }

        return
            &frontendStatus[
                orchestratorId
            ];
    }

    static FrontendStatus::Word
    GetFrontendStatusWord(
        uint8_t orchestratorId = 0)
    {
        const FrontendStatus* status =
            GetFrontendStatus(
                orchestratorId
            );

        return status
            ? status->GetWord()
            : 0;
    }

    static bool IsFrontendOperational(
        uint8_t orchestratorId = 0)
    {
        const FrontendStatus* status =
            GetFrontendStatus(
                orchestratorId
            );

        return
            status &&
            status->IsOperational();
    }

    // ========================================================
    // ORCHESTRATOR STATUS
    // ========================================================

    static bool IsInitialized(
        uint8_t orchestratorId = 0)
    {
        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return
            orchestrator &&
            orchestrator
                ->IsInitialized();
    }

    static bool IsRunning(
        uint8_t orchestratorId = 0)
    {
        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return
            orchestrator &&
            orchestrator
                ->IsRunning();
    }

    static bool IsConnected(
        uint8_t orchestratorId = 0)
    {
        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return
            orchestrator &&
            orchestrator
                ->IsConnected();
    }

    // ========================================================
    // RUNTIME ACCESSOR
    // ========================================================

    static const RuntimeState&
    GetRuntime(
        uint8_t orchestratorId = 0)
    {
        static const RuntimeState
            emptyRuntime {};

        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetRuntime()
            : emptyRuntime;
    }

    // ========================================================
    // INVERTER ACCESSORS
    // ========================================================

    static ISolarInverter*
    GetInstance(
        uint8_t orchestratorId = 0)
    {
        SolarOrchestrator* orchestrator =
            Find(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetInstance()
            : nullptr;
    }

    static const ISolarInverter*
    GetInstanceConst(
        uint8_t orchestratorId = 0)
    {
        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetInstance()
            : nullptr;
    }

    // ========================================================
    // SNAPSHOT ACCESSOR
    // ========================================================

    static const Snapshot&
    GetSnapshot(
        uint8_t orchestratorId = 0)
    {
        static const Snapshot
            emptySnapshot {};

        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetSnapshot()
            : emptySnapshot;
    }

    // ========================================================
    // DIAGNOSTICS ACCESSOR
    // ========================================================

    static const Diagnostics&
    GetDiagnostics(
        uint8_t orchestratorId = 0)
    {
        static const Diagnostics
            emptyDiagnostics {};

        const SolarOrchestrator* orchestrator =
            FindConst(
                orchestratorId
            );

        return orchestrator
            ? orchestrator->GetDiagnostics()
            : emptyDiagnostics;
    }

    // ========================================================
    // RAW COMMAND
    // ========================================================

    static bool SendRawCommand(
        uint8_t orchestratorId,
        const String& command,
        String& response)
    {
        SolarOrchestrator* orchestrator =
            Find(
                orchestratorId
            );

        if (!orchestrator)
            return false;

        return orchestrator
            ->SendRawCommand(
                command,
                response
            );
    }

    // ========================================================
    // LEGACY RAW COMMAND
    // ========================================================

    static bool SendRawCommand(
        const String& command,
        String& response)
    {
        return SendRawCommand(
            0,
            command,
            response
        );
    }

    // ========================================================
    // SNAPSHOT CALLBACK
    // ========================================================

    static void SetSnapshotChangedCallback(
        SnapshotChangedCallback callback)
    {
        snapshotChangedCallback =
            callback;

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .SetSnapshotChangedCallback(
                    callback
                );
        }
    }

    // ========================================================
    // FRAME CALLBACK
    // ========================================================

    static void SetFrameReceivedCallback(
        FrameReceivedCallback callback)
    {
        frameReceivedCallback =
            callback;

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .SetFrameReceivedCallback(
                    callback
                );
        }
    }

    // ========================================================
    // COMMAND STATE CALLBACK
    // ========================================================

    static void SetCommandStateCallback(
        CommandStateCallback callback)
    {
        commandStateCallback =
            callback;

        for (
            uint8_t index = 0;
            index < orchestratorCount;
            ++index)
        {
            orchestrators[index]
                .SetCommandStateCallback(
                    callback
                );
        }
    }
};

// ============================================================
// MQTT FRONTEND ENGINE
// Integrazione MQTT <-> DomoManager
//
// Versione rifattorizzata:
//   - nessuna dipendenza da DomoIntrospection
//   - nessuna dipendenza da DMHADiscovery
//   - nessun runtime Home Assistant
//   - trasporto raw esposto tramite API generiche
//   - callback raw e routed generiche
//   - mapping statici, dirty queue e anti-eco preservati
// ============================================================

class MQTTEngine
{
public:
    static constexpr uint8_t MAX_CLIENTS = 4;
    static constexpr uint8_t MAX_MAPPINGS = 64;

    using RawMessageReceivedCallback = void (*)(
        uint8_t clientIndex,
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        size_t length
    );

    using ServiceLoopCallback = void (*)(unsigned long now);

    using RoutedEventCallback = void (*)(
        uint8_t originClientIndex,
        int area,
        long value
    );

    // ========================================================================
    // INBOUND MESSAGE CALLBACK
    //
    // Callback applicativa unica per tutti i messaggi MQTT normalizzati.
    //
    // Riceve:
    //
    //   - mapping numerici;
    //   - mapping testuali;
    //   - messaggi raw non risolti;
    //   - comandi Home Assistant area;
    //   - comandi Home Assistant RuntimeStore.
    //
    // I puntatori contenuti in MQTTInboundMessage sono validi solamente
    // durante l'esecuzione sincrona della callback.
    // ========================================================================

    using InboundMessageReceivedCallback =
        void (*)(
            const MQTTInboundMessage& message
        );


    struct RuntimeClient
    {
        uint8_t index = 0;
        const FrontendConfig::MQTT::Client* cfg = nullptr;
        EthernetClient* eth = nullptr;
        PubSubClient* pubSub = nullptr;
        MQTT* mqtt = nullptr;
        unsigned long lastHeartbeat = 0;
        bool initialized = false;
        SocketHandle socket;
    };

    struct RuntimeCommandState
    {
        long value = 0;
        unsigned long timestamp = 0;
        bool initialized = false;
    };

    struct RuntimeMapping
    {
        uint8_t clientIndex = 0;
        const FrontendConfig::MQTT::Client* client = nullptr;
        const FrontendConfig::MQTT::Device* device = nullptr;
        const FrontendConfig::MQTT::Mapping* mapping = nullptr;
        RuntimeClient* runtimeClient = nullptr;
        RuntimeCommandState command;
        long lastRaw = 0;
        bool dirty = false;
        bool initialized = false;
    };

private:
    static DomoManager* manager;
    static const FrontendConfig::MQTT* configuration;
    static size_t clientCount;
    static size_t mappingCount;
    static unsigned long lastLoop;
    static DMNetworkManager* networkManager;
    static DMNetworkManager::ProtocolId mqttProtocolId;
    static uint8_t mqttSource;
    static uint16_t dirtyMappings[MAX_MAPPINGS];
    static uint16_t dirtyCount;

    static inline InboundMessageReceivedCallback
        inboundMessageReceivedCallback = nullptr;
    static inline ServiceLoopCallback serviceLoopCallback = nullptr;
    static inline RoutedEventCallback routedEventCallback = nullptr;
    static inline RawMessageReceivedCallback rawMessageReceivedCallback = nullptr;

   

    static RuntimeClient* getClients()
    {
        static RuntimeClient clients[MAX_CLIENTS];
        return clients;
    }

    static RuntimeMapping* getMappings()
    {
        static RuntimeMapping mappings[MAX_MAPPINGS];
        return mappings;
    }

    static void EnqueueDirty(uint16_t mappingIndex)
    {
        if (mappingIndex >= mappingCount)
            return;

        RuntimeMapping& rm = getMappings()[mappingIndex];

        if (rm.dirty)
            return;

        if (dirtyCount >= MAX_MAPPINGS)
        {
            LOG_WF("MQTT", "Dirty queue piena");
            return;
        }

        rm.dirty = true;
        dirtyMappings[dirtyCount++] = mappingIndex;
    }

    static bool CanRead(const FrontendConfig::MQTT::Mapping* mapping)
    {
        if (!mapping)
            return false;

        return
            mapping->direction == FrontendConfig::MQTT::Mapping::Direction::READ ||
            mapping->direction == FrontendConfig::MQTT::Mapping::Direction::READ_WRITE;
    }

    static bool CanWrite(const FrontendConfig::MQTT::Mapping* mapping)
    {
        if (!mapping)
            return false;

        return
            mapping->direction == FrontendConfig::MQTT::Mapping::Direction::WRITE ||
            mapping->direction == FrontendConfig::MQTT::Mapping::Direction::READ_WRITE;
    }

    static void BuildRuntimeMappings(const FrontendConfig::MQTT& cfg)
    {
        RuntimeMapping* mappings = getMappings();
        mappingCount = 0;

        if (!cfg.clients)
            return;

        for (size_t c = 0; c < cfg.clientCount; ++c)
        {
            const FrontendConfig::MQTT::Client& client = cfg.clients[c];

            if (!client.enabled || !client.devices)
                continue;

            for (size_t d = 0; d < client.deviceCount; ++d)
            {
                const FrontendConfig::MQTT::Device& device = client.devices[d];

                for (size_t m = 0;
                     m < FrontendConfig::MQTT::Device::MAX_MAPPINGS;
                     ++m)
                {
                    const FrontendConfig::MQTT::Mapping& mapping = device.mappings[m];

                    if (!mapping.field)
                        continue;

                    if (mappingCount >= MAX_MAPPINGS)
                    {
                        LOG_WF(
                            "MQTT",
                            "MAX_MAPPINGS raggiunto: %u",
                            static_cast<unsigned>(MAX_MAPPINGS)
                        );
                        return;
                    }

                    RuntimeMapping& rm = mappings[mappingCount++];
                    rm = RuntimeMapping{};
                    rm.clientIndex = static_cast<uint8_t>(c);
                    rm.client = &client;
                    rm.device = &device;
                    rm.mapping = &mapping;
                }
            }
        }
    }

    static void BindRuntimeMappings()
    {
        RuntimeClient* clients = getClients();
        RuntimeMapping* mappings = getMappings();

        for (size_t m = 0; m < mappingCount; ++m)
        {
            RuntimeMapping& rm = mappings[m];
            rm.runtimeClient = nullptr;

            for (size_t c = 0; c < clientCount; ++c)
            {
                if (clients[c].index == rm.clientIndex)
                {
                    rm.runtimeClient = &clients[c];
                    break;
                }
            }
        }
    }

    static void InitializeRuntimeValues()
    {
        RuntimeMapping* mappings = getMappings();

        for (size_t i = 0; i < mappingCount; ++i)
        {
            mappings[i].lastRaw = 0;
            mappings[i].dirty = false;
            mappings[i].initialized = false;
            mappings[i].command = RuntimeCommandState{};
        }
    }

    static void PublishDirty()
    {
        RuntimeMapping* mappings = getMappings();
        uint16_t writePosition = 0;

        for (uint16_t i = 0; i < dirtyCount; ++i)
        {
            const uint16_t mappingIndex = dirtyMappings[i];

            if (mappingIndex >= mappingCount)
                continue;

            RuntimeMapping& rm = mappings[mappingIndex];
            RuntimeClient* rc = rm.runtimeClient;

            if (!rc ||
                !rc->initialized ||
                !rc->mqtt ||
                rc->mqtt->state() != 0)
            {
                dirtyMappings[writePosition++] = mappingIndex;
                continue;
            }

            if (rc->mqtt->publishMapping(rm.device, rm.mapping, rm.lastRaw))
            {
                rm.dirty = false;
                rm.initialized = true;
            }
            else
            {
                dirtyMappings[writePosition++] = mappingIndex;
            }
        }

        dirtyCount = writePosition;
    }

    static void PublishMappedEventExcept(
        int excludedClientIndex,
        int area,
        long value)
    {
        if (area < 0)
            return;

        RuntimeMapping* mappings = getMappings();

        for (size_t i = 0; i < mappingCount; ++i)
        {
            RuntimeMapping& rm = mappings[i];

            if (!rm.mapping || rm.mapping->area != area)
                continue;

            if (!CanWrite(rm.mapping))
                continue;

            if (excludedClientIndex >= 0 &&
                rm.clientIndex == static_cast<uint8_t>(excludedClientIndex))
            {
                continue;
            }

            rm.lastRaw = value;
            EnqueueDirty(static_cast<uint16_t>(i));
        }
    }

    // ========================================================================
    // NOTIFY INBOUND MESSAGE
    //
    // Punto unico di uscita applicativo di MQTTEngine.
    // ========================================================================

    static void NotifyInboundMessage(
        const MQTTInboundMessage& message)
    {
        if (!inboundMessageReceivedCallback)
            return;

        if (!message.valid())
        {
            LOG_WF(
                "MQTT",
                "Inbound message non valido: "
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

        inboundMessageReceivedCallback(
            message
        );
    }

    // ========================================================================
    // MQTT INBOUND CALLBACK
    //
    // Riceve i messaggi già normalizzati dal layer MQTT low-level.
    //
    // MAPPING_NUMERIC:
    //
    //   - aggiorna RuntimeMapping;
    //   - aggiorna Buffer;
    //   - inoltra il valore agli altri client MQTT;
    //   - notifica Home Assistant;
    //   - inoltra il messaggio all'applicazione.
    //
    // MAPPING_TEXT / RAW:
    //
    //   - inoltra direttamente il messaggio all'applicazione.
    // ========================================================================

    static void MQTTInboundCallback(
        const MQTTInboundMessage& message)
    {
        using Origin =
            MQTTInboundMessage::Origin;

        // ========================================================
        // NON-NUMERIC MESSAGE
        // ========================================================

        if (message.origin !=
            Origin::MAPPING_NUMERIC)
        {
            NotifyInboundMessage(
                message
            );

            return;
        }

        // ========================================================
        // NUMERIC MAPPING VALIDATION
        // ========================================================

        const FrontendConfig::MQTT::Mapping* mapping =
            message.mapping;

        if (!mapping)
            return;

        const long value =
            message.integerValue;

        // ========================================================
        // COMMAND STATE
        // ========================================================

        RuntimeMapping* mappings =
            getMappings();

        bool unchanged =
            false;

        for (size_t i = 0;
            i < mappingCount;
            ++i)
        {
            RuntimeMapping& runtimeMapping =
                mappings[i];

            if (runtimeMapping.mapping !=
                mapping)
            {
                continue;
            }

            if (runtimeMapping.command.initialized &&
                runtimeMapping.command.value ==
                    value)
            {
                unchanged =
                    true;
            }
            else
            {
                runtimeMapping.command.initialized =
                    true;

                runtimeMapping.command.value =
                    value;

                runtimeMapping.command.timestamp =
                    millis();
            }

            break;
        }

        /*
        * La callback applicativa viene comunque invocata perché rappresenta
        * un messaggio MQTT realmente ricevuto.
        *
        * Il controllo unchanged impedisce solamente di applicare nuovamente
        * lo stesso comando al Buffer.
        */

        NotifyInboundMessage(
            message
        );

        if (unchanged)
            return;

        // ========================================================
        // BUFFER ROUTING
        // ========================================================

        if (mapping->area < 0 ||
            !manager ||
            !CanRead(mapping))
        {
            return;
        }

        manager->forceEvent(
            mapping->area,
            value,
            mqttSource
        );

        // ========================================================
        // CROSS-CLIENT ROUTING
        // ========================================================

        PublishMappedEventExcept(
            static_cast<int>(
                message.clientIndex
            ),
            mapping->area,
            value
        );

        // ========================================================
        // HOME ASSISTANT ROUTING
        // ========================================================

        if (routedEventCallback)
        {
            routedEventCallback(
                message.clientIndex,
                mapping->area,
                value
            );
        }

        LOG_IF(
            "MQTT_ROUTE",
            "Comando mapping applicato: "
            "originClient=%u device=%s field=%s "
            "area=%d value=%ld",
            static_cast<unsigned>(
                message.clientIndex
            ),
            message.device &&
            message.device->id
                ? message.device->id
                : "?",
            mapping->field
                ? mapping->field
                : "?",
            mapping->area,
            value
        );
    }

    static void MQTTRawMessageCallback(
        uint8_t clientIndex,
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        size_t length)
    {
        if (!rawMessageReceivedCallback)
            return;

        rawMessageReceivedCallback(
            clientIndex,
            backend,
            topic,
            payload,
            length
        );
    }


public:
    static void Setup(
        DomoManager& dm,
        DMNetworkManager& netManager,
        FrontendNetwork& network,
        size_t mqttClientCount,
        const FrontendConfig::MQTT& cfg,
        DMNetworkManager::ProtocolId protocol,
        uint8_t eventSource)
    {
        // ========================================================
        // SHARED REFERENCES
        // ========================================================

        manager =
            &dm;

        networkManager =
            &netManager;

        configuration =
            &cfg;

        mqttProtocolId =
            protocol;

        mqttSource =
            eventSource;

        // ========================================================
        // CALLBACK RESET
        //
        // La callback applicativa unificata verrà registrata dal
        // frontend dopo Setup().
        // ========================================================

        inboundMessageReceivedCallback =
            nullptr;

        serviceLoopCallback =
            nullptr;

        routedEventCallback =
            nullptr;

        rawMessageReceivedCallback =
            nullptr;

        // ========================================================
        // RUNTIME STORAGE
        // ========================================================

        RuntimeClient* clients =
            getClients();

        RuntimeMapping* mappings =
            getMappings();

        memset(
            mappings,
            0,
            sizeof(RuntimeMapping) *
                MAX_MAPPINGS
        );

        // ========================================================
        // DESTROY PREVIOUS CLIENTS
        // ========================================================

        for (size_t i = 0;
            i < MAX_CLIENTS;
            ++i)
        {
            if (clients[i].mqtt)
            {
                delete clients[i].mqtt;

                clients[i].mqtt =
                    nullptr;
            }

            clients[i] =
                RuntimeClient{};
        }

        clientCount =
            0;

        mappingCount =
            0;

        dirtyCount =
            0;

        // ========================================================
        // CONFIGURATION VALIDATION
        // ========================================================

        if (!cfg.enabled)
        {
            LOG_IF(
                "MQTT",
                "MQTT disabilitato"
            );

            return;
        }

        if (!cfg.clients ||
            cfg.clientCount == 0)
        {
            LOG_IF(
                "MQTT",
                "Nessun client MQTT configurato"
            );

            return;
        }

        if (mqttClientCount == 0)
        {
            LOG_IF(
                "MQTT",
                "Nessun slot MQTT disponibile"
            );

            return;
        }

        // ========================================================
        // BUILD STATIC DEVICE/MAPPING INDEX
        // ========================================================

        BuildRuntimeMappings(
            cfg
        );

        // ========================================================
        // SOCKET OWNER
        // ========================================================

        const SocketManager::OwnerId mqttSocketOwner =
            networkManager
                ->getProtocol(
                    mqttProtocolId
                )
                .socketOwner;

        if (mqttSocketOwner < 0)
        {
            LOG_EF(
                "MQTT",
                "Socket owner MQTT non valido: protocol=%d",
                static_cast<int>(
                    mqttProtocolId
                )
            );

            return;
        }

        // ========================================================
        // CLIENT LIMIT
        // ========================================================

        size_t maxClients =
            cfg.clientCount;

        if (maxClients >
            MAX_CLIENTS)
        {
            maxClients =
                MAX_CLIENTS;
        }

        if (maxClients >
            mqttClientCount)
        {
            maxClients =
                mqttClientCount;
        }

        // ========================================================
        // CLIENT CREATION
        // ========================================================

        for (size_t configIndex = 0;
            configIndex < maxClients;
            ++configIndex)
        {
            const FrontendConfig::MQTT::Client& clientCfg =
                cfg.clients[
                    configIndex
                ];

            if (!clientCfg.enabled)
                continue;

            RuntimeClient& rc =
                clients[
                    clientCount
                ];

            rc =
                RuntimeClient{};

            rc.index =
                static_cast<uint8_t>(
                    configIndex
                );

            rc.cfg =
                &clientCfg;

            rc.eth =
                &network
                    .mqtt(
                        configIndex
                    )
                    .client;

            rc.pubSub =
                &network
                    .mqtt(
                        configIndex
                    )
                    .pubSub;

            // ====================================================
            // LOW-LEVEL MQTT INSTANCE
            // ====================================================

            rc.mqtt =
                new MQTT(
                    *rc.eth,
                    *rc.pubSub,
                    static_cast<uint8_t>(
                        configIndex
                    ),
                    &clientCfg,
                    clientCfg.devices,
                    clientCfg.deviceCount
                );

            if (!rc.mqtt)
            {
                LOG_EF(
                    "MQTT",
                    "Client[%u] allocazione MQTT fallita",
                    static_cast<unsigned>(
                        configIndex
                    )
                );

                continue;
            }

            // ====================================================
            // SOCKET REGISTRATION
            // ====================================================

            rc.socket.setup(
                networkManager,
                mqttSocketOwner,
                static_cast<int>(
                    rc.index
                ),
                SocketManager::SocketKind::
                    TCP_CLIENT
            );

            // ====================================================
            // MQTT CALLBACKS
            //
            // inbound:
            //   mapping numerici, testuali e raw applicativi.
            //
            // raw:
            //   ponte infrastrutturale verso DMHAOrchestrator.
            // ====================================================

            rc.mqtt
                ->setInboundMessageCallback(
                    MQTTInboundCallback
                );

            rc.mqtt
                ->setRawMessageCallback(
                    MQTTRawMessageCallback
                );

            rc.mqtt
                ->setBackend(
                    clientCfg.backend
                );

            // ====================================================
            // LOW-LEVEL BEGIN
            // ====================================================

            if (!rc.mqtt->begin())
            {
                LOG_EF(
                    "MQTT",
                    "Client[%u] begin fallito: %s",
                    static_cast<unsigned>(
                        configIndex
                    ),
                    clientCfg.name
                        ? clientCfg.name
                        : "unnamed"
                );

                delete rc.mqtt;

                rc.mqtt =
                    nullptr;

                continue;
            }

            // ====================================================
            // MQTT BUFFER
            // ====================================================

            const uint16_t mqttBufferSize =
                clientCfg.backend ==
                    FrontendConfig::MQTT::Client::
                        Backend::HOME_ASSISTANT
                    ? 2048U
                    : 512U;

            if (!rc.pubSub
                    ->setBufferSize(
                        mqttBufferSize
                    ) ||
                rc.pubSub
                    ->getBufferSize() <
                        mqttBufferSize)
            {
                LOG_EF(
                    "MQTT",
                    "Client[%u] buffer non valido: "
                    "requested=%u actual=%u",
                    static_cast<unsigned>(
                        configIndex
                    ),
                    static_cast<unsigned>(
                        mqttBufferSize
                    ),
                    static_cast<unsigned>(
                        rc.pubSub
                            ->getBufferSize()
                    )
                );

                delete rc.mqtt;

                rc.mqtt =
                    nullptr;

                continue;
            }

            rc.initialized =
                true;

            LOG_IF(
                "MQTT",
                "Client[%u]: %s backend=%u "
                "broker=%s:%u devices=%u buffer=%u",
                static_cast<unsigned>(
                    configIndex
                ),
                clientCfg.name
                    ? clientCfg.name
                    : "unnamed",
                static_cast<unsigned>(
                    clientCfg.backend
                ),
                clientCfg.broker
                    .toString()
                    .c_str(),
                static_cast<unsigned>(
                    clientCfg.port
                ),
                static_cast<unsigned>(
                    clientCfg.deviceCount
                ),
                static_cast<unsigned>(
                    rc.pubSub
                        ->getBufferSize()
                )
            );

            ++clientCount;
        }

        // ========================================================
        // RUNTIME MAPPING INITIALIZATION
        // ========================================================

        BindRuntimeMappings();

        InitializeRuntimeValues();

        LOG_IF(
            "MQTT",
            "MQTT Setup completato: "
            "clients=%u mappings=%u protocol=%d",
            static_cast<unsigned>(
                clientCount
            ),
            static_cast<unsigned>(
                mappingCount
            ),
            static_cast<int>(
                mqttProtocolId
            )
        );
    }

    static void Loop(unsigned long now)
    {
        if (!configuration ||
            !configuration->enabled ||
            !networkManager)
        {
            return;
        }

        static bool mqttStartupReady = false;

        if (!mqttStartupReady)
        {
            if (now < 10000UL)
                return;

            mqttStartupReady = true;
        }

        if (!networkManager->tryAcquire(mqttProtocolId, now))
            return;

        RuntimeClient* clients = getClients();

        for (size_t i = 0; i < clientCount; ++i)
        {
            RuntimeClient& rc = clients[i];

            if (!rc.initialized || !rc.mqtt)
                continue;

            if (!rc.mqtt->connected())
            {
                if (rc.mqtt->reconnectDue())
                {
                    if (rc.socket.acquire())
                    {
                        if (!rc.mqtt->reconnect())
                            rc.socket.release();
                    }
                }
            }

            if (rc.mqtt->connected())
                rc.mqtt->loop();
            else
                rc.socket.release();
        }

        PublishDirty();

        if (serviceLoopCallback)
            serviceLoopCallback(now);

        networkManager->release(mqttProtocolId, now);
        lastLoop = now;
    }

    static void PublishEvent(const EventManager::Event& event)
    {
        PublishEvent(event.area, event.value);
    }

    static void PublishEvent(int area, long value)
    {
        PublishMappedEventExcept(-1, area, value);
    }

    static void PublishEventFromMQTT(
        uint8_t originClientIndex,
        int area,
        long value)
    {
        PublishMappedEventExcept(
            static_cast<int>(originClientIndex),
            area,
            value
        );

        if (routedEventCallback)
        {
            routedEventCallback(
                originClientIndex,
                area,
                value
            );
        }
    }

    // ========================================================================
    // SET INBOUND MESSAGE CALLBACK
    //
    // La registrazione può avvenire anche dopo Setup().
    //
    // In tal caso la callback viene propagata a tutte le istanze MQTT già
    // costruite.
    // ========================================================================

    static void setInboundMessageReceivedCallback(
        InboundMessageReceivedCallback callback)
    {
        inboundMessageReceivedCallback =
            callback;

        RuntimeClient* clients =
            getClients();

        for (size_t i = 0;
            i < clientCount;
            ++i)
        {
            RuntimeClient& client =
                clients[i];

            if (!client.initialized ||
                !client.mqtt)
            {
                continue;
            }

            client.mqtt->setInboundMessageCallback(
                MQTTInboundCallback
            );
        }
    }

    static void setServiceLoopCallback(
        ServiceLoopCallback callback)
    {
        serviceLoopCallback = callback;
    }

    static void setRoutedEventCallback(
        RoutedEventCallback callback)
    {
        routedEventCallback = callback;
    }

    static void setRawMessageReceivedCallback(
        RawMessageReceivedCallback callback)
    {
        rawMessageReceivedCallback = callback;
    }

    static size_t getClientCount()
    {
        return clientCount;
    }

    static size_t getMappingCount()
    {
        return mappingCount;
    }

    static const RuntimeClient* getClient(size_t index)
    {
        if (index >= clientCount)
            return nullptr;

        return &getClients()[index];
    }

    static const RuntimeMapping* getMapping(size_t index)
    {
        if (index >= mappingCount)
            return nullptr;

        return &getMappings()[index];
    }

    static RuntimeClient* FindClientByBackend(
        FrontendConfig::MQTT::Client::Backend backend)
    {
        RuntimeClient* clients = getClients();

        for (size_t i = 0; i < clientCount; ++i)
        {
            RuntimeClient& client = clients[i];

            if (!client.initialized ||
                !client.cfg ||
                !client.mqtt)
            {
                continue;
            }

            if (client.cfg->backend == backend)
                return &client;
        }

        return nullptr;
    }

    static bool IsConnected(
        FrontendConfig::MQTT::Client::Backend backend)
    {
        RuntimeClient* client = FindClientByBackend(backend);

        return
            client &&
            client->initialized &&
            client->mqtt &&
            client->mqtt->connected();
    }

    static bool SubscribeRaw(
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic)
    {
        if (!topic || topic[0] == '\0')
            return false;

        RuntimeClient* client = FindClientByBackend(backend);

        if (!client ||
            !client->initialized ||
            !client->mqtt ||
            !client->mqtt->connected())
        {
            return false;
        }

        return client->mqtt->subscribeRaw(topic);
    }

    static bool PublishRaw(
        FrontendConfig::MQTT::Client::Backend backend,
        const char* topic,
        const char* payload,
        bool retain = false)
    {
        if (!topic || topic[0] == '\0' || !payload)
            return false;

        RuntimeClient* client = FindClientByBackend(backend);

        if (!client ||
            !client->initialized ||
            !client->mqtt ||
            !client->mqtt->connected())
        {
            return false;
        }

        return client->mqtt->publishRaw(
            topic,
            payload,
            retain
        );
    }
};

// ============================================================
// STATIC MEMBERS
// ============================================================

DomoManager* MQTTEngine::manager = nullptr;

const FrontendConfig::MQTT*
    MQTTEngine::configuration = nullptr;

size_t MQTTEngine::clientCount = 0;
size_t MQTTEngine::mappingCount = 0;

unsigned long MQTTEngine::lastLoop = 0;

DMNetworkManager*
    MQTTEngine::networkManager = nullptr;

DMNetworkManager::ProtocolId
    MQTTEngine::mqttProtocolId = -1;

uint8_t MQTTEngine::mqttSource = 255;

uint16_t MQTTEngine::dirtyMappings[
    MQTTEngine::MAX_MAPPINGS
];

uint16_t MQTTEngine::dirtyCount = 0;

// ============================================================
// WEB API ENGINE
//
// Gestisce:
//
//   CLIENT:
//     DomoManager -> dispositivi HTTP remoti
//
//   SERVER:
//     dispositivi HTTP remoti -> DomoManager
//
//   ACCESS:
//     AccessControlService
//     FanvilWebAPI
//     AxessMCcWebAPI
//
// Gli endpoint HTTP vengono creati internamente a runtime.
// ============================================================

class WebAPIEngine
{
public:

    using WebRequestCallback =
        std::function<void(
            const char* endpointName,
            const HttpServerRequest& request,
            const HttpServerResponse& response
        )>;

    using AccessEventCallback =
        AccessControlService::EventCallback;

    using AccessAuthorizedCallback =
        AccessControlService::AuthorizedCallback;

    using DeviceResponseCallback =
    std::function<void(
        const WebApiDevice& device,
        const DeviceMessageProfile::OutMessage& message,
        const String& rawResponse,
        const ParsedResponse& parsed
    )>;
private:
    // ========================================================================
    // DOMOMANAGER ONLINE CLIENT
    // ========================================================================

    static inline DomoManagerOnlineClient
        onlineClient;

    // ========================================================
    // CONFIGURATION
    // ========================================================

    static inline const FrontendConfig::
        webApiDeviceMessaging*
        configuration =
            nullptr;


    // ========================================================
    // CLIENT / POLLING
    // ========================================================

    static inline HttpClientTransport*
        clientTransport =
            nullptr;

    static inline DeviceMessageEngine*
        clientEngine =
            nullptr;

    static inline bool
        clientRunning =
            false;


    // ========================================================
    // SERVER / WEBHOOK
    // ========================================================

    static inline HttpServerTransport*
        serverTransport =
            nullptr;

    static inline WebApiServerEngine*
        serverEngine =
            nullptr;

    static inline bool
        serverRunning =
            false;


    // ========================================================
    // ACCESS CONTROL
    // ========================================================

    static inline AccessControlService*
        accessControl =
            nullptr;

    static inline std::vector<IAccessWebAPI*> accessProtocols;

    static inline bool
        accessRunning =
            false;


    // ========================================================
    // RUNTIME SERVER ENDPOINTS
    //
    // La quantità di endpoint dipende dai protocolli
    // registrati a runtime.
    //
    // Lo storage rimane valido per tutta la vita del server.
    // SetupAccessControl() completa la registrazione prima che
    // SetupServer() passi data() al WebApiServerEngine.
    // ========================================================

    static inline std::vector<WebApiServerEndpoint>
        serverEndpoints;


    // ========================================================
    // CLIENT CLEANUP
    // ========================================================

    static void CleanupClient()
    {
        // ====================================================
        // DOMOMANAGER ONLINE
        //
        // Il client online mantiene riferimenti a:
        //
        //   - DeviceMessageEngine;
        //   - WebApiDevice interno al motore;
        //
        // Deve quindi essere resettato prima della distruzione
        // di clientEngine.
        // ====================================================

        onlineClient.Reset();

        // ====================================================
        // STOP RUNTIME
        // ====================================================

        clientRunning =
            false;

        // ====================================================
        // CANCEL TRANSPORT
        //
        // La cancellazione viene eseguita prima di distruggere
        // il DeviceMessageEngine, così eventuali callback HTTP
        // pendenti non possono utilizzare il motore eliminato.
        // ====================================================

        if (clientTransport)
        {
            clientTransport->cancel();
        }

        // ====================================================
        // DEVICE MESSAGE ENGINE
        // ====================================================

        if (clientEngine)
        {
            delete clientEngine;

            clientEngine =
                nullptr;
        }

        // ====================================================
        // HTTP CLIENT TRANSPORT
        // ====================================================

        if (clientTransport)
        {
            delete clientTransport;

            clientTransport =
                nullptr;
        }

        LOG_DF(
            "WEBAPI-CLIENT",
            "HTTP client resources released"
        );
    }


    // ========================================================
    // SERVER CLEANUP
    //
    // WebApiServerEngine deve essere eliminato prima del
    // transport perché contiene gli endpoint adapter.
    // ========================================================

    static void CleanupServer()
    {
        if (serverEngine)
        {
            delete serverEngine;

            serverEngine =
                nullptr;
        }

        if (serverTransport)
        {
            delete serverTransport;

            serverTransport =
                nullptr;
        }

        serverRunning =
            false;
    }


    // ========================================================
    // ACCESS CONTROL CLEANUP
    //
    // Deve essere chiamata dopo CleanupServer().
    // ========================================================

    static void CleanupAccessControl()
    {
        /*
        * Gli endpoint contengono std::function collegate
        * alle istanze dei protocolli.
        *
        * Gli handler devono essere distrutti prima delle
        * istanze FanvilWebAPI e AxessTMCWebAPI.
        */

        serverEndpoints.clear();

        /*
        * Opzionale:
        *
        * Non usiamo shrink_to_fit() perché questa funzione
        * può essere chiamata anche durante una reinizializzazione.
        * Mantenere la capacità evita riallocazioni successive.
        */

        for (
            IAccessWebAPI* protocol :
            accessProtocols
        )
        {
            delete protocol;
        }

        accessProtocols.clear();

        if (accessControl)
        {
            delete accessControl;

            accessControl =
                nullptr;
        }

        accessRunning =
            false;
    }


    // ========================================================
    // COMPLETE CLEANUP
    // ========================================================

    static void Cleanup()
    {
        /*
         * Ordine obbligatorio:
         *
         *   1. client
         *   2. server e adapter
         *   3. endpoint e protocolli
         */

        CleanupClient();
        CleanupServer();
        CleanupAccessControl();

        configuration =
            nullptr;
    }


    // ========================================================
    // REGISTER ACCESS PROTOCOL ENDPOINTS
    //
    // Registra tutti gli endpoint esposti dal protocollo.
    // ========================================================

    static bool RegisterAccessEndpoints(
        IAccessWebAPI& protocol)
    {
        const size_t protocolEndpointCount =
            protocol.endpointCount();

        if (protocolEndpointCount == 0)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Protocol '%s' exposes no endpoints",
                protocol.protocolId()
                    ? protocol.protocolId()
                    : ""
            );

            return false;
        }

        /*
        * Salviamo la dimensione iniziale per poter effettuare
        * il rollback se una definizione del protocollo non è
        * valida o genera un duplicato.
        */
        const size_t initialEndpointCount =
            serverEndpoints.size();

        /*
        * Una sola eventuale riallocazione per tutti gli endpoint
        * esposti dal protocollo corrente.
        *
        * In questa fase il WebApiServerEngine non è ancora stato
        * inizializzato, quindi data() non è ancora utilizzato da
        * alcun componente.
        */
        serverEndpoints.reserve(
            initialEndpointCount +
            protocolEndpointCount
        );

        for (
            size_t protocolEndpointIndex = 0;
            protocolEndpointIndex <
                protocolEndpointCount;
            ++protocolEndpointIndex
        )
        {
            const AccessWebAPIEndpointDef* definition =
                protocol.endpointDefinition(
                    protocolEndpointIndex
                );

            // ====================================================
            // DEFINITION VALIDATION
            // ====================================================

            if (
                !definition ||
                !definition->name ||
                definition->name[0] == '\0' ||
                !definition->path ||
                definition->path[0] == '\0'
            )
            {
                LOG_EF(
                    "WEBAPI-SERVER",
                    "Invalid endpoint definition: "
                    "protocol=%s index=%u",
                    protocol.protocolId()
                        ? protocol.protocolId()
                        : "",
                    static_cast<unsigned>(
                        protocolEndpointIndex
                    )
                );

                serverEndpoints.resize(
                    initialEndpointCount
                );

                return false;
            }

            // ====================================================
            // DUPLICATE VALIDATION
            //
            // Impedisce la registrazione dello stesso metodo
            // sullo stesso path.
            // ====================================================

            for (
                const WebApiServerEndpoint& registered :
                serverEndpoints
            )
            {
                const bool samePath =
                    registered.path &&
                    strcmp(
                        registered.path,
                        definition->path
                    ) == 0;

                const bool sameMethod =
                    (
                        !registered.method &&
                        !definition->method
                    ) ||
                    (
                        registered.method &&
                        definition->method &&
                        strcmp(
                            registered.method,
                            definition->method
                        ) == 0
                    );

                if (
                    samePath &&
                    sameMethod
                )
                {
                    LOG_EF(
                        "WEBAPI-SERVER",
                        "Duplicate endpoint: "
                        "method=%s path=%s "
                        "protocol=%s",
                        definition->method
                            ? definition->method
                            : "*",
                        definition->path,
                        protocol.protocolId()
                            ? protocol.protocolId()
                            : ""
                    );

                    serverEndpoints.resize(
                        initialEndpointCount
                    );

                    return false;
                }
            }

            // ====================================================
            // ENDPOINT CONSTRUCTION
            // ====================================================

            WebApiServerEndpoint endpoint{};

            endpoint.name =
                definition->name;

            endpoint.path =
                definition->path;

            endpoint.method =
                definition->method;

            endpoint.enabled =
                true;

            IAccessWebAPI* protocolPtr =
                &protocol;

            const size_t capturedEndpointIndex =
                protocolEndpointIndex;

            const char* capturedEndpointName =
                definition->name;

            endpoint.handler =
                [
                    protocolPtr,
                    capturedEndpointIndex,
                    capturedEndpointName
                ](
                    const HttpServerRequest& request,
                    HttpServerResponse& response)
                {
                    if (!protocolPtr)
                    {
                        response.statusCode =
                            503;

                        response.contentType =
                            "text/plain";

                        response.body =
                            "SERVICE UNAVAILABLE";

                        response.closeConnection =
                            true;

                        if (webRequestCallback)
                        {
                            webRequestCallback(
                                capturedEndpointName,
                                request,
                                response
                            );
                        }

                        return;
                    }

                    protocolPtr->handleRequest(
                        capturedEndpointIndex,
                        request,
                        response
                    );

                    if (webRequestCallback)
                    {
                        webRequestCallback(
                            capturedEndpointName,
                            request,
                            response
                        );
                    }
                };

            // ====================================================
            // REGISTRATION
            // ====================================================

            serverEndpoints.push_back(
                std::move(
                    endpoint
                )
            );

            LOG_IF(
                "WEBAPI-SERVER",
                "Endpoint registered: "
                "protocol=%s index=%u "
                "name=%s method=%s path=%s "
                "total=%u",
                protocol.protocolId()
                    ? protocol.protocolId()
                    : "",
                static_cast<unsigned>(
                    protocolEndpointIndex
                ),
                definition->name,
                definition->method
                    ? definition->method
                    : "*",
                definition->path,
                static_cast<unsigned>(
                    serverEndpoints.size()
                )
            );
        }

        return true;
    }

     static bool AddAccessProtocol(
        IAccessWebAPI* protocol)
    {
        if (!protocol)
            return false;

        if (
            !RegisterAccessEndpoints(
                *protocol
            )
        )
        {
            delete protocol;

            return false;
        }

        accessProtocols.push_back(
            protocol
        );

        return true;
    }

    static bool BuildAccessProtocols()
    {
        if (!accessControl)
            return false;

        if (
            !AddAccessProtocol(
                new FanvilWebAPI(
                    *accessControl
                )
            )
        )
        {
            LOG_EF(
                "ACCESS",
                "Unable to register Fanvil protocol"
            );

            return false;
        }

        if (
            !AddAccessProtocol(
                new AxessTMCWebAPI(
                    *accessControl
                )
            )
        )
        {
            LOG_EF(
                "ACCESS",
                "Unable to register AxessTMC protocol"
            );

            return false;
        }

        return true;
    }

    // ========================================================
    // ACCESS CONTROL SETUP
    // ========================================================

    static bool SetupAccessControl(
        const FrontendConfig::Access& cfg)
    {
        CleanupAccessControl();

        // ====================================================
        // DISABLED
        // ====================================================

        if (!cfg.enabled)
        {
            LOG_IF(
                "ACCESS",
                "Access control disabled"
            );

            return true;
        }

        // ====================================================
        // CONFIGURATION VALIDATION
        // ====================================================

        if (
            !cfg.readers ||
            cfg.readerCount == 0
        )
        {
            LOG_EF(
                "ACCESS",
                "Access control enabled without readers"
            );

            return false;
        }

        if (cfg.defaultRelayIndex < 0)
        {
            LOG_EF(
                "ACCESS",
                "Invalid default relay index: %d",
                cfg.defaultRelayIndex
            );

            return false;
        }

        if (cfg.defaultRelayTime < 0)
        {
            LOG_EF(
                "ACCESS",
                "Invalid default relay time: %d",
                cfg.defaultRelayTime
            );

            return false;
        }

        // ====================================================
        // ACCESS SERVICE
        // ====================================================

        accessControl =
            new AccessControlService();

        if (!accessControl)
        {
            LOG_EF(
                "ACCESS",
                "Unable to allocate AccessControlService"
            );

            return false;
        }

        // ====================================================
        // ACCESS SERVICE CONFIGURATION
        // ====================================================

        AccessControlService::Config
            accessConfig;

        accessConfig.readers =
            cfg.readers;

        accessConfig.readerCount =
            cfg.readerCount;

        accessConfig.commonCardCode =
            cfg.commonCardCode;

        accessConfig.defaultRelayIndex =
            cfg.defaultRelayIndex;

        accessConfig.defaultRelayTime =
            cfg.defaultRelayTime;

        // ====================================================
        // ALL ACCESS EVENTS
        // ====================================================

        accessConfig.onEvent =
            [](
                const AccessControlService::Request& request,
                const AccessControlService::Decision& decision)
            {
                const char* readerName =
                    (
                        decision.reader &&
                        decision.reader->name
                    )
                        ? decision.reader->name
                        : request.readerName.c_str();

                const char* definitionId =
                    (
                        decision.readerDef &&
                        decision.readerDef->id
                    )
                        ? decision.readerDef->id
                        : "";

                const unsigned authorized =
                    decision.authorized
                        ? 1U
                        : 0U;

                const unsigned commonCode =
                    decision.commonCode
                        ? 1U
                        : 0U;

                const char* result =
                    AccessControlService::ResultToString(
                        decision.result
                    );

                LOG_IF(
                    "ACCESS",
                    "reader=%s def=%s card=%s "
                    "authorized=%u common=%u result=%s",
                    readerName,
                    definitionId,
                    decision.cardCode.c_str(),
                    authorized,
                    commonCode,
                    result
                );

                if (accessEventCallback)
                {
                    accessEventCallback(
                        request,
                        decision
                    );
                }
            };

        // ====================================================
        // AUTHORIZED ACCESS EVENTS
        // ====================================================

        accessConfig.onAuthorized =
        [](
            const AccessControlService::Request& request,
            const AccessControlService::Decision& decision)
        {
            const char* readerName =
                (
                    decision.reader &&
                    decision.reader->name
                )
                    ? decision.reader->name
                    : request.readerName.c_str();

            const int area =
                decision.reader
                    ? decision.reader->area
                    : -1;

            LOG_IF(
                "ACCESS",
                "Access granted: "
                "reader=%s card=%s relay=%d "
                "time=%d area=%d",
                readerName,
                decision.cardCode.c_str(),
                decision.relayIndex,
                decision.relayTime,
                area
            );

            if (accessAuthorizedCallback)
            {
                accessAuthorizedCallback(
                    request,
                    decision
                );
            }
        };

        // ====================================================
        // SERVICE INITIALIZATION
        // ====================================================

        if (
            !accessControl->setup(
                accessConfig
            )
        )
        {
            LOG_EF(
                "ACCESS",
                "AccessControlService setup failed"
            );

            CleanupAccessControl();

            return false;
        }

        // ====================================================
        // ACCESS PROTOCOLS AND ENDPOINTS
        //
        // BuildAccessProtocols() crea e registra i protocolli
        // concreti usando la collezione accessProtocols.
        // ====================================================

        if (!BuildAccessProtocols())
        {
            LOG_EF(
                "ACCESS",
                "Unable to build access protocols"
            );

            CleanupAccessControl();

            return false;
        }

        // ====================================================
        // FINAL VALIDATION
        // ====================================================

        if (accessProtocols.empty())
        {
            LOG_EF(
                "ACCESS",
                "No access protocols registered"
            );

            CleanupAccessControl();

            return false;
        }

        if (serverEndpoints.empty())
        {
            LOG_EF(
                "ACCESS",
                "No access endpoints registered"
            );

            CleanupAccessControl();

            return false;
        }

        // ====================================================
        // SUCCESS
        // ====================================================

        accessRunning =
            true;

        const unsigned configuredReaders =
            static_cast<unsigned>(
                accessControl->readerCount()
            );

        const unsigned configuredProtocols =
            static_cast<unsigned>(
                accessProtocols.size()
            );

        const unsigned configuredEndpoints =
            static_cast<unsigned>(
                serverEndpoints.size()
            );

        const bool commonCardConfigured =
            cfg.commonCardCode &&
            cfg.commonCardCode[0] != '\0';

        const char* commonCardStatus =
            commonCardConfigured
                ? "CONFIGURED"
                : "NOT_CONFIGURED";

        LOG_IF(
            "ACCESS",
            "Access control initialized: "
            "readers=%u protocols=%u "
            "endpoints=%u commonCard=%s",
            configuredReaders,
            configuredProtocols,
            configuredEndpoints,
            commonCardStatus
        );

        return true;
    }


    // ========================================================
    // CLIENT SETUP
    // ========================================================

    static bool SetupClient(
        const FrontendConfig::
            webApiDeviceMessaging::Client& cfg)
    {
        CleanupClient();

        // ====================================================
        // DISABLED
        // ====================================================

        if (!cfg.enabled)
        {
            LOG_IF(
                "WEBAPI-CLIENT",
                "HTTP client disabled"
            );

            return false;
        }

        // ====================================================
        // VALIDATION
        // ====================================================

        if (
            !cfg.devices ||
            cfg.deviceCount == 0
        )
        {
            LOG_WF(
                "WEBAPI-CLIENT",
                "HTTP client enabled without devices"
            );

            return false;
        }

        if (cfg.timeoutMs == 0)
        {
            LOG_EF(
                "WEBAPI-CLIENT",
                "Invalid client timeout"
            );

            return false;
        }

        // ====================================================
        // TRANSPORT
        // ====================================================

        clientTransport =
            new HttpClientTransport(
                cfg.timeoutMs
            );

        if (!clientTransport)
        {
            LOG_EF(
                "WEBAPI-CLIENT",
                "Unable to allocate HTTP client transport"
            );

            return false;
        }

        // ====================================================
        // ENGINE
        // ====================================================

        clientEngine =
            new DeviceMessageEngine();

        if (!clientEngine)
        {
            LOG_EF(
                "WEBAPI-CLIENT",
                "Unable to allocate client engine"
            );

            CleanupClient();

            return false;
        }

        // ====================================================
        // CONTEXT
        // ====================================================

        DeviceMessageEngine::Context context;

        context.devices =
            cfg.devices;

        context.deviceCount =
            cfg.deviceCount;

        context.transport =
            clientTransport;

        context.responseTimeoutMs =
            cfg.timeoutMs;

        // ====================================================
        // PLACEHOLDER RESOLVER
        //
        // DomoManagerOnlineClient gestisce:
        //
        //   ${controllerId}
        //   ${section}
        //
        // Gli altri profili attualmente non utilizzano
        // placeholder applicativi gestiti dal frontend.
        // ====================================================

        context.resolver =
        [](
            const WebApiDevice& device,
            const char* variable) -> String
        {
            if (
                !variable ||
                variable[0] == '\0'
            )
            {
                return "";
            }

            // ====================================================
            // DEVICE USERNAME
            // ====================================================

            if (
                strcmp(
                    variable,
                    "username"
                ) == 0
            )
            {
                return
                    device.credentials.username
                        ? String(
                            device.credentials.username
                        )
                        : String("");
            }

            // ====================================================
            // DEVICE PASSWORD
            // ====================================================

            if (
                strcmp(
                    variable,
                    "password"
                ) == 0
            )
            {
                return
                    device.credentials.password
                        ? String(
                            device.credentials.password
                        )
                        : String("");
            }

            // ====================================================
            // DOMOMANAGER ONLINE
            // ====================================================

            String resolvedValue;

            if (
                onlineClient.ResolvePlaceholder(
                    device,
                    variable,
                    resolvedValue
                )
            )
            {
                return resolvedValue;
            }

            // ====================================================
            // UNRESOLVED
            // ====================================================

            LOG_WF(
                "WEBAPI-CLIENT",
                "[%s] Unresolved placeholder: %s",
                device.name
                    ? device.name
                    : "",
                variable
            );

            return "";
        };

        // ====================================================
        // RESPONSE CALLBACK
        //
        // DomoManagerOnlineClient riceve per primo la risposta.
        //
        // Se HandleResponse() restituisce true, la risposta
        // appartiene al profilo DomoManager Online ed è già
        // stata elaborata.
        //
        // Negli altri casi viene eseguito il log generico per:
        //
        //   - Fanvil;
        //   - AxessTMC;
        //   - futuri profili WebAPI.
        // ====================================================

        context.onResponse =
        [](
            const WebApiDevice& device,
            const DeviceMessageProfile::OutMessage& message,
            const String& rawResponse,
            const ParsedResponse& parsed)
        {
            // ====================================================
            // DOMOMANAGER ONLINE
            // ====================================================

            if (
                onlineClient.HandleResponse(
                    device,
                    message,
                    rawResponse,
                    parsed
                )
            )
            {
                return;
            }

            // ====================================================
            // FRONTEND DEVICE RESPONSE
            //
            // Reolink e futuri device client vengono elaborati
            // dal frontend senza aggiungere dipendenze specifiche
            // dentro WebAPIEngine.
            // ====================================================

            if (deviceResponseCallback)
            {
                deviceResponseCallback(
                    device,
                    message,
                    rawResponse,
                    parsed
                );
            }

            // ==========================================
            // GENERIC RESPONSE LOG
            // ====================================================

            const char* deviceName =
                device.name
                    ? device.name
                    : "";

            const char* messageKey =
                message.key
                    ? message.key
                    : "";

            const unsigned responseLength =
                static_cast<unsigned>(
                    rawResponse.length()
                );

            switch (message.responseFormat)
            {
                // ====================================================
                // KEY VALUE
                // ====================================================

                case DeviceMessageProfile::
                    ResponseFormat::KEY_VALUE:
                {
                    LOG_DF(
                        "WEBAPI-CLIENT",
                        "[%s] message=%s "
                        "format=KEY_VALUE fields=%u bytes=%u",
                        deviceName,
                        messageKey,
                        static_cast<unsigned>(
                            parsed.fields.size()
                        ),
                        responseLength
                    );

                    break;
                }

                // ====================================================
                // JSON
                // ====================================================

                case DeviceMessageProfile::
                    ResponseFormat::JSON:
                {
                    LOG_DF(
                        "WEBAPI-CLIENT",
                        "[%s] message=%s "
                        "format=JSON valid=%u bytes=%u",
                        deviceName,
                        messageKey,
                        parsed.jsonValid
                            ? 1U
                            : 0U,
                        responseLength
                    );

                    break;
                }

                // ====================================================
                // RAW
                // ====================================================

                case DeviceMessageProfile::
                    ResponseFormat::RAW:
                {
                    LOG_DF(
                        "WEBAPI-CLIENT",
                        "[%s] message=%s "
                        "format=RAW bytes=%u",
                        deviceName,
                        messageKey,
                        responseLength
                    );

                    break;
                }

                // ====================================================
                // UNKNOWN FORMAT
                // ====================================================

                default:
                {
                    LOG_WF(
                        "WEBAPI-CLIENT",
                        "[%s] message=%s "
                        "unknown response format bytes=%u",
                        deviceName,
                        messageKey,
                        responseLength
                    );

                    break;
                }
            }
        };

        // ====================================================
        // DEVICE MESSAGE ENGINE INITIALIZATION
        // ====================================================

        if (
            !clientEngine->Init(
                context
            )
        )
        {
            LOG_EF(
                "WEBAPI-CLIENT",
                "Client engine initialization failed"
            );

            CleanupClient();

            return false;
        }

        // ====================================================
        // DOMOMANAGER ONLINE INITIALIZATION
        //
        // Per ora i parametri sono cablati qui.
        //
        // Nel passaggio successivo potranno essere spostati in:
        //
        //   FrontendConfig::webApiDeviceMessaging::Online
        //
        // ====================================================

        DomoManagerOnlineClient::Config onlineConfig;

        onlineConfig.enabled =
            true;

        onlineConfig.deviceName =
            "DomoManagerOnline";

        onlineConfig.controllerId =
            "opta-domotica";

        onlineConfig.allowEmptyWriteResponse =
            false;

        const bool onlineInitialized =
            onlineClient.Begin(
                *clientEngine,
                onlineConfig
            );

        if (!onlineInitialized)
        {
            /*
            * Il client WebAPI generico resta operativo anche
            * quando DomoManager Online non è configurato.
            */
            LOG_WF(
                "WEBAPI-CLIENT",
                "DomoManager Online not initialized"
            );
        }
        else
        {
            // ====================================================
            // VERSION CALLBACK
            // ====================================================

            onlineClient.SetVersionCallback(
                [](
                    const DomoManagerOnlineClient::
                        VersionInfo& version)
                {
                    LOG_IF(
                        "DOMO-ONLINE",
                        "Remote version: "
                        "schema=%lu revision=%lu checksum=%s",
                        static_cast<unsigned long>(
                            version.schemaVersion
                        ),
                        static_cast<unsigned long>(
                            version.revision
                        ),
                        version.checksum.c_str()
                    );
                }
            );

            // ====================================================
            // MANIFEST CALLBACK
            // ====================================================

            onlineClient.SetManifestCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Manifest received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );
                }
            );

            // ====================================================
            // FULL CONFIG CALLBACK
            // ====================================================

            onlineClient.SetFullConfigCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Full configuration received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );

                    /*
                    * Futura integrazione:
                    *
                    * DMConfigInterpreter::Load(
                    *     rawResponse
                    * );
                    */
                }
            );

            // ====================================================
            // SECTION CALLBACK
            // ====================================================

            onlineClient.SetSectionCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Configuration section received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );
                }
            );

            // ====================================================
            // JOBS CALLBACK
            // ====================================================

            onlineClient.SetJobsCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Jobs received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );

                    /*
                    * Futura integrazione:
                    *
                    * DmInterpreter::loadFromString(
                    *     rawResponse.c_str(),
                    *     scheduler,
                    *     manager,
                    *     registry
                    * );
                    */
                }
            );

            
            // ====================================================
            // ACK CALLBACK
            // ====================================================

            onlineClient.SetAckCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Configuration ACK received: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );
                }
            );

            // ====================================================
            // UPLOAD CALLBACK
            // ====================================================

            onlineClient.SetUploadCallback(
                [](
                    const char* messageKey,
                    const String& rawResponse,
                    const JsonDocument& document)
                {
                    (void) document;

                    LOG_IF(
                        "DOMO-ONLINE",
                        "Configuration upload completed: "
                        "message=%s bytes=%u",
                        messageKey
                            ? messageKey
                            : "",
                        static_cast<unsigned>(
                            rawResponse.length()
                        )
                    );
                }
            );

            // ====================================================
            // ERROR CALLBACK
            // ====================================================

            onlineClient.SetErrorCallback(
                [](
                    const char* messageKey,
                    const String& error)
                {
                    LOG_EF(
                        "DOMO-ONLINE",
                        "Request failed: "
                        "message=%s error=%s",
                        messageKey
                            ? messageKey
                            : "",
                        error.c_str()
                    );
                }
            );

            // ====================================================
            // TEMPORARY CONFIG_VERSION TEST
            // ====================================================

            if (!onlineClient.RequestVersion())
            {
                LOG_EF(
                    "DOMO-ONLINE",
                    "Unable to queue CONFIG_VERSION test"
                );
            }
            else
            {
                LOG_IF(
                    "DOMO-ONLINE",
                    "CONFIG_VERSION test queued"
                );
            }
            
        }

        // ====================================================
        // COMPLETE
        // ====================================================

        clientRunning =
            true;

        LOG_IF(
            "WEBAPI-CLIENT",
            "HTTP client initialized: "
            "devices=%u timeout=%lu online=%u",
            static_cast<unsigned>(
                cfg.deviceCount
            ),
            static_cast<unsigned long>(
                cfg.timeoutMs
            ),
            onlineClient.isInitialized()
                ? 1U
                : 0U
        );

        return true;
    }


    // ========================================================
    // SERVER SETUP
    //
    // Gli endpoint sono creati da SetupAccessControl().
    // Non vengono letti da FrontendConfig.
    // ========================================================

    static bool SetupServer(
        const FrontendConfig::
            webApiDeviceMessaging::Server& cfg)
    {
        CleanupServer();

        // ====================================================
        // DISABLED
        // ====================================================

        if (!cfg.enabled)
        {
            LOG_IF(
                "WEBAPI-SERVER",
                "HTTP server disabled"
            );

            return false;
        }

        // ====================================================
        // VALIDATION
        // ====================================================

        if (cfg.port == 0)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Invalid HTTP server port"
            );

            return false;
        }

        if (cfg.timeoutMs == 0)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Invalid HTTP server timeout"
            );

            return false;
        }

        if (serverEndpoints.empty())
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "No runtime HTTP endpoints registered"
            );

            return false;
        }

        // ====================================================
        // TRANSPORT
        // ====================================================

        serverTransport =
            new HttpServerTransport(
                cfg.port,
                cfg.timeoutMs
            );

        if (!serverTransport)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Unable to allocate HTTP server transport"
            );

            return false;
        }

        // ====================================================
        // SERVER ENGINE
        // ====================================================

        serverEngine =
            new WebApiServerEngine();

        if (!serverEngine)
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Unable to allocate server engine"
            );

            CleanupServer();

            return false;
        }

        // ====================================================
        // SERVER CONTEXT
        // ====================================================

        WebApiServerEngine::Context
            context;

        context.transport =
            serverTransport;

        /*
        * La registrazione degli endpoint deve essere conclusa
        * prima di questa assegnazione.
        *
        * Dopo Init() non si devono aggiungere o rimuovere endpoint
        * finché il server è attivo, perché una riallocazione del
        * vector invaliderebbe il puntatore restituito da data().
        */
        context.endpoints =
            serverEndpoints.data();

        context.endpointCount =
            serverEndpoints.size();

        // ====================================================
        // INITIALIZATION
        // ====================================================

        if (
            !serverEngine->Init(
                context
            )
        )
        {
            LOG_EF(
                "WEBAPI-SERVER",
                "Server engine initialization failed"
            );

            CleanupServer();

            return false;
        }

        serverRunning =
            true;

        const unsigned configuredPort =
            static_cast<unsigned>(
                cfg.port
            );

        const unsigned configuredEndpoints =
            static_cast<unsigned>(
                serverEndpoints.size()
            );

        const unsigned long configuredTimeout =
            static_cast<unsigned long>(
                cfg.timeoutMs
            );

        LOG_IF(
            "WEBAPI-SERVER",
            "HTTP server initialized: "
            "port=%u endpoints=%u timeout=%lu",
            configuredPort,
            configuredEndpoints,
            configuredTimeout
        );

        return true;
    }


    // ========================================================
    // FRONTEND CALLBACKS
    // ========================================================

    static inline WebRequestCallback
        webRequestCallback =
            nullptr;

    static inline AccessEventCallback
        accessEventCallback =
            nullptr;

    static inline AccessAuthorizedCallback
        accessAuthorizedCallback =
            nullptr;

    static inline DeviceResponseCallback
        deviceResponseCallback =
            nullptr;
public:
   
    // ========================================================================
    // DOMOMANAGER ONLINE STATUS
    // ========================================================================

    static bool IsOnlineConfigReady()
    {
        return
            onlineClient.isInitialized();
    }


    // ========================================================================
    // REQUEST ONLINE CONFIG VERSION
    // ========================================================================

    static bool RequestOnlineConfigVersion()
    {
        return
            onlineClient.RequestVersion();
    }


    // ========================================================================
    // REQUEST ONLINE CONFIG MANIFEST
    // ========================================================================

    static bool RequestOnlineConfigManifest()
    {
        return
            onlineClient.RequestManifest();
    }


    // ========================================================================
    // REQUEST FULL ONLINE CONFIG
    // ========================================================================

    static bool RequestFullOnlineConfig()
    {
        return
            onlineClient.RequestFullConfig();
    }


    // ========================================================================
    // REQUEST ONLINE CONFIG SECTION
    // ========================================================================

    static bool RequestOnlineConfigSection(
        const char* section)
    {
        return
            onlineClient.RequestSection(
                section
            );
    }


    // ========================================================================
    // REQUEST ONLINE JOBS
    // ========================================================================

    static bool RequestOnlineJobs()
    {
        return
            onlineClient.RequestJobs();
    }


    // ========================================================================
    // SEND ONLINE CONFIG ACK
    // ========================================================================

    static bool SendOnlineConfigAck(
        uint32_t revision,
        const char* status,
        bool rebootRequired,
        const char* detail = nullptr)
    {
        return
            onlineClient.SendAck(
                revision,
                status,
                rebootRequired,
                detail
            );
    }


    // ========================================================================
    // UPLOAD ONLINE CONFIG
    // ========================================================================

    static bool UploadOnlineConfig(
        const JsonDocument& document)
    {
        return
            onlineClient.UploadConfig(
                document
            );
    }

    // ========================================================
    // FRONTEND CALLBACK SETUP
    // ========================================================

    static void SetWebRequestCallback(
        const WebRequestCallback& callback)
    {
        webRequestCallback =
            callback;
    }


    static void SetAccessEventCallback(
        const AccessEventCallback& callback)
    {
        accessEventCallback =
            callback;
    }


    static void SetAccessAuthorizedCallback(
        const AccessAuthorizedCallback& callback)
    {
        accessAuthorizedCallback =
            callback;
    }

    static void SetDeviceResponseCallback(
        const DeviceResponseCallback& callback)
    {
        deviceResponseCallback =
            callback;
    }

    // ========================================================
    // SETUP
    //
    // WebAPI e Access sono configurazioni separate.
    // ========================================================

    static void Setup(
        const FrontendConfig::
            webApiDeviceMessaging& webApiCfg,
        const FrontendConfig::Access& accessCfg)
    {
        // ====================================================
        // CLEANUP PREVIOUS RUNTIME
        // ====================================================

        CleanupClient();
        CleanupServer();
        CleanupAccessControl();

        configuration =
            &webApiCfg;

        // ====================================================
        // COMPLETELY DISABLED
        // ====================================================

        if (
            !webApiCfg.client.enabled &&
            !webApiCfg.server.enabled &&
            !accessCfg.enabled
        )
        {
            LOG_IF(
                "WEBAPI",
                "WebAPI and access control disabled"
            );

            return;
        }

        // ====================================================
        // CONFIGURATION CONSISTENCY
        // ====================================================

        if (
            accessCfg.enabled &&
            !webApiCfg.server.enabled
        )
        {
            LOG_EF(
                "ACCESS",
                "Access control requires WebAPI server"
            );

            /*
             * Il client HTTP può continuare a funzionare.
             */
            if (webApiCfg.client.enabled)
            {
                SetupClient(
                    webApiCfg.client
                );
            }

            LOG_IF(
                "WEBAPI",
                "Setup completed: "
                "client=%s server=DISABLED "
                "access=DISABLED endpoints=0",
                clientRunning
                    ? "RUNNING"
                    : "DISABLED"
            );

            return;
        }

        // ====================================================
        // ACCESS CONTROL
        //
        // Deve precedere SetupServer perché registra
        // gli endpoint runtime.
        // ====================================================

        bool accessSetupResult =
            true;

        if (accessCfg.enabled)
        {
            accessSetupResult =
                SetupAccessControl(
                    accessCfg
                );

            if (!accessSetupResult)
            {
                LOG_EF(
                    "ACCESS",
                    "Access control initialization failed"
                );
            }
        }
        else
        {
            LOG_IF(
                "ACCESS",
                "Access control disabled"
            );
        }

        // ====================================================
        // HTTP CLIENT
        // ====================================================

        if (webApiCfg.client.enabled)
        {
            SetupClient(
                webApiCfg.client
            );
        }
        else
        {
            LOG_IF(
                "WEBAPI-CLIENT",
                "HTTP client disabled"
            );
        }

        // ====================================================
        // HTTP SERVER
        // ====================================================

        if (webApiCfg.server.enabled)
        {
            if (
                accessCfg.enabled &&
                !accessSetupResult
            )
            {
                LOG_EF(
                    "WEBAPI-SERVER",
                    "HTTP server not started because "
                    "access setup failed"
                );
            }
            else if (
                !SetupServer(
                    webApiCfg.server
                )
            )
            {
                CleanupAccessControl();
            }
        }
        else
        {
            LOG_IF(
                "WEBAPI-SERVER",
                "HTTP server disabled"
            );
        }

        // ====================================================
        // RESULT
        // ====================================================

        const char* clientStatus =
            clientRunning
                ? "RUNNING"
                : "DISABLED";

        const char* serverStatus =
            serverRunning
                ? "RUNNING"
                : "DISABLED";

        const char* accessStatus =
            accessRunning
                ? "RUNNING"
                : "DISABLED";

        const unsigned endpointCount =
            static_cast<unsigned>(
                serverEndpoints.size()
            );

        LOG_IF(
            "WEBAPI",
            "Setup completed: "
            "client=%s server=%s access=%s endpoints=%u",
            clientStatus,
            serverStatus,
            accessStatus,
            endpointCount
        );
    }


    // ========================================================
    // LOOP
    // ========================================================

    static void Loop(
        unsigned long now)
    {
        if (!configuration)
            return;

        if (
            clientRunning &&
            clientEngine
        )
        {
            clientEngine->Loop(
                now
            );
        }

        if (
            serverRunning &&
            serverEngine
        )
        {
            serverEngine->Loop(
                now
            );
        }
    }


    // ========================================================
    // CLIENT COMMAND
    // ========================================================

    static bool Send(
        const char* deviceName,
        const char* messageKey,
        const String& body = "")
    {
        if (
            !clientRunning ||
            !clientEngine
        )
        {
            return false;
        }

        return clientEngine->Send(
            deviceName,
            messageKey,
            body
        );
    }


    // ========================================================
    // CLIENT ACCESSORS
    // ========================================================

    static DeviceMessageEngine*
    GetClientEngine()
    {
        return clientEngine;
    }


    static HttpClientTransport*
    GetClientTransport()
    {
        return clientTransport;
    }


    // ========================================================
    // SERVER ACCESSORS
    // ========================================================

    static WebApiServerEngine*
    GetServerEngine()
    {
        return serverEngine;
    }


    static HttpServerTransport*
    GetServerTransport()
    {
        return serverTransport;
    }


    static size_t GetServerEndpointCount()
    {
        return
            serverEndpoints.size();
    }


    // ========================================================
    // ACCESS CONTROL ACCESSORS
    // ========================================================

    static AccessControlService*
    GetAccessControl()
    {
        return accessControl;
    }


    static size_t GetAccessProtocolCount()
    {
        return accessProtocols.size();
    }


    static IAccessWebAPI*
        GetAccessProtocol(
            size_t index)
        {
            if (
                index >=
                accessProtocols.size()
            )
            {
                return nullptr;
            }

            return accessProtocols[index];
        }


    static IAccessWebAPI*
        FindAccessProtocol(
            const char* protocolId)
        {
            if (!protocolId)
                return nullptr;

            for (
                IAccessWebAPI* protocol :
                accessProtocols
            )
            {
                if (
                    protocol &&
                    protocol->protocolId() &&
                    strcmp(
                        protocol->protocolId(),
                        protocolId
                    ) == 0
                )
                {
                    return protocol;
                }
            }

            return nullptr;
        }


    // ========================================================
    // COMPATIBILITY
    // ========================================================

    static DeviceMessageEngine*
    GetInstance()
    {
        return clientEngine;
    }


    // ========================================================
    // STATUS
    // ========================================================

    static bool IsClientRunning()
    {
        return clientRunning;
    }


    static bool IsServerRunning()
    {
        return serverRunning;
    }


    static bool IsAccessRunning()
    {
        return accessRunning;
    }


    static bool IsRunning()
    {
        /*
         * AccessControlService è event-driven e viene
         * eseguito tramite il server HTTP.
         */

        return
            clientRunning ||
            serverRunning;
    }


    // ========================================================
    // SHUTDOWN
    // ========================================================

    static void Shutdown()
    {
        Cleanup();

        webRequestCallback =
            nullptr;

        accessEventCallback =
            nullptr;

        accessAuthorizedCallback =
            nullptr;

        deviceResponseCallback =
            nullptr;
    }
};

// ============================================================
// HMI ENGINE
//
// Responsabilità:
//   1. Gestione connessioni Modbus TCP HMI
//   2. Poll dei client
//   3. Sincronizzazione Buffer <-> HMI
//
// Le modifiche HMI finiscono nel Buffer e seguono
// il normale percorso DomoManager -> NetworkManager -> RTU.
// ============================================================


class HMIEngine
{
public:

    // ========================================================
    // CONFIG
    // ========================================================

    struct Config
    {
        bool enabled;
        uint16_t port;

        Config()
            : enabled(false),
              port(502)
        {
        }
    };


    // ========================================================
    // EVENT -> HMI
    // ========================================================

    void PushEvent(
        const EventManager::Event& event)
    {
        if (!_config.enabled)
            return;

        if (!_running)
            return;


        const int area =
            event.area;


        // ----------------------------------------------------
        // VALIDATE AREA
        // ----------------------------------------------------

        if (area < 0 ||
            area >= static_cast<int>(_registerCount))
        {
            LOG_WF(
                "HMIEngine",
                "PushEvent ignored: invalid area=%d "
                "registerCount=%u",
                area,
                (unsigned)_registerCount
            );

            return;
        }


        const uint16_t value =
            static_cast<uint16_t>(
                event.value
            );


        // ====================================================
        // PROPAGATE EVENT TO ALL CONNECTED HMI CLIENTS
        // ====================================================

        for (uint8_t client = 0;
             client < _maxClients;
             ++client)
        {
            DMPlatform::Network::TCPClient& tcpClient =
                _context.clients[client];


            if (!tcpClient)
                continue;


            if (!tcpClient.connected())
                continue;


            DMPlatform::Modbus::TCPServer& server =
                _context.modbusServers[client];


            // ------------------------------------------------
            // ArduinoModbus:
            //
            // 1 = SUCCESS
            // 0 = FAILURE
            // ------------------------------------------------

            const int result =
                DMPlatform::Modbus::serverHoldingRegisterWrite(
                    server,
                    area,
                    value
                );


            if (result != 1)
            {
                LOG_WF(
                    "HMIEngine",
                    "PushEvent FAIL: "
                    "client=%u area=%d value=%u result=%d",
                    (unsigned)client,
                    area,
                    (unsigned)value,
                    result
                );

                continue;
            }


            // ------------------------------------------------
            // Allinea shadow alla scrittura interna.
            //
            // Evita che SyncHMIToBuffer() interpreti questa
            // scrittura come modifica proveniente dall'HMI.
            // ------------------------------------------------

            _registerShadow[client][area] =
                value;
        }
    }


private:

    // ========================================================
    // STATE
    // ========================================================

    Config _config;

    uint8_t _maxClients = 0;

    uint8_t _activeClients = 0;

    bool _loopEnabled = false;

    bool _running = false;


    // ========================================================
    // PROTOCOL
    // ========================================================

    DMNetworkManager::ProtocolId _protocolId = -1;


    // ========================================================
    // PENDING HMI UPDATE
    // ========================================================
    //
    // Conservato dal modello originale.
    //
    // Attualmente non viene prodotto da SyncHMIToBuffer(),
    // ma rimane disponibile per eventuali percorsi futuri.
    //
    // ========================================================

    struct PendingHMIUpdate
    {
        int area;
        long value;
        uint8_t client;
    };


    std::vector<
        PendingHMIUpdate
    > pendingUpdates;


    // ========================================================
    // HOLDING REGISTER SHADOW
    // ========================================================

    uint16_t _registerCount = 0;

    std::vector<
        std::vector<uint16_t>
    > _registerShadow;


    // ========================================================
    // EVENT SOURCE
    // ========================================================

    uint8_t _eventSource = 0;


    // ========================================================
    // NETWORK MANAGER
    // ========================================================

    DMNetworkManager* _networkManager = nullptr;


    // ========================================================
    // SOCKET OWNER
    // ========================================================

    SocketManager::OwnerId _socketOwner = -1;


    // ========================================================
    // SOCKET RESOURCE IDS
    // ========================================================
    //
    // Sono resource ID LOGICI del SocketManager.
    //
    // ========================================================

    static constexpr int SOCKET_RESOURCE_SERVER =
        0;


    static constexpr int SOCKET_RESOURCE_CLIENT_BASE =
        2000;


    // ========================================================
    // RESOURCE STATE
    // ========================================================

    bool _serverResourceAcquired = false;


    // ========================================================
    // NETWORK CONTEXT
    // ========================================================
    //
    // Nessun riferimento diretto a EthernetServer /
    // EthernetClient.
    //
    // Tutto passa da DMPlatform::Network.
    //
    // ========================================================

    struct Context
    {
        DMPlatform::Network::TCPServer* server = nullptr;

        std::vector<
            DMPlatform::Network::TCPClient
        > clients;

        std::vector<
            DMPlatform::Modbus::TCPServer
        > modbusServers;
    };


    Context _context;


    // ========================================================
    // RESOURCE ID HELPERS
    // ========================================================

    
    static int clientResourceId(
        uint8_t index)
    {
        return
            SOCKET_RESOURCE_CLIENT_BASE +
            static_cast<int>(index);
    }


    // ========================================================
    // SOCKET ACQUIRE
    // ========================================================

    bool acquireServerSocket()
    {
        if (!_networkManager)
        {
            LOG_EF(
                "HMIEngine",
                "NetworkManager unavailable"
            );

            return false;
        }


        if (_socketOwner < 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI socket owner unavailable"
            );

            return false;
        }


        const int slot =
            _networkManager
                ->sockets()
                .acquire(
                    _socketOwner,
                    SOCKET_RESOURCE_SERVER,
                    SocketManager::SocketKind::TCP_SERVER
                );


        if (slot < 0)
        {
            LOG_WF(
                "HMIEngine",
                "HMI TCP listener resource unavailable"
            );

            return false;
        }


        _serverResourceAcquired =
            true;


        return true;
    }


  

    // ========================================================
    // CLIENT RESOURCE ACQUIRE
    // ========================================================

    bool acquireClientSocket(
        uint8_t index)
    {
        if (!_networkManager)
        {
            LOG_EF(
                "HMIEngine",
                "NetworkManager unavailable"
            );

            return false;
        }


        if (_socketOwner < 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI socket owner unavailable"
            );

            return false;
        }


        const int resourceId =
            clientResourceId(index);


        const int slot =
            _networkManager
                ->sockets()
                .acquire(
                    _socketOwner,
                    resourceId,
                    SocketManager::SocketKind::TCP_CLIENT
                );


        if (slot < 0)
        {
            LOG_WF(
                "HMIEngine",
                "HMI client resource unavailable: "
                "client=%u resource=%d",
                (unsigned)index,
                resourceId
            );

            return false;
        }


        return true;
    }


    // ========================================================
    // SERVER RESOURCE RELEASE
    // ========================================================

    void releaseServerSocket()
    {
        if (!_networkManager)
            return;


        if (_socketOwner < 0)
            return;


        if (!_serverResourceAcquired)
            return;


        _networkManager
            ->sockets()
            .release(
                _socketOwner,
                SOCKET_RESOURCE_SERVER,
                SocketManager::SocketKind::TCP_SERVER
            );


        _serverResourceAcquired =
            false;
    }

    // ========================================================
    // CLIENT RESOURCE RELEASE
    // ========================================================

    void releaseClientSocket(
        uint8_t index)
    {
        if (!_networkManager)
            return;


        if (_socketOwner < 0)
            return;


        _networkManager
            ->sockets()
            .release(
                _socketOwner,
                clientResourceId(index),
                SocketManager::SocketKind::TCP_CLIENT
            );
    }


    // ========================================================
    // RELEASE ALL HMI RESOURCES
    // ========================================================

    void releaseAllSockets()
    {
        if (!_networkManager)
            return;


        if (_socketOwner < 0)
            return;


        _networkManager
            ->sockets()
            .releaseOwner(
                _socketOwner
            );


        _serverResourceAcquired =
            false;

    }


    // ========================================================
    // RESET REGISTER SHADOW
    // ========================================================

    void resetRegisterShadow()
    {
        _registerShadow.clear();


        _registerShadow.resize(
            _maxClients
        );


        for (uint8_t client = 0;
             client < _maxClients;
             ++client)
        {
            _registerShadow[client].assign(
                _registerCount,
                0
            );
        }
    }


    // ========================================================
    // DESTROY TCP SERVER OBJECT
    // ========================================================
    //
    // Il lifecycle del transport è incapsulato in:
    //
    //     DMPlatform::Network
    //
    // ========================================================

    void destroyServerObject()
    {
        if (_context.server == nullptr)
            return;

        DMPlatform::Network::TCPServer* server =
            _context.server;

        _context.server = nullptr;

        DMPlatform::Network::DestroyTCPServer(server);
    }


    // ========================================================
    // CREATE TCP SERVER
    //
    // Ricrea sempre il listener TCP.
    //
    // Questo rende Setup() idempotente e impedisce che un
    // precedente oggetto server blocchi la reinizializzazione.
    // ========================================================

    bool createServer()
    {
        // ====================================================
        // CLEANUP PREVIOUS SERVER
        // ====================================================

        if (_context.server != nullptr)
        {
            LOG_WF(
                "HMIEngine",
                "Existing HMI TCP server found: recreating"
            );

            destroyServerObject();
        }

        // ====================================================
        // CREATE SERVER
        // ====================================================

        _context.server =
            DMPlatform::Network::CreateTCPServer(
                _config.port
            );

        if (_context.server == nullptr)
        {
            LOG_EF(
                "HMIEngine",
                "Unable to create TCP server: port=%u",
                static_cast<unsigned>(
                    _config.port
                )
            );

            return false;
        }

        // ====================================================
        // START LISTENER
        // ====================================================

        if (
            !DMPlatform::Network::StartTCPServer(
                *_context.server
            )
        )
        {
            LOG_EF(
                "HMIEngine",
                "Unable to start HMI TCP listener: port=%u",
                static_cast<unsigned>(
                    _config.port
                )
            );

            destroyServerObject();

            return false;
        }

        LOG_IF(
            "HMIEngine",
            "HMI TCP listener started: port=%u",
            static_cast<unsigned>(
                _config.port
            )
        );

        return true;
    }


    // ========================================================
    // CLEANUP MODBUS SERVERS
    // ========================================================

    void cleanupModbusServers(
        uint8_t count)
    {
        const uint8_t available =
            static_cast<uint8_t>(
                _context.modbusServers.size()
            );


        if (count > available)
            count = available;


        for (uint8_t i = 0;
             i < count;
             ++i)
        {
            _context.modbusServers[i].end();
        }
    }


    // ========================================================
    // INITIALIZE MODBUS SERVERS
    // ========================================================

    bool initializeModbusServers()
    {
        uint8_t initializedServers = 0;


        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            DMPlatform::Modbus::TCPServer& server =
                _context.modbusServers[i];


            // =================================================
            // MODBUS BEGIN
            // =================================================
            //
            // ArduinoModbus:
            //
            //     1 = success
            //     0 = failure
            //
            // =================================================

            const int beginResult =
                server.begin();


            if (beginResult != 1)
            {
                LOG_EF(
                    "HMIEngine",
                    "Modbus TCP server initialization failed: "
                    "index=%u result=%d",
                    (unsigned)i,
                    beginResult
                );


                cleanupModbusServers(
                    initializedServers
                );


                return false;
            }


            // =================================================
            // HOLDING REGISTERS
            // =================================================
            //
            // ArduinoModbus implementation used on OPTA:
            //
            //     1 = success
            //     0 = failure
            //
            // =================================================

            const int result =
                DMPlatform::Modbus::serverConfigureHoldingRegisters(
                    server,
                    0,
                    static_cast<int>(_registerCount)
                );


            if (result != 1)
            {
                LOG_EF(
                    "HMIEngine",
                    "Holding register configuration failed: "
                    "index=%u count=%u result=%d",
                    (unsigned)i,
                    (unsigned)_registerCount,
                    result
                );


                server.end();


                cleanupModbusServers(
                    initializedServers
                );


                return false;
            }


            ++initializedServers;
        }


        LOG_IF(
            "HMIEngine",
            "HMI Modbus contexts initialized: count=%u",
            (unsigned)initializedServers
        );


        return true;
    }


    // ========================================================
    // SETUP SERVER
    //
    // Sequenza:
    //   1. cleanup eventuale runtime precedente
    //   2. inizializzazione slot client
    //   3. prenotazione listener
    //   4. creazione server TCP
    //   5. inizializzazione contesti Modbus
    // ========================================================

    bool SetupServer()
    {
        // ====================================================
        // CLEANUP PREVIOUS RUNTIME
        // ====================================================

        cleanupModbusServers(
            static_cast<uint8_t>(
                _context.modbusServers.size()
            )
        );

        for (
            size_t i = 0;
            i < _context.clients.size();
            ++i
        )
        {
            DMPlatform::Network::TCPClient& client =
                _context.clients[i];

            if (client)
            {
                DMPlatform::Network::Stop(
                    client
                );
            }
        }

        destroyServerObject();
        releaseAllSockets();

        // ====================================================
        // RESET STATE
        // ====================================================

        _activeClients = 0;
        _serverResourceAcquired = false;

        _context.clients.clear();
        _context.modbusServers.clear();

        // ====================================================
        // LOGICAL CLIENT SLOTS
        // ====================================================

        _context.clients.resize(
            _maxClients
        );

        _context.modbusServers.resize(
            _maxClients
        );

        // ====================================================
        // SERVER RESOURCE
        // ====================================================

        if (!acquireServerSocket())
        {
            LOG_WF(
                "HMIEngine",
                "Unable to reserve HMI TCP listener resource"
            );

            return false;
        }

        // ====================================================
        // PHYSICAL TCP SERVER
        // ====================================================

        if (!createServer())
        {
            LOG_WF(
                "HMIEngine",
                "Unable to create HMI TCP listener"
            );

            releaseServerSocket();

            return false;
        }

        // ====================================================
        // MODBUS SERVER CONTEXTS
        // ====================================================

        if (!initializeModbusServers())
        {
            LOG_EF(
                "HMIEngine",
                "Unable to initialize HMI Modbus contexts"
            );

            cleanupModbusServers(
                _maxClients
            );

            destroyServerObject();
            releaseServerSocket();

            return false;
        }

        // ====================================================
        // SUCCESS
        // ====================================================

        const unsigned socketDemand =
            1U +
            static_cast<unsigned>(
                _maxClients
            );

        LOG_IF(
            "HMIEngine",
            "HMI Modbus TCP started: "
            "port=%u maxClients=%u registers=%u "
            "socketDemand=%u",
            static_cast<unsigned>(
                _config.port
            ),
            static_cast<unsigned>(
                _maxClients
            ),
            static_cast<unsigned>(
                _registerCount
            ),
            socketDemand
        );

        if (_networkManager)
        {
            _networkManager
                ->sockets()
                .dump();
        }

        return true;
    }


    // ========================================================
    // CLEANUP DISCONNECTED CLIENTS
    // ========================================================

    void cleanupDisconnectedClients()
    {
        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            DMPlatform::Network::TCPClient& client =
                _context.clients[i];


            if (!client)
                continue;


            if (client.connected())
                continue;


            LOG_IF(
                "HMIEngine",
                "HMI client %u disconnected",
                (unsigned)i
            );


            DMPlatform::Network::Stop(
                client
            );


            releaseClientSocket(
                i
            );


            if (_activeClients > 0)
            {
                --_activeClients;
            }
        }
    }


    // ========================================================
    // ACCEPT NEW CLIENT
    //
    // Accetta il client, lo associa al server Modbus e copia
    // immediatamente nei registri holding l'intero stato del
    // buffer DomoManager.
    // ========================================================

    void acceptNewClient(
        DomoManager& manager)
    {
        if (_context.server == nullptr)
            return;

        DMPlatform::Network::TCPClient newClient =
            DMPlatform::Network::AcceptTCPClient(
                *_context.server
            );

        if (!newClient)
            return;

        for (uint8_t i = 0;
            i < _maxClients;
            ++i)
        {
            DMPlatform::Network::TCPClient& slot =
                _context.clients[i];

            if (slot &&
                slot.connected())
            {
                continue;
            }

            if (!acquireClientSocket(i))
            {
                LOG_WF(
                    "HMIEngine",
                    "HMI client rejected: "
                    "no socket resource, client=%u",
                    static_cast<unsigned>(i)
                );

                DMPlatform::Network::Stop(
                    newClient
                );

                return;
            }

            slot =
                newClient;

            _context.modbusServers[i].accept(
                slot
            );

            ++_activeClients;

            LOG_IF(
                "HMIEngine",
                "HMI client %u connected - active=%u",
                static_cast<unsigned>(i),
                static_cast<unsigned>(_activeClients)
            );

            /*
            * Sincronizzazione iniziale obbligatoria.
            *
            * Gli eventi generati prima della connessione del
            * pannello non sono più presenti nella coda, ma i loro
            * valori correnti sono conservati nel Buffer.
            */
            if (!syncBufferToClient(
                    manager,
                    i))
            {
                LOG_WF(
                    "HMIEngine",
                    "Initial buffer sync incomplete: client=%u",
                    static_cast<unsigned>(i)
                );
            }

            return;
        }

        LOG_WF(
            "HMIEngine",
            "HMI connection rejected: logical slots full"
        );

        DMPlatform::Network::Stop(
            newClient
        );
    }


    // ========================================================
    // CONNECTION MANAGEMENT
    // ========================================================

    void ProcessConnections(
        DomoManager& manager)
    {
        cleanupDisconnectedClients();

        acceptNewClient(
            manager
        );
    }


    // ========================================================
    // MODBUS POLL
    // ========================================================

    void Poll()
    {
        for (uint8_t i = 0;
             i < _maxClients;
             ++i)
        {
            DMPlatform::Network::TCPClient& client =
                _context.clients[i];


            if (!client)
                continue;


            if (!client.connected())
                continue;


            _context.modbusServers[i].poll();
        }
    }


    // ========================================================
    // PENDING UPDATE CONSUMER
    // ========================================================

    bool consumePendingUpdate(
        uint8_t client,
        int area,
        long value)
    {
        for (auto it =
                 pendingUpdates.begin();
             it != pendingUpdates.end();
             ++it)
        {
            if (it->client == client &&
                it->area == area &&
                it->value == value)
            {
                pendingUpdates.erase(
                    it
                );

                return true;
            }
        }


        return false;
    }


    // ========================================================
    // HMI -> EVENT MANAGER
    // ========================================================

    void SyncHMIToBuffer(
        DomoManager& manager,
        unsigned long now)
    {
        (void)now;


        // ----------------------------------------------------
        // La rilevazione della modifica HMI è indipendente
        // dalla coda corrente dell'EventManager.
        // ----------------------------------------------------

        for (uint8_t client = 0;
             client < _maxClients;
             ++client)
        {
            DMPlatform::Network::TCPClient& tcpClient =
                _context.clients[client];


            if (!tcpClient)
                continue;


            if (!tcpClient.connected())
                continue;


            DMPlatform::Modbus::TCPServer& server =
                _context.modbusServers[client];


            for (uint16_t area = 0;
                 area < _registerCount;
                 ++area)
            {
                const long rawValue =
                    DMPlatform::Modbus::serverHoldingRegisterRead(
                        server,
                        static_cast<int>(area)
                    );


                // ------------------------------------------------
                // -1 = failure
                // >=0 = valid value
                // ------------------------------------------------

                if (rawValue < 0)
                    continue;


                const uint16_t value =
                    static_cast<uint16_t>(
                        rawValue
                    );


                // ------------------------------------------------
                // Nessuna modifica.
                // ------------------------------------------------

                if (_registerShadow[client][area] ==
                    value)
                {
                    continue;
                }


                // ------------------------------------------------
                // Aggiorna prima la shadow.
                // ------------------------------------------------

                _registerShadow[client][area] =
                    value;


                // ------------------------------------------------
                // HMI -> EventManager.
                // ------------------------------------------------
                
                manager.forceEvent(
                    static_cast<int>(area),
                    value,
                    static_cast<uint8_t>(_eventSource)
                );
            }
        }
    }

    // ========================================================
    // SYNC BUFFER TO CLIENT
    //
    // Copia lo stato corrente del Buffer nei registri holding
    // del client HMI appena connesso.
    //
    // Serve perché gli eventi generati prima della connessione
    // del client non possono essere ricevuti retroattivamente.
    // ========================================================

    bool syncBufferToClient(
        DomoManager& manager,
        uint8_t client)
    {
        if (client >= _maxClients)
        {
            LOG_EF(
                "HMIEngine",
                "Initial sync failed: invalid client=%u",
                static_cast<unsigned>(client)
            );

            return false;
        }

        if (client >= _context.clients.size() ||
            client >= _context.modbusServers.size() ||
            client >= _registerShadow.size())
        {
            LOG_EF(
                "HMIEngine",
                "Initial sync failed: "
                "client structures unavailable, client=%u",
                static_cast<unsigned>(client)
            );

            return false;
        }

        DMPlatform::Network::TCPClient& tcpClient =
            _context.clients[client];

        if (!tcpClient ||
            !tcpClient.connected())
        {
            LOG_WF(
                "HMIEngine",
                "Initial sync skipped: "
                "client=%u not connected",
                static_cast<unsigned>(client)
            );

            return false;
        }

        DMPlatform::Modbus::TCPServer& server =
            _context.modbusServers[client];

        Buffer& buffer =
            manager.getBuffer();

        uint16_t synchronized = 0;
        uint16_t failed = 0;

        for (uint16_t area = 0;
            area < _registerCount;
            ++area)
        {
            const long rawValue =
                buffer.getValueFast(
                    static_cast<int>(area)
                );

            /*
            * I registri HMI sono uint16_t.
            *
            * Il cast conserva la rappresentazione binaria anche
            * per bitmask con bit 15 attivo.
            */
            const uint16_t value =
                static_cast<uint16_t>(
                    rawValue
                );

            const int result =
                DMPlatform::Modbus::
                    serverHoldingRegisterWrite(
                        server,
                        static_cast<int>(area),
                        value
                    );

            if (result != 1)
            {
                ++failed;

                LOG_WF(
                    "HMIEngine",
                    "Initial sync write failed: "
                    "client=%u area=%u value=%u result=%d",
                    static_cast<unsigned>(client),
                    static_cast<unsigned>(area),
                    static_cast<unsigned>(value),
                    result
                );

                continue;
            }

            /*
            * Lo shadow deve essere identico al registro appena
            * scritto, altrimenti SyncHMIToBuffer() interpreterebbe
            * la sincronizzazione iniziale come comando proveniente
            * dal pannello.
            */
            _registerShadow[client][area] =
                value;

            ++synchronized;
        }

        LOG_IF(
            "HMIEngine",
            "Initial buffer sync completed: "
            "client=%u synchronized=%u failed=%u",
            static_cast<unsigned>(client),
            static_cast<unsigned>(synchronized),
            static_cast<unsigned>(failed)
        );

        return failed == 0;
    }
public:

    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    HMIEngine()
        : _config(),
          _maxClients(0),
          _activeClients(0),
          _loopEnabled(false),
          _running(false),
          _protocolId(-1),
          _registerCount(0),
          _eventSource(0),
          _networkManager(nullptr),
          _socketOwner(-1),
          _serverResourceAcquired(false),
          _context()
    {
    }


    // ========================================================
    // SETUP
    // ========================================================

    void Setup(
        const DomoManagerConfig::HMI& cfg,
        DMNetworkManager& networkManager,
        SocketManager::OwnerId socketOwner,
        DMNetworkManager::ProtocolId protocol,
        uint8_t maxClients,
        uint16_t registerCount,
        uint8_t eventSource)
    {
        // ----------------------------------------------------
        // Network references
        // ----------------------------------------------------

        _networkManager =
            &networkManager;


        _socketOwner =
            socketOwner;


        _protocolId =
            protocol;


        // ----------------------------------------------------
        // Configuration
        // ----------------------------------------------------

        _config.enabled =
            cfg.enabled;


        _config.port =
            cfg.port;


        _maxClients =
            maxClients;


        _registerCount =
            registerCount;


        _eventSource =
            eventSource;


        // ----------------------------------------------------
        // Runtime state
        // ----------------------------------------------------

        _activeClients =
            0;


        _loopEnabled =
            false;


        _running =
            false;

        _serverResourceAcquired =
            false;


        // ----------------------------------------------------
        // Reset shadow
        // ----------------------------------------------------

        resetRegisterShadow();


        // ====================================================
        // DISABLED
        // ====================================================

        if (!_config.enabled)
        {
            LOG_IF(
                "HMIEngine",
                "HMI disabled"
            );

            return;
        }


        // ====================================================
        // VALIDATE MAX CLIENTS
        // ====================================================

        if (_maxClients == 0)
        {
            LOG_WF(
                "HMIEngine",
                "HMI enabled but maxClients=0"
            );

            return;
        }


        // ====================================================
        // VALIDATE REGISTER COUNT
        // ====================================================

        if (_registerCount == 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI enabled but registerCount=0"
            );

            return;
        }


        // ====================================================
        // VALIDATE PORT
        // ====================================================

        if (_config.port == 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI enabled but TCP port=0"
            );

            return;
        }


        // ====================================================
        // VALIDATE SOCKET OWNER
        // ====================================================

        if (_socketOwner < 0)
        {
            LOG_EF(
                "HMIEngine",
                "HMI socket owner is invalid"
            );

            return;
        }


        // ====================================================
        // SETUP
        // ====================================================

        _running =
            SetupServer();


        if (!_running)
        {
            LOG_WF(
                "HMIEngine",
                "HMI network resources unavailable - "
                "engine remains disabled"
            );

            return;
        }
    }


    // ========================================================
    // LOOP ENABLE
    // ========================================================

    void SetLoopEnabled(
        bool enabled)
    {
        if (!_config.enabled)
            return;


        if (!_running)
            return;


        if (_loopEnabled == enabled)
            return;


        _loopEnabled =
            enabled;


        LOG_IF(
            "HMIEngine",
            "HMI loop %s",
            _loopEnabled
                ? "ENABLED"
                : "DISABLED"
        );
    }


    // ========================================================
    // NETWORK LOOP
    // ========================================================

    void ProcessNetwork(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;
        (void)now;

        if (!_config.enabled)
            return;

        if (!_running)
            return;

        if (!_loopEnabled)
            return;

        ProcessConnections(
            manager
        );

        Poll();
    }


    // ========================================================
    // SYNC LOOP
    // ========================================================

    void Sync(
        DomoManager& manager,
        unsigned long now)
    {
        if (!_config.enabled)
            return;


        if (!_running)
            return;


        if (!_loopEnabled)
            return;


        SyncHMIToBuffer(
            manager,
            now
        );
    }


    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    bool enabled() const
    {
        return _config.enabled;
    }


    bool running() const
    {
        return _running;
    }


    bool loopEnabled() const
    {
        return _loopEnabled;
    }


    uint16_t port() const
    {
        return _config.port;
    }


    uint8_t activeClients() const
    {
        return _activeClients;
    }


    uint8_t maxClients() const
    {
        return _maxClients;
    }


    uint16_t registerCount() const
    {
        return _registerCount;
    }


    DMNetworkManager::ProtocolId protocolId() const
    {
        return _protocolId;
    }


    SocketManager::OwnerId socketOwner() const
    {
        return _socketOwner;
    }
};

class SecurityEngine
{
private:

    // ============================================================
    // ALARM CALLBACK
    // ============================================================

    using AlarmCallback =
        void (*)(bool engaged,
                 bool active,
                 uint64_t currentAlarmMask,
                 uint64_t effectiveAlarmMask,
                 uint64_t systemBitmask);

    // ============================================================
    // ALARM CALLBACK STATE
    // ============================================================

    struct AlarmCallbackState
    {
        AlarmCallback callback;
        bool valid;
        bool engaged;
        bool active;
        uint64_t currentAlarmMask;
        uint64_t effectiveAlarmMask;
        uint64_t systemBitmask;
    };

    // ============================================================
    // ALARM SILENCE STATE
    // ============================================================

    struct AlarmSilenceState
    {
        bool valid;
        uint64_t lastMask;
    };

    // ============================================================
    // CONTEXT
    // ============================================================

    struct Context
    {
        DomoManager* manager;
        FrontendConfig::Security* config;
        uint8_t securitySource;
    };

    // ============================================================
    // RUNTIME
    // ============================================================

    struct RuntimeState
    {
        bool initialized;
        bool hmiInitialSyncPending;
        bool reportOnChange;
        uint8_t changeFlags;
    };

    // ============================================================
    // STATIC STATE
    // ============================================================

    static inline AlarmCallbackState alarmState;
    static inline AlarmSilenceState silenceState;
    static inline Context context;
    static inline RuntimeState runtime;

    // ============================================================
    // EVENT ENCODER
    // ============================================================

    static long EncodeEvent(const AlarmPanelInterface::Event& event)
    {
        const uint32_t type =
            static_cast<uint32_t>(event.type) & 0xFFu;

        const uint32_t zone =
            static_cast<uint32_t>(
                event.zone < 0 ? 0xFF : event.zone
            ) & 0xFFu;

        const uint32_t partition =
            static_cast<uint32_t>(
                event.partition < 0 ? 0xFF : event.partition
            ) & 0xFFu;

        return static_cast<long>(
            type |
            (zone << 8) |
            (partition << 16)
        );
    }

    // ============================================================
    // PUBLISH PANEL EVENT
    // ============================================================

    static void PublishPanelEvent(
        const AlarmPanelInterface::Event& event,
        bool alarmEvent)
    {
        (void)alarmEvent;

        if(!runtime.initialized ||
        !context.manager ||
        !context.config ||
        context.config->eventArea < 0 ||
        context.securitySource == 255)
        {
            return;
        }

        const long value =
            EncodeEvent(event);

        context.manager->getEventManager().push(
            context.config->eventArea,
            value,
            context.securitySource
        );

        runtime.changeFlags |=
            SecurityOrchestrator::CHANGE_PANEL;
    }

    // ============================================================
    // ALARM PANEL EVENT CALLBACKS
    // ============================================================

    static void AlarmPanelAlarmCallback(
        const AlarmPanelInterface::Event& event)
    {
        PublishPanelEvent(event, true);
    }

    static void AlarmPanelEventCallback(
        const AlarmPanelInterface::Event& event)
    {
        PublishPanelEvent(event, false);
    }

    // ============================================================
    // UPDATE SECURITY HMI
    // ============================================================


    static void UpdateSecurityHmi(bool force = false)
    {
        if(!runtime.initialized ||
        !context.manager ||
        !context.config)
        {
            return;
        }

        const uint8_t changes =
            runtime.changeFlags;

        if(!force &&
        changes == SecurityOrchestrator::CHANGE_NONE)
        {
            return;
        }

        DomoManager* dm =
            context.manager;

        FrontendConfig::Security* cfg =
            context.config;

        auto& buffer =
            dm->getBuffer();

        auto& hmi =
            SecurityHmiInterface::instance();

        auto& ws =
            SecurityOrchestrator::getWiredSensors();

        // ========================================================
        // SENSORI
        // ========================================================

        if(force ||
        (changes &
            (SecurityOrchestrator::CHANGE_SENSOR |
            SecurityOrchestrator::CHANGE_COMMAND)))
        {
            const auto* sensorCfg =
                ws.GetConfig();

            if(sensorCfg)
            {
                const size_t count =
                    ws.Count();

                for(size_t i = 0; i < count; ++i)
                {
                    const int area =
                        sensorCfg[i].statusArea;

                    if(area < 0)
                        continue;

                    SecurityHmiInterface::SensorState state;

                    if(!hmi.getSensorState(i, state))
                        continue;

                    uint16_t value = 0;

                    value |= state.active[0]  ? 0x0001 : 0;
                    value |= state.active[1]  ? 0x0002 : 0;
                    value |= state.active[2]  ? 0x0004 : 0;
                    value |= state.active[3]  ? 0x0008 : 0;

                    value |= state.alarm[0]   ? 0x0010 : 0;
                    value |= state.alarm[1]   ? 0x0020 : 0;
                    value |= state.alarm[2]   ? 0x0040 : 0;
                    value |= state.alarm[3]   ? 0x0080 : 0;

                    value |= state.inhibit[0] ? 0x0100 : 0;
                    value |= state.inhibit[1] ? 0x0200 : 0;
                    value |= state.inhibit[2] ? 0x0400 : 0;
                    value |= state.inhibit[3] ? 0x0800 : 0;

                    value |= state.alarmOut ? 0x1000 : 0;
                    value |= !state.enabled  ? 0x2000 : 0;
                    value |= state.rtMem     ? 0x4000 : 0;
                    value |= state.h24Mem    ? 0x8000 : 0;

                    if(force ||
                    buffer.getValueFast(area) !=
                    static_cast<long>(value))
                    {
                        dm->forceInternalEvent(
                            area,
                            static_cast<long>(value)
                        );
                    }
                }
            }
        }

        // ========================================================
        // ZONE
        // ========================================================

        if(force ||
        (changes &
            SecurityOrchestrator::CHANGE_ZONE))
        {
            auto& zones =
                ws.Zones();

            const size_t zoneCount =
                ws.GetZoneCount();

            for(size_t i = 0;
                i < zoneCount;
                ++i)
            {
                const char* zoneName =
                    ws.GetZoneName(i);

                if(!zoneName)
                    continue;

                const int area =
                    zones.GetZoneStatusArea(zoneName);

                if(area < 0)
                    continue;

                SecurityHmiInterface::ZoneState state;

                if(!hmi.getZoneState(i, state))
                    continue;

                uint16_t value = 0;

                value |= state.alarm    ? 0x0001 : 0;
                value |= state.alarmH24 ? 0x0002 : 0;
                value |= state.trouble  ? 0x0004 : 0;
                value |= state.bypassed ? 0x0008 : 0;
                value |= state.rtMem    ? 0x0010 : 0;
                value |= state.h24Mem   ? 0x0020 : 0;

                if(force ||
                buffer.getValueFast(area) !=
                static_cast<long>(value))
                {
                    dm->forceInternalEvent(
                        area,
                        static_cast<long>(value)
                    );
                }
            }
        }

        // ========================================================
        // SISTEMA / CENTRALE
        // ========================================================

        if(force ||
        (changes &
            (SecurityOrchestrator::CHANGE_SYSTEM |
            SecurityOrchestrator::CHANGE_PANEL  |
            SecurityOrchestrator::CHANGE_COMM   |
            SecurityOrchestrator::CHANGE_COMMAND |
            SecurityOrchestrator::CHANGE_ZONE   |
            SecurityOrchestrator::CHANGE_SENSOR)))
        {
            SecurityHmiInterface::PanelState state;

            if(!hmi.getPanelState(state, 0))
                return;

            // ----------------------------------------------------
            // SYSTEM STATUS
            // ----------------------------------------------------

            const int statusArea =
                cfg->statusArea;

            if(statusArea >= 0)
            {
                const uint16_t value =
                    static_cast<uint16_t>(
                        state.systemBitmask
                    );

                if(force ||
                buffer.getValueFast(statusArea) !=
                static_cast<long>(value))
                {
                    dm->forceInternalEvent(
                        statusArea,
                        static_cast<long>(value)
                    );
                }
            }

            // ----------------------------------------------------
            // SYSTEM STATUS 2
            // ----------------------------------------------------

            const int statusArea2 =
                cfg->statusArea2;

            if(statusArea2 >= 0)
            {
                uint16_t value = 0;

                const bool engaged =
                    state.armState !=
                    AlarmPanelInterface::ArmState::DISARMED;

                // ------------------------------------------------
                // Presenza di una memoria di allarme:
                // bit 6..11 = memorie centrale
                // ------------------------------------------------

                const bool alarmMemorized =
                    (static_cast<uint16_t>(
                        state.systemBitmask
                    ) & 0x0FC0u) != 0;

                const bool alarmSilenceAvailable =
                    engaged &&
                    alarmMemorized;

                value |= state.connected
                    ? 0x0001 : 0;

                value |= state.communicationFault
                    ? 0x0002 : 0;

                value |= state.channelSupervised
                    ? 0x0004 : 0;

                // bit 3 = PRONTO
                value |= state.ready
                    ? 0x0008 : 0;

                // bit 4 = AVARIA
                value |= state.globalTrouble
                    ? 0x0010 : 0;

                // bit 5 = PRONTO PER ATTIVAZIONE
                //
                // Attivo solo se:
                // - centrale pronta
                // - nessuna avaria
                // - nessun allarme memorizzato
                value |= (
                    state.readyForArm &&
                    state.ready &&
                    !state.globalTrouble &&
                    !alarmMemorized
                )
                    ? 0x0020 : 0;

                // bit 6 = disponibilità tacitazione allarme
                value |= alarmSilenceAvailable
                    ? 0x0040 : 0;

                if(force ||
                buffer.getValueFast(statusArea2) !=
                static_cast<long>(value))
                {
                    dm->forceInternalEvent(
                        statusArea2,
                        static_cast<long>(value)
                    );
                }
            }

            // ----------------------------------------------------
            // ARM STATE
            // ----------------------------------------------------

            const int armStateArea =
                cfg->armStateArea;

            if(armStateArea >= 0)
            {
                const long value =
                    static_cast<long>(
                        static_cast<uint8_t>(
                            state.armState
                        )
                    );

                if(force ||
                buffer.getValueFast(armStateArea) !=
                value)
                {
                    dm->forceInternalEvent(
                        armStateArea,
                        value
                    );
                }
            }
        }
    }


    // ============================================================
    // UPDATE ALARM CALLBACK
    // ============================================================

    static void UpdateAlarmCallback()
    {
        auto& hmi =
            SecurityHmiInterface::instance();

        SecurityHmiInterface::PanelState panelState;

        if(!hmi.getPanelState(panelState, 0))
            return;

        const bool engaged =
            panelState.armState !=
            AlarmPanelInterface::ArmState::DISARMED;

        const uint64_t systemBitmask =
            static_cast<uint64_t>(
                panelState.systemBitmask
            );

        const uint64_t currentAlarmMask =
            SecurityOrchestrator::getCurrentAlarmMask();

        auto& alarmPanel =
            DomoManagerAlarmPanel::instance();

        const uint64_t effectiveAlarmMask =
            alarmPanel.getEffectiveAlarmMask();

        const bool active =
            effectiveAlarmMask != 0;

        if(alarmState.valid &&
        engaged == alarmState.engaged &&
        active == alarmState.active &&
        currentAlarmMask ==
            alarmState.currentAlarmMask &&
        effectiveAlarmMask ==
            alarmState.effectiveAlarmMask &&
        systemBitmask ==
            alarmState.systemBitmask)
        {
            return;
        }

        alarmState.engaged =
            engaged;

        alarmState.active =
            active;

        alarmState.currentAlarmMask =
            currentAlarmMask;

        alarmState.effectiveAlarmMask =
            effectiveAlarmMask;

        alarmState.systemBitmask =
            systemBitmask;

        alarmState.valid =
            true;

        if(alarmState.callback)
        {
            alarmState.callback(
                engaged,
                active,
                currentAlarmMask,
                effectiveAlarmMask,
                systemBitmask
            );
        }
    }

public:

    // ============================================================
    // SETUP
    // ============================================================

    static bool Setup(
        DomoManager& dm,
        FrontendConfig::Security& cfg,
        uint8_t source)
    {
        context.manager =
            &dm;

        context.config =
            &cfg;

        context.securitySource =
            source;

        runtime.initialized =
            false;

        runtime.hmiInitialSyncPending =
            true;

        runtime.reportOnChange =
            cfg.reportOnChange;

        runtime.changeFlags =
            SecurityOrchestrator::CHANGE_NONE;

        alarmState.valid =
            false;

        alarmState.engaged =
            false;

        alarmState.active =
            false;

        alarmState.currentAlarmMask =
            0;

        alarmState.effectiveAlarmMask =
            0;

        alarmState.systemBitmask =
            0;

        silenceState.valid =
            false;

        silenceState.lastMask =
            0;

        if(!cfg.enabled)
            return false;

        if(source == 255)
        {
            LOG_EF(
                "SECURITY",
                "Invalid EventManager source"
            );
            return false;
        }

        SecurityOrchestrator::Setup(&cfg);

        if(!SecurityOrchestrator::isInitialized())
        {
            LOG_EF(
                "SECURITY",
                "Security orchestrator initialization failed"
            );
            return false;
        }

        DMRuntime* runtimeInstance =
            DMRuntime::GetInstance();

        if(runtimeInstance)
        {
            runtimeInstance->SetSecurityAreaErrorCallback(
                [](int area)
                {
                    SecurityEngine::NotifyAreaErrorChanged(area);
                }
            );
        }
        else
        {
            LOG_EF(
                "SECURITY",
                "DMRuntime instance not available: "
                "device error callback NOT registered"
            );
        }

        auto& panel =
            DomoManagerAlarmPanel::instance();

        panel.setAlarmCallback(
            AlarmPanelAlarmCallback
        );

        panel.setEventCallback(
            AlarmPanelEventCallback
        );

        runtime.initialized =
            true;

        return true;
    }

    // ============================================================
    // SET ALARM CALLBACK
    // ============================================================

    static void setAlarmCallback(
        AlarmCallback callback)
    {
        alarmState.callback =
            callback;

        alarmState.valid =
            false;

        alarmState.engaged =
            false;

        alarmState.active =
            false;

        alarmState.currentAlarmMask =
            0;

        alarmState.effectiveAlarmMask =
            0;

        alarmState.systemBitmask =
            0;
    }

    // ============================================================
    // AREA ERROR EVENT
    // ============================================================

    static bool NotifyAreaErrorChanged(int area)
    {
        if(!runtime.initialized || area<0)
            return false;

        if(!SecurityOrchestrator::NotifyAreaErrorChanged(area))
            return false;

        runtime.changeFlags |=
            SecurityOrchestrator::CHANGE_SENSOR |
            SecurityOrchestrator::CHANGE_ZONE;

        return true;
    }


    // ============================================================
    // LOOP
    // ============================================================

    static bool Loop(
        DomoManager& dm,
        unsigned long now)
    {
        if(!runtime.initialized ||
        !context.manager ||
        !context.config ||
        !context.config->enabled)
        {
            return false;
        }

        if(!dm.getPowerOnCycleCompleted())
            return false;

        const bool alarmEngaged =
            DomoManagerAlarmPanel::instance()
                .getArmState(0) !=
            AlarmPanelInterface::ArmState::DISARMED;

        runtime.changeFlags |=
            SecurityOrchestrator::Loop(
                now,
                alarmEngaged
            );

        const uint64_t currentAlarmMask =
            SecurityOrchestrator::getCurrentAlarmMask();

        if(!silenceState.valid ||
        currentAlarmMask !=
            silenceState.lastMask)
        {
            DomoManagerAlarmPanel::instance()
                .synchronizeAlarmSilence(
                    currentAlarmMask
                );

            silenceState.lastMask =
                currentAlarmMask;

            silenceState.valid =
                true;
        }

        const bool forceHmi =
            runtime.hmiInitialSyncPending;

        const uint8_t changesBeforeHmi =
            runtime.changeFlags;

        UpdateSecurityHmi(forceHmi);

        if(forceHmi)
            runtime.hmiInitialSyncPending =
                false;

        if(forceHmi ||
        !alarmState.valid ||
        (changesBeforeHmi &
            (SecurityOrchestrator::CHANGE_SYSTEM |
            SecurityOrchestrator::CHANGE_PANEL  |
            SecurityOrchestrator::CHANGE_COMM   |
            SecurityOrchestrator::CHANGE_COMMAND |
            SecurityOrchestrator::CHANGE_ZONE)))
        {
            UpdateAlarmCallback();
        }

        if(runtime.changeFlags ==
        SecurityOrchestrator::CHANGE_NONE)
        {
            return false;
        }

        if(runtime.reportOnChange &&
        (runtime.changeFlags &
            (SecurityOrchestrator::CHANGE_ZONE |
            SecurityOrchestrator::CHANGE_SYSTEM |
            SecurityOrchestrator::CHANGE_PANEL |
            SecurityOrchestrator::CHANGE_COMM |
            SecurityOrchestrator::CHANGE_COMMAND)))
        {
            SecurityOrchestrator::Diagnostic::CoreReport();
        }

        runtime.changeFlags =
            SecurityOrchestrator::CHANGE_NONE;

        return true;
    }

    // ============================================================
    // APPLY COMMAND
    // ============================================================

    static bool ApplyCommand(
        int area,
        long value,
        unsigned long now)
    {
        if (!runtime.initialized ||
            !context.config ||
            !context.manager)
            return false;

        LOG_IF(
            "SECURITY",
            "ApplyCommand ENTRY: area=%d value=%ld now=%lu",
            area,
            value,
            now
        );

        // ========================================================
        // PANEL BIT COMMAND
        // ========================================================

        if (context.config->panelBitCommandArea >= 0 &&
            area == context.config->panelBitCommandArea)
        {
            LOG_IF(
                "SECURITY",
                "PANEL BIT AREA MATCH: area=%d value=0x%08lX",
                area,
                (unsigned long)value
            );

            const bool handled =
                SecurityOrchestrator::
                    ApplyPanelBitCommand(
                        area,
                        value
                    );

            if (handled)
            {
                runtime.changeFlags |=
                    SecurityOrchestrator::CHANGE_COMMAND;

                context.manager->getBuffer()
                    .WriteElement(
                        area,
                        0,
                        false,
                        now
                    );

                LOG_IF(
                    "SECURITY",
                    "PANEL BIT CMD RESET: area=%d value=0",
                    area
                );
            }
            else
            {
                LOG_WF(
                    "SECURITY",
                    "Panel bit command rejected area=%d value=0x%08lX",
                    area,
                    (unsigned long)value
                );
            }

            return true;
        }

        // ========================================================
        // ALARM PANEL COMMAND
        // ========================================================

        if (context.config->panelCommandArea >= 0 &&
            area == context.config->panelCommandArea)
        {
            const bool handled =
                SecurityOrchestrator::
                    ApplyPanelCommand(
                        area,
                        value
                    );

            if (handled)
            {
                runtime.changeFlags |=
                    SecurityOrchestrator::CHANGE_COMMAND;

                LOG_IF(
                    "SECURITY",
                    "PANEL CMD area=%d value=%ld",
                    area,
                    value
                );
            }
            else
            {
                LOG_WF(
                    "SECURITY",
                    "Panel command rejected area=%d value=%ld",
                    area,
                    value
                );
            }

            return true;
        }

        // ========================================================
        // SINGLE SENSOR
        // ========================================================

        if (SecurityOrchestrator::
                ApplySecurityCommand(
                    area,
                    value
                ))
        {
            runtime.changeFlags|=
                SecurityOrchestrator::CHANGE_COMMAND|
                SecurityOrchestrator::CHANGE_SENSOR|
                SecurityOrchestrator::CHANGE_ZONE|
                SecurityOrchestrator::CHANGE_SYSTEM;

            LOG_IF(
                "SECURITY",
                "SENSOR CMD area=%d value=%ld",
                area,
                value
            );

            return true;
        }

        // ========================================================
        // NOT A SECURITY COMMAND
        // ========================================================

        return false;
    }

    // ============================================================
    // DIRECT STATUS ACCESS
    // ============================================================

    static long GetStatus()
    {
        if (!runtime.initialized)
            return 0;

        return SecurityOrchestrator::
            getSystem()
            .getBitmask();
    }

    // ============================================================
    // ACCESSORS
    // ============================================================

    static bool enabled()
    {
        return runtime.initialized &&
               context.config &&
               context.config->enabled;
    }

    static bool isInitialized()
    {
        return runtime.initialized;
    }

    static uint8_t getChangeFlags()
    {
        return runtime.changeFlags;
    }

    static uint8_t getSource()
    {
        return context.securitySource;
    }

    static DomoManager* getManager()
    {
        return context.manager;
    }

    static FrontendConfig::Security* getConfig()
    {
        return context.config;
    }
};

class TaskEngineBase
{
private:

    // ============================================================
    // AEE
    // ============================================================

    static AEERegistry* aee()
    {
        return &AEEEngine::getAEE();
    }


    // ============================================================
    // COMMUNICATION SCHEDULER
    // ============================================================

    static inline CommunicationScheduler communicationScheduler{3};


    // ============================================================
    // COMMUNICATION SERVICES
    // ============================================================

    static void Communication_Bridge(unsigned long now)
    {
        BridgeEngine::get().loop(now);
    }


    static void Communication_MQTT(unsigned long now)
    {
        MQTTEngine::Loop(now);
    }


    // ============================================================
    // TASK: HVAC
    // ============================================================

    static void Task_HVAC(
        DomoManager& manager,
        unsigned long now)
    {
        const auto& cfg =
            TaskEngineOrchestrator::getCfg().hvac;


        // --------------------------------------------------------
        // TIME
        // --------------------------------------------------------

        struct tm t;

        manager
            .getTimeManager()
            .getDateTime(t);


        HeatPumpController::HVACTime ht;

        ht.dayOfWeek = t.tm_wday;
        ht.hour      = t.tm_hour;
        ht.minute    = t.tm_min;


        // --------------------------------------------------------
        // ZONE TEMPERATURES
        // --------------------------------------------------------

        auto& buffer =
            manager.getBuffer();

        constexpr size_t MAX_ZONE_TEMPERATURES = 16;

        static float zoneTemps[MAX_ZONE_TEMPERATURES];


        const size_t zoneCount =
            (cfg.zoneCount < MAX_ZONE_TEMPERATURES)
                ? cfg.zoneCount
                : MAX_ZONE_TEMPERATURES;


        for (size_t i = 0; i < zoneCount; ++i)
        {
            const int area =
                cfg.zones[i].temperatureArea;

            zoneTemps[i] =
                buffer.getValueFast(area) / 10.0f;
        }


        // --------------------------------------------------------
        // GLOBAL TEMPERATURES / WINDOW
        // --------------------------------------------------------

        const float tInterna =
            cfg.readIndoorTemp
                ? cfg.readIndoorTemp()
                : 0.0f;


        const float tEsterna =
            cfg.readOutdoorTemp
                ? cfg.readOutdoorTemp()
                : 0.0f;


        const bool finestraAperta =
            cfg.readWindowOpen
                ? cfg.readWindowOpen()
                : false;


        // --------------------------------------------------------
        // HVAC ENGINE
        // --------------------------------------------------------

        HVACEngine::Loop(
            now,
            zoneTemps,
            tInterna,
            tEsterna,
            finestraAperta,
            ht
        );
    }


    // ============================================================
    // TASK: AVERAGES
    // ============================================================

    static void Task_Averages(
        DomoManager& manager,
        unsigned long now)
    {
        (void)now;

        const auto& cfg =
            TaskEngineOrchestrator::getCfg();

        auto& buffer =
            manager.getBuffer();

        auto& averages =
            manager.getAverages();

        // --------------------------------------------------------
        // GROUPS
        // --------------------------------------------------------

        for (size_t g = 0;
             g < cfg.averages.gruppiCount;
             ++g)
        {
            const auto& gruppo =
                cfg.averages.gruppi[g];


            // ----------------------------------------------------
            // MEASUREMENTS
            // ----------------------------------------------------

            for (size_t i = 0;
                 i < gruppo.count;
                 ++i)
            {
                const auto& s =
                    gruppo.sensori[i];


                const float raw =
                    buffer.getValueFast(s.area);


                const float value =
                    raw * s.factor;


                averages.addMeasurement(
                    gruppo.nome,
                    value
                );
            }


            // ----------------------------------------------------
            // AVERAGE
            // ----------------------------------------------------

            const float average =
                averages.groupAverage(
                    gruppo.nome
                );


            // ----------------------------------------------------
            // OUTPUT
            // ----------------------------------------------------

            if (gruppo.areaOut >= 0)
            {
                manager.forceInternalEvent(
                    gruppo.areaOut,
                    static_cast<long>(
                        average *
                        gruppo.outScale
                    )
                );
            }
        }
    } 

    // ============================================================
    // TASK: SENSORS / SECURITY
    // ============================================================

    static void Task_Sensors(
        DomoManager& manager,
        unsigned long now)
    {
        SecurityEngine::Loop(
            manager,
            now
        );
    }


    // ============================================================
    // TASK: AEE DUMP
    // ============================================================

    static void Task_AEE_Dump(
        DomoManager& manager,
        unsigned long now)
    {
        (void)manager;
        (void)now;


        AEERegistry* registry =
            aee();


        if (!registry)
            return;


        StaticJsonDocument<1024> doc;


        registry->forEach(
            [&](AEEVariableBase* v)
            {
                if (v)
                    v->toJson(doc);
            }
        );


        String out;

        serializeJson(
            doc,
            out
        );


        LOG_IF(
            "AEE-DUMP",
            "%s",
            out.c_str()
        );
    }


    // ============================================================
    // TASK: COMMUNICATION
    // ============================================================

    static void Task_Communication(
        DomoManager& manager,
        unsigned long now)
    {
        // --------------------------------------------------------
        // HOT STANDBY
        // --------------------------------------------------------

        #if HOTSTANDBY_ENABLED

        if (!manager.isClusterMaster())
            return;

        #endif


        // --------------------------------------------------------
        // POWER-ON CYCLE
        // --------------------------------------------------------

        if (!manager.getPowerOnCycleCompleted())
            return;


        // --------------------------------------------------------
        // COMMUNICATION ROUND ROBIN
        // --------------------------------------------------------

        communicationScheduler.update(now);
    }


public:

    // ============================================================
    // SETUP
    //
    // Registra tutti i task standard del frontend.
    // ============================================================

    static void Setup(
        const FrontendConfig& cfg)
    {
        // ========================================================
        // ORCHESTRATOR
        // ========================================================

        TaskEngineOrchestrator::Setup(
            cfg
        );

        // ========================================================
        // STANDARD TASKS
        // ========================================================

        TaskEngineOrchestrator::AddTask(
            Task_Sensors,
            cfg.security.intervalMs,
            cfg.security.enabled
        );

        TaskEngineOrchestrator::AddTask(
            Task_HVAC,
            cfg.hvac.intervalMs,
            cfg.hvac.enabled
        );

        TaskEngineOrchestrator::AddTask(
            Task_Averages,
            cfg.averages.intervalMs,
            cfg.averages.enabled
        );

        /*
        * Sempre presente.
        *
        * Gestisce il round-robin dei servizi di comunicazione.
        */
        TaskEngineOrchestrator::AddTask(
            Task_Communication,
            15,
            true
        );

        // ========================================================
        // COMMUNICATION SERVICES
        // ========================================================

        if (cfg.bridge.enabled)
        {
            communicationScheduler.add(
                Communication_Bridge,
                20,
                100,
                true
            );
        }

        if (cfg.mqtt.enabled)
        {
            communicationScheduler.add(
                Communication_MQTT,
                50,
                50,
                true
            );
        }
    }


    // ============================================================
    // ADD TASK
    //
    // Entry point per i task custom definiti dal frontend.
    // ============================================================

    static void AddTask(
        void (*fn)(DomoManager&, unsigned long),
        uint32_t intervalMs,
        bool enabled = true)
    {
        TaskEngineOrchestrator::AddTask(
            fn,
            intervalMs,
            enabled
        );
    }


    // ============================================================
    // LOOP
    // ============================================================

    static void Loop(
        DomoManager& manager,
        unsigned long now)
    {
        TaskEngineOrchestrator::Loop(
            manager,
            now
        );
    }


    // ============================================================
    // FRONTEND CYCLE
    // ============================================================

    static bool hasFrontendCycleCompleted()
    {
        return
            TaskEngineOrchestrator::
                hasFrontendCycleCompleted();
    }


    static void resetFrontendCycleFlag()
    {
        TaskEngineOrchestrator::
            resetFrontendCycleFlag();
    }
};

class FrontendOwnerMode
{
private:

    static const char* modeToString(
        OwnerManager::Mode mode)
    {
        switch (mode)
        {
            case OwnerManager::OWNER:
                return "OWNER";

            case OwnerManager::GUEST:
                return "GUEST";

            case OwnerManager::DEVELOPER:
                return "DEVELOPER";
        }

        return "UNKNOWN";
    }

public:

    static void OnChanged(
        OwnerManager::Mode oldMode,
        OwnerManager::Mode newMode,
        OwnerManager::Reason reason)
    {
        (void)reason;

        if (newMode == OwnerManager::DEVELOPER)
            LogManager::enable();
        else
            LogManager::disable();

        LOG_IF(
            "FrontendOwnerMode",
            "Callback OwnerModeChanged: %s -> %s",
            modeToString(oldMode),
            modeToString(newMode)
        );
    }
};

class FrontendRuntime
{
protected:
    unsigned long now = 0;
    bool fastServicesCycle = false;

    // ========================================================
    //  CORE REFERENCES
    // ========================================================

    DomoManager& manager;

    FrontendConfig config;


    // ========================================================
    //  USER BUTTON
    // ========================================================

    ButtonManager btnUSER;


    // ========================================================
    //  RUNTIME STATE
    // ========================================================
   
    bool devModeApplied = false;


    #if HOTSTANDBY_ENABLED

        // ========================================================
        //  HOTSTANDBY STATE
        // ========================================================

        bool lastMaster = false;

        bool isMaster = false;

    #endif


    // ========================================================
    //  CUSTOM FRONTEND HOOKS
    //
    //  Il frontend derivato può sovrascrivere questi metodi
    //  per eseguire operazioni specifiche.
    // ========================================================

    virtual void onStartupButton(
        unsigned long now)
    {
        (void)now;
    }


    virtual void onButtonPressed(
        unsigned long now)
    {
        (void)now;
    }


    virtual void onDeveloperMode()
    {
    }


    #if HOTSTANDBY_ENABLED

        virtual void onBecomeMaster()
        {
        }


        virtual void onBecomeSlave()
        {
        }

    #endif


    // ========================================================
    //  BUTTON PROCESSING
    // ========================================================

    void processButton(unsigned long now)
    {
        auto st =
            btnUSER.update(now);


        // ----------------------------------------------------
        // Pulsante premuto durante lo startup
        // ----------------------------------------------------

        if (st.pressedAtStartup)
        {
            LOG_IF(
                "BUTTON",
                "Pulsante tenuto premuto allo startup"
            );


            manager.getWatchDiag().onButtonPressed(
                manager,
                0,
                config.diagnostic
            );


            onStartupButton(now);

            return;
        }


        // ----------------------------------------------------
        // Pulsante premuto durante il normale loop
        // ----------------------------------------------------

        if (st.pressedNow)
        {
            LOG_IF(
                "BUTTON",
                "Pulsante premuto durante il loop"
            );


            auto& wd =
                manager.getWatchDiag();


            wd.onButtonPressed(
                manager,
                1,
                config.diagnostic
            );


            onButtonPressed(now);
        }
    }


    // ========================================================
    //  DEVELOPER MODE
    // ========================================================

    void processDeveloperMode()
    {
        if (devModeApplied)
            return;


        manager.getOwner().setDeveloper(
            OwnerManager::SYSTEM_REBOOT
        );


        devModeApplied = true;


        onDeveloperMode();
    }


    #if HOTSTANDBY_ENABLED

        // ========================================================
        //  HOTSTANDBY PROCESSING
        // ========================================================

        void processHotStandby()
        {
            const bool master =
                manager.isClusterMaster();


            isMaster = master;


            // ----------------------------------------------------
            // Nessuna transizione
            // ----------------------------------------------------

            if (master == lastMaster)
                return;


            // ----------------------------------------------------
            // SLAVE -> MASTER
            // ----------------------------------------------------

            if (master)
            {
                LOG_I(
                    "Main",
                    "Passo a MASTER"
                );


                onBecomeMaster();
            }


            // ----------------------------------------------------
            // MASTER -> SLAVE
            // ----------------------------------------------------

            else
            {
                LOG_I(
                    "Main",
                    "Passo a SLAVE"
                );


                onBecomeSlave();
            }


            lastMaster = master;
        }

    #endif

    void RegisterFrontendDiagnostics()
    {
        Diagnostic::frontendDiagnosticCallback() =
            [this]()
            {
                Serial.println(
                    "\n===== FRONTEND DIAGNOSTIC ====="
                );

                // ===== DIAGNOSTICA BASE =====

                if (config.ps.enabled)
                {
                    PowerSupervisor::Diagnostic::Report(
                        PowerSupervisorOrchestrator::Get()
                    );
                }

                if (config.hvac.enabled)
                {
                    HeatPumpController::Diagnostic::Report(
                        HVACEngine::GetHP()
                    );
                }

                if (config.security.enabled)
                {
                    // Diagnostica completa della centrale virtuale
                    DomoManagerAlarmPanel::instance().diagnostic();

                    // Diagnostica del motore sensori
                    SecurityOrchestrator::Diagnostic::FullReport();

                    // Stato aggregato
                    SecurityOrchestrator::getSystem().DiagnosticReport();
                }

                // ===== ESTENSIONE FRONTEND =====

                onFrontendDiagnostics();

                Serial.println(
                    "===== END FRONTEND DIAGNOSTIC =====\n"
                );
            };
    }

    virtual void onFrontendDiagnostics()
    {
    }

    void updateFastServices(unsigned long now)
    {
        // ====================================================
        // PULSE OUTPUTS
        // ====================================================

        PulseManager::Loop(
            now
        );

        // ====================================================
        // WEB API
        //
        // Il server HTTP deve essere servito con la massima
        // frequenza possibile, senza attendere il round-robin
        // Bridge/MQTT.
        // ====================================================

         /*
        * Client polling e server webhook sono indipendenti.
        *
        * Il servizio viene registrato se almeno uno dei due
        * è abilitato.
        */
        const bool webApiEnabled =
            config.webApi.client.enabled ||
            config.webApi.server.enabled;

        if (webApiEnabled)
        {

            if (WebAPIEngine::IsRunning())
            {
                WebAPIEngine::Loop(
                    now
                );
            }
        }
    }
public:

    // ========================================================
    //  CONSTRUCTOR
    // ========================================================

    FrontendRuntime(
        DomoManager& dm,
        const FrontendConfig& cfg)
        : manager(dm),
          config(cfg),
          btnUSER(cfg.pins.userButton)
    {
        btnUSER.begin();
    }


    // ========================================================
    //  DESTRUCTOR
    // ========================================================

    virtual ~FrontendRuntime() = default;


    // ========================================================
    //  UPDATE RUNTIME
    //
    //  Viene chiamato una volta per ogni ciclo del frontend.
    //
    //  Esegue:
    //    1. acquisizione now
    //    2. gestione pulsante
    //    3. Developer Mode
    //    4. HotStandby
    //
    //  Restituisce il timestamp acquisito.
    // ========================================================

    unsigned long updateRuntime()
    {
        now =
            manager.getTimeManager().nowMs();

        // ====================================================
        // FAST SERVICES
        //
        // Eseguiti un ciclo frontend sì e uno no.
        // ====================================================

        fastServicesCycle =
            !fastServicesCycle;

        if (fastServicesCycle)
        {
            updateFastServices(
                now
            );
        }
        else
        {
            processButton(now);
            processDeveloperMode();
        }

        #if HOTSTANDBY_ENABLED

                processHotStandby();

        #endif

        return now;
    }


    // ========================================================
    //  GET CURRENT TIME
    // ========================================================

    unsigned long getNow() const
    {
        return now;
    }


    // ========================================================
    //  GET MANAGER
    // ========================================================

    DomoManager& getManager()
    {
        return manager;
    }


    const DomoManager& getManager() const
    {
        return manager;
    }


    // ========================================================
    //  GET CONFIG
    // ========================================================

    FrontendConfig& getConfig()
    {
        return config;
    }


    const FrontendConfig& getConfig() const
    {
        return config;
    }


    #if HOTSTANDBY_ENABLED

        // ========================================================
        //  GET MASTER STATE
        // ========================================================

        bool getIsMaster() const
        {
            return isMaster;
        }

    #endif
};
#endif
