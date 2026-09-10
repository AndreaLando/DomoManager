# ============================================================
# DOMOMANAGER – PANORAMICA GENERALE DEL SISTEMA
# ============================================================

## 1. Introduzione

DomoManager non è un semplice firmware, né un hub, né un gateway.

È un **runtime embedded per l'infrastruttura domestica**, progettato per governare una casa come un sistema unico.

L'obiettivo non è semplicemente permettere a dispositivi diversi di comunicare.

L'obiettivo è costruire un sistema capace di:

- acquisire informazioni
- rappresentare lo stato della casa
- elaborare condizioni
- prendere decisioni
- comandare dispositivi
- coordinare automazioni
- gestire energia e climatizzazione
- gestire sicurezza
- comunicare con sistemi esterni
- diagnosticare il proprio comportamento

Tutto questo all'interno di un'architettura coerente.

La filosofia fondamentale è:

> **la domotica non deve essere un insieme di dispositivi intelligenti, ma un sistema coerente che interpreta, decide e agisce.**

DomoManager è progettato per essere:

- deterministico
- non-blocking
- robusto
- modulare
- estendibile
- leggibile
- diagnosticabile
- local-first
- embedded-first
- consapevole delle risorse hardware

Ogni componente ha una responsabilità precisa.

Ogni informazione ha una rappresentazione coerente.

Ogni decisione deve poter essere ricostruita.

---

# 2. Il modello generale

L'architettura di DomoManager può essere rappresentata attraverso quattro grandi domini:

```text id="3zqf4a"
                    MONDO FISICO
                         │
                         ▼
                 ┌───────────────┐
                 │    BACKEND    │
                 │ Device/Modbus │
                 │   I/O / RS485 │
                 └───────┬───────┘
                         │
                         ▼
                 ┌───────────────┐
                 │     BUFFER    │
                 │  SYSTEM STATE │
                 └───────┬───────┘
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
         TASK ENGINE  AUTOMATION   FRONTEND
             │         ENGINE       ENGINES
             │           │           │
             └───────────┼───────────┘
                         │
                         ▼
              COMMUNICATION SCHEDULER
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
          BRIDGE       MQTT        WebAPI
```

Attorno a questi domini operano servizi trasversali fondamentali:

- Time Manager
- Diagnostics
- Watchdog
- Hot Standby
- gestione delle risorse

Il risultato è un sistema nel quale:

**Backend → Buffer → Logic → Communication**

rappresenta il flusso principale delle informazioni.

---

# 3. Il mondo fisico

Il primo livello è costituito dalla realtà fisica della casa.

Comprende:

- sensori
- ingressi digitali
- ingressi analogici
- attuatori
- pompe
- valvole
- fan coil
- carichi elettrici
- dispositivi Modbus
- dispositivi RS485
- dispositivi Ethernet
- RTC
- eventuali dispositivi wireless

È il punto nel quale il sistema incontra il mondo reale.

Le informazioni provenienti dall'impianto vengono acquisite dal Backend.

Le decisioni elaborate dal sistema vengono successivamente trasformate in comandi verso questo livello.

---

# 4. Backend

Il Backend rappresenta il collegamento tra il mondo fisico e il modello software della casa.

I principali componenti comprendono:

- Device Manager
- Modbus Engine
- protocolli RS485
- I/O locali
- gestione dei dispositivi
- gestione degli errori
- retry
- timeout
- priorità
- cooldown

Il Backend ha una responsabilità precisa:

> **acquisire e aggiornare lo stato del mondo fisico senza incorporare la logica applicativa.**

Questo principio è fondamentale.

Il driver di un dispositivo non deve diventare il luogo nel quale viene implementata la logica della casa.

Il dispositivo fornisce dati.

Il sistema li interpreta successivamente.

---

# 5. Buffer Engine

Il Buffer Engine è il **punto centrale dello stato del sistema**.

Tutti i principali componenti utilizzano il Buffer come rappresentazione comune dello stato.

Nel Buffer possono essere rappresentati:

- temperature
- stati
- comandi
- allarmi
- potenze
- valori analogici
- bitmask
- informazioni di sicurezza
- dati HVAC
- dati Weather
- dati Power
- stati delle automazioni
- aree virtuali

Il Buffer fornisce inoltre:

- aree tipizzate
- timestamp
- change tracking
- virtual areas
- reverse
- split
- toggle
- lookup rapido
- diagnostica

Il principio fondamentale è:

> **il Buffer è la fonte di verità del sistema.**

Questo permette di evitare, dove possibile:

- stati duplicati
- variabili nascoste
- sincronizzazioni manuali
- copie incoerenti
- dipendenze dirette tra Engine

Il Buffer è quindi molto più di una memoria dati.

È il **linguaggio comune di DomoManager**.

---

# 6. Time Manager

Il tempo è una dimensione fondamentale dell'architettura.

Il Time Manager fornisce il riferimento temporale utilizzato dal sistema per:

- timestamp
- scheduling
- timeout
- retry
- cooldown
- automazioni
- HVAC
- ACS
- sicurezza
- Power
- eventi
- diagnostica

Il tempo può essere sincronizzato attraverso:

- RTC
- NTP
- meccanismi di fallback

Il sistema non deve conoscere soltanto:

> **"Qual è lo stato?"**

ma anche:

> **"Quando è stato rilevato?"**

e:

> **"Da quanto tempo è così?"**

Il Time Manager è quindi il **metronomo del runtime**.

---

# 7. Task Engine

Il Task Engine coordina l'esecuzione delle attività applicative.

I task vengono definiti attraverso:

- responsabilità
- callback
- intervallo
- stato enabled/disabled

Tra le attività gestite rientrano:

- Security / Sensors
- HVAC
- Averages
- Communication

Il Task Engine separa:

```text id="abm6yo"
COSA FARE
   │
   ▼
TASK
   │
   ▼
QUANDO ESEGUIRLO
```

dalla logica interna del singolo Engine.

Questo permette di ottenere un runtime più prevedibile e di evitare che ogni modulo debba implementare autonomamente la propria politica di scheduling.

---

# 8. Automation Engine

L'Automation Engine rappresenta il livello comportamentale della casa.

È il componente che trasforma:

```text
STATO + CONDIZIONI + TEMPO
             │
             ▼
         DECISIONE
             │
             ▼
           AZIONE
```

Supporta concetti come:

- scene
- regole
- condizioni multiple
- sequenze
- regole temporali
- trend
- debounce
- composite rules
- configurazione JSON
- validazione

L'Automation Engine non deve conoscere i dettagli dei dispositivi.

Lavora sul modello di stato fornito dal sistema.

Questo permette di separare:

**come un dispositivo funziona**

da:

**perché la casa decide di utilizzarlo.**

---

# 9. Frontend Engines

I Frontend Engines rappresentano le principali funzioni applicative della casa.

Ogni Engine ha una responsabilità specifica, ma condivide il modello comune del Buffer.

## HVAC Engine

Gestisce:

- pompe di calore
- zone
- temperature
- fan coil
- ACS
- anti-legionella
- defrost
- finestre
- protezioni
- calendari HVAC

---

## Power Engine

Gestisce:

- carichi prioritari
- limiti di potenza
- protezione della rete
- produzione solare
- forecast
- gestione dinamica dei carichi
- auto-tuning

---

## Weather Engine

Gestisce:

- temperatura
- pioggia
- vento
- luce
- eventi meteorologici
- allarmi
- medie mobili

---

## Security Engine

Gestisce:

- sensori
- zone
- stati di sicurezza
- allarmi
- aggregazione degli stati
- eventi
- diagnostica

---

## AEE Engine

Gestisce la sincronizzazione delle variabili e degli eventi verso i pannelli e i frontend che utilizzano il protocollo AEE.

---

# 10. Communication Layer

La comunicazione non rappresenta la logica della casa.

È un **livello di integrazione**.

DomoManager può comunicare attraverso:

- MQTT
- HTTP
- Modbus TCP
- Modbus RTU
- RS485
- UDP/JSON
- Bridge AEE

Il principio fondamentale è:

> **la casa deve continuare a ragionare anche quando una comunicazione esterna è lenta o temporaneamente indisponibile.**

Per questo la comunicazione è separata dal percorso principale della logica applicativa.

---

# 11. Communication Scheduler

I principali servizi di comunicazione vengono coordinati attraverso un Communication Scheduler dedicato.

```text id="5ddt2k"
             COMMUNICATION SCHEDULER
                       │
          ┌────────────┼────────────┐
          ▼            ▼            ▼
       BRIDGE        MQTT         WebAPI
```

Ogni servizio può essere associato a:

- intervallo
- priorità
- enabled/disabled
- callback

Il modello di esecuzione è basato su scheduling temporale e round-robin.

L'obiettivo è impedire che un singolo servizio possa monopolizzare il runtime frontend.

Questo è particolarmente importante quando più servizi competono per risorse limitate come:

- CPU
- memoria
- connessioni
- socket Ethernet
- banda
- tempo di esecuzione

Il Communication Scheduler è quindi una componente importante della strategia **non-blocking** di DomoManager.

---

# 12. Separazione tra applicazione e comunicazione

Uno dei principi più importanti dell'architettura è la separazione tra:

```text id="0r0w8r"
STATO
 │
 ▼
BUFFER
 │
 ▼
LOGICA APPLICATIVA
 │
 ▼
DECISIONI
 │
 ▼
COMUNICAZIONE
```

Un Engine applicativo non dovrebbe dipendere direttamente dal funzionamento interno di MQTT, WebAPI o Bridge.

Analogamente, un servizio di comunicazione non deve diventare il proprietario della logica applicativa.

Questo permette di mantenere separati:

- stato
- elaborazione
- decisione
- comunicazione

e rende l'intero sistema più prevedibile.

---

# 13. Diagnostica

La diagnostica è un componente architetturale, non un'aggiunta successiva.

DomoManager deve essere in grado di osservare il proprio comportamento.

La diagnostica può riguardare:

- Buffer
- task
- scheduler
- comunicazioni
- dispositivi
- Modbus
- RS485
- RTC
- automazioni
- sicurezza
- HVAC
- Power
- Weather
- watchdog
- aree monitorate
- risorse di rete

L'obiettivo è poter distinguere tra:

```text id="2wq3g8"
ERRORE LOGICO
ERRORE DISPOSITIVO
TIMEOUT
RETRY
TASK LENTO
COMUNICAZIONE LENTA
RISORSA ESAURITA
PROBLEMA DI SCHEDULING
```

Il principio è:

> **un sistema affidabile non deve soltanto funzionare: deve essere in grado di spiegare quando non funziona.**

---

# 14. Watchdog e protezione del runtime

Il watchdog rappresenta una linea di difesa del runtime.

Il suo compito è rilevare condizioni anomale quali:

- blocchi
- sovraccarichi
- mancata progressione del sistema
- condizioni di esecuzione anomale

Il watchdog non sostituisce la diagnostica.

La diagnostica cerca di spiegare il problema.

Il watchdog garantisce che una condizione grave non possa lasciare indefinitamente il sistema in uno stato non operativo.

---

# 15. Hot Standby

DomoManager può utilizzare un'architettura Master/Slave per aumentare la continuità operativa.

```text id="6g0s0x"
                 CLUSTER
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
       MASTER               SLAVE
       ACTIVE                READY
          │                   │
          └────── STATE ──────┘
```

Il nodo Master gestisce normalmente le comunicazioni operative.

Il nodo Slave rimane pronto al subentro.

La gestione del failover può comprendere:

- ruolo Master/Slave
- stato
- processo
- timestamp
- sincronizzazione
- attivazione delle comunicazioni

L'obiettivo è aumentare la continuità del servizio senza duplicare indiscriminatamente le attività.

---

# 16. Gestione delle risorse

DomoManager è progettato per hardware embedded.

Questo significa che le risorse devono essere considerate parte del modello architetturale.

In particolare:

- RAM
- CPU
- memoria
- socket
- connessioni
- bus
- banda
- tempi di esecuzione

non sono risorse infinite.

La progettazione del runtime deve quindi tenere conto della concorrenza tra servizi.

Il sistema deve evitare, dove possibile:

- allocazioni ricorrenti inutili
- retry incontrollati
- timeout eccessivi
- connessioni inutilmente simultanee
- operazioni bloccanti
- monopolizzazione del ciclo frontend

Il principio è:

> **ogni risorsa limitata deve poter essere controllata e, quando possibile, osservata.**

---

# 17. Un sistema che pensa

La forza di DomoManager non risiede nei singoli Engine.

Risiede nel modo in cui collaborano.

Consideriamo un esempio:

```text id="k9s0y1"
Weather
   │
   │ rileva pioggia
   ▼
Buffer
   │
   ├───────────────┐
   ▼               ▼
Security        Automation
   │               │
   │ finestre      │ decisione
   │ aperte        │
   └───────┬───────┘
           ▼
        Buffer
           │
           ▼
        HVAC / Power
           │
           ▼
 Communication Scheduler
           │
           ▼
       Attuatori
```

Il Weather Engine non deve conoscere direttamente l'HVAC.

Il Security Engine non deve comandare direttamente il Power Engine.

L'Automation Engine non deve conoscere il protocollo utilizzato dall'attuatore.

Il Buffer e il runtime forniscono il linguaggio comune.

Questa è la differenza tra un insieme di moduli e un sistema.

---

# 18. Un sistema che cresce

DomoManager è progettato per poter evolvere senza trasformare ogni nuova funzionalità in una modifica globale dell'architettura.

L'introduzione di un nuovo Engine dovrebbe richiedere principalmente:

1. definizione dello stato necessario
2. definizione delle relative aree nel Buffer
3. implementazione della logica
4. integrazione nel Task Engine quando necessario
5. integrazione con il Time Manager
6. integrazione con la diagnostica
7. eventuale integrazione con il Communication Scheduler

Il nuovo componente deve utilizzare le regole comuni del sistema.

La modularità non significa quindi che ogni modulo sia un'isola.

Significa che ogni modulo può evolvere mantenendo **lo stesso linguaggio architetturale**.

---

# 19. Il ciclo completo

Il funzionamento complessivo di DomoManager può essere sintetizzato in questo ciclo:

```text id="p3h7v9"
              MONDO FISICO
                   │
                   ▼
                BACKEND
                   │
                   ▼
                BUFFER
                   │
          ┌────────┼────────┐
          ▼        ▼        ▼
        TASKS   AUTOMATION  ENGINES
          │        │        │
          └────────┼────────┘
                   ▼
             DECISIONI
                   │
                   ▼
       COMMUNICATION SCHEDULER
                   │
          ┌────────┼────────┐
          ▼        ▼        ▼
       BRIDGE    MQTT     WebAPI
                   │
                   ▼
              ATTUATORI
                   │
                   ▼
              MONDO FISICO
```

Il ciclo non è semplicemente:

**input → output**.

È un ciclo continuo di:

**acquisizione → rappresentazione → elaborazione → decisione → azione → osservazione**.

Ed è proprio questa continuità che permette al sistema di comportarsi come un organismo.

---

# 20. La casa come organismo

La visione complessiva può essere descritta attraverso una metafora biologica:

> **La casa non è un insieme di oggetti intelligenti.  
> È un organismo.  
> DomoManager è il suo sistema nervoso.**

In questa metafora:

- i dispositivi sono gli organi
- il Backend è il sistema sensoriale
- il Buffer è il sistema circolatorio
- il Time Manager è il ritmo
- il Task Engine è il sistema di coordinamento
- l'Automation Engine rappresenta il comportamento
- gli Frontend Engines rappresentano gli organi specializzati
- il Communication Scheduler coordina il sistema nervoso periferico
- la diagnostica è il sistema immunitario
- il watchdog è il riflesso di sicurezza
- Hot Standby rappresenta la ridondanza

La metafora non è soltanto descrittiva.

Rappresenta il modello architetturale del progetto:

**ogni parte ha una responsabilità, ma nessuna parte rappresenta da sola l'intero organismo.**

---

# 21. Il principio architetturale finale

L'intera architettura DomoManager può essere riassunta in una sola frase:

> **Il mondo fisico produce dati, il Backend li acquisisce, il Buffer rappresenta lo stato, gli Engine lo interpretano, il runtime coordina l'esecuzione, l'Automation Engine prende decisioni e il Communication Layer porta tali decisioni verso il mondo esterno.**

Tutto questo deve avvenire secondo gli stessi principi:

- determinismo
- non-blocking
- modularità
- tracciabilità
- diagnostica
- local-first
- embedded-first
- resource-awareness

La complessità non deve essere eliminata.

Deve essere **governata dall'architettura**.

---

# ============================================================
# DOMOMANAGER
# PANORAMICA GENERALE DEL SISTEMA
# ============================================================