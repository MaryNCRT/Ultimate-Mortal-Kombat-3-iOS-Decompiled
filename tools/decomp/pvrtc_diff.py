"""
Differential test for the PVRTC decoder, against EA's own PNGs.

The game ships **38 textures twice** — as `NAME.PNG` and `NAME.pvr`. Where the
PNG really is the source the PVR was built from, that is a reference
implementation costing nothing and needing no external tool. It was in the
bundle the whole time.

**But not every pair is a pair.** Three of them are different assets that happen
to share a name, and taking them at face value cost several rounds of hunting a
decoder bug that was not there. They are listed in `INVALID` below with the
reason; each was confirmed by rendering the two images side by side and looking
at them.

Compares `tools/pvrtc.py` output against the PNG channel by channel and reports
the mean absolute difference. PVRTC is lossy, so a correct decoder does not
score zero — it scores low on smooth content and high at hard edges, because a
4x4 block blending two endpoint colours cannot represent an edge inside itself.
**That gradient correlation is what tells compression loss apart from a bug**;
see `--gradient`.

Usage:
  python pvrtc_diff.py                 compare, valid references only
  python pvrtc_diff.py --all           include the invalid ones, for the record
  python pvrtc_diff.py --gradient      error against local image gradient
"""

import glob
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pvr import load as load_pvr          # noqa: E402
from pvrtc import decode                  # noqa: E402

try:
    from PIL import Image, ImageFilter
except ImportError:
    raise SystemExit("Pillow is required: pip install Pillow")

TEXTURES = os.environ.get(
    "UMK3_TEXTURES",
    r"E:\MK3 PROJECT\WORK\stage\Payload\UMK3.app\res\Textures")

#: Pairs that share a name but are not the same image. Verified by eye.
INVALID = {
    "FE_METAL_BG": "the PNG frames the art differently - content fills only the "
                   "top ~60% of the square, the PVR fills it entirely",
    "MYBLOOD1":    "the PNG is the unprocessed source with a magenta chroma key; "
                   "the PVR holds what was actually encoded",
    "MYBLOOD2":    "same as MYBLOOD1",
}


def _asset(name):
    """The seven FE_MAINLOGO_* files are one artwork with different lettering,
    so they count as a single independent sample rather than seven."""
    return "FE_MAINLOGO" if name.startswith("FE_MAINLOGO") else name


def pairs():
    out = []
    for png in sorted(glob.glob(os.path.join(TEXTURES, "*.PNG"))):
        base = png[:-4]
        pvr = base + ".pvr"
        if not os.path.exists(pvr):
            continue
        tex = load_pvr(pvr)
        if Image.open(png).size != (tex.width, tex.height):
            continue                       # different asset, sizes disagree
        out.append((os.path.basename(base), pvr, png, tex))
    return out


def mean_error(tex, png):
    w, h, px = decode(tex)
    rp = Image.open(png).convert("RGBA").tobytes()
    total = 0
    for i in range(0, w * h * 4, 4):
        total += (abs(rp[i] - px[i]) + abs(rp[i + 1] - px[i + 1])
                  + abs(rp[i + 2] - px[i + 2]))
    return total / (w * h * 3.0)


def report(include_invalid=False):
    rows, skipped = [], []
    for name, _pvr, png, tex in pairs():
        if name in INVALID and not include_invalid:
            skipped.append(name)
            continue
        rows.append((name, tex.format_name, mean_error(tex, png)))
    rows.sort(key=lambda r: r[2])

    print("%-22s %-12s %s" % ("texture", "format", "mean error /255"))
    for n, f, e in rows:
        print("%-22s %-12s %8.2f%s"
              % (n, f, e, "   <- INVALID reference" if n in INVALID else ""))

    if skipped:
        print("\nexcluded as invalid references:")
        for n in skipped:
            print("  %-16s %s" % (n, INVALID[n]))

    by_asset = {}
    for n, f, e in rows:
        if n not in INVALID:
            by_asset.setdefault(_asset(n), []).append((f, e))

    print("\nper independent asset:")
    per = []
    for k, v in sorted(by_asset.items()):
        m = statistics.mean(e for _, e in v)
        per.append((v[0][0], m))
        print("  %-20s %-12s %6.2f   (n=%d)" % (k, v[0][0], m, len(v)))

    print("")
    for fmt in ("PVRTC 2bpp", "PVRTC 4bpp"):
        sel = [e for f, e in per if f == fmt]
        if sel:
            m = statistics.mean(sel)
            print("%-12s n=%d  mean=%5.2f  (%.1f%%)"
                  % (fmt, len(sel), m, 100 * m / 255))
    if per:
        m = statistics.mean(e for _, e in per)
        print("\noverall: %.2f / 255  (%.1f%%)" % (m, 100 * m / 255))
    return rows


def gradient(names=("FE_MAINLOGO_EN", "LIGHTNING", "VORTEX1")):
    """Error against local image gradient.

    This is the test that separates compression loss from a decode bug. Block
    compression fails where the image holds detail a block cannot, so error must
    rise with the gradient. A decode bug would track block *position* instead,
    which docs/PVR-FORMAT.md measures separately and finds flat.
    """
    for name in names:
        pvr = os.path.join(TEXTURES, name + ".pvr")
        png = os.path.join(TEXTURES, name + ".PNG")
        if not (os.path.exists(pvr) and os.path.exists(png)):
            continue
        tex = load_pvr(pvr)
        w, h, px = decode(tex)
        ref = Image.open(png).convert("RGBA")
        rp = ref.tobytes()
        edges = ref.convert("L").filter(ImageFilter.FIND_EDGES).tobytes()
        bins = {}
        for i in range(w * h):
            o = i * 4
            e = (abs(rp[o] - px[o]) + abs(rp[o + 1] - px[o + 1])
                 + abs(rp[o + 2] - px[o + 2])) / 3.0
            b = bins.setdefault(min(edges[i] // 32, 7), [0.0, 0])
            b[0] += e
            b[1] += 1
        print("=== %s (%s)" % (name, tex.format_name))
        print("   gradient      pixels   mean error")
        for k in sorted(bins):
            s, c = bins[k]
            print("   %3d-%3d  %10d  %10.2f" % (k * 32, k * 32 + 31, c, s / c))


if __name__ == "__main__":
    if "--gradient" in sys.argv:
        gradient()
    else:
        report(include_invalid="--all" in sys.argv)
