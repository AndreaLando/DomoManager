\# ============================================================

\# VISIONE DEL PROGETTO DOMOMANAGER

\# ============================================================



\## 1. Perché creare un nuovo manager domotico?

La domanda è legittima: perché sviluppare un sistema come DomoManager

quando esistono già piattaforme diffuse come Home Assistant, Hubitat,

OpenHAB, Shelly Cloud, Tuya, Zigbee2MQTT e decine di ecosistemi RF?



La risposta è semplice:

\*\*perché nessuno di questi sistemi nasce per essere deterministico,

industriale, cablato, silenzioso e realmente affidabile.\*\*



La domotica moderna è diventata un insieme di gadget:

\- dispositivi che parlano solo via RF

\- cloud obbligatori

\- automazioni non deterministiche

\- firmware chiusi

\- ecosistemi frammentati

\- comportamenti imprevedibili sotto carico

\- dipendenza totale dalla rete Wi‑Fi



DomoManager nasce per ribaltare questo paradigma.



\---



\## 2. La nostra visione

DomoManager vuole riportare la domotica nel suo contesto naturale:

\*\*impiantistica, affidabile, industriale, cablata, prevedibile.\*\*



La casa non è un giocattolo.  

Non è un insieme di lampadine smart.  

È un sistema complesso che merita strumenti professionali.



La nostra visione si basa su quattro pilastri:



\---



\## 3. Pilastro 1 — Minimizzare le emissioni RF

La casa moderna è invasa da:

\- Zigbee

\- Wi‑Fi

\- Bluetooth

\- Thread

\- Sub‑GHz proprietari

\- ponti, bridge, mesh, repeater



Ogni dispositivo aggiunge:

\- rumore elettromagnetico

\- latenza

\- instabilità

\- dipendenza da batterie

\- interferenze



DomoManager segue un principio opposto:

\*\*prima il cavo, poi il resto.\*\*



Preferiamo:

\- Modbus cablato

\- RS485

\- ingressi digitali

\- sensori a filo

\- attuatori a relè

\- Ethernet



La radio è l’ultima risorsa, non la prima.



\---



\## 4. Pilastro 2 — Portare la domotica da “gadget” a “industriale”

La maggior parte dei sistemi domotici consumer:

\- non ha watchdog

\- non ha fallback

\- non ha diagnostica

\- non ha timestamp affidabili

\- non ha logiche deterministiche

\- non ha protezioni hardware

\- non ha hot‑standby

\- non ha buffer centralizzato



DomoManager invece eredita concetti dei PLC:

\- cicli deterministici

\- zero allocazioni dinamiche ricorrenti

\- buffer come unica fonte di verità

\- diagnostica continua

\- protezioni e failover

\- architettura modulare ma rigorosa



La casa deve funzionare \*\*sempre\*\*, anche senza internet,

anche senza Wi‑Fi, anche senza cloud.



\---



\## 5. Pilastro 3 — Ridurre la complessità, aumentare la leggibilità

Le piattaforme consumer sono spesso:

\- configurate tramite YAML complessi

\- piene di automazioni duplicate

\- difficili da debuggare

\- soggette a comportamenti “magici”



DomoManager invece:

\- usa un buffer centrale

\- ha automazioni dichiarative

\- ha regole leggibili

\- ha sequenze strutturate

\- ha diagnostica integrata

\- ha un flusso dati chiaro e tracciabile



Ogni decisione è motivata.  

Ogni valore è visibile.  

Ogni errore è spiegato.



\---



\## 6. Pilastro 4 — Un sistema operativo per la casa

DomoManager non è un’app.  

Non è un bridge.  

Non è un firmware.



È un \*\*sistema operativo embedded\*\*, con:

\- backend deterministico

\- frontend modulare

\- automazioni dichiarative

\- buffer centrale

\- motori specializzati (HVAC, Power, Weather, Security)

\- integrazioni esterne (MQTT, WebAPI, RS485)

\- diagnostica avanzata



La casa diventa un ecosistema coerente, non un insieme di dispositivi.



\---



\## 7. Quali necessità vogliamo soddisfare?

\### 7.1 Affidabilità 24/7

La casa deve funzionare sempre, anche quando:

\- manca internet

\- il Wi‑Fi cade

\- un dispositivo RF si scarica

\- un cloud è offline



\### 7.2 Predicibilità

Ogni ciclo deve essere ripetibile.  

Ogni automazione deve essere deterministica.



\### 7.3 Basso rumore elettromagnetico

Meno RF = più stabilità, più sicurezza, meno interferenze.



\### 7.4 Manutenibilità

Il sistema deve essere leggibile da un tecnico, non solo da uno sviluppatore.



\### 7.5 Scalabilità

Aggiungere moduli non deve rompere nulla.



\### 7.6 Sicurezza

La casa non deve dipendere da server remoti.



\---



\## 8. In sintesi

DomoManager nasce per chi vuole una domotica:

\- affidabile come un PLC

\- pulita come un impianto elettrico

\- leggibile come un manuale tecnico

\- estendibile come un framework moderno

\- silenziosa come un sistema cablato

\- indipendente come un dispositivo industriale



Non è un’alternativa a Home Assistant.  

È un livello più basso, più profondo, più solido.



È la base su cui costruire una casa intelligente \*\*che non si rompe\*\*.



===============================================================

\# 1. FILOSOFIA DEL PROGETTO

===============================================================



\## 1.1 Perché nasce DomoManager

La domotica moderna è frammentata: dispositivi incompatibili, app

proprietarie, logiche duplicate, cloud obbligatorio, comportamenti

non deterministici.



DomoManager nasce per risolvere tutto questo.



È un sistema operativo per la casa:

\- deterministico

\- affidabile

\- comprensibile

\- estendibile

\- sicuro

\- indipendente dal cloud



La casa non è un insieme di oggetti intelligenti.  

È un organismo.  

DomoManager è il suo sistema nervoso.



\---



\## 1.2 Principi di design

\- \*\*Determinismo\*\*: nessuna allocazione dinamica ricorrente, nessuna

&#x20; logica nascosta.

\- \*\*Buffer‑centrico\*\*: un’unica fonte di verità per tutto il sistema.

\- \*\*Modularità reale\*\*: moduli indipendenti ma coerenti.

\- \*\*Zero magia\*\*: ogni comportamento è dichiarato e tracciabile.

\- \*\*Utente come architetto\*\*: automazioni leggibili, JSON umano.

\- \*\*Robustezza industriale\*\*: watchdog, fallback, hotstandby.

\- \*\*Tempo come fondamento\*\*: il TimeManager è il metronomo del sistema.



===============================================================

\# 2. PANORAMICA GENERALE DEL SISTEMA

===============================================================



DomoManager è costruito come un edificio a più piani:



1\. \*\*Hardware\*\*  

&#x20;  Sensori, attuatori, Modbus, RS485, Opta/Arduino.



2\. \*\*Backend\*\*  

&#x20;  Device Manager, Modbus Engine, Buffer Engine, Time Manager.



3\. \*\*Automazioni\*\*  

&#x20;  Scene, regole, sequenze, scheduled rules.



4\. \*\*Frontend Engines\*\*  

&#x20;  HVAC, Power, Weather, Security, MQTT, WebAPI, RS485, HotStandby.



5\. \*\*Integrazioni\*\*  

&#x20;  Home Assistant, Zigbee2MQTT, Shelly, HTTP.



6\. \*\*Diagnostica\*\*  

&#x20;  Analisi completa di ogni componente.



Il buffer è il cuore del sistema: tutto passa da lì.



===============================================================

\# 3. ARCHITETTURA TECNICA DETTAGLIATA

===============================================================



\## 3.1 Buffer Engine

\- Aree tipizzate

\- Timestamp

\- Change tracking

\- Virtual areas

\- Reverse / Split / Toggle



È la lingua franca del sistema.



\## 3.2 Device Manager \& Modbus Engine

\- Profili dispositivi

\- Mappatura aree → registri

\- Polling deterministico

\- Timeout / Retry / Cooldown



\## 3.3 Time Manager

\- RTC hardware

\- Fallback millis()

\- Callback periodici

\- Base temporale per HVAC, automazioni, power, sicurezza



\## 3.4 Automation Engine

\- Scene

\- Regole

\- Sequenze

\- Trend / Debounce / Composite

\- Scheduled rules



\## 3.5 Frontend Engines

\- HVAC Engine

\- Power Engine

\- Weather Engine

\- Security Engine

\- MQTT Engine

\- WebAPI Engine

\- RS485 Engine

\- HotStandby Engine



\## 3.6 Diagnostica

\- Analisi buffer

\- Analisi automazioni

\- Analisi dispositivi

\- Analisi RTC

\- Analisi sicurezza

\- Analisi HVAC

\- Analisi power



===============================================================

\# 4. DIAGRAMMA ASCII – DIPENDENZE FILE

===============================================================



```

domomanager.ino

│

├── DMUsb.hpp

├── DMFrontend.hpp

│   ├── DMFrontendEngines.hpp

│   ├── DMAutomation.hpp

│   ├── DMBridge.hpp

│   ├── DMPLC.hpp

│   ├── DMEquipment.hpp

│   ├── DMBuffers.hpp

│   ├── DMWeather.hpp

│   ├── DMSetup.hpp

│   ├── DMOptaRTC.hpp

│   ├── DMDiagnostic.hpp

│   ├── DMFncs.hpp

│   └── DMLogger.hpp

└── DMLogger.hpp

```



===============================================================

\# 5. DIAGRAMMA ASCII – ARCHITETTURA A BLOCCHI

===============================================================



```

&#x20;                        ┌──────────────────────┐

&#x20;                        │      HARDWARE        │

&#x20;                        └──────────┬───────────┘

&#x20;                                   │

&#x20;                                   ▼

┌────────────────────────────────────────────────────────────┐

│                      DOMOMANAGER CORE                      │

└───────────┬────────────────────────────────────────────────┘

&#x20;           ▼

┌────────────────────────────────────────────────────────────┐

│                        BUFFER ENGINE                        │

└───────────┬────────────────────────────────────────────────┘

&#x20;           ▼

┌────────────────────────────────────────────────────────────┐

│                     DEVICE MANAGER                          │

└───────────┬────────────────────────────────────────────────┘

&#x20;           ▼

┌────────────────────────────────────────────────────────────┐

│                      MODBUS ENGINE                          │

└───────────┬────────────────────────────────────────────────┘

&#x20;           ▼

┌────────────────────────────────────────────────────────────┐

│                       TIME MANAGER                          │

└───────────┬────────────────────────────────────────────────┘

&#x20;           ▼

┌────────────────────────────────────────────────────────────┐

│                    AUTOMATION ENGINE                        │

└───────────┬────────────────────────────────────────────────┘

&#x20;           ▼

┌────────────────────────────────────────────────────────────┐

│                       FRONTEND ENGINE                       │

└───────────┬────────────────────────────────────────────────┘

&#x20;           ▼

&#x20;  \[HVAC] \[POWER] \[WEATHER] \[SECURITY] \[MQTT] \[WEBAPI] \[RS485]

```



===============================================================

\# 6. DIAGRAMMA ASCII – FLUSSO DATI

===============================================================



```

MONDO FISICO → Device Manager → Modbus Engine → BUFFER →

Frontend Engines → Automation Engine → MQTT/WebAPI/RS485 →

ATTUATORI

```



===============================================================

\# 7. DIAGRAMMA ASCII – CICLO DI ESECUZIONE

===============================================================



```

LOOP()

&#x20; → Time Manager

&#x20; → Device Manager

&#x20; → Modbus Engine

&#x20; → Buffer Engine

&#x20; → Automation Engine

&#x20; → Frontend Engines

&#x20; → MQTT / WebAPI / RS485

&#x20; → Attuatori

&#x20; → Diagnostica (parallela)

```



===============================================================

\# 8. LICENZA

===============================================================



Questo documento è parte del progetto DomoManager.





