#!/usr/bin/env python3
"""sweep.py -- run the readers over every logic file in ONE process.

`genfile.py` is the right tool for a file's first pass: it writes the header,
runs all three readers wide, and creates the file. For an INCREMENTAL sweep --
after a reader learns something new, or after a batch of hand-reading changes
what is left -- it is the wrong shape. It spawns a subprocess per reader per
file, and each one rebuilds the symbol table and rescans the whole decomp tree
to find out what is already written. Fourteen files cost forty-two full scans.

This does the same work in one process, with `--max` small, and appends what
the readers prove. It exists because twenty-three functions `pushfn` accepts
sat unwritten for want of a cheap way to ask it again.

    python tools/sweep.py [--max 128] [--file mkzap.c]
"""

import contextlib
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import dumpfn
import leaffn

BANNER = ("\n\n/* " + "-" * 68 + "\n"
          " * Added by a later sweep -- tools/sweep.py, running the same\n"
          " * readers again after one of them learned something. Each still\n"
          " * refuses anything it cannot account for instruction by\n"
          " * instruction; see tools/pushfn.py and tools/leaffn.py.\n"
          " * " + "-" * 68 + " */\n\n")


def emit(mod, src, cap):
    """One reader's --emit output, captured rather than piped."""
    buf = io.StringIO()
    argv = ["x", "--file", src, "--max", str(cap), "--emit"]
    try:
        with contextlib.redirect_stdout(buf):
            mod.main(argv)
    except SystemExit:
        pass
    return buf.getvalue()


def main(argv):
    import parkfn
    import pushfn

    cap = int(argv[argv.index("--max") + 1]) if "--max" in argv else 128
    only = argv[argv.index("--file") + 1] if "--file" in argv else None

    rows = dumpfn.table()
    srcs = set()
    for a, s, p in rows:
        if "gamecode/logic" in p.replace("\\", "/"):
            srcs.add(os.path.basename(p))

    hdr = io.open("src/decomp/gamecode/logic/mk3logic.h", encoding="utf-8").read()
    total = 0
    for src in sorted(srcs):
        if only and src != only:
            continue
        out = "src/decomp/gamecode/logic/" + src
        if not os.path.exists(out):
            continue

        dumpfn.written(refresh=True)
        push = emit(pushfn, src, cap)
        park = emit(parkfn, src, cap)
        leaf = emit(leaffn, src, min(cap, 96))

        body = ""
        taken = set()
        for chunk in (push, park, leaf):
            kept = []
            # Split on a blank line FOLLOWED BY a comment opener, not on any
            # blank line. parkfn's functions have blank lines inside them, and
            # splitting on those cut functions in half and then dropped the
            # halves as duplicates -- which compiled to nothing but errors.
            for block in re.split(r"\n\n+(?=/\*)", chunk):
                m = re.search(r"^(?:long|void) (\w+)\(", block, re.M)
                if m and m.group(1) in taken:
                    continue
                if m:
                    taken.add(m.group(1))
                kept.append(block)
            body += "\n\n".join(kept).rstrip("\n") + "\n\n"

        n = len(set(re.findall(r"^(?:long|void) (\w+)\(\w", body, re.M)))
        if not n:
            continue

        prior = io.open(out, encoding="utf-8").read()
        known = set(re.findall(r"^(?:long|void) (\w+)\(", prior + hdr, re.M))
        keep = []
        for line in body.splitlines():
            m = re.match(r"^(?:long|void) (\w+)\(.*\);$", line)
            if m and m.group(1) in known:
                continue
            keep.append(line)
        body = "\n".join(keep)

        # A definition arriving now corrects an earlier reader's guessed
        # return type in place; the callers above it still need a declaration.
        defs = dict((m[1], m[0]) for m in
                    re.findall(r"^(long|void) (\w+)\([^;]*$", body, re.M))
        lines = []
        for line in prior.splitlines():
            m = re.match(r"^(long|void) (\w+)\((.*)\);$", line)
            if m and m.group(2) in defs and defs[m.group(2)] != m.group(1):
                line = "%s %s(%s);" % (defs[m.group(2)], m.group(2), m.group(3))
            lines.append(line)

        io.open(out, "w", encoding="utf-8", newline="").write(
            "\n".join(lines).rstrip("\n") + BANNER + body.strip("\n") + "\n")
        total += n
        print("  %-14s %d" % (src, n))

    print("TOTAL:", total)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
