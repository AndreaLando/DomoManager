# DomoManager - Filosofia del progetto

## 1. Perché nasce DomoManager

DomoManager nasce da un'idea semplice ma radicale:

> **La domotica non deve essere un insieme di dispositivi che parlano, ma un sistema coerente che pensa.**

Per anni il settore della home automation ha prodotto soluzioni frammentate:

- dispositivi legati ad applicazioni proprietarie;
- protocolli incompatibili;
- logiche duplicate tra cloud, gateway e firmware;
- dipendenza da servizi esterni;
- perdita o degradazione delle funzioni in assenza di Internet;
- automazioni difficili da comprendere;
- comportamenti non deterministici;
- diagnostica insufficiente;
- stato distribuito tra componenti non coordinati.

DomoManager nasce come risposta a questa frammentazione. Non vuole essere soltanto un hub, un gateway, un bridge, un firmware o un supervisore.

DomoManager è concepito come un **sistema operativo per la casa**: un ambiente runtime che governa, coordina, sincronizza e interpreta l'intero ecosistema domestico come un sistema unitario.

L'obiettivo è costruire un sistema:

- deterministico;
- affidabile;
- comprensibile;
- estendibile;
- sicuro;
- diagnosticabile;
- local-first;
- indipendente dal cloud per le funzioni essenziali;
- consapevole dei limiti e dei costi delle risorse hardware.

Un sistema che non si limita a fare domotica, ma **governa la casa**.

## 2. Principi di progettazione

La progettazione di DomoManager segue principi architetturali espliciti. Non sono semplici linee guida stilistiche, ma criteri con cui valutare ogni decisione tecnica e ogni nuova funzionalità.

### 2.1 Determinismo prima di tutto

Ogni scelta parte da un principio:

> **Il sistema deve comportarsi in modo prevedibile.**

Il comportamento non deve dipendere in modo incontrollato da:

- carico momentaneo;
- ordine casuale delle operazioni;
- condizioni transitorie della rete;
- latenze non delimitate;
- servizi esterni;
- allocazioni dinamiche ricorrenti;
- callback non governate;
- timeout imprevedibili.

Per questo DomoManager privilegia:

- cicli di esecuzione espliciti;
- scheduling controllato;
- intervalli definiti;
- stati osservabili;
- timeout deterministici;
- retry controllati;
- gestione strutturata degli errori;
- lifecycle chiari;
- riduzione delle allocazioni dinamiche nel runtime ordinario.

Il determinismo non implica rigidità. Un sistema può essere dinamico e, allo stesso tempo, mantenere un comportamento comprensibile, tracciabile e controllabile.

> **La complessità può essere dinamica. Il comportamento non deve essere imprevedibile.**

### 2.2 Responsabilità esplicite

Ogni componente deve possedere una responsabilità chiara e un perimetro riconoscibile. L'inizializzazione, la gestione dello stato, l'accesso all'hardware e la pubblicazione degli eventi devono avere un owner definito.

Una responsabilità non dovrebbe essere duplicata in più livelli, perché la duplicazione rende incerti:

- l'ordine di esecuzione;
- gli effetti collaterali;
- il punto corretto di modifica;
- il comportamento in caso di errore;
- la diagnostica.

### 2.3 Esplicitazione e tracciabilità, zero magia

DomoManager evita deliberatamente comportamenti impliciti o difficili da ricostruire.

Non dovrebbero esistere operazioni che:

- si attivano senza una causa riconoscibile;
- modificano lo stato senza lasciare una traccia;
- dipendono da condizioni nascoste;
- richiedono conoscenze non documentate del programmatore.

Ogni comportamento significativo deve essere:

- dichiarato;
- visibile;
- prevedibile;
- tracciabile;
- diagnosticabile.

Le domande fondamentali sono:

- **Perché è successo?**
- **Perché non è successo?**

Il sistema deve permettere di ricostruire entrambe le risposte attraverso stato, eventi, log e diagnostica.

## 3. Il modello di stato

### 3.1 Il Buffer come fonte autorevole dello stato condiviso

In DomoManager esiste un concetto fondamentale:

> **Il Buffer è la fonte autorevole dello stato applicativo condiviso.**

Lo stato che deve essere condiviso tra sensori, attuatori, automazioni, HVAC, sicurezza, Power, Weather, MQTT e Web API non deve essere distribuito arbitrariamente tra moduli indipendenti.

```text
MONDO FISICO
     |
     v
  BACKEND
     |
     v
+------------------+
|      BUFFER      |
| SHARED APP STATE |
+--------+---------+
         |
   +-----+-----+
   |     |     |
   v     v     v
 TASKS  LOGIC  FRONTEND
   |     |     |
   +-----+-----+
         |
         v
  COMMUNICATION
```

Il Buffer fornisce:

- aree numeriche;
- stato centralizzato;
- timestamp;
- change tracking;
- aree virtuali;
- reverse;
- split;
- toggle;
- accesso rapido;
- diagnostica.

Questo permette di ridurre:

- stati applicativi duplicati;
- copie non sincronizzate;
- variabili fantasma;
- dipendenze nascoste;
- sincronizzazioni manuali tra moduli.

Il Buffer non è una semplice struttura dati. È il **linguaggio comune dello stato numerico condiviso**.

### 3.2 Stato condiviso e stato tecnico interno

Non tutto lo stato deve necessariamente risiedere nel Buffer. Gli engine possono mantenere stato tecnico interno quando è parte della loro implementazione, per esempio:

- timer e debounce;
- code di richieste;
- richieste pendenti;
- cursori di polling;
- connessioni e retry;
- snapshot di protocollo;
- stato delle macchine a stati;
- cache e contatori diagnostici.

La distinzione è netta:

- **stato applicativo condiviso**: deve essere pubblicato nel modello comune;
- **stato tecnico interno**: rimane nell'engine che lo possiede, ma deve essere diagnosticabile quando rilevante.

### 3.3 RuntimeStore per lo stato tipizzato non rappresentabile nel Buffer

Il Buffer numerico non è adatto a tutti i valori. Stringhe, JSON, identificativi, codici e variabili tipizzate richiedono un modello complementare.

Il RuntimeStore estende il modello di stato senza sostituire il Buffer:

```text
BUFFER
  area numerica -> long

RUNTIME STORE
  handle -> valore tipizzato
```

Il principio rimane invariato: ogni dato condiviso deve avere un'origine, un tipo, regole di accesso e un lifecycle espliciti.

## 4. Un linguaggio comune per tutti gli engine

DomoManager è modulare, ma non frammentato.

> **Indipendenti nel funzionamento, coerenti nel linguaggio.**

HVAC, Power, Weather, Security, Automation, MQTT, Web API, RS485 e Hot Standby possono avere responsabilità differenti, ma devono integrarsi attraverso modelli comuni.

Un engine non dovrebbe conoscere:

- come un altro engine memorizza internamente i dati;
- quale driver concreto utilizza un dispositivo;
- quale protocollo trasporta un valore;
- dove si trova fisicamente la logica che lo ha prodotto.

Deve conoscere soltanto il contratto necessario per leggere, pubblicare o trasformare lo stato.

## 5. Modularità senza isolamento

La modularità non significa creare molti sistemi separati. Significa assegnare a ogni componente una responsabilità precisa.

In termini generali:

- **Device Manager** gestisce i dispositivi;
- **Modbus** gestisce la comunicazione industriale;
- **Buffer** gestisce lo stato numerico condiviso;
- **RuntimeStore** gestisce lo stato tipizzato complementare;
- **Automation Engine** interpreta le regole;
- **HVAC** gestisce la climatizzazione;
- **Security** gestisce la sicurezza;
- **Power** gestisce energia e supervisione elettrica;
- **Weather** gestisce i dati meteorologici;
- **MQTT** gestisce l'integrazione MQTT;
- **Web API** gestisce l'integrazione HTTP;
- **runtime e scheduler** coordinano l'esecuzione;
- **diagnostica** rende osservabile il sistema.

La modularità deve ridurre la complessità, non trasferirla da un modulo all'altro.

## 6. Orchestrazione del runtime

### 6.1 Il sistema deve sapere quando lavorare

Un sistema complesso non può affidarsi a un unico `loop()` in cui ogni componente tenta di eseguire tutto a ogni iterazione.

DomoManager utilizza un modello di orchestrazione nel quale task e servizi sono governati, secondo il sottosistema, tramite:

- callback;
- pesi e rotazione;
- intervalli;
- condizioni di abilitazione;
- stato runtime;
- prerequisiti di ciclo;
- disponibilità delle risorse.

Il runtime distingue tra:

- logica applicativa;
- scheduling;
- comunicazione;
- aggiornamento dello stato;
- sincronizzazione dei cicli;
- diagnostica.

L'orchestrazione non decide soltanto **cosa** eseguire. Decide anche **quando** e **in quali condizioni** è corretto farlo.

### 6.2 Non-blocking come principio architetturale

Il principio non-blocking è una conseguenza diretta della filosofia deterministica.

Un servizio secondario non deve compromettere l'intero sistema. In particolare, MQTT, Web API, Bridge, Modbus, HMI e RS485 non devono monopolizzare il runtime per un tempo indefinito.

La comunicazione viene quindi separata dalla logica applicativa e coordinata attraverso runtime, scheduler e macchine a stati dedicate.

```text
APPLICATION RUNTIME
        |
        v
COMMUNICATION COORDINATION
        |
  +-----+------+-------+
  |            |       |
  v            v       v
BRIDGE        MQTT   WEB API
```

I servizi possono utilizzare:

- intervalli configurabili;
- priorità;
- pacing;
- timeout;
- retry controllati;
- callback dedicate;
- esecuzione distribuita nel tempo;
- arbitraggio delle risorse di rete.

L'obiettivo non è eliminare la comunicazione, ma impedire che diventi il punto di blocco del sistema.

> **Nessun servizio secondario deve poter bloccare indefinitamente il ciclo applicativo principale.**

## 7. Il tempo come fondamento

Il tempo non è un semplice valore usato dai programmi. È una dimensione fondamentale del sistema.

Il TimeManager rappresenta il riferimento temporale comune per:

- automazioni;
- HVAC;
- ACS;
- sicurezza;
- Power;
- scheduler;
- retry;
- timeout;
- eventi;
- diagnostica.

Il sistema deve sapere non soltanto:

- **Qual è lo stato?**
- **Da quanto tempo è in questo stato?**
- **Quando è avvenuta la transizione?**

Timestamp, intervalli, timeout, cooldown e scheduling fanno quindi parte del modello architetturale.

> **Il TimeManager è il metronomo del sistema.**

## 8. Separazione tra stato, logica e comunicazione

Una conseguenza fondamentale della filosofia DomoManager è la separazione tra stato, logica e comunicazione.

```text
          STATE
            |
            v
   BUFFER / RUNTIME STORE
            |
      +-----+-----+
      |           |
      v           v
    LOGIC      AUTOMATION
      |           |
      +-----+-----+
            |
            v
      COMMUNICATION
```

Il sistema deve continuare a ragionare sul proprio stato anche quando una comunicazione esterna è lenta o temporaneamente indisponibile.

Questo impedisce che:

- MQTT diventi la logica del sistema;
- Home Assistant diventi il sistema;
- una Web API diventi il sistema;
- un dispositivo fisico incorpori direttamente tutta la logica applicativa.

La comunicazione è un **livello di integrazione**. Non è il cuore dell'architettura.

## 9. L'utente come architetto, non come programmatore

DomoManager è pensato per chi progetta e gestisce impianti. Non dovrebbe essere necessario trasformare ogni esigenza funzionale in codice imperativo.

Per questo il sistema privilegia:

- automazioni dichiarative;
- scene leggibili;
- condizioni esplicite;
- sequenze strutturate;
- configurazione JSON;
- validazione;
- comportamenti osservabili.

L'obiettivo è consentire a un tecnico di descrivere logiche complesse senza dover diventare necessariamente uno sviluppatore.

Il sistema deve essere sufficientemente potente per il programmatore e sufficientemente leggibile per il progettista.

## 10. Robustezza industriale, flessibilità domestica

DomoManager prende ispirazione dall'architettura dei sistemi industriali.

Da questo mondo eredita:

- watchdog;
- timeout;
- retry;
- fallback;
- gestione strutturata degli errori;
- protezioni;
- diagnostica continua;
- stati espliciti;
- Hot Standby;
- comportamento deterministico.

Li combina con caratteristiche tipiche dei framework moderni:

- MQTT;
- Web API;
- automazioni dinamiche;
- HVAC avanzato;
- integrazione con Home Assistant;
- dispositivi wireless quando utili;
- configurazione dichiarativa.

L'obiettivo non è trasformare una casa in una fabbrica, ma portare nell'ambiente domestico ciò che l'industria ha imparato sulla continuità operativa.

> **Robusto come un sistema industriale. Flessibile come un framework moderno.**

## 11. Embedded-first

DomoManager nasce con una consapevolezza precisa:

> **L'hardware non è infinito.**

CPU, RAM, memoria, connessioni Ethernet, socket, bus e banda sono risorse reali.

Un'architettura che ignora questi limiti può funzionare in laboratorio e fallire sul campo. DomoManager considera quindi le risorse hardware parte dell'architettura software.

Questo richiede attenzione a:

- allocazioni dinamiche;
- memoria;
- CPU;
- numero di connessioni;
- socket Ethernet;
- timeout;
- retry;
- frequenza dei task;
- concorrenza;
- watchdog.

Il sistema deve sapere non soltanto cosa può fare, ma anche **quanto costa farlo**.

## 12. Resource awareness

La gestione delle risorse non deve essere applicata soltanto dopo un guasto. Deve essere parte della progettazione.

I servizi di comunicazione possono competere per risorse limitate. Per questo DomoManager tende a:

- distribuire le comunicazioni nel tempo;
- evitare concorrenza inutile;
- limitare retry incontrollati;
- controllare i timeout;
- separare servizi applicativi e comunicativi;
- monitorare le risorse disponibili;
- rendere osservabile il comportamento del networking.

Questo è particolarmente importante sulle piattaforme embedded con un numero limitato di socket e connessioni contemporanee.

> **Una risorsa che può esaurirsi deve poter essere osservata, prevista e gestita.**

## 13. Diagnostica come parte del sistema

In molti sistemi la diagnostica viene aggiunta in seguito. In DomoManager è parte dell'architettura.

Un sistema affidabile non deve soltanto funzionare. Deve anche spiegare quando e perché qualcosa non funziona.

Devono essere osservabili:

- task e scheduler;
- comunicazioni;
- Buffer e RuntimeStore;
- dispositivi;
- timeout e retry;
- errori;
- watchdog;
- sicurezza;
- HVAC;
- Power;
- Weather;
- RTC;
- risorse di rete.

La diagnostica deve consentire di distinguere categorie diverse di problema:

```text
TASK LENTO
  +-- CPU
  +-- LOGICA

COMUNICAZIONE LENTA
  +-- NETWORK
  +-- TIMEOUT
  +-- RETRY

RISORSA ESAURITA
  +-- SOCKET
  +-- MEMORY
  +-- CONNECTION

ERRORE DISPOSITIVO
  +-- MODBUS
  +-- RS485
  +-- DEVICE
```

> **Un errore non deve essere soltanto rilevato. Deve essere localizzabile.**

## 14. Local-first, non cloud-first

DomoManager nasce per funzionare localmente.

Le funzioni fondamentali della casa non devono dipendere da:

- Internet;
- cloud;
- server esterni;
- account remoti;
- servizi proprietari.

Il cloud può essere utilizzato quando aggiunge valore, ma non deve essere necessario per mantenere operativo l'impianto.

L'architettura privilegia:

- elaborazione locale;
- Ethernet;
- RS485;
- Modbus;
- protocolli deterministici;
- automazioni locali;
- stato locale.

> **La casa deve continuare a funzionare anche quando Internet non funziona.**

## 15. Wired-first, wireless when useful

DomoManager non considera il wireless un nemico. Considera però l'affidabilità dell'infrastruttura una priorità.

Quando possibile, il sistema privilegia:

- cablaggio;
- Ethernet;
- RS485;
- Modbus RTU;
- Modbus TCP;
- I/O locali.

Le tecnologie wireless possono essere utilizzate dove risultano convenienti, ma non devono diventare necessariamente il fondamento delle funzioni critiche.

> **Wireless quando è utile. Wired quando è importante.**

## 16. Hot Standby e continuità operativa

La filosofia industriale porta naturalmente al concetto di ridondanza.

Quando richiesto, DomoManager può utilizzare un'architettura master/slave:

```text
        CLUSTER
           |
     +-----+-----+
     |           |
     v           v
   MASTER       SLAVE
   ACTIVE       READY
```

Il nodo attivo gestisce il normale funzionamento. Il nodo secondario rimane pronto a subentrare.

La continuità operativa può coinvolgere:

- stato;
- dati di processo;
- timestamp;
- ruolo master/slave;
- failover;
- comunicazioni.

La ridondanza non deve introdurre comportamenti imprevedibili. Deve aumentare la prevedibilità del sistema in presenza di guasti.

## 17. La casa come ecosistema

DomoManager può essere descritto attraverso due metafore complementari:

- sul piano architetturale è il **sistema operativo della casa**;
- sul piano funzionale è il suo **sistema nervoso**.

In questo modello:

- i dispositivi sono gli organi;
- Buffer e RuntimeStore rappresentano la circolazione dello stato;
- il TimeManager scandisce il ritmo;
- runtime e scheduler coordinano le attività;
- le automazioni producono il comportamento;
- la comunicazione costituisce il sistema nervoso periferico;
- la diagnostica svolge una funzione simile al sistema immunitario;
- il watchdog rappresenta il riflesso di sicurezza;
- l'Hot Standby introduce ridondanza operativa.

Ogni componente ha una funzione specifica. Il valore nasce dalla loro **coerenza complessiva**.

## 18. Il principio fondamentale

Tutti i principi precedenti possono essere sintetizzati in una sola idea:

> **DomoManager non deve essere un insieme di funzionalità che convivono. Deve essere un sistema nel quale ogni funzionalità segue le stesse regole architetturali.**

I criteri fondamentali sono:

- determinismo;
- non-blocking;
- stato condiviso coerente;
- modularità;
- ownership esplicita;
- diagnostica;
- local-first;
- embedded-first;
- resource awareness.

Questi non sono soltanto obiettivi. Sono i criteri con cui deve essere valutata l'evoluzione del progetto.

## 19. Regole per l'evoluzione futura

Ogni nuova funzionalità di DomoManager dovrebbe rispondere positivamente alle domande seguenti.

### È deterministica?

Il comportamento è prevedibile anche in presenza di errori, latenze e carico variabile?

### È non-blocking?

Può rallentare o bloccare indefinitamente il resto del sistema?

### Ha un owner chiaro?

È evidente quale componente possiede inizializzazione, stato, lifecycle ed effetti collaterali?

### Usa il modello comune?

Pubblica correttamente lo stato applicativo condiviso attraverso Buffer o RuntimeStore?

### Distingue lo stato applicativo da quello tecnico?

Lo stato interno rimane incapsulato e quello condiviso è esposto in modo coerente?

### È diagnosticabile?

È possibile capire cosa sta facendo, perché lo sta facendo e perché potrebbe non averlo fatto?

### È modulare?

Ha una responsabilità precisa e contratti limitati?

### È resource-aware?

Tiene conto dei limiti di CPU, RAM, socket, bus, banda e tempo di ciclo?

### È local-first?

Può operare senza dipendenze cloud non necessarie?

### È comprensibile?

Un tecnico può ricostruirne configurazione, stato e comportamento?

Se la risposta a una di queste domande è negativa, la funzionalità non è ancora completamente integrata nella filosofia DomoManager.

## 20. Filosofia in una frase

> **DomoManager porta nella casa la disciplina dei sistemi industriali senza rinunciare alla flessibilità del software moderno. La complessità deve essere gestita dal sistema, non scaricata sull'utente.**

---

**DOMOMANAGER**  
**DETERMINISTIC. NON-BLOCKING. MODULAR. DIAGNOSABLE.**  
**EMBEDDED-FIRST. LOCAL-FIRST.**
