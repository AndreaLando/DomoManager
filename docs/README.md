# ============================================================
# VISIONE DEL PROGETTO DOMOMANAGER
# ============================================================


## 1. Perché creare un nuovo manager domotico?

La domanda è legittima: perché sviluppare un sistema come DomoManager
quando esistono già piattaforme diffuse come Home Assistant, Hubitat,
OpenHAB, Shelly Cloud, Tuya, Zigbee2MQTT e decine di ecosistemi RF?

La risposta è semplice:

**perché DomoManager nasce con un obiettivo differente.**

Non vuole essere soltanto una piattaforma di integrazione o una
raccolta di dispositivi intelligenti.

Nasce per costruire una base domotica:

- deterministica
- industriale
- cablata
- locale
- prevedibile
- silenziosa
- diagnosticabile
- indipendente dal cloud

La domotica moderna è diventata spesso un insieme di gadget:

- dispositivi che parlano principalmente via RF
- cloud obbligatori o fortemente integrati
- automazioni difficili da prevedere
- firmware chiusi
- ecosistemi frammentati
- comportamenti variabili sotto carico
- dipendenza dalla rete Wi-Fi

DomoManager nasce per ribaltare questo paradigma.


---

## 2. La nostra visione

DomoManager vuole riportare la domotica nel suo contesto naturale:

**impiantistica, affidabile, industriale, cablata e prevedibile.**

La casa non è un giocattolo.

Non è un insieme di lampadine smart.

È un sistema complesso che merita strumenti professionali.

La nostra visione si basa su alcuni principi fondamentali:

- prima il cavo, poi la radio
- determinismo prima della complessità
- locale prima del cloud
- buffer come fonte unica di verità
- separazione tra logica applicativa e comunicazione
- esecuzione non bloccante
- diagnostica come parte dell'architettura
- risorse hardware considerate parte del progetto software


---

## 3. Pilastro 1 — Minimizzare le emissioni RF

La casa moderna è invasa da:

- Zigbee
- Wi-Fi
- Bluetooth
- Thread
- Sub-GHz proprietari
- mesh
- repeater
- bridge wireless

Ogni dispositivo wireless aggiunge una componente di complessità:

- interferenze
- latenza
- dipendenza dalla qualità del segnale
- gestione delle batterie
- failure point aggiuntivi
- maggiore difficoltà diagnostica

DomoManager segue un principio opposto:

**prima il cavo, poi il resto.**

Preferiamo:

- Modbus cablato
- RS485
- ingressi digitali
- ingressi analogici
- sensori a filo
- attuatori a relè
- Ethernet

La radio può essere utilizzata quando è realmente utile,
ma non deve essere necessariamente la fondazione dell'impianto.


---

## 4. Pilastro 2 — Portare la domotica verso un modello industriale

DomoManager eredita concetti tipici dei sistemi industriali e dei PLC:

- cicli deterministici
- watchdog
- gestione degli errori
- fallback
- failover
- Hot Standby
- diagnostica
- controllo delle risorse
- separazione delle responsabilità
- gestione esplicita dello stato

Il sistema utilizza un modello nel quale:

**il comportamento applicativo non deve dipendere dal comportamento
di un singolo servizio di comunicazione.**

Un problema di MQTT, WebAPI o Bridge non deve trasformarsi
automaticamente in un blocco del ciclo applicativo.

Questo porta a un principio fondamentale:

> **Nessun servizio secondario deve poter bloccare il ciclo principale
> dell'applicazione.**

La casa deve continuare a funzionare anche quando:

- manca internet
- il Wi-Fi non è disponibile
- un servizio cloud è offline
- un'integrazione esterna non risponde
- una comunicazione presenta errori


---

## 5. Pilastro 3 — Buffer come unica fonte di verità

Il cuore dell'architettura è il **Buffer Engine**.

Il Buffer rappresenta lo stato corrente del sistema.

Non devono esistere copie arbitrarie dello stesso stato sparse
nei vari moduli.

Il modello è:

```text
DEVICE
   │
   ▼
BACKEND
   │
   ▼
BUFFER
   │
   ├──────────────┐
   ▼              ▼
APPLICATION    AUTOMATION
   │              │
   └──────┬───────┘
          ▼
       BUFFER
          │
          ▼
      OUTPUT
```

Il Buffer contiene:

- aree tipizzate
- timestamp
- change tracking
- virtual areas
- reverse
- split
- toggle
- eventi interni

Il Buffer è quindi:

**Single Source of Truth.**

Questo permette a:

- Backend
- Task Engine
- Automation Engine
- HVAC
- Security
- Power
- Weather
- MQTT
- WebAPI
- Bridge

di lavorare su una rappresentazione coerente del sistema.


---

## 6. Pilastro 4 — Separare applicazione e comunicazione

Uno dei principi più importanti della nuova architettura è la
separazione tra:

**Application Runtime**

e

**Communication Runtime.**

Il Task Engine gestisce i task applicativi:

- Security / Sensors
- HVAC
- Averages
- Power
- Weather
- altre funzioni applicative

La comunicazione viene invece gestita dal:

**Communication Scheduler.**

Il modello è:

```text
                    TASK ENGINE
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
           HVAC       SECURITY    AVERAGES
             │           │           │
             └───────────┼───────────┘
                         │
                         ▼
                      BUFFER
                         │
                         ▼
               COMMUNICATION TASK
                         │
                         ▼
              COMMUNICATION SCHEDULER
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
          BRIDGE        MQTT       WEBAPI
```

Questo permette di impedire che una comunicazione secondaria
possa monopolizzare il ciclo applicativo.


---

## 7. Pilastro 5 — Scheduling deterministico

Il Task Engine registra ed esegue i task secondo:

- intervallo
- stato enabled / disabled
- ordine controllato
- ciclo di esecuzione

La comunicazione utilizza invece un:

**Communication Scheduler a timed round-robin.**

Ogni servizio può avere:

- interval
- priority
- enabled / disabled
- callback dedicata

Il modello impedisce che un singolo servizio di comunicazione
possa occupare indefinitamente il runtime.

La comunicazione viene quindi trattata come una risorsa schedulata,
non come una funzione privilegiata rispetto all'applicazione.


---

## 8. Pilastro 6 — Non-blocking by design

La filosofia di DomoManager non considera il non-blocking
un'ottimizzazione opzionale.

È un requisito architetturale.

Il principio è:

> **La comunicazione deve avanzare attraverso piccoli passi
> deterministici, senza bloccare il ciclo principale.**

Questo vale per:

- Modbus
- RS485
- Bridge
- MQTT
- WebAPI
- gestione delle connessioni
- retry
- timeout
- cooldown

Il runtime deve continuare a eseguire le proprie responsabilità
anche quando una comunicazione è lenta o temporaneamente
non disponibile.


---

## 9. Pilastro 7 — Le risorse hardware fanno parte dell'architettura

In un sistema embedded le risorse non sono infinite.

Devono essere considerati esplicitamente:

- memoria
- CPU
- Ethernet
- socket TCP
- client simultanei
- connessioni MQTT
- connessioni Modbus
- timeout
- watchdog

Una risorsa hardware esaurita non deve diventare un comportamento
misterioso.

Deve poter essere:

1. rilevata
2. registrata
3. diagnosticata
4. gestita
5. resa visibile

Per questo il resource management e la diagnostica fanno parte
dell'architettura runtime.


---

## 10. Pilastro 8 — Ridurre la complessità, aumentare la leggibilità

DomoManager evita la cosiddetta "magia" software.

Il sistema privilegia:

- buffer centrale
- automazioni dichiarative
- condizioni esplicite
- scene leggibili
- sequenze strutturate
- JSON comprensibile
- diagnostica integrata
- flussi dati tracciabili

Ogni decisione deve essere spiegabile.

Ogni valore deve avere una posizione precisa.

Ogni errore deve poter essere ricondotto a una causa.


---

## 11. L'utente come architetto, non come programmatore

DomoManager è pensato per chi progetta impianti e automazioni,
non esclusivamente per chi sviluppa software.

Per questo privilegia:

- automazioni dichiarative
- scene
- regole
- condizioni
- sequenze
- configurazioni JSON
- diagnostica leggibile

L'obiettivo è permettere a un tecnico di costruire logiche
complesse senza dover trasformare ogni esigenza in codice.


---

## 12. Robustezza industriale, flessibilità domestica

DomoManager combina concetti industriali con strumenti moderni.

### Industrial

- watchdog
- fallback
- error handling
- Hot Standby
- failover
- diagnostica
- cicli deterministici
- gestione delle risorse

### Moderno

- MQTT
- WebAPI
- Bridge
- Home Assistant
- automazioni dinamiche
- JSON
- integrazioni esterne

Il risultato desiderato è:

**solido come un sistema industriale, flessibile come un framework
moderno.**


---

## 13. Il tempo come fondamento

Il Time Manager non è un semplice servizio di data e ora.

È il riferimento temporale del sistema.

Gestisce:

- RTC
- NTP
- epoch
- sincronizzazione temporale
- callback periodiche
- secondi
- minuti
- ore
- giorni

Il tempo alimenta:

- automazioni
- HVAC
- Power
- Security
- Weather
- scheduled rules

Il tempo non è semplicemente un valore.

**È una dimensione del sistema.**


---

## 14. La diagnostica come sistema immunitario

Un sistema affidabile non deve limitarsi a funzionare.

Deve anche spiegare quando qualcosa non funziona.

La diagnostica osserva:

- Buffer
- Backend
- Frontend
- Task Engine
- Communication Scheduler
- dispositivi
- Modbus
- comunicazioni
- Ethernet
- socket
- RTC
- Security
- HVAC
- Power
- Hot Standby
- watchdog
- overload

L'obiettivo non è soltanto:

**"cosa non funziona?"**

ma:

**"perché non funziona?"**

La diagnostica è quindi parte integrante del runtime.


---

## 15. Hot Standby

La continuità operativa può essere estesa attraverso Hot Standby.

Il sistema distingue:

- MASTER
- SLAVE

Il MASTER gestisce il normale runtime operativo e le comunicazioni.

Il comportamento del nodo SLAVE viene mantenuto separato per
evitare conflitti sulle risorse di rete.

Sono quindi parte del modello:

- heartbeat
- failover
- sincronizzazione
- state replication
- process replication
- gestione Ethernet
- cambio di ruolo

Il passaggio MASTER / SLAVE è parte del modello di affidabilità
del sistema.


---

## 16. La casa come ecosistema

La casa non è un insieme di oggetti intelligenti.

È un organismo.

Il modello concettuale è:

```text
DEVICE       → organi
BUFFER       → memoria / stato centrale
TIME MANAGER → ritmo
TASK ENGINE  → attività del sistema
AUTOMATION   → comportamento
COMMUNICATION→ sistema di comunicazione
DIAGNOSTICS  → sistema immunitario
HOT STANDBY  → continuità operativa
```

DomoManager è il sistema nervoso che coordina questi elementi.


# ============================================================
# 2. PANORAMICA GENERALE DEL SISTEMA

DomoManager è organizzato in livelli separati, con responsabilità esplicite e flussi osservabili.

```text
HARDWARE / RETE
      |
      v
BACKEND / DEVICE MANAGER / MODBUS / LOCAL I/O
      |
      v
BUFFER ENGINE
Single Source of Truth per le aree
      |
      +------------------+-------------------+
      |                  |                   |
      v                  v                   v
APPLICATION ENGINES   AUTOMATION       INTROSPECTION
Security / HVAC       Scene / Rule     Modello strutturale
Averages / Power      Sequence         delle entity
Solar / Weather            |                   |
      |                    v                   v
      +-----------------> BUFFER       SEMANTIC RESOLVER
                                               |
                                               v
                                      HOME ASSISTANT MODEL
                                               |
                                               v
COMMUNICATION TASK -> COMMUNICATION SCHEDULER -> MQTT / WEBAPI / BRIDGE
```

Principi invarianti:

- il Buffer è la fonte unica di verità per le aree;
- gli engine applicativi non dipendono dai protocolli di comunicazione;
- l'Introspection non copia la configurazione, ma costruisce viste e correlazioni;
- il resolver semantico arricchisce le entity senza modificare la configurazione originale;
- Home Assistant è una proiezione del modello interno;
- le variabili che non appartengono naturalmente al Buffer sono gestite dal RuntimeStore;
- comunicazione e applicazione avanzano separatamente e in modo non bloccante.

# 3. ARCHITETTURA TECNICA

## 3.1 Backend, Device Manager e Buffer

Il Backend acquisisce e attua lo stato tramite Device Manager, profili dispositivo, Modbus TCP/RTU e Local I/O. Gestisce inoltre routing, retry, timeout, cooldown ed errori.

Il Buffer Engine conserva lo stato delle aree e rimane il punto di convergenza per Backend, applicazione, automazioni e comunicazioni.

## 3.2 Application Engines

Il Task Engine esegue gli engine applicativi secondo intervalli e responsabilità separate:

- Security;
- HVAC;
- Averages;
- Power;
- Solar;
- Weather;
- altri servizi applicativi registrati.

Gli engine leggono dal Buffer e producono aggiornamenti attraverso gli eventi interni di DomoManager.

## 3.3 Automation Engine

L'Automation Engine supporta scene, regole statiche e dinamiche, sequenze, condizioni semplici, trend, debounce, bitmask e condizioni composite.

Le relazioni di automazione vengono esposte all'Introspection come input, output e stati tecnici.

## 3.4 DomoIntrospection

`DomoIntrospection` costruisce il modello strutturale senza duplicare la configurazione.

Indicizza:

- aree configurate;
- associazioni area-dispositivo;
- tipo fisico DI, DO, AI, AO, virtuale o sconosciuto;
- Route, Toggle e Split;
- input e output delle automazioni;
- binding AEE, MQTT e WebAPI;
- zone HVAC;
- gruppi Average;
- sorgenti Solar;
- tag RuntimeStore;
- visibilità e ruolo tecnico delle entity.

Ogni `Entity` contiene identità, origine, gruppo, tipo fisico, riferimenti alla configurazione originale, semantica, accesso, visibilità, relazioni, provenienza e scala.

### EntityOrigin

```text
AREA
DERIVED
SECURITY
HVAC
AVERAGE
SOLAR
POWER
AEE
RUNTIME
EXTERNAL_SOURCE
```

### EntityGroup

```text
ROOT             -> DomoManager
PHYSICAL_DEVICE  -> dispositivo fisico associato
SECURITY         -> Sistema antifurto
SOLAR            -> Inverter solare
```

### implementationOnly

Una entity `implementationOnly` rimane nel modello interno ma non viene pubblicata. Rientrano tipicamente target interni Route/Split, stati tecnici e dettagli implementativi.

## 3.5 DomoSemanticResolver

`DomoSemanticResolver` trasforma il modello strutturale in un modello semantico pubblicabile.

Determina:

- `EntityKind`;
- unità di misura;
- `deviceClass`;
- `stateClass`;
- accesso read/write;
- visibilità finale.

La risoluzione usa, in ordine, semantica specialistica, struttura fisica e relazioni, dizionario lessicale e fallback `UNKNOWN`.

```text
UNKNOWN
SENSOR
BINARY_SENSOR
SWITCH
LIGHT
COVER
CLIMATE
BUTTON
NUMBER
TEXT
STATUS
```

Il resolver non deve contenere ID `AREA_*` applicativi cablati.

## 3.6 Scaling

`Entity::scale` converte il valore Buffer nel valore pubblicato:

```text
valorePubblicato = valoreBuffer * entity.scale
```

Per i gruppi Average:

```text
entity.scale = 1 / group.outScale
```

Lo scaling viene determinato dalla sorgente che produce il valore, non dal nome dell'area.

## 3.7 DMRuntimeStore

`DMRuntimeStore` gestisce variabili che non appartengono necessariamente a un'area Buffer.

Supporta:

- handle e ID stabili;
- nome visualizzato;
- BOOL, INT32, FLOAT, STRING e JSON;
- READ_ONLY, WRITE_ONLY e READ_WRITE;
- persistenza e pubblicazione opzionale;
- metadati semantici;
- limiti, step e precisione;
- provenienza della scrittura.

Topic Home Assistant:

```text
<prefix>/runtime/<id>/state
<prefix>/runtime/<id>/set
```

L'identità pubblica è il Runtime ID, non il valore tecnico `area = -1`.

## 3.8 Security

Il sottosistema Security comprende:

```text
SecurityEngine
    +-- SecurityOrchestrator
    +-- WiredSensorsManager
    +-- SecurityHmiInterface
    +-- DomoManagerAlarmPanel
```

Gestisce sensori cablati, canali RT/H24, zone, armamento, eventi pannello, memorie, tacitazione, fault di comunicazione e bitmask di sensore, zona e sistema.

La sincronizzazione con il Buffer è read/write. Le bitmask devono essere codificate e decodificate da helper interni al sottosistema Security che condividono un'unica sequenza dei segnali. In questo modo l'aggiunta o la modifica di un campo non disallinea lettura e scrittura.

Home Assistant non deve duplicare la mappa dei bit con tabelle parallele.

```text
Security state
      |
      v
Security codec/helper read-write
      |
      v
Buffer status/command areas
      |
      v
Introspection / semantic projection
      |
      v
Home Assistant
```

## 3.9 Solar

L'integrazione Solar registra nell'Introspection le aree configurate e le marca come origine e gruppo `SOLAR`.

Le entity possono comprendere tensione, corrente e potenza fotovoltaica, tensione e percentuale batteria, temperatura, stato inverter e stato comunicazione.

In Home Assistant sono aggregate nel dispositivo logico `Inverter solare`.

## 3.10 Home Assistant

L'integrazione è suddivisa in tre responsabilità:

```text
DMHomeAssistant
    Discovery, topic, payload e Device Registry

DMHAOrchestrator
    Subscription, availability, snapshot, coda stati e comandi inbound

HAMQTTTransport
    Adattamento verso il client MQTT reale
```

### Discovery

Sono pubblicate solo entity non hidden, semanticamente supportate e abilitate dalla configurazione del dominio.

### Topic area

```text
<prefix>/area/<area>/state
<prefix>/area/<area>/set
```

### Topic RuntimeStore

```text
<prefix>/runtime/<id>/state
<prefix>/runtime/<id>/set
```

### Snapshot iniziale

Dopo connessione, subscription, availability e discovery:

1. le entity area leggibili vengono accodate dalla fotografia del Buffer;
2. le entity Runtime leggibili vengono pubblicate dal RuntimeStore;
3. gli aggiornamenti successivi vengono pubblicati incrementalmente.

### Device Registry

Le entity vengono aggregate sotto controller root, dispositivi fisici, Sistema antifurto e Inverter solare. I device secondari possono dichiarare il controller tramite `via_device`.

## 3.11 Communication Runtime

```text
COMMUNICATION TASK
        |
        v
COMMUNICATION SCHEDULER
        +-- MQTT
        +-- WebAPI
        +-- Bridge
```

Il Communication Scheduler rispetta intervallo, priorità, round-robin temporizzato, budget di rete e socket, retry e recovery controllati.

## 3.12 Diagnostica

La diagnostica osserva Buffer, device, profili, relazioni Introspection, entity unknown e hidden, gruppi semantici, scaling, automazioni, task, scheduler, comunicazioni, Security, HVAC, Solar, Power, Weather, RuntimeStore, watchdog e Hot Standby.

# 4. DIPENDENZE ARCHITETTURALI PRINCIPALI

```text
DomoManager.ino
|
+-- DMFrontend.hpp
|   +-- DMFrontendEngines.hpp
|   |   +-- SecurityEngine
|   |   +-- HVAC / Averages / Power / Solar / Weather
|   +-- DMIntrospection.hpp
|   +-- DMSemanticResolver.hpp
|   +-- DMRuntimeStore.hpp
|   +-- DMHomeAssistant.hpp
|   +-- DMHAOrchestrator.hpp
|   +-- HAMQTTTransport
|   +-- DMBridge.hpp
|   +-- WebAPI
|
+-- DomoManager Core
    +-- Device Manager
    +-- Buffer Engine
    +-- Modbus Engine
    +-- Local I/O
    +-- Automation Engine
    +-- Task Engine
    +-- Communication Scheduler
    +-- Time Manager
    +-- Diagnostics
```

Le dipendenze effettive devono essere verificate dagli `#include` del codice sorgente.

# 5. FLUSSO DATI COMPLETO

```text
MONDO FISICO
     |
     v
DEVICE / MODBUS / LOCAL I/O
     |
     v
BUFFER
     +-------------------+--------------------+
     |                   |                    |
     v                   v                    v
APPLICATION          AUTOMATION         INTROSPECTION
     |                   |                    |
     +---------> BUFFER <-+                    v
                                          SEMANTICS
                                              |
                                              v
                                      HOME ASSISTANT MODEL
                                              |
                                              v
                                      MQTT COMMUNICATION
```

Per i RuntimeTag:

```text
DMRuntimeStore -> DomoIntrospection -> DomoSemanticResolver
               -> DMHomeAssistant -> DMHAOrchestrator
```

# 6. CICLO DI ESECUZIONE

```text
LOOP
 +-- Backend cycle
 |     +-- Time
 |     +-- Device Manager
 |     +-- Modbus / Local I/O
 |     +-- Buffer update
 +-- Frontend / Task Engine
 |     +-- Security / HVAC / Averages / Power / Solar / Weather
 +-- Automation
 |     +-- Buffer update
 +-- Communication Task
       +-- Communication Scheduler
             +-- MQTT / WebAPI / Bridge
```

Introspection e Semantic Resolver costruiscono il modello. La pubblicazione runtime usa il modello già risolto.

# 7. REGOLE DI PROGETTAZIONE

## 7.1 Nessuna duplicazione della configurazione

Introspection conserva riferimenti agli oggetti originali. Le proiezioni esterne non devono introdurre mappe applicative duplicate.

## 7.2 Nessun ID applicativo cablato nel framework

I moduli generici non devono contenere `AREA_*` specifiche dell'installazione.

## 7.3 Read/write simmetrico

Quando uno stato è comandabile, codifica e decodifica devono condividere ordine e semantica.

## 7.4 Buffer-centric

Le aree Buffer rimangono la fonte di verità. RuntimeStore è usato per variabili che non appartengono naturalmente al Buffer.

## 7.5 Non-blocking

Una comunicazione lenta o guasta non deve bloccare Backend, Task Engine, Security o Automation.

## 7.6 Diagnostica esplicita

Unknown, hidden, scaling, origine, gruppo e relazioni devono essere osservabili nei report.

## 7.7 Stabilità degli identificatori

Gli ID Home Assistant e RuntimeStore devono rimanere stabili attraverso riavvii e refactor compatibili.

# 8. STATO DELL'INTEGRAZIONE HOME ASSISTANT

Il modello corrente supporta:

- Discovery MQTT automatica;
- availability;
- sensori e attuatori derivati semanticamente;
- Device Registry per controller, device fisici, Security e Solar;
- snapshot iniziale;
- aggiornamenti incrementali;
- comandi area;
- tag RuntimeStore read/write;
- feedback sullo state topic;
- scaling delle aree Average.

Le bitmask Security restano proprietà del sottosistema Security. La proiezione verso binary sensor o `alarm_control_panel` deve partire dal codec/helper read/write interno alla Security, non da configurazioni parallele nel layer Home Assistant.

# 9. PRINCIPI OPERATIVI

- determinismo;
- non-blocking;
- Buffer come fonte unica di verità;
- isolamento dei servizi;
- consapevolezza delle risorse;
- osservabilità;
- funzionamento locale;
- progettazione embedded-first;
- configurazione dichiarativa;
- separazione tra struttura, semantica e protocollo.

# 10. OBIETTIVO FINALE

DomoManager vuole offrire una base domotica affidabile come un PLC, leggibile, estendibile, locale, prevedibile sotto carico e diagnosticabile.

Home Assistant rappresenta il livello di supervisione e interazione. DomoManager conserva il controllo deterministico dell'impianto, dello stato e delle automazioni.

# DOMOMANAGER

# THE OPERATING SYSTEM FOR THE HOME
