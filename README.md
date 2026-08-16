# Stunt Car Racer for Lovers

Geoff Crammond’s *Stunt Car Racer*, remade for modern machines — with shared races, Amiga+ physics, and an **Enhanced Look** that stays inside the original Amiga colour world.

| | |
|---|---|
| **Repo** | https://github.com/nmarchand73/StuntCarRacerForLovers |
| **Play** | https://retro-foundry.github.io/multistuntcar/ |
| **Upstream remake** | https://github.com/ptitSeb/stuntcarremake |

```bash
cmake -S . -B build && cmake --build build
./build/stuntcarracer
```

---

## What’s new

- **Two-player** — local + web (WebRTC guest/host)
- **Amiga+ physics** — Vesuri-tuned springs & damping closer to the Amiga feel
- **Speed feel** — FOV punch and rim blur that scale with pace
- **Enhanced Look (`O`)** — denser SCR world without leaving the palette:
  - Horizon rings of brick buildings / towers
  - Full-track yellow/red stripe panels (preview + race)
  - Irregular rails, flags, boards, floodlights, tyre stacks, billboards
  - Voxel “cube field” outside the circuit (trees, rocks, mesa)
  - Living ambient: layered clouds, birds, dust, blinkers, chase drones
  - Opponent liveries from the SCR palette
- **Track packs** — Classic, TNT, Original, Loops

Toggle Enhanced off anytime for the classic remake look. Visual bumps and wear do **not** change physics.

---

## Controls

| Key | Action | Env override | Default |
|-----|--------|--------------|---------|
| **U** | Amiga+ physics | `SCR_AMIGA_PHYSICS=0\|1` | On |
| **I** | Speed feel | `SCR_SPEED_FEEL=0\|1` | On |
| **O** | Enhanced Look | `SCR_AESTHETICS=0\|1` | On |
| **P** | Pause | — | — |
| **F4** | Cycle scenery type | — | — |

### Track preview

| Input | Action |
|-------|--------|
| **← / →** | Single Player ↔ Multiplayer |
| **↑ / ↓** (SP) | Opponent pack size **1–4** |
| **Enter / A** | Start race |

Preview footer shows physics / speed / look state (`[U]` `[I]` `[O]`).

---

## Build

**Native (macOS / Linux / Windows)** — SDL2 + OpenGL + SDL_ttf:

```bash
cmake -S . -B build
cmake --build build
./build/stuntcarracer
```

Windows Release:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

**Web (Emscripten):**

```bash
emcmake cmake -S . -B build-web
cmake --build build-web
```

WebRTC 2-player signaling: [webrtc/muttistuntcarsignal/README.md](webrtc/muttistuntcarsignal/README.md).

---

## Physics

| Mode | Springs | Damping | Notes |
|------|---------|---------|-------|
| **Amiga+** (`U`) | `276` | `256` | Amiga air/ground angular damping — see `docs/physics-audit.md` |
| **Classic** | `320` | `200` | Original remake defaults |

Parity harness:

```bash
python3 tools/physics_parity_harness.py
```

---

## Tracks

Cycle packs from the track menu.

### Classic

The original remake circuits (Little Ramp, Big Ramp, Draw Bridge, …).

### TNT

Extracted from `reference/SCR-TNT` with `tools/extract_tnt_tracks.py` → `data/Tracks/TNT/`  
(`DizzyDescent` … `RatRace`). Use `--force` to skip fingerprint checks.

### Original

`Skyline Spiral` via `tools/generate_original_tracks.py` → `data/Tracks/Original/`.

### Loops

Steep spiral / banked-bowl approximations (Amiga tracks are an XZ height field — not true inverted loops):

```bash
python3 tools/generate_loops_tracks.py
```

→ `data/Tracks/Loops/` (`Helix Climb`, `Banked Bowl`, `Twin Cork`, `Sky Coil`).  
Y-profile IDs and boost bytes stay intact so jumps remain clearable; only a gentle height lift is applied.

---

## Layout

```
src/          Game + platform (incl. AestheticsFeel, TrackProps)
data/         Tracks, sounds, bitmaps, fonts
  Bitmap/enhanced/   Optional Enhanced Look textures
  Tracks/{TNT,Original,Loops}/
build/        Native build
build-web/    Emscripten build
docs/         Physics audit
tools/        Track extractors / generators
```

---

## Design rule

Enhanced Look stays in **Crammond’s SCR palette** — flat shades, dusty horizon, no photoreal asphalt / dirt / PBR. If it wouldn’t look at home next to Amiga Stunt Car Racer, it doesn’t ship.

---

## Credits

- Original Amiga game — Geoff Crammond / MicroProse  
- Remake base — [ptitSeb/stuntcarremake](https://github.com/ptitSeb/stuntcarremake) ([SourceForge](http://sourceforge.net/projects/stuntcarremake/))  
- Some sound-loading code from Forsaken/ProjectX port work by chino  
- Amiga physics reference — Vesuri framerate-unleashed disassembly
