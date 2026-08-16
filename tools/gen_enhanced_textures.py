#!/usr/bin/env python3
"""Generate Enhanced Look tileable PNGs under data/Bitmap/enhanced/."""

from __future__ import annotations

import math
import os
import struct
import zlib

ROOT = os.path.join(os.path.dirname(__file__), "..", "data", "Bitmap", "enhanced")


def write_png(path: str, w: int, h: int, rgba: bytes) -> None:
    assert len(rgba) == w * h * 4

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + rgba[y * w * 4 : (y + 1) * w * 4] for y in range(h))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


def hash2(x: int, y: int, seed: int = 0) -> float:
    n = (x * 374761393 + y * 668265263 + seed * 982451653) & 0x7FFFFFFF
    n = (n ^ (n >> 13)) * 1274126177
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def value_noise(x: float, y: float, seed: int) -> float:
    x0, y0 = int(math.floor(x)), int(math.floor(y))
    fx, fy = x - x0, y - y0
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    v00 = hash2(x0, y0, seed)
    v10 = hash2(x0 + 1, y0, seed)
    v01 = hash2(x0, y0 + 1, seed)
    v11 = hash2(x0 + 1, y0 + 1, seed)
    return v00 * (1 - fx) * (1 - fy) + v10 * fx * (1 - fy) + v01 * (1 - fx) * fy + v11 * fx * fy


def fbm(x: float, y: float, seed: int, octaves: int = 4) -> float:
    a, f, t, m = 0.0, 1.0, 0.0, 0.0
    for i in range(octaves):
        t += value_noise(x * f, y * f, seed + i * 17) * a if False else value_noise(x * f, y * f, seed + i * 17) * (0.5**i)
        m += 0.5**i
        f *= 2.0
    return t / m


def gen_asphalt(size: int = 256) -> None:
    albedo = bytearray(size * size * 4)
    normal = bytearray(size * size * 4)
    spec = bytearray(size * size * 4)
    height = [[0.0] * size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            n = fbm(x / 32.0, y / 32.0, 11)
            grit = fbm(x / 8.0, y / 8.0, 29)
            v = 38 + int(n * 28) + int(grit * 18)
            v = max(20, min(90, v))
            i = (y * size + x) * 4
            albedo[i : i + 4] = bytes((v, v, v + 2, 255))
            height[y][x] = n * 0.7 + grit * 0.3
            s = int(40 + height[y][x] * 140)
            spec[i : i + 4] = bytes((s, s, s, 255))

    for y in range(size):
        for x in range(size):
            hL = height[y][(x - 1) % size]
            hR = height[y][(x + 1) % size]
            hD = height[(y - 1) % size][x]
            hU = height[(y + 1) % size][x]
            dx = (hL - hR) * 4.0
            dy = (hD - hU) * 4.0
            nz = 1.0
            length = math.sqrt(dx * dx + dy * dy + nz * nz)
            nx, ny, nz = dx / length, dy / length, nz / length
            i = (y * size + x) * 4
            normal[i] = int((nx * 0.5 + 0.5) * 255)
            normal[i + 1] = int((ny * 0.5 + 0.5) * 255)
            normal[i + 2] = int((nz * 0.5 + 0.5) * 255)
            normal[i + 3] = int(40 + height[y][x] * 140)

    write_png(os.path.join(ROOT, "asphalt_albedo.png"), size, size, bytes(albedo))
    write_png(os.path.join(ROOT, "asphalt_normal.png"), size, size, bytes(normal))
    write_png(os.path.join(ROOT, "asphalt_spec.png"), size, size, bytes(spec))


def gen_ground(size: int = 256) -> None:
    albedo = bytearray(size * size * 4)
    normal = bytearray(size * size * 4)
    height = [[0.0] * size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            n = fbm(x / 48.0, y / 48.0, 41)
            m = fbm(x / 12.0, y / 12.0, 53)
            r = int(72 + n * 50 + m * 20)
            g = int(58 + n * 40 + m * 18)
            b = int(36 + n * 22)
            i = (y * size + x) * 4
            albedo[i : i + 4] = bytes((max(0, min(255, r)), max(0, min(255, g)), max(0, min(255, b)), 255))
            height[y][x] = n

    for y in range(size):
        for x in range(size):
            hL = height[y][(x - 1) % size]
            hR = height[y][(x + 1) % size]
            hD = height[(y - 1) % size][x]
            hU = height[(y + 1) % size][x]
            dx = (hL - hR) * 2.5
            dy = (hD - hU) * 2.5
            length = math.sqrt(dx * dx + dy * dy + 1.0)
            nx, ny, nz = dx / length, dy / length, 1.0 / length
            i = (y * size + x) * 4
            normal[i : i + 4] = bytes(
                (int((nx * 0.5 + 0.5) * 255), int((ny * 0.5 + 0.5) * 255), int((nz * 0.5 + 0.5) * 255), 80)
            )

    write_png(os.path.join(ROOT, "ground_albedo.png"), size, size, bytes(albedo))
    write_png(os.path.join(ROOT, "ground_normal.png"), size, size, bytes(normal))


def gen_props(size: int = 256) -> None:
    img = bytearray(size * size * 4)
    # transparent default
    for i in range(0, len(img), 4):
        img[i : i + 4] = bytes((0, 0, 0, 0))

    def fill_rect(x0, y0, x1, y1, rgba):
        for y in range(y0, y1):
            for x in range(x0, x1):
                i = (y * size + x) * 4
                img[i : i + 4] = bytes(rgba)

    # barrier stripe panel
    fill_rect(8, 8, 120, 48, (220, 200, 40, 255))
    fill_rect(8, 48, 120, 88, (30, 30, 30, 255))
    # cone
    for y in range(100, 220):
        t = (y - 100) / 120.0
        half = int(8 + t * 36)
        cx = 180
        fill_rect(cx - half, y, cx + half, y + 1, (230, 90, 30, 255))
    fill_rect(140, 210, 220, 230, (40, 40, 40, 255))
    # post
    fill_rect(40, 120, 56, 240, (180, 180, 190, 255))
    fill_rect(36, 116, 60, 128, (200, 60, 60, 255))

    write_png(os.path.join(ROOT, "props_atlas.png"), size, size, bytes(img))


def gen_car_atlas(w: int = 512, h: int = 128) -> None:
    img = bytearray(w * h * 4)
    # liveries: player rose, blue, green, amber, teal
    colors = [
        (196, 72, 96),
        (48, 96, 200),
        (48, 150, 72),
        (210, 140, 40),
        (40, 150, 160),
    ]
    strip_w = w // len(colors)
    for li, (cr, cg, cb) in enumerate(colors):
        for y in range(h):
            shade = 0.75 + 0.25 * (y / max(1, h - 1))
            for x in range(li * strip_w, (li + 1) * strip_w):
                # dark glass band near top of strip
                if y < h // 5:
                    r, g, b = 28, 32, 40
                else:
                    r = int(cr * shade)
                    g = int(cg * shade)
                    b = int(cb * shade)
                i = (y * w + x) * 4
                img[i : i + 4] = bytes((r, g, b, 255))
    write_png(os.path.join(ROOT, "car_body_atlas.png"), w, h, bytes(img))


def main() -> None:
    os.makedirs(ROOT, exist_ok=True)
    gen_asphalt()
    gen_ground()
    gen_props()
    gen_car_atlas()
    print("Wrote enhanced textures to", os.path.abspath(ROOT))


if __name__ == "__main__":
    main()
