# AGENTS.md

Silent Sit — a silent, vibration-only Vipassana meditation timer for the **Pebble Time 2** (`emery`, 200×228 colour), built in C against the classic Pebble SDK 3. Despite the `package.json`, this is **not** a Node/TypeScript project — it is a Pebble C watchapp; `package.json` is the Pebble app manifest.

Read [`README.md`](README.md) first — it is the source of truth for behaviour, architecture, build/run, publishing, and screenshot/icon regeneration. This file is the agent-oriented summary; do not duplicate the README, point to it.

## Setup

- Requires the Pebble SDK: the `pebble` CLI + QEMU emulator (`pebble` is on `PATH`).
- No `npm install` / dependencies despite `package.json` — `dependencies` is empty.
- Skills the repo expects live in `.agents/skills/` (pinned in `skills-lock.json`); the **pebble-watchapp** skill is the reference for SDK APIs, platforms/memory, and CLI.

## Commands

| Task | Command |
|------|---------|
| Build | `pebble build` (waf / `wscript`) |
| Run in emulator | `pebble install --emulator emery` |
| Install on watch | `pebble install --phone <ip>` |
| Regenerate store screenshots | `tools/generate-screenshots.sh` |
| Regenerate PDC vector glyphs | `tools/generate-pdc.sh` |
| Publish to appstore | see [README → Publishing](README.md#publishing-to-the-appstore) |

## Layout

| Path | Responsibility |
|------|----------------|
| `src/c/main.c` | config load/save + navigation router |
| `src/c/setup_window.c` | home screen (Start action + config summary) |
| `src/c/settings_window.c` | duration / tick-interval menu |
| `src/c/quiet_window.c` | "turn on Quiet Time" reminder |
| `src/c/session_window.c` | running session, confirm-stop, summary |
| `src/c/markers.c` | pure cycle/tick/half/end scheduler |
| `src/c/locale.c` | system-language string tables (EN/IT/ES/FR/DE/PT) |
| `resources/` | icon, PDC vectors + their SVG masters, font |
| `wscript` | waf build rules (incl. `SCREENSHOT_FAKE_ELAPSED` define) |
| `STORE.txt` | appstore listing description |
| `build/` | waf output — generated, gitignored, never edit |

## Conventions & gotchas

- **Marker timing is a pure function** of `(elapsed seconds, config)` — every tick/half/end and the infinite loop derive from absolute elapsed time (zero drift). Keep new timing logic in `markers.c` and keep it pure; don't accumulate time across timers.
- **Precedence is `end > half > tick`** when markers collide on the same instant.
- **Localization**: user-facing strings go in `src/c/locale.c` (one table per language) plus a line in the prefix map. Universal tokens (`min`, `tick`, `off`, `Select`, `Quiet Time`) stay untranslated.
- **No test framework** for Pebble — verify by running in the emulator + screenshots. The pure `markers.c` can be compiled/checked on the host.
- **To exercise the app in the emulator, drive it through `tools/generate-screenshots.sh` (or a script derived from it), not raw `pebble install` + manual clicks.** That script already encodes the reliable recipe: hard-reset of the flaky pypkjs↔QEMU relay, factory-reset of persisted state, `SCREENSHOT_FAKE_ELAPSED` for a non-zero readout without waiting, scripted `pebble emu-button` presses through every screen, and retry-on-`TimeoutError` around `pebble screenshot`. To test a different flow, copy it and change the `press`/`shot` sequence rather than reinventing the emulator plumbing.
- **Target platform is `emery` only** (see `targetPlatforms` in `package.json`).
- **Publishing has sharp edges** documented in the README: screenshot filenames must start with `<platform>_`; `--category` must be one of the fixed set (`health` here); each `--version` must be unique and increasing (bump `package.json`).
- The emulator's pypkjs↔QEMU relay is flaky (`TimeoutError`); `generate-screenshots.sh` hard-resets and retries.
