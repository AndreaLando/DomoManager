\# ============================================================

\# DOMOMANAGER – FILOSOFIA DEL PROGETTO

\# ============================================================



\## 1. Perché nasce DomoManager

DomoManager nasce da un’idea semplice ma radicale:

la domotica non deve essere un insieme di dispositivi che “parlano”,

ma un sistema coerente che “pensa”.



Per anni il settore ha prodotto soluzioni frammentate:

\- dispositivi con app proprietarie

\- protocolli incompatibili

\- logiche duplicate tra cloud, gateway e firmware

\- sistemi che si bloccano senza internet

\- automazioni non deterministiche



DomoManager è la risposta a tutto questo.



Non è un “hub”.

Non è un “bridge”.

Non è un “firmware”.



È un \*\*sistema operativo per la casa\*\*:

un ambiente che governa, coordina e interpreta l’intero ecosistema

domotico come un organismo unico.



L’obiettivo è creare un sistema:

\- deterministico

\- affidabile

\- comprensibile

\- estendibile

\- sicuro

\- indipendente dal cloud



Un sistema che non “fa domotica”, ma \*\*governa\*\* la casa.



\---



\## 2. La filosofia del design



\### 2.1 Determinismo prima di tutto

Ogni scelta architetturale parte da un principio:

\*\*il sistema deve comportarsi sempre nello stesso modo\*\*,

indipendentemente dal carico, dalla rete o dal tempo di esecuzione.



Per questo:

\- nessuna allocazione dinamica ricorrente

\- nessuna callback non controllata

\- nessuna logica nascosta

\- nessuna dipendenza da servizi esterni

\- cicli di vita chiari e prevedibili



Il determinismo è ciò che distingue un giocattolo da un sistema affidabile.



\---



\### 2.2 Il buffer come “lingua franca”

In DomoManager tutto passa attraverso un concetto chiave:

\*\*il buffer è la verità\*\*.



Non esistono variabili sparse o stati duplicati.

Ogni informazione vive in un’unica struttura coerente:

sensori, attuatori, automazioni, MQTT, WebAPI, HVAC.



Questo garantisce:

\- tracciabilità totale

\- diagnostica immediata

\- automazioni più semplici

\- sincronizzazione perfetta con pannelli e frontend

\- assenza di stati fantasma



Il buffer è il cuore del sistema.



\---



\### 2.3 Modularità senza frammentazione

Ogni componente è indipendente, ma non isolato.



La filosofia è:

\*\*“Indipendenti nel funzionamento, coerenti nel linguaggio.”\*\*



HVAC, Power, Weather, Security, MQTT, WebAPI, RS485, HotStandby:

tutti parlano la stessa lingua, il buffer.



Nessuna complessità di integrazione.

Nessuna dipendenza incrociata.



\---



\### 2.4 Zero magia

DomoManager evita la “magia” software:

\- niente comportamenti impliciti

\- niente automazioni nascoste

\- niente logiche che si attivano da sole



Ogni comportamento è:

\- dichiarato

\- visibile

\- tracciabile

\- diagnosticabile



Se qualcosa accade, c’è un motivo chiaro.

Se qualcosa non accade, il sistema te lo dice.



\---



\### 2.5 L’utente come architetto, non come programmatore

Il sistema è pensato per chi progetta impianti, non per chi scrive codice.



Per questo:

\- automazioni dichiarative

\- scene leggibili

\- sequenze strutturate

\- condizioni esplicite

\- JSON umano, non criptico



L’obiettivo è permettere a un tecnico di costruire logiche complesse

senza diventare uno sviluppatore.



\---



\### 2.6 Robustezza industriale, flessibilità domestica

DomoManager eredita concetti tipici dei PLC:

\- watchdog

\- fallback multipli

\- hotstandby

\- diagnostica continua

\- protezioni hardware

\- gestione errori deterministica



E li combina con:

\- MQTT

\- WebAPI

\- automazioni dinamiche

\- HVAC avanzato

\- integrazione Home Assistant

\- sensori cablati e wireless



Solido come un PLC.

Flessibile come un framework moderno.



\---



\### 2.7 Il tempo come fondamento

Il TimeManager non è un accessorio:

è il metronomo dell’intero sistema.



Ogni modulo – HVAC, automazioni, ACS, sicurezza, power –

si sincronizza con lui.



Il tempo non è un valore:

\*\*è una dimensione del sistema.\*\*



\---



\### 2.8 La casa come ecosistema

La filosofia finale è questa:



> “La casa non è un insieme di oggetti intelligenti.

>  È un organismo.

>  DomoManager è il suo sistema nervoso.”



Ogni modulo è un organo.

Il buffer è il sangue.

Il TimeManager è il ritmo.

Le automazioni sono il comportamento.

La diagnostica è il sistema immunitario.



\---



\# ============================================================

\# FINE DELLA SEZIONE "FILOSOFIA DEL PROGETTO"

\# ============================================================



