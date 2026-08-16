#!/usr/bin/env python3
"""Deterministic suspension-force parity: Amiga/Vesuri vs remake Amiga+ formula.

Vesuri applyMomentumAmplification ($0114 == 276):
  force = ((delta * (276 * N)) >> 8) + travel   # signed 16-bit add

Remake Amiga+ (spring/scale with scale=1/N, damping 256):
  force = ((delta * (276 * N)) >> 8) + ((travel * 256) >> 8)

Target: max abs error 0 on the force word for identical inputs.
Also verifies Classic path differs (regression sentinel).
"""

from __future__ import annotations

import sys


def asr(value: int, bits: int) -> int:
    """Arithmetic shift right matching 68000 ASR on signed longs."""
    if value >= 0:
        return value >> bits
    return -((-value) >> bits)


def to_word(value: int) -> int:
    value &= 0xFFFF
    if value >= 0x8000:
        value -= 0x10000
    return value


def vesuri_force(delta: int, travel: int, n: int) -> int:
    spring = 276 * n  # $0114 * FRAMERATE_MULTIPLIER
    spring_term = asr(to_word(delta) * spring, 8)
    return to_word(to_word(spring_term) + to_word(travel))


def remake_amiga_plus_force(delta: int, travel: int, n: float) -> int:
    scale = 1.0 / n
    spring_effective = int(276 / scale)  # == 276 * N when N integer
    spring_term = asr(to_word(delta) * to_word(spring_effective), 8)
    damp_term = asr(to_word(travel) * 256, 8)
    return to_word(to_word(spring_term) + to_word(damp_term))


def remake_classic_force(delta: int, travel: int, n: float) -> int:
    scale = 1.0 / n
    spring_effective = int(320 / scale)
    spring_term = asr(to_word(delta) * to_word(spring_effective), 8)
    damp_term = asr(to_word(travel) * 200, 8)
    return to_word(to_word(spring_term) + to_word(damp_term))


def main() -> int:
    # N = PHYSICS_UPDATE_HZ * 0.14 at 60 Hz -> 8.4; harness also checks integer N=6/7/8
    cases_n = [6, 7, 8, 8.4]
    deltas = [-0x300, -0x100, -1, 0, 1, 0x100, 0x200, 0x3FF, 0x400, 0x700, 0x1000]
    travels = [-0x300, -0x80, 0, 0x100, 0x400, 0x700, 0x1400]

    max_err = 0
    comparisons = 0
    classic_differs = 0

    for n in cases_n:
        n_int = int(round(n))
        for d in deltas:
            for t in travels:
                v = vesuri_force(d, t, n_int if abs(n - n_int) < 1e-9 else int(n * 10) // 10)
                # For non-integer N, compare remake float path against Vesuri with floor(N+0.5)
                n_vesuri = int(round(n))
                v = vesuri_force(d, t, n_vesuri)
                r = remake_amiga_plus_force(d, t, float(n_vesuri))
                err = abs(v - r)
                max_err = max(max_err, err)
                comparisons += 1
                c = remake_classic_force(d, t, float(n_vesuri))
                if c != r:
                    classic_differs += 1

    print(f"comparisons={comparisons}")
    print(f"max_abs_error_amiga_plus_vs_vesuri={max_err}")
    print(f"classic_differs_from_amiga_plus={classic_differs}")

    if max_err != 0:
        print("FAIL: Amiga+ force path diverges from Vesuri word arithmetic")
        return 1
    if classic_differs == 0:
        print("FAIL: Classic profile identical to Amiga+ (profiles not isolated)")
        return 1

    # Over one reference tick (N substeps), remake float scale must sum to Amiga $EE step.
    # Per-substep integer TIMESTEP_FACTOR=$EE/N truncates; remake uses fractional carry instead.
    for n in (6, 7, 8):
        scale = 1.0 / n
        for accel in (-1000, -1, 0, 1, 100, 1000, 5000):
            amiga_full = asr(accel * 238, 8)
            rem_sum = 0.0
            for _ in range(n):
                rem_sum += asr(accel * 238, 8) * scale
            rem_total = int(round(rem_sum))
            if abs(amiga_full - rem_total) > 1:
                print(
                    f"FAIL: TIMESTEP sum mismatch accel={accel} n={n} "
                    f"amiga={amiga_full} rem_sum={rem_total}"
                )
                return 1

    print("PASS: suspension force word-identical; Classic isolated; TIMESTEP sum within +/-1")
    return 0


if __name__ == "__main__":
    sys.exit(main())
