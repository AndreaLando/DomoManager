\# ============================================================

\# ARCHITETTURA TECNICA DETTAGLIATA

\# ============================================================



\## Introduzione

L’architettura di DomoManager è costruita come un sistema operativo

per la casa: un insieme di componenti indipendenti ma coordinati,

ognuno con responsabilità precise, che collaborano attraverso un

linguaggio comune: il buffer.



Questa sezione descrive in modo discorsivo e approfondito come

funziona l’intero ecosistema dal punto di vista tecnico, seguendo

il flusso naturale dei dati: dal mondo fisico → al buffer →

ai motori logici → alle automazioni → ai protocolli esterni.



\---



\## 1. Il cuore del sistema: il Buffer Engine

Il buffer è la “memoria centrale” del sistema.  

Tutto ciò che esiste nella casa — sensori, attuatori, stati, comandi,

allarmi, valori analogici, bitmask, scene — vive qui dentro.



Ogni area del buffer è:

\- tipizzata (Field, FromPanel, ToPanel)

\- timestampata

\- tracciata per cambiamento

\- accessibile da ogni modulo

\- protetta da regole di lettura/scrittura



Il buffer è progettato per essere:

\- \*\*deterministico\*\* (nessuna allocazione dinamica ricorrente)

\- \*\*veloce\*\* (lookup O(1))

\- \*\*coerente\*\* (un’unica fonte di verità)

\- \*\*diagnosticabile\*\* (report di aree mancanti, duplicate, non inizializzate)



Tutto il resto del sistema si appoggia a lui.



\---



\## 2. Device Manager e Modbus Engine

Il mondo fisico entra nel sistema attraverso il Device Manager.



\### 2.1 Device Manager

Gestisce:

\- profili dispositivi

\- mappatura aree → registri

\- priorità

\- errori

\- routing

\- cooldown intelligente



Ogni dispositivo è descritto da una configurazione statica che

definisce:

\- indirizzo IP

\- registri da leggere/scrivere

\- tipo di dato

\- frequenza di polling



\### 2.2 Modbus Engine

Il Modbus Engine è il “traduttore” tra dispositivi e buffer.



Funziona in modo:

\- non bloccante

\- round‑robin

\- con retry e cooldown

\- con chiusura connessione forzata (necessaria su Opta/NINA)

\- con gestione errori deterministica



Ogni lettura aggiorna il buffer.

Ogni scrittura parte dal buffer.



\---



\## 3. Time Manager: il metronomo del sistema

Il Time Manager fornisce un tempo affidabile al sistema.



Caratteristiche:

\- sincronizzazione RTC hardware

\- fallback su millis()

\- conversione epoch → struct tm

\- callback periodici (secondo, minuto, ora, giorno)

\- diagnostica (ultima sync, validità RTC)



Il tempo è fondamentale per:

\- automazioni

\- HVAC

\- ACS

\- anti‑legionella

\- power manager

\- sicurezza

\- scheduler



Il Time Manager è la “dimensione temporale” del sistema.



\---



\## 4. Automation Engine: il comportamento della casa

L’Automation Engine è il cervello logico del sistema.



Gestisce:

\- scene

\- regole

\- condizioni

\- sequenze

\- automazioni dinamiche

\- scheduled rules

\- composite rules

\- trend rules

\- debounce rules

\- bitmask rules



Ogni automazione è:

\- dichiarativa

\- tracciabile

\- deterministica

\- priva di allocazioni ricorrenti



Il builder JSON (AutomationBuilder) traduce la configurazione in

strutture interne ottimizzate.



\---



\## 5. Frontend Engines: i grandi organi del sistema

Ogni motore frontend è un “organo” specializzato che legge dal buffer,

elabora, decide e scrive nel buffer.



\### 5.1 HVAC Engine

Gestisce:

\- zone

\- setpoint

\- fan coil

\- compressore

\- valvola tre vie

\- ACS

\- anti‑legionella

\- defrost

\- sicurezza temperature

\- finestra aperta



Il ciclo HVAC è sincronizzato con il Time Manager e con il buffer.



\### 5.2 Power Engine

Gestisce:

\- carichi prioritari

\- limiti soft/hard

\- forecast solare

\- auto‑tuning

\- minOn/minOff

\- suggerimenti

\- protezione rete



È un motore di ottimizzazione energetica in tempo reale.



\### 5.3 Weather Engine

Elabora:

\- temperatura

\- vento

\- pioggia

\- luce



Con:

\- medie mobili O(1)

\- debounce

\- eventi (RainStart, WindGust, DayStart)

\- allarmi



\### 5.4 Security Engine

Gestisce sensori cablati:

\- PIR

\- DOOR

\- WINDOW

\- SMOKE

\- FLOOD

\- TAMPER



Con:

\- zone

\- bitmask

\- callback

\- startup inhibit



\### 5.5 MQTT Engine

Integra:

\- Home Assistant

\- Zigbee2MQTT

\- Shelly



Pubblica e riceve stati dal buffer.



\### 5.6 WebAPI Engine

Gestisce dispositivi HTTP con:

\- GET/POST

\- correlazione

\- parsing pattern

\- mapping verso buffer



\---



\## 6. Task Engine: l’orchestratore dei cicli

Il Task Engine coordina l’esecuzione dei motori frontend.



Ogni task ha:

\- intervallo

\- lastRun

\- enable flag



Il ciclo è:

1\. backend (Modbus, buffer, automazioni)

2\. frontend (HVAC, Weather, Power, Security, MQTT)

3\. callback full cycle



Il sistema garantisce:

\- nessun blocco

\- nessuna starvation

\- nessuna concorrenza

\- ordine deterministico



\---



\## 7. Comunicazione e integrazione

DomoManager parla molte lingue.



\### 7.1 MQTT

Pubblica:

\- sensori

\- binary\_sensor

\- switch

\- climate

\- light

\- cover



Riceve comandi e aggiorna il buffer.



\### 7.2 WebAPI

Gestisce dispositivi HTTP con:

\- profili

\- correlazione

\- parsing pattern

\- mapping aree



\### 7.3 RS485

Gestisce:

\- frame

\- ack/nack

\- retry

\- state machine



\### 7.4 HotStandby

Gestisce:

\- master/slave

\- heartbeat

\- failover

\- sincronizzazione stato



\---



\## 8. Diagnostica: il sistema immunitario

Il Diagnostic Engine analizza:

\- automazioni

\- scheduler

\- split

\- buffer

\- dispositivi

\- RTC

\- sicurezza

\- power

\- HVAC

\- hotstandby



Ogni anomalia è:

\- rilevata

\- spiegata

\- tracciata



La diagnostica è un pilastro del sistema.



\---



\## 9. DomoManager Core: il direttore d’orchestra

Il core gestisce:

\- setup

\- validazione configurazioni

\- applicazione aree/toggles/splits/routes

\- caricamento automazioni

\- ciclo backend

\- ciclo frontend

\- watchdog

\- diagnostica

\- orchestrazione generale



È il punto di incontro di tutti i moduli.



\---



\# ============================================================

\# FINE ARCHITETTURA TECNICA DETTAGLIATA

\# ============================================================



