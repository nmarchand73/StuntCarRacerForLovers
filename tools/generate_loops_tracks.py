#!/usr/bin/env python3
"""
Generate the Loops track pack.

True inverted loops are impossible in the Amiga height-field format.
We reuse classic steep-track footprints (XZ + angle + Y-profile IDs + boost
verbatim) and only add a gentle uniform height lift.

Important: do NOT scale overall Y shifts. Classic lips cancel against adjacent
shift steps; scaling invents extra cliffs and makes jumps unmanageable at max speed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import List, Sequence, Tuple

TRACK_DATA_SIZE = 804


def put_word(out: bytearray, offset: int, value: int) -> None:
    value &= 0xFFFF
    out[offset] = (value >> 8) & 0xFF
    out[offset + 1] = value & 0xFF


def get_word(data: bytes, offset: int) -> int:
    v = (data[offset] << 8) | data[offset + 1]
    if v >= 0x8000:
        v -= 0x10000
    return v


def decode_track(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < TRACK_DATA_SIZE:
        raise ValueError(f"{path} size {len(data)}, expected at least {TRACK_DATA_SIZE}")
    count = data[0]
    start = data[1]
    left_shifts = [get_word(data, 402 + i * 2) for i in range(count)]
    right_shifts = [get_word(data, 602 + i * 2) for i in range(count)]
    return {
        "count": count,
        "start": start,
        "xz": bytes(data[2 : 2 + count]),
        "angles": bytes(data[102 : 102 + count]),
        "left_ids": bytes(data[202 : 202 + count]),
        "right_ids": bytes(data[302 : 302 + count]),
        "left_shifts": left_shifts,
        "right_shifts": right_shifts,
        "standard_boost": data[802],
        "super_boost": data[803],
        "trailer": bytes(data[TRACK_DATA_SIZE:]),
    }


def amplify_shifts(
    left: Sequence[int],
    right: Sequence[int],
    *,
    lift: int,
    peak: int,
) -> Tuple[List[int], List[int]]:
    """Raise the track without changing jump geometry.

    scale is intentionally fixed at 1.0 — piece-to-piece shift deltas must stay
    intact so classic lips remain clearable at max speed.
    """
    n = len(left)
    out_l: List[int] = []
    out_r: List[int] = []
    for i in range(n):
        t = i / max(1, n - 1)
        bump = 0.0
        # Half-cosine over mid lap — gradient kept tiny vs classic lip sizes.
        if peak and 0.2 < t < 0.8:
            bump = peak * (0.5 - 0.5 * math.cos((t - 0.2) / 0.6 * math.pi))
        nl = int(left[i] + lift + bump)
        nr = int(right[i] + lift + bump)
        nl = max(-32000, min(32000, nl))
        nr = max(-32000, min(32000, nr))
        out_l.append(nl)
        out_r.append(nr)
    return out_l, out_r


def encode_track(
    src: dict,
    left_shifts: Sequence[int],
    right_shifts: Sequence[int],
    standard_boost: int,
    super_boost: int,
) -> bytes:
    count = src["count"]
    out = bytearray(TRACK_DATA_SIZE)
    out[0] = count
    out[1] = src["start"] & 0xFF
    out[2 : 2 + count] = src["xz"]
    out[102 : 102 + count] = src["angles"]
    out[202 : 202 + count] = src["left_ids"]
    out[302 : 302 + count] = src["right_ids"]
    for i in range(count):
        put_word(out, 402 + i * 2, left_shifts[i])
        put_word(out, 602 + i * 2, right_shifts[i])
    out[802] = standard_boost & 0xFF
    out[803] = super_boost & 0xFF
    trailer = src.get("trailer") or b""
    return bytes(out) + bytes(trailer)


TRACKS = (
    {
        "name": "Helix Climb",
        "file": "HelixClimb.bin",
        "source": "data/Tracks/RollerCoaster.bin",
        "lift": 600,
        "peak": 400,
        "design": "Roller Coaster footprint; jump deltas preserved, soft scenic lift.",
    },
    {
        "name": "Banked Bowl",
        "file": "BankedBowl.bin",
        "source": "data/Tracks/BigRamp.bin",
        "lift": 500,
        "peak": 300,
        "design": "Big Ramp footprint; original lip clearable at max speed.",
    },
    {
        "name": "Twin Cork",
        "file": "TwinCork.bin",
        "source": "data/Tracks/SkiJump.bin",
        "lift": 400,
        "peak": 350,
        "design": "Ski Jump footprint; classic lip + source boost kept.",
    },
    {
        "name": "Sky Coil",
        "file": "SkyCoil.bin",
        "source": "data/Tracks/HighJump.bin",
        "lift": 500,
        "peak": 350,
        "design": "High Jump footprint; jump sizes match the classic clear.",
    },
)


def sha1_hex(data: bytes) -> str:
    return hashlib.sha1(data).hexdigest()


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def route_from_xz(xz: bytes) -> List[dict]:
    return [{"x": c & 0x0F, "z": (c >> 4) & 0x0F} for c in xz]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate the Loops track pack")
    parser.add_argument("--output-dir", default="data/Tracks/Loops")
    parser.add_argument("--manifest", default=None)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = Path(args.output_dir)
    manifest_path = Path(args.manifest) if args.manifest else output_dir / "manifest.json"
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest_tracks = []
    for index, spec in enumerate(TRACKS):
        src = decode_track(Path(spec["source"]))
        left_s, right_s = amplify_shifts(
            src["left_shifts"],
            src["right_shifts"],
            lift=spec["lift"],
            peak=spec["peak"],
        )
        std_boost = src["standard_boost"]
        super_boost = src["super_boost"]
        data = encode_track(src, left_s, right_s, std_boost, super_boost)
        out_path = output_dir / spec["file"]
        out_path.write_bytes(data)
        print(
            f"Generated {spec['name']}: {out_path} "
            f"({src['count']} pieces, boost {std_boost}/{super_boost} from source)"
        )
        manifest_tracks.append(
            {
                "index": index,
                "name": spec["name"],
                "file": spec["file"],
                "source_footprint": spec["source"],
                "size": len(data),
                "pieces": src["count"],
                "start_piece": src["start"],
                "standard_boost": std_boost,
                "super_boost": super_boost,
                "y_ids": "preserved_from_source",
                "shift_scale": 1.0,
                "shift_lift": spec["lift"],
                "shift_peak": spec["peak"],
                "sha1": sha1_hex(data),
                "sha256": sha256_hex(data),
                "route": route_from_xz(src["xz"]),
                "design": spec["design"],
            }
        )

    manifest = {
        "pack": "Loops",
        "note": (
            "Classic steep footprints with Y IDs + boost preserved. Only a gentle "
            "uniform lift is applied so jump lips stay clearable at max speed. "
            "Not true inverted loops."
        ),
        "tracks": manifest_tracks,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
