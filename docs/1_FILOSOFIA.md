# ============================================================
# DOMOMANAGER – FILOSOFIA DEL PROGETTO
# ============================================================

## 1. Perché nasce DomoManager

DomoManager nasce da un'idea semplice ma radicale:

> **la domotica non deve essere un insieme di dispositivi che "parlano", ma un sistema coerente che "pensa".**

Per anni il settore della home automation ha prodotto soluzioni frammentate:

- dispositivi con applicazioni proprietarie
- protocolli incompatibili
- logiche duplicate tra cloud, gateway e firmware
- dipendenza da servizi esterni
- sistemi che degradano o perdono funzionalità senza Internet
- automazioni difficili da comprendere
- comportamenti non deterministici
- diagnostica insufficiente
- stato del sistema distribuito in più componenti

DomoManager nasce come risposta a questa frammentazione.

Non vuole essere semplicemente un:

- hub
- gateway
- bridge
- firmware
- supervisore

DomoManager è concepito come un **sistema operativo per la casa**:

un ambiente runtime che governa, coordina, sincronizza e interpreta l'intero ecosistema domestico come un sistema unico.

L'obiettivo è costruire un sistema:

- deterministico
- affidabile
- comprensibile
- estendibile
- sicuro
- diagnosticabile
- local-first
- indipendente dal cloud
- consapevole delle risorse hardware

Un sistema che non si limita a "fare domotica", ma **governa la casa**.

---

# 2. La filosofia del design

La progettazione di DomoManager segue alcuni principi fondamentali.

Non sono semplicemente linee guida software.

Sono i criteri attraverso i quali vengono valutate le decisioni architetturali.

---

## 2.1 Determinismo prima di tutto

Ogni scelta architetturale parte da un principio:

> **il sistema deve comportarsi in modo prevedibile.**

Il comportamento del sistema non dovrebbe dipendere in modo incontrollato da:

- carico momentaneo
- ordine casuale delle operazioni
- rete
- latenza
- servizi esterni
- allocazioni dinamiche ricorrenti
- callback non controllate
- timeout imprevedibili

Per questo DomoManager privilegia:

- cicli di esecuzione espliciti
- scheduling controllato
- intervalli definiti
- stati espliciti
- timeout deterministici
- retry controllati
- gestione strutturata degli errori
- lifecycle chiari
- riduzione delle allocazioni dinamiche ricorrenti

Il determinismo non significa che il sistema debba essere statico.

Significa che anche quando il sistema è dinamico, il suo comportamento deve rimanere **comprensibile e controllabile**.

> **La complessità può essere dinamica.  
> Il comportamento non deve essere imprevedibile.**

---

# 3. Il Buffer come verità del sistema

In DomoManager esiste un concetto fondamentale:

> **il Buffer è la verità.**

Lo stato del sistema non deve essere distribuito arbitrariamente tra moduli diversi.

Sensori, attuatori, automazioni, HVAC, sicurezza, Power, Weather, MQTT e WebAPI devono poter operare su una rappresentazione coerente dello stato.

Il modello fondamentale è:

```text
                 MONDO FISICO
                      │
                      ▼
                   BACKEND
                      │
                      ▼
              ┌───────────────┐
              │     BUFFER    │
              │  SYSTEM STATE │
              └───────┬───────┘
                      │
          ┌───────────┼───────────┐
          ▼           ▼           ▼
       TASKS      AUTOMATION   FRONTEND
          │           │           │
          └───────────┼───────────┘
                      ▼
               COMMUNICATION
```

Il Buffer fornisce:

- aree tipizzate
- stato centralizzato
- timestamp
- change tracking
- virtual areas
- reverse
- split
- toggle
- accesso rapido
- diagnostica

Questo permette di evitare, dove possibile:

- stati duplicati
- copie non sincronizzate
- variabili fantasma
- dipendenze nascoste
- sincronizzazioni manuali tra moduli

Il Buffer non è quindi una semplice struttura dati.

È il **linguaggio comune dell'intero sistema**.

---

# 4. Una sola lingua per tutti gli Engine

DomoManager è modulare, ma non frammentato.

La filosofia è:

> **"Indipendenti nel funzionamento, coerenti nel linguaggio."**

HVAC, Power, Weather, Security, Automation, MQTT, WebAPI, RS485 e HotStandby possono avere responsabilità completamente differenti.

Ma devono poter condividere lo stesso modello di stato.

Questo riduce drasticamente la complessità delle integrazioni.

Un Engine non dovrebbe avere bisogno di conoscere:

- come un altro Engine memorizza internamente i dati
- quale driver utilizza un dispositivo
- quale protocollo utilizza un sensore
- dove si trova fisicamente la logica che ha prodotto un valore

Deve conoscere ciò che gli serve attraverso l'architettura comune.

---

# 5. Modularità senza isolamento

La modularità non significa creare tanti sistemi separati.

Significa assegnare ad ogni componente una responsabilità precisa.

Per esempio:

- Device Manager gestisce i dispositivi
- Modbus gestisce la comunicazione industriale
- Buffer gestisce lo stato
- Automation Engine interpreta le regole
- HVAC gestisce la climatizzazione
- Security gestisce la sicurezza
- Power gestisce l'energia
- Weather gestisce i dati meteorologici
- MQTT gestisce l'integrazione MQTT
- WebAPI gestisce l'interfaccia HTTP
- Communication Scheduler decide quando eseguire i servizi di comunicazione
- Task Engine coordina l'esecuzione dei task

La modularità serve quindi a **ridurre la complessità**, non a spostarla da un modulo all'altro.

---

# 6. Zero magia

DomoManager evita deliberatamente la "magia" software.

Non dovrebbero esistere comportamenti che:

- si attivano senza una causa riconoscibile
- modificano lo stato senza essere tracciabili
- dipendono da condizioni nascoste
- richiedono conoscenze implicite del programmatore

Ogni comportamento significativo deve essere:

- dichiarato
- visibile
- prevedibile
- tracciabile
- diagnosticabile

La domanda fondamentale è sempre:

> **"Perché è successo?"**

E la risposta deve poter essere ricostruita dal sistema.

Allo stesso modo:

> **"Perché non è successo?"**

deve avere una risposta osservabile attraverso lo stato, gli eventi e la diagnostica.

---

# 7. Il Task Engine: il sistema deve sapere quando lavorare

Un sistema complesso non può affidarsi a un unico `loop()` nel quale ogni componente cerca di fare tutto.

DomoManager utilizza un modello di task orchestration.

I task sono definiti attraverso:

- responsabilità
- callback
- intervallo
- stato enabled/disabled

Il runtime può quindi distinguere chiaramente tra:

- logica applicativa
- scheduling
- comunicazione
- sincronizzazione del ciclo

Il Task Engine rappresenta quindi una sorta di **scheduler operativo del sistema**.

Non decide soltanto *cosa* fare.

Decide anche **quando è appropriato farlo**.

---

# 8. Non-blocking come principio architetturale

Il principio non-blocking è una conseguenza diretta della filosofia deterministica.

Un servizio secondario non deve poter compromettere l'intero sistema.

In particolare:

- MQTT
- WebAPI
- Bridge
- Modbus
- HMI
- RS485

non devono poter monopolizzare il runtime per un tempo indefinito.

Per questo la comunicazione viene separata dalla logica applicativa e gestita attraverso un **Communication Scheduler**.

Il modello è:

```text
             TASK ENGINE
                  │
                  ▼
       COMMUNICATION SCHEDULER
                  │
        ┌─────────┼─────────┐
        ▼         ▼         ▼
      BRIDGE     MQTT      WebAPI
```

I servizi possono essere eseguiti con:

- intervalli configurabili
- priorità
- stato enabled
- callback dedicate
- esecuzione distribuita nel tempo

L'obiettivo non è eliminare la comunicazione.

È impedire che la comunicazione possa diventare il punto di blocco dell'intero sistema.

> **Nessun servizio secondario deve poter bloccare il ciclo applicativo principale.**

---

# 9. Il tempo come fondamento

Il tempo non è un semplice valore utilizzato dai programmi.

È una dimensione fondamentale del sistema.

Il TimeManager rappresenta il riferimento temporale comune per:

- automazioni
- HVAC
- ACS
- sicurezza
- Power
- scheduler
- retry
- timeout
- eventi
- diagnostica

Il sistema deve sapere non soltanto:

> **"Qual è lo stato?"**

ma anche:

> **"Da quanto tempo?"**

e:

> **"Quando è successo?"**

Per questo timestamp, intervalli, timeout, cooldown e scheduling sono parte integrante del modello architetturale.

> **Il TimeManager è il metronomo del sistema.**

---

# 10. L'utente come architetto, non come programmatore

DomoManager è pensato per chi progetta e gestisce impianti.

Non deve essere necessario trasformare ogni esigenza funzionale in codice.

Per questo il sistema privilegia:

- automazioni dichiarative
- scene leggibili
- condizioni esplicite
- sequenze strutturate
- configurazione JSON
- validazione
- comportamenti osservabili

L'obiettivo è consentire a un tecnico di descrivere una logica complessa senza dover diventare necessariamente uno sviluppatore.

Il sistema deve essere sufficientemente potente per il programmatore, ma sufficientemente leggibile per il progettista.

---

# 11. Robustezza industriale, flessibilità domestica

DomoManager prende ispirazione dall'architettura dei sistemi industriali.

Da questo mondo eredita concetti come:

- watchdog
- timeout
- retry
- fallback
- gestione degli errori
- protezioni
- diagnostica continua
- stati espliciti
- HotStandby
- comportamento deterministico

Ma li combina con caratteristiche tipiche dei moderni sistemi software:

- MQTT
- WebAPI
- automazioni dinamiche
- HVAC avanzato
- integrazione Home Assistant
- dispositivi wireless quando utili
- configurazione dichiarativa

L'obiettivo non è trasformare una casa in una fabbrica.

È portare nella casa ciò che nell'industria è stato imparato sulla **continuità operativa**.

> **Robusto come un sistema industriale.  
> Flessibile come un framework moderno.**

---

# 12. Embedded-first

DomoManager nasce con una consapevolezza precisa:

> **l'hardware non è infinito.**

CPU, RAM, memoria, connessioni Ethernet, socket, bus e banda sono risorse reali.

Un'architettura progettata ignorando questi limiti può funzionare in laboratorio e fallire sul campo.

Per questo DomoManager considera le risorse hardware parte dell'architettura software.

Questo implica attenzione a:

- allocazioni dinamiche
- memoria
- CPU
- numero di connessioni
- socket Ethernet
- timeout
- retry
- frequenza dei task
- concorrenza
- watchdog

Il sistema deve sapere non soltanto cosa può fare.

Deve sapere **quanto costa farlo**.

---

# 13. Resource-awareness

La gestione delle risorse non deve essere una soluzione applicata dopo che qualcosa si rompe.

Deve essere parte della progettazione.

In particolare, i servizi di comunicazione possono competere per risorse limitate.

Per questo DomoManager tende a:

- distribuire le comunicazioni nel tempo
- evitare concorrenza inutile
- limitare retry incontrollati
- controllare timeout
- separare servizi applicativi e comunicativi
- monitorare le risorse disponibili
- rendere osservabile il comportamento del networking

Questo è particolarmente importante nelle piattaforme embedded con un numero limitato di socket e connessioni contemporanee.

La filosofia è:

> **una risorsa che può esaurirsi deve poter essere osservata, prevista e gestita.**

---

# 14. Diagnostica come parte del sistema

In molti sistemi la diagnostica viene aggiunta dopo.

In DomoManager la diagnostica è parte dell'architettura.

Un sistema realmente affidabile non deve soltanto funzionare.

Deve anche poter spiegare quando qualcosa non funziona.

Per questo devono essere osservabili:

- task
- scheduler
- comunicazioni
- Buffer
- dispositivi
- timeout
- retry
- errori
- watchdog
- sicurezza
- HVAC
- Power
- Weather
- RTC
- risorse di rete

La diagnostica deve permettere di distinguere, ad esempio:

```text
TASK LENTO
     │
     ├── CPU
     └── LOGICA

COMUNICAZIONE LENTA
     │
     ├── NETWORK
     ├── TIMEOUT
     └── RETRY

RISORSA ESAURITA
     │
     ├── SOCKET
     ├── MEMORY
     └── CONNECTION

ERRORE DISPOSITIVO
     │
     ├── MODBUS
     ├── RS485
     └── DEVICE
```

Il principio è:

> **Un errore non deve essere soltanto rilevato. Deve essere localizzabile.**

---

# 15. Local-first, non cloud-first

DomoManager nasce per funzionare localmente.

Le funzioni fondamentali della casa non devono dipendere da:

- Internet
- cloud
- server esterni
- account remoti
- servizi proprietari

Il cloud può essere utilizzato quando aggiunge valore.

Non deve però essere necessario per mantenere operativo l'impianto.

Per questo l'architettura privilegia:

- elaborazione locale
- Ethernet
- RS485
- Modbus
- protocolli deterministici
- automazioni locali
- stato locale

La filosofia è:

> **La casa deve continuare a funzionare anche quando Internet non funziona.**

---

# 16. Wired-first, wireless when useful

DomoManager non considera il wireless un nemico.

Considera però l'affidabilità dell'infrastruttura una priorità.

Per questo il sistema privilegia, quando possibile:

- cablaggio
- Ethernet
- RS485
- Modbus RTU
- Modbus TCP
- I/O locali

Le tecnologie wireless possono essere utilizzate dove sono convenienti.

Non devono però diventare necessariamente il fondamento delle funzioni critiche.

La distinzione è quindi:

> **Wireless quando è utile.  
> Wired quando è importante.**

---

# 17. La separazione tra stato, logica e comunicazione

Una delle conseguenze più importanti della filosofia DomoManager è la separazione tra tre concetti:

```text
             STATO
              │
              ▼
            BUFFER
              │
       ┌──────┴──────┐
       ▼             ▼
     LOGICA      AUTOMAZIONI
       │             │
       └──────┬──────┘
              ▼
        COMMUNICAZIONE
```

Il sistema deve poter continuare a ragionare sul proprio stato anche quando una comunicazione esterna è lenta o temporaneamente indisponibile.

Questo principio impedisce che:

- MQTT diventi la logica del sistema
- Home Assistant diventi il sistema
- WebAPI diventi il sistema
- il dispositivo fisico diventi direttamente la logica applicativa

La comunicazione è un **livello di integrazione**.

Non è il cuore del sistema.

---

# 18. Hot Standby e continuità operativa

La filosofia industriale porta naturalmente al concetto di ridondanza.

Quando richiesto, DomoManager può utilizzare un'architettura Master/Slave.

Il principio è semplice:

```text
             CLUSTER
                │
        ┌───────┴───────┐
        ▼               ▼
      MASTER           SLAVE
        │               │
     ACTIVE            READY
```

Il nodo attivo gestisce il normale funzionamento.

Il nodo secondario rimane pronto a subentrare.

La continuità operativa può coinvolgere:

- stato
- processo
- timestamp
- ruolo Master/Slave
- failover
- comunicazioni

La ridondanza non deve introdurre comportamenti imprevedibili.

Deve invece aumentare la prevedibilità del sistema in presenza di un guasto.

---

# 19. La casa come ecosistema

La filosofia finale di DomoManager può essere descritta attraverso una metafora biologica.

> **La casa non è un insieme di oggetti intelligenti.  
> È un organismo.  
> DomoManager è il suo sistema nervoso.**

In questo modello:

- i dispositivi sono gli organi
- il Buffer è il sistema circolatorio
- il TimeManager è il ritmo
- il Task Engine è il sistema di coordinamento
- le automazioni sono il comportamento
- la comunicazione è il sistema nervoso periferico
- la diagnostica è il sistema immunitario
- il watchdog è il riflesso di sicurezza
- HotStandby rappresenta la ridondanza dell'organismo

Ogni componente ha una funzione specifica.

Ma il valore nasce dalla loro **coerenza complessiva**.

---

# 20. Il principio fondamentale

Tutti i principi precedenti possono essere sintetizzati in una sola idea:

> **DomoManager non deve essere un insieme di funzionalità che convivono.  
> Deve essere un sistema nel quale ogni funzionalità segue le stesse regole architetturali.**

Determinismo.

Non-blocking.

Buffer-centric.

Modularità.

Diagnostica.

Local-first.

Embedded-first.

Resource-aware.

Questi non sono semplicemente obiettivi.

Sono i criteri con cui deve essere valutata l'evoluzione del progetto.

---

# 21. La regola per il futuro

Ogni nuova funzionalità di DomoManager dovrebbe poter rispondere positivamente a queste domande:

### È deterministica?

Il suo comportamento è prevedibile?

### È non-blocking?

Può rallentare o bloccare il resto del sistema?

### Usa il modello comune?

Condivide correttamente lo stato attraverso il Buffer?

### È diagnosticabile?

Possiamo capire cosa sta facendo e perché?

### È modulare?

Ha una responsabilità chiara?

### È resource-aware?

Tiene conto dei limiti dell'hardware?

### È local-first?

Può funzionare senza dipendenze cloud non necessarie?

### È comprensibile?

Un tecnico può capire cosa sta succedendo?

Se la risposta è no, la funzionalità non è ancora completamente integrata nella filosofia DomoManager.

---

# 22. Filosofia in una frase

> **DomoManager porta nella casa la disciplina dei sistemi industriali senza rinunciare alla flessibilità del software moderno.**

E lo fa attraverso una regola fondamentale:

> **la complessità deve essere gestita dal sistema, non scaricata sull'utente.**

---

# ============================================================
# DOMOMANAGER
# DETERMINISTIC. NON-BLOCKING. MODULAR. DIAGNOSABLE.
# EMBEDDED-FIRST. LOCAL-FIRST.
# ============================================================