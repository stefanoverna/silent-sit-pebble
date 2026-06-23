# Scoping — Vipassana Timer per Pebble Time 2

> Breakdown tecnico del pitch (`PITCH.md`). Metodo Shape Up: slice verticali
> indipendenti, ciascuno costruibile e testabile da solo. Appetite: ~6 settimane.
> Piattaforma: `emery` (Pebble Time 2, 200×228, 64 col, 128 KB RAM).

---

## 1. Architettura in una frase

Una watchapp C a **due finestre** (Setup → Session) guidata da uno **scheduler di
marcatori basato sul tempo assoluto** (`time(NULL)`), zero drift, con la durata
scelta usata come **lunghezza di ciclo** che si ripete all'infinito mentre il tempo
totale continua a salire.

## 2. Macchina a stati

```
        ┌─────────┐  SELECT su "Inizia"   ┌──────────────┐
        │  SETUP   │ ────────────────────▶ │ QUIET CHECK  │
        │ (menu)   │ ◀──────────┐          └──────┬───────┘
        └─────────┘   BACK      │   DND off │      │ DND on / "Inizia comunque"
             ▲                  │           ▼      ▼
             │                  │      ┌──────────────────┐
             │   "Nuova seduta" │      │     RUNNING      │ ← i cicli si ripetono
        ┌────┴─────┐            │      │  (mostra N min)  │   QUI DENTRO, senza
        │ SUMMARY  │ ◀──────────┼──────│                  │   cambio di stato
        │ (totale) │  conferma  │      └────────┬─────────┘
        └──────────┘            │   BACK        │
                                └───────────────┤
                                  ┌─────────────▼────┐
                                  │  CONFIRM STOP    │ (BACK di nuovo = stop)
                                  └──────────────────┘
```

- **RUNNING è uno stato solo**: il loop di fine ciclo NON è una transizione, è lo
  scheduler che azzera i marcatori del ciclo e prosegue. Il display non si azzera.
- **QUIET CHECK** è un passaggio lampo: se il Quiet Time è già attivo si tira dritto
  su RUNNING senza mostrare nulla.

## 3. Modello dati

```c
// Config persistita (persist_*), letta in init, scritta on-change
typedef struct {
  uint8_t duration_min;   // 10|15|20|30|45|60   (lunghezza ciclo)
  uint8_t interval_min;   // 0|10|30  (0 = off)  (tick periodico)
} Config;

// Runtime della seduta (solo in RAM)
static time_t  s_start_time;     // time(NULL) all'avvio — riferimento anti-drift
static int     s_last_shown_min; // ultimo minuto disegnato (redraw solo on-change)
```

- Persist keys: `PKEY_DURATION = 1`, `PKEY_INTERVAL = 2`. Default 30 min / tick 10.
- **Anti-drift**: ogni calcolo deriva da `elapsed = time(NULL) - s_start_time`, mai
  da contatori accumulati.

## 4. Cuore: lo scheduler dei marcatori

Tutto il comportamento (tick, metà, fine, loop, collisioni) è una funzione pura di
`elapsed` e `Config`. Logica per ogni secondo trascorso:

```c
int cycle = cfg.duration_min * 60;          // lunghezza ciclo in secondi
int pos   = elapsed % cycle;                 // posizione dentro il ciclo corrente
int tick  = cfg.interval_min * 60;           // 0 se "off"

bool is_end  = (elapsed > 0) && (pos == 0);          // fine ciclo → riparte
bool is_half = (pos == cycle / 2);                   // metà ciclo
bool is_tick = (tick > 0) && (elapsed > 0) && (elapsed % tick == 0);

if (is_end)        vibe_triple();            // fine: tripla, la seduta prosegue
else if (is_half)  vibe_triple();            // metà: tripla
else if (is_tick)  vibe_single();            // tick: singola leggera
//   ^ collisione risolta dalla precedenza: la tripla vince, il tick è soppresso
```

- `cycle/2` è sempre intero in secondi (durate multiple di 60 → metà = multipli di 30;
  es. 15 min → metà a 7:30). Serve quindi **precisione al secondo**, non al minuto.
- **Display**: aggiornato solo quando cambia il minuto (`elapsed/60 != s_last_shown_min`),
  così la UI non si ridisegna 60 volte al minuto.

### Due implementazioni possibili del "battito"
| Approccio | Pro | Contro | Scelta |
|---|---|---|---|
| **A. `TickTimerService(SECOND_UNIT)`** + check del modulo a ogni secondo | semplice, robusto, banale da testare | wakeup ogni secondo (batteria) | **MVP** |
| **B. `app_timer` schedulato all'evento successivo** (calcola il prossimo marcatore e dorme fino a lì) + tick al minuto per il display | pochissimi wakeup, battery-friendly | più codice, edge-case sul prossimo evento | **rifinitura** (slice 6) |

Si parte con A per correttezza, si passa a B se il test batteria su seduta lunga lo
richiede.

## 5. Pattern di vibrazione (calibrare in slice 3)

```c
static void vibe_single(void) { vibes_short_pulse(); }   // o pattern {80} ms (più leggero)

static void vibe_triple(void) {
  static const uint32_t seg[] = {120, 120, 120, 120, 120}; // ON/OFF/ON/OFF/ON, leggera
  vibes_enqueue_custom_pattern((VibePattern){ .durations = seg, .num_segments = 5 });
}
```
Durate in ms da tarare a mano sull'emulatore/hardware (la skill: max 10000ms/segmento,
intensità non regolabile, solo il ritmo).

## 6. Layout file (split contenuto)

```
src/c/
├── main.c          # init/deinit, window stack, owns la Config, routing stati
├── setup_window.c  # MenuLayer: "Inizia" + Durata + Intervallo (cicla i valori)
├── session_window.c# TextLayer "N min", BACK→conferma, summary
└── markers.c       # scheduler puro (sezione 4) — l'unica logica con unit-logica testabile a mano
```
(File singolo resta possibile; lo split tiene `markers.c` isolato e ragionabile.)

## 7. Le slice (in ordine di costruzione)

Ogni slice è **verticale**: si builda, si installa nell'emulatore (`pebble install
--emulator emery`), si verifica con screenshot + `pebble logs`.

### Slice 0 — Scaffold _(½ giorno)_
- Copiare `assets/watchapp-template/`, nuovo `uuidgen`, `package.json` con
  `displayName: "Vipassana"`, `targetPlatforms: ["emery","basalt"]` (basalt comodo
  per l'emulatore), `watchface: false`.
- **Done quando**: `pebble build && pebble install --emulator emery` mostra l'app vuota.

### Slice 1 — Session timer core _(~1 settimana)_ ⭐ il cuore
- Session window: TextLayer centrato `"34 min"` (font grande, vedi §8), sfondo scuro.
- Conteggio da `time(NULL)`, sale all'infinito; il concetto di ciclo esiste ma qui
  basta che il numero salga corretto e senza drift.
- BACK → CONFIRM STOP (secondo BACK conferma) per non uscire per sbaglio.
- **Done quando**: parte, conta i minuti reali, si ferma solo con doppio BACK.
- _Test_: `pebble emu-set-time` per accelerare, screenshot ai cambi minuto.

### Slice 2 — Scheduler marcatori + loop _(~1 settimana)_ ⭐
- Implementare `markers.c` (§4): tick singola, metà tripla, fine tripla, collisioni,
  reset dei marcatori a fine ciclo con il timer che prosegue.
- **Done quando**: con durata corta di test (es. 2 min, tick 1 min via build di prova)
  i log mostrano gli eventi negli istanti giusti, incluso il loop.
- _Test_: `APP_LOG` su ogni evento + `pebble logs`; durate ridotte per non aspettare.

### Slice 3 — Calibrazione vibrazioni _(~2 giorni)_
- Tarare `vibe_single` / `vibe_triple` (§5) su hardware/emulatore: leggere ma percepibili.
- **Done quando**: i tre pattern sono distinguibili al polso e non "bruschi". Time-boxed.

### Slice 4 — Setup screen + persistenza _(~1 settimana)_
- MenuLayer (auto-wire UP/DOWN/SELECT): riga 1 **"▶ Inizia (30 min · tick 10m)"**,
  riga 2 **"Durata: 30 min"**, riga 3 **"Intervallo: 10 min"**.
- Quick-start: all'apertura la selezione è sulla riga "Inizia" → **un SELECT parte**.
- SELECT su Durata/Intervallo **cicla** il valore (wrap) e fa `menu_layer_reload_data`.
- `persist_*` in init (read, con default) e on-change (write).
- **Done quando**: cambio valori, esco/riapro app, ritrova l'ultima config; un tap parte.

### Slice 5 — Quiet mode _(~2 giorni)_
- All'avvio: `quiet_time_is_active()`. Se off → schermata reminder "Attiva Quiet Time"
  (con "Inizia comunque" = SELECT). Se on → dritti su RUNNING + piccolo indicatore DND.
- **Done quando**: il ramo off mostra il reminder, il ramo on parte liscio.

### Slice 6 — Polish & cross-platform _(~1 settimana, comprimibile)_
- Font/layout verificati su `emery` (e graceful su basalt/aplite via `PBL_IF_*`).
- SUMMARY allo stop: durata totale seduta (es. "Seduta: 47 min").
- Switch allo scheduler **B** (app_timer, §4) se il test batteria su seduta lunga lo
  impone; backlight off durante la seduta.
- Icona menu (~25×25 png), `pebble build` su tutti i target, prep `pebble publish`.

## 8. Display "N min" (dettaglio)

- `FONT_KEY_BITHAM_42_BOLD` rende sia cifre sia "min" → un solo TextLayer `"34 min"`
  centrato. In alternativa numero grande (`LECO_42_NUMBERS`, solo cifre) + "min"
  piccolo sotto in un secondo layer.
- Layout da `layer_get_bounds()`, mai coordinate fisse; `PBL_IF_ROUND_ELSE` non serve
  (emery è rettangolare) ma lo teniamo per portabilità.
- Buffer `static char[16]` con `snprintf` (la stringa non viene copiata da `text_layer_set_text`).

## 9. Rischi tecnici & circuit breaker

| Rischio | Mitigazione |
|---|---|
| Batteria con `SECOND_UNIT` su sedute lunghe | passare allo scheduler B (app_timer all'evento) in slice 6 |
| Vibrazione "leggera" difficile da tarare | time-box in slice 3, scelta fissa |
| Uscita accidentale dalla seduta | doppio-BACK con conferma (slice 1) |
| `emery` non installato nell'SDK locale | `pebble sdk list`; ripiego test su `basalt` |
| Quiet Time read-only | già accettato: ramo "rileva + ricorda" (slice 5) |

**Se il tempo stringe, si tagliano in quest'ordine** (senza intaccare il cuore):
opzione intervallo "off" → SUMMARY → scheduler B → indicatore DND. Slice 1+2 sono
intoccabili.

## 10. Sequenza di lavoro consigliata

`Slice 0 → 1 → 2 → 3` formano un'**app già usabile** (setup ancora assente: si parte
con config di default hardcoded). Poi `4 → 5 → 6` la rendono completa. Si può
"bettare" e fermarsi dopo la 4 con un prodotto pienamente funzionale.
