#ifndef DMNetwork_HPP
#define DMNetwork_HPP

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑03‑24
   Note:
    • Nessuna

   ============================================================================ */

#include <Arduino.h>
#include <array>
#include <cstdint>
#include <algorithm>
#include <functional>

#include "DMBaseClassCore.hpp"


#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

/*
 * NOTE TECNICA SULLA GESTIONE DELLE PRIORITÀ PER-IP
 *
 * Ogni IP mantiene una PRIORITY indipendente (Low, Medium, Normal, High).
 * La PRIORITY determina il range di lettura del planner:
 *
 *   - HIGH   → range completo (tutti i device dell’IP)
 *   - LOW    → 1 device per ciclo (jump=1)
 *   - MEDIUM → 2 device per ciclo
 *   - NORMAL → 3 device per ciclo
 *
 * Il pacing (slotDuration superato) è anch’esso PER-IP:
 *   - Se un IP viene interrotto → interrupted=true → si salva il DeviceIndex
 *     e NON si avanza la priority di QUELL’IP.
 *
 *   - Gli altri IP NON vengono influenzati:
 *     se non sono stati interrotti → interrupted=false → avanzano normalmente
 *     la loro priority e il loro DeviceIndex.
 *
 * Questo comportamento è VOLUTO:
 *   - Ogni IP ha il proprio ciclo di polling indipendente.
 *   - Un IP può avere PRIORITY HIGH (range completo) mentre un altro IP
 *     può avere PRIORITY LOW (1 device per ciclo).
 *   - Il pacing di un IP NON deve bloccare o alterare la priority degli altri.
 *
 * In sintesi:
 *   - interrupted è PER-IP
 *   - DeviceIndex è PER-IP
 *   - Priority è PER-IP
 *   - Il planner riprende SOLO per l’IP che ha subito pacing
 *   - Gli altri IP avanzano normalmente secondo la loro priority
 *
 * Questo garantisce:
 *   - lettura corretta dei device ad alta priorità
 *   - lettura progressiva dei device a bassa priorità
 *   - comportamento coerente e prevedibile del ciclo Modbus
 */




// ============================================================
// SocketManager
//
// NON BLOCKING.
//
// Funzioni:
//   - tiene traccia della domanda configurata
//   - tiene traccia delle socket effettivamente acquisite
//   - non blocca mai il firmware
//   - acquire() fallisce immediatamente se non c'è spazio
//
// Compatibile con NetworkManager esistente.
// ============================================================


class SocketManager
{
public:

    // ========================================================
    // OWNER ID
    // ========================================================

    using OwnerId = int;


    // ========================================================
    // SOCKET TYPE
    // ========================================================

    enum class SocketKind : uint8_t
    {
        TCP_SERVER,
        TCP_CLIENT,
        UDP
    };


    // ========================================================
    // SOCKET
    // ========================================================

    struct Socket
    {
        bool used = false;

        OwnerId owner = -1;

        SocketKind kind = SocketKind::TCP_CLIENT;

        int resourceId = -1;
    };


    // ========================================================
    // OWNER
    // ========================================================

    struct Owner
    {
        OwnerId id = -1;

        const char* name = nullptr;

        uint16_t demand = 0;

        // true = per questo owner è già stato emesso
        // un warning di saturazione.
        //
        // Viene azzerato quando l'owner riesce nuovamente
        // ad acquisire una socket.
        bool capacityWarningShown = false;
    };


    // ========================================================
    // CONFIGURATION
    // ========================================================

    static constexpr uint8_t MAX_SOCKETS = 4;


private:

    // ========================================================
    // OWNER TABLE
    // ========================================================

    std::vector<Owner> owners;


    // ========================================================
    // SOCKET TABLE
    // ========================================================

    Socket sockets_[MAX_SOCKETS];


public:

    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    SocketManager() = default;


    // ========================================================
    // RESET
    // ========================================================

    void reset()
    {
        for (uint8_t i = 0;
             i < MAX_SOCKETS;
             ++i)
        {
            sockets_[i] = Socket{};
        }

        owners.clear();
    }


    // ========================================================
    // REGISTER OWNER
    //
    // Owner creato dinamicamente a runtime.
    //
    // NON riserva socket.
    // ========================================================

    OwnerId registerOwner(
        const char* name,
        uint16_t demand)
    {
        if (!name)
            return -1;


        // ----------------------------------------------------
        // Owner già esistente
        // ----------------------------------------------------

        for (auto& owner : owners)
        {
            if (owner.name &&
                strcmp(owner.name, name) == 0)
            {
                owner.demand = demand;

                return owner.id;
            }
        }


        // ----------------------------------------------------
        // Nuovo owner
        // ----------------------------------------------------

        const OwnerId id =
            static_cast<OwnerId>(
                owners.size()
            );


        owners.push_back({
            id,
            name,
            demand,
            false
        });


        return id;
    }


    // ========================================================
    // COMPATIBILITY
    // ========================================================

    OwnerId registerDemand(
        const char* name,
        uint8_t maximum)
    {
        return registerOwner(
            name,
            maximum
        );
    }


    // ========================================================
    // OWNER LOOKUP
    // ========================================================

    OwnerId findOwner(
        const char* name) const
    {
        if (!name)
            return -1;


        for (const auto& owner : owners)
        {
            if (owner.name &&
                strcmp(owner.name, name) == 0)
            {
                return owner.id;
            }
        }


        return -1;
    }


    // ========================================================
    // GET OWNER
    // ========================================================

    const Owner* getOwner(
        OwnerId id) const
    {
        if (id < 0 ||
            id >= static_cast<OwnerId>(
                owners.size()))
        {
            return nullptr;
        }


        return &owners[id];
    }


    // ========================================================
    // OWNER NAME
    // ========================================================

    const char* ownerName(
        OwnerId id) const
    {
        const Owner* owner =
            getOwner(id);


        if (!owner)
            return "?";


        return owner->name
            ? owner->name
            : "?";
    }


    // ========================================================
    // CONFIGURED DEMAND
    // ========================================================

    uint16_t configuredDemand() const
    {
        uint16_t total = 0;


        for (const auto& owner : owners)
        {
            total += owner.demand;
        }


        return total;
    }


    // ========================================================
    // OWNER DEMAND
    // ========================================================

    uint16_t ownerDemand(
        OwnerId id) const
    {
        const Owner* owner =
            getOwner(id);


        if (!owner)
            return 0;


        return owner->demand;
    }


    // ========================================================
    // ACQUIRE
    //
    // NON BLOCKING.
    //
    // Se la socket non è disponibile:
    //   - ritorna -1
    //   - emette un solo warning per owner
    //
    // Il warning viene riabilitato automaticamente quando
    // l'owner riesce nuovamente ad acquisire una socket.
    // ========================================================

    int acquire(
        OwnerId owner,
        int resourceId,
        SocketKind kind)
    {
        if (!isValidOwner(owner))
        {
            LOG_WF(
                "NET::SOCKET",
                "ACQUIRE denied: invalid owner=%d",
                owner
            );

            return -1;
        }


        // ----------------------------------------------------
        // Già acquisita
        // ----------------------------------------------------

        for (uint8_t i = 0;
             i < MAX_SOCKETS;
             ++i)
        {
            Socket& socket =
                sockets_[i];


            if (!socket.used)
                continue;


            if (socket.owner == owner &&
                socket.resourceId == resourceId &&
                socket.kind == kind)
            {
                // La risorsa è disponibile e correttamente
                // tracciata: il prossimo failure potrà
                // nuovamente generare warning.
                owners[owner].capacityWarningShown =
                    false;

                return i;
            }
        }


        // ----------------------------------------------------
        // Cerca socket libera
        // ----------------------------------------------------

        for (uint8_t i = 0;
             i < MAX_SOCKETS;
             ++i)
        {
            Socket& socket =
                sockets_[i];


            if (socket.used)
                continue;


            socket.used = true;

            socket.owner =
                owner;

            socket.kind =
                kind;

            socket.resourceId =
                resourceId;


            owners[owner].capacityWarningShown =
                false;


            return i;
        }


        // ----------------------------------------------------
        // SATURAZIONE
        // ----------------------------------------------------

        Owner& ownerData =
            owners[owner];


        if (!ownerData.capacityWarningShown)
        {
            LOG_WF(
                "NET::SOCKET",
                "NO SOCKET owner=%s(%d) resource=%d kind=%s used=%u/%u",
                ownerData.name
                    ? ownerData.name
                    : "?",
                owner,
                resourceId,
                kindName(kind),
                usedCount(),
                MAX_SOCKETS
            );

            ownerData.capacityWarningShown =
                true;
        }


        return -1;
    }


    // ========================================================
    // COMPATIBILITY
    //
    // Legacy SocketHandle:
    //
    //     acquire(ownerName, resourceId)
    //
    // Cerca un owner già registrato.
    // ========================================================

    int acquire(
        const char* owner,
        int resourceId)
    {
        const OwnerId ownerId =
            findOwner(owner);


        if (ownerId < 0)
        {
            LOG_WF(
                "NET::SOCKET",
                "ACQUIRE denied: owner '%s' not registered",
                owner ? owner : "?"
            );

            return -1;
        }


        return acquire(
            ownerId,
            resourceId,
            SocketKind::TCP_CLIENT
        );
    }


    // ========================================================
    // RELEASE BY SLOT
    // ========================================================

    bool release(
        int slot)
    {
        if (!isValidSlot(slot))
            return false;


        Socket& socket =
            sockets_[slot];


        if (!socket.used)
            return false;


        const OwnerId owner =
            socket.owner;


        socket = Socket{};


        // Se l'owner è valido, il prossimo failure potrà
        // generare nuovamente il warning.
        if (isValidOwner(owner))
        {
            owners[owner].capacityWarningShown =
                false;
        }


        return true;
    }


    // ========================================================
    // RELEASE BY OWNER
    // ========================================================

    bool release(
        OwnerId owner,
        int resourceId,
        SocketKind kind)
    {
        if (!isValidOwner(owner))
            return false;


        for (uint8_t i = 0;
             i < MAX_SOCKETS;
             ++i)
        {
            Socket& socket =
                sockets_[i];


            if (!socket.used)
                continue;


            if (socket.owner == owner &&
                socket.resourceId == resourceId &&
                socket.kind == kind)
            {
                return release(i);
            }
        }


        return false;
    }


    // ========================================================
    // COMPATIBILITY
    // ========================================================

    bool release(
        const char* owner,
        int resourceId,
        SocketKind kind)
    {
        const OwnerId ownerId =
            findOwner(owner);


        if (ownerId < 0)
            return false;


        return release(
            ownerId,
            resourceId,
            kind
        );
    }


    // ========================================================
    // RELEASE OWNER ALL
    // ========================================================

    uint8_t releaseOwner(
        OwnerId owner)
    {
        if (!isValidOwner(owner))
            return 0;


        uint8_t released = 0;


        for (uint8_t i = 0;
             i < MAX_SOCKETS;
             ++i)
        {
            if (!sockets_[i].used)
                continue;


            if (sockets_[i].owner != owner)
                continue;


            if (release(i))
                ++released;
        }


        owners[owner].capacityWarningShown =
            false;


        return released;
    }


    // ========================================================
    // COUNTERS
    // ========================================================

    uint8_t usedCount() const
    {
        uint8_t count = 0;


        for (uint8_t i = 0;
             i < MAX_SOCKETS;
             ++i)
        {
            if (sockets_[i].used)
                ++count;
        }


        return count;
    }


    uint8_t freeCount() const
    {
        return static_cast<uint8_t>(
            MAX_SOCKETS - usedCount()
        );
    }


    // ========================================================
    // QUERY SOCKET
    // ========================================================

    bool isUsed(
        int slot) const
    {
        if (!isValidSlot(slot))
            return false;


        return sockets_[slot].used;
    }


    const Socket* getSocket(
        int slot) const
    {
        if (!isValidSlot(slot))
            return nullptr;


        return &sockets_[slot];
    }


    // ========================================================
    // COUNT BY OWNER
    // ========================================================

    uint8_t usedCountByOwner(
        OwnerId owner) const
    {
        if (!isValidOwner(owner))
            return 0;


        uint8_t count = 0;


        for (uint8_t i = 0;
             i < MAX_SOCKETS;
             ++i)
        {
            if (sockets_[i].used &&
                sockets_[i].owner == owner)
            {
                ++count;
            }
        }


        return count;
    }


    // ========================================================
    // COUNT BY KIND
    // ========================================================

    uint8_t usedCountByKind(
        SocketKind kind) const
    {
        uint8_t count = 0;


        for (uint8_t i = 0;
             i < MAX_SOCKETS;
             ++i)
        {
            if (sockets_[i].used &&
                sockets_[i].kind == kind)
            {
                ++count;
            }
        }


        return count;
    }


    // ========================================================
    // DIAGNOSTICS
    //
    // Chiamata esplicita: stampa tutto.
    // Nessun logging automatico di acquire/release.
    // ========================================================

    void dump() const
    {
        LOG_IF(
            "SocketManager",
            "SOCKETS %u/%u used, %u free, owners=%u configuredDemand=%u",
            usedCount(),
            MAX_SOCKETS,
            freeCount(),
            (unsigned)owners.size(),
            (unsigned)configuredDemand()
        );


        // ----------------------------------------------------
        // Owner table
        // ----------------------------------------------------

        for (const auto& owner : owners)
        {
            LOG_IF(
                "SocketManager",
                "  OWNER [%d] %s demand=%u used=%u",
                owner.id,
                owner.name
                    ? owner.name
                    : "?",
                (unsigned)owner.demand,
                (unsigned)usedCountByOwner(
                    owner.id
                )
            );
        }


        // ----------------------------------------------------
        // Socket table
        // ----------------------------------------------------

        for (uint8_t i = 0;
             i < MAX_SOCKETS;
             ++i)
        {
            const Socket& socket =
                sockets_[i];


            if (!socket.used)
            {
                LOG_IF(
                    "SocketManager",
                    "  SOCKET [%u] FREE",
                    i
                );

                continue;
            }


            LOG_IF(
                "SocketManager",
                "  SOCKET [%u] owner=%s(%d) resource=%d kind=%s",
                i,
                ownerName(socket.owner),
                socket.owner,
                socket.resourceId,
                kindName(socket.kind)
            );
        }
    }


private:

    // ========================================================
    // VALID OWNER
    // ========================================================

    bool isValidOwner(
        OwnerId owner) const
    {
        return
            owner >= 0 &&
            owner <
                static_cast<OwnerId>(
                    owners.size()
                );
    }


    // ========================================================
    // VALID SOCKET SLOT
    // ========================================================

    bool isValidSlot(
        int slot) const
    {
        return
            slot >= 0 &&
            slot <
                static_cast<int>(
                    MAX_SOCKETS
                );
    }


    // ========================================================
    // KIND NAME
    // ========================================================

    static const char* kindName(
        SocketKind kind)
    {
        switch (kind)
        {
            case SocketKind::TCP_SERVER:
                return "TCP_SERVER";

            case SocketKind::TCP_CLIENT:
                return "TCP_CLIENT";

            case SocketKind::UDP:
                return "UDP";

            default:
                return "?";
        }
    }
};


class NetworkManager {
public:
    using ProtocolId = int;

    enum class ProtocolState {
        IDLE,
        RUNNING,
        PARKED
    };

    struct ProtocolRuntime
    {
        ProtocolState state = ProtocolState::IDLE;
        uint32_t acquiredAt = 0;
        uint32_t parkedAt = 0;
        uint32_t missingReleaseCount = 0;
    };

    struct Protocol {
        unsigned long lastExec = 0;      // ultimo timestamp di esecuzione
        unsigned long lastTouch = 0;     // ultima chiamata tryAcquire
        unsigned long minInterval = 0;   // pacing minimo
        unsigned long maxDuration = 0;   // durata massima consentita
        bool active = false;             // protocollo in esecuzione

        // SAFE MODE
        int slotExceededCount = 0;
        unsigned long lastSlotExceeded = 0;
        bool safeMode = false;

        bool pacingEnabled = true;
        unsigned long slotDuration = 0;

        // Stato dinamico del protocollo
        enum class State {
            OK,
            DEVICE_ERROR,
            ERROR,
            WAITING,
            CYCLE_OK,
            WRITE_DONE,
            READ_DONE
        } lastState = State::OK;

        ProtocolRuntime runtime;

        // SOCKET OWNER
        SocketManager::OwnerId socketOwner = -1;

        // EVENT POLICY
        //
        // true  -> gli eventi emessi da questo protocollo
        //          vengono coalesced per source + area.
        //
        // false -> FIFO normale.
        bool coalesceEvents = false;

    };

private:
    struct Entry {
        int id;
        Protocol proto;
    };

    std::vector<Entry> protocols;

    int currentOwner = -1;
    unsigned long ownerSince = 0;
    
    // ------------------------------------------------------------
    // LIVENESS
    //
    // NON viene usata dall'arbitraggio RR.
    // Serve solo al controllo salute dei protocolli.
    // ------------------------------------------------------------
    static constexpr unsigned long LIVENESS_FACTOR = 3;

    // ------------------------------------------------------------
    // PARK RECOVERY
    // ------------------------------------------------------------
    static constexpr unsigned long PARK_RECOVERY_FACTOR = 6;

    // ------------------------------------------------------------
    // ROUND ROBIN
    // ------------------------------------------------------------
    int rrIndex = 0;

    // true = il protocollo ha almeno una chiamata a tryAcquire()
    // e non è stato invalidato dal health check / parking.
    std::vector<uint8_t> protocolActive;

    int lastOwner = -1;

    //Gestione limite Socket
    SocketManager socketManager;
public:

    SocketManager& sockets()
    {
        return socketManager;
    }

    const SocketManager& sockets() const
    {
        return socketManager;
    }
    // ============================================================
    // CONFIG
    // ============================================================

    void setPacingEnabled(int id, bool enabled)
    {
        if (id < 0 || id >= (int)protocols.size())
            return;

        protocols[id].proto.pacingEnabled = enabled;
    }

    void setSlotDuration(int id, unsigned long duration)
    {
        if (id < 0 || id >= (int)protocols.size())
            return;

        protocols[id].proto.slotDuration = duration;
    }

    unsigned long getSlotDuration(int id)
    {
        if (id < 0 || id >= (int)protocols.size())
            return 0;

        return protocols[id].proto.slotDuration;
    }

    // ============================================================
    // REGISTRAZIONE DINAMICA
    // ============================================================

    int registerProtocol(
        const char* name,
        unsigned long minInterval,
        unsigned long maxDuration,
        uint8_t maxSockets = 0,
        bool coalesceEvents = false )
    {
        int id = protocols.size();

        Protocol p;

        p.lastExec = 0;
        p.lastTouch = 0;
        p.minInterval = minInterval;
        p.maxDuration = maxDuration;
        p.active = false;

        p.slotExceededCount = 0;
        p.lastSlotExceeded = 0;
        p.safeMode = false;

        p.pacingEnabled = true;

        p.slotDuration =
            (p.slotDuration == 0)
                ? maxDuration
                : p.slotDuration;

        // ========================================================
        // EVENT POLICY
        // ========================================================

        p.coalesceEvents = coalesceEvents;

        // ========================================================
        // SOCKET OWNER
        // ========================================================

        p.socketOwner =
            socketManager.registerOwner(
                name,
                maxSockets
            );

        // ========================================================
        // REGISTRA PROTOCOLLO
        // ========================================================

        protocols.push_back({ id, p });
        protocolActive.push_back(false);

        // ========================================================
        // SOCKET DEMAND
        // ========================================================

        const uint16_t demand =
            socketManager.configuredDemand();

        if (demand > SocketManager::MAX_SOCKETS)
        {
            LOG_WF(
                "NET::SOCKET",
                "WARNING: configured socket demand=%u exceeds budget=%u after '%s'",
                (unsigned)demand,
                (unsigned)SocketManager::MAX_SOCKETS,
                name ? name : "?"
            );
        }

        LOG_IF(
            "NET::SOCKET",
            "Protocol registered: id=%d name=%s socketOwner=%d "
            "demand=%u coalesceEvents=%d",
            id,
            name ? name : "?",
            p.socketOwner,
            (unsigned)maxSockets,
            p.coalesceEvents
        );

        return id;
    }

    // ============================================================
    // OWNERSHIP TIMEOUT
    // ============================================================

    inline bool isExpired(int id, unsigned long now)
    {
        if (id < 0 || id >= (int)protocols.size())
            return false;

        auto& p = protocols[id].proto;

        if (!p.active)
            return false;

        const unsigned long duration =
            now - ownerSince;

        if (duration > p.maxDuration)
        {
            LOG_WF(
                "NET::isExpired",
                "OWNER EXPIRED id=%d duration=%lu "
                "maxDuration=%lu pacing=%d",
                id,
                duration,
                p.maxDuration,
                p.pacingEnabled
            );

            return true;
        }

        return false;
    }

    // ============================================================
    // PROTOCOL HEALTH
    //
    // IMPORTANTE:
    // questo NON viene chiamato da tryAcquire().
    //
    // Serve esclusivamente a capire se un driver ha smesso
    // di chiamare tryAcquire().
    // ============================================================

    inline bool checkProtocolHealth(
        int id,
        unsigned long now)
    {
        if (id < 0 || id >= (int)protocols.size())
            return false;

        auto& p = protocols[id].proto;

        // Mai visto il protocollo
        if (!protocolActive[id])
            return false;

        // --------------------------------------------------------
        // LIVENESS BASE
        // --------------------------------------------------------

        const unsigned long baseLiveness =
            p.minInterval * LIVENESS_FACTOR;

        // --------------------------------------------------------
        // BUDGET DINAMICO
        // --------------------------------------------------------

        unsigned long runtimeBudget = 0;

        if (p.maxDuration > p.minInterval)
        {
            runtimeBudget =
                p.maxDuration - p.minInterval;
        }

        const unsigned long livenessTimeout =
            baseLiveness + runtimeBudget;

        // --------------------------------------------------------
        // CHECK
        // --------------------------------------------------------

        if ((now - p.lastTouch) > livenessTimeout)
        {
            protocolActive[id] = false;

            LOG_WF(
                "NET::Liveness",
                "Protocol %d INACTIVE - lastTouch=%lu "
                "now=%lu timeout=%lu "
                "(base=%lu runtime=%lu "
                "minInterval=%lu maxDuration=%lu)",
                id,
                p.lastTouch,
                now,
                livenessTimeout,
                baseLiveness,
                runtimeBudget,
                p.minInterval,
                p.maxDuration
            );

            return false;
        }

        return true;
    }

    // ============================================================
    // CHECK HEALTH DI TUTTI I PROTOCOLLI
    //
    // Da chiamare separatamente dal loop applicativo.
    // NON modifica rrIndex.
    // NON blocca tryAcquire().
    // ============================================================

    void checkProtocolsHealth(unsigned long now)
    {
        for (int id = 0;
             id < (int)protocols.size();
             ++id)
        {
            checkProtocolHealth(id, now);
        }
    }

    // ============================================================
    // ACQUISIZIONE
    // ============================================================

    inline bool tryAcquire(int id, unsigned long now)
    {
        if (id < 0 || id >= (int)protocols.size())
        {
            LOG_WF(
                "NET::tryAcquire",
                "DENIED INVALID id=%d protocols=%u",
                id,
                (unsigned)protocols.size()
            );

            return false;
        }

        auto& p = protocols[id].proto;

        // ========================================================
        // PROTOCOLLO PARCHEGGIATO
        // ========================================================

        if (p.runtime.state == ProtocolState::PARKED)
        {
            if (!recoverParkedProtocol(id, now))
            {
                LOG_WF(
                    "NET::tryAcquire",
                    "PARKED id=%d missingReleaseCount=%lu",
                    id,
                    p.runtime.missingReleaseCount
                );

                return false;
            }
        }

        // ========================================================
        // PROTOCOLLO VIVO
        //
        // La chiamata stessa a tryAcquire() aggiorna il touch.
        // ========================================================

        p.lastTouch = now;
        protocolActive[id] = true;

        // ========================================================
        // GIÀ OWNER
        // ========================================================

        if (currentOwner == id)
        {
            if (isExpired(id, now))
            {
                forceRelease(id, now);
                currentOwner = -1;
            }
            else
            {
                return true;
            }
        }

        // ========================================================
        // OWNER
        // ========================================================

        if (currentOwner != -1 &&
            currentOwner != id)
        {
            const int owner = currentOwner;

            // ----------------------------------------------------
            // OWNER SCADUTO
            // ----------------------------------------------------

            if (isExpired(owner, now))
            {
                parkProtocol(owner, now);
            }
            else
            {
                // ------------------------------------------------
                // SAFE MODE
                // ------------------------------------------------

                if (p.safeMode)
                {
                    currentOwner = id;
                    ownerSince = now;
                    p.active = true;

                    p.runtime.state =
                        ProtocolState::RUNNING;

                    p.runtime.acquiredAt = now;

                    return true;
                }

                // ------------------------------------------------
                // OWNER BUSY
                // ------------------------------------------------

                LOG_WF(
                    "NET::tryAcquire",
                    "BUSY id=%d owner=%d age=%lu",
                    id,
                    owner,
                    now - ownerSince
                );

                return false;
            }
        }

        // ========================================================
        // RETE LIBERA - ROUND ROBIN
        //
        // IMPORTANTE:
        // NON usiamo isProtocolAlive().
        //
        // Il RR deve decidere in base al turno.
        // La health/liveness viene controllata separatamente.
        // ========================================================

        if (currentOwner == -1)
        {
            const int count = protocols.size();

            if (count <= 0)
                return false;
            
            rrIndex =
                (id + 1) % count;
        }

        // ========================================================
        // PACING
        // ========================================================

        const unsigned long sinceExec =
            now - p.lastExec;

        if (p.pacingEnabled &&
            sinceExec < p.minInterval)
        {
            LOG_EF(
                "NET::tryAcquire",
                "DENIED PACING id=%d sinceExec=%lu minInterval=%lu",
                id,
                sinceExec,
                p.minInterval
            );
            return false;
        }

        // ========================================================
        // ACQUISIZIONE
        // ========================================================

        currentOwner = id;
        ownerSince = now;
        p.active = true;

        p.runtime.state =
            ProtocolState::RUNNING;

        p.runtime.acquiredAt =
            now;

        // ========================================================
        // ROUND ROBIN
        // ========================================================
        rrIndex =
            (id + 1) % protocols.size();

        return true;
    }

    // ============================================================
    // RILASCIO
    // ============================================================

    inline void release(
        int id,
        unsigned long now)
    {
        if (id < 0 ||
            id >= (int)protocols.size())
            return;

        if (currentOwner != id)
            return;

        auto& p = protocols[id].proto;

        p.lastExec = now;
        p.active = false;

        lastOwner = id;
        currentOwner = -1;

        p.runtime.state =
            ProtocolState::IDLE;

        p.runtime.acquiredAt = 0;
    }

    // ============================================================
    // FORCE RELEASE
    //
    // Il protocollo aveva la rete e ha superato maxDuration.
    // ============================================================

    inline void forceRelease(
        int id,
        unsigned long now)
    {
        if (id < 0 ||
            id >= (int)protocols.size())
            return;

        if (currentOwner != id)
            return;

        auto& p = protocols[id].proto;

        const unsigned long duration =
            now - ownerSince;

        p.runtime.state =
            ProtocolState::PARKED;

        p.runtime.parkedAt =
            now;

        p.runtime.missingReleaseCount++;

        p.lastExec = now;
        p.active = false;

        lastOwner = id;
        currentOwner = -1;

        protocolActive[id] = false;

        LOG_WF(
            "NET::PARK",
            "Protocol %d PARKED: missing RELEASE "
            "duration=%lu maxDuration=%lu count=%lu",
            id,
            duration,
            p.maxDuration,
            p.runtime.missingReleaseCount
        );
    }

    // ============================================================
    // PARK EXPLICITO
    // ============================================================

    inline void parkProtocol(
        int id,
        unsigned long now)
    {
        if (id < 0 ||
            id >= (int)protocols.size())
            return;

        auto& p = protocols[id].proto;

        p.runtime.missingReleaseCount++;

        p.runtime.state =
            ProtocolState::PARKED;

        p.runtime.parkedAt =
            now;

        p.runtime.acquiredAt = 0;

        p.active = false;

        lastOwner = id;
        currentOwner = -1;

        protocolActive[id] = false;

        LOG_WF(
            "NET::PARK",
            "PARK protocol=%d "
            "reason=MISSING_RELEASE count=%lu",
            id,
            p.runtime.missingReleaseCount
        );
    }

    // ============================================================
    // RECOVERY PARK
    // ============================================================

    inline bool recoverParkedProtocol(
        int id,
        unsigned long now)
    {
        if (id < 0 ||
            id >= (int)protocols.size())
            return false;

        auto& p = protocols[id].proto;

        if (p.runtime.state !=
            ProtocolState::PARKED)
        {
            return false;
        }

        const unsigned long recoveryTime =
            p.maxDuration *
            PARK_RECOVERY_FACTOR;

        const unsigned long parkedFor =
            now - p.runtime.parkedAt;

        if (parkedFor < recoveryTime)
            return false;

        LOG_WF(
            "NET::UNPARK",
            "Protocol %d UNPARKED "
            "parkedFor=%lu recovery=%lu count=%lu",
            id,
            parkedFor,
            recoveryTime,
            p.runtime.missingReleaseCount
        );

        p.runtime.state =
            ProtocolState::IDLE;

        p.runtime.acquiredAt = 0;
        p.runtime.parkedAt = 0;

        p.active = false;
        protocolActive[id] = false;

        return true;
    }

    // ============================================================
    // SAFE MODE
    // ============================================================

    void onSlotDurationExceeded(
        int id,
        unsigned long now)
    {
        if (id < 0 ||
            id >= (int)protocols.size())
            return;

        auto& p = protocols[id].proto;

        if (!p.pacingEnabled)
            return;

        LOG_DF(
            "PACING::Exceeded",
            "Protocol %d exceeded slotDuration at %lu "
            "(minInterval=%lu maxDuration=%lu safeMode=%d)",
            id,
            now,
            p.minInterval,
            p.maxDuration,
            p.safeMode
        );

        if (p.slotExceededCount == 0)
        {
            p.lastSlotExceeded = now;
        }

        p.slotExceededCount++;

        if (p.slotExceededCount >= 5 &&
            (now - p.lastSlotExceeded <= 2000))
        {
            p.safeMode = true;

            LOG_DF(
                "PACING::SafeMode",
                "SAFE MODE enabled for protocol %d "
                "(5 exceeds in %lu ms)",
                id,
                now - p.lastSlotExceeded
            );

            p.slotExceededCount = 0;
            p.lastSlotExceeded = now;
        }

        if (now - p.lastSlotExceeded > 2000)
        {
            p.slotExceededCount = 0;
            p.lastSlotExceeded = now;
        }
    }

    // ============================================================
    // STATO
    // ============================================================

    bool isFree() const
    {
        return currentOwner == -1;
    }

    Protocol& getProtocol(int id)
    {
        return protocols[id].proto;
    }

    bool shouldCoalesceEvents(int id) const
    {
        if (id < 0 ||
            id >= static_cast<int>(protocols.size()))
        {
            return false;
        }

        return protocols[id].proto.coalesceEvents;
    }

    // ============================================================
    // STATO DINAMICO DEL PROTOCOLLO
    // ============================================================

    void updateProtocolState(
        int id,
        Protocol::State state)
    {
        if (id < 0 ||
            id >= (int)protocols.size())
            return;

        auto& p = protocols[id].proto;

        p.lastState = state;

        if (!p.pacingEnabled)
            return;

        switch (state)
        {
            case Protocol::State::CYCLE_OK:
                p.safeMode = false;
                break;

            case Protocol::State::DEVICE_ERROR:
                p.minInterval += 10;
                break;

            case Protocol::State::ERROR:
                p.minInterval += 20;
                break;

            default:
                break;
        }

        if (p.minInterval > 120)
            p.minInterval = 120;
    }
};

class SocketHandle
{
private:
    int slot = -1;

    NetworkManager* network = nullptr;

    SocketManager::OwnerId owner = -1;
    int resourceId = -1;

    SocketManager::SocketKind kind =
        SocketManager::SocketKind::TCP_CLIENT;

public:

    SocketHandle() = default;

    SocketHandle(
        NetworkManager* net,
        SocketManager::OwnerId ownerId,
        int resource,
        SocketManager::SocketKind socketKind =
            SocketManager::SocketKind::TCP_CLIENT)
        : network(net),
          owner(ownerId),
          resourceId(resource),
          kind(socketKind)
    {
    }

    void setup(
        NetworkManager* net,
        SocketManager::OwnerId ownerId,
        int resource,
        SocketManager::SocketKind socketKind =
            SocketManager::SocketKind::TCP_CLIENT)
    {
        /*
         * Se il handle era già associato ad una socket,
         * il driver dovrebbe averla rilasciata prima del setup.
         */
        slot = -1;

        network = net;
        owner = ownerId;
        resourceId = resource;
        kind = socketKind;
    }

    bool acquire()
    {
        if (slot >= 0)
            return true;

        if (!network)
            return false;

        if (owner < 0)
            return false;

        slot = network->sockets().acquire(
            owner,
            resourceId,
            kind
        );

        /*
         * Nessun warning qui.
         *
         * SocketManager gestisce già:
         * - owner
         * - saturazione
         * - warning una tantum
         */
        if (slot < 0)
        {
            return false;
        }

        return true;
    }

    void release()
    {
        if (slot < 0)
            return;

        if (network)
            network->sockets().release(slot);

        slot = -1;
    }

    bool valid() const
    {
        return slot >= 0;
    }

    int get() const
    {
        return slot;
    }

    SocketManager::OwnerId getOwner() const
    {
        return owner;
    }

    int getResourceId() const
    {
        return resourceId;
    }

    SocketManager::SocketKind getKind() const
    {
        return kind;
    }
};

class IpManager {
private:
    class PriorityDeviceCursor {
    public:
        int index = -1;

        void init() {
            if (index < 0)
                index = 0;
        }

        void advance(int items) {
            index++;
            if (index >= items)
                index = 0;
        }

        void save(int deviceIndex) {
            index = deviceIndex;
        }

        int get() const {
            return index;
        }
    };
public:
    struct Config {
        unsigned long baseCooldownMs = 5000;      // ex COOLDOWN
        unsigned long extendedCooldownMs = 120000; // ex IP_COOLDOWN
        int maxErrorsBeforeCooldown = 2;          // ex MAX_ERRORS
        int maxErrorsBeforeExclude = 20;          // ex AUTO_EXCLUDE
        int maxIps = 32;                          // ex array statico 32
        int maxPriorities = PriorityCount;                    
    };
    
    struct PriorityMgmtEx {
        PriorityMgmt Base;          // 🔥 contiene Priority + DeviceIndex

        bool Interrupted;           // 🔥 READ interrotta da pacing o write
        PriorityDeviceCursor Cursor;   // ⭐ cursore per questa priorità
        
        PriorityMgmtEx() :
            Interrupted(false)
        {
            Base.deviceIndex = -1;
            Base.priority = Priority::Normal;
        }
    };

    // -------------------------
    // 1. Stato IP più chiaro
    // -------------------------
    enum class IpState {
        OK,
        COOLDOWN,
        EXCLUDED
    };

    struct structPriority {
        std::vector<PriorityMgmtEx> Priorities;   // 🔥 usa la versione estesa
        size_t PrioritySize = 0;
        int Index = 0;

        structPriority() = default;

        structPriority(int maxPriorities)
            : Priorities(maxPriorities)
        {}
    };


    struct structIP {
        arduino::IPAddress IP;
        int Errors = 0;
        IpState state = IpState::OK;

        structPriority Priority;
        int priorityCycleCounter = 0;   // 🔥 nuovo contatore round‑robin perfetto

        unsigned long lastErrorTime = 0;
        unsigned long cooldown = 0;
        bool exclude = false;
        int consecutiveErrors = 0;
        unsigned long lastCycleDuration = 0;   // 🔥 nuovo campo

        structIP(int maxPriorities, unsigned long baseCooldown)
            : Priority(maxPriorities),
            cooldown(baseCooldown)
        {}
    };


    // -------------------------
    // LOG INTELLIGENTE
    // -------------------------
    void logSkip(int ipIdx, const structIP &ip, unsigned long now) {
        // Log solo se cambia stato
        if (lastState[ipIdx] != ip.state) {
            lastState[ipIdx] = ip.state;

            if (ip.state == IpState::COOLDOWN) {
                unsigned long remaining =
                    (ip.lastErrorTime + ip.cooldown > now)
                    ? (ip.lastErrorTime + ip.cooldown - now)
                    : 0;

                LOG_WF("IpManager",
                    "IP %s in COOLDOWN (%lus rimanenti)",
                    ip.IP.toString().c_str(),
                    remaining / 1000
                );
            }
            return;
        }

        // Log periodico ogni 10 secondi
        if (now - lastCooldownLog[ipIdx] >= 10000) {
            lastCooldownLog[ipIdx] = now;

            unsigned long remaining =
                (ip.lastErrorTime + ip.cooldown > now)
                ? (ip.lastErrorTime + ip.cooldown - now)
                : 0;

            LOG_DF("IpManager",
                "Skip IP %s (cooldown %lus)",
                ip.IP.toString().c_str(),
                remaining / 1000
            );
        }
    }

    void logRestore(int ipIdx, const structIP &ip) {
    
        if (wasInCooldown[ipIdx] && ip.state == IpState::OK) {
            LOG_IF("IpManager",
                "IP %s ripristinato",
                ip.IP.toString().c_str()
            );
        }

        wasInCooldown[ipIdx] = (ip.state == IpState::COOLDOWN);
    }

    IpManager()
        : config(Config())
    {
        initArrays();
    }

    IpManager(const Config& cfg)
        : config(cfg)
    {
        initArrays();
    }
private:
    std::vector<structIP> IPs;
    Config config;
    std::unordered_map<uint32_t, std::vector<int>> devicesByIP;

    // 🔥 Array privati dinamici
    std::vector<IpState> lastState;
    std::vector<unsigned long> lastCooldownLog;
    std::vector<bool> wasInCooldown;

    void initArrays() {
        lastState.resize(config.maxIps, IpState::OK);
        lastCooldownLog.resize(config.maxIps, 0);
        wasInCooldown.resize(config.maxIps, false);
    }

    // -------------------------
    // Helpers di stato
    // -------------------------
    const char* StateToString(IpState s) const {
        switch (s) {
            case IpState::OK: return "OK";
            case IpState::COOLDOWN: return "COOLDOWN";
            case IpState::EXCLUDED: return "EXCLUDED";
        }
        return "UNKNOWN";
    }

    bool ExistsIp(arduino::IPAddress ip) {
        for (auto& item : IPs)
            if (item.IP == ip)
                return true;
        return false;
    }

    int GetUsedPriorities(arduino::IPAddress ip,
                          std::vector<GenericPrgDevice>& prgDevices,
                          std::vector<PriorityMgmt>& items)
    {
        for (auto& prgDevice : prgDevices) {
            if (prgDevice.GetIp() != ip)
                continue;

            bool exist = false;
            for (auto& it : items)
                if (it.priority == prgDevice.GetPriority())
                    exist = true;

            if (!exist) {
                PriorityMgmt tmp;
                tmp.priority = prgDevice.GetPriority();
                tmp.deviceIndex = -1;
                items.push_back(tmp);
            }
        }
        return (int)items.size();
    }

    static inline uint32_t MakeIpKey(const IPAddress& ip)
    {
        return (uint32_t(ip[0]) << 24) |
            (uint32_t(ip[1]) << 16) |
            (uint32_t(ip[2]) << 8)  |
                uint32_t(ip[3]);
    }

    void BuildDevicesByIP(std::vector<GenericPrgDevice> &prgDevices) {
        devicesByIP.clear();

        for (int i = 0; i < prgDevices.size(); i++) {
            uint32_t key = MakeIpKey(prgDevices[i].GetIp());

            devicesByIP[key].push_back(i);
        }
    }

    bool HasDevicesForPriority(
        int ipIdx,
        Priority prio,
        const GenericPrgDeviceManager& deviceManager)
    {
        const auto& ip = IPs[ipIdx];

        return deviceManager.HasDevicesByPriority(
            prio,
            ip.IP
        );
    }

    void AdvancePriority(int ipIdx, int items) {
        auto &p = IPs[ipIdx].Priority;

        LOG_DF("PRIO", 
            "AdvancePriority(ip=%d) BEFORE: Index=%d DeviceIndex=%d items=%d",
            ipIdx,
            p.Index,
            p.Priorities[p.Index].Base.deviceIndex,
            items);

        p.Priorities[p.Index].Base.deviceIndex++;

        if (p.Priorities[p.Index].Base.deviceIndex >= items ||
            p.Priorities[p.Index].Base.deviceIndex < 0)
        {
            p.Priorities[p.Index].Base.deviceIndex = 0;
        }

        if (p.Index >= p.PrioritySize - 1)
            p.Index = 0;
        else
            p.Index++;

        LOG_DF("PRIO", 
            "AdvancePriority(ip=%d) AFTER: Index=%d DeviceIndex=%d",
            ipIdx,
            p.Index,
            p.Priorities[p.Index].Base.deviceIndex);
    }


public:
    void setLastCycleDuration(int ipIdx, unsigned long duration) {
        IPs[ipIdx].lastCycleDuration = duration;
    }

    unsigned long getLastCycleDuration(int ipIdx) const {
        return IPs[ipIdx].lastCycleDuration;
    }

    // -------------------------
    // Accesso ai device per IP
    // -------------------------
    std::vector<int>* GetDevicesByIP(arduino::IPAddress &ip) {
        uint32_t key = MakeIpKey(ip);

        auto it = devicesByIP.find(key);
        if (it == devicesByIP.end())
            return nullptr;

        return &it->second;
    }

    // -------------------------
    // Costruzione lista IP
    // -------------------------
        int BuildIps(std::vector<GenericPrgDevice>& prgDevices) {
        IPs.clear();

        for (auto& prgDevice : prgDevices) {
            auto ip = prgDevice.GetIp();

            if (!ExistsIp(ip)) {
                structIP newItem(config.maxPriorities, config.baseCooldownMs);
                newItem.IP = ip;

                // Priorità
                std::vector<PriorityMgmt> tmpPriorities;
                GetUsedPriorities(ip, prgDevices, tmpPriorities);

                for (size_t j = 0; j < tmpPriorities.size(); j++) {
                    newItem.Priority.Priorities[j].Base = tmpPriorities[j];
                    newItem.Priority.Priorities[j].Interrupted = false;
                }
                newItem.Priority.PrioritySize = tmpPriorities.size();
                newItem.Priority.Index = 0;
                
                IPs.push_back(newItem);
            }
        }

        static bool devicesByIPInitialized = false;
        if (!devicesByIPInitialized) {
            BuildDevicesByIP(prgDevices);
            devicesByIPInitialized = true;
        }

        return IPs.size();
    }
    
    // -------------------------
    // Query device per IP
    // -------------------------
    bool ExistDevicesByIp(int ipIdx) const
    {
        if (ipIdx < 0 || ipIdx >= static_cast<int>(IPs.size()))
            return false;

        auto ip = IPs[ipIdx].IP;
        uint32_t key = MakeIpKey(ip);
        auto it = devicesByIP.find(key);

        return it != devicesByIP.end() && !it->second.empty();
    }

    std::vector<structIP>& GetIps() { return IPs; }
    structIP& GetIp(int ipIdx) { return IPs[ipIdx]; }

    // -------------------------
    // ShouldQuery migliorato
    // -------------------------
    bool ShouldQuery(int ipIdx, unsigned long now) {
        auto &ip = IPs[ipIdx];

        // 1. Escluso manualmente o automaticamente
        if (ip.state == IpState::EXCLUDED)
            return false;

        // 2. In cooldown → NON tentare
        if (ip.state == IpState::COOLDOWN) {
            // 🔥 log automatico
            logSkip(ipIdx, ip, now);

            if (now - ip.lastErrorTime >= ip.cooldown) {
                // 🔥 Fine cooldown → prova UNA volta
                ip.state = IpState::OK;
                ip.consecutiveErrors = 0;

                // 🔥 log automatico
                logRestore(ipIdx, ip);
                return true;
            }
            return false;
        }

        // 3. Stato OK → interrogabile
        return true;
    }


    // -------------------------
    // Gestione errori migliorata
    // -------------------------
    void ReportError(int ipIdx, unsigned long now) {
        auto &ip = IPs[ipIdx];

        LOG_EF(
                "IpManager",
                "!!! ReportError ipIdx=%d IP=%s errors=%d consecutive=%d state=%s",
                ipIdx,
                ip.IP.toString().c_str(),
                ip.Errors,
                ip.consecutiveErrors,
                StateToString(ip.state)
            );

        ip.Errors++;
        ip.consecutiveErrors++;
        ip.lastErrorTime = now;

        // 1. Auto-esclusione dopo troppi errori totali
        if (ip.Errors > config.maxErrorsBeforeExclude) {
            ip.state = IpState::EXCLUDED;
            LOG_WF("IpManager", "IP auto-escluso: %s", ip.IP.toString().c_str());
            return;
        }

        // 2. Dopo MAX_ERRORS (2) → entra in COOLDOWN
        if (ip.consecutiveErrors >= config.maxErrorsBeforeCooldown) {
            ip.state = IpState::COOLDOWN;

            // 🔥 Cooldown fisso e sicuro (evita blocchi del TCP stack)
            ip.cooldown = config.extendedCooldownMs;;   

            // 🔥 log automatico
            logSkip(ipIdx, ip, now);
            return;
        }

        // 3. Prima dei 2 errori → non entra ancora in cooldown
    }


    // -------------------------
    // Ripristino migliorato
    // -------------------------
    void ReportSuccess(int ipIdx) {
        auto &ip = IPs[ipIdx];

        ip.consecutiveErrors = 0;

        if (ip.state == IpState::COOLDOWN) {
            ip.state = IpState::OK;

            // 🔥 log automatico
            logRestore(ipIdx, ip);
        }

        if (ip.cooldown > config.baseCooldownMs)
            ip.cooldown -= 1000;

        if (ip.cooldown < config.baseCooldownMs)
            ip.cooldown = config.baseCooldownMs;
    }


    // -------------------------
    // Esclusione manuale
    // -------------------------
    void Exclude(int ipIdx) {
        auto &ip = IPs[ipIdx];
        ip.state = IpState::EXCLUDED;
    }

    // -------------------------
    // Priorità
    // -------------------------
    PriorityMgmtEx& GetCurrentPriority(
        int ipIdx,
        std::vector<GenericPrgDevice>& prgDevices,
        const GenericPrgDeviceManager& deviceManager)
    {
        auto &ip = IPs[ipIdx];
        auto &p  = ip.Priority;

        // LOG MINIMO
        LOG_DF("PRIO", "GetCurrentPriority(ip=%d)", ipIdx);

        int prio = p.Priorities[p.Index].Base.priority;

        // ============================================================
        // 1. Resume se interrupted
        // ============================================================
        for (int i = 0; i < (int)p.PrioritySize; i++) {
            if (p.Priorities[i].Interrupted) {

                if (p.Priorities[i].Base.priority == Low) {
                    LOG_WF("LOW::RESUME",
                        "ip=%d → RESUME LOW (interrupted)",
                        ipIdx);
                }

                p.Index = i;
                
                return p.Priorities[i];
            }
        }

        // ============================================================
        // 2. Se la corrente è interrupted → NON ruotare
        // ============================================================
        if (p.Priorities[p.Index].Interrupted) {

            if (p.Priorities[p.Index].Base.priority == Low) {
                LOG_WF("LOW::STAY",
                    "ip=%d → LOW still interrupted",
                    ipIdx);
            }

            auto &prioEx = p.Priorities[p.Index];

            return prioEx;
        }

        prio = p.Priorities[p.Index].Base.priority;

        // ============================================================
        // 3. Check se ci sono device per questa priorità
        // ============================================================
        const bool has = HasDevicesForPriority(
            ipIdx,
            static_cast<Priority>(prio),
            deviceManager);

        if (!has) {

            if (prio == Low) {
                LOG_WF("LOW::NODEV",
                    "ip=%d → LOW has NO DEVICES → AdvancePriority()",
                    ipIdx);
            }

            AdvancePriority(ipIdx, 0);
            return GetCurrentPriority(ipIdx, prgDevices, deviceManager);
        }

        // ============================================================
        // 4. Allow logic
        // ============================================================
        bool allow = false;

        switch (prio) {
            case High:   allow = true; break;
            case Medium: allow = (ip.priorityCycleCounter % 2 == 0); break;
            case Low:    allow = (ip.priorityCycleCounter % 3 == 0); break;
            case Normal: allow = (ip.priorityCycleCounter % 2 == 0); break;
            default:     allow = true; break;
        }

        if (!allow) {
            AdvancePriority(ipIdx, 0);
            return GetCurrentPriority(ipIdx, prgDevices, deviceManager);
        }

        // ============================================================
        // 5. SELECT PRIORITY
        // ============================================================
        auto &prioEx = p.Priorities[p.Index];

        // ⭐ inizializza il cursore solo la prima volta
        prioEx.Cursor.init();

        return prioEx;
    }


    void UpdatePriorityAfterRead(int ipIdx,
                             bool interrupted,
                             int nextDeviceIndex,
                             int items)
    {
        auto &ip = IPs[ipIdx];
        auto &p  = ip.Priority.Priorities[ip.Priority.Index];

        if (interrupted) {
            // ⭐ Caso pacing: la READ è stata interrotta
            // → salva il punto esatto da cui riprendere
            p.Interrupted = true;
            p.Base.deviceIndex = nextDeviceIndex;
            p.Cursor.save(nextDeviceIndex);     // ⭐ salva nel cursore
            return;
        }

        // ⭐ Caso normale: la priorità NON è interrotta
        // → il round‑robin ha già avanzato il cursore in computeRange()
        p.Interrupted = false;

        // ⭐ Incrementa il ciclo round‑robin delle priorità
        ip.priorityCycleCounter++;

        // ⭐ Avanza alla prossima priorità
        AdvancePriority(ipIdx, items);
    }

    // -------------------------
    // Diagnostica avanzata
    // -------------------------
    void PrintStatus() {
        for (int i = 0; i < IPs.size(); i++) {
            auto &ip = IPs[i];
            LOG_IF("IpManager",
                "IP %d: %s  Errors=%d  Cooldown=%lu  State=%s",
                i,
                ip.IP.toString().c_str(),
                ip.Errors,
                ip.cooldown,
                StateToString(ip.state)
            );
        }
    }
};

#endif
