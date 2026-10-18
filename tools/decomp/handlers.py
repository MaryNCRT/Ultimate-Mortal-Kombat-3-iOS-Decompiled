#!/usr/bin/env python3
"""handlers.py -- summarise every arm of a large jump-table dispatch.

A 6 KB function that is one switch with sixty-nine arms is not readable top to
bottom, and reading each arm blind wastes most of the effort on the arms that do
nothing. This walks the jump table, follows each arm to its `break`, and prints
the calls and globals it touches -- a one-line summary per case:

    case 4 sub 22  @000746d0  [Health] [GameMode] [DestinyNames] [Destiny]
                              [Fatality] EASDK_LogEventEnumEnumString
                              [FatalityMessage]

That is enough to decide which arms deserve an instruction-level read and which
are three stores. It is a MAP, not a decompilation: a summary here is a reason
to go and read the arm, never a substitute for having read it.

    python tools/disasm_range.py work/UMK3.armv7 0x732a8 0x74c9c > ev.txt
    python tools/lits.py ev.txt > evl.txt
    python tools/handlers.py evl.txt --table 0x733c0:69:1 --tail 0x73388

`--table BASE:COUNT:FIRST` describes one inline `.word`/`b.w` table -- the
address of its first entry, how many entries, and the case value the first
entry stands for. Repeat it for each table. `--tail` is the address every arm
branches back to, which is how an arm's end is found; entries pointing at it are
the dead cases and are reported as a count, not listed.
"""

import re
import struct
import sys

VM_BIAS = 0x1000


def load_symbols(path="work/symbols.txt"):
    sym = {}
    try:
        fh = open(path, encoding="utf-8", errors="replace")
    except OSError:
        return sym
    with fh:
        for line in fh:
            p = line.split()
            if len(p) < 3 or not p[0].startswith("0x"):
                continue
            try:
                a = int(p[0], 16)
            except ValueError:
                continue
            if a and p[-1].startswith("_") and "," not in p[-1]:
                sym.setdefault(a, p[-1])
    return sym


def demangle(name):
    m = re.match(r"^__Z(\d+)(.*)$", name)
    return m.group(2)[:int(m.group(1))] if m else name.lstrip("_")


def branch_target(data, va):
    """Decode a Thumb-2 unconditional b.w at `va`."""
    hw1, hw2 = struct.unpack_from("<HH", data, va - VM_BIAS)
    S = (hw1 >> 10) & 1
    imm10 = hw1 & 0x3FF
    J1 = (hw2 >> 13) & 1
    J2 = (hw2 >> 11) & 1
    imm11 = hw2 & 0x7FF
    I1 = (~(J1 ^ S)) & 1
    I2 = (~(J2 ^ S)) & 1
    off = (S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
    if S:
        off -= 1 << 25
    return va + 4 + off


def main(argv):
    if len(argv) < 2:
        sys.exit("usage: handlers.py <annotated-dump> "
                 "--table BASE:COUNT:FIRST [--table ...] --tail ADDR "
                 "[--binary FILE]")

    dump = argv[1]
    tables, tail, binary = [], None, "work/UMK3.armv7"
    i = 2
    while i < len(argv):
        if argv[i] == "--table":
            b, c, f = argv[i + 1].split(":")
            tables.append((int(b, 16), int(c), int(f)))
            i += 2
        elif argv[i] == "--tail":
            tail = int(argv[i + 1], 16)
            i += 2
        elif argv[i] == "--binary":
            binary = argv[i + 1]
            i += 2
        else:
            sys.exit("unknown option %s" % argv[i])
    if not tables or tail is None:
        sys.exit("--table and --tail are both required")

    data = open(binary, "rb").read()
    sym = load_symbols()

    def slotname(a):
        try:
            v = struct.unpack_from("<I", data, a - VM_BIAS)[0]
        except Exception:
            return None
        return sym.get(v)

    lines = [l.rstrip("\n") for l in open(dump, encoding="utf-8", errors="replace")]
    index = {}
    for n, line in enumerate(lines):
        m = re.match(r"^(0x[0-9a-f]+)", line)
        if m:
            index[int(m.group(1), 16)] = n

    arms, dead = [], 0
    entries = set()
    for base, count, first in tables:
        for k in range(count):
            t = branch_target(data, base + k * 4)
            if t == tail:
                dead += 1
            else:
                arms.append((base, k + first, t))
                entries.add(t)

    def summarise(start, limit=90):
        out = []
        n = index.get(start)
        if n is None:
            return ["<not in dump>"]
        seen = 0
        while n < len(lines) and seen < limit:
            line = lines[n]
            m = re.match(r"^(0x[0-9a-f]+)\s+(.*)$", line)
            if m:
                a, body = int(m.group(1), 16), m.group(2)
                if a != start and a in entries:
                    break
                c = re.match(r"bl\s+#(0x[0-9a-f]+)", body)
                if c:
                    out.append(demangle(sym.get(int(c.group(1), 16),
                                                "?" + c.group(1))))
                if re.match(r"b(\.w)?\s+#0x%x\b" % tail, body) or body.startswith("pop"):
                    break
                seen += 1
            else:
                d = re.search(r"; -> (0x[0-9a-f]+)(\s+(_\S+))?", line)
                if d:
                    nm = d.group(3) or slotname(int(d.group(1), 16)) or d.group(1)
                    out.append("[" + nm.lstrip("_") + "]")
            n += 1
        return out

    for base, case, target in sorted(arms, key=lambda x: (x[0], x[1])):
        print("table %08x case %-3d @%08x  %s"
              % (base, case, target, " ".join(summarise(target))))
    print("\n%d live arms, %d dead" % (len(arms), dead))


if __name__ == "__main__":
    main(sys.argv)
