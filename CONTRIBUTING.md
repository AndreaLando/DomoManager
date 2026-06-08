\# ============================================================

\# CONTRIBUTING – COME CONTRIBUIRE A DOMOMANAGER

\# ============================================================



Grazie per il tuo interesse nel contribuire a \*\*DomoManager\*\*.  

Questo progetto non è un semplice firmware, ma un ecosistema

domotico industriale, deterministico e buffer‑centrico.  

Per questo motivo, le linee guida di contribuzione sono più rigorose

rispetto ai progetti consumer.



Questo documento spiega:

\- come contribuire

\- come proporre modifiche

\- come scrivere codice coerente con la filosofia del progetto

\- come aprire issue e pull request

\- come mantenere la qualità del sistema



\---



\# 1. Filosofia delle contribuzioni

Prima di contribuire, è fondamentale comprendere la visione del

progetto:



\- \*\*Determinismo\*\*: nessuna allocazione dinamica ricorrente,

&#x20; nessun comportamento imprevedibile.

\- \*\*Buffer‑centrico\*\*: ogni dato vive nel buffer, non in variabili

&#x20; sparse.

\- \*\*Modularità rigorosa\*\*: ogni modulo ha responsabilità chiare.

\- \*\*Zero magia\*\*: nessuna logica nascosta, nessun side‑effect.

\- \*\*Robustezza industriale\*\*: fallback, watchdog, diagnostica.

\- \*\*RF minimizzata\*\*: preferenza per cablato (Modbus, RS485, I/O).

\- \*\*Leggibilità\*\*: codice chiaro, commentato, prevedibile.



Ogni contributo deve rispettare questi principi.



\---



\# 2. Come proporre modifiche

\## 2.1 Aprire una Issue

Prima di scrivere codice, apri una issue per:

\- proporre una nuova funzionalità

\- segnalare un bug

\- discutere un miglioramento

\- proporre un refactoring



Le issue devono includere:

\- descrizione chiara

\- motivazione

\- impatto previsto

\- eventuali alternative



\## 2.2 Discussione tecnica

Ogni proposta viene discussa per verificare:

\- coerenza con la filosofia del progetto

\- impatto sul determinismo

\- impatto sul buffer

\- impatto sulle performance

\- impatto sulla diagnostica



Solo dopo approvazione si procede alla PR.



\---



\# 3. Linee guida per il codice

\## 3.1 Stile generale

\- C++ moderno, ma senza abuso di astrazioni.

\- Niente `new`/`delete` nel ciclo.

\- Niente `std::vector`, `std::string`, `std::map` nel loop.

\- Preferire array statici e strutture POD.

\- Funzioni brevi, moduli piccoli, responsabilità chiare.



\## 3.2 Buffer Engine

\- Ogni nuovo dato deve essere un’area buffer.

\- Niente variabili globali duplicate.

\- Ogni scrittura deve essere tracciabile.

\- Ogni lettura deve essere idempotente.



\## 3.3 Automazioni

\- Le automazioni devono essere dichiarative.

\- Nessuna logica hardcoded nei motori frontend.

\- Ogni condizione deve essere leggibile e testabile.



\## 3.4 Frontend Engines

\- Nessun accesso diretto all’hardware.

\- Nessuna logica duplicata.

\- Ogni motore deve leggere dal buffer e scrivere nel buffer.



\## 3.5 Modbus / RS485

\- Nessun blocco.

\- Nessun delay.

\- Timeout e retry deterministici.

\- Gestione errori chiara e tracciata.



\## 3.6 Diagnostica

\- Ogni nuovo modulo deve avere diagnostica.

\- Ogni errore deve essere spiegato.

\- Nessun errore silenzioso.



\---



\# 4. Struttura delle Pull Request

Ogni PR deve contenere:



\### 4.1 Descrizione

\- cosa cambia

\- perché cambia

\- impatto sul sistema

\- eventuali rischi



\### 4.2 Checklist tecnica

\- \[ ] Nessuna allocazione dinamica nel loop

\- \[ ] Nessun comportamento non deterministico

\- \[ ] Buffer aggiornato correttamente

\- \[ ] Diagnostica aggiornata

\- \[ ] Documentazione aggiornata

\- \[ ] Test manuali eseguiti



\### 4.3 Revisione

Le PR vengono revisionate con attenzione a:

\- leggibilità

\- coerenza architetturale

\- impatto sulle performance

\- impatto sulla stabilità



\---



\# 5. Test e validazione

\## 5.1 Test manuali

Ogni contributo deve essere testato su:

\- Opta / Arduino

\- dispositivi Modbus reali o simulati

\- sensori cablati

\- MQTT / WebAPI



\## 5.2 Test di regressione

\- Verifica che nessun modulo esistente cambi comportamento.

\- Verifica che il ciclo rimanga deterministico.

\- Verifica che il buffer non abbia aree orfane.



\---



\# 6. Documentazione

Ogni nuova funzionalità deve includere:

\- descrizione tecnica

\- diagramma ASCII (se necessario)

\- esempi di configurazione

\- note di diagnostica



\---



\# 7. Cosa NON è accettato

\- codice non deterministico

\- dipendenze esterne non controllate

\- logiche duplicate

\- automazioni hardcoded

\- uso eccessivo di RF

\- comportamenti impliciti

\- modifiche al buffer senza documentazione

\- PR senza issue associata



\---



\# 8. Come iniziare

1\. Leggi la filosofia del progetto.

2\. Leggi l’architettura tecnica.

3\. Esplora i diagrammi ASCII.

4\. Apri una issue per discutere la tua idea.

5\. Attendi approvazione.

6\. Invia la PR seguendo le linee guida.



\---



\# 9. Grazie

DomoManager è un progetto ambizioso: vuole portare la domotica

dall’essere un insieme di gadget RF a un sistema industriale,

affidabile, cablato, deterministico.



Ogni contributo che rispetta questa visione è benvenuto.



\# ============================================================

\# FINE CONTRIBUTING.md

\# ============================================================



