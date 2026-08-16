# Physics constant audit — Vesuri / Amiga vs remake

Sources:
- Vesuri `StuntCarRacer.s` (FPS-unlocked, labeled)
- `reference/StuntCarRacer.s` (original Amiga disassembly in repo)
- Remake `src/Car_Behaviour.cpp`, `src/PhysicsConfig.h`

## Constant table

| Item | Amiga / Vesuri | Remake Classic | Amiga+ target | Status |
|------|----------------|----------------|---------------|--------|
| Gravity body force | `$013D` / `$FEC3` = **317** (`getPositiveTrigValue`) | `GRAVITY_ACCELERATION 317` | same | PASS |
| Spring (`calculate.difference`) | `#276` = `$0114` | `FRONT/REAR_SUSPENSION_SPRING **320**` | **276** | FIXED in Amiga+ |
| Damping / `add.w d6` | identity add of travel (= **256**/256) | `DAMPING **200**` | **256** | FIXED in Amiga+ |
| Momentum amp | Vesuri `applyMomentumAmplification`: `$0114 * FRAMERATE_MULTIPLIER` then `muls`/`asr #8`/`add.w d6` — **same op as spring**, `$0114`≡276 | Classic: spring/scale with 320 | Amiga+: spring **276**/`g_physicsStepScale` (≡ Vesuri amp) | PASS (equiv. proven) |
| Travel clamp high | `$1400` | `0x1400` | same | PASS |
| Travel clamp low | `$FD00` = **−0x300** | `−0x300` | same | PASS |
| Hard impact | vel ≥`$0400` while prev &lt;`$0200` | `amount ≥0x400` && `old &lt;0x200` | same | PASS |
| Damage band | ≥`$0700` after cushion | `damage >= 0x700` | same | PASS |
| Damage excess | subtract `$0600` | `damage -= 0x600` | same | PASS |
| Susp. vel cap | `$11FF` | remake uses related spike paths | document | EXCEPTION: remake spike guard |
| Major hole | compression ≥`$1400` | `damage_value < 0x1400` gate | same | PASS |
| Major cooldown | orig `$45`; Vesuri `$FF` | smash countdown scaled from 69/50Hz | Classic unchanged; Amiga+ keeps logic-tick gate | DOCUMENTED |
| `REDUCTION` / dt | `$EE`=238; Vesuri `TIMESTEP_FACTOR=$EE/N` | `REDUCTION 238` × `g_physicsStepScale` where scale=`dt/0.14` | prove N=1/scale | PASS (see below) |
| Air ang. damp | Vesuri `ASR #4` when airborne | always `>>4` | Amiga+: air `>>4`, ground `>>1` | FIXED in Amiga+ |
| Ground ang. damp | Vesuri `ASR #1` when grounded | always `>>4` | Amiga+: `>>1` + accel pitch term | FIXED in Amiga+ |
| Impact SFX delay | `$05` × `FRAMERATE_MULTIPLIER` | `grounded_delay = 5` | Amiga+: `5 * N` | FIXED in Amiga+ |
| Delta slew clamp | **absent** in Amiga | remake-only `0x300*scale` | Amiga+: **disabled** | FIXED in Amiga+ |
| Opponent spring | `#276` | `INCREASE 276` (not PhysicsConfig) | active profile getters | FIXED |
| Tick order | `updateGamePhysics` sequence | `CarMovement` approx. same order | match / document | PASS (documented) |
| Crane lift | `lift.car.onto.track` | stub `return` | port stages | FIXED |

## TIMESTEP equivalence proof

- Amiga reference tick uses reduction byte `$EE` = 238.
- Vesuri: `TIMESTEP_FACTOR = $EE / FRAMERATE_MULTIPLIER` applied as `muls`/`asr #8`.
- Remake: `((accel * 238) >> 8)` then scale by `g_physicsStepScale = dt / 0.14`.

Let `N = 0.14 / dt` (= `1/scale`). Then remake step ≈ `accel * 238 / (256 * N)`, identical to Vesuri with `FRAMERATE_MULTIPLIER = N`.

At `PHYSICS_UPDATE_HZ=60`, `N = 60 * 0.14 = 8.4`. Spring path: remake `276/scale = 276*N` matches Vesuri `$0114*N`.

## Runtime toggle

- Key **`U`** toggles Classic ↔ Amiga+ (`P` is pause in this remake).
- Default: **Amiga+ ON** after harness PASS (`SCR_AMIGA_PHYSICS_DEFAULT_ON 1`).
- Env: `SCR_AMIGA_PHYSICS=0|1`.

## Documented exceptions

1. Remake Classic keeps springs 320/200 (frozen baseline for A/B).
2. Remake delta slew clamp remains Classic-only (not in Amiga).
3. Smash / major-impact cooldown uses logic-tick seconds scaling already present; not re-derived from Vesuri `$FF` byte timer (different clock domain).
4. Track surface sampling size (1024 vs Amiga 256) unchanged pending height-error evidence.
5. Toggle key is **`U`** not `P` (plan conflict: `P` already pauses).

## Tick order (Amiga `updateGamePhysics` vs remake)

Amiga: matrices → wheel corners → track heights → expected surfaces → transform → wheel speed → gravity angles → suspension → (orientation/steer/secondary/collision/damping/integrate) → velocity → position.

Remake `CarMovement`: trig/wheels/road → gravity → wheel collision (suspension) → orientation/steer/accel → reduce → rotation → integrate position. Equivalent for body dynamics; UI/audio outside.
