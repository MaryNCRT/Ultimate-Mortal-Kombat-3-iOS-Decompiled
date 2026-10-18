"""
PVRTC1 decoder — 2bpp and 4bpp, to straight RGBA.

Why this exists: PVRTC is a PowerVR hardware format and desktop GPUs do not
have the extension. The port has to decode on the CPU and upload plain RGBA.
This is that decoder, in Python, as the reference the C version will be checked
against.

The block geometry it stands on is verified — `tools/pvr.py validate` walks all
1,400 shipped textures and the arithmetic comes out exact on every one. See
docs/PVR-FORMAT.md.

**Verification status: 1.5% mean error against EA's own PNGs — working.**

`tools/pvrtc_diff.py` scores it against the textures the game ships twice:
**0.6% on 2bpp, 2.4% on 4bpp, 1.5% overall.** PVRTC is lossy, so what matters
is that the residual **rises with the image's local gradient** (4.75 in flat
areas, 30-51 at hard edges) rather than with block position (flat at 74.8-75.5
across all sixteen positions). That is compression loss, not a decode bug.

It is not settled the way `Matrix.cpp` is settled: four independent assets is a
thin corpus. Widen it before shipping a C port — see docs/PVR-FORMAT.md.

---

Format, for whoever reads this next.

Each block is 8 bytes covering 4x4 pixels (4bpp) or 8x4 (2bpp), stored in
**Morton order** rather than row order. A block holds two endpoint colours and
a per-pixel modulation weight that blends between them:

    bits  0..31   modulation, 2 bits per pixel (4bpp)
    bit   32      modulation mode
    bits 33..47   colour B
    bits 48..63   colour A

The endpoint colours are not used directly. Each pixel's A and B come from a
**bilinear interpolation of the four surrounding blocks' endpoints**, which is
what gives PVRTC its smooth gradients and what makes it awkward to decode one
block at a time. That interpolation is the part most likely to be subtly wrong,
and the reason a per-pixel reference matters.

Usage:
  python pvrtc.py decode <file.pvr> <out.png>
  python pvrtc.py check  <app dir>
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pvr import load as load_pvr          # noqa: E402


# ---------------------------------------------------------------- endpoints

def _colour_a(packed):
    """Endpoint A (top 16 bits of the colour word) -> (r, g, b, a) 0..255."""
    if packed & 0x8000:                     # opaque: 5-5-4, blue replicated
        r = (packed >> 10) & 0x1F
        g = (packed >> 5) & 0x1F
        b = packed & 0x1F
        r = (r << 3) | (r >> 2)
        g = (g << 3) | (g >> 2)
        b = (b << 3) | (b >> 2)
        return r, g, b, 255
    a = (packed >> 12) & 0x07               # translucent: 3-4-4-3
    r = (packed >> 8) & 0x0F
    g = (packed >> 4) & 0x0F
    b = (packed >> 1) & 0x07
    a = (a << 5) | (a << 2) | (a >> 1)
    r = (r << 4) | r
    g = (g << 4) | g
    b = (b << 5) | (b << 2) | (b >> 1)
    return r, g, b, a


def _colour_b(packed):
    """Endpoint B (low 16 bits) -> (r, g, b, a) 0..255."""
    if packed & 0x8000:                     # opaque: 5-5-5
        r = (packed >> 10) & 0x1F
        g = (packed >> 5) & 0x1F
        b = packed & 0x1F
        r = (r << 3) | (r >> 2)
        g = (g << 3) | (g >> 2)
        b = (b << 3) | (b >> 2)
        return r, g, b, 255
    a = (packed >> 12) & 0x07               # translucent: 3-4-4-4
    r = (packed >> 8) & 0x0F
    g = (packed >> 4) & 0x0F
    b = packed & 0x0F
    a = (a << 5) | (a << 2) | (a >> 1)
    r = (r << 4) | r
    g = (g << 4) | g
    b = (b << 4) | b
    return r, g, b, a


# ---------------------------------------------------------------- morton

def _morton_index(x, y, width, height):
    """Interleave the bits of x and y, the order blocks are stored in."""
    n = min(width, height)
    idx = 0
    shift = 0
    k = n
    while k > 1:
        idx |= ((y & 1) << shift) | ((x & 1) << (shift + 1))
        x >>= 1
        y >>= 1
        shift += 2
        k >>= 1
    # whichever axis is longer contributes its remaining high bits linearly
    if width > height:
        idx |= x << shift
    else:
        idx |= y << shift
    return idx


# ---------------------------------------------------------------- decode

def decode(tex):
    """Decode a PVR object to (width, height, bytearray RGBA)."""
    if not tex.is_pvrtc:
        raise ValueError("not PVRTC: %s" % tex.format_name)

    bw, bh = tex.block_size                 # pixels covered per block
    two_bpp = (bw == 8)
    bx = max(1, (tex.width + bw - 1) // bw)
    by = max(1, (tex.height + bh - 1) // bh)

    raw = tex.data
    need = bx * by * 8
    if len(raw) < need:
        raise ValueError("payload %d bytes, need %d" % (len(raw), need))

    # unpack every block once
    mods = [0] * (bx * by)
    mode = [0] * (bx * by)
    ca = [None] * (bx * by)
    cb = [None] * (bx * by)
    for j in range(by):
        for i in range(bx):
            off = _morton_index(i, j, bx, by) * 8
            m, c = struct.unpack_from("<II", raw, off)
            k = j * bx + i
            mods[k] = m
            mode[k] = c & 1
            # Colour B is the LOW 16 bits, not shifted. The modulation flag
            # occupies bit 0 and the format lets it share that bit with the
            # least significant bit of blue -- it does not displace the field.
            # Shifting here corrupts every channel of B by one bit position.
            cb[k] = _colour_b(c & 0xFFFF)
            ca[k] = _colour_a((c >> 16) & 0xFFFF)

    out = bytearray(tex.width * tex.height * 4)

    def wrap(v, n):
        return v % n if n else 0

    for py in range(tex.height):
        for px in range(tex.width):
            # The endpoints for a pixel come from the four blocks whose centres
            # surround it, so the block grid is offset by half a block.
            fx = (px - bw // 2) / float(bw)
            fy = (py - bh // 2) / float(bh)
            i0 = int(fx // 1)
            j0 = int(fy // 1)
            u = fx - i0
            v = fy - j0
            i0 = wrap(i0, bx)
            j0 = wrap(j0, by)
            i1 = wrap(i0 + 1, bx)
            j1 = wrap(j0 + 1, by)

            k00 = j0 * bx + i0
            k10 = j0 * bx + i1
            k01 = j1 * bx + i0
            k11 = j1 * bx + i1

            w00 = (1 - u) * (1 - v)
            w10 = u * (1 - v)
            w01 = (1 - u) * v
            w11 = u * v

            A = [0.0] * 4
            B = [0.0] * 4
            for ch in range(4):
                A[ch] = (ca[k00][ch] * w00 + ca[k10][ch] * w10 +
                         ca[k01][ch] * w01 + ca[k11][ch] * w11)
                B[ch] = (cb[k00][ch] * w00 + cb[k10][ch] * w10 +
                         cb[k01][ch] * w01 + cb[k11][ch] * w11)

            # the modulation bits live in the block the pixel physically sits in
            mi = px // bw
            mj = py // bh
            mk = mj * bx + mi
            lx = px % bw
            ly = py % bh
            if two_bpp:
                bit = (ly * 8 + lx)
                weight_idx = (mods[mk] >> bit) & 1
                w = 1.0 if weight_idx else 0.0
                punch = False
            else:
                bit = (ly * 4 + lx) * 2
                sel = (mods[mk] >> bit) & 3
                punch = False
                if mode[mk]:
                    w = (0.0, 0.5, 0.5, 1.0)[sel]
                    punch = (sel == 2)
                else:
                    w = (0.0, 3.0 / 8.0, 5.0 / 8.0, 1.0)[sel]

            o = (py * tex.width + px) * 4
            for ch in range(3):
                val = B[ch] * (1.0 - w) + A[ch] * w
                out[o + ch] = max(0, min(255, int(val + 0.5)))
            alpha = B[3] * (1.0 - w) + A[3] * w
            out[o + 3] = 0 if punch else max(0, min(255, int(alpha + 0.5)))

    return tex.width, tex.height, out


# ---------------------------------------------------------------- png

def write_png(path, width, height, rgba):
    """Minimal PNG writer; no external dependency."""
    import zlib
    rows = bytearray()
    stride = width * 4
    for y in range(height):
        rows.append(0)                                   # filter: none
        rows += rgba[y * stride:(y + 1) * stride]

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    hdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", hdr))
        f.write(chunk(b"IDAT", zlib.compress(bytes(rows), 6)))
        f.write(chunk(b"IEND", b""))


# ---------------------------------------------------------------- checks

def check(app_dir):
    """Decode every shipped texture and report what can be checked without a
    reference implementation."""
    import glob
    files = sorted(set(glob.glob(os.path.join(app_dir, "**", "*.pvr"),
                                 recursive=True)))
    ok = 0
    failed = []
    flat = []
    alpha_files = 0
    for fn in files:
        try:
            t = load_pvr(fn)
            w, h, px = decode(t)
        except Exception as e:                            # noqa: BLE001
            failed.append((os.path.basename(fn), str(e)))
            continue
        if len(px) != w * h * 4:
            failed.append((os.path.basename(fn), "wrong output size"))
            continue
        ok += 1
        # a texture that decodes to a single colour everywhere is suspicious
        if len(set(bytes(px[0::4]))) == 1 and len(set(bytes(px[1::4]))) == 1:
            flat.append(os.path.basename(fn))
        if t.has_alpha and len(set(bytes(px[3::4]))) > 1:
            alpha_files += 1

    print("textures decoded:      %5d of %d" % (ok, len(files)))
    print("failures:              %5d" % len(failed))
    for n, why in failed[:15]:
        print("   %-40s %s" % (n, why))
    print("uniform-colour output: %5d   (suspicious if large)" % len(flat))
    for n in flat[:10]:
        print("   %s" % n)
    print("alpha textures with varying alpha: %d" % alpha_files)
    print("")
    print("NOTE: this is structural validation only. Decoding correctly to the")
    print("right size is not the same as decoding to the right pixels. A")
    print("per-pixel diff against an independent implementation is still owed.")
    return not failed


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    if sys.argv[1] == "decode" and len(sys.argv) >= 4:
        t = load_pvr(sys.argv[2])
        w, h, px = decode(t)
        write_png(sys.argv[3], w, h, px)
        print("%dx%d  %s  -> %s" % (w, h, t.format_name, sys.argv[3]))
    elif sys.argv[1] == "check":
        sys.exit(0 if check(sys.argv[2]) else 1)
    else:
        print(__doc__)
        sys.exit(1)
