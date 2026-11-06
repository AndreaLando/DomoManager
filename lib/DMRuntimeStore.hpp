
#ifndef DMRuntimeStore_HPP
#define DMRuntimeStore_HPP

#pragma once

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it

   Versione modulo: 1.0.0
   Ultima modifica: 2026-11-04

   Note:
                    • Runtime database complementare al Buffer numerico
                    • Accesso runtime O(1) mediante Handle
                    • Nessuna allocazione dinamica
                    • Nessun utilizzo di Arduino String
                    • Storage testuale in pool statico condiviso
                    • Supporto BOOL, INT32, FLOAT, STRING e JSON
                    • Controlli completi anti-overflow
                    • Canary di memoria e validazione integrità
                    • Dirty tracking senza callback sincrone
                    • Progettato per Introspection e Home Assistant Discovery

   ============================================================================ */

/* ============================================================================
   DESCRIZIONE
   ============================================================================

   DMRuntimeStore gestisce variabili runtime non rappresentabili nel Buffer.

   Buffer:

       area numerica -> long

   DMRuntimeStore:

       Handle -> valore tipizzato

   Esempi di variabili RuntimeStore:

       access.fanvil_ingresso.card_code
       access.last_authorized_card
       reolink.last_event
       reolink.last_person
       weather.last_alert
       mqtt.last_payload
       system.service_mode

   DMRuntimeStore NON deve essere utilizzato per:

       - ingressi Modbus;
       - uscite Modbus;
       - route;
       - toggle;
       - split;
       - automazioni numeriche;
       - variabili realtime ad alta frequenza già rappresentabili come area.

   Il percorso ottimizzato previsto è:

       setup:
           ID testuale -> define...() -> Handle

       runtime:
           Handle -> get/set O(1)

   Gli ID testuali vengono cercati solamente:

       - durante il setup;
       - durante Introspection;
       - durante il parsing di comandi esterni non ancora risolti in Handle.

   Nel ciclo runtime ordinario deve essere conservato e utilizzato l'Handle.

   ============================================================================ */

#include <Arduino.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

// ============================================================================
// DM RUNTIME STORE
// ============================================================================

class DMRuntimeStore
{
public:

    // ========================================================================
    // LIMITS
    // ========================================================================
    //
    // Handle è uint8_t.
    //
    // 0xFF è riservato a INVALID_HANDLE.
    // Il numero massimo teorico di entry è quindi 255.
    //
    // MAX_ENTRIES deve rimanere inferiore a INVALID_HANDLE.
    //
    // ========================================================================

    static constexpr size_t MAX_ENTRIES =
        64;

    static constexpr size_t TEXT_POOL_SIZE =
        4096;

    static constexpr size_t MAX_ID_LENGTH =
        64;

    static constexpr size_t MAX_NAME_LENGTH =
        80;

    static constexpr size_t MAX_UNIT_LENGTH =
        16;

    static constexpr size_t MAX_DEVICE_CLASS_LENGTH =
        32;

    static constexpr size_t MAX_STATE_CLASS_LENGTH =
        32;

    static constexpr uint32_t MEMORY_GUARD_VALUE =
        0x55AA39C7UL;

    // ========================================================================
    // COMPILE-TIME VALIDATION
    // ========================================================================

    static_assert(
        MAX_ENTRIES < 0xFF,
        "DMRuntimeStore: MAX_ENTRIES supera la capacita di Handle"
    );

    static_assert(
        TEXT_POOL_SIZE > 0,
        "DMRuntimeStore: TEXT_POOL_SIZE deve essere maggiore di zero"
    );

    static_assert(
        TEXT_POOL_SIZE <= 0xFFFF,
        "DMRuntimeStore: TEXT_POOL_SIZE supera uint16_t"
    );

    static_assert(
        MAX_ID_LENGTH >= 2,
        "DMRuntimeStore: MAX_ID_LENGTH non valido"
    );

    // ========================================================================
    // HANDLE
    // ========================================================================

    using Handle =
        uint8_t;

    static constexpr Handle INVALID_HANDLE =
        static_cast<Handle>(0xFF);

    // ========================================================================
    // TYPE
    // ========================================================================
    //
    // STRING32 / STRING64:
    //     payload testuali generici.
    //
    // JSON128 / JSON256:
    //     payload JSON opachi.
    //
    // RuntimeStore non esegue il parsing JSON.
    // Il parsing resta responsabilità del consumer.
    //
    // ========================================================================

    enum class Type : uint8_t
    {
        BOOL = 0,

        INT32,

        FLOAT,

        STRING32,

        STRING64,

        JSON128,

        JSON256
    };

    // ========================================================================
    // ACCESS
    // ========================================================================
    //
    // READ_ONLY:
    //     leggibile dai frontend;
    //     scrivibile solamente da sorgenti interne.
    //
    // WRITE_ONLY:
    //     scrivibile dai frontend;
    //     non pubblicabile come stato.
    //
    // READ_WRITE:
    //     leggibile e scrivibile.
    //
    // ========================================================================

    enum class Access : uint8_t
    {
        READ_ONLY = 0,

        WRITE_ONLY,

        READ_WRITE
    };

    // ========================================================================
    // SOURCE
    // ========================================================================
    //
    // Provenienza dell'ultima scrittura.
    //
    // Non è collegata agli ID source dell'EventManager numerico.
    //
    // ========================================================================

    enum class Source : uint8_t
    {
        UNKNOWN = 0,

        INTERNAL,

        APPLICATION,

        MQTT,

        HOME_ASSISTANT,

        WEB_API,

        HMI,

        RESTORE
    };

    // ========================================================================
    // DEFINE RESULT
    // ========================================================================

    enum class DefineResult : uint8_t
    {
        CREATED = 0,

        ALREADY_EXISTS,

        INVALID_ID,

        INVALID_NAME,

        INVALID_TYPE,

        INVALID_ACCESS,

        INVALID_METADATA,

        STORE_FULL,

        TEXT_POOL_FULL,

        INITIAL_VALUE_TOO_LONG
    };

    // ========================================================================
    // WRITE RESULT
    // ========================================================================

    enum class WriteResult : uint8_t
    {
        CHANGED = 0,

        UNCHANGED,

        INVALID_HANDLE,

        INVALID_TYPE,

        INVALID_VALUE,

        NULL_VALUE,

        VALUE_TOO_LONG,

        ACCESS_DENIED,

        MEMORY_CORRUPTION
    };

    // ========================================================================
    // READ RESULT
    // ========================================================================

    enum class ReadResult : uint8_t
    {
        OK = 0,

        INVALID_HANDLE,

        INVALID_TYPE,

        NOT_INITIALIZED,

        NOT_READABLE,

        NULL_DESTINATION,

        DESTINATION_TOO_SMALL,

        MEMORY_CORRUPTION
    };

    // ========================================================================
    // INTEGRITY RESULT
    // ========================================================================

    enum class IntegrityResult : uint8_t
    {
        OK = 0,

        BEGIN_GUARD_CORRUPTED,

        END_GUARD_CORRUPTED,

        ENTRY_COUNT_OVERFLOW,

        TEXT_POOL_OVERFLOW,

        INVALID_ENTRY,

        INVALID_TYPE,

        INVALID_ACCESS,

        INVALID_TEXT_RANGE,

        MISSING_TERMINATOR,

        DUPLICATE_ID,

        INVALID_ID
    };

    // ========================================================================
    // METADATA
    // ========================================================================
    //
    // Attenzione:
    //
    // I puntatori devono riferirsi a stringhe con durata statica.
    //
    // Utilizzare preferibilmente:
    //
    //     - string literal;
    //     - static constexpr char[];
    //     - storage globale permanente.
    //
    // Non utilizzare:
    //
    //     - String locali;
    //     - char[] allocati sullo stack;
    //     - c_str() di oggetti temporanei.
    //
    // I metadati non vengono copiati per ridurre RAM e tempo di setup.
    //
    // ========================================================================

    struct Metadata
    {
        const char* id =
            nullptr;

        const char* name =
            nullptr;

        const char* unit =
            nullptr;

        const char* deviceClass =
            nullptr;

        const char* stateClass =
            nullptr;

        Access access =
            Access::READ_WRITE;

        bool persistent =
            false;

        bool publish =
            true;

        bool sensitive =
            false;

        float minimum =
            0.0f;

        float maximum =
            100.0f;

        float step =
            1.0f;

        uint8_t decimals =
            0;
    };

    static Metadata MakeMetadata(
        const char* id,
        const char* name,
        Access access,
        bool persistent = false,
        bool publish = true,
        bool sensitive = false)
    {
        Metadata meta;

        meta.id         = id;
        meta.name       = name;
        meta.access     = access;
        meta.persistent = persistent;
        meta.publish    = publish;
        meta.sensitive  = sensitive;

        return meta;
    }
    
    static Metadata MakeRuntimeMetadata(
        const char* id,
        const char* name)
    {
        return MakeMetadata(
            id,
            name,
            Access::READ_WRITE,
            false,
            true,
            false
        );
    }

    static Metadata MakeSensitiveMetadata(
        const char* id,
        const char* name,
        bool persistent = true)
    {
        return MakeMetadata(
            id,
            name,
            Access::READ_WRITE,
            persistent,
            true,
            true
        );
    }

    // ========================================================================
    // SCALAR VALUE
    // ========================================================================

    union ScalarValue
    {
        bool booleanValue;

        int32_t integerValue;

        float floatValue;

        ScalarValue()
            : integerValue(0)
        {
        }
    };

    // ========================================================================
    // ENTRY
    // ========================================================================

    struct Entry
    {
        // --------------------------------------------------------------------
        // METADATA
        // --------------------------------------------------------------------

        const char* id =
            nullptr;

        const char* name =
            nullptr;

        const char* unit =
            nullptr;

        const char* deviceClass =
            nullptr;

        const char* stateClass =
            nullptr;

        // --------------------------------------------------------------------
        // TYPE AND ACCESS
        // --------------------------------------------------------------------

        Type type =
            Type::BOOL;

        Access access =
            Access::READ_WRITE;

        // --------------------------------------------------------------------
        // FLAGS
        // --------------------------------------------------------------------

        bool used =
            false;

        bool initialized =
            false;

        bool dirty =
            false;

        bool persistent =
            false;

        bool publish =
            true;

        bool sensitive =
            false;

        // --------------------------------------------------------------------
        // VALUE
        // --------------------------------------------------------------------

        ScalarValue scalar;

        /*
         * Per i tipi testuali:
         *
         * textOffset:
         *     posizione iniziale nel pool.
         *
         * textCapacity:
         *     dimensione totale inclusa terminazione '\0'.
         *
         * Per i tipi numerici entrambi restano zero.
         */

        uint16_t textOffset =
            0;

        uint16_t textCapacity =
            0;

        // --------------------------------------------------------------------
        // NUMBER METADATA
        // --------------------------------------------------------------------

        float minimum =
            0.0f;

        float maximum =
            100.0f;

        float step =
            1.0f;

        uint8_t decimals =
            0;

        // --------------------------------------------------------------------
        // CHANGE TRACKING
        // --------------------------------------------------------------------

        Source lastSource =
            Source::UNKNOWN;

        unsigned long changedAt =
            0;

        uint32_t revision =
            0;

        // --------------------------------------------------------------------
        // HELPERS
        // --------------------------------------------------------------------

        bool readable() const
        {
            return
                access == Access::READ_ONLY ||
                access == Access::READ_WRITE;
        }

        bool writable() const
        {
            return
                access == Access::WRITE_ONLY ||
                access == Access::READ_WRITE;
        }

        bool textBased() const
        {
            return
                type == Type::STRING32 ||
                type == Type::STRING64 ||
                type == Type::JSON128 ||
                type == Type::JSON256;
        }

        bool numeric() const
        {
            return
                type == Type::BOOL ||
                type == Type::INT32 ||
                type == Type::FLOAT;
        }
    };

private:

    // ========================================================================
    // MEMORY GUARDS
    // ========================================================================
    //
    // I guard rilevano scritture fuori limite che raggiungono i bordi
    // principali dello store.
    //
    // Non sostituiscono i controlli di bounds.
    //
    // ========================================================================

    uint32_t _beginGuard =
        MEMORY_GUARD_VALUE;

    // ========================================================================
    // ENTRY STORAGE
    // ========================================================================

    Entry _entries[
        MAX_ENTRIES
    ];

    uint8_t _count =
        0;

    // ========================================================================
    // TEXT STORAGE
    // ========================================================================
    //
    // Pool bump-only:
    //
    //     - allocazione solamente in fase define;
    //     - nessuna frammentazione;
    //     - nessuna free;
    //     - nessuna riallocazione;
    //     - address stability garantita.
    //
    // Le entry non vengono rimosse singolarmente.
    // clear() ricostruisce completamente lo store.
    //
    // ========================================================================

    char _textPool[
        TEXT_POOL_SIZE
    ];

    uint16_t _textUsed =
        0;

    // ========================================================================
    // GLOBAL REVISION
    // ========================================================================

    uint32_t _revision =
        0;

    uint32_t _endGuard =
        MEMORY_GUARD_VALUE;

public:

    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    DMRuntimeStore()
    {
        clear();
    }

    DMRuntimeStore(
        const DMRuntimeStore&) = delete;

    DMRuntimeStore& operator=(
        const DMRuntimeStore&) = delete;

    // ========================================================================
    // CLEAR
    // ========================================================================
    //
    // Ricostruisce completamente store e pool.
    //
    // Gli Handle precedentemente restituiti diventano invalidi.
    //
    // Non invocare clear() mentre altri componenti mantengono Handle attivi.
    //
    // ========================================================================

    void clear()
    {
        _beginGuard =
            MEMORY_GUARD_VALUE;

        _endGuard =
            MEMORY_GUARD_VALUE;

        for (size_t i = 0;
             i < MAX_ENTRIES;
             ++i)
        {
            _entries[i] =
                Entry{};
        }

        memset(
            _textPool,
            0,
            sizeof(_textPool)
        );

        _count =
            0;

        _textUsed =
            0;

        _revision =
            0;
    }

    // ========================================================================
    // DEFINE BOOL
    // ========================================================================

    Handle defineBool(
        const Metadata& metadata,
        bool initialValue = false,
        DefineResult* result = nullptr)
    {
        const Handle handle =
            defineEntry(
                Type::BOOL,
                metadata,
                result
            );

        if (handle ==
            INVALID_HANDLE)
        {
            return INVALID_HANDLE;
        }

        Entry& entry =
            _entries[handle];

        entry.scalar.booleanValue =
            initialValue;

        initializeEntry(
            entry,
            Source::INTERNAL,
            0
        );

        return handle;
    }

    // ========================================================================
    // DEFINE INT32
    // ========================================================================

    Handle defineInt32(
        const Metadata& metadata,
        int32_t initialValue = 0,
        DefineResult* result = nullptr)
    {
        const Handle handle =
            defineEntry(
                Type::INT32,
                metadata,
                result
            );

        if (handle ==
            INVALID_HANDLE)
        {
            return INVALID_HANDLE;
        }

        Entry& entry =
            _entries[handle];

        entry.scalar.integerValue =
            initialValue;

        initializeEntry(
            entry,
            Source::INTERNAL,
            0
        );

        return handle;
    }

    // ========================================================================
    // DEFINE FLOAT
    // ========================================================================

    Handle defineFloat(
        const Metadata& metadata,
        float initialValue = 0.0f,
        DefineResult* result = nullptr)
    {
        if (!isfinite(initialValue))
        {
            setResult(
                result,
                DefineResult::INVALID_METADATA
            );

            return INVALID_HANDLE;
        }

        const Handle handle =
            defineEntry(
                Type::FLOAT,
                metadata,
                result
            );

        if (handle ==
            INVALID_HANDLE)
        {
            return INVALID_HANDLE;
        }

        Entry& entry =
            _entries[handle];

        entry.scalar.floatValue =
            initialValue;

        initializeEntry(
            entry,
            Source::INTERNAL,
            0
        );

        return handle;
    }

    // ========================================================================
    // DEFINE STRING32
    // ========================================================================

    Handle defineString32(
        const Metadata& metadata,
        const char* initialValue = "",
        DefineResult* result = nullptr)
    {
        return defineText(
            Type::STRING32,
            metadata,
            initialValue,
            result
        );
    }

    // ========================================================================
    // DEFINE STRING64
    // ========================================================================

    Handle defineString64(
        const Metadata& metadata,
        const char* initialValue = "",
        DefineResult* result = nullptr)
    {
        return defineText(
            Type::STRING64,
            metadata,
            initialValue,
            result
        );
    }

    // ========================================================================
    // DEFINE JSON128
    // ========================================================================

    Handle defineJson128(
        const Metadata& metadata,
        const char* initialValue = "{}",
        DefineResult* result = nullptr)
    {
        return defineText(
            Type::JSON128,
            metadata,
            initialValue,
            result
        );
    }

    // ========================================================================
    // DEFINE JSON256
    // ========================================================================

    Handle defineJson256(
        const Metadata& metadata,
        const char* initialValue = "{}",
        DefineResult* result = nullptr)
    {
        return defineText(
            Type::JSON256,
            metadata,
            initialValue,
            result
        );
    }

    // ========================================================================
    // SET BOOL
    // ========================================================================

    WriteResult setBool(
        Handle handle,
        bool value,
        Source source = Source::INTERNAL,
        unsigned long now = 0)
    {
        Entry* entry =
            validatedEntry(
                handle,
                Type::BOOL
            );

        if (!entry)
        {
            return invalidEntryResult(
                handle,
                Type::BOOL
            );
        }

        if (!canWrite(
                *entry,
                source))
        {
            return
                WriteResult::ACCESS_DENIED;
        }

        if (entry->initialized &&
            entry->scalar.booleanValue ==
                value)
        {
            return
                WriteResult::UNCHANGED;
        }

        entry->scalar.booleanValue =
            value;

        commitChange(
            *entry,
            source,
            now
        );

        return
            WriteResult::CHANGED;
    }

    // ========================================================================
    // SET INT32
    // ========================================================================

    WriteResult setInt32(
        Handle handle,
        int32_t value,
        Source source = Source::INTERNAL,
        unsigned long now = 0)
    {
        Entry* entry =
            validatedEntry(
                handle,
                Type::INT32
            );

        if (!entry)
        {
            return invalidEntryResult(
                handle,
                Type::INT32
            );
        }

        if (!canWrite(
                *entry,
                source))
        {
            return
                WriteResult::ACCESS_DENIED;
        }

        if (entry->initialized &&
            entry->scalar.integerValue ==
                value)
        {
            return
                WriteResult::UNCHANGED;
        }

        entry->scalar.integerValue =
            value;

        commitChange(
            *entry,
            source,
            now
        );

        return
            WriteResult::CHANGED;
    }

    // ========================================================================
    // SET FLOAT
    // ========================================================================

    WriteResult setFloat(
        Handle handle,
        float value,
        Source source = Source::INTERNAL,
        unsigned long now = 0)
    {
        Entry* entry =
            validatedEntry(
                handle,
                Type::FLOAT
            );

        if (!entry)
        {
            return invalidEntryResult(
                handle,
                Type::FLOAT
            );
        }

        if (!canWrite(
                *entry,
                source))
        {
            return
                WriteResult::ACCESS_DENIED;
        }

        if (!isfinite(value))
        {
            return
                WriteResult::INVALID_VALUE;
        }

        if (entry->initialized &&
            entry->scalar.floatValue ==
                value)
        {
            return
                WriteResult::UNCHANGED;
        }

        entry->scalar.floatValue =
            value;

        commitChange(
            *entry,
            source,
            now
        );

        return
            WriteResult::CHANGED;
    }

    // ========================================================================
    // SET STRING
    // ========================================================================

    WriteResult setString(
        Handle handle,
        const char* value,
        Source source = Source::INTERNAL,
        unsigned long now = 0)
    {
        if (!isValidHandle(handle))
        {
            return memoryValid()
                ? WriteResult::INVALID_HANDLE
                : WriteResult::MEMORY_CORRUPTION;
        }

        const Type type =
            _entries[handle].type;

        if (type != Type::STRING32 &&
            type != Type::STRING64)
        {
            return
                WriteResult::INVALID_TYPE;
        }

        return setText(
            handle,
            value,
            source,
            now
        );
    }

    // ========================================================================
    // SET JSON
    // ========================================================================

    WriteResult setJson(
        Handle handle,
        const char* value,
        Source source = Source::INTERNAL,
        unsigned long now = 0)
    {
        if (!isValidHandle(handle))
        {
            return memoryValid()
                ? WriteResult::INVALID_HANDLE
                : WriteResult::MEMORY_CORRUPTION;
        }

        const Type type =
            _entries[handle].type;

        if (type != Type::JSON128 &&
            type != Type::JSON256)
        {
            return
                WriteResult::INVALID_TYPE;
        }

        return setText(
            handle,
            value,
            source,
            now
        );
    }

    // ========================================================================
    // GET BOOL
    // ========================================================================

    bool getBool(
        Handle handle,
        bool fallback = false) const
    {
        const Entry* entry =
            validatedReadableEntry(
                handle,
                Type::BOOL
            );

        return entry
            ? entry->scalar.booleanValue
            : fallback;
    }

    // ========================================================================
    // GET INT32
    // ========================================================================

    int32_t getInt32(
        Handle handle,
        int32_t fallback = 0) const
    {
        const Entry* entry =
            validatedReadableEntry(
                handle,
                Type::INT32
            );

        return entry
            ? entry->scalar.integerValue
            : fallback;
    }

    // ========================================================================
    // GET FLOAT
    // ========================================================================

    float getFloat(
        Handle handle,
        float fallback = 0.0f) const
    {
        const Entry* entry =
            validatedReadableEntry(
                handle,
                Type::FLOAT
            );

        return entry
            ? entry->scalar.floatValue
            : fallback;
    }

    // ========================================================================
    // GET STRING
    // ========================================================================

    const char* getString(
        Handle handle,
        const char* fallback = "") const
    {
        if (!isValidHandle(handle))
        {
            return safeFallback(
                fallback
            );
        }

        const Entry& entry =
            _entries[handle];

        if (entry.type !=
                Type::STRING32 &&
            entry.type !=
                Type::STRING64)
        {
            return safeFallback(
                fallback
            );
        }

        return readableText(
            entry,
            fallback
        );
    }

    // ========================================================================
    // GET JSON
    // ========================================================================

    const char* getJson(
        Handle handle,
        const char* fallback = "{}") const
    {
        if (!isValidHandle(handle))
        {
            return safeFallback(
                fallback
            );
        }

        const Entry& entry =
            _entries[handle];

        if (entry.type !=
                Type::JSON128 &&
            entry.type !=
                Type::JSON256)
        {
            return safeFallback(
                fallback
            );
        }

        return readableText(
            entry,
            fallback
        );
    }

    // ========================================================================
    // COPY STRING
    // ========================================================================
    //
    // Utilizzare quando il chiamante deve conservare il valore oltre la
    // singola espressione.
    //
    // ========================================================================

    ReadResult copyString(
        Handle handle,
        char* destination,
        size_t destinationSize) const
    {
        if (!destination)
        {
            return
                ReadResult::NULL_DESTINATION;
        }

        if (destinationSize == 0)
        {
            return
                ReadResult::DESTINATION_TOO_SMALL;
        }

        destination[0] =
            '\0';

        if (!memoryValid())
        {
            return
                ReadResult::MEMORY_CORRUPTION;
        }

        if (!isValidHandle(handle))
        {
            return
                ReadResult::INVALID_HANDLE;
        }

        const Entry& entry =
            _entries[handle];

        if (entry.type !=
                Type::STRING32 &&
            entry.type !=
                Type::STRING64)
        {
            return
                ReadResult::INVALID_TYPE;
        }

        return copyTextValue(
            entry,
            destination,
            destinationSize
        );
    }

    // ========================================================================
    // COPY JSON
    // ========================================================================

    ReadResult copyJson(
        Handle handle,
        char* destination,
        size_t destinationSize) const
    {
        if (!destination)
        {
            return
                ReadResult::NULL_DESTINATION;
        }

        if (destinationSize == 0)
        {
            return
                ReadResult::DESTINATION_TOO_SMALL;
        }

        destination[0] =
            '\0';

        if (!memoryValid())
        {
            return
                ReadResult::MEMORY_CORRUPTION;
        }

        if (!isValidHandle(handle))
        {
            return
                ReadResult::INVALID_HANDLE;
        }

        const Entry& entry =
            _entries[handle];

        if (entry.type !=
                Type::JSON128 &&
            entry.type !=
                Type::JSON256)
        {
            return
                ReadResult::INVALID_TYPE;
        }

        return copyTextValue(
            entry,
            destination,
            destinationSize
        );
    }

    // ========================================================================
    // FIND BY ID
    // ========================================================================
    //
    // Operazione O(N).
    //
    // Destinata a:
    //
    //     - setup;
    //     - Introspection;
    //     - parsing di command topic;
    //     - diagnostica.
    //
    // Non utilizzare nel loop realtime se l'Handle è già noto.
    //
    // ========================================================================

    Handle find(
        const char* id) const
    {
        if (!memoryValid() ||
            !validId(id))
        {
            return
                INVALID_HANDLE;
        }

        for (uint8_t i = 0;
             i < _count;
             ++i)
        {
            const Entry& entry =
                _entries[i];

            if (!entry.used ||
                !entry.id)
            {
                continue;
            }

            if (strcmp(
                    entry.id,
                    id) == 0)
            {
                return
                    static_cast<Handle>(i);
            }
        }

        return
            INVALID_HANDLE;
    }

    // ========================================================================
    // ENTRY ACCESSORS
    // ========================================================================

    Entry* getEntry(
        Handle handle)
    {
        if (!isValidHandle(handle))
            return nullptr;

        return
            &_entries[handle];
    }

    const Entry* getEntry(
        Handle handle) const
    {
        if (!isValidHandle(handle))
            return nullptr;

        return
            &_entries[handle];
    }

    // ========================================================================
    // DIRTY TRACKING
    // ========================================================================

    bool isDirty(
        Handle handle) const
    {
        if (!isValidHandle(handle))
            return false;

        return
            _entries[handle].dirty;
    }

    bool clearDirty(
        Handle handle)
    {
        if (!isValidHandle(handle))
            return false;

        _entries[handle].dirty =
            false;

        return true;
    }

    void clearAllDirty()
    {
        if (!memoryValid())
            return;

        for (uint8_t i = 0;
             i < _count;
             ++i)
        {
            _entries[i].dirty =
                false;
        }
    }

    size_t dirtyCount() const
    {
        if (!memoryValid())
            return 0;

        size_t result =
            0;

        for (uint8_t i = 0;
             i < _count;
             ++i)
        {
            if (_entries[i].used &&
                _entries[i].dirty)
            {
                ++result;
            }
        }

        return result;
    }

    // ========================================================================
    // RESET VALUE
    // ========================================================================

    WriteResult reset(
        Handle handle,
        Source source = Source::INTERNAL,
        unsigned long now = 0)
    {
        if (!isValidHandle(handle))
        {
            return memoryValid()
                ? WriteResult::INVALID_HANDLE
                : WriteResult::MEMORY_CORRUPTION;
        }

        Entry& entry =
            _entries[handle];

        if (!canWrite(
                entry,
                source))
        {
            return
                WriteResult::ACCESS_DENIED;
        }

        switch (entry.type)
        {
            case Type::BOOL:
            {
                return setBool(
                    handle,
                    false,
                    source,
                    now
                );
            }

            case Type::INT32:
            {
                return setInt32(
                    handle,
                    0,
                    source,
                    now
                );
            }

            case Type::FLOAT:
            {
                return setFloat(
                    handle,
                    0.0f,
                    source,
                    now
                );
            }

            case Type::STRING32:
            case Type::STRING64:
            {
                return setString(
                    handle,
                    "",
                    source,
                    now
                );
            }

            case Type::JSON128:
            case Type::JSON256:
            {
                return setJson(
                    handle,
                    "{}",
                    source,
                    now
                );
            }
        }

        return
            WriteResult::INVALID_TYPE;
    }

    // ========================================================================
    // GLOBAL ACCESSORS
    // ========================================================================

    size_t count() const
    {
        return memoryValid()
            ? static_cast<size_t>(_count)
            : 0;
    }

    size_t capacity() const
    {
        return
            MAX_ENTRIES;
    }

    size_t textUsed() const
    {
        return memoryValid()
            ? static_cast<size_t>(_textUsed)
            : 0;
    }

    size_t textAvailable() const
    {
        if (!memoryValid() ||
            _textUsed > TEXT_POOL_SIZE)
        {
            return 0;
        }

        return
            TEXT_POOL_SIZE -
            static_cast<size_t>(_textUsed);
    }

    uint32_t revision() const
    {
        return memoryValid()
            ? _revision
            : 0;
    }

    bool empty() const
    {
        return
            count() == 0;
    }

    bool full() const
    {
        return
            count() >= MAX_ENTRIES;
    }

    // ========================================================================
    // MEMORY VALIDATION
    // ========================================================================

    bool memoryValid() const
    {
        return
            _beginGuard ==
                MEMORY_GUARD_VALUE &&
            _endGuard ==
                MEMORY_GUARD_VALUE;
    }

    // ========================================================================
    // FULL INTEGRITY VALIDATION
    // ========================================================================

    IntegrityResult validateIntegrity(
        bool logErrors = true) const
    {
        if (_beginGuard !=
            MEMORY_GUARD_VALUE)
        {
            logIntegrityError(
                logErrors,
                "Begin guard corrupted"
            );

            return
                IntegrityResult::
                    BEGIN_GUARD_CORRUPTED;
        }

        if (_endGuard !=
            MEMORY_GUARD_VALUE)
        {
            logIntegrityError(
                logErrors,
                "End guard corrupted"
            );

            return
                IntegrityResult::
                    END_GUARD_CORRUPTED;
        }

        if (_count >
            MAX_ENTRIES)
        {
            logIntegrityError(
                logErrors,
                "Entry count overflow"
            );

            return
                IntegrityResult::
                    ENTRY_COUNT_OVERFLOW;
        }

        if (_textUsed >
            TEXT_POOL_SIZE)
        {
            logIntegrityError(
                logErrors,
                "Text pool overflow"
            );

            return
                IntegrityResult::
                    TEXT_POOL_OVERFLOW;
        }

        for (uint8_t i = 0;
             i < _count;
             ++i)
        {
            const Entry& entry =
                _entries[i];

            if (!entry.used)
            {
                logIntegrityError(
                    logErrors,
                    "Unused entry inside active range"
                );

                return
                    IntegrityResult::
                        INVALID_ENTRY;
            }

            if (!validId(
                    entry.id))
            {
                logIntegrityError(
                    logErrors,
                    "Invalid entry ID"
                );

                return
                    IntegrityResult::
                        INVALID_ID;
            }

            if (!validType(
                    entry.type))
            {
                logIntegrityError(
                    logErrors,
                    "Invalid entry type"
                );

                return
                    IntegrityResult::
                        INVALID_TYPE;
            }

            if (!validAccess(
                    entry.access))
            {
                logIntegrityError(
                    logErrors,
                    "Invalid entry access"
                );

                return
                    IntegrityResult::
                        INVALID_ACCESS;
            }

            if (entry.textBased())
            {
                const size_t offset =
                    entry.textOffset;

                const size_t capacity =
                    entry.textCapacity;

                if (capacity == 0 ||
                    offset >= TEXT_POOL_SIZE ||
                    capacity >
                        TEXT_POOL_SIZE - offset)
                {
                    logIntegrityError(
                        logErrors,
                        "Invalid text range"
                    );

                    return
                        IntegrityResult::
                            INVALID_TEXT_RANGE;
                }

                const char* text =
                    &_textPool[offset];

                if (!hasTerminator(
                        text,
                        capacity))
                {
                    logIntegrityError(
                        logErrors,
                        "Missing text terminator"
                    );

                    return
                        IntegrityResult::
                            MISSING_TERMINATOR;
                }
            }
            else
            {
                if (entry.textOffset != 0 ||
                    entry.textCapacity != 0)
                {
                    logIntegrityError(
                        logErrors,
                        "Numeric entry owns text range"
                    );

                    return
                        IntegrityResult::
                            INVALID_TEXT_RANGE;
                }
            }

            for (uint8_t j = 0;
                 j < i;
                 ++j)
            {
                const Entry& previous =
                    _entries[j];

                if (previous.used &&
                    previous.id &&
                    strcmp(
                        previous.id,
                        entry.id) == 0)
                {
                    logIntegrityError(
                        logErrors,
                        "Duplicate entry ID"
                    );

                    return
                        IntegrityResult::
                            DUPLICATE_ID;
                }
            }
        }

        return
            IntegrityResult::OK;
    }

    // ========================================================================
    // ITERATION
    // ========================================================================

    template<typename F>
    void forEach(F fn)
    {
        if (!memoryValid())
            return;

        for (uint8_t i = 0;
             i < _count;
             ++i)
        {
            fn(
                static_cast<Handle>(i),
                _entries[i]
            );
        }
    }

    template<typename F>
    void forEach(F fn) const
    {
        if (!memoryValid())
            return;

        for (uint8_t i = 0;
             i < _count;
             ++i)
        {
            fn(
                static_cast<Handle>(i),
                _entries[i]
            );
        }
    }

    // ========================================================================
    // STRING HELPERS
    // ========================================================================

    static const char* typeToString(
        Type type)
    {
        switch (type)
        {
            case Type::BOOL:
                return "BOOL";

            case Type::INT32:
                return "INT32";

            case Type::FLOAT:
                return "FLOAT";

            case Type::STRING32:
                return "STRING32";

            case Type::STRING64:
                return "STRING64";

            case Type::JSON128:
                return "JSON128";

            case Type::JSON256:
                return "JSON256";
        }

        return "UNKNOWN";
    }

    static const char* accessToString(
        Access access)
    {
        switch (access)
        {
            case Access::READ_ONLY:
                return "READ_ONLY";

            case Access::WRITE_ONLY:
                return "WRITE_ONLY";

            case Access::READ_WRITE:
                return "READ_WRITE";
        }

        return "UNKNOWN";
    }

    static const char* sourceToString(
        Source source)
    {
        switch (source)
        {
            case Source::UNKNOWN:
                return "UNKNOWN";

            case Source::INTERNAL:
                return "INTERNAL";

            case Source::APPLICATION:
                return "APPLICATION";

            case Source::MQTT:
                return "MQTT";

            case Source::HOME_ASSISTANT:
                return "HOME_ASSISTANT";

            case Source::WEB_API:
                return "WEB_API";

            case Source::HMI:
                return "HMI";

            case Source::RESTORE:
                return "RESTORE";
        }

        return "UNKNOWN";
    }

private:

    // ========================================================================
    // DEFINE ENTRY
    // ========================================================================

    Handle defineEntry(
        Type type,
        const Metadata& metadata,
        DefineResult* result)
    {
        if (!memoryValid())
        {
            setResult(
                result,
                DefineResult::
                    INVALID_METADATA
            );

            return
                INVALID_HANDLE;
        }

        if (!validId(
                metadata.id))
        {
            setResult(
                result,
                DefineResult::INVALID_ID
            );

            return
                INVALID_HANDLE;
        }

        if (metadata.name &&
            boundedLength(
                metadata.name,
                MAX_NAME_LENGTH) >=
                    MAX_NAME_LENGTH)
        {
            setResult(
                result,
                DefineResult::INVALID_NAME
            );

            return
                INVALID_HANDLE;
        }

        if (!validOptionalText(
                metadata.unit,
                MAX_UNIT_LENGTH) ||
            !validOptionalText(
                metadata.deviceClass,
                MAX_DEVICE_CLASS_LENGTH) ||
            !validOptionalText(
                metadata.stateClass,
                MAX_STATE_CLASS_LENGTH))
        {
            setResult(
                result,
                DefineResult::INVALID_METADATA
            );

            return
                INVALID_HANDLE;
        }

        if (!validType(type))
        {
            setResult(
                result,
                DefineResult::INVALID_TYPE
            );

            return
                INVALID_HANDLE;
        }

        if (!validAccess(
                metadata.access))
        {
            setResult(
                result,
                DefineResult::INVALID_ACCESS
            );

            return
                INVALID_HANDLE;
        }

        if (!validNumberMetadata(
                type,
                metadata))
        {
            setResult(
                result,
                DefineResult::INVALID_METADATA
            );

            return
                INVALID_HANDLE;
        }

        if (find(metadata.id) !=
            INVALID_HANDLE)
        {
            setResult(
                result,
                DefineResult::ALREADY_EXISTS
            );

            return
                INVALID_HANDLE;
        }

        if (_count >=
            MAX_ENTRIES)
        {
            setResult(
                result,
                DefineResult::STORE_FULL
            );

            return
                INVALID_HANDLE;
        }

        const Handle handle =
            static_cast<Handle>(
                _count
            );

        Entry& entry =
            _entries[handle];

        entry =
            Entry{};

        entry.id =
            metadata.id;

        entry.name =
            metadata.name
                ? metadata.name
                : metadata.id;

        entry.unit =
            metadata.unit;

        entry.deviceClass =
            metadata.deviceClass;

        entry.stateClass =
            metadata.stateClass;

        entry.type =
            type;

        entry.access =
            metadata.access;

        entry.persistent =
            metadata.persistent;

        entry.publish =
            metadata.publish;

        entry.sensitive =
            metadata.sensitive;

        entry.minimum =
            metadata.minimum;

        entry.maximum =
            metadata.maximum;

        entry.step =
            metadata.step;

        entry.decimals =
            metadata.decimals;

        entry.used =
            true;

        entry.initialized =
            false;

        entry.dirty =
            false;

        entry.lastSource =
            Source::UNKNOWN;

        entry.changedAt =
            0;

        entry.revision =
            0;

        ++_count;

        setResult(
            result,
            DefineResult::CREATED
        );

        return
            handle;
    }

    // ========================================================================
    // DEFINE TEXT
    // ========================================================================

    Handle defineText(
        Type type,
        const Metadata& metadata,
        const char* initialValue,
        DefineResult* result)
    {
        if (!initialValue)
        {
            initialValue =
                "";
        }

        const uint16_t capacity =
            capacityForType(type);

        if (capacity == 0)
        {
            setResult(
                result,
                DefineResult::INVALID_TYPE
            );

            return
                INVALID_HANDLE;
        }

        const size_t initialLength =
            boundedLength(
                initialValue,
                capacity
            );

        if (initialLength >=
            capacity)
        {
            setResult(
                result,
                DefineResult::
                    INITIAL_VALUE_TOO_LONG
            );

            return
                INVALID_HANDLE;
        }

        if (capacity >
            TEXT_POOL_SIZE - _textUsed)
        {
            setResult(
                result,
                DefineResult::TEXT_POOL_FULL
            );

            return
                INVALID_HANDLE;
        }

        const Handle handle =
            defineEntry(
                type,
                metadata,
                result
            );

        if (handle ==
            INVALID_HANDLE)
        {
            return
                INVALID_HANDLE;
        }

        Entry& entry =
            _entries[handle];

        entry.textOffset =
            _textUsed;

        entry.textCapacity =
            capacity;

        char* destination =
            &_textPool[
                entry.textOffset
            ];

        memset(
            destination,
            0,
            capacity
        );

        memcpy(
            destination,
            initialValue,
            initialLength
        );

        destination[
            initialLength
        ] = '\0';

        _textUsed =
            static_cast<uint16_t>(
                _textUsed +
                capacity
            );

        initializeEntry(
            entry,
            Source::INTERNAL,
            0
        );

        return
            handle;
    }

    // ========================================================================
    // SET TEXT
    // ========================================================================

    WriteResult setText(
        Handle handle,
        const char* value,
        Source source,
        unsigned long now)
    {
        if (!memoryValid())
        {
            return
                WriteResult::MEMORY_CORRUPTION;
        }

        if (!isValidHandle(handle))
        {
            return
                WriteResult::INVALID_HANDLE;
        }

        if (!value)
        {
            return
                WriteResult::NULL_VALUE;
        }

        Entry& entry =
            _entries[handle];

        if (!entry.textBased())
        {
            return
                WriteResult::INVALID_TYPE;
        }

        if (!canWrite(
                entry,
                source))
        {
            return
                WriteResult::ACCESS_DENIED;
        }

        if (!validTextRange(entry))
        {
            return
                WriteResult::MEMORY_CORRUPTION;
        }

        const size_t length =
            boundedLength(
                value,
                entry.textCapacity
            );

        if (length >=
            entry.textCapacity)
        {
            return
                WriteResult::VALUE_TOO_LONG;
        }

        char* destination =
            &_textPool[
                entry.textOffset
            ];

        if (entry.initialized &&
            strcmp(
                destination,
                value) == 0)
        {
            return
                WriteResult::UNCHANGED;
        }

        /*
         * Pulizia completa della slot.
         *
         * Impedisce che dati precedenti restino presenti oltre il nuovo
         * terminatore e facilita le verifiche diagnostiche.
         */

        memset(
            destination,
            0,
            entry.textCapacity
        );

        memcpy(
            destination,
            value,
            length
        );

        destination[
            length
        ] = '\0';

        commitChange(
            entry,
            source,
            now
        );

        return
            WriteResult::CHANGED;
    }

    // ========================================================================
    // INITIALIZE ENTRY
    // ========================================================================

    static void initializeEntry(
        Entry& entry,
        Source source,
        unsigned long now)
    {
        entry.initialized =
            true;

        entry.dirty =
            false;

        entry.lastSource =
            source;

        entry.changedAt =
            now;

        entry.revision =
            0;
    }

    // ========================================================================
    // COMMIT CHANGE
    // ========================================================================

    void commitChange(
        Entry& entry,
        Source source,
        unsigned long now)
    {
        entry.initialized =
            true;

        entry.dirty =
            true;

        entry.lastSource =
            source;

        entry.changedAt =
            now;

        incrementRevision();

        entry.revision =
            _revision;
    }

    // ========================================================================
    // REVISION MANAGEMENT
    // ========================================================================

    void incrementRevision()
    {
        ++_revision;

        /*
         * Revisione zero è riservata agli elementi mai modificati.
         */

        if (_revision == 0)
        {
            _revision =
                1;
        }
    }

    // ========================================================================
    // HANDLE VALIDATION
    // ========================================================================

    bool isValidHandle(
        Handle handle) const
    {
        if (!memoryValid())
            return false;

        if (handle ==
            INVALID_HANDLE)
        {
            return false;
        }

        if (handle >=
            _count)
        {
            return false;
        }

        return
            _entries[handle].used;
    }

    // ========================================================================
    // VALIDATED ENTRY
    // ========================================================================

    Entry* validatedEntry(
        Handle handle,
        Type expectedType)
    {
        if (!isValidHandle(handle))
        {
            return nullptr;
        }

        Entry& entry =
            _entries[handle];

        if (entry.type !=
            expectedType)
        {
            return nullptr;
        }

        return
            &entry;
    }

    const Entry* validatedReadableEntry(
        Handle handle,
        Type expectedType) const
    {
        if (!isValidHandle(handle))
        {
            return nullptr;
        }

        const Entry& entry =
            _entries[handle];

        if (entry.type !=
            expectedType)
        {
            return nullptr;
        }

        if (!entry.initialized ||
            !entry.readable())
        {
            return nullptr;
        }

        return
            &entry;
    }

    WriteResult invalidEntryResult(
        Handle handle,
        Type expectedType) const
    {
        if (!memoryValid())
        {
            return
                WriteResult::MEMORY_CORRUPTION;
        }

        if (!isValidHandle(handle))
        {
            return
                WriteResult::INVALID_HANDLE;
        }

        if (_entries[handle].type !=
            expectedType)
        {
            return
                WriteResult::INVALID_TYPE;
        }

        return
            WriteResult::INVALID_HANDLE;
    }

    // ========================================================================
    // READABLE TEXT
    // ========================================================================

    const char* readableText(
        const Entry& entry,
        const char* fallback) const
    {
        if (!memoryValid() ||
            !entry.initialized ||
            !entry.readable() ||
            !validTextRange(entry))
        {
            return
                safeFallback(
                    fallback
                );
        }

        return
            &_textPool[
                entry.textOffset
            ];
    }

    // ========================================================================
    // COPY TEXT VALUE
    // ========================================================================

    ReadResult copyTextValue(
        const Entry& entry,
        char* destination,
        size_t destinationSize) const
    {
        if (!entry.initialized)
        {
            return
                ReadResult::NOT_INITIALIZED;
        }

        if (!entry.readable())
        {
            return
                ReadResult::NOT_READABLE;
        }

        if (!validTextRange(entry))
        {
            return
                ReadResult::MEMORY_CORRUPTION;
        }

        const char* source =
            &_textPool[
                entry.textOffset
            ];

        const size_t length =
            boundedLength(
                source,
                entry.textCapacity
            );

        if (length >=
            entry.textCapacity)
        {
            return
                ReadResult::MEMORY_CORRUPTION;
        }

        if (length >=
            destinationSize)
        {
            return
                ReadResult::
                    DESTINATION_TOO_SMALL;
        }

        memcpy(
            destination,
            source,
            length + 1
        );

        return
            ReadResult::OK;
    }

    // ========================================================================
    // ACCESS POLICY
    // ========================================================================

    static bool canWrite(
        const Entry& entry,
        Source source)
    {
        /*
         * Le sorgenti interne possono aggiornare anche variabili READ_ONLY.
         *
         * Questo consente, ad esempio:
         *
         *     - aggiornamenti da un motore interno;
         *     - restore da persistenza;
         *     - aggiornamenti applicativi;
         *     - valori diagnostici.
         */

        if (source ==
                Source::INTERNAL ||
            source ==
                Source::APPLICATION ||
            source ==
                Source::RESTORE)
        {
            return true;
        }

        return
            entry.writable();
    }

    // ========================================================================
    // TEXT CAPACITY
    // ========================================================================

    static uint16_t capacityForType(
        Type type)
    {
        switch (type)
        {
            case Type::STRING32:
                return 32;

            case Type::STRING64:
                return 64;

            case Type::JSON128:
                return 128;

            case Type::JSON256:
                return 256;

            case Type::BOOL:
            case Type::INT32:
            case Type::FLOAT:
            default:
                return 0;
        }
    }

    // ========================================================================
    // TEXT RANGE VALIDATION
    // ========================================================================

    bool validTextRange(
        const Entry& entry) const
    {
        if (!entry.textBased())
            return false;

        const size_t offset =
            entry.textOffset;

        const size_t capacity =
            entry.textCapacity;

        if (capacity == 0)
            return false;

        if (offset >=
            TEXT_POOL_SIZE)
        {
            return false;
        }

        if (capacity >
            TEXT_POOL_SIZE - offset)
        {
            return false;
        }

        return true;
    }

    // ========================================================================
    // METADATA VALIDATION
    // ========================================================================

    static bool validNumberMetadata(
        Type type,
        const Metadata& metadata)
    {
        if (type != Type::INT32 &&
            type != Type::FLOAT)
        {
            return true;
        }

        if (!isfinite(
                metadata.minimum) ||
            !isfinite(
                metadata.maximum) ||
            !isfinite(
                metadata.step))
        {
            return false;
        }

        if (metadata.maximum <
            metadata.minimum)
        {
            return false;
        }

        if (metadata.step <=
            0.0f)
        {
            return false;
        }

        return true;
    }

    static bool validType(
        Type type)
    {
        switch (type)
        {
            case Type::BOOL:
            case Type::INT32:
            case Type::FLOAT:
            case Type::STRING32:
            case Type::STRING64:
            case Type::JSON128:
            case Type::JSON256:
                return true;
        }

        return false;
    }

    static bool validAccess(
        Access access)
    {
        switch (access)
        {
            case Access::READ_ONLY:
            case Access::WRITE_ONLY:
            case Access::READ_WRITE:
                return true;
        }

        return false;
    }

    // ========================================================================
    // ID VALIDATION
    // ========================================================================

    static bool validId(
        const char* id)
    {
        if (!id ||
            id[0] == '\0')
        {
            return false;
        }

        const size_t length =
            boundedLength(
                id,
                MAX_ID_LENGTH
            );

        if (length == 0 ||
            length >=
                MAX_ID_LENGTH)
        {
            return false;
        }

        for (size_t i = 0;
             i < length;
             ++i)
        {
            const char c =
                id[i];

            const bool valid =
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '_' ||
                c == '-' ||
                c == '.';

            if (!valid)
            {
                return false;
            }
        }

        return true;
    }

    // ========================================================================
    // OPTIONAL TEXT VALIDATION
    // ========================================================================

    static bool validOptionalText(
        const char* text,
        size_t maximumLength)
    {
        if (!text)
            return true;

        return
            boundedLength(
                text,
                maximumLength
            ) < maximumLength;
    }

    // ========================================================================
    // BOUNDED LENGTH
    // ========================================================================
    //
    // Alternativa locale a strnlen() per massima compatibilità Arduino.
    //
    // Se la stringa non termina entro maximumLength, restituisce
    // maximumLength.
    //
    // ========================================================================

    static size_t boundedLength(
        const char* text,
        size_t maximumLength)
    {
        if (!text)
            return 0;

        size_t length =
            0;

        while (length <
                maximumLength &&
               text[length] != '\0')
        {
            ++length;
        }

        return
            length;
    }

    // ========================================================================
    // TERMINATOR CHECK
    // ========================================================================

    static bool hasTerminator(
        const char* text,
        size_t capacity)
    {
        if (!text ||
            capacity == 0)
        {
            return false;
        }

        for (size_t i = 0;
             i < capacity;
             ++i)
        {
            if (text[i] == '\0')
                return true;
        }

        return false;
    }

    // ========================================================================
    // SAFE FALLBACK
    // ========================================================================

    static const char* safeFallback(
        const char* fallback)
    {
        return
            fallback
                ? fallback
                : "";
    }

    // ========================================================================
    // RESULT SETTER
    // ========================================================================

    static void setResult(
        DefineResult* destination,
        DefineResult value)
    {
        if (destination)
        {
            *destination =
                value;
        }
    }

    // ========================================================================
    // INTEGRITY LOG
    // ========================================================================

    static void logIntegrityError(
        bool enabled,
        const char* message)
    {
        if (!enabled)
            return;

        LOG_EF(
            "RUNTIME_STORE",
            "Integrity error: %s",
            message
                ? message
                : "unknown"
        );
    }
};

#endif