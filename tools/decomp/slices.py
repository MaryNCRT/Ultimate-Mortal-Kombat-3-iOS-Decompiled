"""
Work with both slices of the fat binary, and find the functions where the
armv6 build is readable and the armv7 build is not.

Why this exists
---------------

The armv7 slice is compiled for a CPU with NEON, and EA's compiler used
**2-lane packed NEON to do scalar float maths**: `vmul.f32 d6, d6, d6` where
d6 aliases s12/s13. Ghidra models those as opaque vector intrinsics and loses
the arithmetic, so its output for the affected functions is wrong in a way that
still compiles -- see docs/METHODOLOGY.md.

ARMv6 has no NEON. The armv6 slice is therefore a **second, independent
compilation of the same source** in plain scalar VFP, which Ghidra decompiles
correctly. `_Len` is the clearest example:

    armv7                            armv6
    vldr      s12, [r0]              vldr      s15, [r0, #4]
    vldr      s14, [r0, #4]          vldr      s13, [r0]
    vmul.f32  d6, d6, d6             vldr      s14, [r0, #8]
    vmul.f32  d7, d7, d7             vmul.f32  s15, s15, s15
    vldr      s10, [r0, #8]          vmla.f32  s15, s13, s13
    vadd.f32  d6, d6, d7             vmla.f32  s15, s14, s14
    vmul.f32  d7, d5, d5             vsqrt.f32 s15, s15
    vadd.f32  d7, d6, d7             vmov      r0, s15
    vsqrt.f32 s14, s14               bx        lr
    vmov      r0, s14
    bx        lr

The armv6 version is literally `sqrtf(x*x + y*y + z*z)`.

Note that armv6 also builds many functions as ARM rather than Thumb, and the
Thumb flag lives in `n_desc` bit 3 (`N_ARM_THUMB_DEF`) rather than in bit 0 of
the symbol value. Getting that wrong disassembles everything to garbage.

Usage:
  python slices.py extract <fat binary> <outdir>
  python slices.py neon    <armv7 slice> <armv6 slice> [func-to-file.txt]
"""

import collections
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from macho import MachO, read_file          # noqa: E402

try:
    from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
except ImportError:
    raise SystemExit("capstone is required: pip install capstone")

SUBTYPE = {6: "armv6", 9: "armv7", 11: "armv7s"}
TEXT_VMBASE = 0x1000

# Packed single-precision SIMD: a .f32 operation on a D or Q register.
# That is the pattern Ghidra mishandles. A .f32 op on S registers is plain
# scalar VFP and decompiles fine; a .f64 op on a D register is scalar double,
# also fine.
_DQ = re.compile(r"\b[dq]\d+\b")


def extract(fat_path, outdir):
    d = read_file(fat_path)
    magic, n = struct.unpack_from(">II", d, 0)
    if magic != 0xCAFEBABE:
        raise SystemExit("not a fat binary: magic=0x%08x" % magic)
    os.makedirs(outdir, exist_ok=True)
    for i in range(n):
        _cpu, sub, off, size, _align = struct.unpack_from(">IIIII", d, 8 + i * 20)
        name = SUBTYPE.get(sub, "sub%d" % sub)
        out = os.path.join(outdir, "UMK3.%s" % name)
        with open(out, "wb") as f:
            f.write(d[off:off + size])
        print("%-8s offset=0x%08x  %9d bytes  -> %s" % (name, off, size, out))


def _functions(path):
    """{addr: (name, is_thumb)} for every named function in a thin slice.

    The Thumb flag is OR-ed across every symbol table entry for an address:
    this binary carries N_ARM_THUMB_DEF on the STABS entry (type 30) and not
    on the plain one (type 36), so looking at only one of them silently
    disassembles Thumb code as ARM.
    """
    m = MachO(read_file(path), 0)
    thumb = {}
    name = {}
    for sym in m.symbols():
        if not isinstance(sym[0], str):
            continue
        n, typ, _sect, desc, val = sym
        if typ not in (30, 36) or not isinstance(val, int) or val == 0:
            continue
        if not n.startswith(("_", "+[", "-[")):
            continue
        thumb[val] = thumb.get(val, False) or bool(desc & 8)
        name.setdefault(val, n)
    return {a: (name[a], thumb[a]) for a in name}


def packed_neon(path, max_fn_bytes=8192):
    """{function name: count of packed .f32 SIMD instructions}."""
    data = read_file(path)
    fns = _functions(path)
    items = sorted(fns.items())
    out = {}
    for i, (addr, (nm, is_thumb)) in enumerate(items):
        end = items[i + 1][0] if i + 1 < len(items) else addr + 64
        size = min(end - addr, max_fn_bytes)
        if size <= 0:
            continue
        md = Cs(CS_ARCH_ARM, CS_MODE_THUMB if is_thumb else CS_MODE_ARM)
        hits = 0
        try:
            for ins in md.disasm(data[addr - TEXT_VMBASE:addr - TEXT_VMBASE + size],
                                 addr):
                if ".f32" in ins.mnemonic and _DQ.search(ins.op_str):
                    hits += 1
        except Exception:
            pass
        if hits:
            out[nm] = hits
    return out


def _file_map(path):
    m = {}
    if not path or not os.path.exists(path):
        return m
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 3 and parts[0].startswith("0x"):
                m[parts[1]] = parts[2].split("/")[-1]
    return m


def neon_report(v7, v6, f2f_path=None):
    a = packed_neon(v7)
    b = packed_neon(v6)
    only = sorted(set(a) - set(b), key=lambda n: -a[n])
    print("packed .f32 SIMD on D/Q registers")
    print("  armv7: %4d functions" % len(a))
    print("  armv6: %4d functions" % len(b))
    print("  armv7 only: %d  <- the armv6 slice gives these as scalar VFP" % len(only))

    f2f = _file_map(f2f_path)
    if f2f:
        by = collections.Counter(f2f.get(n, "(unattributed)") for n in only)
        print("\nby source file:")
        for fn, c in by.most_common(20):
            print("  %-28s %3d" % (fn, c))

    print("\nworst offenders (instruction count in armv7):")
    for n in only[:15]:
        print("  %-44s %3d   %s" % (n, a[n], f2f.get(n, "")))
    return only


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    cmd = sys.argv[1]
    if cmd == "extract" and len(sys.argv) >= 4:
        extract(sys.argv[2], sys.argv[3])
    elif cmd == "neon" and len(sys.argv) >= 4:
        neon_report(sys.argv[2], sys.argv[3],
                    sys.argv[4] if len(sys.argv) > 4 else None)
    else:
        print(__doc__)
        sys.exit(1)
