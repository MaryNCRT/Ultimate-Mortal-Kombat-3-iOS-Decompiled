#!/usr/bin/env python3
"""logic_tables.py -- extract the fight engine's data tables from the user's binary.

    python tools/logic_tables.py <UMK3 binary> <out.c> [--report <out.txt>]

`decomp/gamecode/logic` is code. The data it runs on -- special-move lists,
animation scripts, reaction tables, AI branch tables, ~1,100 objects in all --
lives in the binary's __DATA,__data and __TEXT,__const and is NOT in this
repository. This tool reads it out of the user's own copy at build time and
writes a C file that defines every one of those objects, so the decompiled
engine links and runs. The generated file holds game data: it belongs in the
build directory and is never committed.

## Which objects

The binary is not stripped, and its STABs say which source file defined each
global and each function-local static (`N_SO` opens a file, `N_GSYM`/`N_STSYM`
name its data, `N_FUN` its functions). Every data object of every
`src/gamecode/logic/*.c` file is emitted.

Zero-filled ones (__common: `G`, `H`, `Plyr`, `mytc` ...) are emitted as
storage under `umk3_common_<name>`, unless the map lists them as `object`.
The decompiled code does not name most of them directly -- it goes through
slot pointers (`char *Plyr`,
`GAMESTATE *G`) that `runtime/fight_runtime.c` points at this storage -- but
tables do: `swtab` holds addresses inside `G`, and those must land in the
same bytes the slot reaches.

## How big each one is

The gap to the next symbol in the same section. Anonymous objects -- the
string literals behind `txt_tie`, for instance -- sit in that gap and come
along with the object before them, at the same relative offset, which is what
a pointer into them needs.

## Which words are pointers

The binary is not position-independent, so there is no rebase table to say.
A word is relocated when, and only when:

  * it is a Thumb function start (bit 0 set) of a function whose STAB places
    it in a gamecode/logic source file -- every function of which has a native
    definition; or
  * it falls inside one of the emitted logic data objects; or
  * it falls inside an object `logic_tables.map` maps to native storage.

and it is not listed as `int` in `logic_tables.map`.

Anything else is an integer and is copied as is. That rule is deliberately
narrow: the special-move lists hold switch masks such as 0x00200000 that land
inside front-end objects, and fixed-point constants such as 0x00050000 land in
the middle of functions. Neither is a pointer, and the rule does not take
them for one. Every relocation and every rejected near-miss is written to the
report so the classification can be audited.

## `int` entries: words checked to be numbers

A value can land inside a logic object by accident. `ochar_headrip_lineups`
is read as `int16_t` pairs by mkfatal.c, and its pair (0x0030, 0x000e) spells
0x000e0030, which is inside `seq_mileena_groundroll`. The map lists each such
word, by symbol and offset, with the read that settles it. A listed word is
never relocated, and the generator refuses a listed word that would not have
been relocated anyway, so a stale entry cannot hide.

## Layout

Every object is emitted in binary address order into one section, zero-filled
ones included (a zero array would otherwise go to .bss and leave the order).
Objects whose address or size is not a multiple of four are emitted as bytes,
at their exact size and alignment; one of those holding a relocation is an
error. So two objects that touch in the image touch on the host, and a read
past the end of one sees the next, as it does on the device:
`ochar_slam_damage` is 25 int16 indexed by character, and sits directly in
front of `ochar_slammed_anis`.

## Pointer width

The generated arrays hold `uintptr_t` and the file refuses to compile unless
that is four bytes: the engine's structs are 32-bit, and so are its tables.
"""

import os
import struct
import sys
import bisect

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import macho  # noqa: E402

N_GSYM, N_FUN, N_STSYM, N_SO = 0x20, 0x24, 0x26, 0x64
LOGIC_DIR = "/gamecode/logic/"


def load(path):
    data = macho.read_file(path)
    base = 0
    for cputype, sub, off, size, align in macho.fat_slices(data):
        if cputype == macho.CPU_TYPE_ARM and sub == 9:
            base = off
            break
    else:
        if macho.fat_slices(data):
            raise SystemExit("%s: no armv7 slice" % path)
    m = macho.MachO(data, base)
    if m.arch() != "armv7":
        raise SystemExit("%s: expected armv7, got %s" % (path, m.arch()))
    return data, base, m


def read_map(path):
    native, alias, objects = {}, {}, set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].split()
            if not line:
                continue
            if line[0] == "native" and len(line) == 3:
                native[line[1]] = line[2]
            elif line[0] == "alias" and len(line) == 3:
                alias[int(line[2], 16)] = line[1]
            elif line[0] == "object" and len(line) == 2:
                objects.add(line[1])
            elif line[0] in ("int", "number") and len(line) == 3:
                pass                    # read_ints / check_logic_tables.py
            else:
                raise SystemExit("%s: bad line %r" % (path, " ".join(line)))
    return native, alias, objects


def read_ints(path):
    """`int <binary symbol> <offset>` lines -> [(symbol, offset)]."""
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].split()
            if line and line[0] == "int":
                out.append((line[1], int(line[2], 16)))
    return out


def c_name(binary_name):
    """`_funcs.7674` -> `funcs_7674`: the leading underscore is the Mach-O C
    prefix, and a function-local static's `.N` suffix is not a C identifier."""
    n = binary_name[1:] if binary_name.startswith("_") else binary_name
    return n.replace(".", "_")


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)
    binpath, outpath = argv[1], argv[2]
    report = argv[argv.index("--report") + 1] if "--report" in argv else None
    mappath = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "logic_tables.map")
    native, alias, objects = read_map(mappath)

    data, base, m = load(binpath)
    secs = m.sections
    syms = m.symbols()

    # 1. Which names belong to gamecode/logic, per the STABs.
    cu = None
    logic_data, logic_funcs = set(), set()
    for name, t, sect, desc, val in syms:
        if t == N_SO and name:
            cu = name
        elif cu and LOGIC_DIR in cu:
            if t in (N_GSYM, N_STSYM):
                logic_data.add(name)
            elif t == N_FUN and name:
                logic_funcs.add(name.split(":", 1)[0])

    # 2. Real (non-STAB) symbols with their sections, sorted.
    real = []
    for name, t, sect, desc, val in syms:
        if t & macho.N_STAB or (t & macho.N_TYPE) != macho.N_SECT:
            continue
        real.append((val, name, secs[sect - 1], desc))
    real.sort(key=lambda r: r[0])

    def extent(i):
        a, _, s, _ = real[i]
        end = s.addr + s.size
        for j in range(i + 1, len(real)):
            if real[j][0] > a:
                if real[j][2] is s or real[j][0] < end:
                    end = min(end, real[j][0])
                break
        return end - a

    forced_int = {}
    for bname, off in read_ints(mappath):
        hit = [r[0] for r in real if r[1] == bname]
        if not hit:
            raise SystemExit("logic_tables.map: int %s: not in the binary" % bname)
        forced_int[hit[0] + off] = "%s+0x%x" % (bname, off)

    thumb_funcs = {}
    for a, name, s, desc in real:
        if s.segname == "__TEXT" and s.sectname == "__text" and desc & 0x8 \
                and name in logic_funcs:
            thumb_funcs[a] = c_name(name)
    # A name the binary defines more than once is a file-local function
    # (`q_yes` lives in moves.c, mkdrone.c and mkboss.c). The decompiled C keeps
    # only one of each as a global, so a table that pointed at another copy
    # could not be linked by name. None does today; this keeps it that way.
    names = list(thumb_funcs.values())
    ambiguous = {n for n in names if names.count(n) > 1}

    objs = []         # (addr, size, cname, binary name, section, zerofill)
    seen = set()
    for i, (a, name, s, desc) in enumerate(real):
        if name not in logic_data or a in seen:
            continue
        seen.add(a)
        zerofill = (s.flags & 0xFF) in (0x1, 0xC)
        cname = alias.get(a, c_name(name))
        if zerofill and name not in objects:
            cname = "umk3_common_" + cname
        objs.append((a, extent(i), cname, name, s, zerofill))

    emitted = [o for o in objs if not o[5]]
    common = [o for o in objs if o[5]]
    ranges = sorted((o[0], o[0] + o[1], o[2]) for o in objs)
    # objects outside gamecode/logic that the map hands to native storage
    for a, name, s, desc in real:
        if name in native and name not in logic_data:
            ranges.append((a, a + extent(real.index((a, name, s, desc))),
                           native[name]))
    ranges.sort()
    starts = [r[0] for r in ranges]
    native_targets = set(native.values())

    def target(v):
        if v & 1 and (v & ~1) in thumb_funcs:
            if thumb_funcs[v & ~1] in ambiguous:
                raise SystemExit("a table points at 0x%x, one of several "
                                 "functions named %s"
                                 % (v & ~1, thumb_funcs[v & ~1]))
            return ("fn", thumb_funcs[v & ~1], 0)
        i = bisect.bisect_right(starts, v) - 1
        if i >= 0 and ranges[i][0] <= v < ranges[i][1]:
            return ("data", ranges[i][2], v - ranges[i][0])
        return None

    def read(a, n):
        s = m.section_for_addr(a)
        off = base + s.offset + (a - s.addr)
        return data[off:off + n]

    # The report keeps every near miss: a word in an address range that the
    # rule nevertheless calls an integer.
    sym_starts = [r[0] for r in real]

    def near_miss(v):
        s = m.section_for_addr(v)
        if s is None or v < 0x1000:
            return None
        j = bisect.bisect_right(sym_starts, v) - 1
        return "%s,%s %s+0x%x" % (s.segname, s.sectname, real[j][1],
                                  v - real[j][0]) if j >= 0 else None

    out = []
    rep = []
    fn_refs, data_defs = set(), []
    fixups = []                         # (object, word, target, offset)
    nrel = 0
    for a, size, cname, bname, s, zf in emitted:
        raw = read(a, size) + b"\0" * (-size % 4)
        words = struct.unpack("<%dI" % (len(raw) // 4), raw)
        items = []
        orel = 0
        for k, v in enumerate(words):
            t = target(v)
            if a + k * 4 in forced_int:
                if t is None:
                    raise SystemExit("logic_tables.map: int %s is not a "
                                     "relocation candidate; drop the entry"
                                     % forced_int.pop(a + k * 4))
                forced_int.pop(a + k * 4)
                if report:
                    rep.append("int!  %-28s +0x%04x %08x  (map: a number, "
                               "not %s+0x%x)" % (cname, k * 4, v, t[1], t[2]))
                t = None
                items.append("0x%x" % v)
                continue
            if t is None:
                items.append("0x%x" % v)
                miss = near_miss(v) if v >= 0x1000 else None
                if miss and report:
                    rep.append("int   %-28s +0x%04x %08x  (%s)"
                               % (cname, k * 4, v, miss))
                continue
            kind, tname, off = t
            nrel += 1
            orel += 1
            if kind == "fn":
                fn_refs.add(tname)
            items.append("UMK3_REL(%s, 0x%x)" % (tname, off))
            fixups.append((cname, k, tname, off))
            if report:
                rep.append("%-5s %-28s +0x%04x %08x -> %s%s"
                           % (kind, cname, k * 4, v, tname,
                              "+0x%x" % off if off else ""))
        exact = a % 4 == 0 and size % 4 == 0
        if not exact and orel:
            raise SystemExit("%s at 0x%x is not word-aligned and holds %d "
                             "relocation(s)" % (bname, a, orel))
        data_defs.append((cname, bname, a, s, items, size,
                          None if exact else read(a, size)))
    if forced_int:
        raise SystemExit("logic_tables.map: int %s is in no emitted object"
                         % ", ".join(sorted(forced_int.values())))

    out.append("/* GENERATED by tools/logic_tables.py -- do not edit, do not commit.")
    out.append(" *")
    out.append(" * Every data object of src/gamecode/logic, read out of the user's own")
    out.append(" * binary (uuid %s) at build time." % m.uuid if hasattr(m, "uuid")
               else " * Every data object of src/gamecode/logic, read at build time.")
    out.append(" * %d objects, %d relocated words. */" % (len(data_defs), nrel))
    out.append("#include <stdint.h>")
    out.append("/* A word is 32 bits, as on the device. A 64-bit build keeps the game")
    out.append(" * below 4 GB (UMK3_LOW32): its words are uint32_t (runtime/savestate.h")
    out.append(" * makes uintptr_t one there) and the addresses, which C cannot put in")
    out.append(" * a 32-bit initializer, are written by umk3_logic_tables_fixup(). */")
    out.append("#ifdef UMK3_LOW32")
    out.append("#define UMK3_REL(p, o) 0")
    out.append("#else")
    out.append("typedef char umk3_logic_tables_need_32bit_pointers"
               "[sizeof(void *) == 4 ? 1 : -1];")
    out.append("#define UMK3_REL(p, o) ((uintptr_t)(p) + (o))")
    out.append("#endif")
    out.append("")
    out.append("/* One section, binary order: see Layout in tools/logic_tables.py. */")
    out.append("#if defined(__APPLE__)")
    out.append("#define UMK3_LOGIC __attribute__((section(\"__DATA,__umk3logic\")))")
    out.append("#elif defined(__wasm__)")
    out.append("#define UMK3_LOGIC  /* no named sections: order is not guaranteed */")
    out.append("#elif defined(_WIN32) && defined(UMK3_STATE_FILE)")
    out.append("/* the game's save states copy this block too (runtime/savestate.h);")
    out.append(" * \"$l\" merges it into .umkstd in one piece, its order kept */")
    out.append("#define UMK3_LOGIC __attribute__((section(\".umkstd$l\")))")
    out.append("#else")
    out.append("#define UMK3_LOGIC __attribute__((section(\".data.umk3logic\")))")
    out.append("#endif")
    out.append("")
    for f in sorted(fn_refs):
        out.append("extern char %s[];" % f)
    for t in sorted(native_targets):
        out.append("extern char %s[];" % t)
    for cname, bname, a, s, items, size, raw in data_defs:
        out.append("extern %s %s[];" % ("unsigned char" if raw else "uintptr_t",
                                        cname))
    for a, size, cname, bname, s, zf in common:
        out.append("extern %s %s[];" % ("uintptr_t" if a % 4 == 0 and size % 4 == 0
                                        else "unsigned char", cname))
    out.append("")
    for cname, bname, a, s, items, size, raw in data_defs:
        out.append("/* %s  0x%08x  %s,%s */" % (bname, a, s.segname, s.sectname))
        if raw is not None:
            align = a & -a if a & 3 else 4
            out.append("unsigned char %s[%d] UMK3_LOGIC __attribute__((aligned(%d))) = {"
                       % (cname, size, min(align, 4)))
            for k in range(0, len(raw), 16):
                out.append("    " + ", ".join(str(b) for b in raw[k:k + 16]) + ",")
        else:
            out.append("uintptr_t %s[%d] UMK3_LOGIC __attribute__((aligned(4))) = {" % (cname, len(items)))
            for k in range(0, len(items), 6):
                out.append("    " + ", ".join(items[k:k + 6]) + ",")
        out.append("};")
    out.append("")
    out.append("/* Runtime state: zero, owned through the slot pointers. Zero-filled")
    out.append(" * on the device too, but kept in the section so it stays in order. */")
    for a, size, cname, bname, s, zf in common:
        if a % 4 == 0 and size % 4 == 0:
            out.append("uintptr_t %s[%d] UMK3_LOGIC __attribute__((aligned(4))) = {0};  /* %s 0x%08x, %d bytes */"
                       % (cname, size // 4, bname, a, size))
        else:
            align = a & -a if a & 3 else 4
            out.append("unsigned char %s[%d] UMK3_LOGIC __attribute__((aligned(%d))) = {0};  /* %s 0x%08x, %d bytes */"
                       % (cname, size, min(align, 4), bname, a, size))
    out.append("")
    out.append("void umk3_logic_tables_fixup(void)")
    out.append("{")
    out.append("#ifdef UMK3_LOW32")
    out.append("    static const struct { uint32_t *at; const char *to; } fix[] = {")
    for cname, k, tname, off in fixups:
        out.append("        { (uint32_t *)%s + %d, (const char *)%s + 0x%x },"
                   % (cname, k, tname, off))
    out.append("    };")
    out.append("    unsigned i;")
    out.append("")
    out.append("    for (i = 0; i < sizeof fix / sizeof fix[0]; i++)")
    out.append("        *fix[i].at = (uint32_t)(uintptr_t)fix[i].to;")
    out.append("#endif")
    out.append("}")
    with open(outpath, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    if report:
        with open(report, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(rep) + "\n")
    print("logic_tables: %d objects, %d relocations, %d functions referenced -> %s"
          % (len(data_defs), nrel, len(fn_refs), outpath))


if __name__ == "__main__":
    main(sys.argv)
