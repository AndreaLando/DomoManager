#ifndef DMOPTARTC_HPP
#define DMOPTARTC_HPP

/* ============================================================================
   SVILUPPATORE
   ============================================================================

   Nome:            Andrea Lando
   Contatto:        mail@domo-manager.it
  
   Versione modulo: 1.0.0
   Ultima modifica: 2026‑03‑24
   Note:
                    Prima versione

   ============================================================================ */


#include <Arduino.h>
#include <time.h>
#include <vector>

#include <NTPClient.h>
#include <WiFi.h>          

#include <Ethernet.h>
#include <EthernetUdp.h>


#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"


class OptaRTC {
private:
    inline static constexpr std::array<const char*, 3> NTP_SERVERS = {
        "pool.ntp.org",
        "time.google.com",
        "time.windows.com"
    };

    enum RTCState { WAIT_WIFI, TRY_NTP, RTC_READY };
    RTCState rtcState = WAIT_WIFI;

    unsigned long lastAttempt = 0;
        
    // --- AUTO RESYNC ---
    unsigned long autoResyncIntervalMs = 0;   // 0 = disabilitato
    unsigned long lastAutoResyncMs = 0;

    int ntpFailCount = 0;                     // fallimenti consecutivi
    static const int MAX_FAILS = 5;           // dopo 5 fallimenti → unsynced

    void forceUnsynced() {
        if (mode == Mode::Hardware) {
            ImplHardwareRTC* hw = static_cast<ImplHardwareRTC*>(impl);
            hw->forceUnsynced();
        } else {
            ImplSoftwareRTC* sw = static_cast<ImplSoftwareRTC*>(impl);
            sw->forceUnsynced();
        }
    }

    time_t ntpClientSingle(const String& server, int tz) {
        static const int DM_NTP_PACKET_SIZE = 48;
        byte packet[DM_NTP_PACKET_SIZE];

        IPAddress ntpIP;

        if (impl->isWifiMode()) {
            if (WiFi.hostByName(server.c_str(), ntpIP) != 1)
                return 0;

            WiFiUDP udp;
            udp.begin(2390);

            memset(packet, 0, DM_NTP_PACKET_SIZE);
            packet[0] = 0b11100011;

            udp.beginPacket(ntpIP, 123);
            udp.write(packet, DM_NTP_PACKET_SIZE);
            udp.endPacket();

            unsigned long start = millis();
            while (millis() - start < 200) {   // 200ms → NON BLOCCANTE
                int len = udp.parsePacket();
                if (len >= DM_NTP_PACKET_SIZE) {
                    udp.read(packet, DM_NTP_PACKET_SIZE);

                    unsigned long high = word(packet[40], packet[41]);
                    unsigned long low  = word(packet[42], packet[43]);
                    unsigned long secs = (high << 16) | low;

                    const unsigned long seventyYears = 2208988800UL;
                    return secs - seventyYears + tz * 3600;
                }
            }
        }
        else {
            if (!Ethernet.hostByName(server.c_str(), ntpIP))
                return 0;

            EthernetUDP udp;
            udp.begin(2390);

            memset(packet, 0, DM_NTP_PACKET_SIZE);
            packet[0] = 0b11100011;

            udp.beginPacket(ntpIP, 123);
            udp.write(packet, DM_NTP_PACKET_SIZE);
            udp.endPacket();

            unsigned long start = millis();
            while (millis() - start < 200) {   // 200ms → NON BLOCCANTE
                int len = udp.parsePacket();
                if (len >= DM_NTP_PACKET_SIZE) {
                    udp.read(packet, DM_NTP_PACKET_SIZE);

                    unsigned long high = word(packet[40], packet[41]);
                    unsigned long low  = word(packet[42], packet[43]);
                    unsigned long secs = (high << 16) | low;

                    const unsigned long seventyYears = 2208988800UL;
                    return secs - seventyYears + tz * 3600;
                }
            }
        }

        return 0;
    }

public:
    enum class Mode {
        Hardware,
        Software
    };

    OptaRTC(Mode mode = Mode::Hardware)
        : mode(mode)
    {
        if (mode == Mode::Hardware) {
            impl = new ImplHardwareRTC();
        } else {
            impl = new ImplSoftwareRTC();
        }
    }

    ~OptaRTC() {
        delete impl;
    }

    // ============================================================
    //  API PUBBLICA (identica per HW e SW)
    // ============================================================
    void setSlaveMode(bool v) { impl->setSlaveMode(v); }
    bool isSlaveMode() const { return impl->isSlaveMode(); }

    bool beginWifi()  { return impl->beginWifi(); }
    bool beginEth()   { return impl->beginEth(); }

    bool syncNTP(const std::vector<String>& servers, unsigned long nowMs, int tz) {
        static size_t index = 0;
        
        if (servers.empty())
            return false;

        const String& server = servers[index];

        LOG_IF("NTP", "Tentativo NTP su %s (index=%u)", server.c_str(), index);

        time_t epoch = ntpClientSingle(server, tz);

        // Avanza al prossimo server per il ciclo successivo
        index = (index + 1) % servers.size();

        if (epoch == 0) {
            LOG_WF("NTP", "Fallito su %s", server.c_str());
            return false;
        }

        LOG_IF("NTP", "Epoch ricevuto: %ld", epoch);
        impl->applyEpoch(epoch);
        return true;
    }


    time_t getEpoch() const { return impl->getEpoch(); }
    void applyEpoch(time_t epoch) {
        impl->applyEpoch(epoch);
    }

    bool isTimeSynced() const { return impl->isTimeSynced(); }

    void update(unsigned long nowMs) { impl->update(nowMs); }

    // Callback identiche
    void onEverySecond(std::function<void()> cb) { impl->onEverySecond(cb); }
    void onEveryMinute(std::function<void()> cb) { impl->onEveryMinute(cb); }
    void onEveryHour  (std::function<void()> cb) { impl->onEveryHour(cb); }
    void onEveryDay   (std::function<void()> cb) { impl->onEveryDay(cb); }
    void onEveryWeek  (std::function<void()> cb) { impl->onEveryWeek(cb); }
    void onEveryMonth (std::function<void()> cb) { impl->onEveryMonth(cb); }

    void loop(unsigned long now) {
        static RTCState lastLoggedState = (RTCState)-1;

        if (rtcState != lastLoggedState) {
            //LOG_IF("RTC::loop", "STATE → %d", rtcState);
            lastLoggedState = rtcState;
        }

        switch (rtcState) {
            case WAIT_WIFI:
                if (impl->isWifiMode()) {
                    // Modalità WiFi → aspetta connessione WiFi
                    if (WiFi.status() == WL_CONNECTED) {
                        LOG_IF("RTC", "WiFi connesso → passo a TRY_NTP");
                        rtcState = TRY_NTP;
                        lastAttempt = now;
                    }
                } else {
                    // Modalità Ethernet → passa SUBITO a TRY_NTP
                    LOG_IF("RTC", "Ethernet attiva → passo a TRY_NTP");
                    rtcState = TRY_NTP;
                    lastAttempt = now;
                }
                break;


            case TRY_NTP:
                if (now - lastAttempt > 60000) {
                    lastAttempt = now;
                    
                    LOG_IF("RTC", "Tentativo syncNTP() now=%d", now);

                    bool ok = syncNTP(
                        std::vector<String>(std::begin(NTP_SERVERS), std::end(NTP_SERVERS)),
                        now, 0);
                    if (ok) {
                        LOG_IF("RTC", "RTC sincronizzato! epoch=%d now=%d", impl->getEpoch(), now);
                        rtcState = RTC_READY;
                    } else {
                        LOG_WF("RTC", "Sync NTP fallito, ritento...");
                    }
                }
                break;

            case RTC_READY:
                // ============================================================
                // AUTO RESYNC NTP
                // ============================================================
                if (autoResyncIntervalMs > 0) {
                    if (now - lastAutoResyncMs >= autoResyncIntervalMs) {
                        lastAutoResyncMs = now;

                        LOG_IF("RTC", "[AUTO-RESYNC] Tentativo NTP automatico...");

                        bool ok = syncNTP(
                            std::vector<String>(std::begin(NTP_SERVERS), std::end(NTP_SERVERS)),
                            now, 0);

                        if (ok) {
                            LOG_IF("RTC", "[AUTO-RESYNC] OK → contatore errori azzerato");
                            ntpFailCount = 0;
                        } else {
                            ntpFailCount++;
                            LOG_WF("RTC", "[AUTO-RESYNC] Fallito (%d/%d)", ntpFailCount, MAX_FAILS);

                            if (ntpFailCount >= MAX_FAILS) {
                                LOG_WF("RTC", "[AUTO-RESYNC] TROPPI FALLIMENTI → forzo unsynced");
                                forceUnsynced();
                            }
                        }
                    }
                }

                impl->update(now);
                break;
        }

        /*
        // Debug periodico
        static unsigned long lastRtcDebug = 0;
        if (now - lastRtcDebug > 10000) {
            lastRtcDebug = now;

            bool synced = impl->isTimeSynced();
            time_t e = impl->getEpoch();

            LOG_IF("RTC", "epoch=%ld synced=%s",
                (long)e,
                synced ? "true" : "false");
        } */
    }
    
    time_t ntpClient(const std::vector<String>& servers, int tz) {
        static const int DM_NTP_PACKET_SIZE = 48;
        byte packet[DM_NTP_PACKET_SIZE];

        for (auto &server : servers) {
            IPAddress ntpIP;

            if (impl->isWifiMode()) {
                LOG_IF("NTP", "NTP via WiFi → %s", server.c_str());

                if (WiFi.hostByName(server.c_str(), ntpIP) != 1) {
                    LOG_WF("NTP", "DNS WiFi fallito");
                    continue;
                }

                WiFiUDP udp;
                udp.begin(2390);

                memset(packet, 0, DM_NTP_PACKET_SIZE);
                packet[0] = 0b11100011;

                udp.beginPacket(ntpIP, 123);
                udp.write(packet, DM_NTP_PACKET_SIZE);
                udp.endPacket();

                unsigned long start = millis();
                while (millis() - start < 2000) {
                    int len = udp.parsePacket();
                    if (len >= DM_NTP_PACKET_SIZE) {
                        udp.read(packet, DM_NTP_PACKET_SIZE);

                        unsigned long high = word(packet[40], packet[41]);
                        unsigned long low  = word(packet[42], packet[43]);
                        unsigned long secs = (high << 16) | low;

                        const unsigned long seventyYears = 2208988800UL;
                        return secs - seventyYears + tz * 3600;
                    }
                }
            }
            else {
                LOG_IF("NTP", "NTP via Ethernet → %s", server.c_str());

                if (!Ethernet.hostByName(server.c_str(), ntpIP)) {
                    LOG_WF("NTP", "DNS Ethernet fallito");
                    continue;
                }

                EthernetUDP udp;
                udp.begin(2390);

                memset(packet, 0, DM_NTP_PACKET_SIZE);
                packet[0] = 0b11100011;

                udp.beginPacket(ntpIP, 123);
                udp.write(packet, DM_NTP_PACKET_SIZE);
                udp.endPacket();

                unsigned long start = millis();
                while (millis() - start < 2000) {
                    int len = udp.parsePacket();
                    if (len >= DM_NTP_PACKET_SIZE) {
                        udp.read(packet, DM_NTP_PACKET_SIZE);

                        unsigned long high = word(packet[40], packet[41]);
                        unsigned long low  = word(packet[42], packet[43]);
                        unsigned long secs = (high << 16) | low;

                        const unsigned long seventyYears = 2208988800UL;
                        return secs - seventyYears + tz * 3600;
                    }
                }
            }
        }

        LOG_WF("NTP", "Nessuna risposta da nessun server");
        return 0;
    }

    void logMode() {
        if (mode == Mode::Hardware)
            LOG_IF("OptaRTC", "MODE = HARDWARE RTC");
        else
            LOG_IF("OptaRTC", "MODE = SOFTWARE RTC");
    }

    void setAutoResyncHours(int hours) {
        if (hours <= 0) {
            autoResyncIntervalMs = 0;
            LOG_IF("OptaRTC", "Auto-resync DISABILITATO");
            return;
        }

        autoResyncIntervalMs = (unsigned long)hours * 3600UL * 1000UL;
        lastAutoResyncMs = millis();
        LOG_IF("OptaRTC", "Auto-resync ogni %d ore (%lu ms)", hours, autoResyncIntervalMs);
    }

    String getDateTimeString() {
        time_t t = impl->getEpoch();
        if (t == 0) return "Invalid";

        struct tm tm;
        gmtime_r(&t, &tm);

        char buffer[25];
        snprintf(buffer, sizeof(buffer),
                "%04d-%02d-%02d %02d:%02d:%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);

        return String(buffer);
    }

    class CallbackEngine {
    public:
        void update(time_t epoch) {
            if (epoch == 0) return;

            struct tm t;
            gmtime_r(&epoch, &t);

            if (t.tm_sec != lastSecond) {
                lastSecond = t.tm_sec;
                if (secondCb) secondCb();
            }
            if (t.tm_min != lastMinute) {
                lastMinute = t.tm_min;
                if (minuteCb) minuteCb();
            }
            if (t.tm_hour != lastHour) {
                lastHour = t.tm_hour;
                if (hourCb) hourCb();
            }
            if (t.tm_mday != lastDay) {
                lastDay = t.tm_mday;
                if (dayCb) dayCb();
            }
            if (t.tm_wday != lastWeek) {
                lastWeek = t.tm_wday;
                if (weekCb) weekCb();
            }
            int month = t.tm_mon + 1;
            if (month != lastMonth) {
                lastMonth = month;
                if (monthCb) monthCb();
            }
        }

        void onEverySecond(std::function<void()> cb) { secondCb = cb; }
        void onEveryMinute(std::function<void()> cb) { minuteCb = cb; }
        void onEveryHour  (std::function<void()> cb) { hourCb   = cb; }
        void onEveryDay   (std::function<void()> cb) { dayCb    = cb; }
        void onEveryWeek  (std::function<void()> cb) { weekCb   = cb; }
        void onEveryMonth (std::function<void()> cb) { monthCb  = cb; }

    private:
        int lastSecond = -1;
        int lastMinute = -1;
        int lastHour   = -1;
        int lastDay    = -1;
        int lastWeek   = -1;
        int lastMonth  = -1;

        std::function<void()> secondCb;
        std::function<void()> minuteCb;
        std::function<void()> hourCb;
        std::function<void()> dayCb;
        std::function<void()> weekCb;
        std::function<void()> monthCb;
    };

private:

    // ============================================================
    //  INTERFACCIA INTERNA
    // ============================================================
    class ImplBase {
    protected:
        bool isSlave = false;

    public:
        void setSlaveMode(bool v) { isSlave = v; }
        bool isSlaveMode() const { return isSlave; }

        virtual ~ImplBase() {}

        virtual time_t getEpoch() const = 0;
        virtual bool isTimeSynced() const = 0;

        virtual void update(unsigned long nowMs) = 0;

        virtual void applyEpoch(time_t epoch) = 0;

        virtual void onEverySecond(std::function<void()> cb) { callbacks.onEverySecond(cb); }
        virtual void onEveryMinute(std::function<void()> cb) { callbacks.onEveryMinute(cb); }
        virtual void onEveryHour  (std::function<void()> cb) { callbacks.onEveryHour(cb); }
        virtual void onEveryDay   (std::function<void()> cb) { callbacks.onEveryDay(cb); }
        virtual void onEveryWeek  (std::function<void()> cb) { callbacks.onEveryWeek(cb); }
        virtual void onEveryMonth (std::function<void()> cb) { callbacks.onEveryMonth(cb); }

        virtual bool beginWifi() { useWiFi = true; return true; }
        virtual bool beginEth()  { useWiFi = false; return true; }


        bool isWifiMode() const { return useWiFi; } 
    protected:
        bool useWiFi = false;

        bool isValidEpoch(time_t epoch) const {
            return epoch >= 1600000000 && epoch <= 2500000000;
        }

        CallbackEngine callbacks;
    };

    // ============================================================
    //  IMPLEMENTAZIONE HARDWARE (il tuo RTC reale)
    // ============================================================
    class ImplHardwareRTC : public ImplBase {
    public:
        ImplHardwareRTC() {}

        void applyEpoch(time_t epoch) override {
            if (!isValidEpoch(epoch)) {
                LOG_WF("HW", "applyEpoch rifiutato: epoch non valido (%ld)", epoch);
                return;
            }

            // Imposta l’RTC hardware
            forceSetTime(epoch);
            lastEpoch = epoch;

            // ⭐ MASTER: epoch da SLAVE → valido subito
            if (!isSlaveMode()) {
                rtcWarmup = false;
                warmupCount = 3;
                timeSynced = true;
                return;
            }

            // ⭐ Epoch ricevuto da NTP o dal bridge → valido SUBITO
            rtcWarmup = false;
            warmupCount = 3;
            timeSynced = true;
        }

        time_t getEpoch() const override { return lastEpoch; }
        bool isTimeSynced() const override { return timeSynced && !rtcWarmup; }

        void update(unsigned long nowMs) override {
            static unsigned long lastUpdateMs = 0;
            if (nowMs - lastUpdateMs < 200) return;
            lastUpdateMs = nowMs;

            time_t hw = time(nullptr);

            if (rtcWarmup) {
                // ⭐ SLAVE NON deve auto-sincronizzarsi
                if (isSlaveMode()) {
                    return;
                }

                warmupCount++;
                if (warmupCount >= 3) {
                    rtcWarmup = false;
                    lastEpoch = hw;
                    timeSynced = true;
                }
                return;
            }

            if (hw == 0) {
                LOG_WF("OptaRTC", "[HW] hw==0 DOPO WARMUP → IGNORO");
                return;
            }

            if (hw != lastEpoch) {
                lastEpoch = hw;
                callbacks.update(hw);
            }
        }

        void forceUnsynced() {
            timeSynced = false;
            rtcWarmup = true;
            warmupCount = 0;
            LOG_WF("HW", "RTC marcato come NON sincronizzato");
        }

    private:
        // --- (qui rimane tutto il tuo codice hardware RTC, identico) ---
        // Per brevità non lo duplico tutto qui, ma è lo stesso che hai già
        // con la protezione anti-reset e warmup.

        // ============================================================
        //  FORCE SET TIME (RTC hardware)
        // ============================================================
        bool forceSetTime(time_t epoch) {
            for (int i = 0; i < 20; i++) {
                set_time(epoch);
                delay(100);

                if (time(nullptr) != 0) {
                    LOG_IF("OptaRTC", "[RTC] set_time OK dopo %d tentativi", i+1);
                    return true;
                }

                LOG_WF("OptaRTC", "[RTC] RTC non pronto, retry...");
            }

            LOG_WF("OptaRTC", "[RTC] ERRORE: impossibile impostare l'RTC hardware");
            return false;
        }

        // ============================================================
        //  NTP (WiFi/Ethernet)
        // ============================================================
        bool timeSynced = false;
        bool rtcWarmup = false;
        int warmupCount = 0;

        time_t lastEpoch = 0;
    };

    // ============================================================
    //  IMPLEMENTAZIONE SOFTWARE (RTC simulato)
    // ============================================================
    class ImplSoftwareRTC : public ImplBase {
    public:
        ImplSoftwareRTC() {}

        void applyEpoch(time_t epoch) override {
            if (!isValidEpoch(epoch)) {
                LOG_WF("SW", "applyEpoch rifiutato: epoch non valido (%ld)", epoch);
                return;
            }

            //LOG_IF("SoftwareRTC", "applyEpoch SW epoch=%ld", epoch);

            // Imposta l’epoch come base
            baseEpoch = epoch;

            // Usiamo millis() solo per calcolare il delta, NON per validare
            baseMs = millis();   // anche se è 0, non importa più
        }

        time_t getEpoch() const override {
            if (baseEpoch == 0) return 0;

            unsigned long now = millis();
            unsigned long delta = (now >= baseMs) ? (now - baseMs) : 0;
            return baseEpoch + delta / 1000;
        }

        bool isTimeSynced() const override {
            LOG_IF("SW", "isTimeSynced() baseEpoch=%ld", baseEpoch);
            return baseEpoch != 0;
        }

        void update(unsigned long nowMs) override {
            time_t t = getEpoch();
            if (t == 0) return;   // NON resettare synced
            if (t == lastEpoch) return;

            lastEpoch = t;
            callbacks.update(t);
        }

        void forceUnsynced() {
            baseEpoch = 0;
            LOG_WF("SW", "RTC software marcato come NON sincronizzato");
        }

    private:
        time_t baseEpoch = 0;
        unsigned long baseMs = 0;
        
        time_t lastEpoch = 0;
    };

private:
    Mode mode;
    ImplBase* impl;
};

#endif
