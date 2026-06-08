#ifndef DMOPTA_RTC_HPP
#define DMOPTA_RTC_HPP

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
#include <functional>

#include <Ethernet.h>
#include <EthernetUdp.h>
#include <WiFi.h>

#define LOG_LEVEL LogLevel::INFO
#include "DMLogger.hpp"

class OptaRTC {
    public:
        const int UDP_PORT=2390;
        const int NTP_PORT=123;

        struct CronEntry {
            String expression;
            std::function<void()> callback;
            int lastMinuteExecuted = -1;
        };
        OptaRTC() {}

        bool beginWifi() {
            useWiFi = true;
            return true;
        }

        bool beginEth() {
            useWiFi = false;
            return true;
        }

        // ---------------- RTC BASE ----------------

        bool setEpoch(uint32_t epoch) {
            struct tm t;
            gmtime_r((time_t*)&epoch, &t);
            return syncRTC(t);
        }

        bool setDateTime(int year, int month, int day, int hour, int minute, int second) {
            struct tm t = {0};
            t.tm_year = year - 1900;
            t.tm_mon = month - 1;
            t.tm_mday = day;
            t.tm_hour = hour;
            t.tm_min = minute;
            t.tm_sec = second;
            return syncRTC(t);
        }

        bool syncRTC(const struct tm &t) {
            time_t newTime = mktime((struct tm*)&t);
            if (newTime < 0) return false;
            set_time(newTime);
            timeSynced = true;   

            //Diagnostica
            everSynced = true;
            lastSyncMs = millis();
            lastValidTime = t;

            return true;
        }

        time_t getEpoch() {
            return time(nullptr);
        }

        bool getDateTime(struct tm &timeinfo) {
            time_t now = time(nullptr);
            if (now == 0) return false;

            // 🔥 Conversione sicura su Opta (mbed)
            gmtime_r(&now, &timeinfo);

            lastValidTime = timeinfo;
            return true;
        }

        String getDateString() {
            struct tm t;
            if (!getDateTime(t)) return "Invalid";

            char buffer[20];
            snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
            return String(buffer);
        }

        String getTimeString() {
            struct tm t;
            if (!getDateTime(t)) return "Invalid";

            char buffer[20];
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
                    t.tm_hour, t.tm_min, t.tm_sec);
            return String(buffer);
        }

        String getDateTimeString() {
            struct tm t;
            if (!getDateTime(t)) return "Invalid";

            char buffer[25];
            snprintf(buffer, sizeof(buffer),
                    "%04d-%02d-%02d %02d:%02d:%02d",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec);

            return String(buffer);
        }

        //Getter di Diagnostica
        unsigned long getLastSyncMs() const { return lastSyncMs; }
        bool hasEverSynced() const { return everSynced; }
        tm getLastValidTime() const { return lastValidTime; }
        size_t getCronCount() const {
            return cronList.size();
        }
        const std::vector<CronEntry>& getCronEntries() const {
            return cronList;
        }

        // ---------------- NTP VIA UDP (COMPATIBILE OPTA) ----------------

        bool updateFromUDP(const String& value) {
            uint32_t epoch = value.toInt();
            if (epoch == 0) {
                LOG_WF("OptaRTC", "[UDP TIME] Valore non valido: %s", value.c_str());
                return false;
            }

            LOG_IF("OptaRTC", "[UDP TIME] Aggiornamento epoch: %lu", epoch);

            bool ok = setEpoch(epoch);
            if (ok) timeSynced = true;  

            //Diagnostica
            everSynced = true;
            lastSyncMs = millis();
            getDateTime(lastValidTime);

            return ok;
        }

        bool ntpSync(const char* server, int timezone = 0) {
            IPAddress ntpIP;

            // --- Modalità WiFi ---
            if (useWiFi) {
                LOG_DF("OptaRTC", "[NTP] Uso WiFi");

                if (WiFi.hostByName(server, ntpIP) != 1) {
                    LOG_WF("OptaRTC", "[NTP] Errore DNS (WiFi)");
                    return false;
                }

                WiFiUDP udp;
                udp.begin(UDP_PORT);

                byte packet[48] = {0};
                packet[0] = 0b11100011;

                udp.beginPacket(ntpIP, NTP_PORT);
                udp.write(packet, 48);
                udp.endPacket();

                unsigned long start = millis();
                while (millis() - start < 2000) {
                    if (udp.parsePacket()) {
                        udp.read(packet, 48);

                        unsigned long high = word(packet[40], packet[41]);
                        unsigned long low  = word(packet[42], packet[43]);
                        unsigned long secs = (high << 16) | low;

                        const unsigned long seventyYears = 2208988800UL;
                        time_t epoch = secs - seventyYears + timezone * 3600;

                        set_time(epoch);
                        timeSynced = true;
                        return true;
                    }
                }

                LOG_WF("OptaRTC", "[NTP] Timeout (WiFi)");
                return false;
            }

            // --- Modalità Ethernet ---
            LOG_DF("OptaRTC", "[NTP] Uso Ethernet");

            if (!Ethernet.hostByName(server, ntpIP)) {
                LOG_WF("OptaRTC", "[NTP] Errore DNS (Ethernet)");
                return false;
            }

            EthernetUDP udp;
            udp.begin(UDP_PORT);

            byte packet[48] = {0};
            packet[0] = 0b11100011;

            udp.beginPacket(ntpIP, 123);
            udp.write(packet, 48);
            udp.endPacket();

            unsigned long start = millis();
            while (millis() - start < 2000) {
                if (udp.parsePacket()) {
                    udp.read(packet, 48);

                    unsigned long high = word(packet[40], packet[41]);
                    unsigned long low  = word(packet[42], packet[43]);
                    unsigned long secs = (high << 16) | low;

                    const unsigned long seventyYears = 2208988800UL;
                    time_t epoch = secs - seventyYears + timezone * 3600;

                    set_time(epoch);
                    timeSynced = true;

                    //Diagnostica
                    everSynced = true;
                    lastSyncMs = millis();
                    getDateTime(lastValidTime);

                    return true;
                }
            }

            LOG_WF("OptaRTC", "[NTP] Timeout (Ethernet)");
            return false;
        }

        bool syncNTP(const std::vector<String>& servers, int timezone = 0, int timeoutMs = 5000) {
            for (auto &s : servers) {
                LOG_DF("OptaRTC", "[NTP] Tentativo con server: %s", s.c_str());

                if (ntpSync(s.c_str(), timezone)) {
                    LOG_DF("OptaRTC", "[NTP] Sincronizzazione riuscita!");
                    return true;
                }

                LOG_DF("OptaRTC", "[NTP] Fallito, passo al prossimo server...");
            }

            LOG_WF("OptaRTC", "[NTP] Errore sincronizzazione NTP");

            return false;
        }


        // ---------------- CALLBACKS ----------------

        void onEverySecond(std::function<void()> cb) { secondCallback = cb; }
        void onEveryMinute(std::function<void()> cb) { minuteCallback = cb; }
        void onEveryHour(std::function<void()> cb) { hourCallback = cb; }
        void onEveryDay(std::function<void()> cb) { dayCallback = cb; }
        void onEveryWeek(std::function<void()> cb) { weekCallback = cb; }
        void onEveryMonth(std::function<void()> cb) { monthCallback = cb; }

        // ---------------- CRON ----------------

        void addCron(const String& expression, std::function<void()> cb) {
            cronList.push_back({expression, cb});
        }

        bool matchCron(const CronEntry& entry, const struct tm& t) {
            int fields[5] = {
                t.tm_min,
                t.tm_hour,
                t.tm_mday,
                t.tm_mon + 1,
                t.tm_wday
            };

            int idx = 0;
            int start = 0;

            for (int i = 0; i < entry.expression.length(); i++) {
                if (entry.expression[i] == ' ' || i == entry.expression.length() - 1) {
                    String token = entry.expression.substring(start, (i == entry.expression.length() - 1) ? i + 1 : i);

                    if (token != "*") {
                        if (token.indexOf('-') > 0) {
                            int a = token.substring(0, token.indexOf('-')).toInt();
                            int b = token.substring(token.indexOf('-') + 1).toInt();
                            if (fields[idx] < a || fields[idx] > b) return false;
                        } else if (token.indexOf(',') > 0) {
                            bool ok = false;
                            int last = 0;
                            for (int j = 0; j <= token.length(); j++) {
                                if (j == token.length() || token[j] == ',') {
                                    int val = token.substring(last, j).toInt();
                                    if (fields[idx] == val) ok = true;
                                    last = j + 1;
                                }
                            }
                            if (!ok) return false;
                        } else {
                            if (fields[idx] != token.toInt()) return false;
                        }
                    }

                    idx++;
                    start = i + 1;
                }
            }

            return true;
        }

    // ---------------- UPDATE ----------------

    void update(unsigned long nowMs) {
        // 1) Evita aggiornamenti troppo ravvicinati
        static unsigned long lastUpdateMs = 0;
        if (nowMs - lastUpdateMs < 200)   // aggiorna max 5 volte al secondo
            return;
        lastUpdateMs = nowMs;

        // 2) Leggi epoch UNA sola volta
        time_t nowEpoch = time(nullptr);

        // 3) Se l’epoch non è avanzato → nessun lavoro da fare
        if (nowEpoch == lastEpoch)
            return;

        lastEpoch = nowEpoch;

        // 4) Converti epoch → tm UNA sola volta
        struct tm t;
        gmtime_r(&nowEpoch, &t);

        // 5) Callback ogni secondo
        if (t.tm_sec != lastSecond) {
            lastSecond = t.tm_sec;
            if (secondCallback) secondCallback();
        }

        // 6) Callback ogni minuto + CRON
        if (t.tm_min != lastMinute) {
            lastMinute = t.tm_min;

            if (minuteCallback) minuteCallback();

            // Esegui cron solo quando cambia il minuto
            for (auto& c : cronList) {
                if (matchCron(c, t)) {
                    if (c.lastMinuteExecuted != t.tm_min) {
                        c.lastMinuteExecuted = t.tm_min;
                        c.callback();
                    }
                }
            }
        }

        // 7) Callback ogni ora
        if (t.tm_hour != lastHour) {
            lastHour = t.tm_hour;
            if (hourCallback) hourCallback();
        }

        // 8) Callback ogni giorno
        if (t.tm_mday != lastDay) {
            lastDay = t.tm_mday;
            if (dayCallback) dayCallback();
        }

        // 9) Callback ogni settimana
        if (t.tm_wday != lastWeek) {
            lastWeek = t.tm_wday;
            if (weekCallback) weekCallback();
        }

        // 10) Callback ogni mese
        int month = t.tm_mon + 1;
        if (month != lastMonth) {
            lastMonth = month;
            if (monthCallback) monthCallback();
        }
    }


    bool isTimeSynced() const {
        return timeSynced;
    }

private:
    bool useWiFi = false;

    std::function<void()> secondCallback;
    std::function<void()> minuteCallback;
    std::function<void()> hourCallback;
    std::function<void()> dayCallback;
    std::function<void()> weekCallback;
    std::function<void()> monthCallback;

    std::vector<CronEntry> cronList;

    int lastSecond = -1;
    int lastMinute = -1;
    int lastHour = -1;
    int lastDay = -1;
    int lastWeek = -1;
    int lastMonth = -1;

    bool timeSynced = false;

    time_t lastEpoch = -1;   // 🔥 AGGIUNGERE QUESTO

    //Diagnostica
    unsigned long lastSyncMs = 0;     // millis() dell’ultima sincronizzazione
    bool everSynced = false;          // almeno una sincronizzazione avvenuta
    tm lastValidTime = {};            // ultima data/ora valida letta
};

#endif
