#!/usr/bin/env bash
# Render Amiga race music to OGG for native + web builds.
# Native decodes Blood_Money.ingame.ogg at runtime via stb_vorbis.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MOD="${1:-$ROOT/data/Music/Race/Blood_Money/mod.ingame}"
OUT="${2:-$ROOT/data/Music/Race/Blood_Money.ingame.ogg}"
DURATION_SEC="${RACE_MUSIC_DURATION_SEC:-253.6}"

if [[ ! -f "$MOD" ]]; then
  echo "Missing race MOD: $MOD" >&2
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

TMP_WAV="$(mktemp /tmp/race_music_render.XXXXXX.wav)"
cleanup() { rm -f "$TMP_WAV"; }
trap cleanup EXIT

(
  cd "$(dirname "$MOD")"
  uade123 --disable-timeouts -1 -f "$TMP_WAV" "$(basename "$MOD")"
)

ffmpeg -y -hide_banner -loglevel error \
  -i "$TMP_WAV" -t "$DURATION_SEC" -strict -2 -c:a vorbis -q:a 5 -f ogg "$OUT"

echo "Wrote $OUT ($(ffprobe -hide_banner -show_entries format=duration -of default=nw=1:nk=1 "$OUT" 2>/dev/null)s)"
