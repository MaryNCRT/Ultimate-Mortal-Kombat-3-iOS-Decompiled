"""
Inventory every GL entry point the engine calls, and where from.

The native port has to reimplement whatever this binary asks OpenGL ES 1.1 to
do. Guessing that list from screenshots is how ports acquire six months of
missing features; reading it off the import table and the call sites is exact.

Depends on `disasm.py` resolving import stubs, which is what makes calls to GL
show up as names rather than bare addresses at all.

Usage:
  python glsurface.py <slice> [func-to-file.txt]

Prints the entry points in use, how often each is called, and which source
files call it -- so "what does the renderer need" and "what does the UI need"
are separable questions.
"""

import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm                                          # noqa: E402


def scan(path, func_to_file=None):
    m, syms, byname = disasm.load(path)

    # where each function came from, so calls can be attributed to a module
    origin = {}
    if func_to_file and os.path.exists(func_to_file):
        for line in open(func_to_file, "r", errors="ignore"):
            parts = line.split()
            if len(parts) >= 3 and parts[0].startswith("0x"):
                origin[parts[1]] = parts[2].split("/")[-1]

    # every address that is a GL (or EGL) import stub
    gl_stubs = {}
    try:
        for addr, nm in m.stub_map().items():
            bare = nm.lstrip("_")
            if bare.startswith("gl") and len(bare) > 2 and bare[2].isupper():
                gl_stubs[addr] = bare
    except Exception:                                   # noqa: BLE001
        return {}, {}

    calls = collections.Counter()
    by_module = collections.defaultdict(collections.Counter)

    # Disassemble by NAME, never by address. Given a bare "0x..." target
    # disasm assumes Thumb, and the armv6 slice is ARM throughout -- the
    # result is silently garbage rather than an error, which is how the first
    # version of this tool reported one GL call in the whole binary.
    for nm, (addr, _thumb) in sorted(byname.items(), key=lambda kv: kv[1][0]):
        if addr in gl_stubs:
            continue
        try:
            text = disasm.disassemble(path, nm)
        except Exception:                               # noqa: BLE001
            continue
        module = origin.get(nm, "?")
        for line in text.splitlines():
            if "; -> " not in line:
                continue
            callee = line.split("; -> ")[1].strip().lstrip("_")
            if callee.startswith("gl") and len(callee) > 2 and callee[2].isupper():
                calls[callee] += 1
                by_module[module][callee] += 1
    return calls, by_module


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    path = argv[1]
    f2f = argv[2] if len(argv) > 2 else None
    calls, by_module = scan(path, f2f)
    if not calls:
        print("no GL stubs found -- is this the right slice?")
        return 1

    print("GL entry points in use: %d\n" % len(calls))
    for name, n in calls.most_common():
        print("  %-32s %4d call sites" % (name, n))

    if by_module:
        print("\nby source file:")
        for mod, c in sorted(by_module.items(),
                             key=lambda kv: -sum(kv[1].values())):
            if mod == "?":
                continue
            print("\n  %s  (%d calls, %d distinct)"
                  % (mod, sum(c.values()), len(c)))
            print("    " + ", ".join(n for n, _ in c.most_common()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
