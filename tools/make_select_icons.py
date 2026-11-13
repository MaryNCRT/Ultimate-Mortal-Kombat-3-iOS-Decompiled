"""Draws the select screen's OUTFIT and COLOR icons (port, Mary 2026-11-13):
white shapes on a clear background, as the game's menus draw their pad
pictures. Writes launcher/aditional textures/ui/outfit.png and color.png.

    python tools/make_select_icons.py
"""
import math
import os
import struct
import zlib

N = 128                                 # the picture's size
SS = 4                                  # samples per pixel, each way


def inside_poly(x, y, pts):
    c = False
    j = len(pts) - 1
    for i in range(len(pts)):
        xi, yi = pts[i]
        xj, yj = pts[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            c = not c
        j = i
    return c


def outfit(x, y):
    """A fighter's gi: the jacket with its sleeves out, a V at the collar,
    the belt a gap across the waist."""
    body = [(0.37, 0.10), (0.50, 0.34), (0.63, 0.10), (0.84, 0.17), (1.00, 0.46),
            (0.85, 0.55), (0.75, 0.38), (0.76, 0.93), (0.24, 0.93), (0.25, 0.38),
            (0.15, 0.55), (0.00, 0.46), (0.16, 0.17)]
    if not inside_poly(x, y, body):
        return False
    if 0.60 < y < 0.66:                 # the belt
        return False
    if abs(x - 0.5) < 0.012 and 0.34 < y < 0.60:   # the lapels' line
        return False
    return True


def color(x, y):
    """A painter's palette: a round board, the thumb's hole, three dabs cut
    out."""
    cx, cy = 0.5, 0.52
    if ((x - cx) / 0.47) ** 2 + ((y - cy) / 0.40) ** 2 > 1.0:
        return False
    if (x - 0.66) ** 2 + (y - 0.70) ** 2 < 0.085 ** 2:    # the thumb's hole
        return False
    if (x - 0.80) ** 2 + (y - 0.86) ** 2 < 0.13 ** 2:     # the notch
        return False
    for dx, dy in ((0.28, 0.42), (0.45, 0.28), (0.65, 0.33), (0.30, 0.64)):
        if (x - dx) ** 2 + (y - dy) ** 2 < 0.065 ** 2:
            return False
    return True


def render(fn, path):
    rows = []
    for py in range(N):
        row = bytearray([0])
        for px in range(N):
            hits = 0
            for sy in range(SS):
                for sx in range(SS):
                    x = (px + (sx + 0.5) / SS) / N
                    y = (py + (sy + 0.5) / SS) / N
                    # a margin, so the shape never touches the edge
                    if fn((x - 0.06) / 0.88, (y - 0.06) / 0.88):
                        hits += 1
            a = round(255 * hits / (SS * SS))
            row += bytes((255, 255, 255, a))
        rows.append(bytes(row))
    raw = b''.join(rows)

    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)

    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', N, N, 8, 6, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(raw, 9))
           + chunk(b'IEND', b''))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, 'wb').write(png)
    print('wrote', path)


if __name__ == '__main__':
    out = os.path.join(os.path.dirname(__file__), '..', 'launcher', 'aditional textures', 'ui')
    render(outfit, os.path.join(out, 'outfit.png'))
    render(color, os.path.join(out, 'color.png'))
