#ifndef DMLOGGER_HPP
#define DMLOGGER_HPP

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
#include <stdarg.h>

enum class LogLevel : uint8_t {
    ERROR = 0,
    WARN  = 1,
    INFO  = 2,
    DEBUG = 3
};

// Livello massimo compilato
#ifndef LOG_LEVEL
#define LOG_LEVEL LogLevel::INFO
#endif

// Abilita colori ANSI su terminali che li supportano
#ifndef LOG_USE_ANSI
#define LOG_USE_ANSI 0
#endif

// ============================================================
//  LOG INFO HIGH VISIBILITY (ANSI COLOR + BOX)
// ============================================================

#if LOG_USE_ANSI
#define ANSI_RESET   "\033[0m"
#define ANSI_CYAN    "\033[96m"
#define ANSI_BLUE    "\033[94m"
#define ANSI_BOLD    "\033[1m"
#else
#define ANSI_RESET   ""
#define ANSI_CYAN    ""
#define ANSI_BLUE    ""
#define ANSI_BOLD    ""
#endif

class LogBuffer {
public:
    static const uint8_t MAX_EVENTS = 10;

    struct Entry {
        LogLevel level;
        const char* className;
        const char* functionName;
        char message[80];
        unsigned long lastTimestamp;
        uint16_t occorrenze;

        Entry() :
            level(LogLevel::INFO),
            className(""),
            functionName(""),
            lastTimestamp(0),
            occorrenze(0)
        {
            message[0] = '\0';
        }

        void setMessage(const char* msg) {
            strncpy(message, msg, sizeof(message));
            message[sizeof(message)-1] = '\0';
        }
    };


private:
    Entry events[MAX_EVENTS];
    uint8_t count = 0;

public:

    // ============================================================
    // AGGIUNTA EVENTO
    // ============================================================
    void add(LogLevel level,
             const char* cls,
             const char* func,
             const char* msg)
    {
        unsigned long now = millis();

        // 1. Cerca evento identico
        for (uint8_t i = 0; i < count; i++) {
            Entry& e = events[i];

            if (e.level == level &&
                strcmp(e.className, cls) == 0 &&
                strcmp(e.functionName, func) == 0 &&
                e.message == msg)
            {
                e.occorrenze++;
                e.lastTimestamp = now;
                return;
            }
        }

        // 2. Se c’è spazio → aggiungi
        if (count < MAX_EVENTS) {
            Entry& e = events[count++];
            e.level = level;
            e.className = cls;
            e.functionName = func;
            e.setMessage(msg);
            e.lastTimestamp = now;
            e.occorrenze = 1;
            return;
        }

        // 3. Buffer pieno → cerca occorrenze==1
        int idxReplace = -1;
        for (uint8_t i = 0; i < MAX_EVENTS; i++) {
            if (events[i].occorrenze == 1) {
                idxReplace = i;
                break;
            }
        }

        if (idxReplace >= 0) {
            Entry& e = events[idxReplace];
            e.level = level;
            e.className = cls;
            e.functionName = func;
            e.setMessage(msg);
            e.lastTimestamp = now;
            e.occorrenze = 1;
        }
        // altrimenti scarta
    }

    // ============================================================
    // RESET
    // ============================================================
    void reset() {
        count = 0;
    }

    // ============================================================
    // SIZE
    // ============================================================
    uint8_t size() const {
        return count;
    }

    // ============================================================
    // REPORT ORDINATO PER OCCORRENZE (DESC)
    // ============================================================
    void report(Stream& out) {
        if (count == 0) {
            out.println("===== LOG BUFFER REPORT =====");
            out.println("No log events recorded.");
            out.println("=============================");
            return;
        }

        // copia locale
        Entry tmp[MAX_EVENTS];
        for (uint8_t i = 0; i < count; i++)
            tmp[i] = events[i];

        // sort desc per occorrenze
        for (uint8_t i = 0; i < count; i++) {
            for (uint8_t j = i + 1; j < count; j++) {
                if (tmp[j].occorrenze > tmp[i].occorrenze) {
                    Entry t = tmp[i];
                    tmp[i] = tmp[j];
                    tmp[j] = t;
                }
            }
        }

        out.println("===== LOG BUFFER REPORT =====");

        for (uint8_t i = 0; i < count; i++) {
            out.println("------------------------------");

            out.print("#");
            out.print(i);
            out.print(" [");
            out.print(tmp[i].occorrenze);
            out.println(" occurrences]");

            out.print("Level: ");
            switch (tmp[i].level) {
                case LogLevel::ERROR: out.println("ERROR"); break;
                case LogLevel::WARN:  out.println("WARN"); break;
                case LogLevel::INFO:  out.println("INFO"); break;
                case LogLevel::DEBUG: out.println("DEBUG"); break;
            }

            out.print("Class: ");
            out.println(tmp[i].className);

            out.print("Function: ");
            out.println(tmp[i].functionName);

            out.print("Message: ");
            out.println(tmp[i].message);

            out.print("Last timestamp: ");
            out.print(tmp[i].lastTimestamp);
            out.println(" ms");
        }

        out.println("=============================");
    }

};

class LogManager {
private:
    inline static bool enabled = true;

public:
    static inline void enable()  { enabled = true; }
    static inline void disable() { enabled = false; }
    static inline bool isEnabled() { return enabled; }
};

class Logger {
private:
    static constexpr int MAX_LOG_LINE = 80;

    static inline LogBuffer LogHistory;

    static void logInternal(LogLevel level,
                        const char *className,
                        const char *functionName,
                        const char *msg)
    {
        unsigned long now = millis();

        // Header comune
        char header[96];
        snprintf(header, sizeof(header),
                "[%lu ms][%s][%s::%s] ",
                now,
                (level == LogLevel::ERROR ? "❌ ERROR" :
                level == LogLevel::WARN  ? "⚠️ WARN"  :
                level == LogLevel::INFO  ? "ℹ️ INFO"  :
                                            "🐞 DEBUG"),
                className,
                functionName);

        const size_t headerLen = strlen(header);

        // Buffer per la riga completa (header + chunk)
        char line[headerLen + MAX_LOG_LINE + 2];

        // Scorri il messaggio e spezza ogni MAX_LOG_LINE caratteri
        const char* p = msg;

        while (*p) {
            size_t chunkLen = 0;

            // Calcola quanti caratteri stampare in questa riga
            while (chunkLen < MAX_LOG_LINE && p[chunkLen] != '\0') {
                chunkLen++;
            }

            // Copia header
            memcpy(line, header, headerLen);

            // Copia chunk
            memcpy(line + headerLen, p, chunkLen);

            // Terminatore
            line[headerLen + chunkLen] = '\0';

            // Stampa la riga
            Serial.println(line);

            // Avanza
            p += chunkLen;
        }
    }


public:
    static void ReportLogBuffer(Stream& out) {
        LogHistory.report(out);
    }

    static void log(LogLevel level,
                const char *className,
                const char *functionName,
                const char *msg)
    {
        if ((uint8_t)level > (uint8_t)LOG_LEVEL)
            return;

        // stampa sempre
        logInternal(level, className, functionName, msg);

        // registra SOLO errori e warning
        if (level == LogLevel::ERROR || level == LogLevel::WARN)
            LogHistory.add(level, className, functionName, msg);
    }

    static void logNoBuf(LogLevel level,
                      const char *className,
                      const char *functionName,
                      const char *msg)
    {
        if ((uint8_t)level > (uint8_t)LOG_LEVEL)
            return;

        // stampa soltanto
        logInternal(level, className, functionName, msg);
    }


    static void logf(LogLevel level,
                     const char *className,
                     const char *functionName,
                     const char *fmt,
                     ...)
    {
        if ((uint8_t)level > (uint8_t)LOG_LEVEL)
            return;

        char buffer[160];

        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        log(level, className, functionName, buffer);
    }

    static void logfNoBuf(LogLevel level,
                       const char *className,
                       const char *functionName,
                       const char *fmt,
                       ...)
    {
        if ((uint8_t)level > (uint8_t)LOG_LEVEL)
            return;

        char buffer[160];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        logNoBuf(level, className, functionName, buffer);
    }

};

// Macro che catturano automaticamente nome classe e funzione
#define LOG_E(cls, msg) do { if (LogManager::isEnabled()) Logger::log(LogLevel::ERROR, cls, __FUNCTION__, msg); } while(0)
#define LOG_W(cls, msg) do { if (LogManager::isEnabled()) Logger::log(LogLevel::WARN,  cls, __FUNCTION__, msg); } while(0)
#define LOG_I(cls, msg) do { if (LogManager::isEnabled()) Logger::log(LogLevel::INFO,  cls, __FUNCTION__, msg); } while(0)
#define LOG_D(cls, msg) do { if (LogManager::isEnabled()) Logger::log(LogLevel::DEBUG, cls, __FUNCTION__, msg); } while(0)

#define LOG_EF(cls, fmt, ...) do { if (LogManager::isEnabled()) Logger::logf(LogLevel::ERROR, cls, __FUNCTION__, fmt, ##__VA_ARGS__); } while(0)
#define LOG_WF(cls, fmt, ...) do { if (LogManager::isEnabled()) Logger::logf(LogLevel::WARN,  cls, __FUNCTION__, fmt, ##__VA_ARGS__); } while(0)
#define LOG_IF(cls, fmt, ...) do { if (LogManager::isEnabled()) Logger::logf(LogLevel::INFO,  cls, __FUNCTION__, fmt, ##__VA_ARGS__); } while(0)
#define LOG_DF(cls, fmt, ...) do { if (LogManager::isEnabled()) Logger::logf(LogLevel::DEBUG, cls, __FUNCTION__, fmt, ##__VA_ARGS__); } while(0)

#define LOG_EN(cls, msg)  Logger::logNoBuf(LogLevel::ERROR, cls, __FUNCTION__, msg)
#define LOG_WN(cls, msg)  Logger::logNoBuf(LogLevel::WARN,  cls, __FUNCTION__, msg)
#define LOG_IN(cls, msg)  Logger::logNoBuf(LogLevel::INFO,  cls, __FUNCTION__, msg)
#define LOG_DN(cls, msg)  Logger::logNoBuf(LogLevel::DEBUG, cls, __FUNCTION__, msg)

#define LOG_EFN(cls, fmt, ...) Logger::logfNoBuf(LogLevel::ERROR, cls, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_WFN(cls, fmt, ...) Logger::logfNoBuf(LogLevel::WARN,  cls, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_IFN(cls, fmt, ...) Logger::logfNoBuf(LogLevel::INFO,  cls, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_DFN(cls, fmt, ...) Logger::logfNoBuf(LogLevel::DEBUG, cls, __FUNCTION__, fmt, ##__VA_ARGS__)

static inline void Logger_InfoBox(const char* cls, const char* func, const char* msg)
{
    unsigned long now = millis();

    // Formato compatto e coerente con gli altri log
    Serial.println("___________________________________________________________");
    Serial.print("[");
    Serial.print(now);
    Serial.print(" ms][ℹ️ INFO][");
    Serial.print(cls);
    Serial.print("::");
    Serial.print(func);
    Serial.print("] ");
#if LOG_USE_ANSI
    Serial.print(ANSI_CYAN);
    Serial.print("▶ ");
    Serial.print(ANSI_RESET);
#else
    Serial.print("▶ ");
#endif

    Serial.println(msg);
    Serial.println("___________________________________________________________");
}


// Messaggio semplice
#define LOG_IH(cls, msg) \
    do { if (LogManager::isEnabled()) Logger_InfoBox(cls, __FUNCTION__, msg); } while(0)

// Messaggio formattato
#define LOG_IFH(cls, fmt, ...) \
    do { \
        if (LogManager::isEnabled()) { \
            char __buf[256]; \
            snprintf(__buf, sizeof(__buf), fmt, ##__VA_ARGS__); \
            Logger_InfoBox(cls, __FUNCTION__, __buf); \
        } \
    } while(0)

#endif
