\# ============================================================

\# PANORAMICA GENERALE DEL SISTEMA

\# ============================================================



\## Introduzione

DomoManager non è un semplice firmware, né un hub, né un gateway.

È un \*\*ecosistema completo\*\*, progettato per governare una casa come

se fosse un organismo vivente: con un sistema nervoso, organi,

riflessi, memoria e capacità di adattamento.



Tutto nasce da un’idea: la domotica non deve essere un insieme di

dispositivi che comunicano, ma un sistema coerente che interpreta,

decide e agisce. DomoManager è questo: un cervello centrale che

coordina sensori, attuatori, automazioni, energia, clima, sicurezza,

comunicazioni e diagnostica.



La sua architettura è pensata per essere:

\- deterministica

\- robusta

\- modulare

\- estendibile

\- leggibile

\- priva di magia nascosta



Ogni componente ha un ruolo preciso, ogni dato ha un luogo unico,

ogni decisione è tracciabile.



\---



\## Una visione d’insieme

Per capire DomoManager bisogna immaginarlo come un edificio a più

piani, dove ogni livello ha una responsabilità chiara ma tutti

collaborano in modo armonico.



\### Livello 1 — Hardware e mondo fisico

Qui vivono:

\- sensori cablati

\- ingressi digitali e analogici

\- attuatori (pompe, valvole, fan coil, carichi elettrici)

\- dispositivi Modbus TCP/RTU

\- rete Ethernet

\- RS485

\- RTC hardware



È il livello più vicino alla realtà: ciò che accade nella casa

entra da qui, e ciò che la casa deve fare esce da qui.



\---



\### Livello 2 — Buffer: la memoria centrale

Il buffer è il \*\*cuore pulsante\*\* del sistema.

Tutto ciò che esiste nel mondo fisico viene rappresentato qui dentro:

temperature, stati, comandi, allarmi, potenze, valori analogici,

bitmask, scene, automazioni.



Il buffer è:

\- l’unica fonte di verità

\- l’unico punto di lettura/scrittura

\- il linguaggio comune tra tutti i moduli



Non esistono variabili sparse, stati duplicati o valori nascosti.

Ogni informazione è tracciata, tipizzata, timestampata.



\---



\### Livello 3 — Device Manager e Modbus Engine

Questo livello collega il mondo fisico al buffer.



Il Device Manager:

\- conosce i dispositivi

\- assegna le aree

\- gestisce errori, priorità e routing



Il Modbus Engine:

\- interroga i dispositivi

\- legge ingressi

\- scrive uscite

\- applica soglie, toggle, split

\- aggiorna il buffer in modo deterministico



È il ponte tra hardware e logica.



\---



\### Livello 4 — Time Manager

Il tempo è una dimensione fondamentale del sistema.



Il Time Manager:

\- fornisce epoch affidabile

\- sincronizza con RTC o NTP

\- gestisce callback per secondi, minuti, ore, giorni

\- alimenta automazioni, HVAC, ACS, sicurezza, power



Senza un tempo stabile, la casa non può comportarsi in modo coerente.



\---



\### Livello 5 — Automation Engine

Qui vive il comportamento della casa.



L’Automation Engine:

\- valuta condizioni

\- attiva scene

\- esegue sequenze

\- gestisce regole temporali

\- esegue automazioni dinamiche



È un motore dichiarativo, leggibile, tracciabile.

Ogni decisione è motivata, ogni azione è registrata.



\---



\### Livello 6 — Frontend Engines

Sono i “grandi organi” della casa.



\- \*\*HVAC Engine\*\*: gestisce pompe di calore, fan coil, ACS, anti-legionella

\- \*\*Power Engine\*\*: controlla carichi, limiti, forecast solare, auto-tuning

\- \*\*Weather Engine\*\*: elabora meteo, pioggia, vento, luce, allarmi

\- \*\*Security Engine\*\*: gestisce sensori cablati, zone, allarmi

\- \*\*MQTT Engine\*\*: integra Home Assistant, Zigbee2MQTT, Shelly

\- \*\*WebAPI Engine\*\*: comunica con dispositivi HTTP

\- \*\*AEE Engine\*\*: sincronizza variabili verso pannelli e frontend

\- \*\*Task Engine\*\*: orchestra i cicli frontend



Ogni motore legge dal buffer, elabora, decide e scrive nel buffer.



\---



\### Livello 7 — Comunicazione e integrazione

DomoManager parla molte lingue:

\- MQTT

\- HTTP

\- RS485

\- UDP/JSON (Bridge AEE)

\- Modbus TCP/RTU



Ogni protocollo è integrato in modo deterministico, senza blocchi,

senza thread, senza allocazioni ricorrenti.



\---



\### Livello 8 — Diagnostica

La diagnostica è un pilastro del sistema.



Il Diagnostic Engine:

\- analizza automazioni

\- analizza scheduler

\- analizza split

\- analizza buffer

\- analizza dispositivi

\- analizza RTC

\- analizza sicurezza

\- analizza power

\- analizza HVAC



Ogni anomalia è visibile, spiegata, tracciata.



\---



\## Un sistema che pensa

La forza di DomoManager non è nei singoli moduli, ma nel modo in cui

collaborano.



Esempio:

\- il Weather Engine rileva pioggia

\- il Security Engine sa quali finestre sono aperte

\- l’Automation Engine decide di chiuderle

\- il Power Engine valuta se la potenza è sufficiente

\- il buffer sincronizza tutto

\- il Time Manager garantisce che accada nel momento giusto



È un ecosistema, non un insieme di pezzi.



\---



\## Un sistema che cresce

DomoManager è progettato per evolvere.



Aggiungere un nuovo modulo significa:

\- definire le aree nel buffer

\- leggere ciò che serve

\- scrivere ciò che serve

\- collegarsi al Time Manager

\- registrare diagnostica



Non serve modificare il resto del sistema.

La modularità è reale, non teorica.



\---



\# ============================================================

\# FINE PANORAMICA GENERALE

\# ============================================================



