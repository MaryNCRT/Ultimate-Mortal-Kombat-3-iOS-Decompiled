#!/usr/bin/env python3
"""
stubs.py -- resolve imported-function stub addresses to names.

Every `bl #0x127xxx` in the armv6 slice lands in the lazy-binding stub section,
so a raw disassembly of the render path is a wall of unnamed calls. This turns
them back into names.

    python tools/stubs.py OUTPUT/armv6/UMK3.armv6                 # dump all
    python tools/stubs.py OUTPUT/armv6/UMK3.armv6 0x127ae8 ...    # look up
    python tools/stubs.py OUTPUT/armv6/UMK3.armv6 --grep gl       # filter

733 stubs resolve in the retail armv6 slice, including all 77 GL ES entry
points the engine uses.

Why this matters more than it looks: an unresolved stub is not just a missing
name, it is an invitation to guess. This project recorded `IsTextureFullBright`
as using `strcmp` when it actually uses `strstr` -- a substring test, not an
equality test -- and that one wrong word made the `.???` wildcard in
`res/nolight.txt` look like an unsolved mystery for several sessions. Run this
before reading any function that calls into libc or GL.
"""
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "OUTPUT", "tools"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
import macho  # noqa: E402


def load(path):
    with open(path, "rb") as fh:
        return macho.MachO(fh.read()).stub_map()


def main(argv):
    if len(argv) < 2:
        print(__doc__.strip())
        return 1

    stubs = load(argv[1])
    args = argv[2:]

    if not args:
        for addr in sorted(stubs):
            print("0x%08x  %s" % (addr, stubs[addr]))
        print("\n%d stubs" % len(stubs), file=sys.stderr)
        return 0

    if args[0] == "--grep":
        needle = args[1].lower()
        hits = [(a, n) for a, n in sorted(stubs.items()) if needle in n.lower()]
        for addr, name in hits:
            print("0x%08x  %s" % (addr, name))
        print("\n%d of %d stubs match %r" % (len(hits), len(stubs), args[1]),
              file=sys.stderr)
        return 0

    missing = 0
    for token in args:
        addr = int(token, 16) if token.lower().startswith("0x") else int(token)
        name = stubs.get(addr)
        if name is None:
            missing += 1
            print("0x%08x  <not a stub>" % addr)
        else:
            print("0x%08x  %s" % (addr, name))
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
