#!/usr/bin/env python3
"""lits.py -- resolve the pc-relative literal loads in a disassembly dump.

    python tools/disasm_range.py work/UMK3.armv7 0x282dc 0x2b00c > hud.txt
    python tools/lits.py hud.txt > hud-annotated.txt

Thumb reaches a constant through the literal pool, so the disassembly shows
`vldr s14, [pc, #0x360]` where the source said `13.0f`. The pool address is
`((site + 4) & ~3) + imm` -- note the ALIGNMENT, which is the part that is easy
to get wrong. This appends the word at that address and its float reading:

    0x0002838e  vldr s14, [pc, #0x360]   ; [0x286f0]=0xbe2aaaab  -0.166667

Reading a large function without this means chasing every constant by hand, and
the constants are where the layout lives -- sprite sizes, UV extents, timer
rates. Values that look like nonsense floats (1.6e-39) are pc-relative
displacements to data, not numbers; `disasm_range.py` already resolves those.
"""

import re
import struct
import sys

VM_BIAS = 0x1000
LOAD = re.compile(r"^(0x[0-9a-f]+)\s+(?:vldr|ldr(?:\.w)?)\s+\S+,\s*\[pc,\s*#(-?(?:0x)?[0-9a-f]+)\]")


def main(argv):
    if len(argv) < 2:
        sys.exit("usage: lits.py <dump> [binary]")
    data = open(argv[2] if len(argv) > 2 else "work/UMK3.armv7", "rb").read()

    for line in open(argv[1], encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        m = LOAD.match(line)
        if m:
            site = int(m.group(1), 16)
            g = m.group(2)
            off = int(g, 16) if "x" in g else int(g)
            pool = ((site + 4) & ~3) + off
            try:
                v = struct.unpack_from("<I", data, pool - VM_BIAS)[0]
                f = struct.unpack("<f", struct.pack("<I", v))[0]
                line += "   ; [0x%x]=0x%08x  %g" % (pool, v, f)
            except Exception:
                pass
        print(line)


if __name__ == "__main__":
    main(sys.argv)
