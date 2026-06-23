#!/usr/bin/env bash
#
# generate-screenshots.sh — reproducibly capture the Silent Sit store screenshots.
#
# Produces, under screenshots/<platform>/, the five representative screens:
#   01-home.png         home screen: Inizia · 30 min · tick 10 min + Down caret
#   02-settings.png     settings menu (duration / tick interval)
#   03-quiet-time.png   "turn on Quiet Time" reminder
#   04-meditation.png   running seduta, ~12 min elapsed (non-zero readout)
#   05-summary.png      "Session ended", 12 min total, ripple bloom behind it
#
# Reproducibility tricks (no manual button cycling, no 12-minute wait):
#   * config 30/10  — we wipe the emulator's persisted state so config_load()
#                     falls back to DEFAULT_DURATION/DEFAULT_INTERVAL (30/10).
#   * 12-min readout — we build with SCREENSHOT_FAKE_ELAPSED=720, which the
#                     wscript turns into a -D define; session_window.c then
#                     back-dates the seduta start by 720 s. Guarded by #ifdef,
#                     so it never affects a normal `pebble build`.
#
# Usage:   tools/generate-screenshots.sh [platform]      (default: emery)
# Re-run as many times as you like — each run factory-resets the emulator.

set -uo pipefail

PLATFORM="${1:-emery}"
PROJ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$PROJ/screenshots/$PLATFORM"
FAKE_ELAPSED=720   # 12 min, in seconds — the meditation/summary readout

mkdir -p "$OUT"

log() { printf '\033[36m▶ %s\033[0m\n' "$*"; }
die() { printf '\033[31m✗ %s\033[0m\n' "$*" >&2; exit 1; }

hard_reset() {
  # The pypkjs<->qemu relay wedges under host load; a clean kill is the only
  # reliable recovery. Safe to call when nothing is running.
  pebble kill >/dev/null 2>&1 || true
  pkill -9 -f qemu-pebble >/dev/null 2>&1 || true
  pkill -9 -f pypkjs      >/dev/null 2>&1 || true
  sleep 2
}

wipe_persist() {
  # Factory-reset the emulator for this platform so config_load() hits defaults.
  # Persist lives at "<data>/Pebble SDK/<sdk-version>/<platform>"; glob across
  # SDK versions rather than hard-coding one.
  local base="$HOME/Library/Application Support/Pebble SDK"
  local d
  for d in "$base"/*/"$PLATFORM"; do
    [ -d "$d" ] && rm -rf "$d" && log "wiped persisted state: $d"
  done
}

press() { pebble emu-button click "$1" >/dev/null 2>&1; sleep "${2:-0.9}"; }

shot() {  # shot <name> — retry through the transient TimeoutError
  local name="$1" tries=0
  until pebble screenshot "$OUT/$name.png" --no-open >/dev/null 2>&1; do
    tries=$((tries + 1))
    [ "$tries" -ge 6 ] && die "screenshot '$name' kept timing out (host load? check uptime)"
    log "  retry $name ($tries)"; sleep 2
  done
  log "  saved $name.png"
}

cd "$PROJ" || die "cannot cd to project"

log "building screenshot variant (SCREENSHOT_FAKE_ELAPSED=$FAKE_ELAPSED)"
SCREENSHOT_FAKE_ELAPSED="$FAKE_ELAPSED" pebble build >/dev/null || die "build failed"

log "factory-resetting $PLATFORM emulator"
hard_reset
wipe_persist

log "cold-booting + installing on $PLATFORM"
pebble install --emulator "$PLATFORM" >/dev/null 2>&1 || {
  # First cold boot can race; one retry after a reset clears it.
  hard_reset
  pebble install --emulator "$PLATFORM" >/dev/null 2>&1 || die "install failed"
}
sleep 1.5

log "capturing screens"
shot 01-home                  # home: Inizia · 30 min · tick 10 min + Down caret
press down                    # Down -> settings menu (duration / tick interval)
shot 02-settings
press back                    # back to home
press select                  # Start -> Quiet Time reminder (Quiet Time off in emu)
shot 03-quiet-time
press select                  # "Select = start anyway" -> running seduta (faked 12 min)
shot 04-meditation
press back                    # 1st back -> "Back = stop" confirm
press back                    # 2nd back -> summary
shot 05-summary

log "restoring a clean (non-screenshot) build"
pebble build >/dev/null || die "clean rebuild failed"

log "done — screenshots in $OUT"
ls -1 "$OUT"
