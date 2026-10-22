"""rollback_pagediff.py -- which variables two runs of one exe disagree on.

    python tools/rollback_pagediff.py <exe> <a.pages> <b.pages> [llvm-pdbutil]

The .pages files come from UMK3_REPLAY_PAGES (src/runtime/rollback_probe.c):
every 120 ticks, a hash of each 4 KB page of .umkstd, .umkstb and the arena.
Pages that differ at the first snapshot are the baseline (addresses, what the
menus left behind); pages that start to differ later are what diverged,
named from the exe's PDB with llvm-pdbutil (in the llvm-mingw toolchain).
"""
import bisect
import os
import re
import struct
import subprocess
import sys

REGIONS = (".umkstd", ".umkstb", "arena")


def read_pages(path):
    snaps = []
    with open(path, "rb") as f:
        while True:
            b = f.read(4)
            if len(b) < 4:
                break
            tick = struct.unpack("<I", b)[0]
            regs = []
            for _ in REGIONS:
                n = struct.unpack("<I", f.read(4))[0]
                regs.append(struct.unpack("<%dQ" % n, f.read(8 * n)))
            snaps.append((tick, regs))
    return snaps


def sections(exe):
    d = open(exe, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    n = struct.unpack_from("<H", d, pe + 6)[0]
    opt = struct.unpack_from("<H", d, pe + 20)[0]
    out = {}
    for i in range(n):
        o = pe + 24 + opt + 40 * i
        out[d[o:o + 8].rstrip(b"\0").decode("latin1")] = i + 1   # PDB counts from 1
    return out


def symbols(exe, pdbutil):
    pdb = os.path.splitext(exe)[0] + ".pdb"
    txt = subprocess.run([pdbutil, "dump", "--globals", pdb],
                         capture_output=True, text=True).stdout
    syms, name = {}, None
    for line in txt.splitlines():
        m = re.search(r"S_[LG]DATA32 \[size = \d+\] `([^`]+)`", line)
        if m:
            name = m.group(1)
            continue
        m = re.search(r"addr = (\d+):(\d+)", line)
        if m and name:
            syms.setdefault(int(m.group(1)), []).append((int(m.group(2)), name))
            name = None
    for s in syms.values():
        s.sort()
    return syms


def names(syms, sec, off):
    """The symbols that overlap the page at `off` of section `sec`."""
    lst = syms.get(sec, [])
    keys = [a for a, _ in lst]
    i = max(bisect.bisect_right(keys, off) - 1, 0)
    out = []
    while i < len(lst) and lst[i][0] < off + 4096:
        a, n = lst[i]
        end = lst[i + 1][0] if i + 1 < len(lst) else a + 1
        if end > off:
            out.append("%s+0x%x" % (n, off - a) if a < off else n)
        i += 1
    return out


def label(syms, sec, r, p):
    if r == 2:
        return ""
    return " ".join(names(syms, sec.get(REGIONS[r]), p * 4096)[:8])


def main():
    exe, pa, pb = sys.argv[1:4]
    pdbutil = sys.argv[4] if len(sys.argv) > 4 else "llvm-pdbutil"
    a, b = read_pages(pa), read_pages(pb)
    sec, syms = sections(exe), symbols(exe, pdbutil)
    base = None
    for (ta, ra), (tb, rb) in zip(a, b):
        if ta != tb:
            print("tick mismatch", ta, tb)
            break
        diff = set()
        for r in range(3):
            for p, (x, y) in enumerate(zip(ra[r], rb[r])):
                if x != y:
                    diff.add((r, p))
            for p in range(min(len(ra[r]), len(rb[r])), max(len(ra[r]), len(rb[r]))):
                diff.add((r, p))
        if base is None:
            base = diff
            print("tick %d: %d pages differ from the start (baseline)" % (ta, len(diff)))
            for r, p in sorted(diff)[:60]:
                print("   base %-7s +0x%08x %s" % (REGIONS[r], p * 4096, label(syms, sec, r, p)))
            continue
        new = sorted(diff - base)
        print("tick %d: %d pages differ, %d new" % (ta, len(diff), len(new)))
        for r, p in new[:60]:
            print("   NEW  %-7s +0x%08x %s" % (REGIONS[r], p * 4096, label(syms, sec, r, p)))
        base |= diff


if __name__ == "__main__":
    main()
