#!/usr/bin/env python3
"""check_logic_tables.py -- verify the fight engine's generated tables against the binary.

    python tools/check_logic_tables.py <UMK3 binary> [--nm <nm output of a linked exe>]

Runs tools/logic_tables.py into a temporary file and checks what it wrote,
reading the binary independently of the generator's own reasoning. Exit status
0 means every check passed; anything else names what failed.

## What is checked

1. **Round trip.** Every emitted word is turned back into the value the binary
   holds -- an integer as written, a relocated word as the binary address of
   the symbol it names plus its offset -- and compared byte for byte with the
   image, over each object's extent. This proves the emission: sizes, order,
   offsets, nothing truncated, no relocation pointing at the wrong place.

2. **Where relocations go.** A function relocation must name a function start;
   a data relocation must go from __DATA to __DATA or __common. The one rule
   the generator applied wrongly, a __DATA word relocated into __TEXT,__const,
   is exactly this check.

3. **Declared types.** A table the decompiled code declares as an array of
   `int16_t`/`uint16_t` cannot hold a 32-bit pointer, so it must have no
   relocation. (`uint8_t` is not checked: the animation scripts are declared
   as byte arrays and do hold pointers.)

4. **Missed pointers.** A word left as an integer that equals a Thumb function
   start, the start of a string in __cstring, or the start of a data symbol is
   a pointer the rule may have missed. Each must be listed in the map as
   `number <symbol> <offset>`, with the reason it is a number.

5. **Layout** (with --nm). Two objects that touch in the image must touch on
   the host, at the same distance: a read past the end of one has to see the
   next, as on the device.

What this does not prove: that a word the rule called an integer and that
lands in the middle of some unrelated object is not a pointer. Those are the
generator report's near misses; 278 of them were read by hand on 2026-10-08
(packed halfword pairs, switch masks and ASCII landing in front-end storage)
and none is a pointer. A second, independently written generator
(tools/mklogicdata.py, PR #43, since removed in favour of this one) agreed on every one of the
35,577 words both emit except 53; all 53 were settled by their readers, and
the only one this generator had wrong is the `int` entry in the map.
"""

import bisect
import glob
import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import logic_tables  # noqa: E402
import macho         # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))


def fail(problems, msg):
    problems.append(msg)
    if len(problems) <= 40:
        print("  FAIL " + msg)


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    binpath = argv[1]
    nmpath = argv[argv.index("--nm") + 1] if "--nm" in argv else None

    tmp = tempfile.mkdtemp(prefix="umk3_logic_")
    out_c = os.path.join(tmp, "logic_tables.c")
    rep_p = os.path.join(tmp, "report.txt")
    subprocess.run([sys.executable, os.path.join(HERE, "logic_tables.py"),
                    binpath, out_c, "--report", rep_p], check=True)
    text = open(out_c, encoding="utf-8").read()

    data, base, m = logic_tables.load(binpath)

    def raw(a, n):
        s = m.section_for_addr(a)
        off = base + s.offset + (a - s.addr)
        return data[off:off + n]

    def word(a):
        return struct.unpack("<I", raw(a, 4))[0]

    # Every real symbol: name -> (addr, segment,section); sorted starts.
    syms, starts, fstarts = {}, set(), set()
    for name, t, sect, desc, val in m.symbols():
        if t & macho.N_STAB or (t & macho.N_TYPE) != macho.N_SECT:
            continue
        s = m.sections[sect - 1]
        syms.setdefault(logic_tables.c_name(name), (val, s.segname, s.sectname))
        syms.setdefault(name, (val, s.segname, s.sectname))
        starts.add(val)
        if s.segname == "__TEXT" and s.sectname == "__text":
            fstarts.add(val)
    starts = sorted(starts)

    native, alias, objects = logic_tables.read_map(
        os.path.join(HERE, "logic_tables.map"))
    numbers = set()
    for line in open(os.path.join(HERE, "logic_tables.map"), encoding="utf-8"):
        q = line.split("#", 1)[0].split()
        if q and q[0] == "number":
            numbers.add((q[1], int(q[2], 16)))

    # C name -> binary address, from the generator's own comments, which name
    # the binary symbol: checked against the symbol table, not trusted.
    problems = []
    cmap = {}
    defs = re.findall(r"/\* (\S+)\s+0x([0-9a-f]{8})\s+(\S+) \*/\s*\n"
                      r"(uintptr_t|unsigned char) (\w+)\[(\d+)\][^=]*= \{(.*?)\};",
                      text, re.S)
    for bname, addr, sect, ctype, cname, n, body in defs:
        a = int(addr, 16)
        if syms.get(bname, (None,))[0] != a:
            fail(problems, "%s: comment says 0x%x, symbol table disagrees" % (bname, a))
        cmap[cname] = a
    for ctype, cname, n, bname, addr in re.findall(
            r"(uintptr_t|unsigned char) (\w+)\[(\d+)\] UMK3_LOGIC[^=]*= \{0\};\s*/\* (\S+) 0x([0-9a-f]{8})",
            text):
        cmap[cname] = int(addr, 16)
    for cname, a in alias.items():
        cmap.setdefault(cname, a)
    for bname, cname in native.items():
        if bname in syms:
            cmap.setdefault(cname, syms[bname][0])

    def resolve(name):
        if name in cmap:
            return cmap[name], "data"
        if name in syms and syms[name][0] in fstarts:
            return syms[name][0] | 1, "fn"
        if "_" + name in syms and syms["_" + name][0] in fstarts:
            return syms["_" + name][0] | 1, "fn"
        return None, None

    def section_of(a):
        s = m.section_for_addr(a)
        return (s.segname, s.sectname) if s else ("?", "?")

    # 1 + 2: round trip and relocation targets.
    words = relocs = 0
    for bname, addr, sect, ctype, cname, n, body in defs:
        a0 = int(addr, 16)
        nxt = starts[bisect.bisect_right(starts, a0)]
        if ctype == "unsigned char":
            got = bytes(int(x, 0) for x in body.replace("\n", " ").split(",") if x.strip())
            if got != raw(a0, len(got)):
                fail(problems, "%s: bytes differ from the image" % bname)
            continue
        items = [x.strip() for x in re.findall(r"UMK3_REL\([^)]+\)|[^,\s]+", body) if x.strip()]
        out = b""
        for k, it in enumerate(items):
            words += 1
            mm = (re.match(r"UMK3_REL\((\w+),\s*(0x[0-9a-f]+|\d+)\)$", it)
                  or re.match(r"\(uintptr_t\)(\w+)(?: \+ (0x[0-9a-f]+))?$", it))
            if not mm:
                out += struct.pack("<I", int(it, 0) & 0xffffffff)
                continue
            relocs += 1
            tgt, kind = resolve(mm.group(1))
            if tgt is None:
                fail(problems, "%s+0x%x: %s does not resolve" % (bname, 4 * k, mm.group(1)))
                out += b"\0\0\0\0"
                continue
            v = (tgt + int(mm.group(2) or "0", 0)) & 0xffffffff
            out += struct.pack("<I", v)
            src_sec = section_of(a0)
            if kind == "fn":
                if (v & ~1) not in fstarts:
                    fail(problems, "%s+0x%x: function target 0x%x is not a function start"
                         % (bname, 4 * k, v))
            elif src_sec[0] != "__DATA" or section_of(v)[0] != "__DATA":
                fail(problems, "%s+0x%x: %s,%s -> %s,%s relocation"
                     % ((bname, 4 * k) + src_sec + section_of(v)))
        size = min(nxt - a0, len(out))
        if out[:size] != raw(a0, size):
            for k in range(0, size, 4):
                if out[k:k + 4] != raw(a0 + k, 4)[:size - k]:
                    fail(problems, "%s+0x%x: emitted 0x%s, image 0x%s"
                         % (bname, k, out[k:k + 4][::-1].hex(), raw(a0 + k, 4)[::-1].hex()))
                    break

    # 3: declared sub-word types hold no pointer.
    declared = {}
    for f in glob.glob(os.path.join(ROOT, "src", "decomp", "gamecode", "logic", "*.[ch]")):
        for mm in re.finditer(r"^\s*extern\s+(?:const\s+)?(u?int16_t|(?:unsigned |signed )?short)\s+(\w+)\s*\[",
                              open(f, encoding="utf-8", errors="replace").read(), re.M):
            declared[mm.group(2)] = mm.group(1)
    for bname, addr, sect, ctype, cname, n, body in defs:
        if cname in declared and ("(uintptr_t)" in body or "UMK3_REL(" in body):
            fail(problems, "%s is declared %s[] but holds a relocation" % (cname, declared[cname]))

    # 4: missed pointers.
    cstr = m.section_by_name("__TEXT", "__cstring")
    data_starts = {v[0] for v in syms.values() if v[1] == "__DATA"}
    used_numbers = set()
    for bname, addr, sect, ctype, cname, n, body in defs:
        if ctype != "uintptr_t":
            continue
        a0 = int(addr, 16)
        for k, it in enumerate(x.strip() for x in re.findall(r"UMK3_REL\([^)]+\)|[^,\s]+", body) if x.strip()):
            if it.startswith("(") or it.startswith("UMK3_REL("):
                continue
            v = int(it, 0) & 0xffffffff
            why = None
            if v & 1 and (v & ~1) in fstarts:
                why = "a Thumb function start"
            elif cstr and cstr.addr <= v < cstr.addr + cstr.size and raw(v - 1, 1) == b"\0":
                why = "a string start"
            elif v in data_starts and v >= 0x1000:
                why = "a data symbol start"
            if why:
                key = (bname, 4 * k)
                if key in numbers:
                    used_numbers.add(key)
                else:
                    fail(problems, "%s+0x%x = 0x%x is %s and was left a number "
                         "(list it as `number` with the read that settles it)"
                         % (bname, 4 * k, v, why))
    for key in numbers - used_numbers:
        fail(problems, "map: number %s 0x%x matches nothing; drop it" % key)

    # 5: layout of a linked program.
    pairs = broken = 0
    if nmpath:
        host = {}
        for line in open(nmpath, encoding="utf-8", errors="replace"):
            p = line.split()
            if len(p) == 3:
                host[p[2][1:] if p[2].startswith("_") else p[2]] = int(p[0], 16)
        order = sorted((a, c) for c, a in cmap.items() if c in host and
                       re.search(r"\b%s\[" % re.escape(c), text))
        for (a1, c1), (a2, c2) in zip(order, order[1:]):
            if starts[bisect.bisect_right(starts, a1)] != a2:
                continue
            pairs += 1
            if host[c2] - host[c1] != a2 - a1:
                broken += 1
                fail(problems, "%s -> %s touch in the image (gap %d) but not on "
                     "the host (gap %d)" % (c1, c2, a2 - a1, host[c2] - host[c1]))

    # The check must have looked at what the generator says it wrote.
    claim = re.search(r"(\d+) objects, (\d+) relocated words", text)
    if not claim or int(claim.group(1)) != len(defs) or int(claim.group(2)) != relocs:
        fail(problems, "checked %d objects / %d relocations, the generator says %s"
             % (len(defs), relocs, claim.group(0) if claim else "nothing"))

    print("objects %d, words %d, relocations %d, numbers checked against the map %d%s"
          % (len(defs), words, relocs, len(used_numbers),
             ", touching pairs %d (broken %d)" % (pairs, broken) if nmpath else ""))
    if problems:
        print("RESULT: FAIL, %d problem(s)" % len(problems))
        return 1
    print("RESULT: the generated tables match the binary")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
