#!/usr/bin/env bash
# Render Amiga Hollywood Poker Pro (ingame) to OGG for the web build.
# Native builds decode dns.ingame + smp.ingame at runtime via libtfmxaudiodecoder.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$ROOT/data/Music/Race/Hollywood_Poker_Pro"
OUT="$ROOT/data/Music/Race/Hollywood_Poker_Pro.ingame.ogg"
DURATION_SEC="${RACE_MUSIC_DURATION_SEC:-138.9}"

if [[ ! -f "$SRC_DIR/dns.ingame" || ! -f "$SRC_DIR/smp.ingame" ]]; then
  echo "Missing Amiga race music in $SRC_DIR" >&2
  exit 1
fi

if ! command -v uade123 >/dev/null 2>&1; then
  echo "uade123 not found; install UADE (brew install uade) to regenerate web OGG" >&2
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg not found" >&2
  exit 1
fi

TMP_WAV="$(mktemp /tmp/hpp_render.XXXXXX.wav)"
cleanup() { rm -f "$TMP_WAV"; }
trap cleanup EXIT

(
  cd "$SRC_DIR"
  uade123 --disable-timeouts -1 -f "$TMP_WAV" dns.ingame
)

ffmpeg -y -hide_banner -loglevel error \
  -i "$TMP_WAV" -t "$DURATION_SEC" -c:a libopus -b:a 96k "$OUT"

echo "Wrote $OUT ($(ffprobe -hide_banner -show_entries format=duration -of default=nw=1:nk=1 "$OUT" 2>/dev/null)s)"
