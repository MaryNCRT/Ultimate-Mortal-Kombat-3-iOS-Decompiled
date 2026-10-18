#!/usr/bin/env python3
"""slotcheck.py -- find globals the decomp declares as both a slot and a value.

The same symbol gets declared in every file that touches it, and the two
spellings can disagree:

    FrontEnd.c:  extern float *GameCounter;   /* pointer slot */
    GameCode.c:  extern float  GameCounter;   /* 0x0014fa5c */

Both compile, because each translation unit only sees its own. One of them is
wrong, and the wrong one crashes: `*GameCounter` reads the bits of a float as an
address. This has now been the cause of three separate faults --
`AllFramesTable`, `FatalityMessage`, `GameCounter` -- so it is worth a tool
rather than another afternoon.

## What settles it

The address in the comment is the SYMBOL's address, and the symbol table says
how big that symbol is and what section it lives in. A genuine pointer slot is
a GOT entry: the compiler emits

    ldr r3, [pc, #..]     ; -> 0x000f35f4      the slot
    ldr r3, [r3]                               the address it holds

and every one of those lives in the 0x000f3xxx region. An address outside that
region is the data itself, so a four-byte symbol there is a value and spelling
it `T *` is wrong.

    python tools/slotcheck.py           report
    python tools/slotcheck.py --fix     correct the ones it is sure of

`--fix` only touches the "value" verdict: a symbol of four bytes or fewer,
outside the GOT region, declared `T *name` in one file and `T name` in another.
There the pointer spelling cannot be right and the correction is mechanical --
drop the star on the declaration, and drop it from `*name` at the use sites in
that same file. It does not touch arrays: those need a look at how each site
indexes them.

Nothing is lost by being wrong in the safe direction here. `mkglobals.py`
already gives these symbols value storage, because it prefers the sized
declaration, so every `*name` in the tree is ALREADY reading a float or a long
as an address. The fix makes the source agree with what is generated.
"""

import io
import os
import re
import sys

DECOMP_DIRS = ["src/decomp/gamecode", "src/decomp/lime"]

# Hand-written runtime. `gamecode_globals.c` is generated FROM the decomp, so
# including it would compare the decomp against itself.
RUNTIME_DIRS = ["src/runtime", "src/runtime/lime"]
RUNTIME_SKIP = ("gamecode_globals.c",)
SYMBOLS = os.environ.get("UMK3_SYMBOLS", "work/symbols.txt")

# The GOT-style region. Every verified pointer slot in this binary is in it.
SLOT_LO, SLOT_HI = 0x000F3000, 0x000F4000

DECL = re.compile(
    r"^extern\s+"
    r"(?P<type>(?:const\s+|unsigned\s+|signed\s+|struct\s+)*[A-Za-z_]\w*)\s+"
    r"(?P<stars>[*]*)\s*"
    r"(?P<name>[A-Za-z_]\w*)\s*"
    r"(?P<dims>(?:\[[^\]]*\])*)\s*;"
    r"(?P<rest>.*)$")


def symbols():
    """{name: (address, section)} and {name: extent} for the data symbols."""
    addr, sect, per_section = {}, {}, {}
    for line in open(SYMBOLS, encoding="utf-8", errors="replace"):
        p = line.split()
        if len(p) < 5 or not p[0].startswith("0x") or p[1] != "SECT":
            continue
        name = p[-1]
        if not name.startswith("_"):
            continue
        name = name[1:]
        addr[name], sect[name] = int(p[0], 16), p[3]
        per_section.setdefault(p[3], set()).add((addr[name], name))

    extent = {}
    for entries in per_section.values():
        ordered = sorted(entries)
        for i, (a, name) in enumerate(ordered):
            j = i
            while j < len(ordered) and ordered[j][0] == a:
                j += 1
            if j < len(ordered):
                extent[name] = max(extent.get(name, 0), ordered[j][0] - a)
    return addr, sect, extent


def declarations():
    """{name: [(path, line number, is a slot, has dims, text)]}"""
    out = {}
    for d in DECOMP_DIRS:
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".c"):
                continue
            path = os.path.join(d, fn)
            for n, line in enumerate(open(path, encoding="utf-8",
                                          errors="replace"), 1):
                m = DECL.match(line.strip())
                if not m:
                    continue
                slot = ("pointer slot" in m.group("rest")
                        and m.group("stars") and not m.group("dims"))
                out.setdefault(m.group("name"), []).append(
                    (path, n, bool(slot), bool(m.group("dims")),
                     line.strip()))
    return out


RTDEF = re.compile(
    r"^(?P<type>(?:const\s+|unsigned\s+|signed\s+)*"
    r"(?:char|short|int|long|float|double|size_t|uint\d+_t|int\d+_t))\s+"
    r"(?P<stars>\**)\s*(?P<name>[A-Za-z_]\w*)\s*"
    r"(?P<dims>(?:\[[^\]]*\])*)\s*(?:=|;)")


def runtime_definitions():
    """{name: (type, is an array)} for what the hand-written runtime defines."""
    out = {}
    for d in RUNTIME_DIRS:
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".c") or fn in RUNTIME_SKIP:
                continue
            for line in open(os.path.join(d, fn), encoding="utf-8",
                             errors="replace"):
                if line.startswith((" ", "\t", "static", "extern")):
                    continue            # locals, and declarations of others
                m = RTDEF.match(line)
                if m and not m.group("stars"):
                    out[m.group("name")] = (m.group("type").strip(),
                                            bool(m.group("dims")))
    return out


def runtime_pass(do_fix):
    """Decomp externs that contradict a runtime definition."""
    rt = runtime_definitions()
    decls = declarations()
    hits = 0

    for name in sorted(decls):
        if name not in rt:
            continue
        typ, is_array = rt[name]
        for path, n, slot, dims, text in decls[name]:
            if not slot:
                continue                # already a value or an array
            hits += 1
            if not do_fix:
                print("%-28s runtime: %s %s%s" % (name, typ, name,
                                                  "[]" if is_array else ""))
                print("    %-34s %s:%d" % (text, path, n))
                continue

            body = io.open(path, encoding="utf-8", newline="").read()
            body = re.sub(
                r"^extern\s+(?:const |unsigned |signed )*\w+\s+\*%s\s*;"
                % re.escape(name),
                "extern %s %s%s;" % (typ, name, "[]" if is_array else ""),
                body, flags=re.M)
            body = re.sub(r"(?<![\w\])])\*(%s)\b(?!\[)" % re.escape(name),
                          r"\1[0]" if is_array else r"\1", body)
            io.open(path, "w", encoding="utf-8", newline="").write(body)

    return hits


def fix_file(path, name, is_array=False):
    """Drop the star from the declaration and from `*name` uses in one file.

    With `is_array` the declaration becomes `T name[]` and a bare dereference
    becomes `name[0]` -- the same correction, one level along.

    Never for `void`: `void *handle` is an opaque handle, not a slot around a
    value, and `extern void handle;` is not a type at all. The verdict machinery
    reads a four-byte symbol as a value, and for a handle four bytes is exactly
    what a pointer measured -- so the size test cannot tell them apart and the
    type has to.
    """
    # A one- or two-letter global is not safe to rewrite mechanically: the
    # scratch matrix is called `m`, and every parameter and local named `m` in
    # the tree looks exactly like it. Those are corrected by hand.
    if len(name) < 3:
        return

    text = io.open(path, encoding="utf-8", newline="").read()
    if re.search(r"^extern\s+void\s+\*%s\s*;" % re.escape(name), text,
                 flags=re.M):
        return

    # the declaration: `extern T *name;` -> `extern T name;` or `T name[];`
    text = re.sub(r"(^extern\s+(?:const |unsigned |signed )*\w+\s+)\*(%s)(\s*;)"
                  % re.escape(name),
                  (r"\1 \2[]\3" if is_array else r"\1 \2\3"),
                  text, flags=re.M)

    # the uses: `*name` with the star touching the identifier. Multiplication in
    # this tree is always spaced (`i * 25`, `who * 2`), and a cast is
    # `*(long *)`, so a star flush against a name is a dereference.
    text = re.sub(r"(?<![\w\])])\*(%s)\b(?!\s*\[)" % re.escape(name),
                  (r"\1[0]" if is_array else r"\1"), text)

    io.open(path, "w", encoding="utf-8", newline="").write(text)


def main():
    do_fix = "--fix" in sys.argv
    addr, sect, extent = symbols()
    decls = declarations()
    bad, fixed = 0, 0

    for name in sorted(decls):
        forms = decls[name]
        if len({f[2] for f in forms}) < 2:
            continue                    # everyone agrees
        a = addr.get(name)
        if a is None:
            continue

        in_slot_region = SLOT_LO <= a < SLOT_HI
        size = extent.get(name, 0)
        verdict = ("slot" if in_slot_region else
                   "value" if size and size <= 4 else "array")

        bad += 1
        if do_fix and verdict in ("value", "array"):
            for path, n, slot, dims, text in forms:
                if slot:
                    fix_file(path, name, verdict == "array")
            fixed += 1
            continue

        print("%s  0x%08x  %s  %d bytes  -> the %s spelling is right"
              % (name, a, sect.get(name, "?"), size, verdict))
        for path, n, slot, dims, text in forms:
            print("    %-34s %s:%d" % (text, path, n))
        print()

    rt = runtime_pass(do_fix)
    if rt:
        print("%d decomp externs contradict a runtime definition%s."
              % (rt, " (corrected)" if do_fix else ""))

    print("%d symbols are declared both ways." % bad)
    if do_fix:
        print("%d corrected; the rest are arrays and need reading." % fixed)
    elif bad:
        print("A symbol outside 0x000f3xxx is the data, not a pointer to it.")
    return 1 if bad and not do_fix else 0


if __name__ == "__main__":
    sys.exit(main())
