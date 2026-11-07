"""Draw Clear Skies' icon (a sky tile with a sun and a cloud) into src/clearskies.ico. No libraries needed.

    py -I tools/make_icon.py
"""
import math
import pathlib
import struct
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SKY_TOP, SKY_LOW = (0x2B, 0x7F, 0xD4), (0xA8, 0xD8, 0xF6)
SUN, CLOUD = (0xFF, 0xE9, 0xA8), (0xF7, 0xFB, 0xFF)


def rounded_box(x, y, r):  # signed distance to a unit square (0..1) with rounded corners of radius r
    qx, qy = abs(x - 0.5) - (0.5 - r), abs(y - 0.5) - (0.5 - r)
    return math.hypot(max(qx, 0), max(qy, 0)) + min(max(qx, qy), 0) - r


def cloud(x, y):  # distance-ish to a cloud made of circles and a flat bottom
    blobs = [(0.36, 0.70, 0.13), (0.52, 0.62, 0.17), (0.68, 0.70, 0.12)]
    d = min(math.hypot(x - cx, y - cy) - r for cx, cy, r in blobs)
    if 0.36 <= x <= 0.68 and 0.66 <= y <= 0.83:
        d = min(d, -0.01)
    return d


def pixel(x, y):
    """RGBA at (x, y) in 0..1 (no anti-aliasing: the caller supersamples)."""
    if rounded_box(x, y, 0.22) > 0:
        return (0, 0, 0, 0)
    t = y
    c = tuple(a + (b - a) * t for a, b in zip(SKY_TOP, SKY_LOW))
    ds = math.hypot(x - 0.66, y - 0.36)
    if ds < 0.32:  # glow
        g = (1 - ds / 0.32) ** 2 * 0.55
        c = tuple(a + (b - a) * g for a, b in zip(c, SUN))
    if ds < 0.17:
        c = SUN
    if cloud(x, y) < 0:
        c = CLOUD
    return (*c, 255)


def render(size, ss=4):
    rows = []
    for py in range(size):
        row = bytearray()
        for px in range(size):
            acc = [0.0] * 4
            for sy in range(ss):
                for sx in range(ss):
                    r, g, b, a = pixel((px + (sx + 0.5) / ss) / size, (py + (sy + 0.5) / ss) / size)
                    acc[0] += r * a; acc[1] += g * a; acc[2] += b * a; acc[3] += a
            a = acc[3] / (ss * ss)
            if a:
                row += bytes((round(acc[0] / acc[3]), round(acc[1] / acc[3]), round(acc[2] / acc[3]), round(a)))
            else:
                row += b"\0\0\0\0"
        rows.append(bytes(row))
    return rows


def png(size, rows):
    raw = b"".join(b"\0" + r for r in rows)
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    sizes = [256, 64, 48, 32, 24, 16]
    images = [png(s, render(s, 2 if s >= 128 else 4)) for s in sizes]
    head = struct.pack("<HHH", 0, 1, len(images))
    off = 6 + 16 * len(images)
    entries = b""
    for s, data in zip(sizes, images):
        entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(data), off)
        off += len(data)
    out = ROOT / "src" / "clearskies.ico"
    out.write_bytes(head + entries + b"".join(images))
    (ROOT / "build").mkdir(exist_ok=True)
    (ROOT / "build" / "icon-preview.png").write_bytes(images[0])
    print(f"make_icon: {out.relative_to(ROOT)}")


main()
