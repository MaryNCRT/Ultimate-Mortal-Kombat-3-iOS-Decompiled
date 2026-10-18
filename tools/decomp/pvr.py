"""
Reader and validator for the game's .pvr textures.

The container is the **legacy PVR v2** header (52 bytes, `PVR!` tag), not the
v3 format modern tooling defaults to. See docs/PVR-FORMAT.md.

This deliberately stops short of decoding pixels. What it does is establish the
**block geometry**, which is the foundation any decoder stands on, and it does
so to the standard this project uses for formats: the arithmetic has to come
out exact across the whole corpus, not approximately right on most of it.

PVRTC1 packs pixels into 8-byte blocks:

    4bpp:  4x4 pixels per block
    2bpp:  8x4 pixels per block

so a texture's compressed payload should be exactly

    ceil(width / bw) * ceil(height / bh) * 8

and the header's own `dataLength` should agree. If it does for all 1,400 files,
the block layout is understood. If it does not, a decoder built on it would be
wrong in a way that is very hard to see in an image.

Usage:
  python pvr.py validate <app dir>
  python pvr.py info     <file.pvr>
"""

import glob
import os
import struct
import sys

HEADER_SIZE = 52
PVR_TAG = b"PVR!"

# The pixel format lives in the low byte of `flags`.
FORMATS = {
    0x10: ("RGBA_4444", None),
    0x11: ("RGBA_5551", None),
    0x12: ("RGBA_8888", None),
    0x13: ("RGB_565", None),
    0x14: ("RGB_555", None),
    0x15: ("RGB_888", None),
    0x16: ("I_8", None),
    0x17: ("AI_88", None),
    0x18: ("PVRTC 2bpp", (8, 4)),     # block covers 8x4 pixels
    0x19: ("PVRTC 4bpp", (4, 4)),     # block covers 4x4 pixels
    0x1A: ("BGRA_8888", None),
}

BLOCK_BYTES = 8


class PVR(object):
    __slots__ = ("width", "height", "num_mipmaps", "flags", "data_length",
                 "bits_per_pixel", "has_alpha", "num_surfaces", "format_id",
                 "format_name", "block_size", "data")

    def __init__(self, raw):
        if len(raw) < HEADER_SIZE:
            raise ValueError("shorter than a header (%d bytes)" % len(raw))
        (hlen, self.height, self.width, self.num_mipmaps, self.flags,
         self.data_length, self.bits_per_pixel, _br, _bg, _bb, balpha,
         tag, self.num_surfaces) = struct.unpack_from("<13I", raw, 0)

        if struct.pack("<I", tag) != PVR_TAG:
            raise ValueError("bad tag %r, not a PVR v2 file"
                             % struct.pack("<I", tag))
        if hlen != HEADER_SIZE:
            raise ValueError("header length %d, expected %d" % (hlen, HEADER_SIZE))

        self.has_alpha = bool(balpha)
        self.format_id = self.flags & 0xFF
        self.format_name, self.block_size = FORMATS.get(
            self.format_id, ("unknown 0x%02x" % self.format_id, None))
        self.data = raw[HEADER_SIZE:]

    @property
    def is_pvrtc(self):
        return self.block_size is not None

    def expected_data_length(self):
        """Compressed payload implied by the block geometry, or None."""
        if not self.is_pvrtc:
            return None
        bw, bh = self.block_size
        blocks_x = (self.width + bw - 1) // bw
        blocks_y = (self.height + bh - 1) // bh
        return blocks_x * blocks_y * BLOCK_BYTES

    def blocks(self):
        """(blocks_x, blocks_y) or None."""
        if not self.is_pvrtc:
            return None
        bw, bh = self.block_size
        return ((self.width + bw - 1) // bw, (self.height + bh - 1) // bh)


def load(path):
    with open(path, "rb") as f:
        return PVR(f.read())


def _find(app_dir):
    out = []
    for pat in ("**/*.pvr", "**/*.PVR"):
        out.extend(glob.glob(os.path.join(app_dir, pat), recursive=True))
    return sorted(set(out))


def validate(app_dir):
    files = _find(app_dir)
    if not files:
        print("no .pvr files under %s" % app_dir)
        return False

    ok = header_ok = 0
    bad = []
    fmt_counts = {}
    mip_levels = set()
    non_pow2 = []
    payload_short = []

    for fn in files:
        name = os.path.basename(fn)
        try:
            t = load(fn)
        except ValueError as e:
            bad.append((name, str(e)))
            continue
        header_ok += 1
        fmt_counts[t.format_name] = fmt_counts.get(t.format_name, 0) + 1
        mip_levels.add(t.num_mipmaps)
        if (t.width & (t.width - 1)) or (t.height & (t.height - 1)):
            non_pow2.append(name)

        expected = t.expected_data_length()
        if expected is None:
            bad.append((name, "not a PVRTC format: %s" % t.format_name))
            continue
        if expected != t.data_length:
            bad.append((name, "dataLength %d, block geometry implies %d"
                        % (t.data_length, expected)))
            continue
        # the file must actually carry the payload it declares
        if len(t.data) < t.data_length:
            payload_short.append((name, len(t.data), t.data_length))
            continue
        ok += 1

    print("files found:              %5d" % len(files))
    print("headers parsed:           %5d" % header_ok)
    print("block geometry exact:     %5d" % ok)
    print("mismatches:               %5d" % len(bad))
    for name, why in bad[:20]:
        print("   %-40s %s" % (name, why))
    if payload_short:
        print("payload shorter than declared: %d" % len(payload_short))
        for name, got, want in payload_short[:10]:
            print("   %-40s %d bytes, header says %d" % (name, got, want))

    print("")
    print("pixel formats:")
    for f, c in sorted(fmt_counts.items(), key=lambda kv: -kv[1]):
        print("   %-14s %5d" % (f, c))
    print("mipmap levels seen:       %s" % sorted(mip_levels))
    print("non-power-of-two:         %5d" % len(non_pow2))

    return ok == len(files) and not bad


def info(path):
    t = load(path)
    bx, by = t.blocks() or (0, 0)
    print("%s" % os.path.basename(path))
    print("  %dx%d  %s  %s" % (t.width, t.height, t.format_name,
                               "alpha" if t.has_alpha else "no alpha"))
    print("  mipmaps      %d" % t.num_mipmaps)
    print("  surfaces     %d" % t.num_surfaces)
    print("  blocks       %d x %d  (%d bytes each)" % (bx, by, BLOCK_BYTES))
    print("  dataLength   %d   geometry implies %d   %s"
          % (t.data_length, t.expected_data_length() or 0,
             "match" if t.data_length == t.expected_data_length() else "MISMATCH"))
    print("  bytes present %d" % len(t.data))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    cmd, path = sys.argv[1], sys.argv[2]
    if cmd == "validate":
        sys.exit(0 if validate(path) else 1)
    elif cmd == "info":
        info(path)
    else:
        print(__doc__)
        sys.exit(1)
