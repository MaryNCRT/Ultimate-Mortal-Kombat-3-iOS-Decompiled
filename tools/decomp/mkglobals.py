#!/usr/bin/env python3
"""mkglobals.py -- emit definitions for every global the decomp only declares.

The decompiled sources are transcriptions: every global they touch is an
`extern` and nothing defines it, so the tree compiles and does not link. This
walks the `extern` declarations, keeps the ones no translation unit defines, and
writes a single C file that gives each one storage.

    python tools/mkglobals.py > runtime/gamecode_globals.c

## The pointer-slot problem

Most globals are declared the way the binary reaches them. A plain scalar is

    extern int GameMode;                    /* slot -> 0x0014faa4 */

and a **pointer slot** is

    extern float *col;                      /* pointer slot -> 0x0014fa00 */

where the code writes `*col` or `col[i]`. Defining that as a bare `float *col;`
gives a null pointer and the first read segfaults. So a pointer-slot global gets
**backing storage plus a definition that points at it**:

    static float col__store[SLOT_WORDS];
    float *col = col__store;

`SLOT_WORDS` is deliberately generous: the real extents are not all known, and a
few words of slack costs nothing against a crash that would take an afternoon to
localise. Where an extent IS known -- because the declaration carries a size --
that size is used instead.

This file is generated. Do not hand-edit it; change this script.
"""

import os
import re
import subprocess
import sys

import mkdata
import tempfile

DECOMP_DIRS = ["src/decomp/gamecode", "src/decomp/lime"]

# Backing words for a pointer slot. These are small in practice -- a slot is
# usually a colour, a matrix or a handful of counters.
SLOT_WORDS = 64

# Bytes for an array the decomp declares as `T name[]`, extent unknown. 229 of
# them, and a slot-sized guess is not slack, it is a silent overrun:
# `AllFramesTable` got 64 bytes and LoadAllFramesTXT sscanf'd 7,244 entries into
# it. This default is generous; where the real extent is known it is listed
# below and used instead.
UNSIZED_BYTES = 64 * 1024

# Where the symbol table lives. Every global's real extent is in it, implicitly:
# the linker laid the data out, so the gap from one symbol to the next IS the
# size of the first. That beats any default -- `AllFramesTable` measures 470,860
# bytes this way, which is ALLFRAMES_COUNT * ALLFRAMES_STRIDE to the byte.
SYMBOLS = os.environ.get("UMK3_SYMBOLS", "work/symbols.txt")

# The image itself. Every initialised global has its value in it, and
# tools/mkdata.py reads them out -- see that file for why a memcpy'd blob
# would not do.
BINARY = os.environ.get("UMK3_BINARY",
                        "E:/MK3 PROJECT/OUTPUT/armv7/UMK3.armv7")

# Sections whose symbols are data. A gap measured across a section boundary is
# not an extent, so only symbols from the same section are compared.
DATA_SECTIONS = ("__DATA,__data", "__DATA,__common", "__DATA,__bss",
                 "__DATA,__const", "__TEXT,__const")


def symbol_extents():
    """Every data symbol's size, as the gap to the next symbol in its section.

    This is a measurement, not a guess, and it is the reason a generated global
    can be the right size without anyone stating the size anywhere: the address
    of the next thing is where this thing ends.

    It is an upper bound where the linker left padding, which is the safe
    direction -- too much storage wastes bytes, too little corrupts whatever is
    next.
    """
    by_section = {}
    for line in open(SYMBOLS, encoding="utf-8", errors="replace"):
        parts = line.split()
        if len(parts) < 5 or not parts[0].startswith("0x"):
            continue
        sect, name = parts[3], parts[-1]
        if sect not in DATA_SECTIONS or not name.startswith("_"):
            continue
        name = name[1:]
        # `__ZL16TaskFunctionList`: a C++ file-static; see tools/mkdata.py.
        m = re.match(r"_ZL(\d+)(\w+)$", name)
        if m and len(m.group(2)) == int(m.group(1)):
            name = m.group(2)
        # `_JoyOffset.11128`: a function-local static; see tools/mkdata.py.
        m = re.match(r"(\w+)\.\d+$", name)
        if m:
            name = m.group(1)
        try:
            by_section.setdefault(sect, set()).add((int(parts[0], 16), name))
        except ValueError:
            pass

    out = {}
    for addrs in by_section.values():
        ordered = sorted(addrs)
        for i, (addr, name) in enumerate(ordered):
            j = i
            while j < len(ordered) and ordered[j][0] == addr:
                j += 1
            if j < len(ordered):
                size = ordered[j][0] - addr
                if 0 < size:
                    # several names can share an address; keep the largest span
                    out[name] = max(out.get(name, 0), size)
    return out

# Field types whose size is known without a layout. Anything else embedded by
# value has to have a body in the decomp, or the struct around it cannot be
# given storage.
SCALARS = {
    "char", "short", "int", "long", "float", "double", "void", "size_t",
    "signed", "unsigned", "uintptr_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
}

# Declarations we must NOT define here: the runtime owns them.
SKIP_PREFIX = ("lime", "EASDK", "EASOC")

# Globals of lime.m that the platform layer defines by name rather than by
# prefix: `ratio` is the aspect LIMEDS_Set3dMode writes, and runtime/lime_menu.c
# owns it with the rest of that file's state.
RUNTIME_NAMES = ("ratio",)


def is_runtime_symbol(name):
    """True for names the platform layer owns rather than the game.

    The GL test is `gl` + a capital, not the bare prefix: `glowProgress` is a
    front-end global and a plain `startswith("gl")` silently swallowed it, which
    then showed up as one unexplained undefined symbol at link time.
    """
    if name.startswith(SKIP_PREFIX) or name in RUNTIME_NAMES:
        return True
    return len(name) > 2 and name[:2] == "gl" and name[2].isupper()

# `extern <stuff> <name>[dims];` on one line. The decomp writes these one per
# line with the address in a trailing comment, which is what makes this safe.
DECL = re.compile(
    r"^extern\s+"
    r"(?P<type>(?:const\s+|unsigned\s+|signed\s+|struct\s+)*"
    r"[A-Za-z_][A-Za-z0-9_]*)\s+"
    r"(?P<stars>[*]*)\s*"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*"
    r"(?P<dims>(?:\[[^\]]*\])*)\s*;"
)


# Just the `extern <type>` head, so a declarator list can be split off it.
HEAD = re.compile(
    r"^extern\s+(?P<type>(?:const\s+|unsigned\s+|signed\s+|struct\s+)*"
    r"[A-Za-z_][A-Za-z0-9_]*)\s+"
)

# `extern void (*TaskFunctionList[])(void);` -- a table of task entry points.
# It has parentheses, so the plain-declaration path skips it as a prototype, but
# it is data and the front end indexes it every frame.
FNPTR = re.compile(
    r"^extern\s+(?P<ret>[A-Za-z_]\w*)\s*"
    r"\(\s*\*\s*(?:const\s+)?(?P<name>[A-Za-z_]\w*)\s*(?P<dims>(?:\[[^\]]*\])*)\s*\)\s*"
    r"\((?P<args>[^)]*)\)\s*;"
)


class FnPtrMatch(object):
    """Quacks like a DECL match so the emitter can treat both the same."""

    def __init__(self, m):
        self._m = m

    def group(self, k):
        if k == "type":
            return self._m.group("ret")
        if k == "name":
            return self._m.group("name")
        if k == "stars":
            return ""
        if k == "dims":
            return self._m.group("dims")
        return self._m.group(k)

    fnptr = True


def decl_lines():
    """Every `extern ...;` declaration in the decomp, one per declarator.

    Two shapes need care. `extern long A, B, C;` declares three globals on one
    line -- taking only the first left sixty of them undefined. And a function-
    pointer table has parentheses, so the prototype filter would throw it away.
    """
    for d in DECOMP_DIRS:
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".c"):
                continue
            path = os.path.join(d, fn)
            with open(path, encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    line = line.strip()
                    if not line.startswith("extern "):
                        continue

                    fp = FNPTR.match(line)
                    if fp:
                        yield path, line, FnPtrMatch(fp)
                        continue

                    if "(" in line.split(";")[0]:
                        continue            # a real function prototype

                    m = DECL.match(line)
                    if m:
                        yield path, line, m
                        continue

                    # `extern long A, B, C;` -- DECL wants the `;` right after
                    # the name, so it does not match this at all. Split the
                    # declarator list and re-match each one against the shared
                    # base type. Sixty globals were missing on this alone.
                    head = HEAD.match(line)
                    if not head:
                        continue
                    body = line.split(";")[0][head.end():]
                    for part in body.split(","):
                        part = part.strip()
                        if not part:
                            continue
                        m2 = DECL.match("extern %s %s;"
                                        % (head.group("type"), part))
                        if m2:
                            yield path, line, m2


# The transcription writes a layout two ways: inline as
# `typedef struct X { ... } X;`, or forward-declared and then given its body at
# the function that proved it, as a bare `struct X { ... };`. Both are real
# definitions and both have to be scraped, or half the layouts go missing.
TYPEDEF = re.compile(r"^typedef struct\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{")
# ... and the anonymous form, whose name only appears on the closing line.
ANONDEF = re.compile(r"^typedef struct\s*\{")
ANONEND = re.compile(r"^\}\s*([A-Za-z_][A-Za-z0-9_]*)\s*;")
STRUCTDEF = re.compile(r"^struct\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{")


def typedef_blocks():
    """Every struct layout in the decomp, by name, plus the names of the ones
    that are only ever forward-declared.

    The layouts live in the .c files that established them -- that is where the
    offsets were proved -- so this scrapes them rather than duplicating them by
    hand. A duplicate would drift.
    """
    out, opaque = {}, set()
    for d in DECOMP_DIRS:
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".c"):
                continue
            with open(os.path.join(d, fn), encoding="utf-8",
                      errors="replace") as fh:
                lines = fh.readlines()
            i = 0
            while i < len(lines):
                line = lines[i]
                fwd = re.match(r"^typedef struct\s+(\w+)\s+\1\s*;", line)
                if fwd:
                    opaque.add(fwd.group(1))
                    i += 1
                    continue
                m = TYPEDEF.match(line) or STRUCTDEF.match(line)
                anon = None if m else ANONDEF.match(line)
                if not m and not anon:
                    i += 1
                    continue
                name, depth, block = (m.group(1) if m else None), 0, []
                while i < len(lines):
                    block.append(lines[i].rstrip("\n"))
                    depth += lines[i].count("{") - lines[i].count("}")
                    end = ANONEND.match(lines[i].strip())
                    i += 1
                    if depth == 0 and len(block) > 1:
                        if name is None and end:
                            name = end.group(1)
                        break
                if name is None:
                    continue
                body = "\n".join(block)
                if STRUCTDEF.match(body):       # give the bare form its typedef
                    body += "\ntypedef struct %s %s;" % (name, name)
                out.setdefault(name, body)
    return out, opaque - set(out)


DEFINE = re.compile(r"^#define\s+([A-Z_][A-Z0-9_]*)\s+(\S.*?)\s*(?:/\*.*)?$")


def define_values():
    """`#define NAME value` from the decomp, for the constants structs size with."""
    out = {}
    for d in DECOMP_DIRS:
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".c"):
                continue
            with open(os.path.join(d, fn), encoding="utf-8",
                      errors="replace") as fh:
                for line in fh:
                    m = DEFINE.match(line.strip())
                    if m and "(" not in m.group(1):
                        out.setdefault(m.group(1), m.group(2))
    return out


def emit_header(needed, blocks, path, opaque=(), decl_texts=()):
    """Write the typedefs, in an order that compiles.

    A struct can name another, so the blocks go out in dependency order: repeat
    a pass over what is left, emitting anything whose unmet references are all
    already out. Anything still circular after that is emitted last -- there is
    none today, and if one appears the compiler says so rather than this silently
    producing something wrong.
    """
    todo = {n: blocks[n] for n in needed if n in blocks}
    done, order = set(), []
    while todo:
        progress = False
        for n in sorted(todo):
            body = todo[n]
            refs = {o for o in todo
                    if o != n and re.search(r"\b%s\b" % o, body)}
            if not refs - done:
                order.append(n)
                done.add(n)
                del todo[n]
                progress = True
        if not progress:
            order.extend(sorted(todo))
            break

    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write("/* Generated by tools/mkglobals.py -- do not hand-edit.\n"
                 " *\n"
                 " * The struct layouts the decompiled globals need, scraped from the .c\n"
                 " * files that established them. Copying them by hand would let the two\n"
                 " * drift, and the offsets are the part that was expensive to prove.\n"
                 " */\n\n"
                 "#ifndef GAMECODE_GLOBALS_H\n#define GAMECODE_GLOBALS_H\n\n"
                 "#include <stdint.h>\n\n")

        # Types the decomp only forward-declares, plus any a struct body names
        # that nothing declares at all. Globals of these are always pointers, so
        # an incomplete type is exactly right -- and inventing a body would be
        # inventing a layout, which is the one thing this project does not do.
        known = define_values()
        # Words that look like a type but are not one. Without this the scan
        # emitted `typedef struct NULL NULL;`.
        NOT_A_TYPE = {"NULL", "GL", "FALSE", "TRUE", "EOF", "RGBA", "UV", "XYZ"}
        unknown = set()
        for n in order:
            for w in re.findall(r"\b([A-Z][A-Z0-9_]{2,})\b", blocks[n]):
                if (w not in blocks and w not in known and w not in opaque
                        and w not in NOT_A_TYPE):
                    unknown.add(w)
        for n in sorted(set(opaque) | unknown):
            fh.write("typedef struct %s %s;\n" % (n, n))
        if opaque or unknown:
            fh.write("\n")

        # Struct bodies AND global declarations size arrays with named
        # constants -- `extern long BioText[BIOS_PAGES];` is as common as a
        # constant inside a struct -- so both have to be searched.
        defs = define_values()
        haystack = "\n".join([blocks[n] for n in order] + list(decl_texts))
        want = {d for d in defs if re.search(r"\b%s\b" % d, haystack)}
        while True:
            grown = set(want)
            for d in list(want):
                for other in defs:
                    if other not in grown and re.search(r"\b%s\b" % other, defs[d]):
                        grown.add(other)
            if grown == want:
                break
            want = grown
        wanted = sorted(want)
        if wanted:
            for d in wanted:
                fh.write("#ifndef %s\n#define %s %s\n#endif\n" % (d, d, defs[d]))
            fh.write("\n")

        for n in order:
            fh.write(blocks[n] + "\n\n")
        fh.write("#endif\n")
    return order


DEFINED = set()   # everything the decomp defines; filled below
POINTED_AT = set()  # data symbols some initialised word points at; filled below


def undefined_symbols():
    """Compile every decomp TU and collect what nothing defines."""
    tmp = tempfile.mkdtemp(prefix="mkglobals")
    objs = []
    for d in DECOMP_DIRS:
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".c"):
                continue
            obj = os.path.join(tmp, "%s_%s.o" % (os.path.basename(d), fn[:-2]))
            r = subprocess.run(
                ["gcc", "-std=c99", "-O0", "-c",
                 "-I", "src/runtime", "-I", "src/decomp/lime",
                 os.path.join(d, fn), "-o", obj],
                capture_output=True)
            if r.returncode == 0:
                objs.append(obj)

    if not objs:
        sys.exit("mkglobals: nothing compiled")

    out = subprocess.run(["nm", "-u"] + objs,
                         capture_output=True, text=True).stdout
    undef = set()
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("U "):
            undef.add(line[2:].strip())

    # The same objects, the other half of the answer: what the tree provides.
    # A function-pointer table can only carry names that are in here.
    out = subprocess.run(["nm", "-g", "--defined-only"] + objs,
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "TtDdBbRr":
            DEFINED.add(parts[2])

    return undef


# Slots the transcription named without writing down where they point. Each
# is read the same way its neighbours are: achievements.c declares
# `FE_WidthScale` "pointer slot -> 0x000ff9b8" two lines above the
# `FE_WidthScaleP` GameCode.c dereferences, and `*GamePausedPtr == 0` gates the
# pause overlay exactly where `GamePaused` does elsewhere.
SLOT_TARGETS = {
    "FE_WidthScaleP": "FE_WidthScale",
    "FE_HeightScaleP": "FE_HeightScale",
    "GamePausedPtr": "GamePaused",
}

_ARROW = re.compile(r"->\s*(?:(0x[0-9a-fA-F]+)|_(\w+))")
_ARROWS = None


def runtime_defines(name):
    """True if a file in runtime/ defines `name` at file scope."""
    pat = re.compile(r"^(?!extern|static|\s|#|/)[^;(]*\b%s\b\s*(\[|=|;)"
                     % re.escape(name), re.M)
    for fn in sorted(os.listdir("src/runtime")):
        if fn == "gamecode_globals.c":
            continue                        # this script's own output
        if fn.endswith(".c") and pat.search(
                open(os.path.join("src/runtime", fn), encoding="utf-8",
                     errors="replace").read()):
            return True
    return False


def slot_target(name, line, img):
    """The global a made-up slot name points at, or None.

    The declaration's comment says it: `/* pointer slot -> 0x00150590 */`
    is CurrentTask, `-> _Players` is Players. Any of the name's declarations
    may carry the arrow, so all of them are looked at.
    """
    global _ARROWS
    if name in SLOT_TARGETS:
        return SLOT_TARGETS[name]
    if _ARROWS is None:
        _ARROWS = {}
        for path, ln, m in decl_lines():
            a = _ARROW.search(ln)
            if a:
                _ARROWS.setdefault(m.group("name"), a.groups())
    hit = _ARROW.search(line) and _ARROW.search(line).groups() \
        or _ARROWS.get(name)
    if not hit:
        return None
    addr, sym = hit
    if sym:
        return sym
    return img.name_at.get(int(addr, 16))


def assigned_names():
    """The names the transcription assigns whole, and those it takes `&` of.

    `X = ...` and `&X` are what a pointer VARIABLE gets; storage reached
    through a slot is only ever read through. Comments are dropped first so a
    line of prose that happens to contain `G = ...` does not count.
    """
    assigned, addressed = set(), set()
    pat = re.compile(r"(?:(?<![\w.>])(\w+)\s*=(?!=))|&\s*(\w+)\b")
    for d in DECOMP_DIRS + ["src/decomp/gamecode/logic"]:
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".c"):
                continue
            text = open(os.path.join(d, fn), encoding="utf-8",
                        errors="replace").read()
            text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
            text = re.sub(r"//[^\n]*", " ", text)
            for a, b in pat.findall(text):
                if a:
                    assigned.add(a)
                else:
                    addressed.add(b)
    return assigned, addressed


def pointed_at(img):
    """Every symbol some word of initialised data points at, at its start or
    inside it -- the names a table can reach without any code naming them."""
    import struct
    out = set()
    for name in img.addr:
        if not img.initialised(name):
            continue
        raw = img.bytes_of(name)
        if not raw:
            continue
        for i in range(0, len(raw) - 3, 4):
            w = struct.unpack_from("<I", raw, i)[0]
            if not (mkdata.VM_BIAS <= w < img.sym_end):
                continue
            t = img.name_at.get(w) or img.containing(w)[0]
            if t:
                out.add(t)
    return out


def main():
    undef = undefined_symbols()
    POINTED_AT.update(pointed_at(mkdata.Image(BINARY, SYMBOLS)))

    # The same global is often declared several different ways across the tree,
    # because each transcription wrote it the way ITS function reached it.
    # `FrameRemapTable` is declared four ways: a `long *` slot in Blood.c and
    # GameCode.c, a `long **` in FrontEnd.c, and -- in Players.c, which is the
    # one that clears it -- `long FrameRemapTable[FRAME_REMAP_ENTRIES][2]`.
    #
    # Only the last knows how big it is, and taking the first one alphabetically
    # gave it 64 words and a segfault in ClearAnimRemapTables. So: prefer a
    # declaration that carries a real extent over one that does not.
    def rank(m):
        dims = m.group("dims")
        sized = dims and "[]" not in dims
        return (1 if sized else 0,                 # a real extent wins
                1 if dims else 0,                  # then any array form
                -len(m.group("stars")))            # then the fewest stars

    seen, conflicts = {}, {}
    for path, line, m in decl_lines():
        name = m.group("name")
        # `nm -u` is per object: GameCode.o lists MatrixPalette2 as undefined
        # even though lime_globals.o defines it. A name the tree defines is
        # never emitted again -- that is a duplicate symbol at link time.
        # A name declared but never used in code is still emitted when a table
        # points at it: `HUDTextures` is reached only through HUDANIM_ttl and
        # the *_MeshAndTexture tables, and without it those came out empty.
        if name in DEFINED or is_runtime_symbol(name):
            continue
        if name not in undef and name not in POINTED_AT:
            continue
        if name in seen:
            conflicts.setdefault(name, 1)
            conflicts[name] += 1
            if rank(m) <= rank(seen[name][2]):
                continue
        seen[name] = (path, line, m)

    if conflicts:
        sys.stderr.write("mkglobals: %d globals are declared more than one way; "
                         "kept the declaration with a known extent\n"
                         % len(conflicts))

    blocks, opaque = typedef_blocks()

    # A struct names other structs, so the set of layouts the header must carry
    # is the transitive closure over the ones the globals mention -- not just
    # the first level. Missing that put HUDANIM and MKEVENT in the header's
    # bodies without their own definitions.
    # Strip the qualifier before matching: `const IdleSet` names the
    # typedef `IdleSet`, and comparing the whole string left the struct
    # out of the header while still emitting a global that uses it.
    needed = {re.sub(r"^(?:const|unsigned|signed|struct)\s+", "",
                     m.group("type")).strip()
              for _, _, m in seen.values()} & set(blocks)
    while True:
        grown = set(needed)
        for n in list(needed):
            for other in blocks:
                if other not in grown and re.search(r"\b%s\b" % other, blocks[n]):
                    grown.add(other)
        if grown == needed:
            break
        needed = grown

    # A struct that embeds an unknown type BY VALUE cannot be given storage: its
    # size is not known, and this project does not invent layouts. `HUD` is the
    # live case -- it holds a `HUDANIM anim` and `HUDANIM` has no body anywhere
    # in the decomp. Find those transitively and drop both the type and every
    # global declared with it, by value, and say which.
    field = re.compile(r"\s*(?:const\s+|unsigned\s+|struct\s+)*"
                       r"([A-Za-z_]\w*)\s+(?!\*)\w+\s*(?:\[[^\]]*\])?\s*;")
    incomplete = set()
    while True:
        grown = set(incomplete)
        for n in needed:
            for line in blocks[n].split("\n"):
                mm = field.match(line)
                if mm and mm.group(1) not in SCALARS and (
                        mm.group(1) in grown or mm.group(1) not in blocks):
                    grown.add(n)
        if grown == incomplete:
            break
        incomplete = grown

    needed -= incomplete
    dropped = sorted(n for n, (_, _, m) in seen.items()
                     if not m.group("stars") and not m.group("dims")
                     and (m.group("type") in incomplete
                          or m.group("type") in opaque))
    for n in dropped:
        del seen[n]

    order = emit_header(needed, blocks, "src/runtime/gamecode_globals.h",
                        opaque, [l for _, l, _ in seen.values()])
    sys.stderr.write("mkglobals: %d typedefs -> runtime/gamecode_globals.h\n"
                     % len(order))
    if dropped:
        sys.stderr.write("mkglobals: %d globals skipped -- incomplete type: %s\n"
                         % (len(dropped), ", ".join(dropped)))

    print("/* Generated by tools/mkglobals.py -- do not hand-edit.")
    print(" *")
    print(" * Storage for the %d globals the decompiled sources declare and no"
          % len(seen))
    print(" * translation unit defines. See the script for the pointer-slot rule.")
    print(" */")
    print()
    print('#include <stddef.h>')
    print('#include "gamecode_globals.h"')
    print()
    print("#define SLOT_WORDS    %d" % SLOT_WORDS)
    print("#define UNSIZED_BYTES %d" % UNSIZED_BYTES)
    print()

    # A `*` in the declaration does NOT mean a pointer slot. `void *FloorTexture`
    # is an opaque handle the code assigns and never dereferences; `float *col`
    # is a slot the code reads through as `col[i]`. The two need opposite
    # treatment, and the only thing that tells them apart is the comment the
    # transcription carries -- which is exactly what it is for.
    #
    # The comment is not always there. `GAMESTATE *G` carries only its address,
    # and so do `TreasureGained`, `SizeofIntroLists` and the fight's mesh
    # tables -- each of them storage in the image (G is 1,144 bytes of
    # __common), each reached through a slot, and each defined here as a bare
    # pointer that stayed NULL until something read through it. So the image
    # is asked too: a pointer whose symbol is MORE than one word wide cannot be
    # a pointer variable, unless the code treats it as one -- assigns it
    # whole, or takes its address, the way `SelectBGTexture` (two words, an
    # array the front end indexes as `(&SelectBGTexture)[i]`) is used.
    extents = symbol_extents()
    assigned, addressed = assigned_names()

    # The third case is the one taking `&`. `HUDANIM_Render` passes
    # `&Fight_MeshAndTexture` -- the address of a 264-byte table of
    # (name, texture list) pairs -- and `(&SelectBGTexture)[i]` indexes two
    # words. The declared `T *X` is the table's first element there, so the
    # definition is the whole table under the image's own name: `X = v` still
    # writes element 0 and `&X` is the table, exactly as in the image.
    widened = {}
    plain, slots = [], []
    for name, (path, line, m) in sorted(seen.items()):
        is_slot = "pointer slot" in line and m.group("stars") and not m.group("dims")
        # `extern int *ErmacUnlockedPtr; /* slot -> 0x000ff974 */` says the
        # same thing in fewer words, about a name the image does not have.
        if (not is_slot and m.group("stars") and not m.group("dims")
                and name not in extents and _ARROW.search(line)):
            is_slot = True
        wide = (m.group("stars") and not m.group("dims")
                and not getattr(m, "fnptr", False)
                and extents.get(name, 0) > 4)
        if not is_slot and wide and name in addressed:
            widened[name] = extents[name] // 4
        elif not is_slot and wide and name not in assigned:
            is_slot = True
        (slots if is_slot else plain).append((name, path, line, m))

    guessed, measured = [], []

    # The initial VALUE of each global, read out of the image. Storage
    # without it is storage full of zeros, and for 498 of these zero is not
    # a neutral default -- it is the wrong number.
    img = mkdata.Image(BINARY, SYMBOLS)
    sources = [open(f, encoding='utf-8', errors='replace').read()
               for d in DECOMP_DIRS
               for f in [os.path.join(d, n) for n in sorted(os.listdir(d))
                         if n.endswith('.c')]]
    layouts = mkdata.struct_layouts(sources)
    defs = mkdata.defines(sources)
    warn, valued = {}, []

    # Only these names exist in the generated file, so only these can be
    # referred to by address from an initialiser -- until a table asks for one
    # that does not, which `demand` collects and the pass below supplies.
    known = set(seen)
    demand = {}

    # Function-pointer tables: the entry count and the names, from the image.
    # These have to be worked out before the forward declarations are printed,
    # because the declared length is part of the declaration.
    fntables = {}

    def init_for(name, typ, stars, dims):
        # No `demand` on the real pass. The dry run below already asked for
        # every symbol a table needs and `known` holds the ones that could be
        # supplied; accepting a new demand here would emit a reference to a name
        # that never gets defined, which is a link error dressed as an
        # initialiser.
        text = mkdata.initialiser(img, name, typ, stars, dims, layouts, warn,
                                  defs, known)
        if text is None:
            return ';'
        valued.append(name)
        return ' = %s;' % text

    slot_names = set(n for n, _p, _l, _m in slots)

    def store_shape(name, m):
        """A slot's storage as (type, stars, count), or None if unsized.

        The element count is the IMAGE's -- the measured bytes over the width
        an element has there -- not the host's sizeof. `void` has no storage
        of its own, so a `void *` slot backs onto pointer-sized words.
        """
        typ, stars = m.group("type"), m.group("stars")
        inner = stars[:-1]
        words = extents.get(name)
        base = re.sub(r"^const\s+", "", typ).strip()
        if typ == "void" and not inner:
            inner = "*"
        elem = 4 if inner else (mkdata.SCALAR_SIZE.get(base)
                                or mkdata.image_stride(base, layouts))
        if not words or not elem:
            return None
        return typ, inner, words // elem

    def emit_shape(name, m):
        """The (stars, dims) a global is DEFINED with, slot storage included."""
        if name in slot_names:
            st = store_shape(name, m)
            if st:
                return st[1], "[%d]" % st[2]
            return None
        if name in widened:
            return m.group("stars"), "[%d]" % widened[name]
        return m.group("stars"), m.group("dims")

    # A dry run first, purely to find out which unnamed symbols the tables
    # point at. Its output is thrown away; only the demands are kept, and they
    # join `known` before anything is printed, so the real pass can name them.
    scratch = {}
    for name, (path, line, m) in seen.items():
        shp = emit_shape(name, m)
        if shp is None:
            continue
        mkdata.initialiser(img, name, m.group("type"), shp[0], shp[1],
                           layouts, scratch, defs, known, demand)
    for name, (path, line, m) in seen.items():
        if not getattr(m, "fnptr", False):
            continue
        made = mkdata.function_table(img, name, scratch, DEFINED,
                                     set())
        if made:
            fntables[name] = made

    extra = []
    for _ in range(4):                      # a synthesised table can want more
        pending = [(n, ty) for n, ty in sorted(demand.items())
                   if n not in known]
        if not pending:
            break
        for n, ty in pending:
            known.add(n)
        for n, ty in pending:
            made = mkdata.synthesise(img, n, ty, layouts, scratch, defs,
                                     known, demand)
            if made:
                extra.append(made)
            else:
                known.discard(n)
    valued[:] = []

    # Resolve every declared extent once, before anything is printed. An
    # initialiser may name any other global -- `TrainingData` points at
    # `FatalityMessage` -- and C wants a declaration in hand before the
    # initialiser that uses it, which no single ordering of definitions can
    # promise. Declaring them all up front removes the ordering problem instead
    # of trying to satisfy it.
    shape = {}
    for name, path, line, m in plain:
        typ, dims = m.group("type"), m.group("dims")
        if name in widened:
            dims = "[%d]" % widened[name]
        if dims and "[]" in dims:
            if name in extents:
                base = re.sub(r"^(?:const|unsigned|signed|struct)\s+", "",
                              typ).strip()
                stride = mkdata.image_stride(base, layouts)
                if stride:
                    # `sizeof` is the HOST's, and for a struct holding pointers
                    # the host's is bigger: PLAYERDEF is 52 bytes in the image
                    # and 80 here, so dividing the measured 1352 by sizeof gave
                    # 16 entries for a table of 26. The count comes from the
                    # stride the image was laid out with.
                    dims = dims.replace("[]", "[%d]"
                                        % (extents[name] // stride), 1)
                elif m.group("stars"):
                    # An array of pointers: four bytes an element in the
                    # image, whatever the host's are. `DestinyNames` was
                    # 108 / sizeof(const char) -- 108 entries for 27.
                    dims = dims.replace("[]", "[%d]" % (extents[name] // 4), 1)
                else:
                    dims = dims.replace(
                        "[]", "[%d / sizeof(%s)]" % (extents[name], typ), 1)
                measured.append(name)
            else:
                dims = dims.replace(
                    "[]", "[UNSIZED_BYTES / sizeof(%s)]" % typ, 1)
                guessed.append(name)
        shape[name] = dims

    # Prototypes for whatever the function tables name. The table itself gives
    # the signature -- it is a table OF that type -- so these are not invented,
    # they are the same declaration written the other way round.
    if fntables:
        named = set()
        for name, (path, line, m) in seen.items():
            n = fntables.get(name)
            if not n:
                continue
            for cell in re.findall(r"^\s*(\w+),\s*$", n[1], re.M):
                if cell != "NULL":
                    named.add((cell, m.group("type"), m.group("args")))
        for fn, ret, args in sorted(named):
            print("%s %s(%s);" % (ret, fn, args))
        print()

    print("/* ---- every name, so an initialiser can reach any other ---- */")
    print()
    for name, path, line, m in plain:
        if getattr(m, "fnptr", False):
            n = fntables.get(name)
            print("extern %s (*%s%s)(%s);"
                  % (m.group("type"), name,
                     ("[%d]" % n[0]) if n else (shape[name] or "[SLOT_WORDS]"),
                     m.group("args")))
        else:
            print("extern %s %s%s%s;"
                  % (m.group("type"), m.group("stars"), name, shape[name]))
    for name, path, line, m in slots:
        print("extern %s %s%s;" % (m.group("type"), m.group("stars"), name))
    for decl, _body in extra:
        print(decl)
    print()

    if extra:
        print("/* ---- %d symbols only a table names ---- */" % len(extra))
        print("/*")
        print(" * Nothing in the decomp declares these; the front end reaches")
        print(" * them only through the tables above. Element type from the")
        print(" * pointer, length from the symbol table, contents from the")
        print(" * image -- see tools/mkdata.py, synthesise().")
        print(" */")
        print()
        for _decl, body in extra:
            print(body)
        print()

    print("/* ---- %d plain globals ---- */" % len(plain))
    print()
    for name, path, line, m in plain:
        typ, dims = m.group("type"), shape[name]
        comment = line.split("/*", 1)[1].rstrip("*/ ").strip() if "/*" in line else ""
        if getattr(m, "fnptr", False):
            n = fntables.get(name)
            print("%s (*%s%s)(%s)%s;%s"
                  % (typ, name,
                     ("[%d]" % n[0]) if n else (dims or "[SLOT_WORDS]"),
                     m.group("args"),
                     (" = %s" % n[1]) if n else "",
                     ("  /* %s */" % comment) if comment else ""))
            if n:
                valued.append(name)
            continue
        # Keep the stars: `extern void *X[26]` is an array of pointers, and
        # dropping them declares an array of void.
        print("%s %s%s%s%s%s"
              % (typ, m.group("stars"), name, dims,
                 init_for(name, typ, m.group("stars"), dims),
                 ("  /* %s */" % comment) if comment else ""))

    print()
    print("/* ---- %d pointer slots: storage, then a pointer at it ---- */"
          % len(slots))
    print()
    defined_here = set(seen) | set(
        re.findall(r"(\w+)\s*\[", " ".join(d for d, _b in extra)))
    for name, path, line, m in slots:
        typ, stars = m.group("type"), m.group("stars")
        # A name the transcription made up for a slot -- `FE_WidthScaleP` is
        # the slot that holds &FE_WidthScale -- is not storage of its own.
        # Giving it some made the slot point at zeros: the splash screen drew
        # 480 * 0 wide. It points at the variable it names.
        target = slot_target(name, line, img) if name not in extents else None
        if target:
            ref = ("%s__store" % target) if target in slot_names else \
                  "(void *)&%s" % target
            if target not in defined_here:
                if (target not in DEFINED and not runtime_defines(target)
                        and extents.get(target)):
                    # Nothing defines the variable itself -- the
                    # transcription only ever reached it through this slot.
                    # It is a word or two of the image: define it here,
                    # with the image's bytes.
                    raw = img.bytes_of(target) or b""
                    body = ", ".join(str(b) for b in raw)
                    print("unsigned char %s[%d] __attribute__((aligned(4)))%s;"
                          % (target, extents[target],
                             (" = { %s }" % body) if any(raw) else ""))
                    defined_here.add(target)
                else:
                    print("extern char %s[];" % target)
                ref = "(void *)%s" % target
            print("%s %s%s = (%s %s)%s;  /* the slot holding &%s */"
                  % (typ, stars, name, typ, stars, ref, target))
            continue
        # The storage starts with what the image holds. A zeroed backing for
        # `darkcol` is a black colour and for `IntroLists` a table of null
        # lists: the slot rule fixed the crash and kept the wrong value.
        st = store_shape(name, m)
        if st:
            _t, inner, n = st
            store_t = "%s %s" % (typ, inner)
            init = init_for(name, typ, inner, "[%d]" % n)
            print("static %s%s__store[%d]%s" % (store_t, name, n, init))
        else:
            inner = stars[:-1]
            store_t = "void *" if (typ == "void" and not inner) else "%s %s" % (typ, inner)
            if extents.get(name):
                # A type this file cannot size -- `limeVECTOR2` is only a
                # name here -- still has a measured extent: give it that many
                # bytes, aligned for anything, and point at them.
                print("static unsigned char %s__store[%d] __attribute__((aligned(16)));"
                      % (name, extents[name]))
                print("%s %s%s = (%s %s)(void *)%s__store;"
                      % (typ, stars, name, typ, stars, name))
                continue
            print("static %s%s__store[SLOT_WORDS];" % (store_t, name))
        print("%s %s%s = %s__store;" % (typ, stars, name, name))

    print()
    print("/* %d plain + %d slots = %d */"
          % (len(plain), len(slots), len(seen)))
    if guessed:
        print("/*")
        print(" * %d of these are arrays the decomp declares as `T name[]` with no"
              % len(guessed))
        print(" * extent, so each got UNSIZED_BYTES of slack rather than a known size.")
        print(" * A crash indexing one of them is a size to look up and add to")
        print(" * EXTENTS in tools/mkglobals.py, not a size to raise here:")
        print(" *")
        for i in range(0, len(guessed), 4):
            print(" *   " + "  ".join("%-22s" % g for g in guessed[i:i + 4]).rstrip())
        print(" */")
        sys.stderr.write("mkglobals: %d arrays have a guessed extent\n"
                         % len(guessed))
    sys.stderr.write("mkglobals: %d globals carry a value from the image\n"
                     % len(valued))
    sys.stderr.write("mkglobals: %d symbols recovered that only a table names\n"
                     % len(extra))
    mkdata.report(warn)


if __name__ == "__main__":
    main()
