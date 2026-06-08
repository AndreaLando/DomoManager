\# ============================================================

\# DOMOMANAGER – CHANGELOG VERSIONE BETA

\# ============================================================



Questa è la prima release \*\*BETA pubblica\*\* di DomoManager.  

Rappresenta la transizione ufficiale del progetto da prototipo

privato a piattaforma domotica industriale aperta alla comunità.



La Beta introduce:

\- un’architettura completa e stabile

\- un ciclo deterministico backend→buffer→frontend

\- un sistema di automazioni dichiarative

\- un ecosistema di motori (HVAC, Power, Weather, Security)

\- un livello di diagnostica industriale

\- integrazioni con MQTT, WebAPI, RS485, Modbus



È la base solida su cui costruire la versione \*\*1.0\*\*.



\---



\# 1. Novità principali



\## 1.1 Architettura buffer‑centrica (NUOVA)

\- Introduzione del \*\*Buffer Engine\*\* come unica fonte di verità.

\- Supporto per:

&#x20; - aree tipizzate

&#x20; - timestamp

&#x20; - change tracking

&#x20; - virtual areas

&#x20; - reverse / split / toggle

\- Diagnostica completa su aree non inizializzate, duplicate, inutilizzate.



\*\*Impatto:\*\*  

La casa diventa un sistema coerente, leggibile e tracciabile.



\---



\## 1.2 Backend deterministico (NUOVO)

\- Device Manager con priorità, cooldown, errori intelligenti.

\- Modbus Engine con:

&#x20; - polling round‑robin

&#x20; - retry deterministici

&#x20; - scritture sicure

&#x20; - gestione errori robusta

\- Time Manager con RTC + fallback + cron + callback periodiche.



\*\*Impatto:\*\*  

Il sistema funziona come un PLC, non come un gadget RF.



\---



\## 1.3 Automation Engine (NUOVO)

\- Scene dichiarative

\- Regole con condizioni multiple

\- Sequenze temporizzate

\- Scheduled rules

\- Trend / Debounce / Composite rules

\- Builder JSON con validazione completa



\*\*Impatto:\*\*  

Automazioni leggibili, prevedibili, senza magia.



\---



\## 1.4 Frontend Engines (NUOVO)

\### HVAC Engine

\- Gestione pompe di calore

\- Zone multiple

\- ACS + anti‑legionella

\- Defrost

\- Sicurezze outdoor/window



\### Power Engine

\- Gestione carichi prioritari

\- Forecast solare

\- Auto‑tuning

\- Protezione rete



\### Weather Engine

\- Medie mobili O(1)

\- Pioggia / vento / luce

\- Eventi meteo

\- Allarmi



\### Security Engine

\- Sensori cablati

\- Zone

\- Allarmi

\- Aggregazione stato



\*\*Impatto:\*\*  

La casa diventa un ecosistema coordinato.



\---



\## 1.5 Integrazioni (NUOVO)

\### MQTT Engine

\- Home Assistant

\- Zigbee2MQTT

\- Shelly



\### WebAPI Engine

\- GET/POST non bloccanti

\- Profili messaggi

\- Correlazione out→in



\### RS485 Engine

\- Frame con ACK/NACK

\- Retry

\- State machine non bloccante



\### HotStandby

\- Master/slave

\- Failover automatico

\- Replica stato/process/timestamp



\*\*Impatto:\*\*  

La casa parla molte lingue, ma resta deterministica.



\---



\## 1.6 Diagnostica industriale (NUOVO)

\- Analisi automazioni

\- Analisi scheduler

\- Analisi split

\- Analisi buffer

\- Analisi dispositivi

\- Analisi RTC

\- Analisi sicurezza

\- Analisi power/HVAC

\- Watch areas



\*\*Impatto:\*\*  

Ogni anomalia è visibile, spiegata, tracciata.



\---



\# 2. Miglioramenti rispetto alle versioni Alpha



\## 2.1 Stabilità

\- Eliminati comportamenti non deterministici.

\- Ridotte allocazioni dinamiche.

\- Migliorata gestione errori Modbus.

\- Ottimizzato ciclo backend.



\## 2.2 Performance

\- Buffer più veloce (lookup O(1)).

\- Medie mobili O(1) nel Weather Engine.

\- Riduzione log superflui.

\- Ottimizzazione split/toggle.



\## 2.3 Robustezza

\- Watchdog migliorato.

\- Validazione configurazioni più severa.

\- Failover HotStandby più rapido.



\---



\# 3. Funzionalità deprecate o rimosse



\## 3.1 RF superflua

\- Nessun supporto nativo per Zigbee/Thread/Wi‑Fi devices.

\- RF solo tramite MQTT (Z2M, Shelly) e solo se necessario.



\## 3.2 Automazioni hardcoded

\- Rimosse tutte le logiche fisse.

\- Tutto è dichiarativo via JSON.



\## 3.3 Dipendenze esterne

\- Nessun cloud richiesto.

\- Nessun servizio esterno obbligatorio.



\---



\# 4. Limitazioni note della Beta



\- Documentazione HVAC/Power in espansione.

\- WebAPI Engine ancora privo di alcuni profili.

\- Auto‑tuning Power in fase di ottimizzazione.

\- HotStandby non ancora testato in cluster reali.

\- Mancano strumenti grafici di configurazione.



\---



\# 5. Obiettivi della prossima release (RC1)



\- Editor JSON ufficiale

\- Profili WebAPI aggiuntivi

\- Dashboard diagnostica

\- Miglioramento forecast solare

\- Ottimizzazione ACS/anti‑legionella

\- Supporto a più modelli di pompe di calore

\- Test estesi su RS485 e HotStandby



\---



\# 6. Contributi

Vuoi contribuire?



Leggi:

\- \*\*CONTRIBUTING.md\*\*

\- \*\*CODE\_OF\_CONDUCT.md\*\*

\- \*\*ROADMAP.md\*\*



Ogni contributo deve rispettare la filosofia industriale del progetto.



\# ============================================================

\# FINE CHANGELOG BETA

\# ============================================================



