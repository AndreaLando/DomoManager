# DomoManager - Panoramica generale del sistema

## 1. Introduzione

DomoManager non è un semplice firmware, un hub o un gateway. È un **runtime embedded per l'infrastruttura domestica**, progettato per governare la casa come un sistema unitario.

Il suo compito non è soltanto mettere in comunicazione dispositivi diversi. Il sistema deve essere in grado di:

- acquisire informazioni dal mondo fisico;
- rappresentare lo stato applicativo della casa;
- elaborare condizioni;
- prendere decisioni;
- comandare dispositivi e attuatori;
- coordinare automazioni;
- gestire climatizzazione, energia e sicurezza;
- integrare sistemi esterni;
- diagnosticare il proprio comportamento.

La filosofia fondamentale è:

> **La domotica non deve essere un insieme di dispositivi intelligenti, ma un sistema coerente che acquisisce, interpreta, decide e agisce.**

DomoManager è progettato per essere:

- deterministico;
- non-blocking;
- robusto;
- modulare;
- estendibile;
- leggibile;
- diagnosticabile;
- local-first;
- embedded-first;
- consapevole delle risorse hardware.

Ogni componente deve avere una responsabilità precisa. Ogni informazione condivisa deve possedere una rappresentazione coerente. Ogni decisione significativa deve poter essere ricostruita.

## 2. Modello generale

L'architettura può essere descritta attraverso domini distinti ma coordinati:

```text
MONDO FISICO
     |
     v
+-------------------+
|      BACKEND      |
| Device / Modbus   |
| Local I/O / RS485 |
+---------+---------+
          |
          v
+-------------------+       +-------------------+
|      BUFFER       |       |   RUNTIME STORE   |
| Numeric app state |       | Typed app state   |
+---------+---------+       +---------+---------+
          |                           |
          +-------------+-------------+
                        |
              +---------+---------+
              |                   |
              v                   v
       RUNTIME / TASKS       AUTOMATION
              |                   |
              +---------+---------+
                        |
                        v
             FRONTEND / INTEGRATION
                        |
          +-------------+-------------+
          |             |             |
          v             v             v
        MQTT         WEB API        BRIDGE
```

Attorno a questi domini operano servizi trasversali:

- TimeManager;
- diagnostica;
- watchdog;
- gestione delle risorse;
- networking;
- Hot Standby, quando abilitato.

Il flusso principale può essere sintetizzato come:

> **Acquisizione → stato condiviso → elaborazione → decisione → integrazione → azione.**

Non tutti i dati percorrono necessariamente la stessa pipeline. I diversi engine mantengono lifecycle e stato tecnico propri, ma condividono contratti, regole di accesso e modelli di osservabilità comuni.

## 3. Mondo fisico

Il primo livello è la realtà fisica dell'impianto:

- sensori;
- ingressi digitali e analogici;
- attuatori;
- pompe e valvole;
- fan coil;
- carichi elettrici;
- dispositivi Modbus;
- dispositivi RS485;
- dispositivi Ethernet;
- RTC;
- dispositivi wireless, quando utili.

Il backend acquisisce informazioni da questo livello e traduce le decisioni del sistema in operazioni verso l'hardware.

La configurazione dell'hardware deve avere un owner esplicito. L'inizializzazione di pin, bus, expander e periferiche non deve essere duplicata tra entry point, runtime e driver.

## 4. Backend

Il backend collega il mondo fisico al modello software della casa.

Comprende principalmente:

- Device Manager;
- motore Modbus;
- I/O locali;
- protocolli RS485;
- gestione dei dispositivi;
- rilevazione degli errori;
- retry e timeout;
- priorità e pacing;
- cooldown ed esclusione temporanea;
- coordinamento delle risorse di rete.

La sua responsabilità è:

> **Acquisire, validare e pubblicare lo stato del mondo fisico senza incorporare la logica applicativa della casa.**

Il driver conosce il dispositivo e il protocollo. Non deve decidere perché una funzione domestica debba essere attivata.

## 5. Modello dello stato condiviso

### 5.1 Buffer

Il Buffer è la fonte autorevole dello **stato applicativo numerico condiviso**.

Può rappresentare:

- temperature;
- stati e comandi;
- allarmi;
- potenze e misure elettriche;
- valori analogici;
- bitmask;
- informazioni di sicurezza;
- dati HVAC, Weather e Power;
- aree virtuali;
- risultati e stati delle automazioni.

Il Buffer fornisce inoltre:

- accesso indicizzato;
- timestamp;
- change tracking;
- aree virtuali;
- reverse;
- split;
- toggle;
- diagnostica delle inizializzazioni e delle modifiche.

> **Il Buffer è il linguaggio comune dello stato numerico condiviso, non il contenitore di ogni stato tecnico interno.**

### 5.2 RuntimeStore

Il RuntimeStore completa il Buffer per i dati non rappresentabili correttamente come `area → long`.

Supporta valori tipizzati come:

- booleani;
- interi a 32 bit;
- floating point;
- stringhe;
- documenti JSON opachi.

Il modello è:

```text
BUFFER
  area -> valore numerico

RUNTIME STORE
  handle -> valore tipizzato
```

Il RuntimeStore è adatto, per esempio, a identificativi, codici, payload testuali, informazioni prodotte da servizi esterni e proprietà pubblicabili tramite introspezione.

Non sostituisce il Buffer per I/O, route, toggle, split o variabili numeriche realtime ad alta frequenza.

### 5.3 Stato applicativo e stato tecnico

Gli engine possono mantenere stato tecnico interno, tra cui:

- timer e debounce;
- macchine a stati;
- connessioni;
- richieste pendenti;
- code;
- cursori di polling;
- retry e cooldown;
- snapshot di protocollo;
- contatori diagnostici.

La regola è:

- lo **stato applicativo condiviso** deve essere esposto tramite Buffer o RuntimeStore;
- lo **stato tecnico interno** resta incapsulato nel componente proprietario, ma deve essere diagnosticabile quando influenza l'operatività.

## 6. Eventi e propagazione delle modifiche

Il runtime utilizza un modello a eventi per disaccoppiare produzione e consumo delle modifiche.

Le sorgenti possono includere:

- Modbus;
- I/O fisici;
- route;
- toggle;
- split;
- automazioni;
- MQTT;
- comandi interni.

Le modifiche vengono pubblicate e processate nel normale ciclo del runtime, evitando catene di callback sincrone non controllate.

Questo modello consente di:

- identificare l'origine di una modifica;
- limitare il lavoro eseguito in ogni ciclo;
- separare acquisizione, trasformazione e consumo;
- ridurre ricorsione e accoppiamento;
- alimentare diagnostica e watchdog.

## 7. TimeManager

Il tempo è una dimensione fondamentale dell'architettura.

Il TimeManager fornisce il riferimento temporale per:

- timestamp;
- scheduling;
- timeout;
- retry;
- cooldown;
- automazioni;
- HVAC e ACS;
- sicurezza;
- Power;
- eventi;
- diagnostica.

Il sistema deve conoscere:

- qual è lo stato;
- quando è stato rilevato;
- da quanto tempo persiste;
- quando è avvenuta l'ultima transizione.

> **Il TimeManager è il metronomo del runtime.**

Le fonti effettivamente disponibili dipendono dalla piattaforma e dalla configurazione. La panoramica non assume che RTC, NTP o sincronizzazione peer siano sempre attivi contemporaneamente.

## 8. Runtime e scheduling

DomoManager non utilizza un unico scheduler uniforme per ogni sottosistema.

Il coordinamento avviene, secondo il componente, tramite:

- task pesati e rotazione;
- intervalli temporali;
- callback;
- condizioni di abilitazione;
- prerequisiti di ciclo;
- pacing;
- macchine a stati;
- disponibilità delle risorse.

Il runtime coordina:

- acquisizione degli I/O locali;
- ciclo Modbus;
- elaborazione degli eventi;
- split;
- automazioni;
- activity loop;
- scheduler asincrono;
- watchdog;
- aggiornamento diagnostico.

L'obiettivo è garantire progressione, separazione delle responsabilità e prevedibilità del ciclo.

## 9. Automation Engine

L'Automation Engine rappresenta il livello comportamentale del sistema.

```text
STATO + CONDIZIONI + TEMPO
            |
            v
        DECISIONE
            |
            v
          AZIONE
```

Supporta concetti come:

- scene;
- regole a soglia;
- trend;
- bitmask;
- condizioni multiple;
- regole temporali;
- sequenze;
- regole composite;
- definizione JSON;
- validazione.

L'Automation Engine opera sul modello di stato e non sui dettagli del protocollo fisico.

Questo separa:

- **come funziona il dispositivo**;
- **perché il sistema decide di utilizzarlo**.

## 10. Engine applicativi

Gli engine applicativi implementano domini funzionali specifici. Non tutti sono necessariamente abilitati in ogni configurazione.

### 10.1 HVAC

Il dominio HVAC comprende:

- pompe di calore;
- zone;
- setpoint;
- fan coil;
- circolazione;
- ACS;
- sbrinamento;
- valvola a tre vie;
- ciclo antilegionella;
- protezioni legate alle aperture.

### 10.2 Power e Power Supervisor

Il dominio Power comprende gestione e supervisione energetica, tra cui:

- carichi;
- limiti di potenza;
- priorità;
- suggerimenti di attacco e distacco;
- supervisione della tensione principale;
- ingressi di fault e alimentazione;
- funzionamento a batteria;
- soglie con isteresi;
- allarmi e diagnostica.

Le funzioni disponibili dipendono dalla configurazione concreta e non devono essere confuse con funzionalità pianificate ma non attive.

### 10.3 Weather

Il dominio Weather gestisce:

- temperatura;
- vento;
- pioggia;
- luminosità;
- medie mobili;
- transizioni giorno/notte;
- eventi meteorologici;
- allarmi con debounce.

### 10.4 Security e Intrusion

Il dominio Security comprende:

- sensori cablati;
- canali di allarme;
- zone;
- aggregazione degli stati;
- bitmask;
- tamper e trouble;
- partizioni;
- bypass;
- eventi e audit trail;
- diagnostica delle centrali.

### 10.5 AEE e Bridge

Il dominio AEE gestisce variabili sincronizzabili, direzioni di trasferimento, schema e applicazione degli aggiornamenti.

Il Bridge coordina:

- disponibilità del peer;
- ACK e ritrasmissione;
- allineamento;
- snapshot iniziale;
- trasferimento dello schema;
- comunicazione multiframe.

### 10.6 Access Control e Web API

Il controllo accessi coordina:

- lettori;
- normalizzazione dei codici;
- autorizzazione;
- relay associati;
- stato runtime correlato.

Le Web API forniscono client e server HTTP non bloccanti, profili di dispositivo, code limitate, parsing delle risposte e webhook.

### 10.7 Solar Inverter

Il sottosistema Solar supporta trasporti UART e TCP, discovery dei comandi, polling asincrono, pacing, snapshot strutturati e diagnostica degli inverter configurati.

## 11. Frontend, introspezione e semantica

Il frontend non è soltanto un trasporto. Coordina la presentazione e l'integrazione dello stato verso sistemi esterni.

La catena concettuale è:

```text
BUFFER / RUNTIME STORE
          |
          v
    INTROSPECTION
          |
          v
  SEMANTIC RESOLVER
          |
          v
 FRONTEND INTEGRATIONS
```

L'introspezione costruisce un modello delle entity, delle relazioni e delle sorgenti. Il resolver semantico aggiunge classificazione, unità, device class, state class, accesso e gruppi funzionali.

Questo modello può essere utilizzato dai frontend, incluso Home Assistant, senza duplicare manualmente tutta la configurazione.

## 12. MQTT e Home Assistant

Il layer MQTT normalizza i messaggi inbound e supporta backend differenti.

Può gestire:

- mapping numerici;
- mapping testuali;
- messaggi raw;
- Zigbee2MQTT;
- comandi Home Assistant;
- comandi destinati al Buffer;
- comandi destinati al RuntimeStore.

L'integrazione Home Assistant comprende:

- discovery;
- availability;
- sottoscrizione dei comandi;
- pubblicazione degli stati;
- snapshot iniziale;
- entity derivate dall'introspezione.

MQTT e Home Assistant restano livelli di integrazione. Non diventano proprietari della logica applicativa.

## 13. Communication Layer

DomoManager può comunicare attraverso:

- MQTT;
- HTTP;
- Modbus TCP;
- Modbus RTU;
- RS485;
- UDP;
- Bridge AEE;
- trasporti dedicati per inverter.

La comunicazione è coordinata da engine, orchestratori, pacer, scheduler e macchine a stati specifiche. Non esiste necessariamente un singolo scheduler universale attraversato da ogni protocollo.

```text
APPLICATION STATE
       |
       v
ORCHESTRATORS / ENGINES
       |
 +-----+------+--------+--------+
 |            |        |        |
 v            v        v        v
MQTT        WEB API  MODBUS   BRIDGE
```

Il principio fondamentale è:

> **La casa deve continuare a elaborare il proprio stato anche quando un'integrazione esterna è lenta o temporaneamente indisponibile.**

## 14. Networking e gestione delle risorse

Sulle piattaforme embedded, CPU, RAM, socket, bus, connessioni e banda sono risorse limitate.

La gestione della rete comprende:

- ownership dei protocolli;
- arbitraggio;
- pacing;
- stato operativo;
- timeout;
- retry;
- socket manager;
- connessioni persistenti quando utili;
- rilascio esplicito delle risorse.

Il runtime deve evitare, dove possibile:

- connessioni simultanee non necessarie;
- retry incontrollati;
- timeout eccessivi;
- monopolizzazione del ciclo;
- allocazioni ricorrenti evitabili;
- mancato rilascio di socket.

> **Ogni risorsa limitata deve avere un owner, un lifecycle e una diagnostica.**

## 15. Diagnostica

La diagnostica è una parte strutturale dell'architettura.

Può riguardare:

- Buffer e RuntimeStore;
- task e scheduler;
- comunicazioni;
- dispositivi;
- Modbus e RS485;
- RTC;
- automazioni;
- sicurezza;
- HVAC;
- Power e Weather;
- watchdog;
- aree monitorate;
- risorse di rete;
- introspezione e modello semantico.

Deve permettere di distinguere tra:

```text
ERRORE LOGICO
ERRORE DISPOSITIVO
TIMEOUT O RETRY
TASK LENTO
COMUNICAZIONE LENTA
RISORSA ESAURITA
MANCATA PROGRESSIONE
CONFIGURAZIONE INCOERENTE
```

> **Un sistema affidabile non deve soltanto funzionare. Deve poter spiegare quando, dove e perché non funziona.**

## 16. Watchdog e protezione del runtime

Il watchdog rileva condizioni anomale come:

- blocco;
- sovraccarico;
- instabilità;
- inattività;
- tempi di esecuzione anomali.

Il watchdog non sostituisce la diagnostica:

- la diagnostica spiega il problema;
- il watchdog protegge il sistema quando la progressione non è più affidabile.

Le reazioni devono essere proporzionate alla gravità e osservabili nei log.

## 17. Hot Standby

DomoManager dispone di meccanismi di ridondanza orientati alla continuità operativa.

```text
        CLUSTER
           |
     +-----+-----+
     |           |
     v           v
   MASTER       SLAVE
   ACTIVE       READY
```

La sincronizzazione può comprendere:

- heartbeat;
- ruolo master/slave;
- failover e failback;
- stato;
- dati di processo;
- timestamp.

L'implementazione effettiva dipende dalla piattaforma e dal sottosistema utilizzato. La ridondanza deve evitare l'esecuzione concorrente non controllata delle funzioni operative.

## 18. Esempio di cooperazione tra domini

Un evento meteorologico può attraversare più livelli senza creare dipendenze dirette tra engine:

```text
WEATHER
  | rileva pioggia
  v
SHARED STATE / EVENT
  |
  +------------+
  |            |
  v            v
SECURITY    AUTOMATION
  |            |
  +------+-----+
         |
         v
      DECISIONE
         |
         v
      ATTUATORE
```

Weather non deve conoscere il protocollo dell'attuatore. Security non deve incorporare la logica HVAC. Automation non deve controllare direttamente il trasporto Modbus o MQTT.

Lo stato condiviso, gli eventi e il runtime forniscono il linguaggio comune.

## 19. Crescita del sistema

L'introduzione di un nuovo engine dovrebbe richiedere principalmente:

- definizione della responsabilità e dell'owner;
- definizione dello stato applicativo condiviso;
- scelta tra Buffer e RuntimeStore;
- implementazione dello stato tecnico interno;
- integrazione con eventi e runtime;
- integrazione temporale;
- integrazione diagnostica;
- eventuale integrazione frontend o di comunicazione;
- validazione delle risorse richieste.

Un nuovo componente non deve introdurre un modello parallelo incompatibile con il resto del sistema.

La modularità non crea isole. Permette ai moduli di evolvere mantenendo lo stesso linguaggio architetturale.

## 20. Ciclo completo

Il funzionamento complessivo può essere sintetizzato così:

```text
MONDO FISICO
     |
     v
ACQUISIZIONE
     |
     v
BUFFER / RUNTIME STORE
     |
 +---+-------------+
 |                 |
 v                 v
LOGIC          AUTOMATION
 |                 |
 +--------+--------+
          |
          v
       DECISIONE
          |
          v
ORCHESTRATOR / PROTOCOL
          |
          v
       ATTUATORE
          |
          v
    MONDO FISICO
```

Il ciclo non è un semplice `input → output`, ma una sequenza continua di:

> **Acquisizione → rappresentazione → elaborazione → decisione → azione → osservazione.**

## 21. La casa come organismo

DomoManager può essere descritto attraverso due metafore complementari:

- è il **sistema operativo della casa** sul piano architetturale;
- è il suo **sistema nervoso** sul piano funzionale.

In questa rappresentazione:

- i dispositivi sono gli organi;
- il backend è il sistema sensoriale;
- Buffer e RuntimeStore costituiscono la circolazione dello stato;
- il TimeManager scandisce il ritmo;
- runtime e scheduler coordinano le attività;
- Automation produce il comportamento;
- gli engine sono funzioni specializzate;
- il Communication Layer costituisce il sistema nervoso periferico;
- la diagnostica svolge una funzione simile al sistema immunitario;
- il watchdog rappresenta il riflesso di sicurezza;
- l'Hot Standby introduce ridondanza.

Ogni parte ha una responsabilità, ma nessuna parte rappresenta da sola l'intero sistema.

## 22. Principio architetturale finale

L'architettura può essere riassunta così:

> **Il mondo fisico produce dati. Il backend li acquisisce. Buffer e RuntimeStore rappresentano lo stato condiviso. Runtime, logica e automazioni lo elaborano. Gli orchestratori traducono le decisioni nei protocolli necessari. La diagnostica rende l'intero ciclo osservabile.**

Tutto deve rispettare gli stessi criteri:

- determinismo;
- non-blocking;
- modularità;
- ownership esplicita;
- stato coerente;
- tracciabilità;
- diagnostica;
- local-first;
- embedded-first;
- resource awareness.

La complessità non deve essere eliminata. Deve essere **governata dall'architettura**.

---

**DOMOMANAGER**  
**PANORAMICA GENERALE DEL SISTEMA**  
**DETERMINISTIC. NON-BLOCKING. DIAGNOSABLE.**
