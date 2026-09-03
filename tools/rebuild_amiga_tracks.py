#!/usr/bin/env python3
"""
Append Amiga trailer metadata to Classic + TNT track blobs.

When an existing remake .bin is present, its 804-byte geometry core is preserved
(only boost stock bytes 802-803 are refreshed). Re-decoding SCR-TNT into a full
804-byte core can diverge slightly and warp a track (LittleRamp road_xz[37]).

When file size > 804:
  [804] near.start.line.section
  [805] half.a.lap.section
  [806] damaged.limit (standard league)   # B.1ca2a
  [807] damaged.limit (super league)      # B.1ca2b
  [808] speed overlay count               # B.1ca2e
  [809] restart-exclude count             # B.1ca2f
  [810 ..)  count_e * (section, speed)    # DAT.1c8a8 / DAT.1c8c8
  then count_f section ids                # DAT.1c8e8
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from extract_tnt_tracks import (  # noqa: E402
    PIECE_DATA_OFFSETS_SIGNATURE,
    TAB_5B2C4,
    TRACK_DATA_SIZE,
    _decode_ptr_word,
    _read_u16_be,
    _srd1_sub3,
)

MAX_PIECES_PER_TRACK = 100

CLASSIC_TRACKS: List[Tuple[str, int]] = [
    ("LittleRamp", 0x1A030),
    ("SteppingStones", 0x1A0AC),
    ("HumpBack", 0x1A13D),
    ("BigRamp", 0x1A1CE),
    ("SkiJump", 0x1A25C),
    ("DrawBridge", 0x1A2ED),
    ("HighJump", 0x1A3C2),
    ("RollerCoaster", 0x1A450),
]

TNT_TRACKS: List[Tuple[str, int]] = [
    ("DizzyDescent", 0x03BE),
    ("WittyWay", 0x04BE),
    ("CrazyCaper", 0x0563),
    ("AmazingAdept", 0x067C),
    ("JerkilyJump", 0x074F),
    ("EvillyEpisode", 0x0807),
    ("TeasingTemper", 0x0926),
    ("RatRace", 0x09F3),
]


def _sha1_hex(data: bytes) -> str:
    return hashlib.sha1(data).hexdigest()


def decode_track(data: bytes, base_offset: int, piece_offsets: bytes, y_offsets: bytes, raw_start: int) -> Dict:
    cursor = 0

    def get_byte() -> int:
        nonlocal cursor
        pos = raw_start + cursor
        if pos >= len(data):
            raise ValueError(f"EOF at cursor={cursor} raw_start=0x{raw_start:X}")
        value = data[pos]
        cursor += 1
        return value

    num_sections = get_byte()
    player_start = get_byte()
    near_start = get_byte()
    half_lap = get_byte()

    hi = get_byte()
    lo = get_byte()
    near_x = (lo << 8) | hi
    near_z = near_x

    other_road_line_colour = 0
    prompt_chars = 0
    last_factor = 0
    last_xz = 0

    road_xz = [0] * MAX_PIECES_PER_TRACK
    road_angle_piece = [0] * MAX_PIECES_PER_TRACK
    left_y_ids = [0] * MAX_PIECES_PER_TRACK
    right_y_ids = [0] * MAX_PIECES_PER_TRACK
    left_shifts = [0] * MAX_PIECES_PER_TRACK
    right_shifts = [0] * MAX_PIECES_PER_TRACK

    section = 0
    while True:
        if prompt_chars:
            prompt_chars = (prompt_chars - 1) & 0xFF
            factor1 = last_factor
            road_angle_piece[section] = factor1
            if factor1 & 0x10:
                factor1 ^= 0xC0
            xz_value = _srd1_sub3(last_xz, factor1)
        else:
            factor1 = get_byte()
            road_angle_piece[section] = factor1
            if (factor1 & 0x0F) == 0x0F:
                prompt_chars = (factor1 >> 4) & 0xFF
                continue
            last_factor = factor1
            xz_value = get_byte()

        road_xz[section] = xz_value
        last_xz = xz_value
        line_value = 0x80 if (other_road_line_colour == 2) else 0x00

        template_low = factor1 & 0x0F
        if template_low >= 12:
            road_angle_piece[section] &= 0xF0
            left_val = TAB_5B2C4[template_low - 12] if 0 <= template_low - 12 < len(TAB_5B2C4) else 0
            right_source = TAB_5B2C4[template_low - 10] if 0 <= template_low - 10 < len(TAB_5B2C4) else 0
            left_y_ids[section] = left_val
        else:
            left_val = get_byte()
            left_y_ids[section] = left_val
            right_source = left_val if (factor1 & 0x20) else get_byte()

        right_y_ids[section] = ((right_source & 0x7F) | line_value) & 0xFF

        left_id = left_y_ids[section] & 0xFF
        y_words_flag = left_id
        left_off_word = _read_u16_be(y_offsets, ((left_id << 1) & 0xFF))

        right_id = right_y_ids[section] & 0xFF
        other_road_line_colour = 2 if (right_id & 0x80) else 0
        right_off_word = _read_u16_be(y_offsets, ((right_id << 1) & 0xFF))

        piece_template = road_angle_piece[section] & 0x0F
        piece_word = _read_u16_be(piece_offsets, (piece_template * 2) & 0x1F)
        piece_ptr = _decode_ptr_word(base_offset, piece_word)

        piece_d2 = data[piece_ptr]
        num_coords = data[piece_ptr + piece_d2]
        num_segments = ((num_coords >> 1) - 1) & 0xFF

        def read_y(offset_word: int, is_word_flag: int, segment: int) -> int:
            ptr = _decode_ptr_word(base_offset, offset_word)
            if is_word_flag & 0x80:
                i = (segment * 2) & 0xFF
                return (((data[ptr + i] & 0x7F) << 8) | data[ptr + i + 1]) & 0xFFFF
            packed = data[ptr + segment]
            return (((packed & 0x0F) << 8) | ((packed << 1) & 0xE0)) & 0xFFFF

        dy = read_y(left_off_word, y_words_flag, 0)
        near_x = (near_x - dy) & 0xFFFF
        left_shifts[section] = near_x

        dy = read_y(left_off_word, y_words_flag, num_segments)
        near_x = (near_x + dy) & 0xFFFF

        dy = read_y(right_off_word, y_words_flag, 0)
        near_z = (near_z - dy) & 0xFFFF
        right_shifts[section] = near_z

        dy = read_y(right_off_word, y_words_flag, num_segments)
        near_z = (near_z + dy) & 0xFFFF

        other_road_line_colour = (other_road_line_colour + ((num_coords - 2) & 0xFF)) & 2

        section += 1
        if section == num_sections:
            break

    # B.1ca2a .. B.1ca2f
    dmg_std = get_byte()
    dmg_sup = get_byte()
    std_boost = get_byte()
    super_boost = get_byte()
    count_e = get_byte()
    count_f = get_byte()

    overlays: List[Tuple[int, int]] = []
    for _ in range(count_e):
        overlays.append((get_byte(), get_byte()))
    restart_exclude = [get_byte() for _ in range(count_f)]

    out = bytearray(TRACK_DATA_SIZE)
    out[0] = num_sections & 0xFF
    out[1] = player_start & 0xFF
    out[2:102] = bytes(road_xz)
    out[102:202] = bytes(road_angle_piece)
    out[202:302] = bytes(left_y_ids)
    out[302:402] = bytes(right_y_ids)

    p = 402
    for value in left_shifts:
        out[p] = (value >> 8) & 0xFF
        out[p + 1] = value & 0xFF
        p += 2
    for value in right_shifts:
        out[p] = (value >> 8) & 0xFF
        out[p + 1] = value & 0xFF
        p += 2

    out[802] = std_boost & 0xFF
    out[803] = super_boost & 0xFF

    ext = bytearray()
    ext.append(near_start & 0xFF)
    ext.append(half_lap & 0xFF)
    ext.append(dmg_std & 0xFF)
    ext.append(dmg_sup & 0xFF)
    ext.append(count_e & 0xFF)
    ext.append(count_f & 0xFF)
    for section_id, speed in overlays:
        ext.append(section_id & 0xFF)
        ext.append(speed & 0xFF)
    ext.extend(restart_exclude)

    return {
        "blob": bytes(out) + bytes(ext),
        "num_sections": num_sections,
        "player_start": player_start,
        "near_start": near_start,
        "half_lap": half_lap,
        "dmg_std": dmg_std,
        "dmg_sup": dmg_sup,
        "std_boost": std_boost,
        "super_boost": super_boost,
        "overlays": overlays,
        "restart_exclude": restart_exclude,
    }


def _amiga_trailer(decoded: Dict) -> bytes:
    """Bytes after the 804-byte remake core: near/half/damage + overlay/exclude tables."""
    ext = bytearray()
    ext.append(decoded["near_start"] & 0xFF)
    ext.append(decoded["half_lap"] & 0xFF)
    ext.append(decoded["dmg_std"] & 0xFF)
    ext.append(decoded["dmg_sup"] & 0xFF)
    overlays = decoded["overlays"]
    restart_exclude = decoded["restart_exclude"]
    ext.append(len(overlays) & 0xFF)
    ext.append(len(restart_exclude) & 0xFF)
    for section_id, speed in overlays:
        ext.append(section_id & 0xFF)
        ext.append(speed & 0xFF)
    ext.extend(restart_exclude)
    return bytes(ext)


def rebuild_pack(
    source: bytes,
    base_offset: int,
    piece_offsets: bytes,
    y_offsets: bytes,
    tracks: Sequence[Tuple[str, int]],
    output_dir: Path,
) -> List[dict]:
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_tracks: List[dict] = []
    for index, (name, raw_start) in enumerate(tracks):
        decoded = decode_track(source, base_offset, piece_offsets, y_offsets, raw_start)
        out_path = output_dir / f"{name}.bin"
        # Prefer the existing remake 804-byte core when present. Re-decoding from
        # SCR-TNT can differ by a byte (seen on LittleRamp road_xz[37]) and warp geometry.
        existing = out_path.read_bytes() if out_path.is_file() else b""
        if len(existing) >= TRACK_DATA_SIZE:
            core = existing[:TRACK_DATA_SIZE]
            # Keep Amiga boost stock from decode inside the legacy core slots.
            core = bytearray(core)
            core[802] = decoded["std_boost"] & 0xFF
            core[803] = decoded["super_boost"] & 0xFF
            blob = bytes(core) + _amiga_trailer(decoded)
            geom_src = "preserved"
        else:
            blob = decoded["blob"]
            geom_src = "decoded"
        out_path.write_bytes(blob)
        manifest_tracks.append(
            {
                "index": index,
                "name": name,
                "file": out_path.name,
                "raw_start_hex": f"0x{raw_start:04X}",
                "size": len(blob),
                "sha1": _sha1_hex(blob),
                "geometry_source": geom_src,
                "near_start": decoded["near_start"],
                "half_lap": decoded["half_lap"],
                "damage_limit_standard": decoded["dmg_std"],
                "damage_limit_super": decoded["dmg_sup"],
                "standard_boost": decoded["std_boost"],
                "super_boost": decoded["super_boost"],
                "speed_overlays": [{"section": s, "speed": v} for s, v in decoded["overlays"]],
                "restart_exclude_sections": decoded["restart_exclude"],
            }
        )
        print(
            f"{name:16} size={len(blob):4} dmg={decoded['dmg_std']}/{decoded['dmg_sup']} "
            f"half={decoded['half_lap']} overlays={len(decoded['overlays'])} exclude={len(decoded['restart_exclude'])} "
            f"[{geom_src}]"
        )
    return manifest_tracks


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default=str(ROOT / "reference" / "SCR-TNT"))
    parser.add_argument("--classic-dir", default=str(ROOT / "data" / "Tracks"))
    parser.add_argument("--tnt-dir", default=str(ROOT / "data" / "Tracks" / "TNT"))
    args = parser.parse_args()

    source = Path(args.input).read_bytes()
    base_offset = source.find(PIECE_DATA_OFFSETS_SIGNATURE)
    if base_offset < 0:
        raise SystemExit("Could not locate piece.data.offsets signature")

    piece_offsets = source[base_offset : base_offset + 32]
    y_offsets = source[base_offset + 32 : base_offset + 288]

    print("=== Classic ===")
    classic = rebuild_pack(source, base_offset, piece_offsets, y_offsets, CLASSIC_TRACKS, Path(args.classic_dir))
    print("=== TNT ===")
    tnt = rebuild_pack(source, base_offset, piece_offsets, y_offsets, TNT_TRACKS, Path(args.tnt_dir))

    manifest = {
        "source_file": str(Path(args.input).as_posix()),
        "source_sha1": _sha1_hex(source),
        "piece_data_offsets_base_hex": f"0x{base_offset:06X}",
        "classic": classic,
        "tnt": tnt,
    }
    manifest_path = ROOT / "data" / "Tracks" / "amiga_track_metadata.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
