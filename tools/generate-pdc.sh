#!/usr/bin/env bash
#
# generate-pdc.sh — regenerate the Pebble Draw Command (PDC) icon resources from
# their SVG masters in resources/.
#
# The watch app draws three glyphs as crisp, scalable vectors (not bitmaps):
#   resources/caret_down.svg      ->  resources/caret_down.pdc    (Down hint, home)
#   resources/action_start.svg    ->  resources/action_start.pdc  (play ▶, home)
#   resources/quiet_time_mouse.svg->  resources/quiet_time.pdc     (Quiet Time hero)
#
# The glyphs are authored in their final on-watch colours (white on the dark
# UI; the mouse keeps its black outline, which reads fine on the indigo Quiet
# Time background). PDC has no scaling at draw time, so each glyph is rendered at
# its authored size — the 80x80 mouse converts 1:1, a crisp hero on the 200x228
# emery screen. (Downscaling it smeared the thin tail stroke with anti-aliasing.)
#
# Conversion uses pdc_tool by Heiko Behrens (https://github.com/HBehrens/pdc_tool),
# a single self-contained binary. If it isn't on PATH or in tools/, we fetch the
# matching release into tools/ (gitignored). Re-run any time a master SVG changes.
#
# NOTE: the menu icon (resources/icon.png) stays a bitmap — Pebble menu icons
# must be bitmaps, not PDC.

set -euo pipefail

PDC_VERSION="v0.3.4"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
RES="$ROOT/resources"

# --- locate (or download) pdc_tool ------------------------------------------
find_pdc_tool() {
  if [[ -n "${PDC_TOOL:-}" && -x "${PDC_TOOL:-}" ]]; then echo "$PDC_TOOL"; return; fi
  if command -v pdc_tool >/dev/null 2>&1;            then command -v pdc_tool; return; fi
  if [[ -x "$HERE/pdc_tool" ]];                       then echo "$HERE/pdc_tool"; return; fi
  echo ""
}

download_pdc_tool() {
  local os arch target
  case "$(uname -s)" in
    Darwin) os=macos ;;
    Linux)  os=linux ;;
    *) echo "Unsupported OS for auto-download; install pdc_tool manually." >&2; exit 1 ;;
  esac
  case "$(uname -m)" in
    arm64|aarch64) arch=aarch64 ;;
    x86_64|amd64)  arch=x86_64 ;;
    *) echo "Unsupported arch for auto-download; install pdc_tool manually." >&2; exit 1 ;;
  esac
  if [[ "$os" == macos ]]; then
    target="${arch}-apple-darwin"
  else
    target="${arch}-unknown-linux-gnu"
  fi
  local url="https://github.com/HBehrens/pdc_tool/releases/download/${PDC_VERSION}/pdc_tool_${os}_${target}.zip"
  echo "Downloading pdc_tool ${PDC_VERSION} ($target)..." >&2
  local tmp; tmp="$(mktemp -d)"
  curl -sL "$url" -o "$tmp/pdc.zip"
  unzip -oq "$tmp/pdc.zip" -d "$HERE"
  chmod +x "$HERE/pdc_tool"
  xattr -d com.apple.quarantine "$HERE/pdc_tool" 2>/dev/null || true
  rm -rf "$tmp"
  echo "$HERE/pdc_tool"
}

PDC_TOOL="$(find_pdc_tool)"
[[ -z "$PDC_TOOL" ]] && PDC_TOOL="$(download_pdc_tool)"

# --- convert -----------------------------------------------------------------
echo "Using $PDC_TOOL"
"$PDC_TOOL" "$RES/caret_down.svg"      pdc "$RES/caret_down.pdc"
"$PDC_TOOL" "$RES/action_start.svg"    pdc "$RES/action_start.pdc"
"$PDC_TOOL" "$RES/quiet_time_mouse.svg" pdc "$RES/quiet_time.pdc"

echo "Done:"
for f in caret_down action_start quiet_time; do
  printf '  resources/%-18s ' "$f.pdc"
  "$PDC_TOOL" "$RES/$f.pdc" info 2>/dev/null | sed -n '2p'
done
