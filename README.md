<p align="center">
  <img src="data/Bitmap/icon.png" alt="Stunt Car Racer for Lovers" width="128" height="128">
</p>

<h1 align="center">Stunt Car Racer for Lovers</h1>

<p align="center">
  <strong>The classic jump. Shared.</strong><br>
  Geoff Crammond’s Amiga classic — remade for modern machines,<br>
  with two-player races, Amiga+ physics, and an Enhanced Look<br>
  that stays inside the original colour world.
</p>

<p align="center">
  <a href="https://retro-foundry.github.io/multistuntcar/"><img src="https://img.shields.io/badge/Play-in%20browser-ff7a18?style=for-the-badge" alt="Play in browser"></a>
  &nbsp;
  <a href="https://github.com/nmarchand73/StuntCarRacerForLovers"><img src="https://img.shields.io/badge/GitHub-repo-5599ff?style=for-the-badge&logo=github" alt="GitHub"></a>
  &nbsp;
  <a href="https://github.com/ptitSeb/stuntcarremake"><img src="https://img.shields.io/badge/Upstream-stuntcarremake-777755?style=for-the-badge" alt="Upstream"></a>
</p>

<p align="center">
  <code>macOS</code> · <code>Linux</code> · <code>Windows</code> · <code>Web</code>
  &nbsp;·&nbsp;
  <code>U</code> Amiga+ &nbsp;·&nbsp; <code>I</code> Speed &nbsp;·&nbsp; <code>O</code> Enhanced Look
</p>

---

## Quick start

```bash
cmake -S . -B build && cmake --build build
./build/stuntcarracer
```

Prefer the browser? → **[Play now](https://retro-foundry.github.io/multistuntcar/)**

---

## Features

| | |
|:---|:---|
| **Two-player** | Local splits + web guest/host over WebRTC |
| **Amiga+ physics** | Vesuri-tuned springs & damping closer to the Amiga |
| **Speed feel** | FOV punch and rim blur that scale with pace |
| **Enhanced Look** | Denser SCR world — still flat-shaded, never photoreal |
| **Track packs** | Classic · TNT · Original · Loops |

### Enhanced Look <kbd>O</kbd>

Turn it on for a lived-in Crammond arena. Turn it off for the classic remake presentation.

| Layer | What you get |
|:------|:-------------|
| **Horizon** | Dense brick scenery rings (buildings, towers) |
| **Track** | Full yellow/red stripe panels, worn sides, bumpy stretches |
| **Roadside** | Rails, flags, boards, floodlights, tyre stacks, billboards |
| **Field** | Voxel clusters outside the circuit (trees, rocks, mesa) |
| **Sky & life** | Layered clouds, birds, dust, blinkers, chase drones |
| **Cars** | Opponent liveries from the SCR palette |

> Visual bumps and border wear do **not** change physics.  
> Preview footer: `Physics … [U] | Speed … [I] | Look … [O]`

---

## Controls

### Presentation

| Key | Action | Env | Default |
|:---:|:-------|:----|:-------:|
| <kbd>U</kbd> | Amiga+ physics | `SCR_AMIGA_PHYSICS=0\|1` | On |
| <kbd>I</kbd> | Speed feel | `SCR_SPEED_FEEL=0\|1` | On |
| <kbd>O</kbd> | Enhanced Look | `SCR_AESTHETICS=0\|1` | On |
| <kbd>P</kbd> | Pause | — | — |
| <kbd>F4</kbd> | Cycle scenery | — | — |

### Track preview

| Input | Action |
|:------|:-------|
| <kbd>←</kbd> <kbd>→</kbd> | Single Player ↔ Multiplayer |
| <kbd>↑</kbd> <kbd>↓</kbd> | Opponent pack size **1–4** (SP) |
| <kbd>Enter</kbd> / <kbd>A</kbd> | Start race |

---

## Build

<details>
<summary><strong>Native</strong> — macOS / Linux / Windows (SDL2 + OpenGL + SDL_ttf)</summary>

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

</details>

<details>
<summary><strong>Web</strong> — Emscripten</summary>

```bash
emcmake cmake -S . -B build-web
cmake --build build-web
```

WebRTC signaling for 2-player: [webrtc/muttistuntcarsignal/README.md](webrtc/muttistuntcarsignal/README.md)

</details>

---

## Physics

| Mode | Springs | Damping | Notes |
|:-----|--------:|--------:|:------|
| **Amiga+** <kbd>U</kbd> | `276` | `256` | Amiga air/ground angular damping — [`docs/physics-audit.md`](docs/physics-audit.md) |
| **Classic** | `320` | `200` | Original remake defaults |

```bash
python3 tools/physics_parity_harness.py
```

---

## Track packs

Cycle packs from the track menu.

| Pack | Contents | How |
|:-----|:---------|:----|
| **Classic** | Little Ramp, Big Ramp, Draw Bridge, … | Bundled |
| **TNT** | `DizzyDescent` … `RatRace` | `tools/extract_tnt_tracks.py` → `data/Tracks/TNT/` |
| **Original** | Skyline Spiral | `tools/generate_original_tracks.py` → `data/Tracks/Original/` |
| **Loops** | Helix Climb, Banked Bowl, Twin Cork, Sky Coil | `tools/generate_loops_tracks.py` → `data/Tracks/Loops/` |

**Loops** are steep spiral / banked-bowl approximations (Amiga tracks are an XZ height field — not true inverted loops). Y-profile IDs and boost stay intact so jumps remain clearable.

```bash
python3 tools/generate_loops_tracks.py
```

---

## Project layout

```text
src/                 Game + platform
  AestheticsFeel.*   Enhanced Look toggle / fog
  TrackProps.*       Roadside props, cube field, living ambient
data/
  Bitmap/            Atlases, UI, icon
  Bitmap/enhanced/   Optional Enhanced textures
  Tracks/            Classic + TNT + Original + Loops
build/               Native out-of-source build
build-web/           Emscripten build
docs/                Physics audit
tools/               Track extractors & generators
```

---

## Design rule

Enhanced Look lives in **Crammond’s SCR palette** — flat shades, dusty horizon, no photoreal asphalt / dirt / PBR.

> If it wouldn’t look at home next to Amiga *Stunt Car Racer*, it doesn’t ship.

---

## Credits

| | |
|:---|:---|
| **Original** | Geoff Crammond / MicroProse |
| **Remake base** | [ptitSeb/stuntcarremake](https://github.com/ptitSeb/stuntcarremake) · [SourceForge](http://sourceforge.net/projects/stuntcarremake/) |
| **Amiga physics ref** | Vesuri framerate-unleashed disassembly |
| **Sound loading** | Forsaken / ProjectX port work by chino |

---

<p align="center">
  <sub>Stunt Car Racer for Lovers — the classic jump, shared.</sub>
</p>
