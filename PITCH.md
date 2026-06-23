# Pitch — Vipassana Timer per Pebble Time 2

> Documento di shaping (metodo Shape Up). Stato: **bozza** — la sezione
> "Fattibilità tecnica" è in attesa della ricerca sull'SDK Pebble.

---

## 1. Problema

Chi pratica Vipassana vuole un timer **da polso, silenzioso e tattile** che scandisca
la seduta solo con vibrazioni — niente campane, niente schermo invadente, niente
telefono in mano. Le app da smartphone distraggono, vibrano in modo grossolano e
tentano l'utente a guardare lo schermo. Il polso con Pebble è discreto, sempre lì,
e comunica con il solo tatto.

Servono inoltre **sedute aperte**: la durata scelta non è una fine, è un *ciclo* che
si ripete finché si vuole, senza interruzioni nel ritmo.

## 2. Appetite

**Big batch — ~6 settimane**, un developer. Include il nucleo + le rifiniture
(preset memorizzato, schermata di riepilogo, gestione elegante del Quiet Time).

## 3. Soluzione

### 3a. Schermata di setup (pre-seduta)
- **Quick-start**: all'apertura l'app mostra l'**ultima configurazione usata** già
  selezionata; un **singolo tap su Select fa partire subito** la seduta con quei
  valori. Il caso comune (riparto come ieri) è un solo gesto.
- Per cambiare: **durata ciclo** 10 / 15 / 20 / 30 / 45 / 60 min e **intervallo tick**
  10 min / 30 min (valuto anche "off") si regolano con i tasti su/giù, poi Select.
- La configurazione è **persistita** (`persist_*` storage), sopravvive a chiusura
  app e riavvio del watch.

### 3b. Schermata seduta (durante)
- A schermo **solo il tempo trascorso**, grande e centrato, in minuti: `34 min`.
- **Niente** progress bar, niente countdown, niente conteggio dei cicli.
  Minimalismo deliberato: lo schermo non è un oggetto di attenzione.
- Backlight spento; sfondo scuro. Si legge solo se si gira il polso.
- Il tempo **continua a salire** attraverso i loop (es. ciclo da 30m → `34`, `35`… `65 min`).

### 3c. Pattern di vibrazione
| Evento            | Pattern                          | Note |
|-------------------|----------------------------------|------|
| Tick periodico    | **singola**, breve e leggera     | ogni 10 o 30 min |
| Metà ciclo        | **tripla** leggera               | es. ciclo 30m → a 15m |
| Fine ciclo        | **tripla** → poi **riparte**     | la seduta NON si interrompe; i marcatori del ciclo si azzerano, il timer totale prosegue |

**Regole di collisione (default proposto):** se il tick periodico cade nello stesso
istante della metà o della fine, **vince la tripla** e il singolo viene soppresso in
quel momento (evita doppie vibrazioni confuse). Es.: ciclo 30m + tick 10m → tick a
10 e 20, a 30 scatta solo la tripla di fine.

### 3d. Quiet mode (anti-distrazione) — risolto: ramo "rileva + ricorda"
La ricerca conferma che il Quiet Time di Pebble è **read-only**: nessuna app può
attivarlo/disattivarlo via codice (richiesta aperta dalla community dal 2015, mai
esposta). Quindi:
- All'avvio della seduta l'app **controlla `quiet_time_is_active()`**.
- Se è **off**: schermata di **reminder** "Attiva Quiet Time per non essere
  disturbato" prima di partire.
- Se è **on**: parte diretta, con un piccolo indicatore "DND attivo".
- L'app **rispetta sempre** il Quiet Time anche per le proprie vibrazioni? → **No**:
  i tick/marcatori della seduta sono il fine stesso dell'app, quindi vibrano comunque.
  Il Quiet Time qui serve solo a zittire le notifiche *esterne*.

### 3e. Stop / fine
- Stop manuale con tasto Back **con conferma** (per non uscire per sbaglio dalla seduta).
- Schermata di **riepilogo**: durata totale della seduta — rifinitura big-batch.

## 4. Rabbit holes (da disinnescare)

- **Quiet Time programmatico**: ✅ chiarito — è read-only, **non attivabile da app**.
  Si va dritti sul ramo "rileva + ricorda". **Non** inseguire workaround/hack.
- **Calibrazione "vibrazione leggera"**: trovare la durata in ms che sia percepibile
  ma non brusca. Time-box: poche prove, scelta fissa.
- **Drift del timer su sedute lunghe**: usare il **tempo assoluto** di sistema come
  riferimento, non l'accumulo dei tick, per evitare derive su loop da ore.
- **Schermo per 60+ min e batteria**: e-paper consuma poco; tenere **backlight off**.
- **Uscita accidentale**: gestione esplicita del tasto Back.

## 5. No-gos (fuori scope, dichiarati)

- ❌ Audio / campane / suoni.
- ❌ Statistiche, streak, storico, cloud, account.
- ❌ Companion app complessa sul telefono (al più una config JS minima).
- ❌ Countdown o conteggio cicli visibili a schermo.
- ❌ Contenuti guidati / meditazioni audio.

## 6. Fattibilità tecnica

Verdetto complessivo: **tutto il nucleo è fattibile con API standard.** L'unico
vincolo reale è il Quiet Time (read-only), già disinnescato nel design (§3d).

| Area | Verdetto | API / note |
|------|----------|-----------|
| **Piattaforma** | ✅ FATTIBILE | Pebble revival attivo (Core Devices/rePebble). SDK classico in **C** (C99, `#include <pebble.h>`) + CLI `pebble` + emulatore QEMU. Pebble Time 2 = piattaforma **`emery`**, **200×228** a colori (codename confermato dalla skill; verificare la risoluzione esatta sulla hardware page). App C girano anche sui Pebble classici se in futuro si volesse allargare. |
| **Vibrazioni** | ✅ FATTIBILE | `vibes_short_pulse()` per il tick singolo leggero (o custom `{100}` ms). Tripla via `vibes_enqueue_custom_pattern()` con `VibePattern`, es. durate `{200,100,200,100,200}`. Intensità fissa: si controlla solo il ritmo. |
| **Timer / app in foreground** | ✅ FATTIBILE | Watchapp resta in foreground **senza timeout** per 60+ min. `TickTimerService` a **`MINUTE_UNIT`** (basta per "34 min", risparmia batteria). **Anti-drift**: riferirsi al tempo assoluto di sistema, non accumulare i tick. Backlight off **non** ferma i timer. |
| **Quiet Time** | ⚠️ PARZIALE | `quiet_time_is_active()` esiste ma è **read-only**. Niente attivazione da codice → ramo "rileva + ricorda" (§3d). |
| **Display** | ✅ FATTIBILE | `TextLayer` centrato + font grande (`FONT_KEY_LECO_42_NUMBERS` / `BITHAM_42_BOLD`) per il tempo. Display `emery` rettangolare 200×228, niente mascheratura rotonda. Layout da `layer_get_bounds()`, mai coordinate hardcoded. |
| **Wakeup (opz.)** | ✅ FATTIBILE | `wakeup_schedule()` per far scattare la tripla di fine anche se l'utente esce dall'app. Limiti: max 8 wakeup, non entro 30s, finestra ±1 min. Probabilmente **non necessario** (l'app resta aperta tutta la seduta). |

**Note di incertezza / da verificare in fase di build:**
- Codename di Pebble Time 2 = **`emery`** (200×228) per la skill — confermare con
  `pebble sdk list` / hardware page prima di fissare `targetPlatforms`.
- Che l'ultima `pebble.h` non abbia (improbabilmente) reso scrivibile il Quiet Time.

**Disciplina obbligatoria dalla skill** (vale per tutto il codice C):
- Ogni `*_create()` ha il suo `*_destroy()` nell'unload/deinit (causa #1 di crash/OOM).
- Le stringhe di `text_layer_set_text()` puntano a buffer **`static`**, mai stack.
- Stato a livello di modulo `static` con prefisso `s_`; redraw solo on-change
  (`layer_mark_dirty()`), mai redraw su timer fitto.
- Test = girare nell'emulatore QEMU (`pebble install --emulator emery`) + screenshot
  + `pebble logs`; non esiste unit-test framework.
